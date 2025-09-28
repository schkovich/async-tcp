#pragma once
#include "EventBridge.hpp"

#include <Arduino.h>
#include <memory>

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

            void run(uint32_t run_in);

        protected:

            void takeOwnership(std::unique_ptr<EphemeralBridge> self);

            std::unique_ptr<EphemeralBridge> releaseOwnership();

            template<typename DerivedHandler>
                static void runHandler(std::unique_ptr<DerivedHandler> handler) {
                DerivedHandler* raw_ptr = handler.get();
                raw_ptr->takeOwnership(std::move(handler));
                handler.reset(); // Release unique_ptr to avoid double deletion
                raw_ptr->initialiseBridge();
                raw_ptr->run(0);
            }
    };

} // namespace async_tcp
