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
#include "TcpClient.hpp"
#include <TcpClientContext.hpp>
#include <utility> // @todo: clarify if needed

#include "EventBridge.hpp"
#include "LwipEthernet.h"
#include "lwip/tcp.h"

template <>
async_tcp::TcpClient *SList<async_tcp::TcpClient>::_s_first = nullptr;

namespace async_tcp {

    using ::std::make_unique;
    using ::std::unique_ptr;

    uint16_t TcpClient::_localPort = 0;

    static bool defaultNoDelay = true; // false == Nagle enabled by default
    static bool defaultSync = false;   // @todo: clarify if this is used

    [[maybe_unused]] void TcpClient::setDefaultNoDelay(const bool noDelay) {
        defaultNoDelay = noDelay;
    }

    [[maybe_unused]] void TcpClient::setDefaultSync(const bool sync) {
        defaultSync = sync;
    }

    [[maybe_unused]] bool TcpClient::getDefaultNoDelay() {
        return defaultNoDelay;
    }

    [[maybe_unused]] bool TcpClient::getDefaultSync() {
        return defaultSync;
    }

    TcpClient::TcpClient() : _ctx(nullptr), _owned(nullptr) {
        _timeout = 5000;
        _add(this);
    }

    TcpClient::TcpClient(TcpClientContext *ctx)
        : _ctx(ctx), _owned(nullptr) {
        _timeout = 5000;
        _ctx->ref();

        _add(this);

        setSync(defaultSync);
        setNoDelay(defaultNoDelay);
    }

    TcpClient::~TcpClient() {
        _remove(this);
        if (_ctx) {
            _ctx->unref();
        }
    }

    [[maybe_unused]] unique_ptr<TcpClient> TcpClient::clone() const {
        return make_unique<TcpClient>(*this);
    }

    TcpClient::TcpClient(const TcpClient &other)
        : Client(other), SList(other) {
        _ctx = other._ctx;
        _timeout = other._timeout;
        _localPort = localPort();
        _owned = other._owned;
        if (_ctx) {
            _ctx->ref();
        }
        _add(this);
    }

    TcpClient &TcpClient::operator=(const TcpClient &other) {
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
        _localPort = localPort();
        _owned = other._owned;

        return *this;
    }
    int TcpClient::connect(const char *host, uint16_t port) {
        AIPAddress remote_addr;
        if (hostByName(
                host, remote_addr,
                static_cast<int>(_ctx->getTimeout()))) { // from WiFiClient
            return connect(remote_addr, port);
        }
        return 0;
    }

    int TcpClient::connect(const AString &host, uint16_t port) {
        return connect(host.c_str(), port);
    }

    int TcpClient::connect(AIPAddress ip, const uint16_t port) {
        if (_ctx) {
            stop();
            _ctx->unref();
            _ctx = nullptr;
        }

        tcp_pcb *pcb = tcp_new();
        if (!pcb) {
            DEBUGWIRE("No PCB\n");
            return 0;
        }

        if (_localPort > 0) {
            pcb->local_port = _localPort++;
        }

        _ctx = new TcpClientContext(pcb, nullptr, nullptr);
        _ctx->ref();
        _ctx->setTimeout(_timeout);
        _ctx->setOnConnectCallback([this] { _onConnectCallback(); });
        _ctx->setOnCloseCallback([this] { _onCloseCallback(); });
        _ctx->setOnErrorCallback([this](auto &&PH1) {
            _onErrorCallback(std::forward<decltype(PH1)>(PH1));
        });
        _ctx->setOnReceiveCallback([this](auto &&PH1) {
            _onReceiveCallback(std::forward<decltype(PH1)>(PH1));
        });
        _ctx->setOnAckCallback([this](auto &&PH1, auto &&PH2) {
            _onAckCallback(std::forward<decltype(PH1)>(PH1),
                           std::forward<decltype(PH2)>(PH2));
        });

        if (const int res = _ctx->connect(ip, port); res == 0) {
            DEBUGWIRE("Client did not menage to connect.\n");
            _ctx->unref();
            _ctx = nullptr;
            return 0;
        }

        setSync(defaultSync);
        setNoDelay(defaultNoDelay);

        return 1;
    }

    void TcpClient::setNoDelay(const bool no_delay) const {
        if (!_ctx) {
            return;
        }
        _ctx->setNoDelay(no_delay);
    }

    [[maybe_unused]] bool TcpClient::getNoDelay() const {
        if (!_ctx) {
            return false;
        }
        return _ctx->getNoDelay();
    }

    void TcpClient::setSync(const bool sync) const {
        if (!_ctx) {
            return;
        }
        _ctx->setSync(sync);
    }

    [[maybe_unused]] bool TcpClient::getSync() const {
        if (!_ctx) {
            return false;
        }
        return _ctx->getSync();
    }

    int TcpClient::availableForWrite() {
        return _ctx ? _ctx->availableForWrite() : 0;
    }

    size_t TcpClient::write(uint8_t b) { return write(&b, 1); }

    size_t TcpClient::write(const uint8_t *buf, size_t size) {
        if (!_ctx || !size) {
            return 0;
        }
        _ctx->setTimeout(_timeout);
        return _ctx->write(reinterpret_cast<const char *>(buf), size);
    }

    size_t TcpClient::write(Stream &stream) const {
        if (!_ctx || !stream.available()) {
            return 0;
        }
        _ctx->setTimeout(_timeout);
        return _ctx->write(stream);
    }

    int TcpClient::available() {
        if (!_ctx) {
            return 0;
        }

        int result = _ctx->getSize();

        return result;
    }

    int TcpClient::read() {
        if (!available()) {
            return -1;
        }
        return _ctx->read();
    }

    int TcpClient::read(uint8_t *buf, size_t size) {
        return static_cast<int>(
            _ctx->read(reinterpret_cast<char *>(buf), size));
    }

    int TcpClient::read(char *buf, size_t size) const {
        return static_cast<int>(_ctx->read(buf, size));
    }

    int TcpClient::peek() {
        if (!_ctx) {
            return -1;
        }
        return _ctx->peek();
    }

    size_t TcpClient::peekBytes(uint8_t *buffer, const size_t length) {
        if (!_ctx) {
            return 0;
        }
        return _ctx->peekBytes(reinterpret_cast<char *>(buffer), length);
    }

    const char *TcpClient::peekBuffer() const {
        if (!_ctx) {
            return nullptr;
        }
        return _ctx->peekBuffer();
    }

    size_t TcpClient::peekAvailable() const {
        if (!_ctx) {
            return 0;
        }
        return _ctx->peekAvailable();
    }

    void TcpClient::peekConsume(size_t size) const {
        if (!_ctx) {
            return;
        }
        _ctx->peekConsume(size);
    }

    bool TcpClient::flush(unsigned int maxWaitMs) const {
        if (!_ctx) {
            return true;
        }

        if (maxWaitMs == 0) {
            maxWaitMs = ASYNC_TCP_CLIENT_MAX_FLUSH_WAIT_MS;
        }
        return _ctx->wait_until_acked(maxWaitMs);
    }

    bool TcpClient::stop(unsigned int maxWaitMs) const {
        if (!_ctx) {
            return true;
        }

        bool ret = flush(maxWaitMs); // virtual, may be ssl's
        if (_ctx->close() != ERR_OK) {
            ret = false;
        }
        return ret;
    }

    uint8_t TcpClient::connected() {
        if (!_ctx || _ctx->state() == CLOSED) {
            return CLOSED;
        }

        return _ctx->state() == ESTABLISHED || available();
    }

    uint8_t TcpClient::status() {
        if (!_ctx) {
            return CLOSED;
        }
        return _ctx->state();
    }

    TcpClient::operator bool() { return available() || connected(); }

    [[maybe_unused]] AIPAddress TcpClient::remoteIP() const {
        if (!_ctx || !_ctx->getRemoteAddress()) {
            return {0};
        }

        return _ctx->getRemoteAddress();
    }

    [[maybe_unused]] uint16_t TcpClient::remotePort() const {
        if (!_ctx) {
            return 0;
        }

        return _ctx->getRemotePort();
    }

    AIPAddress TcpClient::localIP() const {
        if (!_ctx || !_ctx->getLocalAddress()) {
            return {0};
        }

        return {_ctx->getLocalAddress()};
    }

    [[maybe_unused]] uint16_t TcpClient::localPort() const {
        if (!_ctx) {
            return 0;
        }

        return _ctx->getLocalPort();
    }

    [[maybe_unused]] void TcpClient::stopAll() {
        for (TcpClient *it = _s_first; it; it = it->_next) {
            it->stop();
        }
    }

    [[maybe_unused]] void TcpClient::stopAllExcept(TcpClient *except) {
        // Stop all will look at the lowest-level wrapper connections only
        while (except->_owned) {
            except = except->_owned;
        }
        for (TcpClient *it = _s_first; it; it = it->_next) {
            TcpClient *conn = it;
            // Find the lowest-level owner of the current list entry
            while (conn->_owned) {
                conn = conn->_owned;
            }
            if (conn != except) {
                conn->stop();
            }
        }
    }

    void TcpClient::keepAlive(uint16_t idle_sec, uint16_t intv_sec,
                                   uint8_t count) const {
        _ctx->keepAlive(idle_sec, intv_sec, count);
    }

    [[maybe_unused]] bool TcpClient::isKeepAliveEnabled() const {
        return _ctx->isKeepAliveEnabled();
    }

    [[maybe_unused]] uint16_t TcpClient::getKeepAliveIdle() const {
        return _ctx->getKeepAliveIdle();
    }

    [[maybe_unused]] uint16_t TcpClient::getKeepAliveInterval() const {
        return _ctx->getKeepAliveInterval();
    }

    [[maybe_unused]] uint8_t TcpClient::getKeepAliveCount() const {
        return _ctx->getKeepAliveCount();
    }
    void TcpClient::setOnReceivedCallback(std::unique_ptr<EventBridge> worker) {
        _received_callback_worker = std::move(worker);
    }

    void
    TcpClient::setOnConnectedCallback(
        std::unique_ptr<EventBridge> worker) {
        _connected_callback_worker = std::move(worker);
    }

    void TcpClient::setOnClosedCallback(std::unique_ptr<EventBridge> worker) {
        _closed_callback_worker = std::move(worker);
    }

    void TcpClient::_onConnectCallback() const {
        const AIPAddress remote_ip = remoteIP();
        (void)remote_ip;
        DEBUGWIRE("TcpClient::_onConnectCallback(): Connected to %s.\n",
                  remote_ip.toString().c_str());
        if (_connected_callback_worker) {
            _connected_callback_worker->run();
        } else {
            DEBUGWIRE("TcpClient::_onConnectCallback: No event handler\n");
        }
    }

    void TcpClient::_onCloseCallback() const {
        DEBUGWIRE("TcpClient::_onCloseCallback(): Connection closed.\n");
        if (_closed_callback_worker) {
            _closed_callback_worker->run();
        } else {
            DEBUGWIRE("TcpClient::_onCloseCallback: No event handler\n");
        }
    }

    void TcpClient::_onErrorCallback(err_t err) {
        DEBUGWIRE("The ctx failed with the error code: %d", err);
        _ctx->close();
        _ctx = nullptr;
    }

    void TcpClient::_onReceiveCallback(std::unique_ptr<int> size) const {
        if (_received_callback_worker) {
            _received_callback_worker->run();
        } else {
            DEBUGWIRE("TcpClient::_onReceiveCallback: No event handler\n");
        }
    }

    void TcpClient::_onAckCallback(struct tcp_pcb *tpcb,
                                        uint16_t len) const {
        (void)tpcb;
        (void)len;
        DEBUGWIRE("TcpClient::_onAckCallback: ack callback "
                  "triggered.length: %d\n",
                  len);
        // @todo: implement later
    }
} // namespace AsyncTcp
