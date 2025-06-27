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
    * Implements proper callback handling for connection, data receipt, and connection closure events.

3. **Thread-Safe Buffer Management**

    * Uses the `QuoteBuffer` class which extends `SyncBridge` to provide thread-safe access to string data.
    * Implements SET, GET, and APPEND operations for flexible buffer manipulation.
    * Ensures all modifications happen on the core where the ContextManager was initialized.

4. **Event Bridge Pattern for Asynchronous Operations**

    * Implements the `EventBridge` pattern with specialized handlers for each type of event:
        * `QotdConnectedHandler`: Handles successful connections to the QOTD server
        * `QotdReceivedHandler`: Processes data received from the QOTD server
        * `QotdClosedHandler`: Handles connection closure events, which signal the end of a complete quote
        * `EchoConnectedHandler`: Manages connections to the echo server
        * `EchoReceivedHandler`: Processes data echoed back from the server
    * Each handler runs on the appropriate core with proper thread safety.

5. **QOTD Protocol Implementation**

    * Implements the complete QOTD protocol which uses connection closure as the logical delimiter.
    * Appends an "End of Quote" marker when a connection is closed to indicate a complete quote.
    * Ensures quotes are processed sequentially by waiting for the buffer to be empty before getting a new quote.
    * Removes the marker before displaying echoed quotes for clean output.

6. **Asynchronous Context Management**

    * Utilizes the Raspberry Pi Pico SDK's `async_context` through the `ContextManager` class.
    * Creates separate context managers for each core to ensure proper task distribution.
    * Uses `EventBridge` derivatives to ensure operations occur on the correct core.

Concepts Demonstrated
---------------------

* **Event-Driven Asynchronous Networking**: The example showcases non-blocking, asynchronous TCP operations, allowing
  the main program to handle other tasks while waiting for network responses.

* **Thread-Safe Printing Operations**: All print operations are routed through the `PrintHandler` class to core 1,
  ensuring thread-safe serial output. This is critical since Arduino print operations are not inherently thread-safe,
  and the async_context guarantees logical single-threaded execution within a core.

* **Protocol Implementation with Connection-Based Delimiter**: Demonstrates how to implement a protocol (QOTD) that uses
  connection closure as the logical end-of-data marker.

* **Thread Safety in Dual-Core Systems**: Shows how to ensure data consistency when operating across both cores of the
  RP2040 using the SyncBridge pattern.

* **Core Affinity Management**: Illustrates how to ensure operations run on the appropriate core for handling specific
  hardware or timing-sensitive tasks.

* **Callback-Based Event Processing**: Implements a callback system for connection events, data reception, and
  connection closure.

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
This example serves as a proof-of-concept demonstrating how to replace boost::asio functionality in an embedded context
using:

- TcpClient for network operations (replacing boost networking)
- Pico SDK's async_context for event handling (replacing boost event loop)
- EventBridge pattern for handling network events.
- SyncBridge pattern for thread-safe resource access.
