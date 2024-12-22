// OnConnectedCallbackHandler.cpp
#include "OnConnectedCallbackHandler.h"
#include "WorkerData.hpp"
#include <string>

namespace AsyncTcp {

void OnConnectedCallbackHandler::handleEvent() {
    auto data = std::make_unique<WorkerData>(_client);
    data->message = std::make_shared<std::string>("Connected!");
    _worker->setWorkerData(std::move(data));
    _ctx->setWorkPending(*_worker);
    DEBUGV("OnConnectedCallbackHandler::handleEvent: set work pending");
}

} // namespace AsyncTcp