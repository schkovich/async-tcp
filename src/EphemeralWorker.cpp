// EphemeralWorker.cpp

#include "EphemeralWorker.hpp"

namespace AsyncTcp {

    /**
     * @brief Constructs a `EphemeralWorker` instance with default
     * initialization.
     *
     * Sets the `do_work` function pointer to `nullptr` until it is defined by
     * `setHandler`. This ensures that the worker’s work function is not invoked
     * until explicitly set.
     */
    EphemeralWorker::EphemeralWorker() : m_worker{} {
        m_worker.do_work =
            nullptr; // Initialize to nullptr until set by setHandler
        m_worker.user_data =
            nullptr; // Initialize to nullptr until set by setPayload.
    }

    /**
     * @brief Sets the custom work function for the worker.
     *
     * @param handler_function Pointer to the function that defines the work
     * logic for this worker.
     *
     * This function assigns the provided work function to `do_work`, allowing
     * the worker to execute custom logic when asynchronous work is triggered.
     */
    void EphemeralWorker::setHandler(
        void (*handler_function)(async_context_t *, async_work_on_timeout *)) {
        m_worker.do_work = handler_function;
    }

    /**
     * @brief Retrieves a pointer to the internal `async_at_time_worker_t`
     * instance.
     *
     * @return Pointer to `async_at_time_worker_t`, allowing internal management
     *         and monitoring of the worker instance.
     *
     * This function provides access to the internal worker, intended for usage
     * within the library’s asynchronous context handling.
     */
    async_at_time_worker_t *EphemeralWorker::getWorker() { return &m_worker; }

    /**
     *
     * @param data Pointer to EventBridge instance
     */
    void EphemeralWorker::setPayload(void *data) { m_worker.user_data = data; }

} // namespace AsyncTcp
