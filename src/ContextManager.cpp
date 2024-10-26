// ContextManager.cpp

#include "ContextManager.hpp"
#include "Arduino.h"

namespace AsyncTcp {

    /**
     * @brief Constructs a `ContextManager` instance and initializes the default context.
     *
     * The constructor attempts to initialize the default asynchronous context by calling
     * `initDefaultContext`. If initialization fails, `ctx` remains `nullptr`.
     */
    ContextManager::ContextManager() : ctx(nullptr) {
        if (!initDefaultContext()) {
            // Error handling can be performed here if context initialization fails
        }
    }

    /**
     * @brief Initializes the default asynchronous context.
     *
     * @return `true` if the context is successfully initialized, `false` otherwise.
     *
     * This private method sets up the `background_ctx` with the default configuration
     * and assigns `ctx` to the core of `background_ctx` if initialization succeeds.
     */
    bool ContextManager::initDefaultContext() {
        async_context_threadsafe_background_config_t config = async_context_threadsafe_background_default_config();

        if (async_context_threadsafe_background_init(&background_ctx, &config)) {
            ctx = &background_ctx.core;
            return true;
        } else {
            return false;
        }
    }

    /**
     * @brief Adds a worker to the default asynchronous context.
     *
     * @param worker Reference to the `Worker` instance to add.
     * @return `true` if the worker was added successfully, `false` otherwise.
     *
     * This method attempts to add the specified worker to the `ctx`. If `ctx` is `nullptr`
     * or the addition fails, it returns `false` and logs a message to Serial.
     */
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

    /**
     * @brief Retrieves a pointer to the default asynchronous context.
     *
     * @return Pointer to the `async_context_t` instance, representing the default context.
     *
     * This method provides read-only access to the internal context for managing
     * asynchronous operations.
     */
    async_context *ContextManager::getDefaultContext() const {
        return ctx;
    }

    /**
     * @brief Acquires a blocking lock on the asynchronous context.
     *
     * Ensures exclusive access to the context by blocking other threads until
     * `releaseLock` is called.
     */
    void ContextManager::acquireLock() {
        if (ctx) {
            async_context_acquire_lock_blocking(ctx);
        }
    }

    /**
     * @brief Releases the lock on the asynchronous context.
     *
     * This method should be called after `acquireLock` to allow other threads
     * access to the context.
     */
    void ContextManager::releaseLock() {
        if (ctx) {
            async_context_release_lock(ctx);
        }
    }

    /**
     * @brief Signals that a worker has pending work within the context.
     *
     * @param worker Reference to the `Worker` instance for which work is pending.
     *
     * Calls `async_context_set_work_pending` on the specified worker within the context
     * to schedule it for execution. If `ctx` is `nullptr`, a message is logged to Serial.
     */
    void ContextManager::setWorkPending(Worker &worker) {
        if (ctx) {
            async_context_set_work_pending(ctx, worker.getWorker());
        } else {
            ::Serial.println("CTX not available");
        }
    }

} // namespace AsyncTcp
