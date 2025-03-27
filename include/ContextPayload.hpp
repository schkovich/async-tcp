/**
* @file ContextPayload.hpp
 */

#pragma once
#include <memory>

namespace AsyncTcp {
    class ContextPayload {
    public:
        typedef struct
        {
        } Params;

        virtual ~ContextPayload() = default;
        virtual const Params* getParams() = 0;
    };

}