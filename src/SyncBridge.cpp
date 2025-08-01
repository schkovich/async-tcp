/**
 * @file SyncBridge.cpp
 * @brief Implementation of SyncBridge for thread-safe, context-aware resource
 * access
 *
 * This file implements the SyncBridge pattern, providing a mechanism for safely
 * executing operations on shared resources across different execution contexts
 * (e.g., multiple threads or cores).
 *
 * The implementation uses heap-allocated workers and semaphores for each
 * synchronous operation, ensuring reentrancy and thread safety. All
 * synchronization is per-call and per-instance.
 *
 * @author goran
 * @date 2025-02-21
 */

#include "SyncBridge.hpp"
#include "pico/critical_section.h"

namespace async_tcp {

    SyncBridge::SyncBridge(const ContextManagerPtr &ctx) : m_ctx(ctx) {
        recursive_mutex_init(&m_execution_mutex);
    }

    uint32_t SyncBridge::doExecute(SyncPayloadPtr payload) { // NOLINT
        return onExecute(std::move(payload));
    }

    uint32_t SyncBridge::execute(SyncPayloadPtr payload) {
        lockBridge();
        auto semaphore = getSemaphore();
        auto exec_ctx = getExecutionContext(std::move(payload), semaphore);
        auto worker = getWorker(exec_ctx.get());
        auto result = executeWork(std::move(worker), std::move(exec_ctx),
                                  std::move(semaphore));
        unlockBridge();
        return result;
    }

    inline void SyncBridge::lockBridge() {
        recursive_mutex_enter_blocking(&m_execution_mutex);
    }

    inline void SyncBridge::unlockBridge() {
        recursive_mutex_exit(&m_execution_mutex);
    }

    inline std::unique_ptr<semaphore_t> SyncBridge::getSemaphore() {
        auto semaphore = std::make_unique<semaphore_t>();
        sem_init(semaphore.get(), 0, 1);
        return std::move(semaphore);
    }

    inline std::unique_ptr<SyncBridge::ExecutionContext>
    SyncBridge::getExecutionContext(SyncPayloadPtr payload,
                                    std::unique_ptr<semaphore_t> &semaphore) {
        return std::make_unique<ExecutionContext>(
            ExecutionContext{this, std::move(payload), 0, semaphore.get()});
    }

    inline std::unique_ptr<PerpetualWorker>
    SyncBridge::getWorker(ExecutionContext *exec_ctx) {
        auto worker = std::make_unique<PerpetualWorker>();
        worker->setHandler(sync_handler);
        worker->setPayload(exec_ctx);
        return worker;
    }

    inline uint32_t
    SyncBridge::executeWork(std::unique_ptr<PerpetualWorker> worker,
                            std::unique_ptr<ExecutionContext> exec_ctx,
                            std::unique_ptr<semaphore_t> semaphore) {
        critical_section_t crit_sec = {nullptr};

        critical_section_init(&crit_sec);

        critical_section_enter_blocking(&crit_sec);
        m_ctx->addWorker(*worker);
        m_ctx->setWorkPending(*worker);
        critical_section_exit(&crit_sec);

        sem_acquire_blocking(semaphore.get());

        critical_section_enter_blocking(&crit_sec);
        m_ctx->removeWorker(*worker);
        critical_section_exit(&crit_sec);

        critical_section_deinit(&crit_sec);

        return exec_ctx->result;
    }

    extern "C" void sync_handler(async_context_t *context,
                                 async_when_pending_worker_t *worker) {
        (void)context;
        auto *exec_ctx =
            static_cast<SyncBridge::ExecutionContext *>(worker->user_data);

        exec_ctx->result =
            exec_ctx->bridge->doExecute(std::move(exec_ctx->payload));

        sem_release(exec_ctx->semaphore);
    }
} // namespace async_tcp
