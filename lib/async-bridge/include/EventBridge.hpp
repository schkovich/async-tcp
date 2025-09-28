#pragma once

#include "ContextManager.hpp"

namespace async_tcp {

    extern "C" {
    void perpetual_bridging_function(async_context_t *context,
                                     async_when_pending_worker_t *worker);

    void ephemeral_bridging_function(async_context_t *context,
                                     async_work_on_timeout *worker);
    }

    class EventBridge {

            const AsyncCtx &m_ctx; /**< Reference to the context manager. */

        protected:
            explicit EventBridge(const AsyncCtx &ctx) : m_ctx(ctx) {}

            virtual void onWork() = 0;
            void doWork() { onWork(); };

            [[nodiscard]] const AsyncCtx &getContext() const {
                return m_ctx;
            }
        public:
            virtual ~EventBridge() = default;
            virtual void initialiseBridge() = 0;
    };

} // namespace async_tcp
