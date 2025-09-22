//
// Created by goran on 21/08/2025.
//
#include "IoRxBuffer.hpp"

#include "TcpClientContext.hpp"
#include <algorithm>
#include <cassert>

namespace async_tcp {

    /**
     * @brief lwIP tcp_recv bridge.
     *
     * Behavior:
     * - When err != ERR_OK: frees p (if non-null) and returns the error.
     * - When p == nullptr: FIN indicated; notifies FIN callback and returns
     *   ERR_ABRT to stop further receives.
     * - Otherwise: appends p to the existing chain (or takes ownership if
     *   no head), then notifies the receive callback and returns ERR_OK.
     *
     * Notes:
     * - Stores the pcb pointer for later tcp_recved() window updates.
     * - IoRxBuffer owns the pbuf chain after this call.
     */
    err_t lwip_receive_callback(void *arg, tcp_pcb *tpcb, pbuf *p, err_t err) {

        // In embedded systems, these should never be null during normal
        // operation If they are, it's a fundamental initialization failure.
        assert(arg);
        const auto *ctx = static_cast<TcpClientContext *>(arg);

        const auto rx_buffer = ctx->getRxBuffer();
        assert(rx_buffer);

        // ReSharper disable once CppDFAUnreachableCode
        rx_buffer->_pcb = tpcb;

        // If lwIP reports an error with a non-null pbuf, free it and
        // propagate the error to avoid leaking the pbuf
        if (err != ERR_OK) {
            if (p) {
                pbuf_free(p);
            }
            return err; // todo: notify app? return ERR_ABRT?
        }

        // The remote peer sends a TCP segment with the FIN flag set to close.
        if (p == nullptr) {
            DEBUGWIRE("[:i%d] :rxclb st=%d\n", ctx->getClientId(),
                      tpcb->state);

            // FIN received — connection is closing
            rx_buffer->_onFinCallback();

            return ERR_ABRT;
        }

        // Normal case: append new data or take ownership of first pbuf
        if (rx_buffer->_head) {
            DEBUGWIRE("[:i%d] :rxclb cat h%p p=%p\n", ctx->getClientId(),
                      rx_buffer->_head, p);
            // Append to existing buffer chain (different pbuf)
            pbuf_cat(rx_buffer->_head, p);
        } else {
            DEBUGWIRE("[:i%d] :rxclb new h%p = p=%p\n", ctx->getClientId(),
                      rx_buffer->_head, p);
            // No existing data - take ownership of new pbufNo existing data - take ownership of new pbuf
            rx_buffer->_head = p;
            rx_buffer->_offset = 0;
        }

        // Notify application that new data is available
        rx_buffer->_onReceivedCallback();

        // We took ownership of the pbuf, so return ERR_OK
        return ERR_OK;
    }

    /**
     * @brief Notify application of new data arrival (if registered).
     */
    void IoRxBuffer::_onReceivedCallback() const {
        if (_receivedCb)
            _receivedCb();
    }

    /**
     * @brief Notify application that a FIN was received (if registered).
     */
    void IoRxBuffer::_onFinCallback() const {

        if (_finCb) {
            _finCb();
        }
    }

    /**
     * @brief Free current head segment and advance to next.
     *
     * After calling, _head may become nullptr and _offset is reset to 0.
     * Keeps the chain alive via pbuf_ref() while freeing the old segment.
     */
    void IoRxBuffer::_free() {
        // Save a pointer to the pbuf we are about to free.
        pbuf* old = _head;
        // Advance to the next segment (may become nullptr).
        _head = _head->next;
        // Reset offset for the new head
        _offset = 0;

        // Keep the chain alive while we free the old segment.
        if (_head) {
            pbuf_ref(_head);
        }
        pbuf_free(old);
    }

    /**
     * @brief Fast path consumption within a single pbuf.
     *
     * Advances offset by remaining; if the segment is exactly exhausted,
     * calls _free() to advance to the next pbuf.
     * @return bytes consumed (equal to remaining)
     */
    std::size_t IoRxBuffer::_fastPath(const std::size_t remaining) {
        _offset += remaining;
        const auto consumed = remaining;
        // If we exactly consumed the remainder of this pbuf, advance and free it
        if (_offset == _head->len) {
            _free();
        }
        return consumed;
    }

    /**
     * @brief Slow path crossing segment boundaries.
     *
     * Recomputes available per-iteration, accumulates consumed across pbufs,
     * and frees segments as necessary until the request is satisfied or the
     * chain ends.
     * @return total bytes consumed (<= requested)
     */
    std::size_t IoRxBuffer::_slowPath(std::size_t remaining) {
        // Slow‑path: consume whole pbufs until we satisfy the request.
        std::size_t consumed = 0;          // total bytes actually removed
        while (remaining > 0 && _head) {
            const std::size_t available = _head->len - _offset;
            if (remaining < available) {
                // Partial consumption of the current pbuf
                _offset += remaining;
                consumed += remaining;
                return consumed; // Exit consumption loop now
            }
            // Consume the rest of this pbuf
            consumed += available;
            remaining -= available;
            _free();
        }
        return consumed;
    }

    /**
     * @brief Acknowledge consumed bytes to lwIP (tcp_recved in u16_t chunks).
     */
    void IoRxBuffer::_toAck(const std::size_t consumed) const {
        // tcp_recved takes a u16_t, so split large values.
        std::size_t to_ack = consumed;
        while (to_ack) {
            const u16_t chunk =
                static_cast<u16_t>(std::min<std::size_t>(to_ack, 0xFFFF));
            tcp_recved(_pcb, chunk);
            to_ack -= chunk;
        }

    }

    IoRxBuffer::IoRxBuffer(pbuf *chain) { _head = chain; }

    /**
     * @brief Reset buffer state and release any remaining pbufs.
     */
    void IoRxBuffer::reset() {
        if (_head) {
            pbuf_free(_head);
            _head = nullptr;
            _offset = 0;
            _pcb = nullptr;
        }
    }

    std::size_t IoRxBuffer::size() { return 0; }

    char IoRxBuffer::peek() const {
        if (!_head) {
            return 0;
        }
        return static_cast<char *>(_head->payload)[_offset];
    }

    std::size_t IoRxBuffer::peekAvailable() const {
        if (!_head) {
            return 0;
        }
        return _head->len - _offset;
    }

    const char *IoRxBuffer::peekBuffer() const {
        if (!_head) {
            return nullptr;
        }
        return static_cast<const char *>(_head->payload) + _offset;
    }

    /**
     * @brief Consume n bytes, choosing fast or slow path.
     *
     * Notifies lwIP of the exact number of bytes removed from the receive
     * window using tcp_recved().
     */
    void IoRxBuffer::peekConsume(const std::size_t n) {
        // Guard against empty buffers or zero‑length requests
        if (n == 0 || !_head) {
            return;
        }

        const std::size_t available = _head->len - _offset;
        const std::size_t remaining = n;         // bytes still to consume
        std::size_t consumed = 0;          // total bytes actually removed

        // Fast‑path: everything fits in the current pbuf
        if (remaining <= available) {
            consumed = _fastPath(remaining);
        } else {
            // Slow‑path: consume whole pbufs until we satisfy the request.
            consumed = _slowPath(remaining);
        }

        // Notify lwIP of the exact amount we have removed.
        if (_pcb && consumed > 0) {
            _toAck(consumed);
        }
    }

    void IoRxBuffer::setOnFinCallback(const fin_callback_t &cb) {
        _finCb = cb;
    }

    void IoRxBuffer::setOnReceivedCallback(const received_callback_t &cb) {
        _receivedCb = cb;
    }
} // namespace async_tcp
