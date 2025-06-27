# AsyncTCPClient Library

This library provides a thread-safe, asynchronous TCP client implementation for embedded systems using Raspberry Pi
Pico's dual-core architecture. It enables safe execution of non-thread-safe operations with proper core affinity
guarantees.

## Key Architecture

### Core Affinity Enforcement

- Uses `async_context_threadsafe_background` bound to a specific core
- All operations execute through the core's event loop
- Guarantees non-preemptive execution of operations

### Thread Safety Mechanism

```cpp
// Simplified execution flow
[Any Core/IRQ] --> [Queue Task] --> [Target Core Context] --> [Execute Operation]
```

## Core Components

### 1. ContextManager

- Wraps Pico SDK's `async_context_threadsafe_background`
- Provides cross-core task queuing
- Ensures atomic access through internal locking
- Core affinity determined by initialization core

### 2. SyncBridge

- Implements synchronous resource access pattern
- Ensures thread-safe modifications to shared resources
- Handles both core-to-core and IRQ-to-core communication
- Used for task management operations

### 3. EventBridge

- Implements asynchronous event handling pattern
- Ensures proper core affinity for event handlers
- Provides a clean interface for implementing callbacks
- Used for network events and one-shot operations

### 4. TaskRegistry

- Fixed-size, memory-efficient alternative to std::map
- Uses pointer addresses as keys for efficient lookup
- Pre-allocated memory for deterministic performance
- Singly-linked list implementation for minimal overhead

### 5. QuoteBuffer

- Thread-safe buffer for storing and accessing string data
- Uses SyncBridge pattern for synchronized access
- Provides methods for setting, appending, and retrieving data
- Ensures all operations execute on the correct core

### 6. IoWrite

- Thread-safe wrapper for TcpClient write operations
- Ensures write operations execute on the correct core
- Supports writing from buffers, strings, and streams
- Non-invasive approach that doesn't modify TcpClient

## Implementation Patterns

### 1. Network Event Handling

```cpp
class EchoConnectedHandler final : public EventBridge {
protected:
    void onWork() override {
        // Configure connection
        m_io.keepAlive();
        m_io.setNoDelay(true);  // Disable Nagle's algorithm
        
        // Print connection info
        m_serial_printer.print("Connected!");
    }
};

// Usage
auto handler = std::make_unique<EchoConnectedHandler>(ctx, client, printer);
client.setOnConnectedCallback(std::move(handler));
```

### 2. Asynchronous Printing

```cpp
void SerialPrinter::print(const char* message) {
    // Create message buffer and handler
    MessageBuffer buf(message);
    auto print_handler = std::make_unique<PrintHandler>(m_ctx, std::move(buf), *this);
    void* handler_ptr = print_handler.get();
    
    // Register and trigger handler
    addTask(handler_ptr, std::move(print_handler));
    static_cast<PrintHandler*>(handler_ptr)->setPending();
    return handler_ptr;
}
```

### 3. Thread-Safe Task Management

```cpp
void SerialPrinter::addTask(void* handler_ptr, PrintHandlerPtr print_handler) {
    auto payload = std::make_unique<TaskMutationPayload>();
    payload->action = TaskMutationPayload::ADD;
    payload->handler_ptr = handler_ptr;
    payload->handler = std::move(print_handler);
    execute(std::move(payload));  // Executes on target core through SyncBridge
}
```

### 4. Thread-Safe Buffer Access

```cpp
std::string QuoteBuffer::get() {
    std::string result;
    auto payload = std::make_unique<BufferPayload>();
    payload->op = BufferPayload::GET;
    payload->result_ptr = &result;
    execute(std::move(payload));
    return result;
}
```

### 5. Thread-Safe I/O Operations

```cpp
void get_echo() {
    if (!echo_client.connected()) {
        if (0 == echo_client.connect(echo_ip_address, echo_port)) {
            DEBUGV("Failed to connect to echo server..\n");
        }
    } else {
        // Get the quote buffer content in a thread-safe manner
        std::string buffer_content = qotd_buffer.get();
        
        if (!buffer_content.empty()) {
            // Use IoWrite for thread-safe write operations
            IoWrite io_write(ctx, echo_client);
            io_write.write(buffer_content.c_str());
        }
    }
}
```

## Usage Examples

### Basic TCP Client

```cpp
// Initialize context manager on desired core
auto ctx = std::make_unique<ContextManager>();
ctx->initDefaultContext();

// Create TCP client and serial printer
TcpClient client;
SerialPrinter printer(ctx);

// Set up event handlers
auto connected_handler = std::make_unique<ConnectedHandler>(ctx, client, printer);
client.setOnConnectedCallback(std::move(connected_handler));

auto received_handler = std::make_unique<ReceivedHandler>(ctx, client, printer);
client.setOnReceivedCallback(std::move(received_handler));

// Connect to server
IPAddress server_ip(192, 168, 1, 1);
client.connect(server_ip, 80);
```

### Asynchronous Printing

```cpp
// Initialize context manager on Core 1
auto ctx = std::make_unique<ContextManager>();
ctx->initDefaultContext();

// Create serial printer
SerialPrinter printer(ctx);

// Print messages from any core or interrupt context
void irq_handler() {
    printer.print("IRQ triggered");  // Thread-safe, non-blocking
}
```

### Thread-Safe Buffer Operations

```cpp
// Initialize context manager and buffer
auto ctx = std::make_unique<ContextManager>();
ctx->initDefaultContext();
QuoteBuffer buffer(ctx);

// Thread-safe operations from any core
buffer.set("Hello, World!");
buffer.append(" More text.");
std::string content = buffer.get();
buffer.clear();
```

## Example Application: QOTD Echo Mirror

The library includes a practical example application that demonstrates its capabilities:

- **Quote of the Day (QOTD) Client**: Connects to a QOTD server and retrieves quotes
- **Echo Client**: Sends the received quotes to an echo server
- **Mirroring**: Displays the echo server's response (reversed quote)
- **Performance Monitoring**: Tracks and displays heap usage statistics

This example serves multiple purposes:

1. **Demonstrates Thread Safety**: Shows how to safely access shared resources from multiple contexts
2. **Tests Core Affinity**: Ensures operations execute on the correct core
3. **Simulates Real-World Workloads**: Configurable intervals allow testing different load scenarios
4. **Monitors Resource Usage**: Provides insights into memory consumption
5. **Showcases Integration**: Shows how all components work together in a practical application

## Design Patterns

The library implements several design patterns:

1. **Bridge Pattern**: Separates abstraction from implementation (EventBridge, SyncBridge)
2. **Template Method Pattern**: Defines algorithm skeleton with customizable steps (onWork, onExecute)
3. **Observer Pattern**: Notifies objects of state changes (callback handlers)
4. **RAII Pattern**: Manages resource lifecycle through object lifetime (unique_ptr ownership)
5. **Proxy Pattern**: Controls access to another object (ContextManager)
6. **Adapter Pattern**: Adapts interfaces to work together (IoWrite)

## Performance Considerations

- **Memory Efficiency**: Fixed-size TaskRegistry with pre-allocated memory
- **Core Affinity**: All operations execute on the core where ContextManager was initialized
- **Thread Safety**: All shared resource access is synchronized through SyncBridge
- **Non-Blocking**: Operations return immediately, with actual work happening asynchronously
- **Minimal Overhead**: Focused wrappers (IoWrite) add thread safety only where needed

## Requirements

- Raspberry Pi Pico SDK 1.5.0+
- C++17 compatible toolchain
- Dual-core configuration
- ESP-Hosted-FG firmware for Wi-Fi connectivity

## Best Practices

1. **Initialize ContextManager on the appropriate core** for your non-thread-safe operations
2. **Use EventBridge for event handlers** that need to execute asynchronously
3. **Use SyncBridge for thread-safe resource access** when multiple threads need to modify shared data
4. **Consider memory constraints** when setting MAX_PRINT_TASKS and other fixed-size parameters
5. **Batch operations where possible** to reduce context switching overhead
6. **Use thread-safe wrappers** like IoWrite for operations that might be called from multiple contexts
7. **Resolve hostnames once** at startup rather than on each connection attempt
8. **Clear buffers on new connections** to avoid mixing data from different sessions

This library is particularly useful for:

- Network applications requiring thread safety
- Systems with shared resources across cores
- Applications needing deterministic execution timing
- Legacy code that isn't thread-safe
