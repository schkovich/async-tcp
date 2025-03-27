/**
 * @file TaskTypes.hpp
 * @brief Common type definitions and constants for task management
 *
 * This file defines common constants, type aliases, and forward declarations
 * used across the task management system. It serves as a single source of truth
 * for shared definitions to prevent circular dependencies between components.
 *
 * @author Goran
 * @date 2025-03-10
 * @ingroup AsyncTCPClient
 */

#pragma once
#include <memory>

namespace e5 {

    // Forward declarations
    class PrintHandler;

    // Type aliases
    using PrintHandlerPtr = std::unique_ptr<PrintHandler>;
}