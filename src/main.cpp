#include <Arduino.h>
#include <Wire.h>

#include <LSM6DS3.h>

LSM6DS3 imu(I2C_MODE, 0x6A); // IMU I2C address is 0x6A

void setup()
{
  Serial.println("Initializing IMU...");
  Serial.begin(115200);
  delay(1000);

  if (imu.begin() != 0)
  {
    Serial.println("IMU initialization failed!");
    while (1)
      ;
  }

  Serial.println("IMU initialized successfully!");
}

void loop()
{
  float ax = imu.readFloatAccelX();
  float ay = imu.readFloatAccelY();
  float az = imu.readFloatAccelZ();

  float gx = imu.readFloatGyroX();
  float gy = imu.readFloatGyroY();
  float gz = imu.readFloatGyroZ();

  float temp = imu.readTempC();

  Serial.print("Accel (g): ");
  Serial.print(ax);
  Serial.print(", ");
  Serial.print(ay);
  Serial.print(", ");
  Serial.println(az);

  Serial.print("Gyro (°/s): ");
  Serial.print(gx);
  Serial.print(", ");
  Serial.print(gy);
  Serial.print(", ");
  Serial.println(gz);

  Serial.print("Temp: ");
  Serial.print(temp);
  Serial.println(" °C");

  Serial.println("----------------------");
  delay(200);
}
