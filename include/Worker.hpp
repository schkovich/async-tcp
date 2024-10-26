// Worker.hpp

#pragma once

#include <pico/async_context_threadsafe_background.h>
#include "WorkerData.hpp" // To include the WorkerData definition

namespace AsyncTcp {

    class Worker {
    public:
        // Constructor
        Worker();

        // Function to set the custom work function for the worker
        void setWorkFunction(void (*work_func)(async_context_t *, async_when_pending_worker_t *));

        // Getter for the worker instance (for internal use)
        async_when_pending_worker_t *getWorker();

        // Set WorkerData
        void setWorkerData(std::unique_ptr<WorkerData> data);

    private:
        async_when_pending_worker_t worker;
        std::unique_ptr<WorkerData> workData;
    };

} // namespace AsyncTcp
