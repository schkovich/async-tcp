#pragma once

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

    extern "C" uint32_t executor_bridging_function(void *bridgingArgsPtr);

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

            const ContextManagerPtr &m_ctx;

            /**
             * @brief Abstract method that defines the resource-specific
             * operation logic
             *
             * @param payload A unique pointer to a SyncPayload object
             * containing operation data
             * @return uint32_t Result code from the operation
             */
            virtual uint32_t
            onExecute(std::unique_ptr<SyncPayload> payload) = 0;

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
            uint32_t doExecute(std::unique_ptr<SyncPayload> payload);

            friend uint32_t executor_bridging_function(void *bridgingArgsPtr);

        public:
            /**
             * @brief Constructs a SyncBridge with the specified context manager
             *
             * @param ctx Reference to a shared context manager pointer
             */
            explicit SyncBridge(const ContextManagerPtr &ctx) : m_ctx(ctx) {}

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
            virtual uint32_t execute(std::unique_ptr<SyncPayload> payload);
    };

    /**
     * @struct BridgingArgs
     * @brief Internal structure that packages a SyncBridge with its payload for
     * execution
     *
     * This structure allows passing both the bridge instance and its payload
     * through the C-style bridging function interface required by the execution
     * context.
     */
    struct BridgingArgs {
            SyncBridge *bridge{};
            std::unique_ptr<SyncPayload> payload{};
    };

    /**
     * @typedef SyncPayloadPtr
     * @brief Convenience alias for a unique pointer to a SyncPayload
     */
    using SyncPayloadPtr = std::unique_ptr<SyncPayload>;

} // namespace AsyncTcp
