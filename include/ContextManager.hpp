// ContextManager.hpp

#pragma once

#include <pico/async_context_threadsafe_background.h>
#include "Worker.hpp"

namespace AsyncTcp {

    class ContextManager {
    public:
        // Constructor
        ContextManager();

        // Adds worker to the default context
        bool addWorker(Worker &worker);

        // Gets the default context
        [[nodiscard]] async_context_t *getDefaultContext() const;

        // Acquires lock on the context
        void acquireLock();

        // Releases lock on the context
        void releaseLock();

        // Sets work pending for a worker
        void setWorkPending(Worker &worker);

    private:
        // Initializes the default context
        bool initDefaultContext();

        async_context_threadsafe_background_t background_ctx = {};

        async_context_t *ctx = nullptr;
    };

} // namespace AsyncTcp
