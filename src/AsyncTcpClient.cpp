/*
    AsyncTcpClient.cpp - TCP/IP client for esp8266, mostly compatible
                   with Arduino WiFi shield library

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
#include "AsyncTcpClient.hpp"
#include "LwipEthernet.h"
#include "lwip/tcp.h"
#include <AsyncTcpClientContext.hpp>
#include <bm_client.h>
#include <utility> // @todo: clarify if needed

template<>
AsyncTcp::AsyncTcpClient* SList<AsyncTcp::AsyncTcpClient>::_s_first = nullptr;

namespace AsyncTcp {

    using ::std::make_unique;
    using ::std::unique_ptr;

    uint16_t AsyncTcpClient::_localPort = 0;

    static bool defaultNoDelay = true; // false == Nagle enabled by default
    static bool defaultSync = false;

    [[maybe_unused]] void AsyncTcpClient::setDefaultNoDelay(const bool noDelay) {
        defaultNoDelay = noDelay;
    }

    [[maybe_unused]] void AsyncTcpClient::setDefaultSync(const bool sync) {
        defaultSync = sync;
    }

    [[maybe_unused]] bool AsyncTcpClient::getDefaultNoDelay() {
        return defaultNoDelay;
    }

    [[maybe_unused]] bool AsyncTcpClient::getDefaultSync() {
        return defaultSync;
    }

    AsyncTcpClient::AsyncTcpClient() : _ctx(nullptr), _owned(nullptr) {
        _timeout = 5000;
        _add(this);
    }

    AsyncTcpClient::AsyncTcpClient(AsyncTcpClientContext *ctx) : _ctx(ctx), _owned(nullptr) {
        _timeout = 5000;
        _ctx->ref();

        _add(this);

        setSync(defaultSync);
        setNoDelay(defaultNoDelay);
    }

    AsyncTcpClient::~AsyncTcpClient() {
        _remove(this);
        if (_ctx) {
            _ctx->unref();
        }
    }

    [[maybe_unused]] unique_ptr<AsyncTcpClient> AsyncTcpClient::clone() const {
        return make_unique<AsyncTcpClient>(*this);
    }

    AsyncTcpClient::AsyncTcpClient(const AsyncTcpClient &other) : Client(other), SList(other) {
        _ctx = other._ctx;
        _timeout = other._timeout;
        _localPort = AsyncTcpClient::_localPort;
        _owned = other._owned;
        if (_ctx) {
            _ctx->ref();
        }
        _add(this);
    }

    AsyncTcpClient& AsyncTcpClient::operator=(const AsyncTcpClient& other) {
        // Self-assignment check to avoid unnecessary work
        if (this == &other) {
            return *this;
        }

        // Decrement the reference count of the current context if it exists
        if (_ctx) {
            _ctx->unref();
        }

        // Copy the new context and increment the reference count
        _ctx = other._ctx;
        if (_ctx) {
            _ctx->ref();
        }

        // Copy other members
        _timeout = other._timeout;
        _localPort = AsyncTcpClient::_localPort;  // Seems like this is a static member?
        _owned = other._owned;

        return *this;
    }
    int AsyncTcpClient::connect(const char *host, uint16_t port) {
        AIPAddress remote_addr;
        if (hostByName(host, remote_addr, static_cast<int>(_ctx->getTimeout()))) { // from WiFiClient
            return connect(remote_addr, port);
        }
        return 0;
    }

    int AsyncTcpClient::connect(const AString &host, uint16_t port) {
        return connect(host.c_str(), port);
    }

    int AsyncTcpClient::connect(AIPAddress ip, uint16_t port) {
        if (_ctx) {
            stop();
            _ctx->unref();
            _ctx = nullptr;
        }

        tcp_pcb *pcb = tcp_new();
        if (!pcb) {
            Serial.println("No PCB");
            return 0;
        }

        if (_localPort > 0) {
            pcb->local_port = _localPort++;
        }

        _ctx = new AsyncTcpClientContext(pcb, nullptr, nullptr);
        _ctx->ref();
        _ctx->setTimeout(_timeout);
        _ctx->setOnConnectCallback([this] { _onConnectCallback(); });
        _ctx->setOnErrorCallback([this](auto &&PH1) { _onErrorCallback(std::forward<decltype(PH1)>(PH1)); });
        _ctx->setOnReceiveCallback([this](auto &&PH1) {
            _onReceiveCallback(std::forward<decltype(PH1)>(PH1));
        });
        _ctx->setOnAckCallback([this](auto &&PH1, auto &&PH2) {
            _onAckCallback(std::forward<decltype(PH1)>(PH1), std::forward<decltype(PH2)>(PH2));
        });
        int res = _ctx->connect(ip, port);
        if (res == 0) {
            Serial.println("Client did not menage to connect.");
            _ctx->unref();
            _ctx = nullptr;
            return 0;
        }

        setSync(defaultSync);
        setNoDelay(defaultNoDelay);

        return 1;
    }

    void AsyncTcpClient::setNoDelay(const bool no_delay) const {
        if (!_ctx) {
            return;
        }
        _ctx->setNoDelay(no_delay);
    }

    [[maybe_unused]] bool AsyncTcpClient::getNoDelay() const {
        if (!_ctx) {
            return false;
        }
        return _ctx->getNoDelay();
    }

    void AsyncTcpClient::setSync(const bool sync) const {
        if (!_ctx) {
            return;
        }
        _ctx->setSync(sync);
    }

    [[maybe_unused]] bool AsyncTcpClient::getSync() const {
        if (!_ctx) {
            return false;
        }
        return _ctx->getSync();
    }

    int AsyncTcpClient::availableForWrite() {
        return _ctx ? _ctx->availableForWrite() : 0;
    }

    size_t AsyncTcpClient::write(uint8_t b) {
        return write(&b, 1);
    }

    size_t AsyncTcpClient::write(const uint8_t *buf, size_t size) {
        if (!_ctx || !size) {
            return 0;
        }
        _ctx->setTimeout(_timeout);
        return _ctx->write((const char *) buf, size);
    }

    size_t AsyncTcpClient::write(Stream &stream) {
        if (!_ctx || !stream.available()) {
            return 0;
        }
        _ctx->setTimeout(_timeout);
        return _ctx->write(stream);
    }

    int AsyncTcpClient::available() {
        if (!_ctx) {
            Serial.println("Man, there is no ctx!");
            return 0;
        }

        int result = _ctx->getSize();

        return result;
    }

    int AsyncTcpClient::read() {
        if (!available()) {
            return -1;
        }
        return _ctx->read();
    }

    int AsyncTcpClient::read(uint8_t *buf, size_t size) {
        return static_cast<int>(_ctx->read(reinterpret_cast<char *>(buf), size));
    }

    int AsyncTcpClient::read(char *buf, size_t size) {
        return static_cast<int>(_ctx->read(buf, size));
    }

    int AsyncTcpClient::peek() {
        if (!_ctx) {
            return -1;
        }
        return _ctx->peek();
    }

    size_t AsyncTcpClient::peekBytes(uint8_t *buffer, const size_t length) {
        if (!_ctx) {
            return 0;
        }
        return _ctx->peekBytes(reinterpret_cast<char *>(buffer), length);
    }

    const char* AsyncTcpClient::peekBuffer() const {
        if (!_ctx) {
            return nullptr;
        }
        return _ctx->peekBuffer();
    }

    size_t AsyncTcpClient::peekAvailable() const {
        if (!_ctx) {
            return 0;
        }
        return _ctx->peekAvailable();
    }

    void AsyncTcpClient::peekConsume(size_t size) const {
        if (!_ctx) {
            return;
        }
        _ctx->peekConsume(size);
    }

    bool AsyncTcpClient::flush(unsigned int maxWaitMs) {
        if (!_ctx) {
            return true;
        }

        if (maxWaitMs == 0) {
            maxWaitMs = ASYNC_TCP_CLIENT_MAX_FLUSH_WAIT_MS;
        }
        return _ctx->wait_until_acked(maxWaitMs);
    }

    bool AsyncTcpClient::stop(unsigned int maxWaitMs) {
        if (!_ctx) {
            return true;
        }

        bool ret = flush(maxWaitMs); // virtual, may be ssl's
        if (_ctx->close() != ERR_OK) {
            ret = false;
        }
        return ret;
    }

    uint8_t AsyncTcpClient::connected() {
        if (!_ctx || _ctx->state() == CLOSED) {
            return CLOSED;
        }

        return _ctx->state() == ESTABLISHED || available();
    }

    uint8_t AsyncTcpClient::status() {
        if (!_ctx) {
            return CLOSED;
        }
        return _ctx->state();
    }

    AsyncTcpClient::operator bool() {
        return available() || connected();
    }

    [[maybe_unused]] AIPAddress AsyncTcpClient::remoteIP() const {
        if (!_ctx || !_ctx->getRemoteAddress()) {
            return {0};
        }

        return _ctx->getRemoteAddress();
    }

    [[maybe_unused]] uint16_t AsyncTcpClient::remotePort() const {
        if (!_ctx) {
            return 0;
        }

        return _ctx->getRemotePort();
    }

    AIPAddress AsyncTcpClient::localIP() {
        if (!_ctx || !_ctx->getLocalAddress()) {
            return {0};
        }

        return {_ctx->getLocalAddress()};
    }

    [[maybe_unused]] uint16_t AsyncTcpClient::localPort() {
        if (!_ctx) {
            return 0;
        }

        return _ctx->getLocalPort();
    }

    [[maybe_unused]] void AsyncTcpClient::stopAll() {
        for (AsyncTcpClient *it = _s_first; it; it = it->_next) {
            it->stop();
        }
    }


    [[maybe_unused]] void AsyncTcpClient::stopAllExcept(AsyncTcpClient *except) {
        // Stop all will look at the lowest-level wrapper connections only
        while (except->_owned) {
            except = except->_owned;
        }
        for (AsyncTcpClient *it = _s_first; it; it = it->_next) {
            AsyncTcpClient *conn = it;
            // Find the lowest-level owner of the current list entry
            while (conn->_owned) {
                conn = conn->_owned;
            }
            if (conn != except) {
                conn->stop();
            }
        }
    }

    void AsyncTcpClient::keepAlive(uint16_t idle_sec, uint16_t intv_sec, uint8_t count) {
        _ctx->keepAlive(idle_sec, intv_sec, count);
    }

    [[maybe_unused]] bool AsyncTcpClient::isKeepAliveEnabled() const {
        return _ctx->isKeepAliveEnabled();
    }

    [[maybe_unused]] uint16_t AsyncTcpClient::getKeepAliveIdle() const {
        return _ctx->getKeepAliveIdle();
    }

    [[maybe_unused]] uint16_t AsyncTcpClient::getKeepAliveInterval() const {
        return _ctx->getKeepAliveInterval();
    }

    [[maybe_unused]] uint8_t AsyncTcpClient::getKeepAliveCount() const {
        return _ctx->getKeepAliveCount();
    }

    [[maybe_unused]] int8_t AsyncTcpClient::_s_connected(void *arg, void *tpcb, int8_t err) {
        return 8; // _s_connected is a protected member of AsyncTcpContext
    }

    int8_t AsyncTcpClient::_connected(void *tpcb, int8_t err) {
        return 8; // _connected is a protected member of AsyncTcpContext
    }

    [[maybe_unused]] void AsyncTcpClient::_s_err(void *arg, int8_t err) {
//    probably the method should be named _s_error for consistency
//    _s_error is a protected member of AsyncTcpContext
    }

    [[maybe_unused]] void AsyncTcpClient::_err(int8_t err) {
//    probably the method should be named _error for consistency
//    _error is a protected member of AsyncTcpContext
    }

    void AsyncTcpClient::setOnReceiveCallback(std::shared_ptr<EventHandler> handler) {
        _receive_callback_handler = std::move(handler);
    }

    void AsyncTcpClient::setOnConnectedCallback(std::shared_ptr<EventHandler> handler) {
        _connected_callback_handler = std::move(handler);
    }

    void AsyncTcpClient::_onConnectCallback() const {
        const AIPAddress remote_ip = remoteIP();
        (void) remote_ip;
        DEBUGV("AsyncTcpClient::_onConnectCallback(): Connected to %S.\n", remote_ip.toString().c_str());
        if (_connected_callback_handler) {
            _connected_callback_handler->handleEvent();
        } else {
            DEBUGV("AsyncTcpClient::_onConnectCallback: No event handler\n");
        }
    }

    void AsyncTcpClient::_onErrorCallback(err_t err) {
        DEBUGV("The ctx failed with the error code: %d", err);
        _ctx->close();
        _ctx = nullptr;
    }

    void AsyncTcpClient::_onReceiveCallback(std::unique_ptr<int> size) const {
        if (_receive_callback_handler) {
            _receive_callback_handler->handleEvent();
        } else {
            DEBUGV("AsyncTcpClient::_onReceiveCallback: No event handler\n");
        }
    }

    void AsyncTcpClient::_onAckCallback(struct tcp_pcb *tpcb, uint16_t len) const {
        (void) tpcb;
        (void) len;
        DEBUGV("AsyncTcpClient::_onAckCallback: ack callback triggered.length: %d\n", len);
        // @todo: implement later
    }
} // namespace AsyncTcp