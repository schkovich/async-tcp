/**
 * @file TcpWriter.cpp
 * @brief Implementation of stateful asynchronous TCP writer with chunking
 *
 * This file implements the TcpWriter class which provides asynchronous
 * TCP write capabilities with proper state management for multi-chunk operations.
 */

#include "TcpWriter.hpp"
#include "TcpWriteHandler.hpp"
#include "pico/time.h"
#include <cstring>

namespace async_tcp {

    TcpWriter::TcpWriter(const ContextManagerPtr& ctx, TcpClient& io)
        : m_ctx(ctx), m_io(io), m_data(nullptr), m_written(0),
          m_total_size(0), m_write_in_progress(false), m_write_start_time(nil_time) {
    }

    void TcpWriter::write(const uint8_t* data, size_t size) {
        // In an embedded environment, these conditions should never happen —
        // assert instead of graceful handling.
        assert(data && "Data pointer must be valid");
        assert(size > 0 && "Write size must be non-zero");

        // Copy the data to our internal buffer
        m_data = std::make_unique<uint8_t[]>(size);
        std::memcpy(m_data.get(), data, size);

        m_written = 0;
        m_total_size = size;
        m_write_start_time = get_absolute_time();  // Record start time for timeout tracking
        m_write_in_progress.store(true);

        DEBUGWIRE("[TcpWriter] Starting write operation: %zu bytes total\n", m_total_size);

        // Send the first chunk
        sendNextChunk();
    }

    void TcpWriter::sendNextChunk() {
        if (!m_data) {
            DEBUGWIRE("[TcpWriter] sendNextChunk called but no write in progress\n");
            return;
        }

        // Calculate chunk size (remaining data to write)
        const auto chunk_size = m_total_size - m_written;
        const auto chunk_data = m_data.get() + m_written;

        DEBUGWIRE("[TcpWriter] Sending chunk: %zu bytes at offset %zu\n",
               chunk_size, m_written);

        // Create an ephemeral handler for this chunk - let TcpClientContext handle flags
        TcpWriteHandler::create(m_ctx, chunk_data, chunk_size, m_io);
    }

    void TcpWriter::onAckReceived(uint16_t ack_len) {
        // ACKs should only arrive when a write is in progress
        assert(isWriteInProgress() && "Received ACK but no write in progress - protocol violation");

        // Update progress based on ACKed bytes
        m_written += ack_len;

        // This should never happen - catch protocol violations immediately
        assert(m_written <= m_total_size && "Received ACKs for more bytes than sent - protocol violation");

        DEBUGWIRE("[TcpWriter] ACK received: %u bytes, total written: %zu/%zu\n",
               ack_len, m_written, m_total_size);

        // Check if write operation is complete
        if (m_written == m_total_size) {
            DEBUGWIRE("[TcpWriter] All data ACKed, completing write operation\n");
            completeWrite();
            return;
        }

        // Continue with next chunk if more data needs to be sent
        sendNextChunk();
    }

    void TcpWriter::completeWrite() {
        DEBUGWIRE("[TcpWriter] Write operation completed successfully\n");

        // Reset state for next write
        m_data.reset();
        m_written = 0;
        m_total_size = 0;
        m_write_start_time = nil_time;
        m_write_in_progress.store(false);
    }

    bool TcpWriter::hasTimedOut() const {
        if (!isWriteInProgress()) {
            return false;
        }

        const uint64_t elapsed_us = absolute_time_diff_us(m_write_start_time, get_absolute_time());
        return elapsed_us >= WRITE_TIMEOUT_US;
    }

    void TcpWriter::onWriteTimeout() {
        const uint64_t elapsed_us = absolute_time_diff_us(m_write_start_time, get_absolute_time());

        DEBUGWIRE("[TcpWriter] Write operation timed out after %llu us, written: %zu/%zu bytes\n",
               elapsed_us, m_written, m_total_size);

        // Clean up the timed-out write operation
        completeWrite();
    }

} // namespace async_tcp
