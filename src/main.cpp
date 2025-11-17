#include <Arduino.h>
#include <Wire.h>

void setup()
{
  Serial.begin(115200);
  while (!Serial)
    ;

  // Correct for Adafruit nRF52 core used by PlatformIO:
  Wire.setPins(4, 5); // SDA = D4, SCL = D5
  Wire.begin();

  Serial.println("Scanning I2C bus...");

  uint8_t count = 0;

  for (uint8_t i = 1; i < 127; i++)
  {
    Wire.beginTransmission(i);
    if (Wire.endTransmission() == 0)
    {
      Serial.print("Found I2C device at: 0x");
      Serial.println(i, HEX);
      count++;
    }
    delay(5);
  }

  if (count == 0)
  {
    Serial.println("No I2C devices found.");
  }
}

void loop()
{
}
