#include "touch/axs5106l.h"

#include <Arduino.h>
#include <Wire.h>

#include "config.h"

// Register map, from Waveshare's driver (there's no public datasheet).
static const uint8_t REG_TOUCH_DATA = 0x01;  // [1] = point count, then 6 bytes per point
static const uint8_t REG_CHIP_ID = 0x08;

// If no new report arrives for this long, treat the finger as lifted. (The
// controller doesn't always send a final "no touch" report.)
static const unsigned long RELEASE_TIMEOUT_MS = 60;

static const unsigned long ERROR_LOG_INTERVAL_MS = 5000;

// Set from the interrupt handler, cleared when we read the report. `volatile`
// because it changes outside normal program flow; IRAM_ATTR keeps the handler
// in RAM so it can run while flash is busy.
static volatile bool reportReady = false;
static volatile uint32_t diagInterrupts = 0;  // DIAG

static void IRAM_ATTR onTouchInterrupt() {
  reportReady = true;
  diagInterrupts = diagInterrupts + 1;  // DIAG
}

bool Axs5106l::begin(uint8_t rotation) {
  rotation_ = rotation;

  Wire.begin(TOUCH_SDA_PIN, TOUCH_SCL_PIN, 400000);

  // Hardware reset. The controller needs a moment to boot afterwards.
  pinMode(TOUCH_RST_PIN, OUTPUT);
  digitalWrite(TOUCH_RST_PIN, LOW);
  delay(20);
  digitalWrite(TOUCH_RST_PIN, HIGH);
  delay(300);

  pinMode(TOUCH_INT_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(TOUCH_INT_PIN), onTouchInterrupt, FALLING);

  uint8_t id[3] = {0};
  if (!readRegister(REG_CHIP_ID, id, sizeof(id))) {
    Serial.println("Touch: AXS5106L not responding on I2C");
    return false;
  }
  Serial.printf("Touch: AXS5106L id %02X %02X %02X\n", id[0], id[1], id[2]);
  return true;
}

bool Axs5106l::read(int32_t& x, int32_t& y) {
  const unsigned long now = millis();

  if (reportReady) {
    reportReady = false;

    // Point 1 is bytes 2-5; we only need one finger, so 6 bytes is enough.
    uint8_t data[6] = {0};
    if (readRegister(REG_TOUCH_DATA, data, sizeof(data))) {
      lastReportMs_ = now;
      const bool wasPressed = pressed_;
      pressed_ = (data[1] & 0x0F) > 0;

      if (pressed_) {
        // 12-bit coordinates, in the panel's native portrait orientation.
        const int32_t rawX = ((data[2] & 0x0F) << 8) | data[3];
        const int32_t rawY = ((data[4] & 0x0F) << 8) | data[5];

        // The touch layer's X axis runs opposite to the display's, hence the
        // mirroring even at rotation 0.
        const int32_t maxX = TFT_PANEL_WIDTH - 1;
        const int32_t maxY = TFT_PANEL_HEIGHT - 1;
        switch (rotation_) {
          case 1:  x_ = rawY;        y_ = rawX;        break;
          case 2:  x_ = rawX;        y_ = maxY - rawY; break;
          case 3:  x_ = maxY - rawY; y_ = maxX - rawX; break;
          default: x_ = maxX - rawX; y_ = rawY;        break;
        }
      }

      // DIAG
      diagReports_++;
      if (pressed_ && !wasPressed) {
        diagDownMs_ = now;
        diagReports_ = 1;
        Serial.printf("[diag] down %d,%d\n", (int)x_, (int)y_);
      } else if (!pressed_ && wasPressed) {
        Serial.printf("[diag] up (release report) after %lums, %u reports, %u interrupts total\n",
                      now - diagDownMs_, (unsigned)diagReports_, (unsigned)diagInterrupts);
      }
    } else if (now - lastErrorLogMs_ >= ERROR_LOG_INTERVAL_MS) {
      lastErrorLogMs_ = now;
      Serial.println("Touch: I2C read failed");
    }
  }

  if (pressed_ && now - lastReportMs_ > RELEASE_TIMEOUT_MS) {
    pressed_ = false;
    // DIAG
    Serial.printf("[diag] up (timeout) after %lums, %u reports, %u interrupts total\n",
                  now - diagDownMs_, (unsigned)diagReports_, (unsigned)diagInterrupts);
  }

  if (pressed_) {
    x = x_;
    y = y_;
  }
  return pressed_;
}

// DIAG
void Axs5106l::probe() {
  uint8_t id[3] = {0};
  const bool ok = readRegister(REG_CHIP_ID, id, sizeof(id));
  Serial.printf("[diag] probe at t=%lus, %lus since last touch report: %s\n", millis() / 1000,
                (millis() - lastReportMs_) / 1000, ok ? "answered" : "NO ANSWER");
}

bool Axs5106l::readRegister(uint8_t reg, uint8_t* data, uint8_t len) {
  Wire.beginTransmission(TOUCH_I2C_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission() != 0) return false;

  if (Wire.requestFrom(static_cast<uint8_t>(TOUCH_I2C_ADDR), len) != len) return false;
  Wire.readBytes(data, len);
  return true;
}
