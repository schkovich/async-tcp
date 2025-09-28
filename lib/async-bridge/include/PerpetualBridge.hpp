#pragma once
#include "EventBridge.hpp"

#include <memory>

namespace async_tcp {

    class PerpetualBridge : public EventBridge {
            friend void
            perpetual_bridging_function(async_context_t *context,
                                        async_when_pending_worker_t *worker);

        public:
            explicit PerpetualBridge(const AsyncCtx &ctx) : EventBridge(ctx) {}

            ~PerpetualBridge() override {
                getContext().removeWorker(m_perpetual_worker);
            }

            void initialiseBridge() override;

            void run();

            // RxBuffer
            virtual void workload(void *data);

        protected:
            PerpetualWorker m_perpetual_worker = {};
    };

    using PerpetualBridgePtr = std::unique_ptr<PerpetualBridge>;

} // namespace async_tcp
