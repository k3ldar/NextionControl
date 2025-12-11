#include "NextionControl.h"

NextionControl::NextionControl(Stream* serialPort, BaseDisplayPage** pageArray, size_t count)
    : nextionSerialPort(serialPort),
      pageCount(count),
      pages(new BaseDisplayPage*[count]),
      currPage(0),
      refreshTimer(0),
      currentPage(nullptr)
{
    nextionSerialPort = serialPort;
    pageCount = count;

    for (size_t i = 0; i < pageCount; i++)
    {
        pages[i] = pageArray[i];
    }

    // Set the initial page as active
    if (pageCount > 0 && pages[0]) {
        currentPage = pages[0];
        currentPage->_isActive = true;
    }
}

NextionControl::~NextionControl()
{
    delete[] pages;
    pages = nullptr;
}

#ifdef NEXTION_DEBUG
void NextionControl::setDebugCallback(DebugCallback callback)
{
    debugCallback = callback;
}

void NextionControl::debugLog(const String& message)
{
    if (debugCallback)
    {
        debugCallback(message);
    }
}
#endif

bool NextionControl::begin()
{
    // Initialize the first page
    if (currentPage && !currentPage->_initialized)
    {
        currentPage->begin();
        currentPage->_initialized = true;
        
#ifdef NEXTION_DEBUG
        debugLog(String(F("NextionControl: Called begin() on page ID ")) + String(currentPage->getPageId()) + String(F(" (marked as active)")));
#endif
    }
    
    // Request the actual current page from the display to ensure synchronization
    requestCurrentPage();
    
#ifdef NEXTION_DEBUG
    debugLog(String(F("NextionControl initialized. Waiting for Nextion page events...")));
#endif
    return true;
}

void NextionControl::update(unsigned long now)
{
    readSerial(now);
    
    // Optional periodic updates (for other text fields, numbers, etc.)
    if (currentPage && (now - refreshTimer) > RefreshTime)
    {
        currentPage->refresh(now);
        refreshTimer = now;
    }
}

void NextionControl::sendCommand(const String& cmd)
{
#ifdef NEXTION_DEBUG
    debugLog(String(F("Sending Command:")) + cmd);
#endif
    nextionSerialPort->print(cmd);
    nextionSerialPort->write(0xFF);
    nextionSerialPort->write(0xFF);
    nextionSerialPort->write(0xFF);
}

void NextionControl::readSerial(unsigned long now)
{
    while (nextionSerialPort->available() > 0)
    {
        uint8_t b = nextionSerialPort->read();
        
#ifdef NEXTION_DEBUG
        debugLog(String(F("RX: 0x")) + String(b, HEX) + String(F(" (START of message)")));
#endif
        
        _lastCharTime = millis();

        if (!_readingMessage)
        {
            _terminatorCount = 0;

            // Skip leading 0xFF bytes (these are just noise/incomplete terminators)
            if (b == 0xFF) {
#ifdef NEXTION_DEBUG
                debugLog(String(F("RX: 0x")) + String(b, HEX) + String(F(" (skipped - leading 0xFF)")));
#endif
                continue;
            }

            _readingMessage = true;
            _serialBufferPos = 0;
#ifdef NEXTION_DEBUG
            debugLog(String(F("RX: 0x")) + String(b, HEX) + String(F(" (START of message)")));
#endif
        }

        if (_serialBufferPos < SerialBufferSize)
        {
            _serialBuffer[_serialBufferPos++] = b;
        }
        else
        {
#ifdef NEXTION_DEBUG
            debugLog(String(F("ERROR: Serial buffer overflow!")));
#endif
            _readingMessage = false;
            _serialBufferPos = 0;
            _terminatorCount = 0;
            break;
        }

        if (b == 0xFF)
            _terminatorCount++;
        else
            _terminatorCount = 0;

        if (_terminatorCount >= 3)
        {
            size_t msgLen = _serialBufferPos - 3; // exclude terminator
            
#ifdef NEXTION_DEBUG
            debugLog(String(F("Complete message assembled: ")) + String(msgLen) + String(F(" bytes")));
#endif
            
            handleNextionMessage(_serialBuffer, msgLen);
            
            _serialBufferPos = 0;
            _terminatorCount = 0;
            _readingMessage = false;
        }
    }

    // Timeout handling for incomplete messages
    if (_readingMessage && (now - _lastCharTime > SerialTimeout))
    {
#ifdef NEXTION_DEBUG
        debugLog(String(F("TIMEOUT: Abandoning incomplete message (")) + String(_serialBufferPos) + String(F(" bytes received)")));
#endif
        _readingMessage = false;
        _terminatorCount = 0;
        _serialBufferPos = 0;
        requestCurrentPage();
    }
}

void NextionControl::handleNextionMessage(const uint8_t* data, size_t len)
{
    if (len == 0)
        return;

    uint8_t cmd = data[0];

#ifdef NEXTION_DEBUG
    String hexData = String(F("Nextion MSG: 0x")) + String(cmd, HEX) + String(F(" len=")) + String(len) + String(F(" data=["));
    for (size_t i = 0; i < len; i++) {
        if (i > 0) hexData += String(F(" "));
        if (data[i] < 0x10) hexData += String(F("0"));
        hexData += String(data[i], HEX);
    }
    hexData += String(F("]"));
    debugLog(hexData);
#endif

    switch (cmd)
    {
        case 0x01: // Instruction successful
        {
#ifdef NEXTION_DEBUG
            debugLog(String(F("  -> Instruction successful")));
#endif
            if (currentPage)
                currentPage->handleCommandResponse(cmd);

            break;
        }

        case 0x00: // Invalid instruction
        case 0x02: // Invalid component ID
        case 0x03: // Invalid page ID
        case 0x04: // Invalid picture ID
        case 0x1A: // Invalid variable name/attribute
        case 0x1B: // Invalid variable operation
        case 0x1C: // Assignment failed
        {
#ifdef NEXTION_DEBUG
            debugLog(String(F("  -> Error code: 0x")) + String(cmd, HEX));
#endif
            // Forward command execution results to current page
            if (currentPage)
                currentPage->handleErrorCommandResponse(cmd);

            break;
        }

        case 0x65: // default touch event
        { 
            // Touch event requires at least 4 bytes: [65 pageId compId eventType]
            if (len < 4) {
#ifdef NEXTION_DEBUG
                debugLog(String(F("  -> Touch FAILED: message too short")));
#endif
                return;
            }
            
            uint8_t pageId = data[1];
            uint8_t compId = data[2];
            uint8_t eventType = data[3];
            
#ifdef NEXTION_DEBUG
            debugLog(String(F("  -> Touch: page=")) + String(pageId) +
                String(F(" comp=")) + String(compId) +
                    String(F(" event=")) + String(eventType == 1 ? String(F("PRESS")) : String(F("RELEASE"))));
#endif

            // Defensive synchronization: If touch event is for a different page than our current page,
            // the Nextion display must have changed pages (either we missed a 0x66 event, or the display
            // was manually navigated). Synchronize our internal state with the display's actual state.
            if (!currentPage || currentPage->getPageId() != pageId) {
#ifdef NEXTION_DEBUG
                debugLog(String(F("  -> SYNC: Touch event indicates page mismatch")));
#endif
                switchToPageById(pageId);
            }

            // Now handle the touch event (currentPage should be synchronized)
            if (currentPage && currentPage->getPageId() == pageId) {
                currentPage->handleTouch(compId, eventType);
            }
#ifdef NEXTION_DEBUG
            else {
                debugLog(String(F("  -> Touch IGNORED (page still mismatched after sync attempt)")));
            }
#endif

            break;
        }

        case 0x66: // Page change (sendme or page change event)
        {
            if (len < 2) {
#ifdef NEXTION_DEBUG
                debugLog(String(F("  -> Page change FAILED: message too short")));
#endif
                return;
            }
            
            // Extract page ID from data[1], ignore any extra bytes
            uint8_t newPageId = data[1];
            
#ifdef NEXTION_DEBUG
            if (len > 2) {
                debugLog(String(F("  -> Page change to: ")) + String(newPageId));
            }
            
            debugLog(String(F("  -> Page change to: ")) + String(newPageId));
#endif
            
            // Use centralized page switching logic
            switchToPageById(newPageId);

            break;
        }

        case 0x67: // Touch coordinate (awake)
        case 0x68: // Touch coordinate (sleep)
        {
            if (len < 6)
                return;

            uint16_t x = (data[1] << 8) | data[2];
            uint16_t y = (data[3] << 8) | data[4];
            uint8_t eventType = data[5];

#ifdef NEXTION_DEBUG
            debugLog(String(F("  -> Touch XY: x=")) + String(x) +
                String(F(" y=")) + String(y) +
                    String(F(" event=")) + String(eventType));
#endif

            if (currentPage)
                currentPage->handleTouchXY(x, y, eventType);

            break;
        }

        case 0x70: // String return
        { 
            // Build null-terminated C string directly from data buffer
            char textBuffer[SerialBufferSize];
            size_t textLen = len - 1; // Exclude the command byte

            // Copy text data (skip first byte which is 0x70)
            for (size_t i = 0; i < textLen && i < (SerialBufferSize - 1); ++i)
                textBuffer[i] = (char)data[i + 1];

            // Null-terminate
            textBuffer[textLen < SerialBufferSize ? textLen : SerialBufferSize - 1] = '\0';

#ifdef NEXTION_DEBUG
			debugLog(String(F("  -> String: \"")) + text + String(F("\"")));
#endif

            if (currentPage)
                currentPage->handleText(textBuffer);

            break;
        }

        case 0x71: // Numeric data
        {
            if (len < 5)
                return;

            // Convert 4 bytes to 32-bit value (little endian)
            uint32_t value = data[1] |
                (data[2] << 8) |
                (data[3] << 16) |
                (data[4] << 24);

#ifdef NEXTION_DEBUG
			debugLog(String(F("  -> Numeric: ")) + String(value));
#endif

            if (currentPage)
                currentPage->handleNumeric(value);

            break;
        }

        case 0x86: // Auto sleep mode entered
        case 0x87: // Auto wake from sleep
        {
#ifdef NEXTION_DEBUG
			debugLog(String(F("Sleep change: ")) + String(cmd == 0x86 ? String(F("Sleep mode entered")) : String(F("Wake from sleep"))));
#endif

            if (currentPage)
                currentPage->handleSleepChange(cmd == 0x86);

            break;
        }

        default: // Unhandled Nextion cmd
#ifdef NEXTION_DEBUG
			debugLog(String(F("  -> Unhandled Nextion command: 0x")) + String(cmd, HEX));
#endif
            break;
    }
}

bool NextionControl::switchToPageById(uint8_t pageId)
{
    // Find the page with matching ID
    BaseDisplayPage* newPage = nullptr;
    for (size_t i = 0; i < pageCount; i++) {
        if (pages[i]->getPageId() == pageId) {
            newPage = pages[i];
            break;
        }
    }
    
    // Page not found
    if (!newPage) {
#ifdef NEXTION_DEBUG
		debugLog(String(F("  -> Page ID ")) + String(pageId) + String(F(" not found in registered pages!")));
#endif
        return false;
    }
    
    // Already on this page
    if (newPage == currentPage) {
#ifdef NEXTION_DEBUG
		debugLog(String(F("  -> Already on page ")) + String(pageId));
#endif
        return true;
    }
    
    // Switch pages
#ifdef NEXTION_DEBUG
	debugLog(String(F("  -> Switching from page ")) + String(currentPage ? currentPage->getPageId() : 255) + String(F("  -> to page ID ")) + String(pageId));
#endif
    
    // Deactivate the old page
    if (currentPage) {
        currentPage->onLeavePage();
        currentPage->_isActive = false;
#ifdef NEXTION_DEBUG
		debugLog(String(F("  -> Deactivated page ")) + String(currentPage->getPageId()));
#endif
    }
    
    // Activate the new page
    currentPage = newPage;
    currentPage->_isActive = true;
	currentPage->onEnterPage();
    
#ifdef NEXTION_DEBUG
	debugLog(String(F("  -> onEnterPage() called for page ")) + String(currentPage->getPageId()));
#endif
    
    // Initialize the newly activated page (only if not already initialized)
    if (!currentPage->_initialized) {
#ifdef NEXTION_DEBUG
        debugLog(String(F("  -> Calling begin() on new page (first time)")));
#endif
        currentPage->begin();
        currentPage->_initialized = true;
    }
#ifdef NEXTION_DEBUG
    else {
        debugLog(String(F("  -> Page already initialized")));
    }
#endif
    
    return true;
}

void NextionControl::refreshCurrentPage()
{
    if (currentPage)
        currentPage->refresh(millis());
}

void NextionControl::requestCurrentPage()
{
    // Send "sendme" command - Nextion will respond with 0x66 page change message
    sendCommand(String(F("sendme")));
    
#ifdef NEXTION_DEBUG
    debugLog(String(F("NextionControl: Requested current page from display (sendme)")));
#endif
}
