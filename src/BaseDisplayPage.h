#pragma once

// forward declaration
class NextionControl;


/**
 * @class BaseDisplayPage
 * @brief Abstract base class for a display page on a Nextion display.
 *
 * This class provides the foundation for creating custom display pages with
 * lifecycle management, event handling, and communication with the Nextion display.
 * Derived classes implement page-specific behavior by overriding virtual methods.
 *
 * Key features:
 * - Lifecycle hooks: onEnterPage() and onLeavePage() for state management
 * - Event handlers: touch, text, numeric, sleep, and coordinate events
 * - Helper methods for sending commands and updating UI components
 * - Automatic active state tracking via NextionControl
 */
class BaseDisplayPage {
	friend class NextionControl;

public:
    /**
     * @brief Destructor (virtual to allow proper cleanup in derived classes)
     */
    virtual ~BaseDisplayPage() {}

    /**
     * @brief Handle external state updates from command handlers or other sources.
     * 
     * This method provides a generic extension point for domain-specific updates
     * without coupling the base class to specific data types. This allows command
     * handlers to notify the current page of state changes that may need to be
     * reflected in the UI.
     * 
     * @param updateType Numeric identifier for the update type. Define constants
     *                   or enums in your derived class for specific update types.
     *                   Recommended to use values starting from 0x01 and above,
     *                   with 0x00 reserved for "no update" or invalid type.
     *                   Using uint8_t provides fast integer comparison and no
     *                   memory allocation overhead (256 possible values is more
     *                   than sufficient for embedded display applications).
     * 
     * @param data Pointer to update-specific data. The caller must cast to the
     *             appropriate type based on updateType. Data is only valid during
     *             the method call; copy if persistence is needed. May be nullptr
     *             if the update type requires no additional data.
     * 
     * @note Default implementation does nothing. Override in derived classes to
     *       handle specific update types relevant to that page.
     * 
     * @note This method is called from the same context as the command handler,
     *       so updates will be processed immediately and synchronously.
     * 
     * Example usage in derived class:
     * @code
     * // Define update type constants or enum
     * enum class MyPageUpdateType : uint8_t {
     *     None = 0x00,
     *     RelayState = 0x01,
     *     SensorData = 0x02
     * };
     * 
     * // Define data structure
     * struct RelayStateUpdate {
     *     uint8_t relayIndex;  // 0-based relay index
     *     bool isOn;           // true = on, false = off
     * };
     * 
     * // Override in derived class
     * void MyPage::handleExternalUpdate(uint8_t updateType, const void* data) {
     *     if (updateType == static_cast<uint8_t>(MyPageUpdateType::RelayState) 
     *         && data != nullptr) {
     *         const RelayStateUpdate* update = static_cast<const RelayStateUpdate*>(data);
     *         updateButtonState(update->relayIndex, update->isOn);
     *     }
     * }
     * 
     * // Call from command handler
     * RelayStateUpdate update = {3, true};
     * currentPage->handleExternalUpdate(
     *     static_cast<uint8_t>(MyPageUpdateType::RelayState), 
     *     &update
     * );
     * @endcode
     */
    virtual void handleExternalUpdate(uint8_t updateType, const void* data)
    {
        (void)updateType;
        (void)data;
        // Default: do nothing - keeps base class generic
    }
    
    /**
     * @brief Check if this page is currently active (displayed on Nextion).
     * @return true if this page is the currently active page, false otherwise
     */
    bool isActive() const { return _isActive; }

protected:
    /**
     * @brief Construct a display page.
     * @param serialPort Pointer to the Stream connected to the Nextion display.
     *                   Must remain valid for the lifetime of this page.
     */
    explicit BaseDisplayPage(Stream* serialPort) 
        : nextionSerialPort(serialPort), 
          _initialized(false),
          _isActive(false) {}

    /**
     * @brief Get the unique page identifier matching the Nextion HMI page ID.
     * @return Page ID (0-255) corresponding to the page number in the Nextion Editor.
     * @note Must be implemented by derived classes to return their page ID.
     */
    virtual uint8_t getPageId() const = 0;

    /**
     * @brief Called when this page becomes the active page.
     * 
     * This lifecycle hook is invoked by NextionControl when the page is activated.
     * Override to perform setup tasks such as:
     * - Restoring cached state or scroll positions
     * - Refreshing UI elements with current data
     * - Starting timers or animations
     * - Initializing page-specific resources
     * 
     * @note The _isActive flag is automatically managed by NextionControl.
     * @note This is called after begin() on first activation, or directly on subsequent activations.
     */
    virtual void onEnterPage()
    {
        // Default: do nothing
    }

    /**
     * @brief Called when this page is about to be deactivated.
     * 
     * This lifecycle hook is invoked by NextionControl before switching to another page.
     * Override to perform cleanup tasks such as:
     * - Saving page state or user input
     * - Caching data to avoid re-fetching
     * - Stopping timers or animations
     * - Releasing page-specific resources
     * 
     * @note The _isActive flag is automatically managed by NextionControl.
     * @note Sending commands to Nextion components in this method may fail as the page is being deactivated.
     */
    virtual void onLeavePage()
    {
        // Default: do nothing
    }

    /**
     * @brief Refresh the page contents (called periodically by NextionControl).
     * 
     * Override to update dynamic content such as:
     * - Real-time sensor readings
     * - Clock/timer displays
     * - Status indicators
     * - Progress bars or gauges
     * 
     * @note Called at intervals defined by NextionControl::RefreshTime (default 1000ms).
     * @note Only called when this page is active.
     * @note Must be implemented by derived classes.
     */
    virtual void refresh(unsigned long now) = 0;

    /**
     * @brief Initialize the page (called once before first use).
     * 
     * Override to perform one-time initialization such as:
     * - Setting default component states
     * - Loading configuration
     * - Initializing internal data structures
     * 
     * @note Called automatically by NextionControl before first page activation.
     * @note Called only once per page lifetime.
     * @note Must be implemented by derived classes.
     */
    virtual void begin() = 0;

    /**
     * @brief Handle touch events from Nextion components.
     * 
     * Called when a component with a send component ID event is touched.
     * 
     * @param compId Component ID (set in Nextion Editor properties)
     * @param eventType Event type: 0x01 = press, 0x00 = release
     * @note Default implementation does nothing. Override to handle touch events.
     */
    virtual void handleTouch(uint8_t compId, uint8_t eventType)
    {
        (void)compId;
        (void)eventType;
    }

    /**
     * @brief Handle text return values from Nextion.
     * 
     * Called when a component sends text data (e.g., via get command or text input).
     * 
     * @param text The text string returned from the Nextion display
     * @note Default implementation does nothing. Override to handle text data.
     */
    virtual void handleText(const char* text)
    {
        (void)text;
    }

    /**
     * @brief Handle successful command execution responses.
     * 
     * Called when Nextion returns a success code (0x01) for a command.
     * 
     * @param responseCode Response code from Nextion (typically 0x01)
     * @note Default implementation does nothing. Override to track command success.
     */
    virtual void handleCommandResponse(uint8_t responseCode)
    {
        (void)responseCode;
    }

    /**
     * @brief Handle command execution error responses.
     * 
     * Called when Nextion returns an error code for a failed command.
     * 
     * @param responseCode Error code: 0x00 = invalid instruction, 0x02 = invalid component ID,
     *                     0x03 = invalid page ID, 0x04 = invalid picture ID,
     *                     0x1A = invalid variable, 0x1B = invalid operation, 0x1C = assignment failed
     * @note Default implementation does nothing. Override to handle errors.
     */
    virtual void handleErrorCommandResponse(uint8_t responseCode)
    {
        (void)responseCode;
    }

    /**
     * @brief Handle touch coordinate events.
     * 
     * Called when coordinate send is enabled and user touches the screen.
     * 
     * @param x X coordinate of touch (pixels)
     * @param y Y coordinate of touch (pixels)
     * @param eventType Event type: 0x67 = awake touch, 0x68 = sleep touch
     * @note Default implementation does nothing. Override for coordinate-based input.
     */
    virtual void handleTouchXY(uint16_t x, uint16_t y, uint8_t eventType)
    {
        (void)x;
        (void)y;
        (void)eventType;
    }

    /**
     * @brief Handle numeric return values from Nextion.
     * 
     * Called when a component sends numeric data (e.g., via get command).
     * 
     * @param value The 32-bit numeric value returned from the Nextion display
     * @note Default implementation does nothing. Override to handle numeric data.
     */
    virtual void handleNumeric(uint32_t value)
    {
        (void)value;
    }

    /**
     * @brief Handle sleep mode state changes.
     * 
     * Called when Nextion enters or exits auto-sleep mode.
     * 
     * @param entering true if entering sleep mode, false if waking from sleep
     * @note Default implementation does nothing. Override to respond to sleep events.
     */
    virtual void handleSleepChange(bool entering)
    {
        (void)entering;
    }

    /**
     * @brief Send a raw command to the Nextion display.
     * 
     * Automatically appends the required 0xFF 0xFF 0xFF terminator sequence.
     * Commands are only sent if this page is currently active.
     * 
     * @param cmd Command string (e.g., "t0.txt=\"Hello\"", "n0.val=42")
     * @note Commands sent from inactive pages are ignored (with debug warning).
     * @note Use setPage() if you need to send page change commands from inactive pages.
     */
    void sendCommand(const char* cmd)
    {
        if (!nextionSerialPort || !cmd)
            return;

        // Only send commands if this page is currently active
        if (!_isActive) {
#ifdef NEXTION_DEBUG
            Serial.print("Page ");
            Serial.print(getPageId());
            Serial.print(" ignoring command (not active): ");
            Serial.println(cmd);
#endif
            return;
        }

        nextionSerialPort->print(cmd);
        endCommand();
    }

    /**
     * @brief Set a property value on a Nextion component.
     * 
     * Convenience method for setting numeric component properties.
     * 
     * @param component Component name (e.g., "b0", "t1")
     * @param property Property name (e.g., "val", "pic", "font")
     * @param value Numeric value to assign
     * @note Only sends if page is active.
     */
    void setComponentProperty(const char* component, const char* property, int value)
    {
        if (!nextionSerialPort || !component || !property)
            return;

        if (!_isActive)
            return;

        // Stream directly: component.property=value
        nextionSerialPort->print(component);
        nextionSerialPort->print('.');
        nextionSerialPort->print(property);
        nextionSerialPort->print('=');
        nextionSerialPort->print(value);
        endCommand();
    }

    /**
     * @brief Switch to a different page on the Nextion display.
     * 
     * Sends a page change command to the display.
     * 
     * @param pageId Target page ID (0-255) to switch to
     * @note Unlike other commands, page changes are allowed from any page (active or not).
     * @note This bypasses the normal _isActive check to allow page navigation.
     */
    void setPage(const uint8_t pageId)
    {
        if (!nextionSerialPort)
            return;
        
        // Always allow page change commands regardless of active state
        char pageCommand[15];
		snprintf(pageCommand, sizeof(pageCommand), "page %d", pageId);
        nextionSerialPort->print(pageCommand);
        endCommand();
	}

    /**
     * @brief Set the primary picture attribute of a component.
     * 
     * @param component Component name (e.g., "p0" for a picture box)
     * @param pictureId Picture resource ID from Nextion Editor
     */
    void setPicture(const char* component, int pictureId)
    {
        setComponentProperty(component, "pic", pictureId);
    }

    /**
     * @brief Set the secondary picture attribute of a component (e.g., button pressed state).
     * 
     * @param component Component name
     * @param pictureId Picture resource ID from Nextion Editor
     */
    void setPicture2(const char* component, int pictureId)
    {
        setComponentProperty(component, "pic2", pictureId);
    }

    /**
     * @brief Set the font attribute of a text component.
     * 
     * @param component Component name (e.g., "t0" for a text field)
     * @param fontId Font resource ID from Nextion Editor
     */
    void setFont(const char* component, int fontId)
    {
        setComponentProperty(component, "font", fontId);
    }

    /**
     * @brief Set a numeric value on a component.
     * 
     * Convenience method for setting the value attribute (typically used with
     * number components, sliders, progress bars, etc.).
     * 
     * @param component Component name
     * @param value Numeric value to assign
     */
    void sendValue(const char* component, int value)
    {
        if (!nextionSerialPort)
            return;

        if (!component)
            return;

        if (!_isActive)
            return;

        // Stream directly: component=value
        nextionSerialPort->print(component);
        nextionSerialPort->print('=');
        nextionSerialPort->print(value);
        endCommand();
    }

    /**
     * @brief Set the text attribute of a component.
     * 
     * Convenience method for updating text in text fields, buttons, etc.
     * 
     * @param component Component name (e.g., "t0")
     * @param text Text string to display
     * @note Text is automatically quoted in the command.
     */
    void sendText(const char* component, const char* text)
    {
        if (!nextionSerialPort || !component || !text)
            return;

        if (!_isActive)
            return;

        // Stream directly: component.txt="text"
        nextionSerialPort->print(component);
        nextionSerialPort->print(F(".txt=\""));
        nextionSerialPort->print(text);
        nextionSerialPort->print('"');
        endCommand();
    }

private:
    Stream* nextionSerialPort;
    
    bool _initialized;
    bool _isActive;

    void endCommand()
    {
        if (!nextionSerialPort)
            return;
        nextionSerialPort->write(0xFF);
        nextionSerialPort->write(0xFF);
		nextionSerialPort->write(0xFF);
    }
    
    friend class NextionControl;  // Allow NextionControl to access _initialized and _isActive
};
