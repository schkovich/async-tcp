// Worker.hpp

#pragma once

#include <pico/async_context_threadsafe_background.h>
#include "WorkerData.hpp" // To include the WorkerData definition

namespace AsyncTcp {

    /**
     * @class Worker
     * @brief Manages asynchronous work functions and data within an asynchronous TCP client context.
     *
     * The `Worker` class is responsible for storing and executing a custom work function
     * within an asynchronous context. Each `Worker` instance can be associated with
     * `WorkerData` to facilitate custom data processing.
     */
    class Worker {
    public:
        /**
         * @brief Default constructor for `Worker`.
         *
         * Initializes the worker instance for pending asynchronous work.
         */
        Worker();

        /**
         * @brief Sets a custom work function for the worker.
         *
         * @param work_func Pointer to the function that defines the work logic for this worker.
         *
         * This function allows the user to specify the behavior of the worker when
         * pending work is signaled. The work function receives an `async_context_t`
         * and an `async_when_pending_worker_t` as parameters.
         */
        void setWorkFunction(void (*work_func)(async_context_t *, async_when_pending_worker_t *));

        /**
         * @brief Retrieves the internal worker instance.
         *
         * @return Pointer to the `async_when_pending_worker_t` worker instance.
         *
         * This function provides access to the internal worker instance for use in
         * managing and monitoring asynchronous work states. Primarily intended for internal use.
         */
        async_when_pending_worker_t *getWorker();

        /**
         * @brief Sets the data associated with this worker.
         *
         * @param data Unique pointer to a `WorkerData` instance containing the data for this worker.
         *
         * This function associates `WorkerData` with the worker, allowing the
         * work function to access necessary data for processing.
         */
        void setWorkerData(std::unique_ptr<WorkerData> data);

    private:
        async_when_pending_worker_t worker;            /**< Internal worker instance for async processing. */
        std::unique_ptr<WorkerData> workData;          /**< Data associated with this worker instance. */
    };

} // namespace AsyncTcp
