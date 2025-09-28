// SPDX-License-Identifier: MPL-2.0
#pragma once

#include "IAsyncContext.hpp"

#include <memory>

namespace async_bridge {
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

class SyncBridge {

public:
    explicit SyncBridge(const IAsyncContext &ctx) : m_ctx(ctx) {}
    virtual ~SyncBridge() = default;

    // Derived classes implement this to run payloads on the context core
    virtual uint32_t onExecute(std::unique_ptr<SyncPayload> payload) = 0;

    // Execute synchronously on the context; blocks until completion
    uint32_t execute(std::unique_ptr<SyncPayload> payload);

protected:
    const IAsyncContext &m_ctx;
};

} // namespace async_bridge

