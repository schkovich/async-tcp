#pragma once
#include "EphemeralWorker.hpp"
#include "EventBridge.hpp"

#include <Arduino.h>

namespace async_tcp {

    class EphemeralBridge : public EventBridge {

            friend void
            ephemeral_bridging_function(async_context_t *context,
                                        async_work_on_timeout *worker);
            EphemeralWorker m_ephemeral_worker;
            std::unique_ptr<EphemeralBridge> m_self =
                nullptr; /**< Self-reference for automatic cleanup */

        public:

            explicit EphemeralBridge(const AsyncCtx &ctx)
                : EventBridge(ctx) {}

            void initialiseBridge() override;

            void takeOwnership(std::unique_ptr<EphemeralBridge> self);

            void run(uint32_t run_in);

        protected:

            std::unique_ptr<EphemeralBridge> releaseOwnership();
    };

} // namespace async_tcp
