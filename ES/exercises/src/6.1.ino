#include "Display.h"

#ifdef __AVR_ATmega2560__
Display display(240, 640); // 2.2" TFT, 240x620 pixel
#else
Display display(160, 128); // 1.8" TFT, 160x128 pixel
#endif

void demo()
{
  // clear display
  Window area = Window(80, 80);
  display.configureArea(area);
  display.fill(Color::white);
  display.drawPixels();

  // draw blue rectangle centered on Display
  const int diameter = 10;
  const int xc = area.width / 2, yc = area.height / 2;

  display.configureArea(Window(xc - diameter, xc + diameter, yc - diameter, yc + diameter));
  display.fill(Color::blue);
  display.drawPixels();

  // draw centered red colored cross with one pixel space to border
  display.configureArea(Window(area.xs(), area.xe(), yc, yc + 1));
  for (int x = 0; x < area.width; x++)
    display.setPixel(x, 0, Color::red);

  display.drawPixels();

  display.configureArea(Window(xc, xc + 1, area.ys(), area.ye()));
  for (int y = 0; y < area.height; y++)
    display.setPixel(0, y, Color::red);

  display.drawPixels();
};

void setup()
{
  Serial.begin(9600); // open serial port
  Serial.println("[Demo] TFT-LCD demo starting…");

  display.init(); // power-on-reset of Display

  demo(); // draw the demo image
  Serial.println("[Demo] Setup done.");
}

bool isInverted = false;

void loop()
{
  delay(1000);
  display.invert(isInverted = !isInverted);
  Serial.print("[Demo] Color inversion is ");
  Serial.println(isInverted ? "on." : "off.");
}
