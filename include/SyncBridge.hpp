#pragma once

#include <memory>
#include "ContextManager.hpp"

namespace AsyncTcp {

    /**
     * @struct SyncPayload
     * @brief Base type for synchronous work payloads
     *
     * Derived payload types must be final to prevent slicing
     */
    struct SyncPayload {
        SyncPayload() noexcept = default;
        virtual ~SyncPayload() noexcept = default;
    };

    extern "C" uint32_t executor_bridging_function(void* instance);

    /**
     * @class SyncBridge
     * @brief Base class that provides a uniform way to execute synchronous tasks with an async context.
     *
     * Inspired by the Worker class and bridging_function model, but tailored for synchronous
     * operations. In principle, onExecute() is the specialized logic—like onWork()—but runs
     * synchronously through an async_context.
     */
    class SyncBridge {
        const ContextManagerPtr& m_ctx;
        std::unique_ptr<SyncPayload> m_executor_payload = nullptr; /**< Data associated with this executor instance. */

        /**
         * @brief Override this in derived classes to implement synchronous logic.
         */
        virtual uint32_t onExecute(std::unique_ptr<SyncPayload> payload) = 0;

    protected:

        /**
         * @brief Called by the bridging function to perform the actual execution task.
         *        Derived classes implement onExecute() with domain-specific logic.
         *
         * @return A result code. Could be extended for more robust returns.
         */
        uint32_t doExecute();

        void lockBridge() const;

        void unlockBridge() const;
        void pause(absolute_time_t until) const;

        friend  uint32_t executor_bridging_function(void* instance);

    public:
        explicit SyncBridge(const ContextManagerPtr& ctx) : m_ctx(ctx) {}
        virtual ~SyncBridge() = default;
        virtual uint32_t execute(std::unique_ptr<SyncPayload> payload);
    };

    using SyncPayloadPtr = std::unique_ptr<SyncPayload>;

}