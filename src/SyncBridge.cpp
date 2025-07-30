/**
 * @file SyncBridge.cpp
 * @brief Implementation of SyncBridge for thread-safe, context-aware resource access
 *
 * This file implements the SyncBridge pattern, providing a mechanism for safely executing operations
 * on shared resources across different execution contexts (e.g., multiple threads or cores).
 *
 * The implementation uses stack-allocated workers and semaphores for each synchronous operation,
 * ensuring reentrancy and thread safety. All synchronization is per-call and per-instance.
 *
 * @author goran
 * @date 2025-02-21
 */

#include "SyncBridge.hpp"
#include <atomic>

namespace async_tcp {

    /**
     * @brief Internal execution method that delegates to the derived class implementation.
     *
     * This method is called by the bridging function after the operation has been properly scheduled
     * in the correct execution context. It simply forwards the payload to the derived class's onExecute().
     *
     * @param payload Unique pointer to operation data being passed to the handler
     * @return uint32_t Result code from the operation execution
     */
    uint32_t
    SyncBridge::doExecute(SyncPayloadPtr payload) { // NOLINT
        return onExecute(std::move(payload));
    }

    /**
     * @brief Thread-safe execution method that schedules work in the proper context.
     *
     * This method packages the bridge instance with its payload and schedules the operation to be executed
     * synchronously in the appropriate context. It uses a stack-allocated worker and semaphore for each call,
     * ensuring reentrancy and thread safety. The function blocks until execution completes.
     *
     * @param payload Unique pointer to operation data
     * @return uint32_t Result code from the operation execution
     */
    uint32_t SyncBridge::execute(SyncPayloadPtr payload) {

        recursive_mutex_enter_blocking(&m_execution_mutex); // Acquire the serialisation lock.

        semaphore_t semaphore = {nullptr};
        sem_init(&semaphore, 0, 1);  // Initialize semaphore for signaling

        auto* exec_ctx = new ExecutionContext{
            this,
            std::move(payload),
            0,
            &semaphore
        };

        PerpetualWorker worker = {};
        worker.setHandler(sync_handler);
        worker.setPayload(exec_ctx);
        m_ctx->addWorker(worker);
        m_ctx->setWorkPending(worker);
        sem_acquire_blocking(&semaphore);

        // Get result and cleanup
        uint32_t result = exec_ctx->result;
        delete exec_ctx;
        m_ctx->removeWorker(worker);
        recursive_mutex_exit(&m_execution_mutex);  // Release the serialisation lock.
        return result;
    }

    /**
     * @brief Handler for PerpetualWorker, called in the correct async context.
     *
     * This function executes the user operation by calling doExecute() on the bridge, then signals completion
     * by releasing the stack-allocated semaphore in the ExecutionContext. Used for synchronous bridging.
     *
     * @param context Pointer to the async context (unused)
     * @param worker  Pointer to the async_when_pending_worker_t containing the ExecutionContext
     */
    extern "C" void sync_handler(async_context_t *context,
                      async_when_pending_worker_t *worker) {
        (void) context; // Unused in this implementation
        auto *exec_ctx = static_cast<ExecutionContext *>(worker->user_data);

        // Execute the work
        exec_ctx->result = exec_ctx->bridge->doExecute(std::move(exec_ctx->payload));

        // Signal completion using the original semaphore
        sem_release(exec_ctx->semaphore);
    }

} // namespace AsyncTcp
