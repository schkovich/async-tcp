// WorkerData.hpp

#pragma once
#include <memory>

namespace AsyncTcp {
    // Forward declaration
    class AsyncTcpClient;
    /**
     * @struct WorkerData
     * @brief Contains data necessary for a worker's processing within the asynchronous TCP client context.
     *
     * `WorkerData` holds essential information, such as the size of incoming data
     * and a reference to the associated TCP client, enabling worker functions to handle
     * and process network events effectively.
     */
    struct WorkerData
    {
        virtual ~WorkerData() = default;

        explicit WorkerData(AsyncTcpClient& client_ref) : client(client_ref) {}
        std::unique_ptr<int> read_size; /**< Pointer to the size of data available for reading. */
        AsyncTcpClient& client; /**< Raw pointer to the associated `AsyncTcpClient` instance. */
        std::shared_ptr<std::string> message; /**< Shared pointer to message content for print operations on Core 1. */

        // Additional fields can be added here as needed to support more complex data handling.
    };

} // namespace AsyncTcp
