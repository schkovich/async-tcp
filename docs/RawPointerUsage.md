### Raw Pointer Usage in async_tcp Library

The use of raw pointers for `TcpClient` is a deliberate design choice that aligns with both the Pico SDK
requirements and the client's own lifecycle management:

1. **TcpClient Lifecycle**
    - Has built-in reference counting through `_ctx`
    - Protected constructor with `AsyncTcpClientContext`
    - Copy constructor manages reference counting via `_ctx->ref()`

2. **Client Pointer Journey**
   ```cpp
   // Step 1: Client instance in main
   async_tcp::TcpClient qotd_client;

   // Step 2: Handler initialization
   void ReceiveCallbackHandler::init(TcpClient &client) {
       _client = &client;  // Stores raw pointer
   }

   // Step 3: Event handling
   // Handler creates WorkerData and passes client pointer
   auto data = std::make_unique<WorkerData>();
   data->client = _client;

   // Step 4: Worker data transfer
   // Data ownership moves to Pico SDK's async_context
   worker->user_data = data.release();

   // Step 5: Work function access
   // In read_qotd function
   auto *pData = static_cast<WorkerData *>(worker->user_data);
   pData->client->read(/*...*/);
   ```

3. **Why Raw Pointers Work Here**
    - Pico SDK's `async_context` requires raw pointers
    - Client's lifecycle is managed by its own reference counting
    - Worker data lifecycle is well-defined:
        * Created in handler
        * Moved to worker's `user_data`
        * Cleaned up when work is complete

4. **Integration with Async Context**
    - Raw pointers allow seamless integration with Pico SDK
    - Work is executed under context lock, preventing race conditions
    - Clear ownership transfer through the async system

This design provides a practical balance between the embedded system requirements and safe memory management, while
maintaining compatibility with the Pico SDK's async context system.

**Object Lifecycle Diagram**

```plaintext
[TcpClient] --owns--> [AsyncTcpClientContext]
       ^                           |
       |                          |
       +--------ref counting------+
```

**Pointer Journey Flow**

```plaintext
[Main]
   |
   v
[TcpClient] ----raw ptr----> [ReceiveCallbackHandler]
                                         |
                                         v
                                  [WorkerData (unique_ptr)]
                                         |
                                         v
                                  [async_context worker]
                                  (user_data raw ptr)
                                         |
                                         v
                                  [Work Function]
                                  (cleanup on completion)
```

**Async Context Integration**

```plaintext
[Core 0]                    [Core 1]
   |                           |
[Context Lock]            [Context Lock]
   |                           |
[TcpClient]          [Worker]
   |                           |
[Network Events]         [Processing]
```
