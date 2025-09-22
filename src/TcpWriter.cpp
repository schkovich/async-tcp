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
#include <cstring>

namespace async_tcp {

    // --- Pure C bridge ---
    err_t lwip_sent_cb(void *arg, tcp_pcb *tpcb,
                       u16_t len) { // NOLINT len canot be constant
        auto *tx = static_cast<TcpClientContext *>(arg)->getTxWriter();
        assert(tx && "IoTxWriter must exist when ACK callback is invoked - "
                     "setup error!");
        // ReSharper disable once CppDFAUnreachableCode
        tx->onAckCallback(tpcb, len);
        return ERR_OK;
    }

    TcpWriter::TcpWriter(tcp_pcb *pcb) : m_pcb(pcb) {}

    std::size_t TcpWriter::availableForWrite() const {
        return m_pcb ? tcp_sndbuf(m_pcb) : 0;
    }

    std::size_t TcpWriter::writeData(const uint8_t *data,
                                     const std::size_t size) {
        if (!m_pcb || !data || size == 0) {
            return 0; // nothing to do / invalid state
        }

        std::size_t total_queued = 0;

        while (total_queued < size) {
            const std::size_t remaining = size - total_queued;
            const std::size_t chunk_size = getOptimalChunkSize(remaining);
            if (chunk_size == 0) {
                DEBUGWIRE(
                    "[TcpWriter] Send buffer full (queued=%zu) - rejected\n",
                    total_queued);
                total_queued = 0;
                break;
            }

            // Set TCP_WRITE_FLAG_MORE only if we know we will write more
            // afterwards.
            const u8_t flags =
                (total_queued + chunk_size < size) ? TCP_WRITE_FLAG_MORE : 0;

            const err_t err =
                tcp_write(m_pcb, data + total_queued, chunk_size, flags);
            if (err != ERR_OK) {
                DEBUGWIRE("[TcpWriter] tcp_write error %d\n",
                          static_cast<int>(err));
                total_queued = 0;
                break;
            }

            total_queued += chunk_size;
        }

        // Flush immediately – Nagle is disabled, so this forces the packet out.
        if (total_queued > 0) {
            tcp_output(m_pcb);
        }

        return total_queued;
    }

    void TcpWriter::onAckCallback(tcp_pcb *pcb, uint16_t len) { /* no-op */ }

    void TcpWriter::onError(const err_t error) {
        DEBUGWIRE("[TcpWriter] Error %d -> reset\n", error);
        // completeWrite();
    }

} // namespace async_tcp
