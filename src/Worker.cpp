// Worker.cpp

#include "Worker.hpp"

namespace AsyncTcp {

    /**
     * @brief Constructs a `Worker` instance with default initialization.
     *
     * Initializes the `WorkerData` member and sets the `do_work` function pointer to `nullptr`
     * until it is defined by `setWorkFunction`. This ensures that the worker’s work function
     * is not invoked until explicitly set.
     */
    Worker::Worker() : worker{}, workData(std::make_unique<WorkerData>()) {
        worker.do_work = nullptr; // Initialize to nullptr until set by setWorkFunction
    }

    /**
     * @brief Sets the custom work function for the worker.
     *
     * @param work_func Pointer to the function that defines the work logic for this worker.
     *
     * This function assigns the provided work function to `do_work`, allowing the worker
     * to execute custom logic when asynchronous work is triggered.
     */
    void Worker::setWorkFunction(void (*work_func)(async_context_t *, async_when_pending_worker_t *)) {
        worker.do_work = work_func;
    }

    /**
     * @brief Retrieves a pointer to the internal `async_when_pending_worker_t` instance.
     *
     * @return Pointer to `async_when_pending_worker_t`, allowing internal management
     *         and monitoring of the worker instance.
     *
     * This function provides access to the internal worker, intended for usage within
     * the library’s asynchronous context handling.
     */
    async_when_pending_worker_t *Worker::getWorker() {
        return &worker;
    }

    /**
     * @brief Sets the `WorkerData` associated with this worker.
     *
     * @param data Unique pointer to a `WorkerData` instance to be used by the worker.
     *
     * The function transfers ownership of the provided `WorkerData` to the worker’s
     * `user_data` pointer. This allows the worker to access necessary data during
     * processing. The `data` pointer is released, ensuring sole ownership by `user_data`.
     */
    void Worker::setWorkerData(std::unique_ptr<WorkerData> data) {
        worker.user_data = data.release();
    }

} // namespace AsyncTcp
