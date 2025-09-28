// EphemeralWorker.hpp

#pragma once

#include "WorkerBase.hpp"

#include <pico/async_context.h>

namespace async_tcp {

    /**
     * @class EphemeralWorker
     * @brief Manages asynchronous work functions and data within an
     * asynchronous TCP client context.
     *
     * The `EphemeralWorker` class is responsible for storing and executing a
     * custom work function within an asynchronous context.
     */
    class EphemeralWorker final : public WorkerBase {

            async_at_time_worker_t
                m_worker; /**< Internal worker instance for async processing. */

        public:
            /**
             * @brief Default constructor for `EphemeralWorker`.
             *
             * Initializes the worker instance for pending asynchronous work.
             */
            EphemeralWorker();

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
            void setHandler(void (*handler_function)(async_context_t *,
                                                     async_work_on_timeout *));

            /**
             * @brief Retrieves the internal worker instance.
             *
             * @return Pointer to the `async_at_time_worker_t` worker instance.
             *
             * This function provides access to the internal worker instance for
             * use in managing and monitoring asynchronous work states.
             * Primarily intended for internal use.
             */
            async_at_time_worker_t *getWorker();

            /**
             *
             * @param data raw pointer
             */
            void setPayload(void *data) override;
    };

} // namespace AsyncTcp
