# NextionControl

High-level controller for Nextion HMI displays on Arduino-compatible boards. It manages serial communication, page lifecycle, and event routing to page classes derived from `BaseDisplayPage`.

- C++14 compatible
- Works with any `Stream` (e.g., `Serial`, `Serial1`, `SoftwareSerial`, `HardwareSerial`)
- Periodic page refresh with configurable interval
- Event routing for touch, text, numeric, sleep/wake, and command responses
- Simple page model via `BaseDisplayPage`

## Requirements
- Arduino framework (or compatible environment)
- A Nextion HMI display (Basic/Enhanced/Intelligent)
- A serial port connected to the display (3-wire: TX, RX, GND)

## Installation
- As a project component: copy `NextionControl.h/.cpp` and `BaseDisplayPage.h` into your project.
- As a library: place headers/sources under `src/` in a library folder and add an `examples/` folder. Then include with `#include <NextionControl.h>`.

## Quick start
1) Connect your Nextion display to a serial port.
2) Create a page class deriving from `BaseDisplayPage` and implement `getPageId()`, `begin()`, and `refresh()`.
3) Create a `NextionControl` with your `Stream*` and array of pages.
4) Call `begin()` in `setup()` and `update(millis())` in `loop()`.

See a complete sketch in `examples/BasicUsage/BasicUsage.ino`.

## Page model (`BaseDisplayPage`)
Implement the following in your page class:
- `uint8_t getPageId() const` – Return the Nextion page id (as configured in the HMI editor).
- `void begin()` – Initialize widgets or send initial commands.
- `void refresh()` – Periodic UI updates (called every `RefreshTime`).

Optional handlers you can override:
- `handleTouch(uint8_t compId, uint8_t eventType)` – Component touch press/release.
- `handleTouchXY(uint16_t x, uint16_t y, uint8_t eventType)` – Raw XY touch events (if enabled on HMI).
- `handleText(String text)` – Text return values.
- `handleNumeric(uint32_t value)` – Numeric return values.
- `handleCommandResponse(uint8_t responseCode)` / `handleErrorCommandResponse(uint8_t responseCode)` – Command ack/error codes.
- `handleSleepChange(bool entering)` – Sleep/wake notifications.
- `handleExternalUpdate(uint8_t updateType, const void* data)` – Push domain updates from your app (see `docs/ExternalUpdatePattern.md`).

Helpers for sending commands:
- `sendCommand(const String& cmd)` – Sends raw command plus 0xFF 0xFF 0xFF terminators.
- `sendText(component, text)`, `sendValue(component, value)`, `setPicture(component, id)`, etc.

## Controller (`NextionControl`)
Constructor:
- `NextionControl(Stream* serial, BaseDisplayPage** pages, size_t count)`

Main API:
- `bool begin()` – Initializes the display and first page.
- `void update(unsigned long now)` – Call frequently to process serial and refresh pages.
- `void sendCommand(const String& cmd)` – Send a raw command.
- `void refreshCurrentPage()` – Force an immediate page refresh.
- `BaseDisplayPage* getCurrentPage() const` – Access current page.

Constants:
- `RefreshTime` – Interval between `refresh()` calls (ms).
- `SerialBufferSize` – Input buffer size.
- `SerialTimeout` – Timeout to discard stalled partial messages.
- `EventPress`, `EventRelease` – Touch event codes.

## Nextion HMI notes
- Ensure components use consistent ids with your page code.
- If using component touch events, configure `Send Component ID` in HMI editor.
- For text/numeric responses, use `print`/`printh`/`get` commands as appropriate.

## Example
- See `examples/BasicUsage/BasicUsage.ino` for a minimal compile-ready sketch showing one page.
- See [SmartFuseBox](https://github.com/k3ldar/SmartFuseBox) for a real world (in progress) example.

## Troubleshooting
- No response: check wiring, baud rate, and that `update(millis())` runs frequently.
- Garbled data: confirm common GND and matching baud rates.
- No touch events: enable `Send Component ID` in HMI for affected components.

## License
[GPL-3](https://github.com/k3ldar/NextionControl/tree/main?tab=GPL-3.0-1-ov-file#readme)
