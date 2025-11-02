#include <stdint.h>
#pragma once

#include <Arduino.h>
#include "BaseDisplayPage.h"

/**
 * @def NEXTION_DEBUG
 * @brief Enable debug output for NextionControl library.
 * 
 * Uncomment the line below to enable detailed debug output via Serial.
 * This includes:
 * - Raw byte reception from the Nextion display
 * - Parsed message content and routing
 * - Touch events, page changes, and command responses
 * - Error conditions and warnings
 * 
 * Debug output can significantly impact performance and memory usage.
 * Only enable for development and troubleshooting.
 * 
 * To enable debug output, uncomment the following line:
 */
// #define NEXTION_DEBUG

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
 * - Defensive page synchronization: If a touch event (0x65) is received for a page
 *   that doesn't match the current page, the controller automatically switches to
 *   that page. This handles cases where page change events (0x66) are missed or
 *   the display is manually navigated.
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
     * @brief Request the current page ID from the Nextion display.
     * 
     * Sends a "sendme" command to the Nextion, which will respond with a page
     * change event (0x66) containing the actual current page ID. This is useful
     * for synchronizing the internal state with the display's actual state,
     * especially after initialization or when recovering from communication errors.
     * 
     * The response will be handled asynchronously in the next update() call.
     */
    void requestCurrentPage();

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
     * @brief Switch to a page by its page ID.
     * 
     * Finds the page with the matching page ID and activates it. Handles
     * deactivation of the old page and initialization of the new page.
     * 
     * @param pageId The Nextion page ID to switch to (0-255).
     * @return true if the page was found and activated, false if not found.
     */
    bool switchToPageById(uint8_t pageId);

    /**
     * @brief Decode and route a single Nextion message to the current page.
     * @param data Pointer to the received message bytes (without trailing 0xFFs).
     * @param len  Length of the message payload in bytes.
     */
    void handleNextionMessage(const uint8_t* data, size_t len);
};

