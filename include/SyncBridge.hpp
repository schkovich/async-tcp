#pragma once

#include "ContextManager.hpp"
#include <memory>
#include <atomic>

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

    extern "C" void sync_handler(async_context_t *context, async_when_pending_worker_t *worker);

    /**
     * @class SyncBridge
     * @brief Thread-safety pattern that provides synchronized access to
     * resources across different execution contexts
     *
     * The SyncBridge design pattern solves a common issue in
     * asynchronous/multi-threaded systems: safely accessing resources that may
     * be modified from different execution contexts. It works by channeling all
     * operations through a synchronized execution mechanism.
     *
     * Key features:
     * - Guarantees thread-safe access to shared resources
     * - Enables operations to run in the correct execution context
     * - Provides a consistent interface for synchronous operations in an async
     * environment
     * - Can be extended for different types of resources (buffers, connections,
     * etc.)
     *
     * Typical usage involves:
     * 1. Deriving from SyncBridge
     * 2. Implementing the onExecute() method with your domain-specific logic
     * 3. Exposing higher-level methods that use execute() internally
     *
     * @see ContextManager for the underlying execution context management
     */
    class SyncBridge {

            const ContextManagerPtr &m_ctx;        ///< Context manager for execution
            PerpetualWorker m_worker;       ///< Worker for async operations
            semaphore_t m_semaphore = {};   ///< Semaphore for synchronization
            recursive_mutex_t m_execution_mutex = {}; // Serialize access to execution
            volatile bool m_executing = false;  ///< Track if this instance is currently executing

            /**
             * @brief Abstract method that defines the resource-specific
             * operation logic
             *
             * @param payload A unique pointer to a SyncPayload object
             * containing operation data
             * @return uint32_t Result code from the operation
             */
            virtual uint32_t onExecute(SyncPayloadPtr payload) = 0;

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

            friend void sync_handler(async_context_t *context,
                                         async_when_pending_worker_t *worker);

        public:
            /**
             * @brief Constructs a SyncBridge with the specified context manager
             *
             * @param ctx Reference to a shared context manager pointer
             */
            explicit SyncBridge(const ContextManagerPtr &ctx) : m_ctx(ctx) {}

            void initialize() {
                m_worker.setHandler(sync_handler);
                m_ctx->addWorker(m_worker);
                sem_init(&m_semaphore, 0, 1);
                recursive_mutex_init(&m_execution_mutex);
            }

            /**
             * @brief Virtual destructor to allow proper cleanup in derived
             * classes
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

            static std::atomic<bool> executing_flag; ///< Global re-entrancy guard
    };

    /**
     * @struct ExecutionContext
     * @brief Per-execution context that holds payload and result
     *
     * This structure is allocated per execution call to avoid race conditions
     * when multiple threads call execute() on the same SyncBridge instance.
     */
    struct ExecutionContext {
        SyncBridge* bridge{nullptr};
        SyncPayloadPtr payload{};
        uint32_t result{0};
    };


} // namespace AsyncTcp
