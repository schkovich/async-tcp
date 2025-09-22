//
// Created by goran on 14/08/2025.
//
#include "TcpClientSyncAccessor.hpp"
#include "TcpClient.hpp"
#include "debug_internal.h"

// Next optional refinements (if you want to pursue them later):
// Introduce a small struct Result<size_t> { int status; size_t value; } to avoid ambiguity.
// Reserve a sentinel (e.g. SIZE_MAX) for “error” and keep current return type.
// Adjust callers (e.g. TcpWriter watermark logic) to treat values above a sane maximum (like 256 KB) as error and ignore them.
namespace async_tcp {

    uint32_t TcpClientSyncAccessor::onExecute(const SyncPayloadPtr payload) {
        switch (const auto *p = static_cast<AccessorPayload *>(payload.get()); // NOLINT RTTI disabled
                p->op) {
        case AccessorPayload::STATUS:
            if (p->result_ptr) {
                *p->result_ptr = m_io._ts_status();
                return PICO_OK;
            }
            return PICO_ERROR_NO_DATA;
        case AccessorPayload::CONNECT:
            if (p->ip_ptr && p->connect_result) {
                *p->connect_result = m_io._ts_connect(*p->ip_ptr, p->port);
                return PICO_OK;
            }
            return PICO_ERROR_NO_DATA;
        default:
            return PICO_ERROR_INVALID_ARG;
        }
    }

    TcpClientSyncAccessor::TcpClientSyncAccessor(const AsyncCtx &ctx,
                                                 TcpClient &io)
        : SyncBridge(ctx), m_io(io) {}

    uint8_t TcpClientSyncAccessor::status() {
        // Same-core: take the async context lock and read directly
        if (!isCrossCore()) {
            ctxLock();
            const uint8_t v = m_io._ts_status();
            ctxUnlock();
            return v;
        }

        // Cross-core: execute via bridge to run in the networking context
        uint8_t result = 0;
        auto payload = std::make_unique<AccessorPayload>();
        payload->op = AccessorPayload::STATUS;
        payload->result_ptr = &result;

        if (const auto res = execute(std::move(payload)); res != PICO_OK) {
            DEBUGCORE(
                "[ERROR] TcpClientSyncAccessor::status() returned error %d.\n",
                res);
        }
        return result;
    }

    int TcpClientSyncAccessor::connect(const AIPAddress &ip,
                                       const uint16_t port) {
        // Same-core: take the async context lock and call directly
        if (!isCrossCore()) {
            ctxLock();
            const int result = m_io._ts_connect(ip, port);
            ctxUnlock();
            return result;
        }

        // Cross-core: execute via bridge to run in the networking context
        int result = PICO_ERROR_GENERIC;
        auto payload = std::make_unique<AccessorPayload>();
        payload->op = AccessorPayload::CONNECT;
        payload->ip_ptr = const_cast<AIPAddress*>(&ip);
        payload->port = port;
        payload->connect_result = &result;

        if (const auto res = execute(std::move(payload)); res != PICO_OK) {
            DEBUGCORE(
                "[ERROR] TcpClientSyncAccessor::connect() returned error %d.\n",
                res);
            return static_cast<int>(res);
        }
        return result;
    }

} // namespace async_tcp
