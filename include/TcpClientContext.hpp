/*
    AsyncTcpClientContext.h - TCP connection handling on top of lwIP

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

#include <Arduino.h>
#include <cassert>
#include <lwip/ip.h>
#include <lwip/opt.h>
#include <lwip/tcp.h>
#include <utility>

namespace async_tcp {

    class TcpClientContext;

    class TcpClient;

    typedef void (*discard_cb_t)(void *, TcpClientContext *);

    class TcpClientContext {
        public:
            TcpClientContext(tcp_pcb *pcb, const discard_cb_t discard_cb,
                                  void *discard_cb_arg)
                : _pcb(pcb), _rx_buf(nullptr), _rx_buf_offset(0),
                  _discard_cb(discard_cb), _discard_cb_arg(discard_cb_arg),
                  _ref_cnt(0), _next(nullptr) {
                // Removed _sync initialization - sync mode is redundant and dangerous
                tcp_setprio(_pcb, TCP_PRIO_MIN);
                tcp_arg(_pcb, this);
                tcp_recv(_pcb, &_s_recv);
                tcp_sent(_pcb, &_s_acked);
                tcp_err(_pcb, &_s_error);
                tcp_poll(_pcb, &_s_poll, 1);
                // keep-alive not enabled by default
                // keepAlive();
            }

            [[maybe_unused]] [[nodiscard]] tcp_pcb *getPCB() const {
                return _pcb;
            }

            err_t abort() {
                if (_pcb) {
                    DEBUGWIRE(":abort\n");
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
                    DEBUGWIRE(":close\n");
                    tcp_arg(_pcb, nullptr);
                    tcp_sent(_pcb, nullptr);
                    tcp_recv(_pcb, nullptr);
                    tcp_err(_pcb, nullptr);
                    tcp_poll(_pcb, nullptr, 0);
                    err = tcp_close(_pcb);
                    if (err != ERR_OK) {
                        DEBUGWIRE(":tc err %d\n", static_cast<int>(err));
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
                DEBUGWIRE(":ref %d\n", _ref_cnt);
            }

            void unref() {
                DEBUGWIRE(":ur %d\n", _ref_cnt);
                if (--_ref_cnt == 0) {
                    discard_received();
                    close();
                    if (_discard_cb) {
                        _discard_cb(_discard_cb_arg, this);
                    }
                    DEBUGWIRE(":del\n");
                    delete this;
                }
            }

            int connect(ip_addr_t *addr, uint16_t port) const {
                // note: not using `const ip_addr_t* addr` because
                // - `ip6_addr_assign_zone()` below modifies `*addr`
                // - caller's parameter `AsyncTcpClient::connect` is a local
                // copy
#if LWIP_IPV6
                // Set zone so that link local addresses use the default
                // interface
                if (IP_IS_V6(addr) &&
                    ip6_addr_lacks_zone(ip_2_ip6(addr), IP6_UNKNOWN)) {
                    ip6_addr_assign_zone(ip_2_ip6(addr), IP6_UNKNOWN,
                                         netif_default);
                }
#endif
                err_t err = tcp_connect(_pcb, addr, port,
                                        &TcpClientContext::_s_connected);
                if (err != ERR_OK) {
                    DEBUGWIRE(":connect err %d\n", static_cast<int>(err));
                    return 0;
                }

                if (!_pcb) {
                    DEBUGWIRE(":cabrt\n");
                    return 0;
                }
                DEBUGWIRE(":conn\n");
                return 1;
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
             * and advances the buffer offset by one. If the receive buffer is
             * empty, the method returns 0.
             *
             * @return The byte read from the receive buffer, or 0 if the buffer
             * is empty.
             *
             * @note This method is not thread-safe. Ensure that appropriate
             * synchronization mechanisms are used if this method is called from
             * multiple threads.
             *
             * @warning If the receive buffer is empty, this method will return
             * 0.
             */
            char read() {
                char c = peek();
                if (c != 0) {
                    _consume(1);
                }
                return c;
            }

            /**
             * @brief Reads data from the receive buffer into the provided
             * destination buffer.
             *
             * This method attempts to read up to `size` bytes from the internal
             * receive buffer into the buffer pointed to by `dst`. The actual
             * number of bytes read may be less than `size` if there is less
             * data available in the receive buffer.
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
             * @warning If the receive buffer is empty, this method will return
             * 0.
             */
            size_t read(char *dst, size_t size) {
                if (!dst || size == 0) {
                    DEBUGWIRE(":read invalid parameters\n");
                    return 0;
                }

                // Get the total amount of data available for reading
                size_t max_size = getSize();
                // Limit size to available data
                size = std::min(size, max_size);

                DEBUGWIRE(":rd %d, %d, %d\n", size, _rx_buf->tot_len,
                          _rx_buf_offset);
                size_t size_read = 0;

                // Keep reading from the buffer while there's data to read
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
             * @brief Peek at the next byte in the receive buffer
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

                DEBUGWIRE(":pd %d, %d, %d\n", size, _rx_buf->tot_len,
                          _rx_buf_offset);
                const size_t buf_size = peekAvailable();
                const size_t copy_size = (size < buf_size) ? size : buf_size;
                DEBUGWIRE(":rpi %d, %d\n", buf_size, copy_size);
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
                DEBUGWIRE(":dsrcv %d\n", _rx_buf ? _rx_buf->tot_len : 0);
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

            bool wait_until_acked(
                int max_wait_ms = ASYNC_TCP_CLIENT_MAX_FLUSH_WAIT_MS) {
                // https://github.com/esp8266/Arduino/pull/3967#pullrequestreview-83451496
                // option 1 done
                // option 2 / _write_some() not necessary since _datasource is
                // always nullptr here

                if (!_pcb) {
                    return true;
                }

                int prevsndbuf = -1;

                // wait for peer's acks to flush lwIP's output buffer
                uint32_t last_sent = millis();
                while (true) {
                    if (millis() - last_sent >
                        static_cast<uint32_t>(max_wait_ms)) {
                        // wait until sent: timeout
                        DEBUGWIRE(":wustmo\n");
                        // All data was not flushed, timeout hit
                        return false;
                    }

                    if (!_pcb) {
                        return false;
                    }
                    // force lwIP to send what can be sent
                    tcp_output(_pcb);

                    int sndbuf = tcp_sndbuf(_pcb);
                    if (sndbuf != prevsndbuf) {
                        // send buffer has changed (or first iteration)
                        prevsndbuf = sndbuf;
                        // We just sent a bit, move timeout forward
                        last_sent = millis();
                    }

                    // esp_yield(); // from sys or os context

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
                    // local copy
                    size_t i;
                    for (i = 0; (i < sizeof(buff)) && stream.available(); i++) {
                        buff[i] = stream.read();
                    }
                    if (i) {
                        // Send as a single packet
                        size_t len =
                            write(reinterpret_cast<const char *>(buff), i);
                        sent += len;
                        if (len != static_cast<int>(i)) {
                            break; // Write error...
                        }
                    } else {
                        // Out of data...
                        break;
                    }
                }
                return sent;
            }

            void
            keepAlive(uint16_t idle_sec = TCP_DEFAULT_KEEP_ALIVE_IDLE_SEC,
                      uint16_t intv_sec = TCP_DEFAULT_KEEP_ALIVE_INTERVAL_SEC,
                      uint8_t count = TCP_DEFAULT_KEEP_ALIVE_COUNT) {
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
                _errorCb = cb; // Set the error callback
            }

            void setOnReceiveCallback(
                const std::function<void(std::unique_ptr<int>)> &cb) {
                _receiveCb = cb;
            }

            void setOnAckCallback(const std::function<void(struct tcp_pcb *tpcb,
                                                           uint16_t len)> &cb) {
                _ackCb = cb;
            }

            void setOnCloseCallback(const std::function<void()> &cb) {
                _closeCb = cb;
            }

        protected:
            // store pending write buffers for async scheduling
            const char* _datasource = nullptr;
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
             * @return true if the operation has timed out (elapsed time >
             * timeout)
             * @return false if the operation is still within the timeout window
             *
             * @note Uses millis() for time measurement which wraps around every
             * ~49 days
             */
            [[nodiscard]] bool _is_timeout(uint32_t start_time) const {
                return millis() - start_time > _timeout_ms;
            }

            void _notify_error() {
                // @todo: consider removing
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
                bool ok = _write_some(_datasource, _datalen, &_written);
                if (!ok) {
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
             * @return true Connection is valid (_pcb exists and state is not
             * CLOSED)
             * @return false Connection is invalid
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
                auto sbuf = static_cast<size_t>(tcp_sndbuf(_pcb));
                DEBUGWIRE(":sbuf %d, rem %d, scale %d\n", sbuf, remaining, scale);
                size_t chunk_size =
                    std::min(sbuf, remaining);

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
             * (more data coming) and if TCP_WRITE_FLAG_COPY should be set
             * (based on sync mode).
             */
            [[nodiscard]] uint8_t
            _get_write_flags(const size_t chunk_size,
                             const size_t remaining) const {
                uint8_t flags = TCP_WRITE_FLAG_COPY;  // Always copy data for safety
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
             * @return true If any data was written
             * @return false If no data could be written
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

                DEBUGWIRE(":wr %d %d\n", data_len - *written, *written);

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

                    DEBUGWIRE(":wrc %d %d %d\n", next_chunk_size, remaining,
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

            //    void _write_some_from_cb() {
            //        if (_send_waiting) {
            //            // resume _write_from_source
            //            _send_waiting = false;
            //            //esp_schedule();
            //        }
            //    }

            err_t _acked(tcp_pcb *pcb, uint16_t len) {
                (void)pcb;
                (void)len;
                // advance acked count and notify
                _written += len;
                _ackCb(pcb, len);
                // reset timeout on progress
                if (len > 0) {
                    _write_start_time = millis();
                }
                // schedule next chunk if data remains
                if (_datasource && _written < _datalen) {
                    bool ok = _write_some(_datasource, _datalen, &_written);
                    if (!ok) {
                        _errorCb(ERR_MEM);
                        _datasource = nullptr;
                        _datalen = 0;
                    }
                } else {
                    // all data sent
                    _datasource = nullptr;
                    _datalen = 0;
                }
                return ERR_OK;
            }

            void _consume(size_t size) {
                ptrdiff_t left = _rx_buf->len - _rx_buf_offset - size;
                if (left > 0) {
                    _rx_buf_offset += size;
                } else if (!_rx_buf->next) {
                    DEBUGWIRE(":c0 %d, %d\n", size, _rx_buf->tot_len);
                    auto head = _rx_buf;
                    _rx_buf = nullptr;
                    _rx_buf_offset = 0;
                    pbuf_free(head);
                } else {
                    DEBUGWIRE(":c %d, %d, %d\n", size, _rx_buf->len,
                              _rx_buf->tot_len);
                    auto head = _rx_buf;
                    _rx_buf = _rx_buf->next;
                    _rx_buf_offset = 0;
                    pbuf_ref(_rx_buf);
                    pbuf_free(head);
                }
                if (_pcb) {
                    tcp_recved(_pcb, size);
                }
            }

            err_t _recv(tcp_pcb *pcb, pbuf *pb, err_t err) {
                (void)pcb;
                (void)err;
                // The remote peer sends a TCP segment with the FIN flag set to
                // close.
                if (pb == nullptr) {
                    DEBUGWIRE(":rcl pb=%p sz=%d\n", _rx_buf,
                              _rx_buf ? _rx_buf->tot_len : -1);
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
                    DEBUGWIRE(":rch %d, %d\n", _rx_buf->tot_len, pb->tot_len);
                    pbuf_cat(_rx_buf, pb);
                } else {
                    DEBUGWIRE(":rn %d\n", pb->tot_len);
                    _rx_buf = pb;
                    _rx_buf_offset = 0;
                }

                auto size = std::make_unique<int>(getSize());
                _receiveCb(std::move(size));

                return ERR_OK;
            }

            void _error(const err_t err) {
                DEBUGWIRE(":er %d 0x%%\n", static_cast<int>(err));
                tcp_arg(_pcb, nullptr);
                tcp_sent(_pcb, nullptr);
                tcp_recv(_pcb, nullptr);
                tcp_err(_pcb, nullptr);
                _pcb = nullptr;
                (void)err;

                _errorCb(err);
                _notify_error();
            }

            err_t _connected(const struct tcp_pcb *pcb, const err_t err) const {
                (void)err;
                (void)pcb;
                assert(pcb == _pcb && "Inconsistent _pcb");
                _connectCb();
                return ERR_OK;
            }

            err_t _poll(tcp_pcb *pcb) {
                // check for pending write and timeout
                if (_datasource && _written < _datalen) {
                    if (millis() - _write_start_time > _timeout_ms) {
                        // write timed out, notify application
                        _errorCb(ERR_TIMEOUT);
                        // abort pending write
                        _datasource = nullptr;
                        _datalen = 0;
                    } else {
                        // continue sending next chunk
                        bool ok = _write_some(_datasource, _datalen, &_written);
                        if (!ok) {
                            _errorCb(ERR_MEM);
                            _datasource = nullptr;
                            _datalen = 0;
                        }
                    }
                }
                return ERR_OK;
            }

            // We may receive a nullptr as arg in the case when an IRQ happens
            // during a shutdown sequence In that case, just ignore the CB
            static err_t _s_recv(void *arg, struct tcp_pcb *tpcb,
                                 struct pbuf *pb, err_t err) {
                if (arg) {
                    auto *context =
                        reinterpret_cast<TcpClientContext *>(arg);
                    return context->_recv(tpcb, pb, err);
                } else {
                    return ERR_OK;
                }
            }

            static void _s_error(void *arg, err_t err) {
                if (arg) {
                    reinterpret_cast<TcpClientContext *>(arg)->_error(err);
                }
            }

            static err_t _s_poll(void *arg, struct tcp_pcb *tpcb) {
                if (arg) {
                    return reinterpret_cast<TcpClientContext *>(arg)
                        ->_poll(tpcb);
                } else {
                    return ERR_OK;
                }
            }

            static err_t _s_acked(void *arg, struct tcp_pcb *tpcb,
                                  uint16_t len) {
                if (arg) {
                    return reinterpret_cast<TcpClientContext *>(arg)
                        ->_acked(tpcb, len);
                } else {
                    return ERR_OK;
                }
            }

            static err_t _s_connected(void *arg, struct tcp_pcb *pcb,
                                      err_t err) {
                if (arg) {
                    return reinterpret_cast<TcpClientContext *>(arg)
                        ->_connected(pcb, err);
                } else {
                    return ERR_OK;
                }
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
            std::function<void(std::unique_ptr<int>)> _receiveCb;
            std::function<void(struct tcp_pcb *tpcb, uint16_t len)> _ackCb;
            std::function<void()> _closeCb;
    };
} // namespace AsyncTcp
