#pragma once

// forward declaration
class NextionControl;


// Abstract base class for a display page
class BaseDisplayPage {
	friend class NextionControl;

public:
    // Destructor (virtual to allow proper cleanup in derived classes)
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
    virtual void handleExternalUpdate(uint8_t updateType, const void* data) {
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
    explicit BaseDisplayPage(Stream* serialPort) 
        : nextionSerialPort(serialPort), 
          _initialized(false),
          _isActive(false) {}

    // Return the page identifier
    virtual uint8_t getPageId() const = 0;

    // Refresh the page contents (called periodically)
    virtual void refresh() = 0;

    // Initialization if any
    virtual void begin() = 0;

    // Touch events
    virtual void handleTouch(uint8_t compId, uint8_t eventType) {
        (void)compId;
        (void)eventType;
    }

    virtual void handleText(String text) {
        (void)text;
    }

    // Handle command execution responses
    virtual void handleCommandResponse(uint8_t responseCode) {
        (void)responseCode;
    }

    // Handle command execution responses
    virtual void handleErrorCommandResponse(uint8_t responseCode) {
        (void)responseCode;
    }

    // Handle XY touch coordinates
    virtual void handleTouchXY(uint16_t x, uint16_t y, uint8_t eventType) {
        (void)x;
        (void)y;
        (void)eventType;
    }

    // Handle numeric return values
    virtual void handleNumeric(uint32_t value) {
        (void)value;
    }

    // Handle sleep mode changes
    virtual void handleSleepChange(bool entering) {
        (void)entering;
    }

    // Helper for sending commands to the Nextion
    void sendCommand(const String& cmd) {
        if (!nextionSerialPort)
            return;

        // Only send commands if this page is currently active
        if (!_isActive) {
#ifdef NEXTION_DEBUG
            Serial.print("Page ");
            Serial.print(getPageId());
            Serial.println(" ignoring command (not active): " + cmd);
#endif
            return;
        }

        nextionSerialPort->print(cmd);
        nextionSerialPort->write(0xFF);
        nextionSerialPort->write(0xFF);
        nextionSerialPort->write(0xFF);
    }

    // Centralized command sender for property assignment
    void setComponentProperty(const String& component, const String& property, int value) {
        if (!nextionSerialPort)
            return;
        String cmd = component + "." + property + "=" + String(value);
        sendCommand(cmd);
    }

    // Wrappers for common properties
    void setPage(const uint8_t pageId) {
        if (!nextionSerialPort)
            return;
        
        // Always allow page change commands regardless of active state
        String cmd = "page " + String(pageId);
        nextionSerialPort->print(cmd);
        nextionSerialPort->write(0xFF);
        nextionSerialPort->write(0xFF);
        nextionSerialPort->write(0xFF);
	}

    void setPicture(const String& component, int pictureId) {
        setComponentProperty(component, "pic", pictureId);
    }

    void setPicture2(const String& component, int pictureId) {
        setComponentProperty(component, "pic2", pictureId);
    }

    void setFont(const String& component, int fontId) {
        setComponentProperty(component, "font", fontId);
    }

    // Optional helper: send numeric assignment, common in Nextion pages
    void sendValue(const String& component, int value) {
        if (!nextionSerialPort)
            return;

        String cmd = component + "=" + String(value);
        sendCommand(cmd);
    }

    // Optional helper: set text field
    void sendText(const String& component, const String& text) {
        if (!nextionSerialPort)
            return;

        String cmd = component + ".txt=\"" + text + "\"";
        sendCommand(cmd);
    }

private:
    Stream* nextionSerialPort;
    bool _initialized;  // Track if begin() has been called for this page
    bool _isActive;     // Track if this page is currently displayed on Nextion
    
    friend class NextionControl;  // Allow NextionControl to access _initialized and _isActive
};
