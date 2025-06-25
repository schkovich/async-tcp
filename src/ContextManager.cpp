/**
 * @file ContextManager.cpp
 * @brief Implements asynchronous context management and worker scheduling for the AsyncTCP library.
 *
 * This file defines the ContextManager class methods which manage the asynchronous execution
 * context, worker scheduling, thread-safe lock handling, and synchronous execution across cores.
 *
 * The ContextManager provides several critical capabilities:
 *   - Safe cross-core execution through the execWorkSynchronously mechanism
 *   - Worker lifecycle management (adding, removing, signaling)
 *   - Lock-based thread safety for atomic operations
 *   - Resource management with explicit initialization and cleanup
 *
 * Most methods verify the context's validity before performing operations, making
 * the class robust against improper usage sequences (e.g., using before initialization).
 *
 * @note The execWorkSynchronously method is a cornerstone of the thread-safety patterns
 * used throughout the library, as it guarantees that operations execute in the proper context.
 */

#include "ContextManager.hpp"
#include "Arduino.h"

namespace AsyncTcp {

    /**
     * @brief Constructs a ContextManager instance with a specific background context.
     *
     * This constructor initializes the ContextManager without immediately setting up the asynchronous context.
     * The context must be explicitly initialized by calling initDefaultContext(), allowing the consuming code
     * to control the timing and conditions of initialization and handle initialization failures explicitly.
     *
     * @param context Reference to the background context structure to be managed
     */
    ContextManager::ContextManager(async_context_threadsafe_background_t& context) : m_context(&context), m_context_core(&context.core) {}

    /**
     * @brief Destructor that deinitializes the background asynchronous context.
     *
     * If the context was successfully initialized, the destructor:
     * 1. Calls async_context_deinit to properly clean up the context
     * 2. Nullifies internal pointers to prevent use-after-free issues
     * 3. Resets the initialization flag
     *
     * This ensures all resources are properly released even if the user code doesn't
     * explicitly clean up the context.
     */
    ContextManager::~ContextManager() {
        if (initiated) {
            // Deinitialize the thread-safe background context
            async_context_deinit(m_context_core);
            m_context_core = nullptr;
            m_context = {};
            initiated = false;
        }
    }
    
    /**
     * @brief Initializes the asynchronous context with the provided configuration.
     *
     * This method prepares the context for worker registration and task execution by:
     * 1. Checking if the context is already initialized (to prevent double initialization)
     * 2. Calling async_context_threadsafe_background_init with the provided configuration
     * 3. Setting the initiated flag if initialization succeeds
     *
     * @param config Reference to a configuration structure for the background context
     * @return true if initialization succeeded or context was already initialized, false on failure
     *
     * By separating initialization from construction, this design allows for explicit error handling
     * and resource management, which is crucial in environments where exceptions are not used.
     */
    bool ContextManager::initDefaultContext(async_context_threadsafe_background_config_t& config) {
        if (false == initiated) {
            if (async_context_threadsafe_background_init(m_context, &config)) {
                initiated = true;
                return true;
            }
            return false;
        }
        return true;
    }

    /**
     * @brief Adds a PerpetualWorker to the asynchronous context for ongoing task execution.
     *
     * This method registers a Worker object with the context, allowing it to be
     * triggered for execution when needed. It first checks context validity, then
     * attempts to add the worker to the underlying SDK context.
     *
     * @param worker Reference to the PerpetualWorker instance to be added
     * @return true if the worker was successfully added, false if the context is invalid or addition failed
     */
    bool ContextManager::addWorker(PerpetualWorker &worker) const {
        if (!m_context_core) {
            return false;
        }
        if (!async_context_add_when_pending_worker(m_context_core, worker.getWorker())) {
            DEBUGV("ContextManager::addWorker - Failed to add worker!\n");
            return false;
        }
        return true;
    }

    /**
     * @brief Adds an ephemeral worker that executes once after an optional
     * delay.
     *
     * This method schedules a temporary worker that will be automatically
     * removed after execution. It performs several validations to ensure the
     * worker is properly configured:
     * 1. Checks if the context is valid
     * 2. Verifies that the worker has a work handler function
     * 3. Confirms that user data is set
     * 4. Attempts to add the worker with the specified delay
     *
     * @param worker The ephemeral worker to schedule
     * @param delay Milliseconds to wait before executing the worker (0 =
     * immediate execution)
     * @return true if the worker was successfully scheduled, false if any
     * validation failed
     */
    bool ContextManager::addWorker(EphemeralWorker& worker, const uint32_t delay) const {

        if (!m_context_core) {
            DEBUGV("ContextManager::addWorker - no context!\n");
            return false;
        }

        const auto worker_ptr = worker.getWorker();

        if (worker_ptr->do_work == nullptr) {
            DEBUGV("ContextManager::addWorker - handler function not defined!\n");
            return false;
        }

        if (worker_ptr->user_data == nullptr) {
            DEBUGV("ContextManager::addWorker - no user data set!\n");
            return false;
        }

        if (!async_context_add_at_time_worker_in_ms(m_context_core, worker_ptr, delay)) {
            DEBUGV("ContextManager::addWorker - Failed to add worker!\n");
            return false;
        }

        return true;
    }

    /**
     * @brief Removes a previously added PerpetualWorker from the context.
     *
     * This prevents the worker from being executed even if setWorkPending() is called.
     * The method checks context validity before attempting the removal.
     *
     * @param worker Reference to the PerpetualWorker instance to be removed
     * @return true if the worker was successfully removed, false if the context is invalid or removal failed
     */
    bool ContextManager::removeWorker(PerpetualWorker &worker) const {
        if (!m_context_core) {
            DEBUGV("ContextManager::removeWorker - no context!\n");
            return false;
        }

        if (!async_context_remove_when_pending_worker(m_context_core, worker.getWorker())) {
            DEBUGV("ContextManager::removeWorker - Failed to remove when pending worker!\n");
            return false;
        }

        return true;
    }

    /**
     * @brief Removes an ephemeral worker from the scheduled queue before it executes.
     *
     * This method can cancel a previously scheduled ephemeral worker if it hasn't
     * executed yet. It checks context validity before attempting the removal.
     *
     * @param worker Reference to the EphemeralWorker instance to be removed
     * @return true if the worker was successfully removed, false if the context is invalid or removal failed
     */
    bool ContextManager::removeWorker(EphemeralWorker &worker) const {
        if (!m_context_core) {
            return false;
        }

        if (!async_context_remove_at_time_worker(m_context_core, worker.getWorker())) {
            DEBUGV("ContextManager::removeWorker(Worker &worker) - Failed to remove at time worker!\n");
            return false;
        }

        return true;
    }

    /**
     * @brief Marks a PerpetualWorker as having pending work to be processed.
     *
     * This method signals the asynchronous context that the worker has work ready
     * to be executed. The context will call the worker's do_work function as soon
     * as the event loop processes the request.
     *
     * @param worker Reference to the PerpetualWorker instance to be signaled
     */
    void ContextManager::setWorkPending(PerpetualWorker &worker) const {
        if (m_context_core) {
            async_context_set_work_pending(m_context_core, worker.getWorker());
        }
    }

    /**
     * @brief Acquires a blocking lock on the asynchronous context.
     *
     * This method blocks until it can acquire exclusive access to the context,
     * ensuring that operations between acquireLock() and releaseLock() execute
     * atomically without interference from other threads or interrupt handlers.
     *
     * @warning Always pair this with a releaseLock() call to prevent deadlocks
     */
    void ContextManager::acquireLock() const {
        if (m_context_core) {
            async_context_acquire_lock_blocking(m_context_core);
        }
    }

    /**
     * @brief Releases a previously acquired lock on the context.
     *
     * This should always be called after a successful acquireLock() to allow
     * other threads and interrupt handlers to access the context.
     */
    void ContextManager::releaseLock() const {
        if (m_context_core) {
            async_context_release_lock(m_context_core);
        }
    }

    /**
     * @brief Executes a function synchronously on the context's core.
     *
     * This is the primary method for ensuring thread-safe execution across cores.
     * It guarantees that the handler function will be executed in the context's
     * core, even if called from a different core. The method blocks until execution
     * is complete, providing a synchronous interface for cross-core operations.
     *
     * This method is the foundation for patterns like SyncBridge that need guaranteed
     * execution context for thread safety.
     *
     * @param handler Function to execute in the context's core
     * @param param Pointer to data needed by the handler function
     * @return The value returned by the handler function
     */
    uint32_t ContextManager::execWorkSynchronously(const HandlerFunction& handler, void* param) const {
        return async_context_execute_sync(m_context_core, handler, param);
    }

    /**
     * @brief Gets the CPU core number where this context is running.
     *
     * Useful for logging or when implementing core-specific behavior.
     *
     * @return The core number (typically 0 or 1 on RP2040 platforms)
     */
    uint8_t ContextManager::getCore() const { return m_context_core->core_num; }

    /**
     * @brief Verifies that the caller holds the context lock.
     *
     * This diagnostic function checks that the lock is held by the current execution context,
     * useful for debugging and validating code that requires the lock to be held.
     */
    void ContextManager::checkLock() const {
        async_context_lock_check(m_context_core);
    }

    /**
     * @brief Blocks the calling thread until the specified time is reached.
     *
     * This correctly yields the core within the context's execution model,
     * allowing other tasks to run while waiting. It's a thread-safe alternative
     * to basic delay functions that properly integrates with the async context.
     *
     * @param until The absolute time until which to wait
     */
    void ContextManager::waitUntil(const absolute_time_t until) const {
        async_context_wait_until(m_context_core, until);
    }
} // namespace AsyncTcp
