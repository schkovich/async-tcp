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

#include <cassert>
#include <utility>
#include "lwip/timeouts.h"
#include "Arduino.h"

namespace AsyncTcp {

    class AsyncTcpClientContext;

    class AsyncTcpClient;

    typedef void (*discard_cb_t)(void *, AsyncTcpClientContext *);

    class AsyncTcpClientContext {
    public:
        AsyncTcpClientContext(tcp_pcb *pcb, discard_cb_t discard_cb, void *discard_cb_arg) :
                _pcb(pcb), _rx_buf(nullptr), _rx_buf_offset(0), _discard_cb(discard_cb),
                _discard_cb_arg(discard_cb_arg),
                _ref_cnt(0), _next(nullptr),
                _sync(AsyncTcpClient::getDefaultSync()) {
            tcp_setprio(_pcb, TCP_PRIO_MIN);
            tcp_arg(_pcb, this);
            tcp_recv(_pcb, &_s_recv);
            tcp_sent(_pcb, &_s_acked);
            tcp_err(_pcb, &_s_error);
            tcp_poll(_pcb, &_s_poll, 1);
            // keep-alive not enabled by default
            //keepAlive();
        }

        [[maybe_unused]] tcp_pcb *getPCB() {
            return _pcb;
        }

        err_t abort() {
            if (_pcb) {
                DEBUGV(":abort\r\n");
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
                DEBUGV(":close\r\n");
                tcp_arg(_pcb, nullptr);
                tcp_sent(_pcb, nullptr);
                tcp_recv(_pcb, nullptr);
                tcp_err(_pcb, nullptr);
                tcp_poll(_pcb, nullptr, 0);
                err = tcp_close(_pcb);
                if (err != ERR_OK) {
                    DEBUGV(":tc err %d\r\n", (int) err);
                    tcp_abort(_pcb);
                    err = ERR_ABRT;
                }
                _pcb = nullptr;
            }
            return err;
        }

        ~AsyncTcpClientContext() = default;

        [[nodiscard]] AsyncTcpClientContext *next() const {
            return _next;
        }

        AsyncTcpClientContext *next(AsyncTcpClientContext *new_next) {
            _next = new_next;
            return _next;
        }

        void ref() {
            ++_ref_cnt;
            DEBUGV(":ref %d\r\n", _ref_cnt);
        }

        void unref() {
            DEBUGV(":ur %d\r\n", _ref_cnt);
            if (--_ref_cnt == 0) {
                discard_received();
                close();
                if (_discard_cb) {
                    _discard_cb(_discard_cb_arg, this);
                }
                DEBUGV(":del\r\n");
                delete this;
            }
        }

        int connect(ip_addr_t *addr, uint16_t port) {
            // note: not using `const ip_addr_t* addr` because
            // - `ip6_addr_assign_zone()` below modifies `*addr`
            // - caller's parameter `AsyncTcpClient::connect` is a local copy
#if LWIP_IPV6
            // Set zone so that link local addresses use the default interface
            if (IP_IS_V6(addr) && ip6_addr_lacks_zone(ip_2_ip6(addr), IP6_UNKNOWN)) {
                ip6_addr_assign_zone(ip_2_ip6(addr), IP6_UNKNOWN, netif_default);
            }
#endif
            err_t err = tcp_connect(_pcb, addr, port, &AsyncTcpClientContext::_s_connected);
            if (err != ERR_OK) {
                Serial.println("LWIP tcp_connect failed.");
                Serial.print(err);
                Serial.println("");
                return 0;
            }

            if (!_pcb) {
                DEBUGV(":cabrt\r\n");
                Serial.println("No PCB here,");
                return 0;
            }
            return 1;
        }

        [[nodiscard]] size_t availableForWrite() const {
            return _pcb ? tcp_sndbuf(_pcb) : 0;
        }

        void setNoDelay(bool nodelay) {
            if (!_pcb) {
                return;
            }
            if (nodelay) {
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

        void setTimeout(int timeout_ms) {
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

            return _rx_buf->tot_len - _rx_buf_offset;
        }

        /**
         * @brief Reads a single byte from the internal receive buffer.
         *
         * This method retrieves one byte from the internal receive buffer and advances
         * the buffer offset by one. If the receive buffer is empty, the method returns 0.
         *
         * @return The byte read from the receive buffer, or 0 if the buffer is empty.
         *
         * @note This method is not thread-safe. Ensure that appropriate synchronization
         * mechanisms are used if this method is called from multiple threads.
         *
         * @warning If the receive buffer is empty, this method will return 0.
         */
        char read() {
            char c = peek();
            if (c != 0) {
                _consume(1);
            }
            return c;
        }

        /**
         * @brief Reads data from the receive buffer into the provided destination buffer.
         *
         * This method attempts to read up to `size` bytes from the internal receive buffer
         * into the buffer pointed to by `dst`. The actual number of bytes read may be less
         * than `size` if there is less data available in the receive buffer.
         *
         * @param dst Pointer to the destination buffer where the data will be copied.
         * @param size The maximum number of bytes to read into the destination buffer.
         * @return The actual number of bytes read into the destination buffer.
         *
         * @note This method is not thread-safe. Ensure that appropriate synchronization
         * mechanisms are used if this method is called from multiple threads.
         *
         * @warning If the receive buffer is empty, this method will return 0.
         */
        size_t read(char *dst, size_t size) {
            if (!dst || size == 0) {
                DEBUGV(":read invalid parameters\r\n");
                return 0;
            }

            // Get the total amount of data available for reading
            size_t max_size = getSize();
            // Limit size to available data
            size = std::min(size, max_size);

            DEBUGV(":rd %d, %d, %d\r\n", size, _rx_buf->tot_len, _rx_buf_offset);
            size_t size_read = 0;

            // Keep reading from the buffer while there's data to read
            while (size > 0) {
                 // Use peekBytes to copy data into the destination buffer
                size_t copy_size = peekBytes(dst, size);
                if (copy_size == 0) {
                    DEBUGV(":read no more data to copy\r\n");
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

        [[nodiscard]] char peek() const {
            if (!_rx_buf) {
                return 0;
            }

            return reinterpret_cast<char *>(_rx_buf->payload)[_rx_buf_offset];
        }

        size_t peekBytes(char *dst, size_t size) const {
            if (!_rx_buf) {
                return 0;
            }

            size_t max_size = getSize();
            size = (size < max_size) ? size : max_size;

            DEBUGV(":pd %d, %d, %d\r\n", size, _rx_buf->tot_len, _rx_buf_offset);
            size_t buf_size = _rx_buf->len - _rx_buf_offset;
            size_t copy_size = (size < buf_size) ? size : buf_size;
            DEBUGV(":rpi %d, %d\r\n", buf_size, copy_size);
            memcpy(dst, reinterpret_cast<char *>(_rx_buf->payload) + _rx_buf_offset, copy_size);
            return copy_size;
        }

        // return a pointer to available data buffer (size = peekAvailable())
        // semantic forbids any kind of read() before calling peekConsume()
        const char *peekBuffer() {
            if (!_rx_buf) {
                return nullptr;
            }
            return (const char *) _rx_buf->payload + _rx_buf_offset;
        }

        // return number of byte accessible by peekBuffer()
        size_t peekAvailable() {
            if (!_rx_buf) {
                return 0;
            }
            return _rx_buf->len - _rx_buf_offset;
        }

        // consume bytes after use (see peekBuffer)
        void peekConsume(size_t consume) {
            _consume(consume);
        }

        void discard_received() {
            DEBUGV(":dsrcv %d\n", _rx_buf ? _rx_buf->tot_len : 0);
            if (!_rx_buf) {
                return;
            }
            if (_pcb) {
                tcp_recved(_pcb, (size_t) _rx_buf->tot_len);
            }
            pbuf_free(_rx_buf);
            _rx_buf = nullptr;
            _rx_buf_offset = 0;
        }

        bool wait_until_acked(int max_wait_ms = ASYNC_TCP_CLIENT_MAX_FLUSH_WAIT_MS) {
            // https://github.com/esp8266/Arduino/pull/3967#pullrequestreview-83451496
            // option 1 done
            // option 2 / _write_some() not necessary since _datasource is always nullptr here

            if (!_pcb) {
                return true;
            }

            int prevsndbuf = -1;

            // wait for peer's acks to flush lwIP's output buffer
            uint32_t last_sent = millis();
            while (true) {
                if (millis() - last_sent > (uint32_t) max_wait_ms) {
#ifdef DEBUGV
                    // wait until sent: timeout
                    DEBUGV(":wustmo\n");
#endif
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
                    // ((TCP_SND_BUF-sndbuf) is the amount of un-acked bytes)
                    break;
                }
            }

            // All data flushed
            return true;
        }

        [[nodiscard]] uint8_t state() const {
            if (!_pcb || _pcb->state == CLOSE_WAIT || _pcb->state == CLOSING) {
                // CLOSED for WiFIClient::status() means nothing more can be written
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
                // Stream only lets you read 1 byte at a time, so buffer in local copy
                size_t i;
                for (i = 0; (i < sizeof(buff)) && stream.available(); i++) {
                    buff[i] = stream.read();
                }
                if (i) {
                    // Send as a single packet
                    size_t len = write((const char *) buff, i);
                    sent += len;
                    if (len != (int) i) {
                        break; // Write error...
                    }
                } else {
                    // Out of data...
                    break;
                }
            }
            return sent;
        }

        void keepAlive(uint16_t idle_sec = TCP_DEFAULT_KEEP_ALIVE_IDLE_SEC,
                       uint16_t intv_sec = TCP_DEFAULT_KEEP_ALIVE_INTERVAL_SEC,
                       uint8_t count = TCP_DEFAULT_KEEP_ALIVE_COUNT) {
            if (idle_sec && intv_sec && count) {
                _pcb->so_options |= SOF_KEEPALIVE;
                _pcb->keep_idle = (uint32_t) 1000 * idle_sec;
                _pcb->keep_intvl = (uint32_t) 1000 * intv_sec;
                _pcb->keep_cnt = count;
            } else {
                _pcb->so_options &= ~SOF_KEEPALIVE;
            }
        }

        [[nodiscard]] bool isKeepAliveEnabled() const {
            return !!(_pcb->so_options & SOF_KEEPALIVE);
        }

        [[nodiscard]] uint16_t getKeepAliveIdle() const {
            return isKeepAliveEnabled() ? (_pcb->keep_idle + 500) / 1000 : 0;
        }

        [[nodiscard]] uint16_t getKeepAliveInterval() const {
            return isKeepAliveEnabled() ? (_pcb->keep_intvl + 500) / 1000 : 0;
        }

        [[nodiscard]] uint8_t getKeepAliveCount() const {
            return isKeepAliveEnabled() ? _pcb->keep_cnt : 0;
        }

        [[nodiscard]] bool getSync() const {
            return _sync;
        }

        void setSync(bool sync) {
            _sync = sync;
        }

        void setOnConnectCallback(const std::function<void()> &cb) {
            _connectCb = cb;  // Set the success callback
        }

        void setOnErrorCallback(const std::function<void(err_t err)> &cb) {
            _errorCb = cb;  // Set the error callback
        }

        void setOnReceiveCallback(const std::function<void(std::unique_ptr<int>)> &cb) {
            _receiveCb = cb;
        }

        void setOnAckCallback(const std::function<void(struct tcp_pcb *tpcb, uint16_t len)> &cb) {
            _ackCb = cb;
        }

    protected:

        [[nodiscard]] bool _is_timeout() const {
            return millis() - _op_start_time > _timeout_ms;
        }

        void _notify_error() {
            // @todo: consider removing
        }

        size_t _write_from_source(const char *ds, const size_t dl) {
            assert(_datasource == nullptr);
//        assert(!_send_waiting);
            _datasource = ds;
            _data_len = dl;
            _written = 0;
            _op_start_time = millis();
            do {
                if (_write_some()) {
                    _op_start_time = millis();
                }

                if (_written == _data_len || _is_timeout() || state() == CLOSED) {
                    if (_is_timeout()) {
                        DEBUGV(":wtmo\r\n");
                    }
                    _datasource = nullptr;
                    _data_len = 0;
                    break;
                }

//            _send_waiting = true;
                // will resume on timeout or when _write_some_from_cb or _notify_error fires
                // give scheduled functions a chance to run (e.g. Ethernet uses recurrent)
//            esp_delay(_timeout_ms, [this]() {
//                return this->_send_waiting;
//            }, 1);
//            _send_waiting = false;
            } while (true);

            if (_sync) {
                wait_until_acked();
            }

            return _written;
        }

        bool _write_some() {
            if (!_datasource || !_pcb) {
                return false;
            }

            DEBUGV(":wr %d %d\r\n", _data_len - _written, _written);

            bool has_written = false;
            int scale = 0;

            while (_written < _data_len) {
                if (state() == CLOSED) {
                    return false;
                }
                const auto remaining = _data_len - _written;
                size_t next_chunk_size;
                if (state() == CLOSED) {
                    return false;
                }
                next_chunk_size = std::min((size_t) tcp_sndbuf(_pcb), remaining);
                // Potentially reduce transmit size if we are tight on memory, but only if it doesn't return a 0 chunk size
                if (next_chunk_size > (size_t) (1 << scale)) {
                    next_chunk_size >>= scale;
                }
                if (!next_chunk_size) {
                    return false;
                }
                const char *buf = _datasource + _written;

                uint8_t flags = 0;
                if (next_chunk_size < remaining)
                    //   PUSH is meant for peer, telling to give data to user app as soon as received
                    //   PUSH "may be set" when sender has finished sending a "meaningful" data block
                    //   PUSH does not break Nagle
                    //   #5173: windows needs this flag
                    //   more info: https://lists.gnu.org/archive/html/lwip-users/2009-11/msg00018.html
                {
                    flags |= TCP_WRITE_FLAG_MORE;    // do not tcp-PuSH (yet)
                }
                if (!_sync)
                    // user data must be copied when data are sent but not yet acknowledged
                    // (with sync, we wait for acknowledgment before returning to user)
                {
                    flags |= TCP_WRITE_FLAG_COPY;
                }

                if (state() == CLOSED) {
                    return false;
                }

                err_t err = tcp_write(_pcb, buf, next_chunk_size, flags);

                DEBUGV(":wrc %d %d %d\r\n", next_chunk_size, remaining, (int) err);

                if (err == ERR_OK) {
                    _written += next_chunk_size;
                    has_written = true;
                } else if (err == ERR_MEM) {
                    if (scale < 4) {
                        // Retry sending at 1/2 the chunk size
                        scale++;
                    } else {
                        break;
                    }
                } else {
                    // ERR_MEM(-1) is a valid error meaning
                    // "come back later". It leaves state() opened
                    break;
                }
            }

            if (has_written && (state() != CLOSED)) {
                // lwIP's tcp_output doc: "Find out what we can send and send it"
                // *with respect to Nagle*
                // more info: https://lists.gnu.org/archive/html/lwip-users/2017-11/msg00134.html
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
            (void) pcb;
            (void) len;
            DEBUGV(":ack %d\r\n", len);
//        Serial.print("ACK len: ");
//        Serial.println(len);
//        _write_some_from_cb();
            _written += len;
//        tcp_recved(pcb, len);
            _ackCb(pcb, len);
            return ERR_OK;
        }

        void _consume(size_t size) {
            ptrdiff_t left = _rx_buf->len - _rx_buf_offset - size;
            if (left > 0) {
                _rx_buf_offset += size;
            } else if (!_rx_buf->next) {
                DEBUGV(":c0 %d, %d\r\n", size, _rx_buf->tot_len);
                auto head = _rx_buf;
                _rx_buf = nullptr;
                _rx_buf_offset = 0;
                pbuf_free(head);
            } else {
                DEBUGV(":c %d, %d, %d\r\n", size, _rx_buf->len, _rx_buf->tot_len);
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
            (void) pcb;
            (void) err;
            if (pb == nullptr) {
                // connection closed by peer
                DEBUGV(":rcl pb=%p sz=%d\r\n", _rx_buf, _rx_buf ? _rx_buf->tot_len : -1);
                _notify_error();
                if (_rx_buf && _rx_buf->tot_len) {
                    // there is still something to read
                    return ERR_OK;
                } else {
                    // nothing in receive buffer,
                    // peer closed = nothing can be written:
                    // closing in the legacy way
                    abort();
                    return ERR_ABRT;
                }
            }

            if (_rx_buf) {
                DEBUGV(":rch %d, %d\r\n", _rx_buf->tot_len, pb->tot_len);
                pbuf_cat(_rx_buf, pb);
            } else {
                DEBUGV(":rn %d\r\n", pb->tot_len);
                _rx_buf = pb;
                _rx_buf_offset = 0;
            }

            std::unique_ptr<int> size = std::make_unique<int>(getSize());
            _receiveCb(std::move(size));

            return ERR_OK;
        }

        void _error(err_t err) {
            (void) err;
            DEBUGV(":er %d 0x%08lx\r\n", (int) err, (uint32_t) _datasource);
            Serial.print("Error: ");
            Serial.println(err);
            Serial.print("Datasource: ");
            Serial.println(_datasource);
            tcp_arg(_pcb, nullptr);
            tcp_sent(_pcb, nullptr);
            tcp_recv(_pcb, nullptr);
            tcp_err(_pcb, nullptr);
            _pcb = nullptr;
            _errorCb(err);
            _notify_error();
        }

        err_t _connected(struct tcp_pcb *pcb, err_t err) {
            (void) err;
            (void) pcb;
            assert(pcb == _pcb);
            _connectCb();
            return ERR_OK;
        }

        err_t _poll(tcp_pcb *) {
            return ERR_OK;
        }

        // We may receive a nullptr as arg in the case when an IRQ happens during a shutdown sequence
        // In that case, just ignore the CB
        static err_t _s_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *pb, err_t err) {
            if (arg) {
                auto *context = reinterpret_cast<AsyncTcpClientContext *>(arg);
                return context->_recv(tpcb, pb, err);
            } else {
                return ERR_OK;
            }
        }

        static void _s_error(void *arg, err_t err) {
            if (arg) {
                reinterpret_cast<AsyncTcpClientContext *>(arg)->_error(err);
            }
        }

        static err_t _s_poll(void *arg, struct tcp_pcb *tpcb) {
            if (arg) {
                return reinterpret_cast<AsyncTcpClientContext *>(arg)->_poll(tpcb);
            } else {
                return ERR_OK;
            }
        }

        static err_t _s_acked(void *arg, struct tcp_pcb *tpcb, uint16_t len) {
            if (arg) {
                return reinterpret_cast<AsyncTcpClientContext *>(arg)->_acked(tpcb, len);
            } else {
                return ERR_OK;
            }
        }

        static err_t _s_connected(void *arg, struct tcp_pcb *pcb, err_t err) {
            if (arg) {
                return reinterpret_cast<AsyncTcpClientContext *>(arg)->_connected(pcb, err);
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

        const char *_datasource = nullptr;
        size_t _data_len = 0;
        size_t _written = 0;
        uint32_t _timeout_ms = 5000;
        uint32_t _op_start_time = 0;
//        bool _send_waiting = false;
//    bool _connect_pending = false;

        int8_t _ref_cnt;
        AsyncTcpClientContext *_next;
        std::function<void()> _connectCb;
        std::function<void(err_t err)> _errorCb;
        std::function<void(std::unique_ptr<int>)> _receiveCb;
        std::function<void(struct tcp_pcb *tpcb, uint16_t len)> _ackCb;
        bool _sync;
    };
} // namespace AsyncTcp
