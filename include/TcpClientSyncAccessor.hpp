#pragma once

#include "async_bridge/SyncBridge.hpp"

#include <cassert>

#include "iprs_util.hpp"
#include "WiFi.h"  // For IPAddress

namespace async_tcp {

    using namespace async_bridge;
    class TcpClient;
    using AIPAddress = IPAddress;  // Local alias for IPAddress

    class TcpClientSyncAccessor final : public SyncBridge {
            TcpClient &m_io; ///< TCP client reference

            // Called in the correct async context
            uint32_t onExecute(SyncPayloadPtr payload) override;

            void verify_execution_context() const {
#ifndef NDEBUG
                assert(!isCrossCore() && "must run on networking core");
                assert(!is_in_isr() && "must not be called from ISR");
#endif
            }

        public:
            TcpClientSyncAccessor(IAsyncContext &ctx, TcpClient &io);

            // Blocking, thread-safe status() call
            uint8_t status();

            // Blocking, thread-safe connect() call
            int connect(const AIPAddress &ip, uint16_t port);

            // Generic same-core execution helper (prohibits cross-core)
            template <typename F> uint32_t run_local(F &&callMe) {
                verify_execution_context();
                ctxLock();
                const auto v = static_cast<uint32_t>(callMe());
                ctxUnlock();
                return v;
            }

        protected:
            void onWork() override {
                // onExecute();
            };

            void workload(void *data) override {/* No workload data needed */ };

        private:
            // Payload for accessor operations
            struct AccessorPayload final : SyncPayload {
                enum Operation {
                    STATUS, ///< Get the TCP client status
                    CONNECT ///< Connect to remote host
                };

                Operation op;            ///< The operation to perform
                uint8_t *result_ptr = nullptr; ///< Pointer to store the result (STATUS)

                // Connect operation parameters
                AIPAddress *ip_ptr = nullptr; ///< IP address for connect
                uint16_t port = 0;            ///< Port for connect
                int *connect_result = nullptr; ///< Connect result storage

                AccessorPayload() : op(STATUS) {}
            };

    };

} // namespace async_tcp
