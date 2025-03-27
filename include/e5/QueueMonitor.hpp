/**
 * @file QueueMonitor.hpp
 * @brief Monitors and visualizes async context queue size using LEDs
 *
 * This class provides real-time visual feedback on the async context queue size
 * using the LedDebugger. It tracks current and maximum queue sizes and displays
 * them using LED patterns.
 */

#pragma once

#include <atomic>
#include "e5/LedDebugger.hpp"
#include "pico/time.h"

namespace e5 {

    /**
     * @class QueueMonitor
     * @brief Monitors and visualizes async context queue size
     *
     * This class tracks the size of the async context queue and provides
     * visual feedback using the LedDebugger. It maintains current and maximum
     * queue sizes and can display different patterns based on queue load.
     */
    class QueueMonitor
    {
    public:
        /**
         * @brief Constructs a QueueMonitor instance
         *
         * @param warning_threshold Queue size that triggers a warning indication
         * @param critical_threshold Queue size that triggers a critical indication
         */
        explicit QueueMonitor(const int warning_threshold = 5, const int critical_threshold = 10) :
            m_warning_threshold(warning_threshold), m_critical_threshold(critical_threshold), m_queue_size(0),
            m_max_queue_size(0) {}

        /**
         * @brief Updates the queue size based on worker counts
         *
         * This method sets the queue size directly based on the provided counts
         * and updates the maximum queue size if needed.
         *
         * @param atTimeCount Number of workers in the at_time_list
         * @param whenPendingCount Number of workers in the when_pending_list
         * @return The new total queue size
         */
        int updateQueueSize(const uint32_t atTimeCount, const uint32_t whenPendingCount) {
            const int new_size = static_cast<int>(atTimeCount + whenPendingCount);
            m_queue_size.store(new_size);

            if (new_size > m_max_queue_size) {
                m_max_queue_size.store(new_size);
            }

            return new_size;
        }

        /**
         * @brief Gets the current queue size
         *
         * @return The current queue size
         */
        [[nodiscard]] int getQueueSize() const { return m_queue_size.load(); }

        /**
         * @brief Gets the maximum queue size observed
         *
         * @return The maximum queue size
         */
        [[nodiscard]] int getMaxQueueSize() const { return m_max_queue_size.load(); }

        /**
         * @brief Resets the maximum queue size counter
         */
        void resetMaxQueueSize() { m_max_queue_size.store(0); }

        /**
         * @brief Updates the LED status based on queue utilization percentage
         *
         * This method updates the LED pattern to reflect the current queue utilization
         * as a percentage of the maximum observed queue size, in 5% increments.
         * The path LEDs indicate the absolute state (normal/warning/critical).
         *
         * @param current_size Optional parameter to specify the current size.
         *                     If not provided, the stored queue size will be used.
         */
        void updateLedStatus(const int current_size = -1) const {
            return;
            // Use provided size or get from stored value
            const int current = (current_size >= 0) ? current_size : m_queue_size.load();
            const int max = m_max_queue_size.load();

            // Set path LEDs based on absolute queue size
            if (current >= m_critical_threshold) {
                // Critical state: Right path LED on
                LedDebugger::setState(LedDebugger::NONE_oRooo);
            }
            else if (current >= m_warning_threshold) {
                // Warning state: Left path LED on
                LedDebugger::setState(LedDebugger::NONE_ooYoo);
            }
            else {
                // Normal state: Both path LEDs on
                LedDebugger::setState(LedDebugger::NONE_oooGo);
            }

            // Calculate utilization percentage (avoid division by zero)
            const int percentage = (max > 0) ? (current * 100 / max) : 0;

            // Map percentage to LED pattern in 5% increments
            // Define patterns for 0%, 5%, 10%, ..., 95%, 100%
            static constexpr LedDebugger::CombinedState percentagePatterns[] = {
                LedDebugger::NONE_ooooo, // 0%
                LedDebugger::NONE_ooooL, // 5%
                LedDebugger::NONE_oooGo, // 10%
                LedDebugger::NONE_oooGL, // 15%
                LedDebugger::NONE_ooYoo, // 20%
                LedDebugger::NONE_ooYoL, // 25%
                LedDebugger::NONE_ooYGo, // 30%
                LedDebugger::NONE_ooYGL, // 35%
                LedDebugger::NONE_oRooo, // 40%
                LedDebugger::NONE_oRooL, // 45%
                LedDebugger::NONE_oRoGo, // 50%
                LedDebugger::NONE_oRoGL, // 55%
                LedDebugger::NONE_oRYoo, // 60%
                LedDebugger::NONE_oRYoL, // 65%
                LedDebugger::NONE_oRYGo, // 70%
                LedDebugger::NONE_oRYGL, // 75%
                LedDebugger::NONE_Boooo, // 80%
                LedDebugger::NONE_BoooL, // 85%
                LedDebugger::NONE_BooGo, // 90%
                LedDebugger::NONE_BooGL, // 95%
                LedDebugger::NONE_BRYGL // 100%
            };

            // Round to nearest 5% and get the corresponding pattern
            int index = (percentage + 2) / 5; // Round to nearest 5%
            if (index > 20)
                index = 20; // Cap at 100%

            // Set the LED pattern
            LedDebugger::setState(percentagePatterns[index]);
        }

        /**
         * @brief Gets a formatted string with queue statistics
         *
         * This method returns a formatted string with the current and maximum queue sizes
         * for debugging and monitoring purposes.
         *
         * @param buffer Buffer to store the formatted string
         * @param buffer_size Size of the buffer
         * @return Number of characters written to the buffer (excluding null terminator)
         */
        int getQueueStatsString(char* buffer, const size_t buffer_size) const {
            if (!enabled) {
                return 0;
            }
            const int current = m_queue_size.load();
            const int max = m_max_queue_size.load();

            return snprintf(buffer, buffer_size, "Queue stats - Current: %d, Max: %d\n", current, max);
        }

        /**
         * @brief Gets a detailed formatted string with queue statistics
         *
         * This method returns a formatted string with the current, maximum, and detailed
         * worker counts for debugging and monitoring purposes.
         *
         * @param buffer Buffer to store the formatted string
         * @param buffer_size Size of the buffer
         * @param atTimeCount Number of workers in the at_time_list
         * @param whenPendingCount Number of workers in the when_pending_list
         * @return Number of characters written to the buffer (excluding null terminator)
         */
        int getDetailedStatsString(char* buffer, const size_t buffer_size, const uint32_t atTimeCount,
                                   const uint32_t whenPendingCount) const {
            if (!enabled) {
                return 0;
            }
            const int current = m_queue_size.load();
            const int max = m_max_queue_size.load();

            return snprintf(buffer, buffer_size, "Queue stats - Current: %d (at_time: %lu, when_pending: %lu), Max: %d",
                            current, atTimeCount, whenPendingCount, max);
        }

        /**
         * @brief Checks if it's time to take a new sample
         *
         * @return true if it's time to take a new sample, false otherwise
         */
        bool shouldSample() {
            if (enabled) {
                absolute_time_t current_time = get_absolute_time();
                // Use the Pico SDK function to compare timestamps in microseconds
                if (absolute_time_diff_us(m_last_sample_time, current_time) >= m_sampling_interval_us) {
                    m_last_sample_time = current_time;
                    return true;
                }
                return false;
            }
            return false;
        }

        void enable() { enabled = true; } /**< Enables queue monitoring */

    private:
        absolute_time_t m_last_sample_time = 0; /**< Last time a sample was taken */
        absolute_time_t m_sampling_interval_us = 1000000; /**< Sampling interval in microseconds */
        int m_warning_threshold; /**< Queue size that triggers a warning indication */
        int m_critical_threshold; /**< Queue size that triggers a critical indication */
        std::atomic<int> m_queue_size; /**< Current queue size */
        std::atomic<int> m_max_queue_size; /**< Maximum queue size observed */
        bool enabled = false; /**< Flag to disable queue monitoring */
    };

} // namespace e5
