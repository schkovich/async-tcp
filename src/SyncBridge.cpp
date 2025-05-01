/**
 * @file SyncBridge.cpp
 * @brief Implementation of the SyncBridge pattern for thread-safe resource access
 * 
 * The SyncBridge pattern provides a mechanism to safely access and modify resources
 * across different execution contexts. This implementation handles the core bridging
 * functionality that enables thread-safe operations through context-managed execution.
 * 
 * @author goran
 * @date 2025-02-21
 */

#include "SyncBridge.hpp"
#include "Arduino.h"

namespace AsyncTcp {

    /**
     * @brief Internal execution method that delegates to the derived class implementation
     * 
     * This method is called by the bridging function after the operation has been 
     * properly scheduled in the correct execution context.
     *
     * @param payload Unique pointer to operation data being passed to the handler
     * @return uint32_t Result code from the operation execution
     */
    uint32_t SyncBridge::doExecute(std::unique_ptr<SyncPayload> payload) { // NOLINT
        return onExecute(std::move(payload));
    }

    /**
     * @brief Thread-safe execution method that schedules work in the proper context
     * 
     * This method packages the bridge instance with its payload and schedules
     * the operation to be executed synchronously in the appropriate context.
     * It blocks until execution completes, ensuring thread safety for resource
     * operations.
     *
     * @param payload Unique pointer to operation data
     * @return uint32_t Result code from the operation execution
     */
    uint32_t SyncBridge::execute(std::unique_ptr<SyncPayload> payload) {
        auto* args = new BridgingArgs {
            this,
            std::move(payload)
        };
        return  m_ctx->execWorkSynchronously(&executor_bridging_function, args);
    }

    /**
     * @brief C-style bridging function that translates between the context system and SyncBridge
     * 
     * This function accepts a void pointer to bridging arguments, performs type restoration,
     * delegates to the appropriate SyncBridge instance, and handles cleanup of temporary objects.
     * It serves as the adapter between the C-style callback interface and the C++ object model.
     *
     * @param bridgingArgsPtr Void pointer to a BridgingArgs structure
     * @return uint32_t Result code from the execution
     */
    extern "C" uint32_t executor_bridging_function(void* bridgingArgsPtr) {
        auto* args = static_cast<BridgingArgs*>(bridgingArgsPtr);
        const auto result = args->bridge->doExecute(std::move(args->payload));
        delete args;
        return result;
    }

} // namespace AsyncTcp
