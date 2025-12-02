// #include <Wire.h>
// #include <Arduino.h>

// bool isMagAddress(uint8_t addr)
// {
//     // Common I2C addresses for magnetometers
//     switch (addr)
//     {
//     case 0x1E: // HMC5883L, LIS3MDL (default)
//     case 0x1C: // Some LIS3MDL / MAG chips alt address
//     case 0x0C: // AK8963 / AK09916 (in some IMUs)
//     case 0x0E: // Some other AKM mags
//         return true;
//     default:
//         return false;
//     }
// }

// void setup()
// {
//     Serial.begin(115200);
//     while (!Serial)
//     {
//         ; // wait for Serial on some boards
//     }

//     Serial.println("I2C device scan (looking for magnetometer)...");
//     Wire.begin();
// }

// void loop()
// {
//     byte error, address;
//     int nDevices = 0;

//     Serial.println("Scanning I2C bus...");

//     for (address = 1; address < 127; address++)
//     {
//         Wire.beginTransmission(address);
//         error = Wire.endTransmission();

//         if (error == 0)
//         {
//             Serial.print("I2C device found at address 0x");
//             if (address < 16)
//                 Serial.print("0");
//             Serial.print(address, HEX);

//             if (isMagAddress(address))
//             {
//                 Serial.print("  <-- possible magnetometer");
//             }

//             Serial.println();
//             nDevices++;
//         }
//     }

//     if (nDevices == 0)
//     {
//         Serial.println("No I2C devices found.");
//     }
//     else
//     {
//         Serial.println("Scan done.");
//     }

//     Serial.println("\n--- Wait 5 seconds for next scan ---\n");
//     delay(5000);
// }
