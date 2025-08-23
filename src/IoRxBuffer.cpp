//
// Created by goran on 21/08/2025.
//
#include "IoRxBuffer.hpp"

#include "TcpClient.hpp"
namespace async_tcp {

        err_t lwip_receive_callback(void *arg, tcp_pcb *tpcb, pbuf *p,
                                    err_t err) {
            (void) err; // Acknowledge unused parameter

            // In embedded systems, these should never be null during normal operation
            // If they are, it's a fundamental initialization failure
            assert(arg);
            const auto *client = static_cast<TcpClient *>(arg);

            const auto rx_buffer = client->getRxBuffer();
            assert(rx_buffer);

            rx_buffer->_pcb = tpcb;
            // The remote peer sends a TCP segment with the FIN flag set to
            // close.
            if (p == nullptr) {
                // Step 1: First flush any remaining data in the buffer to the application
                if (rx_buffer->_head && rx_buffer->_head->tot_len) {
                    rx_buffer->_onReceivedCallback();
                    return ERR_OK;
                }

                // Step 2: Then notify the application about connection closing
                rx_buffer->_onClosedCallback();

                // Step 3: Finally free resources and reset buffer state
                if (rx_buffer->_head) {
                    pbuf_free(rx_buffer->_head);
                    rx_buffer->_head = nullptr;
                    rx_buffer->_offset = 0;
                }

                return ERR_ABRT;
            }

            // CRITICAL: Check if we already own this pbuf (_head == p)
            // This can happen if lwIP retries the callback
            if (rx_buffer->_head == p) {
                // We already own this pbuf, just notify application
                rx_buffer->_onReceivedCallback();
                return ERR_OK;
            }

            // Normal case: append new data or take ownership of first pbuf
            if (rx_buffer->_head) {
                // Append to existing buffer chain (different pbuf)
                pbuf_cat(rx_buffer->_head, p);
            } else {
                // No existing data - take ownership of new pbuf
                rx_buffer->_head = p;
                rx_buffer->_offset = 0;
            }

            // Notify application that new data is available
            rx_buffer->_onReceivedCallback();

            // We took ownership of the pbuf, so return ERR_OK
            return ERR_OK;
        }

        void IoRxBuffer::_onReceivedCallback() const {
            if (_receivedCb) _receivedCb();
        }

        void IoRxBuffer::_onClosedCallback() const {
            if (_closedCb) {
                _closedCb();
            }
        }

        IoRxBuffer::IoRxBuffer(pbuf *chain) { _head = chain; }

        std::size_t IoRxBuffer::size() { return 0; }

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

        void IoRxBuffer::peekConsume(const std::size_t n) {
            if (const auto left =
                    static_cast<ptrdiff_t>(_head->len - _offset - n);
                left > 0) {
                _offset += n;
            } else if (!_head->next) {
                const auto head = _head;
                _head = nullptr;
                _offset = 0;
                pbuf_free(head);
            } else {
                const auto head = _head;
                _head = _head->next;
                _offset = 0;
                pbuf_ref(_head);
                pbuf_free(head);
            }
            if (_pcb) {
                tcp_recved(_pcb, n);
            }
        }
        void IoRxBuffer::setOnClosedCallback(const closed_callback_t &cb) {
            _closedCb = cb;
        }

        void IoRxBuffer::setOnReceivedCallback(const received_callback_t &cb) {
            _receivedCb = cb;
        }
} // namespace async_tcp
