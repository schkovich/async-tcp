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
#include "TcpClient.hpp"

#include <Arduino.h>
#include <cassert>
#include <functional>
#include <lwip/ip.h>
#include <lwip/opt.h>
#include <lwip/tcp.h>
#include <utility>

namespace async_tcp {

    class TcpClientContext;

    class TcpClient;

    typedef void (*discard_cb_t)(void *, TcpClientContext *);

    using receive_cb_t = std::function<void(std::unique_ptr<int>)>;

    class TcpClientContext {
        public:
            TcpClientContext(tcp_pcb *pcb, const discard_cb_t discard_cb,
                             void *discard_cb_arg)
                : _pcb(pcb), _rx_buf(nullptr), _rx_buf_offset(0),
                  _discard_cb(discard_cb), _discard_cb_arg(discard_cb_arg),
                  _ref_cnt(0), _next(nullptr) {
                tcp_setprio(_pcb, TCP_PRIO_MIN);
                // NOTE: Do NOT set tcp_arg here - it will be set by TcpClient::connect()
                // All callbacks will get the same arg (TcpClient*) as set by tcp_arg()
                tcp_recv(_pcb, lwip_receive_callback);
                tcp_sent(_pcb, &_s_acked);
                tcp_err(_pcb, &_s_error);
                tcp_poll(_pcb, reinterpret_cast<tcp_poll_fn>(&_s_poll), 1);
            }

            [[maybe_unused]] [[nodiscard]] tcp_pcb *getPCB() const {
                return _pcb;
            }

            err_t abort() {
                if (_pcb) {
                    DEBUGWIRE("[:i%d] :abort\n", getClientId());
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

            ~TcpClientContext() = default;

            [[nodiscard]] TcpClientContext *next() const { return _next; }

            TcpClientContext *next(TcpClientContext *new_next) {
                _next = new_next;
                return _next;
            }

            void ref() {
                ++_ref_cnt;
                DEBUGWIRE("[:i%d] :ref %d\n", getClientId(), _ref_cnt);
            }

            void unref() {
                DEBUGWIRE("[:i%d] :ur %d\n", getClientId(), _ref_cnt);
                if (--_ref_cnt == 0) {
                    discard_received();
                    close();
                    if (_discard_cb) {
                        _discard_cb(_discard_cb_arg, this);
                    }
                    DEBUGWIRE("[:i%d] :del\n", getClientId());
                    delete this;
                }
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

            [[nodiscard]] size_t availableForWrite() const {
                return _pcb ? tcp_sndbuf(_pcb) : 0;
            }

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

            [[nodiscard]] size_t getSize() const {
                if (!_rx_buf) {
                    return 0;
                }
                return _rx_buf->len - _rx_buf_offset;
            }

            /**
             * @brief Reads a single byte from the internal receive buffer.
             *
             * This method retrieves one byte from the internal receive buffer
             * and advances the buffer offset by one. If the buffer is
             * empty, the method returns 0.
             *
             * @return The byte read from the internal receive buffer, or 0 if
             * the buffer is empty.
             *
             * @note This method is not thread-safe. Ensure that appropriate
             * synchronization mechanisms are used if this method is called from
             * multiple threads.
             *
             * @warning If the buffer is empty, this method will return
             * 0.
             */
            char read() {
                const char c = peek();
                if (c != 0) {
                    _consume(1);
                }
                return c;
            }

            /**
             * @brief Reads data from the LwIP receive buffer into the provided
             * destination buffer.
             *
             * This method attempts to read up to `size` bytes from the internal
             * receive buffer into the buffer pointed to by `dst`. The actual
             * number of bytes read may be less than `size` if there is less
             * data available in the internal receive buffer.
             *
             * @param dst Pointer to the destination buffer where the data will
             * be copied.
             * @param size The maximum number of bytes to read into the
             * destination buffer.
             * @return The actual number of bytes read into the destination
             * buffer.
             *
             * @note This method is not thread-safe. Ensure that appropriate
             * synchronization mechanisms are used if this method is called from
             * multiple threads.
             *
             * @warning If the buffer is empty, this method will return
             * 0.
             */
            size_t read(char *dst, size_t size) {
                assert(dst);
                if (size == 0) {
                    return size;
                }

                // Get the total amount of data available for reading
                const auto max_size = getSize();
                // Limit size to available data
                size = std::min(size, max_size);

                DEBUGWIRE("[:i%d] :rd %d, %d, %d\n", getClientId(), size,
                          _rx_buf->tot_len, _rx_buf_offset);
                size_t size_read = 0;

                // Keep reading from the buffer while there's data to read.
                while (size > 0) {
                    // Use peekBytes to copy data into the destination buffer
                    const size_t copy_size = peekBytes(dst, size);
                    if (copy_size == 0) {
                        break;
                    }
                    dst += copy_size;

                    // Mark the copied bytes as consumed
                    _consume(copy_size);

                    // Reduce the remaining size and update the total size read
                    size -= copy_size;
                    size_read += copy_size;
                }

                return size_read;
            }

            /**
             * @brief A group of functions for non-consuming inspection and
             * controlled consumption of received data
             *
             * These functions provide a mechanism to inspect and process
             * received data without immediately consuming it. Data can only be
             * consumed after successful processing using peekConsume().
             */

            /**
             * @brief Peek at the next byte in the internal receive buffer
             * @return The next byte, or 0 if buffer is empty
             * @note Does not consume the byte
             */
            [[nodiscard]] char peek() const {
                if (!_rx_buf) {
                    return 0;
                }

                return static_cast<char *>(_rx_buf->payload)[_rx_buf_offset];
            }

            /**
             * @brief Copy available bytes into a destination buffer without
             * consuming
             * @param dst Destination buffer
             * @param size Maximum number of bytes to copy
             * @return Number of bytes actually copied
             * @note Does not consume the bytes
             */
            size_t peekBytes(char *dst, size_t size) const {
                if (!_rx_buf) {
                    return 0;
                }

                const size_t max_size = getSize();
                size = (size < max_size) ? size : max_size;

                DEBUGWIRE("[:i%d] :pd %d, %d, %d\n", getClientId(), size,
                          _rx_buf->tot_len, _rx_buf_offset);
                const size_t buf_size = peekAvailable();
                const size_t copy_size = (size < buf_size) ? size : buf_size;
                DEBUGWIRE("[:i%d] :rpi %d, %d\n", getClientId(), buf_size,
                          copy_size);
                memcpy(dst,
                       static_cast<char *>(_rx_buf->payload) + _rx_buf_offset,
                       copy_size);
                return copy_size;
            }

            // return a pointer to available data buffer (size =
            // peekAvailable()) semantic forbids any kind of read() before
            // calling peekConsume()
            /**
             * @brief Get direct pointer to available data in receive buffer
             * @return Pointer to data buffer, or nullptr if empty
             * @note Any read() operation is forbidden before calling
             * peekConsume()
             */
            [[nodiscard]] const char *peekBuffer() const {
                if (!_rx_buf) {
                    return nullptr;
                }
                return static_cast<const char *>(_rx_buf->payload) +
                       _rx_buf_offset;
            }

            // return number of byte accessible by peekBuffer()
            /**
             * @brief Get number of bytes available in current receive buffer
             * @return Number of available bytes
             */
            [[nodiscard]] size_t peekAvailable() const {
                if (!_rx_buf) {
                    return 0;
                }
                return _rx_buf->len - _rx_buf_offset;
            }

            // consume bytes after use (see peekBuffer)
            /**
             * @brief Consume specified number of bytes after processing
             * @param consume Number of bytes to consume
             * @note Must be called after successful processing of peeked data
             */
            void peekConsume(const size_t consume) { _consume(consume); }

            void discard_received() {
                DEBUGWIRE("[:i%d] :dsrcv %d\n", getClientId(),
                          _rx_buf ? _rx_buf->tot_len : 0);
                if (!_rx_buf) {
                    return;
                }
                if (_pcb) {
                    tcp_recved(_pcb, (size_t)_rx_buf->tot_len);
                }
                pbuf_free(_rx_buf);
                _rx_buf = nullptr;
                _rx_buf_offset = 0;
            }

            [[nodiscard]] bool
            wait_until_acked(const int max_wait_ms =
                                 ASYNC_TCP_CLIENT_MAX_FLUSH_WAIT_MS) const {
                // https://github.com/esp8266/Arduino/pull/3967#pullrequestreview-83451496
                // option 1 done
                // option 2 / _write_some() not necessary since _datasource is
                // always nullptr here.

                if (!_pcb) {
                    return true;
                }

                int prevsndbuf = -1;

                // wait for a peer's ACKs to flush lwIP's output buffer,
                uint32_t last_sent = millis();
                while (true) {
                    if (millis() - last_sent >
                        static_cast<uint32_t>(max_wait_ms)) {
                        // wait until sent: timeout
                        DEBUGWIRE("[:i%d] :wustmo\n", getClientId());
                        // All data not flushed, timeout hit
                        return false;
                    }

                    if (!_pcb) { // NOLINT
                        // No PCB, connection closed
                        DEBUGWIRE("[:i%d] :wustpcb\n", getClientId());
                        return false; // NOLINT
                    }
                    // force lwIP to send what can be sent
                    tcp_output(_pcb);

                    const int sndbuf = tcp_sndbuf(_pcb);
                    if (sndbuf != prevsndbuf) {
                        // send buffer has changed (or first iteration)
                        prevsndbuf = sndbuf;
                        // We just sent a bit, move timeout forward
                        last_sent = millis();
                    }

                    if ((state() != ESTABLISHED) || (sndbuf == TCP_SND_BUF)) {
                        // peer has closed or all bytes are sent and acked
                        // ((TCP_SND_BUF-sndbuf) is the amount of un-acked
                        // bytes)
                        break;
                    }
                }

                // All data flushed
                return true;
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

            size_t write(const char *ds, const size_t dl) {
                if (!_pcb) {
                    return 0;
                }
                return _write_from_source(ds, dl);
            }

            size_t write(Stream &stream) {
                if (!_pcb) {
                    return 0;
                }
                size_t sent = 0;
                uint8_t buff[256];
                while (stream.available()) {
                    // Stream only lets you read 1 byte at a time, so buffer in
                    // a local copy.
                    size_t i;
                    for (i = 0; (i < sizeof(buff)) && stream.available(); i++) {
                        buff[i] = stream.read();
                    }
                    if (i) {
                        // Send as a single packet
                        const size_t len =
                            write(reinterpret_cast<const char *>(buff), i); // NOLINT
                        sent += len;
                        if (len != static_cast<int>(i)) {
                            break; // Write error…
                        }
                    } else {
                        // Out of data…
                        break;
                    }
                }
                return sent;
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

            void setOnErrorCallback(const std::function<void(err_t err)> &cb) {
                _errorCb = cb;
            }

            /**
             * @deprecated
             * @param cb
             */
            void setOnReceiveCallback(
                const receive_cb_t &cb) {
                _receiveCb = cb;
            }

            void setOnAckCallback(const std::function<void(struct tcp_pcb *tpcb,
                                                           uint16_t len)> &cb) {
                _ackCb = cb;
            }

            /**
             * @deprecated
             * @param cb
             */
            void setOnCloseCallback(const std::function<void()> &cb) {
                _closeCb = cb;
            }

            void setOnWrittenCallback(
                const std::function<void(size_t bytes_written)> &cb) {
                _writtenCb = cb;
            }

            void setOnPollCallback(const std::function<void()> &cb) {
                _pollCb = cb;
            }

            /**
             * @brief Set the client ID for this TcpClientContext instance.
             * @param id The client ID to assign (uint8_t)
             */
            void setClientId(const uint8_t id) { m_client_id = id; }

        protected:
            // store pending write buffers for async scheduling
            const char *_datasource = nullptr;
            size_t _datalen = 0;
            // timestamp when the first write chunk was scheduled
            uint32_t _write_start_time = 0;

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
             * @brief Write data from source buffer to TCP connection
             *
             * @param ds Pointer to source data buffer
             * @param dl Length of data to write
             * @return size_t Number of bytes written
             *
             * This function attempts to write all data from the source buffer.
             * It returns when either all data is written, timeout occurs,
             * or connection becomes invalid.
             */
            size_t _write_from_source(const char *ds, const size_t dl) {
                // schedule initial write and save state for async continuation
                _datasource = ds;
                _datalen = dl;
                _written = 0;
                // record write start for timeout
                _write_start_time = millis();
                if (const bool ok =
                        _write_some(_datasource, _datalen, &_written);
                    !ok) {
                    // nothing sent, abort scheduling
                    _datasource = nullptr;
                    _datalen = 0;
                    return 0;
                }
                return _written;
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

            /**
             * @brief Write a portion of data to TCP connection
             *
             * @param datasource Source data buffer
             * @param data_len Total length of data
             * @param written Pointer to track number of bytes written
             * @return `true` If any data was written
             * @return `false` If no data could be written
             *
             * This function attempts to write data in chunks, handling TCP
             * buffer constraints and memory limitations through scaling
             * mechanism.
             */
            bool _write_some(const char *datasource, const size_t data_len,
                             size_t *written) const {
                if (!datasource || !_is_connection_valid()) {
                    return false;
                }

                DEBUGWIRE("[:i%d] :wr %d %d\n", getClientId(),
                          data_len - *written, *written);

                bool has_written = false;
                int scale = 0;

                while (*written < data_len) {
                    if (!_is_connection_valid()) {
                        return false;
                    }

                    const auto remaining = data_len - *written;
                    const auto next_chunk_size =
                        _calculate_chunk_size(remaining, scale);

                    if (!next_chunk_size) {
                        return false;
                    }

                    const uint8_t flags =
                        _get_write_flags(next_chunk_size, remaining);
                    const err_t err = tcp_write(_pcb, &datasource[*written],
                                                next_chunk_size, flags);

                    DEBUGWIRE("[:i%d] :wrc %d %d %d\n", getClientId(),
                              next_chunk_size, remaining,
                              static_cast<int>(err));

                    if (err == ERR_OK) {
                        *written += next_chunk_size;
                        has_written = true;
                    } else if (err == ERR_MEM) {
                        if (scale < 4) {
                            scale++;
                        } else {
                            break;
                        }
                    } else {
                        break;
                    }
                }

                if (has_written && _is_connection_valid()) {
                    tcp_output(_pcb);
                }

                return has_written;
            }

            err_t _acked(tcp_pcb *pcb, const uint16_t len) const {
                // Notify the integration layer via a callback.
                // The integration layer (e5::TcpAckHandler) will handle the
                // chunking policy.
                if (_ackCb) {
                    _ackCb(pcb, len);
                }
                return ERR_OK;
            }

            void _consume(const size_t size) {
                if (const auto left = static_cast<ptrdiff_t>(
                        _rx_buf->len - _rx_buf_offset - size);
                    left > 0) {
                    _rx_buf_offset += size;
                } else if (!_rx_buf->next) {
                    DEBUGWIRE("[:i%d] :c0 %d, %d\n", getClientId(), size,
                              _rx_buf->tot_len);
                    const auto head = _rx_buf;
                    _rx_buf = nullptr;
                    _rx_buf_offset = 0;
                    pbuf_free(head);
                } else {
                    DEBUGWIRE("[:i%d] :c %d, %d, %d\n", getClientId(), size,
                              _rx_buf->len, _rx_buf->tot_len);
                    const auto head = _rx_buf;
                    _rx_buf = _rx_buf->next;
                    _rx_buf_offset = 0;
                    pbuf_ref(_rx_buf);
                    pbuf_free(head);
                }
                if (_pcb) {
                    tcp_recved(_pcb, size);
                }
            }

            err_t _recv(const tcp_pcb *pcb, pbuf *pb, const err_t err) {
                (void)pcb;
                (void)err;
                // The remote peer sends a TCP segment with the FIN flag set to
                // close.
                if (pb == nullptr) {
                    DEBUGWIRE("[:i%d] :rcl pb=%p sz=%d\n", getClientId(),
                              _rx_buf, _rx_buf ? _rx_buf->tot_len : -1);
                    // flush any remaining data first
                    if (_rx_buf && _rx_buf->tot_len) {
                        auto size = std::make_unique<int>(getSize());
                        _receiveCb(std::move(size));
                    }

                    if (_closeCb) {
                        _closeCb();
                    }

                    return ERR_ABRT;
                }

                if (_rx_buf) {
                    DEBUGWIRE("[:i%d] :rch %d, %d\n", getClientId(),
                              _rx_buf->tot_len, pb->tot_len);
                    pbuf_cat(_rx_buf, pb);
                } else {
                    DEBUGWIRE("[:i%d] :rn %d\n", getClientId(), pb->tot_len);
                    _rx_buf = pb;
                    _rx_buf_offset = 0;
                }

                auto size = std::make_unique<int>(getSize());
                _receiveCb(std::move(size));

                return ERR_OK;
            }

            void _error(const err_t err) {
                DEBUGWIRE("[:i%d] :er %d 0x%%\n", getClientId(),
                          static_cast<int>(err));
                _pcb = nullptr;
                _errorCb(err);
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

            // We may receive a nullptr as arg in the case when an IRQ happens
            // during a shutdown sequence In that case, just ignore the CB.
            static err_t _s_recv(void *arg, const tcp_pcb *tpcb, pbuf *pb,
                                 const err_t err) {
                if (arg) {
                    auto *context = static_cast<TcpClientContext *>(arg);
                    return context->_recv(tpcb, pb, err);
                }

                return ERR_OK;
            }

            static void _s_error(void *arg, const err_t err) {
                if (arg) {
                    const auto jump = static_cast<TcpClient *>(arg);
                        jump->getContext()->_error(err);
                }
            }

            static err_t _s_poll(void *arg, // NOLINT
                                 const tcp_pcb *tpcb) {
                if (arg) {
                    const auto jump = static_cast<TcpClient *>(arg);
                    return jump->getContext()->_poll(tpcb);
                }
                return ERR_OK;
            }

            static err_t _s_acked(void *arg, tcp_pcb *tpcb, // NOLINT
                                  const uint16_t len) {
                if (arg) {
                    const auto jump = static_cast<TcpClient *>(arg);
                    return jump->getContext()->_acked(tpcb,
                                                                        len);
                }

                return ERR_OK;
            }

            static err_t _s_connected(void *arg, const struct tcp_pcb *pcb,
                                      const err_t err) {
                if (arg) {
                    const auto jump = static_cast<TcpClient *>(arg);
                    return jump->getContext()->_connected(pcb, err);
                }
                return ERR_OK;
            }

        private:
            tcp_pcb *_pcb;

            pbuf *_rx_buf;
            size_t _rx_buf_offset;

            discard_cb_t _discard_cb;
            void *_discard_cb_arg;

            uint32_t _timeout_ms = 5000;
            size_t _written = 0;
            //    bool _connect_pending = false;

            int8_t _ref_cnt;
            TcpClientContext *_next;
            std::function<void()> _connectCb;
            std::function<void(err_t err)> _errorCb;
            receive_cb_t _receiveCb;
            std::function<void(struct tcp_pcb *tpcb, uint16_t len)> _ackCb;
            std::function<void()> _closeCb;
            std::function<void(size_t bytes_written)> _writtenCb;
            std::function<void()> _pollCb;

            // --- Client ID for logging and traceability ---
            uint8_t m_client_id = 0; // Smallest integer type for client ID
            /**
             * @brief Get the client ID (for internal logging)
             * @return uint8_t client id
             */
            [[nodiscard]] uint8_t getClientId() const { return m_client_id; }
    };
} // namespace async_tcp
