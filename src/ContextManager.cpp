/**
 * @file ContextManager.cpp
 * @brief Implements asynchronous context management and worker scheduling for the AsyncTCP library.
 *
 * This file defines the ContextManager class methods which manage the asynchronous execution
 * context, worker addition, thread-safe lock handling, and synchronous execution via a dedicated API.
 *
 * The key responsibilities include:
 *   - Initializing and deinitializing the background asynchronous context.
 *   - Managing Workers: adding them to the context, marking work as pending.
 *   - Providing a thin wrapper over async_context_execute_sync for synchronous work execution.
 *   - Coordinating thread-safe access via acquireLock and releaseLock.
 *
 * @note The execWorkSynchronously method forwards all parameters directly to
 * async_context_execute_sync, which expects a HandlerFunction (function pointer) and a void* parameter.
 */

#include <sys/stat.h>
#include "ContextManager.hpp"
#include "Arduino.h"
#include <chrono>
#include <future>
#include "e5/LedDebugger.hpp"

namespace AsyncTcp {

/**
     * @brief Constructs a ContextManager instance.
     *
     * This constructor initializes the ContextManager without immediately setting up the asynchronous context.
     * The context must be explicitly initialized by calling initDefaultContext, allowing the consuming code
     * to control the timing and conditions of initialization. It also allows the consuming code to handle
     * initialization failures explicitly.
     */
    ContextManager::ContextManager() = default;

    /**
     * @brief Destructor that deinitializes the background asynchronous context.
     *
     * If the context was successfully initialized, the destructor calls async_context_deinit
     * to deinitialize it and then resets the internal state.
     */
    ContextManager::~ContextManager() {
        if (initiated) {
            // Deinitialize the thread-safe background context
            async_context_deinit(ctx);
            ctx = nullptr;
            background_ctx = {};
            initiated = false;
        }
    }
    
    /**
     * @brief Initializes the default asynchronous context.
     *
     * This method sets up the `background_ctx` with the default configuration and assigns `ctx`
     * to the core of `background_ctx` if initialization succeeds. It must be called explicitly
     * by the consuming code to control the timing and conditions of initialization.
     *
     * @return `true` if the context is successfully initialized, `false` otherwise.
     *
     * By separating initialization from construction, this design allows for explicit error handling
     * and resource management, which is crucial in environments where exceptions are not used.
     */
    bool ContextManager::initDefaultContext() {
        if (false == initiated) {
            async_context_threadsafe_background_config_t config = async_context_threadsafe_background_default_config();
            if (async_context_threadsafe_background_init(&background_ctx, &config)) {
                ctx = &background_ctx.core;
                initiated = true;
                return true;
            }
            return false;
        }
        return true;
    }

    /**
     * @brief Adds a Worker to the asynchronous context.
     *
     * Attempts to register the provided Worker with the current context.
     * If the context pointer is null or the worker addition fails, returns false.
     *
     * @param worker Reference to the Worker instance to be added.
     * @return true if the worker was added successfully; false otherwise.
     */
    bool ContextManager::addWorker(Worker &worker) const {
        if (!ctx) {
            return false;
        }
        if (!async_context_add_when_pending_worker(ctx, worker.getWorker())) {
            DEBUGV("ContextManager::addWorker - Failed to add worker!\n");
            return false;
        }
        DEBUGV("ContextManager::addWorker - when pending worker added!\n");
        return true;
    }

    /**
     * @brief Adds a Worker to the asynchronous context.
     *
     * Attempts to register the provided Worker with the current context.
     * If the context pointer is null or the worker addition fails, returns false.
     *
     * @param worker Reference to the Worker instance to be added.
     * @return true if the worker was added successfully; false otherwise.
     */
    bool ContextManager::addWorker(async_when_pending_worker_t& worker) const {
        if (!ctx) {
            return false;
        }
        if (!async_context_add_when_pending_worker(ctx, &worker)) {
            DEBUGV("ContextManager::addWorker 2 - Failed to add worker!\n");
            return false;
        }
        DEBUGV("ContextManager::addWorker 2 - when pending worker added!\n");
        return true;
    }

    bool ContextManager::addEphemeralWorker(EphemeralWorker& worker, const uint32_t delay) const {

        if (!ctx) {
            DEBUGV("ContextManager::addEphemeralWorker - no context!\n");
            return false;
        }

        const auto worker_ptr = worker.getWorker();

        if (worker_ptr->do_work == nullptr) {
            DEBUGV("ContextManager::addEphemeralWorker - handler function not defined!\n");
            return false;
        }

        if (worker_ptr->user_data == nullptr) {
            DEBUGV("ContextManager::addEphemeralWorker - no user data set!\n");
            return false;
        }

        if (!async_context_add_at_time_worker_in_ms(ctx, worker_ptr, delay)) {
            DEBUGV("ContextManager::addEphemeralWorker - Failed to add worker!\n");
            return false;
        }

        DEBUGV("[c%d][%llu][INFO] ContextManager::addEphemeralWorker - %p\n",
            rp2040.cpuid(), time_us_64(), worker_ptr->user_data);

        return true;
    }

    bool ContextManager::removeWorker(Worker &worker) const {
        if (!ctx) {
            DEBUGV("ContextManager::removeWorker - no context!\n");
            return false;
        }

        if (!async_context_remove_when_pending_worker(ctx, worker.getWorker())) {
            DEBUGV("ContextManager::removeWorker - Failed to remove when pending worker!\n");
            return false;
        }

        DEBUGV("ContextManager::removeWorker - when pending worker removed!\n");
        return true;
    }

    bool ContextManager::removeWorker(EphemeralWorker &worker) const {
        if (!ctx) {
            return false;
        }

        if (!async_context_remove_at_time_worker(ctx, worker.getWorker())) {
            DEBUGV("ContextManager::removeWorker(Worker &worker) - Failed to remove at time worker!\n");
            return false;
        }

        return true;

    }

    /**
     * @brief Marks a Worker as having pending work within the context.
     *
     * Signals that the specified Worker has work to be processed by invoking
     * async_context_set_work_pending. If the context pointer is null, a debug message is logged.
     *
     * @param worker Reference to the Worker instance for which work is pending.
     */
    void ContextManager::setWorkPending(Worker &worker) const {
        if (ctx) {
            async_context_set_work_pending(ctx, worker.getWorker());
        }
        else {
            DEBUGV("CTX not available\n");
        }
    }

    void ContextManager::setWorkPending(async_when_pending_worker_t& worker) const {
        if (ctx) {
            async_context_set_work_pending(ctx, &worker);
        }
        else {
            DEBUGV("CTX not available\n");
        }
    }

    /**
     * @brief Acquires a blocking lock on the asynchronous context.
     *
     * Ensures exclusive access to the context by preventing interrupts from other threads
     * until releaseLock is called.
     */
    void ContextManager::acquireLock() const {
        if (ctx) {
            async_context_acquire_lock_blocking(ctx);
        }
    }

    /**
     * @brief Releases the lock on the asynchronous context.
     *
     * This function should be called after acquireLock to enable other threads to access the context.
     */
    void ContextManager::releaseLock() const {
        if (ctx) {
            async_context_release_lock(ctx);
        }
    }

    /**
     * @brief Executes a handler function synchronously on the asynchronous context's core.
     *
     * This method serves as a thin wrapper around the async_context_execute_sync function.
     * It forwards the provided callback (HandlerFunction) along with its associated parameter
     * to async_context_execute_sync for synchronous execution.
     *
     * @param handler The function pointer (HandlerFunction) to execute. Must conform to the expected signature.
     * @param param   Pointer to the parameters required by the handler function.
     * @return The return value from the handler function.
     */
    uint32_t ContextManager::execWorkSynchronously(const HandlerFunction& handler, void* param) const {
        const auto result = async_context_execute_sync(ctx, handler, param);
        return result; // Return success code
    }

    /**
     * @brief Retrieves the core number associated with the asynchronous context.
     *
     * @return The core number managed by the asynchronous context.
     */
    uint8_t ContextManager::getCore() const { return ctx->core_num; }

    void ContextManager::checkLock() const {
        // async_context_wait_until(ctx, time_us_64() + 100000);
        async_context_lock_check(ctx);
    }

    void ContextManager::waitUntil(const absolute_time_t until) const {
        async_context_wait_until(ctx, until);
    }
} // namespace AsyncTcp
