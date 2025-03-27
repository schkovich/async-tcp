/**
 * @file MessageBuffer.hpp
 * @brief Defines a memory-safe buffer for message handling in asynchronous operations.
 *
 * This file contains the MessageBuffer class which provides ownership semantics
 * for character data used in asynchronous operations. It ensures that message data
 * remains valid throughout the lifetime of asynchronous tasks by creating and
 * managing its own copy of the data.
 *
 * The class is particularly useful for scenarios where the original message data
 * might go out of scope before an asynchronous operation completes.
 *
 * @author Goran
 * @date 2025-02-19
 * @ingroup AsyncTCPClient
 */

#pragma once
#include <cstring>
#include <memory>

namespace e5 {

/**
 * @class MessageBuffer
 * @brief Provides ownership semantics for message data in asynchronous operations.
 *
 * This class creates and manages a copy of message data, ensuring that the data
 * remains valid throughout the lifetime of asynchronous tasks. It uses RAII principles
 * to handle memory management automatically.
 *
 * Key features:
 * - Creates a deep copy of the input message
 * - Automatically handles memory allocation and deallocation
 * - Provides access to the underlying data and its size
 * - Handles null input gracefully
 */
class MessageBuffer {
    size_t m_size;                   /**< Total size of the buffer including null terminator */
    std::unique_ptr<char[]> m_data{};  /**< Owned buffer containing the message data */

public:
    /**
     * @brief Constructs a MessageBuffer with a copy of the provided message.
     *
     * Creates a deep copy of the input message, including the null terminator.
     * If the input is null, the buffer will be empty.
     *
     * @param msg Null-terminated string to copy into the buffer
     */
    explicit MessageBuffer(const char* msg)
        : m_size(msg ? strlen(msg) + 1 : 0),
          m_data(m_size ? new char[m_size] : nullptr)
    {
        if(m_data) memcpy(m_data.get(), msg, m_size);
    }

    /**
     * @brief Gets a pointer to the message data.
     *
     * @return Pointer to the null-terminated message data, or nullptr if empty
     */
    [[nodiscard]] const char* get() const { return m_data.get(); }

    /**
     * @brief Gets the size of the message in bytes, excluding the null terminator.
     *
     * @return Size of the message in bytes (0 if empty)
     */
    [[nodiscard]] size_t size() const { return m_size ? m_size-1 : 0; } // Exclude null terminator
};

} // namespace e5