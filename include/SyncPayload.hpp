/**
 * @file SyncPayload.hpp
 */
#pragma once

#include <cstdint>


#include "ContextPayload.hpp"

namespace AsyncTcp {

    extern "C" {
        typedef uint32_t (*HandlerFunction)(void* param);
    }

    class SyncPayload : public ContextPayload
    {
    public:
        explicit SyncPayload(const HandlerFunction handler) : m_function(handler) {}

        [[nodiscard]] virtual HandlerFunction getHandler() const;
    protected:
        HandlerFunction m_function;
    };

}
