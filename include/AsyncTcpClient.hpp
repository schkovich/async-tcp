/*
    AsyncTcpClient.hpp - Library for Arduino Wifi shield.
    Copyright (c) 2011-2014 Arduino.  All right reserved.

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

    Modified by Ivan Grokhotkov, December 2014 - esp8266 support
    Hacked to tiny bits and set on fire by Earle F. Philhower, III - 2022 - Pico W support
*/

#pragma once

#include <memory>
#include "Print.h"
#include "Client.h"
#include "IPAddress.h"
#include "include/slist.h"

#ifndef TCP_MSS
#define TCP_MSS 1460 // lwip1.4
#endif

#define ASYNC_TCP_CLIENT_MAX_PACKET_SIZE TCP_MSS
#define ASYNC_TCP_CLIENT_MAX_FLUSH_WAIT_MS 300

#define TCP_DEFAULT_KEEP_ALIVE_IDLE_SEC          7200 // 2 hours
#define TCP_DEFAULT_KEEP_ALIVE_INTERVAL_SEC      75   // 75 sec
#define TCP_DEFAULT_KEEP_ALIVE_COUNT             9    // fault after 9 failures

class AsyncTcpClientContext;
class WiFiServer; // Is it needed and used?

class AsyncTcpClient : public Client, public SList<AsyncTcpClient> {
protected:
    AsyncTcpClient(AsyncTcpClientContext* client);

public:
    AsyncTcpClient();
    virtual ~AsyncTcpClient();
    AsyncTcpClient(const AsyncTcpClient&);
    AsyncTcpClient& operator=(const AsyncTcpClient&);

    // b/c this is both a real class and a virtual parent of the secure client, make sure
    // there's a safe way to copy from the pointer without 'slicing' it; i.e. only the base
    // portion of a derived object will be copied, and the polymorphic behavior will be corrupted.
    //
    // this class still implements the copy and assignment though, so this is not yet enforced
    // (but, *should* be inside the Core itself, see httpclient & server)
    //
    // ref.
    // - https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#Rc-copy-virtual
    // - https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#Rh-copy
    [[maybe_unused]] virtual std::unique_ptr<AsyncTcpClient> clone() const;

    virtual uint8_t status();
    virtual int connect(IPAddress ip, uint16_t port) override;
    virtual int connect(const char *host, uint16_t port) override;
    virtual int connect(const String& host, uint16_t port);
    virtual size_t write(uint8_t) override;
    virtual size_t write(const uint8_t *buf, size_t size) override;
    size_t write(Stream& stream);

    virtual int available() override;
    virtual int read() override;
    virtual int read(uint8_t* buf, size_t size) override;
    int read(char* buf, size_t size);

    virtual int peek() override;
    virtual size_t peekBytes(uint8_t *buffer, size_t length);

    [[maybe_unused]] size_t peekBytes(char *buffer, size_t length) {
        return peekBytes((uint8_t *) buffer, length);
    }
    virtual void flush() override {
        (void)flush(0);    // wait for all outgoing characters to be sent, output buffer should be empty after this call
    }
    virtual void stop() override {
        (void)stop(0);
    }
    bool flush(unsigned int maxWaitMs);
    bool stop(unsigned int maxWaitMs);
    virtual uint8_t connected() override;
    virtual operator bool() override;

    [[maybe_unused]] IPAddress remoteIP();

    [[maybe_unused]] uint16_t  remotePort();
    IPAddress localIP();

    [[maybe_unused]] uint16_t  localPort();

    [[maybe_unused]] static void setLocalPortStart(uint16_t port) {
        _localPort = port;
    }

    int availableForWrite() override;

    friend class WiFiServer;

    using Print::write;

    [[maybe_unused]] static void stopAll();

    [[maybe_unused]] static void stopAllExcept(AsyncTcpClient * c);

    void     keepAlive(uint16_t idle_sec = TCP_DEFAULT_KEEP_ALIVE_IDLE_SEC, uint16_t intv_sec = TCP_DEFAULT_KEEP_ALIVE_INTERVAL_SEC, uint8_t count = TCP_DEFAULT_KEEP_ALIVE_COUNT);

    [[maybe_unused]] bool     isKeepAliveEnabled() const;

    [[maybe_unused]] uint16_t getKeepAliveIdle() const;

    [[maybe_unused]] uint16_t getKeepAliveInterval() const;

    [[maybe_unused]] uint8_t  getKeepAliveCount() const;

    [[maybe_unused]] void     disableKeepAlive() {
        keepAlive(0, 0, 0);
    }

    // default NoDelay=False (Nagle=True=!NoDelay)
    // Nagle is for shortly delaying outgoing data, to send less/bigger packets
    // Nagle should be disabled for telnet-like/interactive streams
    // Nagle is meaningless/ignored when Sync=true
    [[maybe_unused]] static void setDefaultNoDelay(bool noDelay);

    [[maybe_unused]] static bool getDefaultNoDelay();

    [[maybe_unused]] bool getNoDelay() const;
    void setNoDelay(bool nodelay);

    // default Sync=false
    // When sync is true, all writes are automatically flushed.
    // This is slower but also does not allocate
    // temporary memory for sending data
    [[maybe_unused]] static void setDefaultSync(bool sync);

    [[maybe_unused]] static bool getDefaultSync();

    [[maybe_unused]] bool getSync() const;
    void setSync(bool sync);

    // peek buffer API is present
    //virtual bool hasPeekBufferAPI () const override;

    // return number of byte accessible by peekBuffer()
    //virtual size_t peekAvailable () override;

    // return a pointer to available data buffer (size = peekAvailable())
    // semantic forbids any kind of read() before calling peekConsume()
    //virtual const char* peekBuffer () override;

    // consume bytes after use (see peekBuffer)
    //virtual void peekConsume (size_t consume) override;

    //virtual bool outputCanTimeout () override { return connected(); }
    //virtual bool inputCanTimeout () override { return connected(); }

protected:

    [[maybe_unused]] static int8_t _s_connected(void* arg, void* tpcb, int8_t err);

    [[maybe_unused]] static void _s_err(void* arg, int8_t err);

    static int8_t _connected(void* tpcb, int8_t err);

    [[maybe_unused]] void _err(int8_t err);

    AsyncTcpClientContext*  _client;
    AsyncTcpClient* _owned;
    static uint16_t _localPort;
};
