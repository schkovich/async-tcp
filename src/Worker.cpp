// Worker.cpp

#include "Worker.hpp"

namespace AsyncTcp {

// Constructor
    Worker::Worker() : worker{}, workData(std::make_unique<WorkerData>()) {
        // Initialization of worker should be here (if needed)
        worker.do_work = nullptr; // Initialize to nullptr until set by setWorkFunction
    }

// Function to set the custom work function for the worker
    void Worker::setWorkFunction(void (*work_func)(async_context_t *, async_when_pending_worker_t *)) {
        worker.do_work = work_func;
    }

// Getter for the worker instance (for internal use)
    async_when_pending_worker_t *Worker::getWorker() {
        return &worker;
    }

    void Worker::setWorkerData(std::unique_ptr<WorkerData> data) {
        worker.user_data = data.release();
    }
} // namespace AsyncTcp
