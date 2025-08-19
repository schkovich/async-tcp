#pragma once
#include "EventBridge.hpp"
#include "PerpetualWorker.hpp"

namespace async_tcp {

    class PerpetualBridge : public EventBridge {
            friend void
            perpetual_bridging_function(async_context_t *context,
                                        async_when_pending_worker_t *worker);

        public:
            explicit PerpetualBridge(const AsyncCtx &ctx) : EventBridge(ctx) {}

            ~PerpetualBridge() override {
                getContext().removeWorker(m_perpetual_worker);
                m_perpetual_worker = {}; ///< Reset the perpetual worker
            }

            void initialiseBridge() override;

            void run();

        protected:
            PerpetualWorker m_perpetual_worker = {};
    };

} // namespace async_tcp
