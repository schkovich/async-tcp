/**
 * @file SyncBridge.cpp
 * @brief Implementation of the SyncBridge pattern for thread-safe resource
 * access
 *
 * The SyncBridge pattern provides a mechanism to safely access and modify
 * resources across different execution contexts. This implementation handles
 * the core bridging functionality that enables thread-safe operations through
 * context-managed execution.
 *
 * @author goran
 * @date 2025-02-21
 */

#include "SyncBridge.hpp"
#include <atomic>

namespace async_tcp {

std::atomic<bool> SyncBridge::executing_flag{false};

    /**
     * @brief Internal execution method that delegates to the derived class
     * implementation
     *
     * This method is called by the bridging function after the operation has
     * been properly scheduled in the correct execution context.
     *
     * @param payload Unique pointer to operation data being passed to the
     * handler
     * @return uint32_t Result code from the operation execution
     */
    uint32_t
    SyncBridge::doExecute(SyncPayloadPtr payload) { // NOLINT
        return onExecute(std::move(payload));
    }

    /**
     * @brief Thread-safe execution method that schedules work in the proper
     * context
     *
     * This method packages the bridge instance with its payload and schedules
     * the operation to be executed synchronously in the appropriate context.
     * It blocks until execution completes, ensuring thread safety for resource
     * operations.
     *
     * @param payload Unique pointer to operation data
     * @return uint32_t Result code from the operation execution
     */
    uint32_t SyncBridge::execute(SyncPayloadPtr payload) {
        bool expected = false;
        // Use class static member for re-entrancy guard
        if (!executing_flag.compare_exchange_strong(expected, true, std::memory_order_acquire)) {
            // Already executing, return error or handle contention
            return PICO_ERROR_RESOURCE_IN_USE;
        }
        recursive_mutex_enter_blocking(&m_execution_mutex);

        auto* exec_ctx = new ExecutionContext{
            this,
            std::move(payload),
            0
        };

        m_worker.setPayload(exec_ctx);  // Pass ExecutionContext as user_data
        m_ctx->setWorkPending(m_worker);
        sem_acquire_blocking(&m_semaphore);  // Wait for original semaphore

        // Get result and cleanup
        uint32_t result = exec_ctx->result;
        delete exec_ctx;

        recursive_mutex_exit(&m_execution_mutex);  // Release serialization lock
        executing_flag.store(false, std::memory_order_release);
        return result;
    }

    extern "C" void sync_handler(async_context_t *context,
                      async_when_pending_worker_t *worker) {
        (void) context; // Unused in this implementation
        auto *exec_ctx = static_cast<ExecutionContext *>(worker->user_data);

        // Execute the work
        exec_ctx->result = exec_ctx->bridge->doExecute(std::move(exec_ctx->payload));

        // Signal completion using the original semaphore
        sem_release(&exec_ctx->bridge->m_semaphore);
    }

} // namespace AsyncTcp
