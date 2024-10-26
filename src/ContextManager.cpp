// ContextManager.cpp

#include "ContextManager.hpp"
#include "Arduino.h"

namespace AsyncTcp {

// Constructor
    ContextManager::ContextManager() : ctx(nullptr) {
        // Initialize default context within the constructor
        if (!initDefaultContext()) {
            // Handle error, for example by setting ctx_initiated to false
        }
    }

    // Private method to initialize the default context
    bool ContextManager::initDefaultContext() {
        async_context_threadsafe_background_config_t config = async_context_threadsafe_background_default_config();

        if (async_context_threadsafe_background_init(&background_ctx, &config)) {
            ctx = &background_ctx.core;
            return true;
        } else {
            return false;
        }
    }

// Method to add a worker to the default context
    bool ContextManager::addWorker(Worker &worker) {
        if (!ctx) {
            return false;
        }

        if (!async_context_add_when_pending_worker(ctx, worker.getWorker())) {
            Serial.println("Failed to add read qotd worker!");
            return false;
        }

        return true;
    }

// Getter for the default context
    async_context *ContextManager::getDefaultContext() const {
        return ctx;
    }

    void ContextManager::acquireLock() {
        if (ctx) {
            async_context_acquire_lock_blocking(ctx);
        }
    }

    void ContextManager::releaseLock() {
        if (ctx) {
            async_context_release_lock(ctx);
        }
    }

    void ContextManager::setWorkPending(Worker &worker) {
        if (ctx) {
            async_context_set_work_pending(ctx, worker.getWorker());
        } else {
            ::Serial.println("CTX not available");
        }
    }

} // namespace AsyncTcp