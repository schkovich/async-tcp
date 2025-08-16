#pragma once

#include "SyncBridge.hpp"

namespace async_tcp {

    class TcpClient; ///< Forward declaration of TcpClient

    class TcpClientSyncAccessor final : public SyncBridge {

            TcpClient &m_io; ///< TCP client for write operations

            // Payload for status() call
            struct StatusPayload final : SyncPayload {
                    uint8_t *result_ptr = nullptr;
            };

            // Called in the correct async context
            uint32_t onExecute(SyncPayloadPtr payload) override;

        public:
            TcpClientSyncAccessor(const AsyncCtx &ctx, TcpClient &io);

            // Blocking, thread-safe status() call
            uint8_t status();
    };

} // namespace async_tcp
