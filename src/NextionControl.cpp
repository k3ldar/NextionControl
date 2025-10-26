
#include "NextionControl.h"

NextionControl::NextionControl(Stream* serialPort, BaseDisplayPage** pageArray, size_t count)
    : nextionSerialPort(serialPort),
      pageCount(count),
      pages(new BaseDisplayPage*[count]),
      currPage(0),
      refreshTimer(0)
{
    nextionSerialPort = serialPort;
    pageCount = count;

    for (size_t i = 0; i < pageCount; i++)
    {
        pages[i] = pageArray[i];
    }

    currentPage = pages[0];
}

NextionControl::~NextionControl()
{
    delete[] pages;
    pages = nullptr;
}

bool NextionControl::begin()
{
    for (size_t i = 0; i < pageCount; i++)
    {
        pages[i]->begin();
    }

    return true;
}

void NextionControl::update(unsigned long now)
{
    readSerial(now);
    
    // Optional periodic updates (for other text fields, numbers, etc.)
    if (currentPage && (now - refreshTimer) > RefreshTime)
    {
        currentPage->refresh();
        refreshTimer = now;
    }
}

void NextionControl::sendCommand(const String& cmd)
{
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
        _lastCharTime = millis();

        if (!_readingMessage)
        {
            _terminatorCount = 0;

            if (b == 0xFF)
                continue;

            _readingMessage = true;
            _serialBufferPos = 0;
        }

        if (_serialBufferPos < SerialBufferSize)
        {
            _serialBuffer[_serialBufferPos++] = b;
        }
        else
        {
			_readingMessage = false;
            return;
        }

        if (b == 0xFF)
            _terminatorCount++;
        else
            _terminatorCount = 0;

        if (_terminatorCount >=3)
        {
            size_t msgLen = _serialBufferPos - 3; // exclude terminator
            handleNextionMessage(_serialBuffer, msgLen);
            _serialBufferPos = 0;
            _terminatorCount = 0;
            _readingMessage = false;
            break;
        }

        if (_serialBufferPos >= SerialBufferSize)
        {
            _readingMessage = false;
            return;
        }
    }

    if (_readingMessage && (now - _lastCharTime > SerialTimeout))
    {
        _readingMessage = false;
        _terminatorCount = 0;
        _serialBufferPos = 0;
    }
}

void NextionControl::handleNextionMessage(const uint8_t* data, size_t len)
{
    if (len == 0)
        return;

    uint8_t cmd = data[0];

    switch (cmd)
    {
        case 0x01: // Instruction successful
        {
            if (currentPage)
				currentPage->handleCommandResponse(cmd);
        }

        case 0x00: // Invalid instruction
        case 0x02: // Invalid component ID
        case 0x03: // Invalid page ID
	    case 0x04: // Invalid picture ID
        case 0x1A: // Invalid variable name/attribute
        case 0x1B: // Invalid variable operation
        case 0x1C: // Assignment failed
        {
            // Forward command execution results to current page
            if (currentPage)
                currentPage->handleErrorCommandResponse(cmd);
            break;
        }

        case 0x65: // default touch event
        { 
            // Touch event
            if (len < 4)
                return;
            uint8_t pageId = data[1];
            uint8_t compId = data[2];
            uint8_t eventType = data[3];

            if (currentPage && currentPage->getPageId() == pageId)
                currentPage->handleTouch(compId, eventType);

            break;
        }

        case 0x66: // Page change (sendme or page change event)
        {
            if (len < 2)
                return;
            uint8_t newPageId = data[1];
            for (size_t i = 0; i < pageCount; i++) {
                if (pages[i]->getPageId() == newPageId) {
                    currentPage = pages[i];
                    break;
                }
            }
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

            if (currentPage)
                currentPage->handleTouchXY(x, y, eventType);
            break;
        }

        case 0x70: // String return
        { 
            String text;
            for (size_t i = 1; i < len; ++i)
                text += (char)data[i];

            if (currentPage)
                currentPage->handleText(text);

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

            if (currentPage)
                currentPage->handleNumeric(value);
            break;
        }

        case 0x86: // Auto sleep mode entered
        case 0x87: // Auto wake from sleep
        {
            if (currentPage)
                currentPage->handleSleepChange(cmd == 0x86);
            break;
        }

        default: // Unhandled Nextion cmd
            break;
    }
}

void NextionControl::setCurrentPage(size_t index)
{
    if (index < pageCount)
        currentPage = pages[index];
}

void NextionControl::refreshCurrentPage()
{
    if (currentPage)
        currentPage->refresh();
}
