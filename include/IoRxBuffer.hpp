#pragma once

#include "lwip/tcp.h"
#include <cstddef>
#include <functional>

namespace async_tcp {

    using receive_cb = tcp_recv_fn;
    using fin_callback_t = std::function<void()>;
    using received_callback_t = std::function<void()>;

    extern "C" {
    /**
     * @brief Bridge function for lwIP tcp_recv callback.
     *
     * Takes ownership of the provided pbuf chain (if non-null), appends it to
     * this IoRxBuffer instance, and notifies the registered receive handler.
     * When p is null, FIN is indicated and the FIN handler is notified; ERR_ABRT
     * is returned to stop further receive callbacks for this PCB.
     */
    err_t lwip_receive_callback(void *arg, tcp_pcb *tpcb, pbuf *p, err_t err);
    }

    /**
     * @brief RAII wrapper for a TCP receive buffer (lwIP pbuf chain) with
     * cursor-style access.
     *
     * Ownership and lifetime:
     * - IoRxBuffer assumes ownership of the head of the pbuf chain it receives
     *   from lwIP and frees segments as they are fully consumed.
     * - peekBuffer() returns a pointer valid until the next peekConsume() or
     *   reset(); it may be null if no data is available.
     *
     * Consumption model:
     * - peekAvailable() reports bytes remaining in the current pbuf segment
     *   (not the entire chain).
     * - peekConsume(n) advances the cursor across segments, freeing exhausted
     *   pbufs and updating the TCP receive window via tcp_recved() with the
     *   exact consumed count.
     *
     * Thread-safety and context:
     * - Not thread-safe. Call only from the networking core’s async context
     *   (e.g., inside PerpetualBridge/EphemeralBridge on that core) or from
     *   lwIP callbacks. Do not call from ISRs or other cores.
     */
    class IoRxBuffer {

            friend err_t lwip_receive_callback(void *arg, tcp_pcb *tpcb,
                                               pbuf *p, err_t err);
            tcp_pcb *_pcb = nullptr; ///< Pointer to the TCP PCB
            pbuf *_head{};           ///< Head of the pbuf chain or nullptr
            std::size_t _offset{};   ///< Byte offset into current head payload
            received_callback_t _receivedCb{};
            fin_callback_t _finCb = nullptr;

            void _onReceivedCallback() const;
            void _onFinCallback() const;
            void _free();
            std::size_t _fastPath(std::size_t remaining);
            std::size_t _slowPath(std::size_t remaining);
            void _toAck(std::size_t consumed) const;

        public:
            /**
             * @brief Construct a buffer from a pbuf chain; takes ownership.
             */
            explicit IoRxBuffer(pbuf *chain);

            /**
             * @brief Destructor frees any remaining pbufs and clears state.
             */
            ~IoRxBuffer() { reset(); }

            // Non-copyable
            IoRxBuffer(const IoRxBuffer &) = delete;
            IoRxBuffer &operator=(const IoRxBuffer &) = delete;

            // Non-movable
            IoRxBuffer(IoRxBuffer &&other) = delete;
            IoRxBuffer &operator=(IoRxBuffer &&other) = delete;

            /**
             * @brief Free the current chain, reset the cursor and PCB pointer.
             */
            void reset();

            /**
             * @brief Returns total unconsumed bytes across the chain.
             * @note Currently unimplemented placeholder; returns 0.
             */
            [[nodiscard]] static std::size_t size();

            /**
             * @brief Peek at the next byte in the current segment.
             * @return Next byte value, or 0 if the buffer is empty.
             * @note Does not advance the cursor.
             */
            [[nodiscard]] char peek() const;

            /**
             * @brief Bytes available in the current pbuf segment.
             * @return Number of readable bytes before crossing to next segment.
             */
            [[nodiscard]] std::size_t peekAvailable() const;

            /**
             * @brief Pointer to the current pbuf payload at the cursor.
             * @return Pointer to readable bytes, or nullptr if empty.
             * @note Valid until the next peekConsume() or reset().
             */
            [[nodiscard]] const char *peekBuffer() const;

            /**
             * @brief Consume n bytes from the buffer.
             *
             * Advances the cursor, frees segments when fully consumed (exact-fit
             * handled in the fast path), and calls tcp_recved() with the exact
             * number of bytes consumed (chunked to u16_t as required by lwIP).
             */
            void peekConsume(std::size_t n);

            /**
             * @brief Register FIN notification callback.
             * @param cb Functor invoked when lwIP indicates FIN (p == nullptr).
             */
            void setOnFinCallback(const fin_callback_t &cb);

            /**
             * @brief Register receive notification callback.
             * @param cb Functor invoked when new data is appended by lwIP.
             */
            void setOnReceivedCallback(const received_callback_t &cb);
    };

} // namespace async_tcp
