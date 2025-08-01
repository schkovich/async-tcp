#pragma once

/**
 * @file SyncBridge.hpp
 * @brief Synchronous bridge for thread-safe, context-aware resource access
 *
 * This file defines the SyncBridge pattern, which provides a mechanism for
 * safely executing operations on shared resources across different execution
 * contexts (e.g., multiple threads or cores).
 *
 * The SyncBridge ensures:
 *   - Thread-safe access to shared resources
 *   - Operations are executed in the correct async context
 *   - Synchronous (blocking) interface for async operations
 *
 * Usage:
 *   1. Derive from SyncBridge and implement onExecute().
 *   2. Use execute() to perform thread-safe, context-aware operations.
 *
 * @author goran
 * @date 2025-02-21
 */

#include "ContextManager.hpp"
#include <memory>

namespace async_tcp {

    /**
     * @struct SyncPayload
     * @brief Base type for synchronous work payloads
     *
     * This structure serves as a polymorphic base for all payload types that
     * can be passed to SyncBridge for thread-safe execution. It defines a
     * common interface for different types of work data.
     *
     * @note Derived payload types must be final to prevent slicing issues
     * during polymorphic use
     */
    struct SyncPayload {
            SyncPayload() noexcept = default;
            virtual ~SyncPayload() noexcept = default;
    };

    /**
     * @typedef SyncPayloadPtr
     * @brief Convenience alias for a unique pointer to a SyncPayload
     */
    using SyncPayloadPtr = std::unique_ptr<SyncPayload>;

    /**
     * @brief Synchronous handler for async context worker execution (C
     * linkage).
     *
     * This function is registered as the handler for PerpetualWorker in
     * SyncBridge::execute(). It is always called in the correct async context,
     * executes the user operation, and signals completion by releasing the
     * stack-allocated semaphore in the ExecutionContext.
     *
     * @param context Pointer to the async context (unused).
     * @param worker  Pointer to the async_when_pending_worker_t containing the
     * ExecutionContext.
     */
    extern "C" void sync_handler(async_context_t *context,
                                 async_when_pending_worker_t *worker);

    /**
     * @class SyncBridge
     * @brief Thread-safety pattern for synchronized access to resources across
     * execution contexts
     *
     * The SyncBridge design pattern solves a common issue in
     * asynchronous/multi-threaded systems: safely accessing resources that
     * may be modified from different execution contexts. It works by
     * channeling all operations through a synchronized execution mechanism.
     *
     * Key features:
     * - Guarantees thread-safe access to shared resources
     * - Enables operations to run in the correct execution context
     * - Provides a consistent interface for synchronous operations in an async environment
     * - Can be extended for different types of resources (buffers, connections, etc.)
     *
     * Typical usage involves:
     * 1. Deriving from SyncBridge
     * 2. Implementing the onExecute() method with your domain-specific logic
     * 3. Exposing higher-level methods that use execute() internally
     *
     * @see ContextManager for the underlying execution context management
     */
    class SyncBridge {

            const ContextManagerPtr &m_ctx; ///< Context manager for execution
            /**
             * @brief Recursive mutex for serializing access to execute() per
             * SyncBridge instance.
             *
             * Ensures that only one thread or context can execute a synchronous
             * operation on this instance at a time. Initialized in the
             * constructor. Not shared between calls or instances.
             */
            recursive_mutex_t m_execution_mutex = {};

            /**
             * @struct ExecutionContext
             * @brief Per-call context for synchronous execution in SyncBridge
             *
             * Holds all data needed for a single synchronous operation, including:
             *   - Pointer to the SyncBridge instance
             *   - Unique payload for the operation
             *   - Result value
             *   - Pointer to a heap-allocated semaphore for signaling completion
             *
             * This struct is allocated per execute() call, ensuring thread safety
             * and reentrancy.
             */
            struct ExecutionContext {
                    SyncBridge *bridge{nullptr};
                    SyncPayloadPtr payload{};
                    uint32_t result{0};
                    semaphore_t *semaphore{
                        nullptr}; ///< Heap-allocated semaphore for this call
            };

            /**
             * @brief Pure virtual method to be implemented by derived classes
             * for actual resource operation.
             *
             * This method is called in the correct execution context by the
             * SyncBridge machinery. It receives a unique payload for the
             * operation and must return a result code.
             *
             * @param payload Unique pointer to operation data for this
             * execution.
             * @return uint32_t Result code from the operation.
             */
            virtual uint32_t onExecute(SyncPayloadPtr payload) = 0;

            /**
             * @brief Acquire the bridge's recursive mutex for thread-safe
             * execution.
             */
            inline void lockBridge();

            /**
             * @brief Release the bridge's recursive mutex after execution.
             */
            inline void unlockBridge();

            /**
             * @brief Create and initialize a heap-allocated semaphore for this synchronous
             * operation.
             * @return A unique pointer to semaphore_t for signaling completion.
             */
            static inline std::unique_ptr<semaphore_t> getSemaphore();

            /**
             * @brief Allocate and initialize the execution context for this
             * operation.
             * @param payload Pointer to the payload unique_ptr (may be moved
             * from)
             * @param semaphore Pointer to the semaphore for signaling
             * completion
             * @return Pointer to a new ExecutionContext for this call
             */
            inline std::unique_ptr<ExecutionContext>
            getExecutionContext(SyncPayloadPtr payload,
                                std::unique_ptr<semaphore_t> &semaphore);

            /**
             * @brief Allocate and initialize a PerpetualWorker for this
             * operation.
             * @param exec_ctx Pointer to the execution context for this
             * operation
             * @return Unique pointer to a configured PerpetualWorker
             */
            static inline std::unique_ptr<PerpetualWorker>
            getWorker(ExecutionContext *exec_ctx);

            /**
             * @brief Schedule the worker for execution in the async context
             * (atomic with respect to interrupts), wait for completion, and
             * remove the worker.
             * @param worker Unique pointer to the configured PerpetualWorker
             * (ownership transferred)
             * @param exec_ctx Unique pointer to the execution context
             * (ownership transferred)
             * @param semaphore Unique pointer to the semaphore (shared
             * ownership)
             * @return Result code from the operation
             */
            inline uint32_t
            executeWork(std::unique_ptr<PerpetualWorker> worker,
                        std::unique_ptr<ExecutionContext> exec_ctx,
                        std::unique_ptr<semaphore_t> semaphore);

        protected:
            /**
             * @brief Internal method called by the bridging function to perform
             * the execution
             *
             * This method delegates to the virtual onExecute() method, allowing
             * derived classes to implement specific behavior while maintaining
             * the bridge pattern structure.
             *
             * @param payload A unique pointer to a SyncPayload object
             * containing operation data
             * @return uint32_t Result code from the operation
             */
            uint32_t doExecute(SyncPayloadPtr payload);

            /**
             * @brief Friend function for PerpetualWorker handler,
             * called in the correct async context.
             *
             * Executes the user operation by calling doExecute() on the bridge,
             * then signals completion by releasing the heap-allocated semaphore
             * in the ExecutionContext.
             *
             * @param context Pointer to the async context (unused)
             * @param worker Pointer to the async_when_pending_worker_t
             * containing the ExecutionContext
             */
            friend void sync_handler(async_context_t *context,
                                     async_when_pending_worker_t *worker);

        public:
            /**
             * @brief Constructs a SyncBridge with the specified context manager
             *
             * Initializes the recursive mutex for thread-safe execution. This ensures
             * that all synchronous operations on this instance are properly serialized.
             *
             * @param ctx Reference to a shared context manager pointer
             */
            explicit SyncBridge(const ContextManagerPtr &ctx);

            /**
             * @brief Virtual destructor to allow proper cleanup in derived classes
             */
            virtual ~SyncBridge() = default;

            /**
             * @brief Thread-safe execution method that runs the operation in
             * the proper context
             *
             * This is the main entry point for operations on the resource. It:
             * 1. Packages the payload with this bridge instance
             * 2. Submits the operation to be executed synchronously in the
             * proper context
             * 3. Returns the result after execution completes
             *
             * @param payload A unique pointer to a SyncPayload object
             * containing operation data
             * @return uint32_t Result code from the operation
             */
            virtual uint32_t execute(SyncPayloadPtr payload);
    };

} // namespace async_tcp
