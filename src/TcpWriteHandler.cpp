/**
 * @file TcpWriteHandler.cpp
 * @brief Implementation of the handler for TCP write operations with proper serialization.
 *
 * This file implements the TcpWriteHandler class which handles TCP write
 * operations in a thread-safe manner. The handler is ephemeral and handles
 * a single chunk write operation, then self-destructs.
 *
 * @author Goran
 * @date 2025-02-20
 * @ingroup AsyncTCPClient
 */

#include "TcpWriteHandler.hpp"

namespace async_tcp {

    TcpWriteHandler::TcpWriteHandler(const AsyncCtx& ctx,
                                     const uint8_t* data,
                                     const size_t size,
                                     TcpClient& io)
            : EphemeralBridge(ctx), m_io(io), m_data(data), m_size(size) {
    }

    void TcpWriteHandler::onWork() {
        if (!m_data || m_size == 0) {
            // No valid data to write - let the cleanup mechanism handle destruction.
            return;
        }

        m_io.writeChunk(m_data, m_size);
    }

} // namespace async_tcp
