/**
 * @file WorkerCounter.hpp
 * @brief Utility class for counting workers in an async context
 *
 * This class provides methods to count the number of workers in the async context's
 * at_time_list and when_pending_list. It uses direct traversal of the linked lists
 * to provide accurate counts without the need for manual tracking.
 */

#pragma once

#include <utility>

#include "pico/async_context.h"

namespace e5 {

    class WorkerCounter
    {
    public:
        /**
         * @brief Count the number of workers in the at_time_list
         *
         * @param context The async context to inspect
         * @return uint32_t The number of workers in the at_time_list
         */
        static uint32_t countAtTimeWorkers(const async_context_t* context) {
            if (!context)
                return 0;

            uint32_t count = 0;
            for (const async_at_time_worker_t* worker = context->at_time_list; worker; worker = worker->next) {
                count++;
            }

            return count;
        }

        /**
         * @brief Count the number of workers in the when_pending_list
         *
         * @param context The async context to inspect
         * @return uint32_t The number of workers in the when_pending_list
         */
        static uint32_t countWhenPendingWorkers(const async_context_t* context) {
            if (!context)
                return 0;

            uint32_t count = 0;
            for (const async_when_pending_worker_t* worker = context->when_pending_list; worker;
                 worker = worker->next) {
                count++;
            }

            return count;
        }

        /**
         * @brief Get a snapshot of worker counts from the context
         *
         * @param context The async context to inspect
         * @return std::pair<uint32_t, uint32_t> Pair of (at_time_count, when_pending_count)
         */
        static std::pair<uint32_t, uint32_t> getWorkerCounts(const async_context_t* context) {
            return {countAtTimeWorkers(context), countWhenPendingWorkers(context)};
        }
    };

} // namespace e5
