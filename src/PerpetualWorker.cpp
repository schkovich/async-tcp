//
// Created by goran on 02/03/25.
//

#include "PerpetualWorker.hpp"

namespace async_tcp {

    PerpetualWorker::PerpetualWorker() : m_worker{} {}

    async_when_pending_worker_t *PerpetualWorker::getWorker() {
        return &m_worker;
    }

    void PerpetualWorker::setHandler(const handler_function_t handler_function) {
        m_worker.do_work = handler_function;
    }

    void PerpetualWorker::setPayload(void *data) { m_worker.user_data = data; }

} // namespace async_tcp