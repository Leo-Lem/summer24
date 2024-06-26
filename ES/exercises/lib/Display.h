#include <SPI.h>
#include "DisplayBuffer.h"
#include "Font_6x8.h"
#include "Window.h"

#ifndef DISPLAY_H
#define DISPLAY_H

class Display
{
public:
  enum Command : uint8_t
  {
    NOP = 0x00,     // no Operation
    SWRESET = 0x01, // Software reset
    SLPOUT = 0x11,  // Sleep out & booster on
    DISPOFF = 0x28, // Display off
    DISPON = 0x29,  // Display on
    CASET = 0x2A,   // Column adress set
    RASET = 0x2B,   // Row adress set
    RAMWR = 0x2C,   // Memory write
    MADCTL = 0x36,  // Memory Data Access Control
    COLMOD = 0x3A,  // RGB-format 12/16/18bit
    INVOFF = 0x20,  // Display inversion off
    INVON = 0x21,   // Display inversion on
    INVCTR = 0xB4,  // Display Inversion mode control
    NORON = 0x13,   // Partial off (Normal)
    PWCRT1 = 0xC0,  // Power Control 1
    PWCRT2 = 0xC1,  // Power Control 2
    PWCRT3 = 0xC2,  // Power Control 3
    PWCRT4 = 0xC3,  // Power Control 4
    PWCRT5 = 0xC4,  // Power Control 5
    VMCTR1 = 0xC5   // VCOM Voltage setting
  };

  Window window;

  Display(int width, int height) : window(Window(width, height)),
                                   buffer(DisplayBuffer<COLOR_FORMAT>()){};

  void init()
  {
    pinMode(RST, OUTPUT);
    pinMode(DC, OUTPUT);
    pinMode(CS, OUTPUT);

    digitalWrite(RST, HIGH);
    digitalWrite(DC, HIGH);
    digitalWrite(CS, HIGH);

    SPI.begin();
    delay(100);

#ifdef DEBUG
    Serial.print("[SPI] Initialized with ");
    Serial.print(SPI_DEFAULT_FREQ, DEC);
    Serial.println("Hz.");

    int time = millis();
#endif

    // hardware reset
    digitalWrite(RST, HIGH);
    delay(1000);
    digitalWrite(RST, LOW);
    delay(1000);
    digitalWrite(RST, HIGH);
    delay(1000);

    beginTransaction();

    // software reset, turn off sleep mode, power control settings, VCOM voltage setting
    writeCommand(SWRESET);
    delay(1200);          // mandatory delay
    writeCommand(SLPOUT); // turn off sleep mode.
    delay(1200);
    writeCommand(PWCRT1);
    writeDatum(0xA2);
    writeDatum(0x02);
    writeDatum(0x84);
    writeCommand(PWCRT4);
    writeDatum(0x8A);
    writeDatum(0x2A);
    writeCommand(PWCRT5);
    writeDatum(0x8A);
    writeDatum(0xEE);
    writeCommand(VMCTR1);
    writeDatum(0x0E); // VCOM = -0.775V

    // Memory Data Access Control D7/D6/D5/D4/D3/D2/D1/D0
    //  MY/MX/MV/ML/RGB/MH/-/-
    //  MY- Row Address Order; ‘0’ =Increment, (Top to Bottom)
    //  MX- Column Address Order; ‘0’ =Increment, (Left to Right)
    //  MV- Row/Column Exchange; '0’ = Normal,
    //  ML- Scan Address Order; ‘0’ =Decrement,(LCD refresh Top to Bottom)
    //  RGB - '0'= RGB color fill order
    //  MH - '0'= LCD horizontal refresh left to right

    writeCommand(MADCTL);
    writeDatum(0x08);

    // RGB-format
    writeCommand(COLMOD);
    writeDatum(0x05); // 16-bit/pixel; high nibble don't care

    configureArea(window);

    writeCommand(NORON);
    writeCommand(DISPON);

    endTransaction();

#ifdef DEBUG
    time = millis() - time;
    Serial.print("[Display] Initialized in ");
    Serial.print(time, DEC);
    Serial.println("ms.");
#endif
  };

  void configureArea(Window area)
  {
    beginTransaction();
    buffer.init(area);
    writeCommand(NOP);
    writeCommand(CASET);
    writeDatum(area.xs() >> 8);
    writeDatum(area.xs() & 0xFF);
    writeDatum(area.xe() - 1 >> 8);
    writeDatum(area.xe() - 1 & 0xFF);
    writeCommand(RASET);
    writeDatum(area.ys() >> 8);
    writeDatum(area.ys() & 0xFF);
    writeDatum(area.ye() - 1 >> 8);
    writeDatum(area.ye() - 1 & 0xFF);
    endTransaction();

#ifdef DEBUG
    Serial.print("[Display] Configured drawing area x(");
    Serial.print(area.xs());
    Serial.print(" to ");
    Serial.print(area.xe());
    Serial.print("), y(");
    Serial.print(area.ys());
    Serial.print(" to ");
    Serial.print(area.ye());
    Serial.println(").");
#endif
  };
  void setPixel(int x, int y, Color color)
  {
    buffer.set(x, y, color);

#ifdef DEBUG
    Serial.print("[Display] Pixel in buffer at ");
    Serial.print(x);
    Serial.print(", ");
    Serial.print(y);
    Serial.print(" set to color #");
    Serial.print((uint16_t)color, HEX);
    Serial.println(" (RGB565).");
#endif
  };
  void drawPixels()
  {
    beginTransaction();
    writeCommand(RAMWR);
    COLOR_FORMAT *colors = buffer.getBuffer();

    for (int i = 0; i < buffer.size(); i++)
    {
      writeDatum((uint16_t)colors[i] >> 8);
      writeDatum((uint16_t)colors[i] & 0xFF);
    }

    endTransaction();

#ifdef DEBUG
    Serial.print("[Display] Drawn ");
    Serial.print(buffer.size());
    Serial.print(" pixels in ");
    Serial.print(buffer.getArea().width);
    Serial.print("x");
    Serial.print(buffer.getArea().height);
    Serial.println(" window.");
#endif
  };

  void fill(Color color)
  {
#ifdef DEBUG
    Serial.print("[Display] Filling with #");
    Serial.print((uint16_t)color, HEX);
    Serial.println(" (RGB565).");
#endif
    buffer.fill(color);
  };

  void invert(bool on)
  {
    beginTransaction();
    writeCommand(on ? INVON : INVOFF);
    endTransaction();
  };
  void clear()
  {
    configureArea(window);
    fill(Color::black);
    drawPixels();

#ifdef DEBUG
    Serial.println("[Display] Cleared.");
#endif
  };

  int printChar(int x, int y, char value, Color fgColor, Color bgColor)
  {

    if (x + CHAR_WIDTH > window.width || y + CHAR_HEIGHT > window.height)
      return -1;

    for (int i = 0; i < CHAR_WIDTH; i++)
      for (int j = 0; j < CHAR_HEIGHT; j++)
        if ((font[value - ' '][i]) & (1 << (CHAR_HEIGHT - 1 - j)))
          setPixel(x + i, y + j, fgColor);
        else
          setPixel(x + i, y + j, bgColor);

    drawPixels();

    return 0;

#ifdef DEBUG
    Serial.print("[Display] Character '");
    Serial.print(value);
    Serial.print("' printed at ");
    Serial.print(x);
    Serial.print(", ");
    Serial.print(y);
    Serial.print(" with fg #");
    Serial.print((uint16_t)fgColor, HEX);
    Serial.print(" and bg #");
    Serial.println((uint16_t)bgColor, HEX);
#endif
  }
  int printString(int x, int y, char *c_str, Color fgColor, Color bgColor)
  {
    if (x + strlen(c_str) * 6 > window.width || y + 8 > window.height)
      return -1;
    else
      for (int i = 0; c_str[i] != '\0'; i++)
        printChar(x + i * 6, y, c_str[i], fgColor, bgColor);

    return 0;
  }

private:
  const static int CHAR_WIDTH = 6, CHAR_HEIGHT = 8;

  const static int RST = 9, DC = 8;

#ifdef __AVR_ATmega2560__
  const static int CS = 53;
  typedef uint8_t COLOR_FORMAT;
#else
  const static int CS = 10;
  typedef uint16_t COLOR_FORMAT;
#endif

  const static int SPI_DEFAULT_FREQ = 1e6;
  SPISettings settings = SPISettings(SPI_DEFAULT_FREQ, MSBFIRST, SPI_MODE0);

  DisplayBuffer<COLOR_FORMAT> buffer;

  void commandMode() { digitalWrite(DC, LOW); }
  void dataMode() { digitalWrite(DC, HIGH); }

  void on() { digitalWrite(CS, LOW); }
  void off() { digitalWrite(CS, HIGH); }

  void beginTransaction()
  {
    SPI.beginTransaction(settings);
    on();
  };
  void endTransaction()
  {
    off();
    SPI.endTransaction();
  };

  void writeCommand(Command cmd)
  {
    commandMode();
    SPI.transfer(cmd);
    dataMode();
  };

  void writeDatum(uint8_t datum)
  {
    SPI.transfer(datum);
  }

public:
  const static Display ili9341, st7735, limitedMemory;
};

const Display
    Display::ili9341 = Display(240, 320),     // 2.2" TFT, 240x320 pixel
    Display::st7735 = Display(128, 160),      // 1.8" TFT, 160x128 pixel
    Display::limitedMemory = Display(64, 64); // 64x64 pixel

#endif // DISPLAY_H