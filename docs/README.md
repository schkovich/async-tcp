Example: Asynchronous TCP Client on Arduino RP2040 Connect (Raspberry Pi Pico Core)
===================================================================================

This example demonstrates the usage of the `AsyncTcpClient` library on the Arduino RP2040 Connect board, running the **Raspberry Pi Pico Arduino core**. It establishes asynchronous TCP connections to two servers: a "Quote of the Day" (QOTD) server and an echo server. This example leverages **ESP-Hosted-FG firmware** on the ESP32 co-processor for Wi-Fi connectivity and utilizes **Raspberry Pi Pico SDK**'s asynchronous context handling for efficient event-based programming.

Key Components and Functionality
--------------------------------

1.  **Wi-Fi Connection and ESP-Hosted-FG Integration**

    *   Connects to a Wi-Fi network using ESP-Hosted-FG, which enables Wi-Fi functionality on the ESP32 co-processor through SPI communication. Credentials are defined in the `secrets.h` file.
    *   Demonstrates seamless integration with ESP-Hosted-FG firmware on non-standard cores like the Raspberry Pi Pico Arduino core.
2.  **Asynchronous TCP Client (`AsyncTcpClient`)**

    *   Establishes two TCP connections:
        *   **QOTD Server** (Quote of the Day) - Retrieves and prints a quote.
        *   **Echo Server** - Sends the received quote to the server and receives it back as-is.
    *   Manages connection attempts, data reads, and writes asynchronously, without blocking the main execution thread.
3.  **Asynchronous Event Handling with `async_context` and Workers**

    *   Utilizes the Raspberry Pi Pico SDK’s `async_context` for event-based handling of network interactions, managed by the `ContextManager`.
    *   **Workers** are created and assigned specific callback functions (`do_work`) to handle asynchronous TCP tasks, such as reading data from the QOTD server and echo server.
    *   Events are handled by `ReceiveCallbackHandler` instances, which ensure proper task-specific execution for each network operation.
4.  **Data Processing and Reusable Worker Functionality**

    *   Received data (quotes) from the QOTD server is stored in a global `tx_buffer`.
    *   The data is then sent to the echo server, and the response is processed by the `echo_worker`, which reverses the returned quote and prints it to Serial. This demonstrates how workers can be customized for response processing, allowing consuming applications to tailor handling to specific use cases.
    *   Modular data handling and `WorkerData` management ensure each worker has independent, task-specific data for smooth operation.

Concepts Demonstrated
---------------------

*   **Event-Driven Asynchronous Networking**: The example showcases non-blocking, asynchronous TCP operations, allowing the main program to handle other tasks while waiting for network responses.
*   **Memory and Resource Management in Asynchronous Systems**: The program uses smart pointers (`std::shared_ptr`, `std::unique_ptr`) to ensure safe memory management and resource-sharing between threads.
*   **Concurrency Control with `async_context` and `ContextManager`**: Utilizes the Raspberry Pi Pico SDK’s concurrency features, demonstrating effective worker scheduling and task management for asynchronous programming on resource-constrained systems.
*   **Customizing Event Handlers and Workers**: Implements reusable event handlers (`EventHandler`, `ReceiveCallbackHandler`) and flexible worker functions to adapt response processing to specific application needs, emphasizing modularity and reusability.

Requirements
------------

*   **Board**: Arduino RP2040 Connect with Raspberry Pi Pico Arduino core.
*   **Wi-Fi Firmware**: ESP-Hosted-FG firmware on the ESP32 co-processor, for SPI-based Wi-Fi control.

How to Run
----------

1.  Flash the ESP-Hosted-FG firmware on the ESP32 co-processor following the instructions [here](https://github.com/Networking-for-Arduino/ESPHost).
2.  Set up your Wi-Fi credentials in `secrets.h`.
3.  Upload `main.cpp` to the Arduino RP2040 Connect board.

The program will output the received quote and the processed (reversed) echo response over Serial, demonstrating the `AsyncTcpClient` library’s asynchronous networking capabilities and flexibility in response handling.

Purpose
-------
This example serves as a proof-of-concept demonstrating how to replace boost::asio functionality in an embedded context using:
- AsyncTcpClient for network operations (replacing boost networking)
- Pico SDK's async_context for event handling (replacing boost event loop)
- Worker pattern for task management (replacing boost async operations)

These concepts are fundamental to the project's main goal of porting cc.mqtt5.libs example applications to the RP2040 Connect platform.
