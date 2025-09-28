//
// Created by goran on 02/03/25.
//

#pragma once
#include "WorkerBase.hpp"
#include <pico/async_context.h>

namespace async_tcp {

    using handler_function_t = void (*)(async_context_t *, async_when_pending_worker_t *);

    class PerpetualWorker final : public WorkerBase {
            async_when_pending_worker_t
                m_worker; /**< Internal worker instance for async processing. */

        public:
            PerpetualWorker();

            /**
             * @brief Retrieves the internal worker instance.
             *
             * @return Pointer to the `async_when_pending_worker_t` worker
             * instance.
             *
             * This function provides access to the internal worker instance for
             * use in managing and monitoring asynchronous work states.
             * Primarily intended for internal use.
             */
            async_when_pending_worker_t *getWorker();

            /**
             * @brief Sets a custom work function for the worker.
             *
             * @param handler_function Pointer to the function that defines the
             * work logic for this worker.
             *
             * This function allows the user to specify the behavior of the
             * worker when pending work is signaled. The work function receives
             * an `async_context_t` and an `async_work_on_timeout` as
             * parameters.
             */
            void
            setHandler(handler_function_t handler_function);

            void setPayload(void *data) override;
    };

} // namespace AsyncTcp
