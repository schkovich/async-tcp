//
// Created by goran on 14/08/2025.
//
#include "TcpClientSyncAccessor.hpp"
#include "TcpClient.hpp"
#include "debug_internal.h"

namespace async_tcp {

    uint32_t TcpClientSyncAccessor::onExecute(const SyncPayloadPtr payload) {
        const auto *p = static_cast<StatusPayload *>(payload.get());
        *p->result_ptr = m_io._ts_status();
        return PICO_OK;
    }

    TcpClientSyncAccessor::TcpClientSyncAccessor(const AsyncCtx &ctx,
                                                 TcpClient &io)
        : SyncBridge(ctx), m_io(io) {}

    uint8_t TcpClientSyncAccessor::status() {
        uint8_t result = 0;
        auto payload = std::make_unique<StatusPayload>();
        payload->result_ptr = &result;
        if (const auto res = execute(std::move(payload)); res != PICO_OK) {
            DEBUGCORE(
                "[ERROR] TcpClientSyncAccessor::status() returned error %d.\n",
                res);
        }

        return result;
    }

} // namespace async_tcp
