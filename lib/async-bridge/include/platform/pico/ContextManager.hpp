/**
 * @file ContextManager.hpp
 * @brief Manages asynchronous context handling and worker scheduling in the
 * AsyncTCP library.
 *
 * This file declares the ContextManager class, which serves as the foundation
 * for thread-safe operations across different cores in a multi-core system. It
 * provides:
 *   - A thread-safe environment for executing code on a specific core
 *   - Management of worker tasks that can be scheduled for asynchronous
 * execution
 *   - Synchronous execution capabilities across execution contexts
 *   - Lock management to ensure atomic operations when needed
 *
 * ContextManager is a key component in implementing patterns like SyncBridge,
 * which rely on guaranteed execution context for thread safety.
 *
 * The design emphasizes explicit initialization and resource management,
 * particularly important in embedded systems with limited error recovery
 * options.
 *
 * @ingroup AsyncTCPClient
 */

#pragma once

#include "../../IAsyncContext.hpp"

#include <pico/async_context_threadsafe_background.h>

namespace async_bridge {
    class PerpetualWorker;
    class EphemeralWorker;

    /**
     * @brief Function pointer type for asynchronous work handlers.
     *
     * This type represents a pointer to a function that takes a void pointer as
     * its argument and returns an uint32_t. It is used as the callback
     * signature for executing work synchronously through the async_context
     * system.
     *
     * @note The void* parameter allows passing arbitrary data to the handler,
     * and the uint32_t return value can be used to indicate success, failure
     * or other status information.
     */
    typedef uint32_t (*HandlerFunction)(void *param);

    /**
     * @class ContextManager
     * @brief Manages execution contexts and ensures thread
     * safety.
     *
     * The ContextManager provides a stable environment for executing code that
     * needs to run on a specific core or needs to be protected from concurrent
     * access. It serves several key purposes:
     *
     * 1. Thread-Safety: Ensures operations happen in the correct execution
     * context
     * 2. Worker Management: Handles scheduling, tracking, and execution of
     * worker tasks
     * 3. Synchronous Execution: Allows code from one core to be safely executed
     * on another
     * 4. Resource Coordination: Provides locking mechanisms for atomic
     * operations
     *
     * Typical usage involves:
     * - Creating a ContextManager for each core that needs managed execution
     * - Explicitly initializing it with initDefaultContext()
     * - Adding workers or executing synchronous work as needed
     * - Letting the destructor clean up resources when the context is no longer
     * needed
     *
     * Thread safety is guaranteed by the underlying async_context
     * implementation.
     */
    class ContextManager : public IAsyncContext {

            async_context_threadsafe_background_t m_context; /**< Thread-safe background context for asynchronous operations. */
            async_context_t *m_context_core = nullptr; /**< Reference to the core asynchronous context. */
            bool initiated = false; /**< Flag indicating whether the context has been initialized. */

        public:
            /**
             * @brief Constructs a ContextManager with its own background context.
             *
             * This constructor initializes a ContextManager instance and sets up
             * the reference to the internal core context. The context must be
             * explicitly initialized later with initDefaultContext() before use.
             * This separation allows the caller to handle initialization failures
             * appropriately.
             */
            ContextManager();

            /**
             * @brief Destructor that cleans up the asynchronous context and
             * related resources.
             *
             * Ensures all resources associated with the context are properly
             * released by:
             * 1. Checking if the context was successfully initialized
             * 2. Deinitializing the context if it was active
             * 3. Resetting internal pointers to prevent use-after-free issues
             *
             * This cleanup is crucial for preventing resource leaks in
             * long-running applications.
             */
            ~ContextManager() override;

            /**
             * @brief Adds a persistent worker to the context for ongoing task
             * execution.
             *
             * Workers added with this method remain registered until explicitly
             * removed, and can be triggered repeatedly by calling
             * setWorkPending().
             *
             * @param worker Reference to the PerpetualWorker instance to be
             * added to the context.
             * @return true if the worker was successfully added, false if the
             * context is invalid or addition failed.
             */
            bool addWorker(PerpetualWorker& worker) const;

            /**
             * @brief Adds a temporary worker that executes once after an
             * optional delay.
             *
             * Unlike regular workers, ephemeral workers are automatically
             * removed after execution and can be scheduled with a millisecond
             * delay.
             *
             * @param worker The ephemeral worker to schedule
             * @param delay Milliseconds to wait before executing the worker (0
             * = immediate execution)
             * @return true if the worker was successfully scheduled, false
             * otherwise
             */
            bool addWorker(EphemeralWorker& worker, uint32_t delay = 0) const;

            /**
             * @brief Removes a previously added worker from the context.
             *
             * This prevents the worker from being executed even if
             * setWorkPending() is called.
             *
             * @param worker Reference to the PerpetualWorker instance to be
             * removed
             * @return true if the worker was successfully removed, false
             * otherwise
             */
            bool removeWorker(PerpetualWorker& worker) const;

            /**
             * @brief Removes an ephemeral worker from the scheduled queue
             * before it executes.
             *
             * @param worker Reference to the EphemeralWorker instance to be
             * removed
             * @return true if the worker was successfully removed, false
             * otherwise
             */
            bool removeWorker(EphemeralWorker& worker) const;

            /**
             * @brief Marks a worker as having pending work to be processed.
             *
             * This method signals the asynchronous context that the worker has
             * work ready to be executed. The context will call the worker's
             * do_work function as soon as the event loop processes the request.
             *
             * @param worker Reference to the PerpetualWorker instance for which
             * work is set as pending
             */
            void setWorkPending(PerpetualWorker& worker) const;

            /**
             * @brief Acquires a blocking lock on the asynchronous context.
             *
             * This method blocks until it can acquire exclusive access to the
             * context, ensuring that operations between acquireLock() and
             * releaseLock() execute atomically without interference from other
             * threads.
             *
             * @warning Always pair this with a releaseLock() call to prevent
             * deadlocks
             */
            void acquireLock() const;

            /**
             * @brief Releases a previously acquired lock on the context.
             *
             * This should always be called after a successful acquireLock() to
             * allow other threads to access the context.
             */
            void releaseLock() const;

            /**
             * @brief Initializes the asynchronous context with the provided
             * configuration.
             *
             * This method must be called after construction and before using
             * any other methods that interact with the context. It sets up the
             * internal state and prepares the context for worker registration
             * and execution.
             *
             * @param config Reference to a configuration structure for the
             * background context
             * @return true if initialization succeeded or context was already
             * initialized, false otherwise
             */
            bool initDefaultContext(
                async_context_threadsafe_background_config_t &config);

            /**
             * @brief Executes a function synchronously on the context's core.
             *
             * This is the primary method for ensuring thread-safe execution
             * across cores. It guarantees that the handler function will be
             * executed in the context's core, even if called from a different
             * core. The method blocks until execution is complete, providing a
             * synchronous interface for cross-core operations.
             *
             * @note This is the foundation for patterns like SyncBridge that
             * need guaranteed execution context for thread safety.
             *
             * @param handler Function to execute in the context's core
             * @param param Pointer to data needed by the handler function
             * @return The value returned by the handler function
             */
            [[nodiscard]] uint32_t
            execWorkSynchronously(const HandlerFunction &handler,
                                  void *param) const;

            /**
             * @brief Gets the CPU core number where this context is running.
             *
             * Useful for logging or when implementing core-specific behavior.
             *
             * @return The core number (typically 0 or 1 on RP2040 platforms)
             */
            [[nodiscard]] uint8_t getCore() const override;

            /**
             * @brief Verifies that the caller holds the context lock.
             *
             * Useful for debugging and validation in code that requires the
             * lock to be held.
             */
            void checkLock() const override;

            /**
             * @brief Blocks the calling thread until the specified time is
             * reached.
             *
             * This correctly yields the core within the context's execution
             * model, allowing other tasks to run while waiting.
             *
             * @param until The absolute time until which to wait
             */
            void waitUntil(absolute_time_t until) const;
    };

} // namespace async_tcp
