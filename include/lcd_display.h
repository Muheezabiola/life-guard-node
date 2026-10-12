#pragma once

#include <Arduino.h>

// I2C character display (16x2, PCF8574 backpack) behind one set of
// calls, so a screen is drawn through the same padding rule no
// matter which mode asked for it.
void lcdDisplayInit();

// Clears both rows and writes the two lines, padding each to the
// full width so nothing from the previous screen shows through.
void lcdDisplayShow(const String& firstLine, const String& secondLine);

// Writes one row, padded to LCD_COLUMNS and clipped to the row.
void lcdDisplayWrite(uint8_t row, const String& text);

// Clears both rows.
void lcdDisplayClear();
