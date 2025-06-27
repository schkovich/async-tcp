# Async-TCP

Async-TCP is a streamlined C++ library that offers genuine, non-blocking network communication tailored for RP2040
series boards with Arduino Pico core.

Rather than managing threads, blocking calls, or low-level callbacks, you can organise your code around straightforward,
event-driven handlers.

Behind the scenes, the library effectively connects system-level events from the Pico SDK and LwIP to safe and
responsive C++ callbacks that run where they should—on the appropriate core in the defined execution context.

This design enables concurrency without unexpected side effects and makes asynchronous network code easy to comprehend
and expand.