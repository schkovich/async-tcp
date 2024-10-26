// WorkerData.hpp

#pragma once
#include <memory>

namespace AsyncTcp {
    class AsyncTcpClient;  // Forward declaration

    struct WorkerData {
        std::unique_ptr<int> read_size;
        AsyncTcpClient* client = nullptr;  // Use raw pointer for client
        // Add other relevant fields if necessary
    };
}