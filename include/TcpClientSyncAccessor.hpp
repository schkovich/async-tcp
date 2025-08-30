#pragma once

#include "SyncBridge.hpp"
#include <cassert>

#include "iprs_util.hpp"

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

            void verify_execution_context() const {
#ifndef NDEBUG
                assert(!isCrossCore() && "must run on networking core");
                assert(!is_in_isr() && "must not be called from ISR");
#endif
            }

        public:
            TcpClientSyncAccessor(const AsyncCtx &ctx, TcpClient &io);

            // Blocking, thread-safe status() call
            uint8_t status();

            // Generic same-core execution helper (prohibits cross-core)
            template <typename F> uint32_t run_local(F &&callMe) {
                verify_execution_context();
                ctxLock();
                const auto v = static_cast<uint32_t>(callMe());
                ctxUnlock();
                return v;
            }
    };

} // namespace async_tcp
