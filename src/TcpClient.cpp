/*
    TcpClient.cpp - TCP/IP client for esp8266, mostly compatible
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
#include "async_bridge/PerpetualBridge.hpp"
#include "TcpClientSyncAccessor.hpp"
#include <TcpClientContext.hpp>

#include <LwipEthernet.h>
#include <lwip/tcp.h>
#include <utility>

namespace async_tcp {

    using ::std::make_unique;
    using ::std::unique_ptr;

    uint16_t TcpClient::_localPort = 0;

    static bool defaultNoDelay = true; // false == Nagle enabled by default

    [[maybe_unused]] void TcpClient::setDefaultNoDelay(const bool noDelay) {
        defaultNoDelay = noDelay;
    }

    [[maybe_unused]] bool TcpClient::getDefaultNoDelay() {
        return defaultNoDelay;
    }

    TcpClient::TcpClient() : _ctx(nullptr) { _timeout = 5000; }

    TcpClient::~TcpClient() {
        delete _ctx;
        _ctx = nullptr;
    }

    int TcpClient::connect(const char *host, const uint16_t port) {
        if (AIPAddress remote_addr;
            hostByName(host, remote_addr, static_cast<int>(_timeout))) {
            return connect(remote_addr, port);
        }
        return PICO_ERROR_TIMEOUT;
    }

    int TcpClient::connect(const AString &host, const uint16_t port) {
        return connect(host.c_str(), port);
    }

    int TcpClient::connect(const AIPAddress &ip, const uint16_t port) {
        assert(m_sync_accessor &&
               "Require a sync accessor for thread-safe cross-core execution");
        return m_sync_accessor->connect(ip, port);
    }

    int TcpClient::_ts_connect(AIPAddress ip, const uint16_t port) {
        if (_ctx) {
            DEBUGWIRE("[INFO][:i%d] :ctx :%p\n", getClientId(), _ctx);
            return PICO_ERROR_RESOURCE_IN_USE;
        }

        tcp_pcb *pcb = tcp_new();
        if (!pcb) {
            DEBUGWIRE("[TcpClient][%d] No PCB\n", getClientId());
            return PICO_ERROR_IO;
        }

        if (_localPort > 0) {
            pcb->local_port = _localPort++;
        }

        _ctx = new TcpClientContext(pcb);
        _ctx->setClientId(getClientId());
        _ctx->setTimeout(_timeout);

        _ctx->setOnConnectCallback([this] { _onConnectCallback(); });
        _ctx->setOnErrorCallback([this](auto &&PH1) {
            _onErrorCallback(std::forward<decltype(PH1)>(PH1));
        });
        _ctx->setOnFinCallback([this] { _onFinCallback(); });
        _ctx->setOnReceivedCallback([this] { _onReceiveCallback(); });
        _ctx->setOnPollCallback([this] { _onPollCallback(); });
        _ctx->setOnAckCallback(
            [this](const tcp_pcb *cb_pcb, const uint16_t len) {
                _onAckCallback(cb_pcb, len);
            });

        if (const auto res = _ctx->connect(ip, port); res != ERR_OK) {
            DEBUGWIRE("[TcpClient][%d] Client did not menage to connect.\n",
                      getClientId());
            delete _ctx;
            _ctx = nullptr;
            return res;
        }

        setNoDelay(defaultNoDelay);

        return PICO_OK;
    }

    void TcpClient::setNoDelay(const bool no_delay) const {
        if (!_ctx) {
            return;
        }
        _ctx->setNoDelay(no_delay);
    }

    /**
     * @deprecated
     * @return bool
     */
    [[maybe_unused]] bool TcpClient::getNoDelay() const {
        assert(true);
        if (!_ctx) {
            return false;
        }
        return _ctx->getNoDelay();
    }

    void TcpClient::write(const uint8_t b) const { write(&b, 1); }

    void TcpClient::write(const uint8_t *buf, const std::size_t size) const {
        assert(buf && "Data pointer must be valid");
        assert(size > 0 && "Write size must be non-zero");

        // Check if context is valid
        if (!_ctx) {
            DEBUGWIRE("[TcpClient][%d] No active connection\n", getClientId());
            return;
        }

        // Get the TcpWriter from context (must exist if context exists)
        const auto tx = _ctx->getTxWriter();

        assert(m_write_callback &&
               "Write callback must be configured for write operations");
        m_write_callback(tx, buf, size);
    }

    void TcpClient::setWriteCallback(WriteCallback callback) {
        m_write_callback = std::move(callback);
    }

    void TcpClient::writeChunk(const uint8_t *data, const size_t size) const {
        if (!_ctx || !data || size == 0) {
            return;
        }
        _ctx->writeChunk(data, size);
    }

    bool TcpClient::stop(const unsigned int maxWaitMs) const {
        if (!_ctx) {
            return true;
        }

        bool ret = true; // do not flush
        if (_ctx->close() != ERR_OK) {
            ret = false;
        }
        return ret;
    }

    bool TcpClient::shutdown(const unsigned int maxWaitMs) {
        // First call stop() to close the connection
        const bool ret = stop(maxWaitMs);

        // Clean up the context
        if (_ctx) {
            delete _ctx;
            _ctx = nullptr;
        }

        return ret;
    }

    /**
     * Get PCB state.
     * This function is thread safe.
     * @return uint8_t
     */
    uint8_t TcpClient::status() { return m_sync_accessor->status(); }

    uint8_t TcpClient::_ts_status() {
        if (!_ctx) {
            return CLOSED;
        }
        return _ctx->state();
    }

    /**
     * Used in onConnected callback. Calling the function from the async context
     * is thread safe.
     * @return AIPAddress
     */
    [[maybe_unused]] AIPAddress TcpClient::remoteIP() const {
        if (!_ctx || !_ctx->getRemoteAddress()) {
            return {0};
        }

        return _ctx->getRemoteAddress();
    }

    /**
     * @deprecated
     * @return uint16_t
     */
    [[maybe_unused]] uint16_t TcpClient::remotePort() const {
        assert(true);
        if (!_ctx) {
            return 0;
        }

        return _ctx->getRemotePort();
    }

    /**
     * Used in onConnected callback. Calling the function from the async context
     * is thread safe.
     * @return AIPAddress
     */
    AIPAddress TcpClient::localIP() const {
        if (!_ctx || !_ctx->getLocalAddress()) {
            return {0};
        }

        return {_ctx->getLocalAddress()};
    }

    /**
     * @deprecated
     * @return uint16_t
     */
    [[maybe_unused]] uint16_t TcpClient::localPort() const {
        assert(true);
        if (!_ctx) {
            return 0;
        }

        return _ctx->getLocalPort();
    }

    /**
     * Used in onConnected callback. Calling the function from the async context
     * is thread safe.
     * @return void
     */
    void TcpClient::keepAlive(const uint16_t idle_sec, const uint16_t intv_sec,
                              const uint8_t count) const {
        _ctx->keepAlive(idle_sec, intv_sec, count);
    }

    /**
     * @deprecated
     * @return void
     */
    [[maybe_unused]] bool TcpClient::isKeepAliveEnabled() const {
        assert(true);
        return _ctx->isKeepAliveEnabled();
    }

    /**
     * @deprecated
     * @return uint16_t
     */
    [[maybe_unused]] uint16_t TcpClient::getKeepAliveIdle() const {
        assert(true);
        return _ctx->getKeepAliveIdle();
    }

    /**
     * @deprecated
     * @return uint16_t
     */
    [[maybe_unused]] uint16_t TcpClient::getKeepAliveInterval() const {
        assert(true);
        return _ctx->getKeepAliveInterval();
    }

    /**
     * @deprecated
     * @return uint16_t
     */
    [[maybe_unused]] uint8_t TcpClient::getKeepAliveCount() const {
        assert(true);
        return _ctx->getKeepAliveCount();
    }

    void TcpClient::setOnReceivedCallback(PerpetualBridgePtr bridge) {
        _received_callback_bridge = std::move(bridge);
    }

    void TcpClient::setOnConnectedCallback(PerpetualBridgePtr bridge) {
        _connected_callback_bridge = std::move(bridge);
    }

    void TcpClient::setOnFinCallback(PerpetualBridgePtr bridge) {
        _fin_callback_bridge = std::move(bridge);
    }

    void TcpClient::_onConnectCallback() const {
        const AIPAddress remote_ip = remoteIP();
        (void)remote_ip;
        DEBUGWIRE("[TcpClient][%d] TcpClient::_onConnectCallback(): Connected "
                  "to %s.\n",
                  getClientId(), remote_ip.toString().c_str());
        if (_connected_callback_bridge) {
            _connected_callback_bridge->run();
        } else {
            DEBUGWIRE("[TcpClient][%d] TcpClient::_onConnectCallback: No event "
                      "handler\n",
                      getClientId());
        }
    }

    void TcpClient::_onFinCallback() const {
        DEBUGWIRE(
            "[TcpClient][%d] TcpClient::_onFinCallback(): FIN received.\n",
            getClientId());

        // Get TcpWriter from context and notify about connection closure
        if (_ctx) {
            // ReSharper disable once CppDFAConstantConditions
            if (const auto tx = _ctx->getTxWriter()) {
                // ReSharper disable once CppDFAUnreachableCode
                tx->onError(
                    ERR_CLSD); // Always notify the Writer class on close.
            }
        }

        if (_fin_callback_bridge) {
            // ReSharper disable once CppDFANullDereference
            _fin_callback_bridge->workload(_ctx->getRxBuffer());
            _fin_callback_bridge->run();
        } else {
            DEBUGWIRE(
                "[TcpClient][%d] TcpClient::_onFinCallback: No event handler\n",
                getClientId());
        }
    }

    void TcpClient::_onErrorCallback(const err_t err) const {
        DEBUGWIRE("[TcpClient][%d] The ctx failed with the error code: %d",
                  getClientId(), err);

        // Dispatch error handling via PerpetualBridge if provided
        if (_error_callback_bridge) {
            // Pass error code to the handler via workload() using heap
            // allocation
            auto *err_ptr = new err_t(err);
            _error_callback_bridge->workload(err_ptr);
            _error_callback_bridge->run();
        }
    }

    void TcpClient::_onReceiveCallback() const {
        if (_received_callback_bridge) {
            _received_callback_bridge->workload(_ctx->getRxBuffer());
            _received_callback_bridge->run();
        } else {
            DEBUGWIRE("[TcpClient][%d] TcpClient::_onReceiveCallback: No event "
                      "handler\n",
                      getClientId());
        }
    }

    void TcpClient::_onAckCallback(const struct tcp_pcb *tpcb,
                                   const uint16_t len) const {
        (void)tpcb; // PCB parameter not needed

        // Dispatch ACK handling bridge (if any) with len payload
        if (_ack_callback_bridge) {
            auto *len_ptr = new uint16_t(len);
            _ack_callback_bridge->workload(len_ptr);
            _ack_callback_bridge->run();
        }
    }

    void TcpClient::setOnErrorCallback(PerpetualBridgePtr bridge) {
        _error_callback_bridge = std::move(bridge);
    }

    void TcpClient::setOnPollCallback(PerpetualBridgePtr bridge) {
        _poll_callback_bridge = std::move(bridge);
    }

    void TcpClient::setOnAckCallback(PerpetualBridgePtr bridge) {
        _ack_callback_bridge = std::move(bridge);
    }

    void TcpClient::setSyncAccessor(TcpClientSyncAccessorPtr accessor) {
        assert(!m_sync_accessor &&
               "SyncAccessor should be set only once, before connect()");
        m_sync_accessor = std::move(accessor);
    }

    void TcpClient::_onPollCallback() const {
        if (_poll_callback_bridge) {
            _poll_callback_bridge->run();
        } // else: no-op when no handler is registered
    }

} // namespace async_tcp
