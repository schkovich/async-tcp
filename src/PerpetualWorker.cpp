//
// Created by goran on 02/03/25.
//

#include "PerpetualWorker.hpp"

namespace AsyncTcp {

    PerpetualWorker::PerpetualWorker() : m_worker{} {}

    async_when_pending_worker_t* PerpetualWorker::getWorker() {
        return &m_worker;
    }
} // AsyncTcp