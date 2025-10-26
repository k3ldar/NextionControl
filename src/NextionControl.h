#include <stdint.h>
#pragma once

#include <Arduino.h>
#include "BaseDisplayPage.h"

/**
 * @file NextionControl.h
 * @brief High-level controller for a Nextion HMI display.
 *
 * This component manages page lifecycle, periodic refresh, and serial protocol
 * communication with a Nextion display. It reads and parses messages coming
 * from the display and routes them to the active `BaseDisplayPage` instance.
 *
 * Typical usage pattern in an Arduino sketch:
 * - Construct the controller with a `Stream` implementation (e.g. `Serial2`) and
 *   an array of page instances derived from `BaseDisplayPage`.
 * - Call `begin()` in `setup()` to initialize the display and the first page.
 * - Call `update(millis())` frequently in `loop()` to process serial input and
 *   perform periodic page refresh.
 */

/// Time in milliseconds between periodic page refresh calls.
const int RefreshTime = 1000;          // ms between periodic updates

/// Size of the internal serial receive buffer used to assemble messages.
const size_t SerialBufferSize = 256;

/// Timeout (ms) for considering a partial message as aborted when no more bytes arrive.
const unsigned long SerialTimeout = 800;

/// Touch event code reported by Nextion for a press.
const byte EventPress = 1;

/// Touch event code reported by Nextion for a release.
const byte EventRelease = 0;

/**
 * @class NextionControl
 * @brief Orchestrates communication and page management for a Nextion display.
 *
 * Responsibilities:
 * - Sending commands to the display with proper termination.
 * - Reading and parsing incoming messages/responses.
 * - Dispatching events (touch, text, numeric, etc.) to the current page.
 * - Managing the active page and invoking its `begin()` and `refresh()` hooks.
 */
class NextionControl {
public:
    /**
     * @brief Construct a controller.
     *
     * @param serialPort Pointer to the `Stream` connected to the Nextion (e.g., `HardwareSerial`).
     *                   The lifetime must exceed that of this controller.
     * @param pageArray  Array of pointers to page instances. The controller does not take ownership
     *                   and expects the pages to remain valid for the controller's lifetime.
     * @param count      Number of entries in `pageArray`.
     */
    NextionControl(Stream* serialPort, BaseDisplayPage** pageArray, size_t count);

    /// @brief Destructor. Does not delete provided page instances or the serial port.
    ~NextionControl();

    /**
     * @brief Initialize communication and set the initial page.
     *
     * Sends the necessary setup commands and calls `begin()` on the first page
     * if available.
     *
     * @return true on successful initialization; false otherwise.
     */
    bool begin();

    /**
     * @brief Run periodic tasks and process incoming serial data.
     *
     * Should be called frequently from the main loop. This method:
     * - Reads from the `Stream` and parses complete messages.
     * - Dispatches messages to the active page.
     * - Triggers periodic `refresh()` on the current page according to `RefreshTime`.
     *
     * @param now Current time in milliseconds (typically from `millis()`).
     */
    void update(unsigned long now);

    /**
     * @brief Send a raw Nextion command.
     *
     * Appends the required 0xFF 0xFF 0xFF terminators to the command.
     *
     * @param cmd Command string (e.g., "page 0", "t0.txt=\"Hello\"").
     */
    void sendCommand(const String& cmd);

    /**
     * @brief Force an immediate refresh of the current page.
     *
     * Calls the active page's `refresh()` regardless of the `RefreshTime` interval.
     */
    void refreshCurrentPage();

    /**
     * @brief Get the currently active page.
     * @return Pointer to the current `BaseDisplayPage`, or nullptr if no page is active.
     */
    BaseDisplayPage* getCurrentPage() const { return currentPage; }

private:
    /// @brief Indicates if a message is currently being assembled from the serial stream.
	bool _readingMessage = false;

    /// @brief Timestamp (ms) of the last received character for timeout management.
    unsigned long _lastCharTime = 0;

    /// @brief Count of 0xFF terminator bytes observed for the current message.
    uint8_t _terminatorCount;

    /// @brief Buffer holding bytes received from the serial stream.
    uint8_t _serialBuffer[SerialBufferSize];

    /// @brief Current write position within `_serialBuffer`.
    size_t _serialBufferPos;

    /// @brief Stream connected to the Nextion display.
    Stream* nextionSerialPort;

    /// @brief Total number of managed pages.
    size_t pageCount;

    /// @brief Array of pointers to page instances (not owned).
    BaseDisplayPage** pages;

    /// @brief Index of the current page.
    uint8_t currPage;

    /// @brief Timestamp for scheduling periodic refresh calls.
    unsigned long refreshTimer;

    /// @brief Pointer to the currently active page.
    BaseDisplayPage* currentPage = nullptr;

    /**
     * @brief Drain the serial port and assemble/parse messages.
     * @param now Current time in milliseconds for timeout calculations.
     */
    void readSerial(unsigned long now);

    /**
     * @brief Switch the active page.
     *
     * Calls `begin()` on the new page and updates internal state.
     *
     * @param index Page index to activate (0-based, must be < `pageCount`).
     */
    void setCurrentPage(size_t index);

    /**
     * @brief Decode and route a single Nextion message to the current page.
     * @param data Pointer to the received message bytes (without trailing 0xFFs).
     * @param len  Length of the message payload in bytes.
     */
    void handleNextionMessage(const uint8_t* data, size_t len);
};

