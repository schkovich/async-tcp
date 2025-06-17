/**
 * @file LedDebugger.hpp
 * @brief LED-based debugging utility for asynchronous operations
 *
 * This file defines a utility class for real-time visual debugging using LEDs
 * to represent execution states in asynchronous operations.
 *
 * @author Goran
 * @date 2025-03-15
 * @ingroup AsyncTCPClient
 */

#pragma once

#include "Arduino.h"

namespace e5 {

/**
 * @class LedDebugger
 * @brief Thread-safe LED-based debugging for asynchronous operations
 *
 * This class provides methods to visually represent execution states
 * using a 5-bit pattern displayed on LEDs. This allows for real-time
 * debugging without the delays and buffering issues of serial output.
 *
 * The 5 LEDs are:
 * - B: Blue LED
 * - R: Red LED
 * - Y: Yellow LED
 * - G: Green LED
 * - L: LED_BUILTIN
 *
 * Each state is named using a pattern string where each letter represents
 * an ON state, and 'o' represents an OFF state.
 */
class LedDebugger {

    static constexpr int PATH_LED_1 = D5; // green and left
    static constexpr int PATH_LED_2 = D6; // red and right

    static constexpr int BLUE_LED = D7;
    static constexpr int RED_LED = D8;
    static constexpr int YELLOW_LED = D9;
    static constexpr int GREEN_LED = D10;

    // Debug state codes (5-bit values) with pattern-based naming
    enum DebugZone : uint8_t {
        ooooo = 0x00,  // All LEDs off (idle state)
        ooooL = 0x01,  // Only LED_BUILTIN on
        oooGo = 0x02,  // Only Green LED on
        oooGL = 0x03,  // Green and LED_BUILTIN on
        ooYoo = 0x04,  // Only Yellow LED on
        ooYoL = 0x05,  // Yellow and LED_BUILTIN on
        ooYGo = 0x06,  // Yellow and Green on
        ooYGL = 0x07,  // Yellow, Green, and LED_BUILTIN on
        oRooo = 0x08,  // Only Red LED on
        oRooL = 0x09,  // Red and LED_BUILTIN on
        oRoGo = 0x0A,  // Red and Green on
        oRoGL = 0x0B,  // Red, Green, and LED_BUILTIN on
        oRYoo = 0x0C,  // Red and Yellow on
        oRYoL = 0x0D,  // Red, Yellow, and LED_BUILTIN on
        oRYGo = 0x0E,  // Red, Yellow, and Green on
        oRYGL = 0x0F,  // Red, Yellow, Green, and LED_BUILTIN on
        Boooo = 0x10,  // Only Blue LED on
        BoooL = 0x11,  // Blue and LED_BUILTIN on
        BooGo = 0x12,  // Blue and Green on
        BooGL = 0x13,  // Blue, Green, and LED_BUILTIN on
        BoYoo = 0x14,  // Blue and Yellow on
        BoYoL = 0x15,  // Blue, Yellow, and LED_BUILTIN on
        BoYGo = 0x16,  // Blue, Yellow, and Green on
        BoYGL = 0x17,  // Blue, Yellow, Green, and LED_BUILTIN on
        BRooo = 0x18,  // Blue and Red on
        BRooL = 0x19,  // Blue, Red, and LED_BUILTIN on
        BRoGo = 0x1A,  // Blue, Red, and Green on
        BRoGL = 0x1B,  // Blue, Red, Green, and LED_BUILTIN on
        BRYoo = 0x1C,  // Blue, Red, and Yellow on
        BRYoL = 0x1D,  // Blue, Red, Yellow, and LED_BUILTIN on
        BRYGo = 0x1E,  // Blue, Red, Yellow, and Green on
        BRYGL = 0x1F   // All LEDs on (error state)
    };

    // Execution path codes (2-bit values)
    enum ExecutionPath : uint8_t {
        oo = 0x00,           // default
        Lo = 0x01,            //
        oR = 0x02,            //
        LR = 0x03           //
    };

public:

    /**
     * @brief Initialize the LED pins for debugging
     */
    static void init() {
        pinMode(BLUE_LED, OUTPUT);
        pinMode(RED_LED, OUTPUT);
        pinMode(YELLOW_LED, OUTPUT);
        pinMode(GREEN_LED, OUTPUT);
        pinMode(LED_BUILTIN, OUTPUT);

        // Initialize path tracking LEDs
        pinMode(PATH_LED_1, OUTPUT);
        pinMode(PATH_LED_2, OUTPUT);
    }

    /**
     * A combined 7-bit state: 2 bits for the path, 5 bits for the zone.
     * Example: NONE_ooooo = (oo << 5) | ooooo
     *          LEFT_ooooo = (Lo << 5) | ooooo
     */
    enum CombinedState : uint8_t
    {
        // NONE (oo = 0) => 0x00..0x1F
        NONE_ooooo = (oo << 5) | ooooo,  // 0x00
        NONE_ooooL = (oo << 5) | ooooL,  // 0x01
        NONE_oooGo = (oo << 5) | oooGo,  // 0x02
        NONE_oooGL = (oo << 5) | oooGL,  // 0x03
        NONE_ooYoo = (oo << 5) | ooYoo,  // 0x04
        NONE_ooYoL = (oo << 5) | ooYoL,  // 0x05
        NONE_ooYGo = (oo << 5) | ooYGo,  // 0x06
        NONE_ooYGL = (oo << 5) | ooYGL,  // 0x07
        NONE_oRooo = (oo << 5) | oRooo,  // 0x08
        NONE_oRooL = (oo << 5) | oRooL,  // 0x09
        NONE_oRoGo = (oo << 5) | oRoGo,  // 0x0A
        NONE_oRoGL = (oo << 5) | oRoGL,  // 0x0B
        NONE_oRYoo = (oo << 5) | oRYoo,  // 0x0C
        NONE_oRYoL = (oo << 5) | oRYoL,  // 0x0D
        NONE_oRYGo = (oo << 5) | oRYGo,  // 0x0E
        NONE_oRYGL = (oo << 5) | oRYGL,  // 0x0F
        NONE_Boooo = (oo << 5) | Boooo,  // 0x10
        NONE_BoooL = (oo << 5) | BoooL,  // 0x11
        NONE_BooGo = (oo << 5) | BooGo,  // 0x12
        NONE_BooGL = (oo << 5) | BooGL,  // 0x13
        NONE_BoYoo = (oo << 5) | BoYoo,  // 0x14
        NONE_BoYoL = (oo << 5) | BoYoL,  // 0x15
        NONE_BoYGo = (oo << 5) | BoYGo,  // 0x16
        NONE_BoYGL = (oo << 5) | BoYGL,  // 0x17
        NONE_BRooo = (oo << 5) | BRooo,  // 0x18
        NONE_BRooL = (oo << 5) | BRooL,  // 0x19
        NONE_BRoGo = (oo << 5) | BRoGo,  // 0x1A
        NONE_BRoGL = (oo << 5) | BRoGL,  // 0x1B
        NONE_BRYoo = (oo << 5) | BRYoo,  // 0x1C
        NONE_BRYoL = (oo << 5) | BRYoL,  // 0x1D
        NONE_BRYGo = (oo << 5) | BRYGo,  // 0x1E
        NONE_BRYGL = (oo << 5) | BRYGL,  // 0x1F

        // LEFT (Lo = 1) => 0x20..0x3F
        LEFT_ooooo = (Lo << 5) | ooooo,  // 0x20
        LEFT_ooooL = (Lo << 5) | ooooL,  // 0x21
        LEFT_oooGo = (Lo << 5) | oooGo,  // 0x22
        LEFT_oooGL = (Lo << 5) | oooGL,  // 0x23
        LEFT_ooYoo = (Lo << 5) | ooYoo,  // 0x24
        LEFT_ooYoL = (Lo << 5) | ooYoL,  // 0x25
        LEFT_ooYGo = (Lo << 5) | ooYGo,  // 0x26
        LEFT_ooYGL = (Lo << 5) | ooYGL,  // 0x27
        LEFT_oRooo = (Lo << 5) | oRooo,  // 0x28
        LEFT_oRooL = (Lo << 5) | oRooL,  // 0x29
        LEFT_oRoGo = (Lo << 5) | oRoGo,  // 0x2A
        LEFT_oRoGL = (Lo << 5) | oRoGL,  // 0x2B
        LEFT_oRYoo = (Lo << 5) | oRYoo,  // 0x2C
        LEFT_oRYoL = (Lo << 5) | oRYoL,  // 0x2D
        LEFT_oRYGo = (Lo << 5) | oRYGo,  // 0x2E
        LEFT_oRYGL = (Lo << 5) | oRYGL,  // 0x2F
        LEFT_Boooo = (Lo << 5) | Boooo,  // 0x30
        LEFT_BoooL = (Lo << 5) | BoooL,  // 0x31
        LEFT_BooGo = (Lo << 5) | BooGo,  // 0x32
        LEFT_BooGL = (Lo << 5) | BooGL,  // 0x33
        LEFT_BoYoo = (Lo << 5) | BoYoo,  // 0x34
        LEFT_BoYoL = (Lo << 5) | BoYoL,  // 0x35
        LEFT_BoYGo = (Lo << 5) | BoYGo,  // 0x36
        LEFT_BoYGL = (Lo << 5) | BoYGL,  // 0x37
        LEFT_BRooo = (Lo << 5) | BRooo,  // 0x38
        LEFT_BRooL = (Lo << 5) | BRooL,  // 0x39
        LEFT_BRoGo = (Lo << 5) | BRoGo,  // 0x3A
        LEFT_BRoGL = (Lo << 5) | BRoGL,  // 0x3B
        LEFT_BRYoo = (Lo << 5) | BRYoo,  // 0x3C
        LEFT_BRYoL = (Lo << 5) | BRYoL,  // 0x3D
        LEFT_BRYGo = (Lo << 5) | BRYGo,  // 0x3E
        LEFT_BRYGL = (Lo << 5) | BRYGL,  // 0x3F

        // RIGHT (oR = 2) => 0x40..0x5F
        RIGHT_ooooo = (oR << 5) | ooooo, // 0x40
        RIGHT_ooooL = (oR << 5) | ooooL, // 0x41
        RIGHT_oooGo = (oR << 5) | oooGo, // 0x42
        RIGHT_oooGL = (oR << 5) | oooGL, // 0x43
        RIGHT_ooYoo = (oR << 5) | ooYoo, // 0x44
        RIGHT_ooYoL = (oR << 5) | ooYoL, // 0x45
        RIGHT_ooYGo = (oR << 5) | ooYGo, // 0x46
        RIGHT_ooYGL = (oR << 5) | ooYGL, // 0x47
        RIGHT_oRooo = (oR << 5) | oRooo, // 0x48
        RIGHT_oRooL = (oR << 5) | oRooL, // 0x49
        RIGHT_oRoGo = (oR << 5) | oRoGo, // 0x4A
        RIGHT_oRoGL = (oR << 5) | oRoGL, // 0x4B
        RIGHT_oRYoo = (oR << 5) | oRYoo, // 0x4C
        RIGHT_oRYoL = (oR << 5) | oRYoL, // 0x4D
        RIGHT_oRYGo = (oR << 5) | oRYGo, // 0x4E
        RIGHT_oRYGL = (oR << 5) | oRYGL, // 0x4F
        RIGHT_Boooo = (oR << 5) | Boooo, // 0x50
        RIGHT_BoooL = (oR << 5) | BoooL, // 0x51
        RIGHT_BooGo = (oR << 5) | BooGo, // 0x52
        RIGHT_BooGL = (oR << 5) | BooGL, // 0x53
        RIGHT_BoYoo = (oR << 5) | BoYoo, // 0x54
        RIGHT_BoYoL = (oR << 5) | BoYoL, // 0x55
        RIGHT_BoYGo = (oR << 5) | BoYGo, // 0x56
        RIGHT_BoYGL = (oR << 5) | BoYGL, // 0x57
        RIGHT_BRooo = (oR << 5) | BRooo, // 0x58
        RIGHT_BRooL = (oR << 5) | BRooL, // 0x59
        RIGHT_BRoGo = (oR << 5) | BRoGo, // 0x5A
        RIGHT_BRoGL = (oR << 5) | BRoGL, // 0x5B
        RIGHT_BRYoo = (oR << 5) | BRYoo, // 0x5C
        RIGHT_BRYoL = (oR << 5) | BRYoL, // 0x5D
        RIGHT_BRYGo = (oR << 5) | BRYGo, // 0x5E
        RIGHT_BRYGL = (oR << 5) | BRYGL, // 0x5F

        // MIDDLE (LR = 3) => 0x60..0x7F
        MIDDLE_ooooo = (LR << 5) | ooooo, // 0x60
        MIDDLE_ooooL = (LR << 5) | ooooL, // 0x61
        MIDDLE_oooGo = (LR << 5) | oooGo, // 0x62
        MIDDLE_oooGL = (LR << 5) | oooGL, // 0x63
        MIDDLE_ooYoo = (LR << 5) | ooYoo, // 0x64
        MIDDLE_ooYoL = (LR << 5) | ooYoL, // 0x65
        MIDDLE_ooYGo = (LR << 5) | ooYGo, // 0x66
        MIDDLE_ooYGL = (LR << 5) | ooYGL, // 0x67
        MIDDLE_oRooo = (LR << 5) | oRooo, // 0x68
        MIDDLE_oRooL = (LR << 5) | oRooL, // 0x69
        MIDDLE_oRoGo = (LR << 5) | oRoGo, // 0x6A
        MIDDLE_oRoGL = (LR << 5) | oRoGL, // 0x6B
        MIDDLE_oRYoo = (LR << 5) | oRYoo, // 0x6C
        MIDDLE_oRYoL = (LR << 5) | oRYoL, // 0x6D
        MIDDLE_oRYGo = (LR << 5) | oRYGo, // 0x6E
        MIDDLE_oRYGL = (LR << 5) | oRYGL, // 0x6F
        MIDDLE_Boooo = (LR << 5) | Boooo, // 0x70
        MIDDLE_BoooL = (LR << 5) | BoooL, // 0x71
        MIDDLE_BooGo = (LR << 5) | BooGo, // 0x72
        MIDDLE_BooGL = (LR << 5) | BooGL, // 0x73
        MIDDLE_BoYoo = (LR << 5) | BoYoo, // 0x74
        MIDDLE_BoYoL = (LR << 5) | BoYoL, // 0x75
        MIDDLE_BoYGo = (LR << 5) | BoYGo, // 0x76
        MIDDLE_BoYGL = (LR << 5) | BoYGL, // 0x77
        MIDDLE_BRooo = (LR << 5) | BRooo, // 0x78
        MIDDLE_BRooL = (LR << 5) | BRooL, // 0x79
        MIDDLE_BRoGo = (LR << 5) | BRoGo, // 0x7A
        MIDDLE_BRoGL = (LR << 5) | BRoGL, // 0x7B
        MIDDLE_BRYoo = (LR << 5) | BRYoo, // 0x7C
        MIDDLE_BRYoL = (LR << 5) | BRYoL, // 0x7D
        MIDDLE_BRYGo = (LR << 5) | BRYGo, // 0x7E
        MIDDLE_BRYGL = (LR << 5) | BRYGL  // 0x7F
    };

    /**
     * @brief Public method to set a combined 7-bit state.
     * This will set the zone bits (5) and path bits (2) internally.
     */
    static void setState(const CombinedState combined)
    {
        const auto zone = static_cast<DebugZone>(combined & 0x1F);
        setZone(zone);

        const auto path = static_cast<ExecutionPath>((combined >> 5) & 0x03);
        setPath(path);
    }

    /**
     * @brief Public method to get the current combined 7-bit state.
     * Reads back the zone bits (5) and path bits (2).
     */
    static CombinedState getState()
    {
        const DebugZone zone = getZone();
        const ExecutionPath path = getPath();
        const uint8_t combinedValue = (static_cast<uint8_t>(path) << 5)
                                      | static_cast<uint8_t>(zone);
        return static_cast<CombinedState>(combinedValue);
    }

    /**
     * @brief Get a descriptive string for the current or given DebugZone
     */
    static const char* getZoneString(const DebugZone state = getZone())
    {
        static char stateStr[6];
        stateStr[0] = (state & 0x10) ? 'B' : 'o';
        stateStr[1] = (state & 0x08) ? 'R' : 'o';
        stateStr[2] = (state & 0x04) ? 'Y' : 'o';
        stateStr[3] = (state & 0x02) ? 'G' : 'o';
        stateStr[4] = (state & 0x01) ? 'L' : 'o';
        stateStr[5] = '\0';
        return stateStr;
    }

private:
    /**
     * @brief Set the debug zone (5 bits). Now private.
     */
    static void setZone(const DebugZone zone)
    {
        digitalWrite(BLUE_LED,    (zone & 0x10) ? HIGH : LOW);
        digitalWrite(RED_LED,     (zone & 0x08) ? HIGH : LOW);
        digitalWrite(YELLOW_LED,  (zone & 0x04) ? HIGH : LOW);
        digitalWrite(GREEN_LED,   (zone & 0x02) ? HIGH : LOW);
        digitalWrite(LED_BUILTIN, (zone & 0x01) ? HIGH : LOW);
    }

    /**
     * @brief Get the current debug zone from the pins. Private.
     */
    static DebugZone getZone()
    {
        uint8_t zone = 0;
        if (digitalRead(BLUE_LED)    == HIGH) zone |= 0x10;
        if (digitalRead(RED_LED)     == HIGH) zone |= 0x08;
        if (digitalRead(YELLOW_LED)  == HIGH) zone |= 0x04;
        if (digitalRead(GREEN_LED)   == HIGH) zone |= 0x02;
        if (digitalRead(LED_BUILTIN) == HIGH) zone |= 0x01;
        return static_cast<DebugZone>(zone);
    }

    /**
     * @brief Set the execution path (2 bits). Now private.
     */
    static void setPath(const ExecutionPath path)
    {
        digitalWrite(PATH_LED_1, (path & 0x01) ? HIGH : LOW);
        digitalWrite(PATH_LED_2, (path & 0x02) ? HIGH : LOW);
    }

    /**
     * @brief Get the current path from the pins. Private.
     */
    static ExecutionPath getPath()
    {
        uint8_t path = 0;
        if (digitalRead(PATH_LED_1) == HIGH) path |= 0x01;
        if (digitalRead(PATH_LED_2) == HIGH) path |= 0x02;
        return static_cast<ExecutionPath>(path);
    }

};

} // namespace e5
