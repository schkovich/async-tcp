Example: Asynchronous TCP Client on Arduino RP2040 Connect (Raspberry Pi Pico Core)
===================================================================================

This example demonstrates the usage of the `TcpClient` library on the Arduino RP2040 Connect board, running the *
*Raspberry Pi Pico Arduino core**. It establishes asynchronous TCP connections to two servers: a "Quote of the Day" (
QOTD) server and an echo server. This example leverages **ESP-Hosted-FG firmware** on the ESP32 co-processor for Wi-Fi
connectivity and utilises **Raspberry Pi Pico SDK**'s asynchronous context handling for efficient event-based
programming.

Key Components and Functionality
--------------------------------

1. **Wi-Fi Connection and ESP-Hosted-FG Integration**

    * Connects to a Wi-Fi network using ESP-Hosted-FG, which enables Wi-Fi capability on the ESP32 co-processor through
      SPI communication. Credentials are defined in the `secrets.h` file.
    * Demonstrates seamless integration with ESP-Hosted-FG firmware on non-standard cores like the Raspberry Pi Pico
      Arduino core.

2. **Asynchronous TCP Client (`TcpClient`)**

    * Establishes two TCP connections:
        * **QOTD Server** (Quote of the Day) - Retrieves quotes and processes them using connection closure as the
          logical delimiter.
        * **Echo Server** - Sends the received quote to the server and displays the echoed response.
    * Manages connection attempts, data reads, and writes asynchronously, without blocking the main execution thread.
    * Implements proper callback handling for connection, data receipt, and connection FIN events.
    * Uses direct execution paths for same-core operations to improve performance while maintaining thread safety.

3. **Thread-Safe Buffer Management**

    * Uses the `QuoteBuffer` class which extends `SyncBridge` to provide thread-safe access to string data.
    * Implements SET, GET, APPEND, and new SET_COMPLETE, IS_COMPLETE, RESET_COMPLETE operations for comprehensive buffer management.
    * Features quote completion tracking to identify when a complete quote has been received.
    * Supports partial buffer consumption for efficient data processing.
    * Ensures all modifications happen on the core where the ContextManager was initialized.

4. **Event Bridge Pattern for Asynchronous Operations**

    * Implements the `EventBridge` pattern with specialized handlers for each type of event:
        * `QotdConnectedHandler`: Handles successful connections to the QOTD server
        * `QotdReceivedHandler`: Processes data received from the QOTD server with partial consumption support
        * `QotdFinHandler`: Handles TCP FIN events, which signal the end of a complete quote
        * `EchoConnectedHandler`: Manages connections to the echo server
        * `EchoReceivedHandler`: Processes data echoed back from the server with proper delimiter handling
    * Each handler runs on the appropriate core with proper thread safety through context locking mechanisms.

5. **QOTD Protocol Implementation**

    * Implements the complete QOTD protocol, using TCP FIN as the logical delimiter.
    * Tracks quote completion state to know when a quote is fully received.
    * Resets the quote buffer on new connections to accommodate a new quote.
    * Properly drains remaining data when FIN packet is received.
    * Ensures quotes are processed sequentially by waiting for the buffer to be empty before getting a new quote.

6. **Asynchronous Context Management**

    * Utilizes the Raspberry Pi Pico SDK's `async_context` through the `ContextManager` class.
    * Creates separate context managers for each core to ensure proper task distribution.
    * Uses `EventBridge` derivatives to ensure operations occur on the correct core.
    * Uses optimized execution paths with `isCrossCore()`, `ctxLock()`, and `ctxUnlock()` methods.
    * Prevents misuse in interrupt contexts through appropriate assertions.

Concepts Demonstrated
---------------------

* **Event-Driven Asynchronous Networking**: The example showcases non-blocking, asynchronous TCP operations, allowing
  the main program to handle other tasks while waiting for network responses.

* **Thread-Safe Printing Operations**: All print operations are routed through the `PrintHandler` class to core 1,
  ensuring thread-safe serial output.

* **Protocol Implementation with Connection-Based Delimiter**: Demonstrates how to implement a protocol (QOTD) that uses
  TCP FIN as the logical end-of-data marker.

* **Thread Safety in Dual-Core Systems**: Shows how to ensure data consistency when operating across both cores of the
  RP2040 using the SyncBridge pattern with optimized same-core execution paths.

* **Core Affinity Management**: Illustrates how to ensure operations run on the appropriate core for handling specific
  hardware or timing-sensitive tasks.

* **Partial Buffer Consumption**: Demonstrates efficient handling of incoming data through threshold-based partial consumption.

* **Quote Completion Tracking**: Shows how to track the completion state of received data for better application-level protocol handling.

Requirements
------------

* **Board**: Arduino RP2040 Connect with Raspberry Pi Pico Arduino core.
* **Wi-Fi Firmware**: ESP-Hosted-FG firmware on the ESP32 co-processor, for SPI-based Wi-Fi control.
* **Servers**: Access to a QOTD server and an Echo server for testing.

How to Run
----------

1. Flash the ESP-Hosted-FG firmware on the ESP32 co-processor following the
   instructions [here](https://github.com/Networking-for-Arduino/ESPHost).
2. Set up your Wi-Fi credentials and server addresses in `secrets.h`.
3. Upload the example code to the Arduino RP2040 Connect board.
4. Monitor the serial output to see quotes being received, processed, and echoed back.

The program will connect to the QOTD server at regular intervals, process the complete quotes when connections close,
and then send them to the echo server. This demonstrates the `TcpClient` library's asynchronous networking
capabilities, proper protocol handling, and thread-safe operation in a dual-core environment.

Purpose
-------
This example is used for functional and stress testing of the async-tcp library.
