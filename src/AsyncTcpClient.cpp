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

//#include "WiFi.h"
#include "LwipEthernet.h"
#include "delay.cpp" // added
//#include "lwip/opt.h"
//#include "lwip/ip.h"
#include "lwip/tcp.h"
//#include "lwip/inet.h"
//#include "lwip/netif.h"
#include "AsyncTcpClient.hpp"
#include <AsyncTcpClientContext.hpp>
//#include <StreamDev.h>

uint16_t AsyncTcpClient::_localPort = 0;

static bool defaultNoDelay = false; // false == Nagle enabled by default
static bool defaultSync = false;

bool getDefaultPrivateGlobalSyncValue() {
    return defaultSync;
}

[[maybe_unused]] void AsyncTcpClient::setDefaultNoDelay(bool noDelay) {
    defaultNoDelay = noDelay;
}

[[maybe_unused]] void AsyncTcpClient::setDefaultSync(bool sync) {
    defaultSync = sync;
}

[[maybe_unused]] bool AsyncTcpClient::getDefaultNoDelay() {
    return defaultNoDelay;
}

[[maybe_unused]] bool AsyncTcpClient::getDefaultSync() {
    return defaultSync;
}

template<>
AsyncTcpClient* SList<AsyncTcpClient>::_s_first = nullptr;


AsyncTcpClient::AsyncTcpClient()
        : _client(nullptr), _owned(nullptr) {
    _timeout = 5000;
    AsyncTcpClient::_add(this);
}

AsyncTcpClient::AsyncTcpClient(AsyncTcpClientContext* client)
        : _client(client), _owned(nullptr) {
    _timeout = 5000;
    _client->ref();
    AsyncTcpClient::_add(this);

    setSync(defaultSync);
    setNoDelay(defaultNoDelay);
}

AsyncTcpClient::~AsyncTcpClient() {
    AsyncTcpClient::_remove(this);
    if (_client) {
        _client->unref();
    }
}

[[maybe_unused]] std::unique_ptr<AsyncTcpClient> AsyncTcpClient::clone() const {
    return std::make_unique<AsyncTcpClient>(*this);
}

AsyncTcpClient::AsyncTcpClient(const AsyncTcpClient& other)  : Client(other), SList(other) {
    _client = other._client;
    _timeout = other._timeout;
    _localPort = AsyncTcpClient::_localPort;
    _owned = other._owned;
    if (_client) {
        _client->ref();
    }
    AsyncTcpClient::_add(this);
}

AsyncTcpClient& AsyncTcpClient::operator=(const AsyncTcpClient& other) {
    if (_client) {
        _client->unref();
    }
    _client = other._client;
    _timeout = other._timeout;
    _localPort = AsyncTcpClient::_localPort;
    _owned = other._owned;
    if (_client) {
        _client->ref();
    }
    return *this;
}

int AsyncTcpClient::connect(const char* host, uint16_t port) {
    IPAddress remote_addr;
    if (::hostByName(host, remote_addr, _timeout)) { // from WiFiClient
        return connect(remote_addr, port);
    }
    return 0;
}

int AsyncTcpClient::connect(const String& host, uint16_t port) {
    return connect(host.c_str(), port);
}

int AsyncTcpClient::connect(IPAddress ip, uint16_t port) {
    if (_client) {
        stop();
        _client->unref();
        _client = nullptr;
    }

    tcp_pcb* pcb = tcp_new();
    if (!pcb) {
        Serial.println("No PCB");
        return 0;
    }

    if (_localPort > 0) {
        pcb->local_port = _localPort++;
    }

    _client = new AsyncTcpClientContext(pcb, nullptr, nullptr);
    _client->ref();
    _client->setTimeout(_timeout);
    _client->setOnConnectCallback(_onConnectHandler);
    _client->setOnErrorCallback(_onErrorHandler);
    int res = _client->connect(ip, port);
    if (res == 0) {
        Serial.println("Client did not menage to connect.");
        _client->unref();
        _client = nullptr;
        return 0;
    }

    setSync(defaultSync);
    setNoDelay(defaultNoDelay);

    return 1;
}

void AsyncTcpClient::setNoDelay(bool nodelay) {
    if (!_client) {
        return;
    }
    _client->setNoDelay(nodelay);
}

[[maybe_unused]] bool AsyncTcpClient::getNoDelay() const {
    if (!_client) {
        return false;
    }
    return _client->getNoDelay();
}

void AsyncTcpClient::setSync(bool sync) {
    if (!_client) {
        return;
    }
    _client->setSync(sync);
}

[[maybe_unused]] bool AsyncTcpClient::getSync() const {
    if (!_client) {
        return false;
    }
    return _client->getSync();
}

int AsyncTcpClient::availableForWrite() {
    return _client ? _client->availableForWrite() : 0;
}

size_t AsyncTcpClient::write(uint8_t b) {
    return write(&b, 1);
}

size_t AsyncTcpClient::write(const uint8_t *buf, size_t size) {
    if (!_client || !size) {
        return 0;
    }
    _client->setTimeout(_timeout);
    return _client->write((const char*)buf, size);
}

size_t AsyncTcpClient::write(Stream& stream) {
    if (!_client || !stream.available()) {
        return 0;
    }
    _client->setTimeout(_timeout);
    return _client->write(stream);
}

int AsyncTcpClient::available() {
    if (!_client) {
        return 0;
    }

    int result = _client->getSize();

    return result;
}

int AsyncTcpClient::read() {
    if (!available()) {
        return -1;
    }

    return _client->read();
}

int AsyncTcpClient::read(uint8_t* buf, size_t size) {
    return (int)_client->read((char*)buf, size);
}

int AsyncTcpClient::read(char* buf, size_t size) {
    return (int)_client->read(buf, size);
}

int AsyncTcpClient::peek() {
    if (!available()) {
        return -1;
    }

    return _client->peek();
}

size_t AsyncTcpClient::peekBytes(uint8_t *buffer, size_t length) {
    size_t count;

    if (!_client) {
        return 0;
    }

    _startMillis = millis();
    while ((available() < (int) length) && ((millis() - _startMillis) < _timeout)) {
        yield();
    }

    if (available() < (int) length) {
        count = available();
    } else {
        count = length;
    }

    return _client->peekBytes((char *)buffer, count);
}

bool AsyncTcpClient::flush(unsigned int maxWaitMs) {
    if (!_client) {
        return true;
    }

    if (maxWaitMs == 0) {
        maxWaitMs = ASYNC_TCP_CLIENT_MAX_FLUSH_WAIT_MS;
    }
    return _client->wait_until_acked(maxWaitMs);
}

bool AsyncTcpClient::stop(unsigned int maxWaitMs) {
    if (!_client) {
        return true;
    }

    bool ret = flush(maxWaitMs); // virtual, may be ssl's
    if (_client->close() != ERR_OK) {
        ret = false;
    }
    return ret;
}

uint8_t AsyncTcpClient::connected() {
    if (!_client || _client->state() == CLOSED) {
        return 0;
    }

    return _client->state() == ESTABLISHED || available();
}

uint8_t AsyncTcpClient::status() {
    if (!_client) {
        return CLOSED;
    }
    return _client->state();
}

AsyncTcpClient::operator bool() {
    return available() || connected();
}

[[maybe_unused]] IPAddress AsyncTcpClient::remoteIP() {
    if (!_client || !_client->getRemoteAddress()) {
        return {0};
    }

    return _client->getRemoteAddress();
}

[[maybe_unused]] uint16_t AsyncTcpClient::remotePort() {
    if (!_client) {
        return 0;
    }

    return _client->getRemotePort();
}

IPAddress AsyncTcpClient::localIP() {
    if (!_client || !_client->getLocalAddress()) {
        return {0};
    }

    return {_client->getLocalAddress()};
}

[[maybe_unused]] uint16_t AsyncTcpClient::localPort() {
    if (!_client) {
        return 0;
    }

    return _client->getLocalPort();
}

[[maybe_unused]] void AsyncTcpClient::stopAll() {
    for (AsyncTcpClient* it = _s_first; it; it = it->_next) {
        it->stop();
    }
}


[[maybe_unused]] void AsyncTcpClient::stopAllExcept(AsyncTcpClient* except) {
    // Stop all will look at the lowest-level wrapper connections only
    while (except->_owned) {
        except = except->_owned;
    }
    for (AsyncTcpClient* it = _s_first; it; it = it->_next) {
        AsyncTcpClient* conn = it;
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
    _client->keepAlive(idle_sec, intv_sec, count);
}

[[maybe_unused]] bool AsyncTcpClient::isKeepAliveEnabled() const {
    return _client->isKeepAliveEnabled();
}

[[maybe_unused]] uint16_t AsyncTcpClient::getKeepAliveIdle() const {
    return _client->getKeepAliveIdle();
}

[[maybe_unused]] uint16_t AsyncTcpClient::getKeepAliveInterval() const {
    return _client->getKeepAliveInterval();
}

[[maybe_unused]] uint8_t AsyncTcpClient::getKeepAliveCount() const {
    return _client->getKeepAliveCount();
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

void AsyncTcpClient::setOnConnectCallback(const std::function<void()>& callback)
{
    _onConnectHandler = callback;
}

void AsyncTcpClient::setOnErrorCallback(const std::function<void(err_t)> &callback) {
    _onErrorHandler = callback;
}
