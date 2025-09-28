#pragma once

#include "platform/pico/types.hpp"

namespace async_bridge {

class PerpetualWorker;
class EphemeralWorker;

using HandlerFunction = uint32_t (*)(void *param);

/**
 * Minimal abstract async context interface used by bridge base classes.
 *
 * This header intentionally avoids any platform (Pico) includes and
 * exposes only the tiny surface needed by consumers of the async-bridge API.
 */
class IAsyncContext {
public:
    virtual bool addWorker(PerpetualWorker &worker) = 0;
    virtual bool addWorker(EphemeralWorker &worker, std::uint32_t delay) = 0;
    virtual bool removeWorker(PerpetualWorker &worker) = 0;
    virtual bool removeWorker(EphemeralWorker &worker) = 0;
    virtual void setWorkPending(PerpetualWorker &worker) = 0;

    virtual void acquireLock() = 0;
    virtual void releaseLock() = 0;

    virtual uint32_t execWorkSynchronously(const handler_function_t &handler, void *param) = 0;

    [[nodiscard]] virtual uint8_t getCore() const = 0;

    /**
     * Wait until the provided time. Type is intentionally opaque here to avoid
     * pulling in platform headers in the public API.
     */
    virtual void waitUntil(std::int64_t until) = 0;

    virtual void checkLock() const = 0;

    virtual ~IAsyncContext() = default;
};

using AsyncCtx = IAsyncContext; // compatibility alias used across the codebase

} // namespace async_tcp
