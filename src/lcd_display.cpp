#include "lcd_display.h"

#include <LiquidCrystal_I2C.h>

#include "config.h"

// ============================================================
//  I2C character display
//
//  Every screen in the firmware comes through here, which is what
//  makes the padding rule worth having: a row is always written as
//  exactly LCD_COLUMNS characters, so a shorter line cannot leave
//  the tail of the previous screen standing behind it. The display
//  wraps a row that is too long onto the next one by itself, so a
//  line is clipped here rather than allowed to bleed into the line
//  below.
// ============================================================

namespace
{
LiquidCrystal_I2C g_lcd(LCD_I2C_ADDRESS, LCD_COLUMNS, LCD_ROWS);
} // namespace

void lcdDisplayInit()
{
    g_lcd.init();
    g_lcd.backlight();
}

void lcdDisplayWrite(uint8_t row, const String& text)
{
    char line[LCD_COLUMNS + 1];

    const size_t length = min(static_cast<size_t>(text.length()), static_cast<size_t>(LCD_COLUMNS));
    memcpy(line, text.c_str(), length);
    memset(line + length, ' ', LCD_COLUMNS - length);
    line[LCD_COLUMNS] = '\0';

    g_lcd.setCursor(0, row);
    g_lcd.print(line);
}

void lcdDisplayShow(const String& firstLine, const String& secondLine)
{
    lcdDisplayClear();
    lcdDisplayWrite(0, firstLine);
    lcdDisplayWrite(1, secondLine);
}

void lcdDisplayClear()
{
    g_lcd.clear();
}
