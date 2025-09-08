/**
 * @file TcpWriter.cpp
 * @brief Implementation of stateful asynchronous TCP writer with chunking
 *
 * This file implements the TcpWriter class, which provides asynchronous
 * TCP write capabilities with proper state management for multi-chunk
 * operations.
 */

#include "TcpWriter.hpp"

#include "TcpClient.hpp"
#include "TcpClientContext.hpp"
#include "TcpWriteHandler.hpp"
#include "pico/time.h"
#include <cstring>

namespace async_tcp {

    TcpWriter::TcpWriter(const AsyncCtx &ctx, TcpClient &io)
        : m_ctx(ctx), m_io(io), m_data(nullptr), m_acked(0), m_queued(0),
          m_total_size(0), m_write_in_progress(false),
          m_write_start_time(nil_time) {}

    void TcpWriter::write(const uint8_t *data, const size_t size) {
        assert(data && "Data pointer must be valid");
        assert(size > 0 && "Write size must be non-zero");
        // In normal usage TcpClient::write() performed tryStartWrite() and set
        // the flag. We assert instead of setting it again to catch accidental
        // direct/reentrant calls.
        assert(m_write_in_progress.load() &&
               "TcpWriter::write must be preceded by tryStartWrite() CAS in "
               "TcpClient::write()");

        // Copy the data to our internal buffer (ACK-based completion retains
        // until fully ACKed)
        m_data = std::make_unique<uint8_t[]>(size);
        std::memcpy(m_data.get(), data, size);

        m_acked = 0;
        m_queued = 0;
        m_total_size = size;
        m_write_start_time = get_absolute_time();
        m_last_progress_time = m_write_start_time; // initial progress timestamp

        DEBUGWIRE("[TcpWriter] Start: total=%zu bytes\n", m_total_size);

        sendNextChunk();
    }

    void TcpWriter::sendNextChunk() {
        assert(isWriteInProgress() && m_data &&
               "sendNextChunk with no write in progress");
        assert(m_queued >= m_acked && "Invariant broken: queued < acked");

        // Batch queue as many fragments as current send buffer allows to reduce
        // callback latency.
        while (true) {
            const size_t remaining = remainingUnqueued();
            if (remaining == 0) {
                break; // nothing left to queue; wait for ACKs (only relevant in
                       // Acked mode)
            }

            // Capture free space snapshot inside async context (safe)
            m_last_free_estimate = m_io._ts_availableForWrite();
            if (m_last_free_estimate == 0) {
                DEBUGWIRE("[TcpWriter] No send buffer space (queued=%zu "
                          "acked=%zu) - deferring\n",
                          m_queued, m_acked);
                break;
            }

            size_t chunk_size = remaining;
            if (chunk_size > m_last_free_estimate) {
                chunk_size = m_last_free_estimate; // partial fill
            }
            if (chunk_size == 0) { // NOLINT clang-analyzer-deadcode.DeadStores
                // defensive (should not happen if free > 0)
                break; // NOLINT clang-analyzer-deadcode.DeadStores
            }

            const uint8_t *chunk_data = m_data.get() + m_queued;
            DEBUGWIRE(
                "[TcpWriter] Queue chunk: off=%zu size=%zu remaining_after=%zu "
                "(acked=%zu queued_before=%zu free=%zu)\n",
                m_queued, chunk_size, remaining - chunk_size, m_acked, m_queued,
                m_last_free_estimate);

            m_queued += chunk_size;
            m_last_progress_time = get_absolute_time();
            DEBUGWIRE(
                "[TcpWriter] Sending chunk: %zu bytes at offset %zu "
                "(queued=%zu acked=%zu total=%zu mode=%s)\n",
                chunk_size, m_acked, m_queued, m_acked, m_total_size,
                (m_mode == CompletionMode::Enqueued ? "ENQUEUED" : "ACKED"));

            TcpWriteHandler::create(m_ctx, chunk_data, chunk_size, m_io);

            // Enqueue-complete policy: finish as soon as everything is queued.
            if (m_mode == CompletionMode::Enqueued &&
                m_queued == m_total_size) {
                DEBUGWIRE("[TcpWriter] All data queued (enqueue-complete mode) "
                          "-> completing write\n");
                completeWrite();
                break;
            }

            // If we queued less than we need and still have space, loop again;
            // otherwise break to wait for ACK freeing space. (Loop naturally
            // continues; break conditions above handle exit.)
        }
    }

    void TcpWriter::onAckReceived(const uint16_t ack_len) {
        if (!isWriteInProgress()) {
            DEBUGWIRE("[TcpWriter] Late ACK %u (ignored)\n", ack_len);
            return;
        }

        // Update progress based on ACKed bytes (always tracked regardless of
        // mode)
        m_acked += ack_len;
        m_last_progress_time = get_absolute_time();
        assert(m_acked <= m_total_size && "ACK overflow (acked > total)");
        DEBUGWIRE("[TcpWriter] ACK: +%u -> acked=%zu/%zu queued=%zu "
                  "in_flight=%zu mode=%s\n",
                  ack_len, m_acked, m_total_size, m_queued, inFlightBytes(),
                  (m_mode == CompletionMode::Enqueued ? "ENQUEUED" : "ACKED"));

        // ACK-based completion only if mode == Acked
        if (m_mode == CompletionMode::Acked && m_acked == m_total_size) {
            DEBUGWIRE("[TcpWriter] All data ACKed (ACK-complete mode) -> "
                      "completing write\n");
            completeWrite();
            return;
        }

        // Need to continue queueing remaining bytes (both modes) if not fully
        // queued yet
        if (remainingUnqueued() > 0) {
            sendNextChunk();
        }
    }

    void TcpWriter::onError(const err_t error) {
        DEBUGWIRE("[TcpWriter] Error %d -> reset\n", error);
        completeWrite();
    }

    void TcpWriter::completeWrite() {
        DEBUGWIRE("[TcpWriter] Complete (acked=%zu queued=%zu total=%zu)\n",
                  m_acked, m_queued, m_total_size);
        m_data.reset();
        m_acked = 0;
        m_queued = 0;
        m_total_size = 0;
        m_last_free_estimate = 0;
        m_write_start_time = nil_time;
        m_last_progress_time = nil_time;
        m_write_in_progress.store(false);
    }

    bool TcpWriter::hasTimedOut() const {
        if (!isWriteInProgress())
            return false;
        if (m_last_progress_time == nil_time)
            return false; // no active timing
        const uint64_t stall_us =
            absolute_time_diff_us(m_last_progress_time, get_absolute_time());
        return stall_us >= STALL_TIMEOUT_US;
    }

    void TcpWriter::onWriteTimeout() {
        if (!isWriteInProgress() || m_last_progress_time == nil_time)
            return;
        const uint64_t stall_us =
            absolute_time_diff_us(m_last_progress_time, get_absolute_time());
        if (stall_us < STALL_TIMEOUT_US)
            return; // false alarm (re-check)
        DEBUGWIRE("[TcpWriter] Stall timeout: no progress for %llu us "
                  "(acked=%zu queued=%zu total=%zu)\n",
                  stall_us, m_acked, m_queued, m_total_size);
        completeWrite();
    }

    // ---- Dynamic watermark helpers (use cached free estimate only) ----
    size_t TcpWriter::totalCapacityEstimate() const {
        // Use last captured free space + current in-flight (queued - acked)
        return m_last_free_estimate + inFlightBytes();
    }

    size_t TcpWriter::dynamicHighWatermarkBytes() const {
        const size_t cap = totalCapacityEstimate();
        if (cap == 0)
            return 0;                            // no context
        return (cap * HIGH_WATERMARK_PCT) / 100; // integer floor
    }

    size_t TcpWriter::dynamicLowWatermarkBytes() const {
        const size_t cap = totalCapacityEstimate();
        if (cap == 0)
            return 0;
        return (cap * LOW_WATERMARK_PCT) / 100;
    }

    bool TcpWriter::shouldBackpressure() const {
        const size_t in_flight = inFlightBytes();
        const size_t high = dynamicHighWatermarkBytes();
        if (high == 0)
            return false;
        return in_flight >= high;
    }

    bool TcpWriter::canReleaseBackpressure() const {
        const size_t in_flight = inFlightBytes();
        const size_t low = dynamicLowWatermarkBytes();
        if (low == 0)
            return true;
        return in_flight <= low;
    }

} // namespace async_tcp
