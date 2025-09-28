// SPDX-License-Identifier: MPL-2.0
#pragma once

namespace async_bridge {
    class IAsyncContext;

    class EventBridge {
protected:
    explicit EventBridge(const IAsyncContext &ctx) : m_ctx(ctx) {}

    virtual void onWork() = 0;
    void doWork() { onWork(); }

    [[nodiscard]] const IAsyncContext &getContext() const { return m_ctx; }

public:
    virtual ~EventBridge() = default;

    /**
     * Initialise any runtime structures required by the bridge. Implementations
     * should register workers with the context here.
     */
    virtual void initialiseBridge() = 0;

protected:
    const IAsyncContext &m_ctx;
};

} // namespace async_bridge


