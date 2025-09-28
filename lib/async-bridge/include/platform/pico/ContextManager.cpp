/**
 * @file ContextManager.cpp
 * @brief Implements asynchronous context management and worker scheduling for
 * the AsyncTCP library.
 *
 * This file defines the ContextManager class methods, which manage the
 * asynchronous execution context, worker scheduling, thread-safe lock handling,
 * and synchronous execution across cores.
 *
 * The ContextManager provides several critical capabilities:
 *   - Safe cross-core execution through the execWorkSynchronously mechanism
 *   - Worker lifecycle management (adding, removing, signaling)
 *   - Lock-based thread safety for atomic operations
 *   - Resource management with explicit initialization and cleanup
 *
 * Most methods verify the context's validity before performing operations,
 * making the class robust against improper usage sequences (e.g., using before
 * initialization).
 *
 * @note The execWorkSynchronously method is a cornerstone of the thread-safety
 * patterns used throughout the library, as it guarantees that operations
 * execute in the proper context.
 */

#include "ContextManager.hpp"
#include "../../PerpetualWorker.hpp"
#include "../../EphemeralWorker.hpp"
#include <Arduino.h>

namespace async_bridge {

    ContextManager::ContextManager() : m_context() {
        m_context_core = &m_context.core; // Set the core context reference
    }

    ContextManager::~ContextManager() {
        if (initiated) {
            async_context_deinit(m_context_core);
            m_context_core = nullptr;
            initiated = false;
        }
    }

    bool ContextManager::initDefaultContext(
        async_context_threadsafe_background_config_t &config) {
        if (!initiated) {
            if (async_context_threadsafe_background_init(&m_context, &config)) {
                initiated = true;
                return true;
            }
            return false;
        }
        return true;
    }

    bool ContextManager::addWorker(PerpetualWorker& worker) const {
        if (!initiated) {
            return false;
        }
        bool added = false;

        if (get_core_num() != m_context_core->core_num) {
            critical_section_t crit_sec = {nullptr};
            critical_section_init(&crit_sec);
            critical_section_enter_blocking(&crit_sec);
            added = async_context_add_when_pending_worker(m_context_core,
                                                          worker.getWorker());
            critical_section_exit(&crit_sec);
            critical_section_deinit(&crit_sec);
        } else {
            added = async_context_add_when_pending_worker(m_context_core,
                                                          worker.getWorker());
        }

        if (!added) {
            DEBUGV("ContextManager::addWorker - Failed to add worker!\n");
            return false;
        }

        return true;
    }

    bool ContextManager::addWorker(EphemeralWorker& worker,
                                   const uint32_t delay) const {
        if (!initiated) {
            DEBUGV("ContextManager::addWorker - no context!\n");
            return false;
        }
        const auto worker_ptr = worker.getWorker();
        if (worker_ptr->do_work == nullptr) {
            DEBUGV(
                "ContextManager::addWorker - handler function not defined!\n");
            return false;
        }
        if (worker_ptr->user_data == nullptr) {
            DEBUGV("ContextManager::addWorker - no user data set!\n");
            return false;
        }

        bool added = false;

        if (get_core_num() != m_context_core->core_num) {
            critical_section_t crit_sec = {nullptr};
            critical_section_init(&crit_sec);
            critical_section_enter_blocking(&crit_sec);
            added = async_context_add_at_time_worker_in_ms(m_context_core,
                                                           worker_ptr, delay);
            critical_section_exit(&crit_sec);
            critical_section_deinit(&crit_sec);
        } else {
            added = async_context_add_at_time_worker_in_ms(m_context_core,
                                                           worker_ptr, delay);
        }

        if (!added) {
            DEBUGV("ContextManager::addWorker - Failed to add worker!\n");
            return false;
        }

        return true;
    }

    bool ContextManager::removeWorker(PerpetualWorker& worker) const {
        if (!initiated) {
            DEBUGV("ContextManager::removeWorker - no context!\n");
            return false;
        }
        bool removed = false;

        if (get_core_num() != m_context_core->core_num) {
            critical_section_t crit_sec = {nullptr};
            critical_section_init(&crit_sec);
            critical_section_enter_blocking(&crit_sec);
            removed = async_context_remove_when_pending_worker(
                m_context_core, worker.getWorker());
            critical_section_exit(&crit_sec);
            critical_section_deinit(&crit_sec);
        } else {
            removed = async_context_remove_when_pending_worker(
                m_context_core, worker.getWorker());
        }

        if (!removed) {
            DEBUGV("ContextManager::removeWorker - Failed to remove when "
                   "pending worker!\n");
            return false;
        }
        return true;
    }

    bool ContextManager::removeWorker(EphemeralWorker& worker) const {
        if (!initiated) {
            return false;
        }
        if (!async_context_remove_at_time_worker(m_context_core,
                                                 worker.getWorker())) {
            DEBUGV("ContextManager::removeWorker(Worker &worker) - Failed to "
                   "remove at time worker!\n");
            return false;
        }
        return true;
    }

    void ContextManager::setWorkPending(PerpetualWorker& worker) const {
        if (initiated) {
            async_context_set_work_pending(m_context_core, worker.getWorker());
        }
    }

    void ContextManager::acquireLock() const {
        if (initiated) {
            async_context_acquire_lock_blocking(m_context_core);
        }
    }

    void ContextManager::releaseLock() const {
        if (initiated) {
            async_context_release_lock(m_context_core);
        }
    }

    uint32_t
    ContextManager::execWorkSynchronously(const HandlerFunction &handler,
                                          void *param) const {
        return async_context_execute_sync(m_context_core, handler, param);
    }

    uint8_t ContextManager::getCore() const { return m_context_core->core_num; }

    void ContextManager::checkLock() const {
        async_context_lock_check(m_context_core);
    }

    void ContextManager::waitUntil(const absolute_time_t until) const {
        async_context_wait_until(m_context_core, until);
    }
} // namespace async_bridge
