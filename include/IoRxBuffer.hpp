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
     * @brief Bridging function for LwIP TCP receive callback
     *
     * This function is called by LwIP when data is received on a TCP
     * connection. It bridges the C-style callback to the C++ receive
     * handler.
     *
     * @param arg User-defined argument passed to the callback
     * @param tpcb Pointer to the TCP PCB that received data
     * @param p Pointer to the received packet buffer
     * @param err Error code if any error occurred
     */
    err_t lwip_receive_callback(void *arg, tcp_pcb *tpcb, pbuf *p, err_t err);
    }

    /**
     * @brief RAII wrapper for a TCP receive buffer (lwIP pbuf chain) with
     * cursor access.
     *
     * IoRxBuffer encapsulates ownership of a received TCP pbuf chain,
     * providing a cursor-style interface (`peekAvailable()`,
     * `peekBuffer()`, `peekConsume()`) for applications to inspect and
     * consume data at their own pace.
     */
    class IoRxBuffer {

            friend err_t lwip_receive_callback(void *arg, tcp_pcb *tpcb,
                                               pbuf *p, err_t err);
            tcp_pcb *_pcb = nullptr; ///< Pointer to the TCP PCB
            pbuf *_head;
            std::size_t _offset{};
            received_callback_t _receivedCb;
            fin_callback_t _finCb = nullptr;

            void _onReceivedCallback() const;
            void _onFinCallback() const;

        public:
            /**
             * @brief Construct a buffer from a TCP PCB and pbuf chain.
             *
             * Takes ownership of the pbuf chain.
             */
            explicit IoRxBuffer(pbuf *chain);

            ~IoRxBuffer() { reset(); }

            // Non-copyable
            IoRxBuffer(const IoRxBuffer &) = delete;
            IoRxBuffer &operator=(const IoRxBuffer &) = delete;

            // Non-movable
            IoRxBuffer(IoRxBuffer &&other) = delete;
            IoRxBuffer &operator=(IoRxBuffer &&other) = delete;

            void reset();

            /**
             * @brief Returns total unconsumed bytes across the chain.
             */
            [[nodiscard]] static std::size_t size();

            /**
             * @brief Peek at the next byte in the internal receive buffer
             * @return The next byte, or 0 if buffer is empty
             * @note Does not consume the byte
             */
            [[nodiscard]] char peek() const;

            /**
             * @brief Returns number of bytes available in the current pbuf
             * segment.
             */
            [[nodiscard]] std::size_t peekAvailable() const;

            /**
             * @brief Returns pointer to the current pbuf payload at the
             * cursor.
             *
             * Pointer remains valid until `peekConsume()` advances the
             * cursor, or the buffer is destroyed.
             */
            [[nodiscard]] const char *peekBuffer() const;

            /**
             * @brief Consume `n` bytes from the buffer.
             *
             * Advances the cursor and frees pbuf segments as they are
             * exhausted. Also updates TCP receive window (`tcp_recved()`).
             * The value is internally cast to `u16_t` for lwIP.
             */
            void peekConsume(std::size_t n);

            void setOnFinCallback(const fin_callback_t &cb);

            void setOnReceivedCallback(const received_callback_t &cb);
    };

} // namespace async_tcp
