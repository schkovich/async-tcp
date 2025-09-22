/*
    TcpClientContext.h - TCP connection handling on top of lwIP

    Copyright (c) 2014 Ivan Grokhotkov. All rights reserved.
    This file is part of the esp8266 core for Arduino environment.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#pragma once

#include "IoRxBuffer.hpp"
#include "TcpWriter.hpp"

#include <Arduino.h>
#include <cassert>
#include <functional>
#include <lwip/ip.h>
#include <lwip/opt.h>
#include <lwip/tcp.h>
#include <memory>

namespace async_tcp {

#ifndef TCP_MSS
#define TCP_MSS 1460 // lwip1.4
#endif

#define ASYNC_TCP_CLIENT_MAX_PACKET_SIZE TCP_MSS
#define ASYNC_TCP_CLIENT_MAX_FLUSH_WAIT_MS 300

#define TCP_DEFAULT_KEEP_ALIVE_IDLE_SEC 7200   // 2 hours
#define TCP_DEFAULT_KEEP_ALIVE_INTERVAL_SEC 75 // 75 sec
#define TCP_DEFAULT_KEEP_ALIVE_COUNT 9         // fault after 9 failures

    class TcpClientContext;

    typedef void (*discard_cb_t)(void *, TcpClientContext *);

    using receive_cb_t = std::function<void(std::unique_ptr<int>)>;

    using error_cb_t = std::function<void(err_t err)>;

    class TcpClientContext {
        public:
            explicit TcpClientContext(tcp_pcb *pcb)
                : _pcb(pcb) {
                tcp_setprio(_pcb, TCP_PRIO_MIN);
                tcp_arg(_pcb, this);
                tcp_recv(_pcb, lwip_receive_callback);
                tcp_sent(_pcb, lwip_sent_cb);
                tcp_err(_pcb, &_s_error);
                tcp_poll(_pcb, reinterpret_cast<tcp_poll_fn>(&_s_poll), 1);
                initRxBuffer();
                initTxWriter(pcb);
            }

            err_t abort() {
                if (_pcb) {
                    DEBUGWIRE("[:i%d] :abort\n", getClientId());
                    // Ensure any pending RX data is released
                    if (_rx) { _rx->reset(); }
                    tcp_arg(_pcb, nullptr);
                    tcp_sent(_pcb, nullptr);
                    tcp_recv(_pcb, nullptr);
                    tcp_err(_pcb, nullptr);
                    tcp_poll(_pcb, nullptr, 0);
                    tcp_abort(_pcb);
                    _pcb = nullptr;
                }
                return ERR_ABRT;
            }

            err_t close() {
                err_t err = ERR_OK;
                if (_pcb) {
                    DEBUGWIRE("[:i%d] :close\n", getClientId());
                    // Ensure any pending RX data is released
                    if (_rx) { _rx->reset(); }
                    tcp_arg(_pcb, nullptr);
                    tcp_sent(_pcb, nullptr);
                    tcp_recv(_pcb, nullptr);
                    tcp_err(_pcb, nullptr);
                    tcp_poll(_pcb, nullptr, 0);
                    err = tcp_close(_pcb);
                    if (err != ERR_OK) {
                        DEBUGWIRE("[:i%d] :tc err %d\n", getClientId(),
                                  static_cast<int>(err));
                        tcp_abort(_pcb);
                        err = ERR_ABRT;
                    }
                    _pcb = nullptr;
                }
                return err;
            }

            ~TcpClientContext() {
                // Clean up IoRxBuffer when context is destroyed
                cleanupRxBuffer();
                // Clean up IoTxWriter when context is destroyed
                cleanupTxWriter();
            }

            err_t connect(ip_addr_t *addr, const uint16_t port) const {
                // note: not using `const ip_addr_t* addr` because
                // — `ip6_addr_assign_zone()` below modifies `*addr`
                // — caller's parameter `AsyncTcpClient::connect` is a local
                // copy.
#if LWIP_IPV6
                // Set zone so that link local addresses use the default
                // interface.
                if (IP_IS_V6(addr) &&
                    ip6_addr_lacks_zone(ip_2_ip6(addr), IP6_UNKNOWN)) {
                    ip6_addr_assign_zone(ip_2_ip6(addr), IP6_UNKNOWN,
                                         netif_default);
                }
#endif
                const err_t err =
                    tcp_connect(_pcb, addr, port,
                                reinterpret_cast<tcp_connected_fn>(
                                    &TcpClientContext::_s_connected));
                if (err != ERR_OK) {
                    DEBUGWIRE("[:i%d] :connect err %d\n", getClientId(),
                              static_cast<int>(err));
                    return err;
                }

                if (!_pcb) {
                    DEBUGWIRE("[:i%d] :cabrt\n", getClientId());
                    return ERR_ABRT;
                }
                DEBUGWIRE("[:i%d] :conn\n", getClientId());
                return ERR_OK;
            }

            /**
             * Sets Nagle's algorithm. When set to true, disables Nagle's
             * algorithm (no delay). When set to false, enables Nagle's
             * Calling the function from the async context is thread safe.
             * @return void
             */
            void setNoDelay(const bool no_delay) const {
                if (!_pcb) {
                    return;
                }
                if (no_delay) {
                    tcp_nagle_disable(_pcb);
                } else {
                    tcp_nagle_enable(_pcb);
                }
            }

            [[nodiscard]] bool getNoDelay() const {
                if (!_pcb) {
                    return false;
                }
                return tcp_nagle_disabled(_pcb);
            }

            void setTimeout(const uint32_t timeout_ms) {
                _timeout_ms = timeout_ms;
            }

            [[maybe_unused]] [[nodiscard]] uint32_t getTimeout() const {
                return _timeout_ms;
            }

            [[nodiscard]] const ip_addr_t *getRemoteAddress() const {
                if (!_pcb) {
                    return nullptr;
                }

                return &_pcb->remote_ip;
            }

            [[nodiscard]] uint16_t getRemotePort() const {
                if (!_pcb) {
                    return 0;
                }

                return _pcb->remote_port;
            }

            [[nodiscard]] const ip_addr_t *getLocalAddress() const {
                if (!_pcb) {
                    return nullptr;
                }

                return &_pcb->local_ip;
            }

            [[nodiscard]] uint16_t getLocalPort() const {
                if (!_pcb) {
                    return 0;
                }

                return _pcb->local_port;
            }


            [[nodiscard]] uint8_t state() const {
                if (!_pcb || _pcb->state == CLOSE_WAIT ||
                    _pcb->state == CLOSING) {
                    // CLOSED for WiFIClient::status() means nothing more can be
                    // written
                    return CLOSED;
                }

                return _pcb->state;
            }

            /**
             * @brief Write a single chunk directly to TCP connection
             *
             * This method bypasses _write_from_source and _write_some
             * completely, directly calling tcp_write() for a single chunk. Used
             * by the new worker-based write system.
             *
             * @param data Pointer to binary data to write
             * @param size Size of data chunk
             */
            void writeChunk(const uint8_t *data, const size_t size) const {
                if (!_pcb) {
                    // No PCB — connection not established or closed
                    _errorCb(ERR_CONN);
                    return;
                }

                if (!data || size == 0) {
                    // Invalid parameters
                    _errorCb(PICO_ERROR_INVALID_ARG);
                    return;
                }

                // Calculate chunk size.
                const auto sbuf = static_cast<size_t>(tcp_sndbuf(_pcb));
                const auto chunk_size = std::min(sbuf, size);

                if (chunk_size == 0) {
                    // Buffer space not available — this constitutes an ERR_MEM
                    // condition.
                    _errorCb(ERR_MEM);
                    return;
                }

                // Direct tcp_write call
                if (const auto err = tcp_write(_pcb, data, chunk_size, 0);
                    err != ERR_OK) {
                    // Error — notify integration layer via callback
                    _errorCb(err);
                    return;
                }

                tcp_output(_pcb); // Ensure data is sent immediately
            }

            void keepAlive(
                const uint16_t idle_sec = TCP_DEFAULT_KEEP_ALIVE_IDLE_SEC,
                const uint16_t intv_sec = TCP_DEFAULT_KEEP_ALIVE_INTERVAL_SEC,
                const uint8_t count = TCP_DEFAULT_KEEP_ALIVE_COUNT) const {
                if (idle_sec && intv_sec && count) {
                    _pcb->so_options |= SOF_KEEPALIVE;
                    _pcb->keep_idle = static_cast<uint32_t>(1000) * idle_sec;
                    _pcb->keep_intvl = static_cast<uint32_t>(1000) * intv_sec;
                    _pcb->keep_cnt = count;
                } else {
                    _pcb->so_options &= ~SOF_KEEPALIVE;
                }
            }

            [[nodiscard]] bool isKeepAliveEnabled() const {
                return !!(_pcb->so_options & SOF_KEEPALIVE);
            }

            [[nodiscard]] uint16_t getKeepAliveIdle() const {
                return isKeepAliveEnabled() ? (_pcb->keep_idle + 500) / 1000
                                            : 0;
            }

            [[nodiscard]] uint16_t getKeepAliveInterval() const {
                return isKeepAliveEnabled() ? (_pcb->keep_intvl + 500) / 1000
                                            : 0;
            }

            [[nodiscard]] uint8_t getKeepAliveCount() const {
                return isKeepAliveEnabled() ? _pcb->keep_cnt : 0;
            }

            void setOnConnectCallback(const std::function<void()> &cb) {
                _connectCb = cb; // Set the success callback
            }

            void setOnErrorCallback(const error_cb_t &cb) {
                _errorCb = cb;
            }

            void setOnAckCallback(const std::function<void(struct tcp_pcb *tpcb,
                                                           uint16_t len)> &cb) {
                _ackCb = cb;
            }

            void setOnWrittenCallback(
                const std::function<void(size_t bytes_written)> &cb) {
                _writtenCb = cb;
            }

            void setOnPollCallback(const std::function<void()> &cb) {
                _pollCb = cb;
            }

            void setOnFinCallback(const std::function<void()> &cb) {
                _finCb = cb;
            }

            void setOnReceivedCallback(const std::function<void()> &cb) {
                _receiveCb = cb;
            }

            /**
             * @brief Set the client ID for this TcpClientContext instance.
             * @param id The client ID to assign (uint8_t)
             */
            void setClientId(const uint8_t id) { m_client_id = id; }

            /**
             * @brief Get the IoRxBuffer owned by this context
             * @return IoRxBuffer* pointer to the receive buffer
             */
            [[nodiscard]] IoRxBuffer* getRxBuffer() const { return _rx; }

            /**
             * @brief Initialize the IoRxBuffer for this context
             */
            void initRxBuffer() {
                if (!_rx) {
                    _rx = new IoRxBuffer(nullptr);
                    _rx->setOnFinCallback([this] { _finCb(); });
                    _rx->setOnReceivedCallback(
                                [this] { _receiveCb(); });
                }
            }

            /**
             * @brief Clean up the IoRxBuffer
             */
            void cleanupRxBuffer() {
                if (_rx) { _rx->reset(); }
                delete _rx;
                _rx = nullptr;
            }

            /**
             * @brief Initialize the IoTxWriter for this context
             * @param pcb
             */
            void initTxWriter(tcp_pcb *pcb) {
                _tx = new TcpWriter(pcb);
                _tx->setOnAckCallback(_ackCb);
            }

            /**
             * @brief Clean up the IoTxWriter
             */
            void cleanupTxWriter() {
                delete _tx;
                _tx = nullptr;
            }

            /**
             * @brief Get the client ID (for internal logging)
             * @return uint8_t client id
             */
            [[nodiscard]] uint8_t getClientId() const { return m_client_id; }

            [[nodiscard]] TcpWriter *getTxWriter() const { return _tx; }


        protected:

            /**
             * @brief Checks if the operation has timed out based on a given
             * start time
             *
             * This function replaces the blocking esp_delay mechanism with a
             * non-blocking timeout check. It compares the elapsed time since
             * start_time against the configured timeout value (_timeout_ms).
             *
             * @param start_time The timestamp (in milliseconds) when the
             * operation started
             * @return `true` if the operation has timed out (elapsed time >
             * timeout)
             * @return `false` if the operation is still within the timeout
             * window.
             *
             * @note Uses millis() for time measurement, which wraps around
             * every ~49 days.
             */
            [[nodiscard]] bool _is_timeout(const uint32_t start_time) const {
                return millis() - start_time > _timeout_ms;
            }

            /**
             * @brief Check if TCP connection is valid
             *
             * @return `true` Connection is valid (_pcb exists and state is not
             * CLOSED)
             * @return `false` Connection is invalid
             *
             * This function checks both the existence of PCB and connection
             * state to determine if TCP operations can be performed.
             */
            [[nodiscard]] bool _is_connection_valid() const {
                return (_pcb != nullptr && state() != CLOSED);
            }

            /**
             * @brief Calculate next chunk size for TCP write operation
             *
             * @param remaining Number of bytes remaining to write
             * @param scale Current scale factor for chunk size adjustment
             * @return size_t Calculated chunk size (0 if no write possible)
             *
             * This function determines the optimal chunk size based on TCP send
             * buffer and applies scaling for memory constraint handling.
             */
            [[nodiscard]] size_t _calculate_chunk_size(const size_t remaining,
                                                       const int scale) const {
                const auto sbuf = static_cast<size_t>(tcp_sndbuf(_pcb));
                DEBUGWIRE("[:i%d] :sbuf %d, rem %d, scale %d\n", getClientId(),
                          sbuf, remaining, scale);
                size_t chunk_size = std::min(sbuf, remaining);

                if (chunk_size > static_cast<size_t>(1 << scale)) {
                    chunk_size >>= scale;
                }

                return chunk_size;
            }

            /**
             * @brief Get TCP write flags based on current write operation
             *
             * @param chunk_size Size of current chunk to be written
             * @param remaining Total remaining bytes to write
             * @return uint8_t Combined TCP write flags
             *
             * This function determines if TCP_WRITE_FLAG_MORE should be set
             * (more data coming) and if TCP_WRITE_FLAG_COPY should be set.
             */
            static uint8_t _get_write_flags(const size_t chunk_size,
                                            const size_t remaining) {
                uint8_t flags =
                    TCP_WRITE_FLAG_COPY; // Always copy data for safety
                if (chunk_size < remaining) {
                    flags |= TCP_WRITE_FLAG_MORE;
                }
                return flags;
            }

            void _error(const err_t err) {
                DEBUGWIRE("[:i%d] :er %d 0x%%\n", getClientId(),
                          static_cast<int>(err));

                // Do NOT immediately nullify _pcb - this creates race conditions
                // The PCB is already invalidated by lwIP when error callback is invoked
                // Let the controlled shutdown sequence handle cleanup

                // Mark connection as in error state but preserve PCB reference
                // until proper cleanup can be coordinated

                _errorCb(err);
            }

            void _fin() const {
                if (_finCb) {
                    _finCb();
                }
            }

            void _recv() const {
                if (_receiveCb) {
                    _receiveCb();
                }
            }

            err_t _connected(const tcp_pcb *pcb, const err_t err) const {
                (void)err;
                assert(pcb == _pcb && "Inconsistent _pcb");
                _connectCb();
                return ERR_OK;
            }

            err_t _poll(const tcp_pcb *pcb) const {
                (void)pcb;

                // Call the registered poll callback (for TcpWriter timeout
                // checks)
                if (_pollCb) {
                    _pollCb();
                }
                return ERR_OK;
            }

            static void _s_error(void *arg, const err_t err) {
                if (arg) {
                    const auto *ctx = static_cast<TcpClientContext *>(arg);
                    const_cast<TcpClientContext*>(ctx)->_error(err);
                }
            }

            static err_t _s_poll(void *arg, // NOLINT
                                 const tcp_pcb *tpcb) {
                if (arg) {
                    const auto *ctx = static_cast<TcpClientContext *>(arg);
                    return ctx->_poll(tpcb);
                }
                return ERR_OK;
            }

            static err_t _s_connected(void *arg, const struct tcp_pcb *pcb,
                                      const err_t err) {
                if (arg) {
                    const auto *ctx = static_cast<TcpClientContext *>(arg);
                    return ctx->_connected(pcb, err);
                }
                return ERR_OK;
            }

        private:
            tcp_pcb *_pcb;
            IoRxBuffer *_rx = nullptr;  // Move IoRxBuffer ownership here
            TcpWriter *_tx = nullptr;  // Tx writer (set by higher layer)

            uint32_t _timeout_ms = 5000;

            std::function<void()> _finCb;
            std::function<void()> _connectCb;
            error_cb_t _errorCb;
            received_callback_t _receiveCb;
            std::function<void(struct tcp_pcb *tpcb, uint16_t len)> _ackCb;
            std::function<void()> _closeCb;
            std::function<void(size_t bytes_written)> _writtenCb;
            std::function<void()> _pollCb;

            // --- Client ID for logging and traceability ---
            uint8_t m_client_id = 0; // Smallest integer type for client ID
    };
} // namespace async_tcp
