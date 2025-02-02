/**
 * @file SyncPayload.h
 */
#pragma once

#include "ContextPayload.h"

namespace AsyncTcp {

    extern "C" {
    typedef uint32_t (*HandlerFunction)(void* param);
    typedef struct
    {
    } Params;
    }

    class SyncPayload : public ContextPayload
    {
    protected:
        HandlerFunction m_function;
        Params* m_params;

    public:
        explicit SyncPayload(const HandlerFunction handler) : m_function(handler) {}

        virtual Params* getParams();
        [[nodiscard]] virtual HandlerFunction getHandler() const;
    };

} // namespace AsyncTcp
