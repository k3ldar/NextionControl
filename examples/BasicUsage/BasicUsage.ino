// Basic example using NextionControl with a single page.
// Hardware: Connect Nextion TX->RX, RX->TX, and GND.
// Adjust serial port as needed (e.g., Serial1/Serial2) and baud rate.

#include <Arduino.h>
#include <NextionControl.h>
#include "BaseDisplayPage.h"

// Simple page showing time since boot in seconds on a Nextion text component t0
class SimplePage : public BaseDisplayPage {
public:
  explicit SimplePage(Stream* s) : BaseDisplayPage(s) {}

  uint8_t getPageId() const override { return 0; }

  void begin() override {
    // Set initial text and font (assuming component t0 exists)
    sendText("t0", "Booting...");
  }

  void refresh() override {
    // Update t0 with seconds since boot
    unsigned long sec = millis() / 1000UL;
    sendText("t0", String("Uptime: ") + String(sec) + "s");
  }

  void handleTouch(uint8_t compId, uint8_t eventType) override {
    // Example: react to a button with id 1
    if (compId == 1 && eventType == EventPress) {
      sendText("t0", "Button pressed");
    }
  }
};

// Choose the serial port connected to your Nextion
#if defined(ARDUINO_AVR_UNO)
  #define NEXTION_SERIAL Serial
#elif defined(ARDUINO_AVR_MEGA2560)
  #define NEXTION_SERIAL Serial1
#else
  #define NEXTION_SERIAL Serial1
#endif

SimplePage page0(&NEXTION_SERIAL);
BaseDisplayPage* pages[] = { &page0 };
NextionControl display(&NEXTION_SERIAL, pages, sizeof(pages)/sizeof(pages[0]));

void setup() {
  NEXTION_SERIAL.begin(9600);
  delay(50);

  // Initialize controller and current page
  display.begin();
}

void loop() {
  display.update(millis());
}
