/*
  XIAO nRF52840 Sense + StressSense trained INT8 TFLite Micro model
  - Expects input window: 100 samples × 6 axes (Ax Ay Az Gx Gy Gz) => 600 values
  - Model is INT8 (quantized), so we:
      1) normalize: (x - mean) / std
      2) quantize to int8 using input tensor scale/zero_point
  - Output is INT8; we dequantize using output tensor scale/zero_point

  IMPORTANT:
  1) Replace model.h with the one generated from your stress_model.tflite
  2) Fill MEAN[] and STD_[] with values from norm_params.npz (see notes below)
*/

#include <LSM6DS3.h>
#include <Wire.h>

#include <TensorFlowLite.h>
#include <tensorflow/lite/micro/all_ops_resolver.h>
#include <tensorflow/lite/micro/micro_error_reporter.h>
#include <tensorflow/lite/micro/micro_interpreter.h>
#include <tensorflow/lite/schema/schema_generated.h>

#include "model.h"

// OLED
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET   -1
#define OLED_ADDR    0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ========================
// CONFIG (match training)
// ========================
static constexpr int kNumSamples = 100;   // WIN = 100 (2 sec @ ~50Hz)
static constexpr int kNumAxes    = 6;     // Ax Ay Az Gx Gy Gz
static constexpr int kInputSize  = kNumSamples * kNumAxes; // 600

// Motion gate (optional): start sampling only after movement
const float accelerationThreshold = 2.5f; // sum(|Ax|+|Ay|+|Az|) threshold (in g's)

// Replace these with your real normalization values from norm_params.npz
// Order MUST be: Ax, Ay, Az, Gx, Gy, Gz
const float MEAN[kNumAxes] = {
  /* mAx, mAy, mAz, mGx, mGy, mGz */
  // 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f
  -1.0797258615493774, -2.1959586143493652, 3.1541600227355957, 0.0001183523127110675, -0.008691100403666496, -0.031837768852710724
};

const float STD_[kNumAxes] = {
  /* sAx, sAy, sAz, sGx, sGy, sGz */
  // 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f
  7.534790515899658, 3.393746852874756, 3.386632204055786, 0.4659119248390198, 0.2757088840007782, 0.49304184317588806
};

// Labels must match the order used in Python training: labels = sorted(unique(Activity))
const char* GESTURES[] = {
  "Eating",
  "Face_Touch",
  "Nail_Biting",
  "Smoking",
  "Staying_Still"
};
#define NUM_GESTURES (sizeof(GESTURES) / sizeof(GESTURES[0]))

// ========================
// IMU
// ========================
LSM6DS3 myIMU(I2C_MODE, 0x6A);

// ========================
// TFLM Globals
// ========================
tflite::MicroErrorReporter tflErrorReporter;
tflite::AllOpsResolver tflOpsResolver;

const tflite::Model* tflModel = nullptr;
tflite::MicroInterpreter* tflInterpreter = nullptr;
TfLiteTensor* tflInputTensor = nullptr;
TfLiteTensor* tflOutputTensor = nullptr;

// Increase arena vs Seeed demo (your model likely needs more than 8KB)
constexpr int tensorArenaSize = 40 * 1024;
byte tensorArena[tensorArenaSize] __attribute__((aligned(16)));

static void oledShowBoot(const char* line1, const char* line2 = nullptr) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println(line1);
  if (line2) display.println(line2);
  display.display();
}

static void oledShowResult(const char* label, float conf01) {
  if (conf01 < 0.0f) conf01 = 0.0f;
  if (conf01 > 1.0f) conf01 = 1.0f;

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Prediction:");

  display.setTextSize(2);
  display.setCursor(0, 16);
  display.println(label);

  display.setTextSize(1);
  display.setCursor(0, 48);
  display.print("Conf: ");
  display.print(conf01 * 100.0f, 1);
  display.println("%");

  display.display();
}

// Quantize float -> int8 using tensor params
static inline int8_t quantize_to_int8(float x, float scale, int zero_point) {
  int32_t q = (int32_t)lroundf(x / scale) + zero_point;
  if (q < -128) q = -128;
  if (q > 127)  q = 127;
  return (int8_t)q;
}

// Dequantize int8 -> float using tensor params
static inline float dequantize_from_int8(int8_t q, float scale, int zero_point) {
  return ( (int)q - zero_point ) * scale;
}

void setup() {
  Serial.begin(115200);
  while (!Serial);

  Wire.begin();

  // OLED init
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("SSD1306 allocation failed");
    while (1);
  }
  oledShowBoot("OLED OK", "Starting IMU...");

  // ---- IMU config: set approx 52Hz if supported by library ----
  // NOTE: In this library, settings must be set BEFORE begin().
  myIMU.settings.accelEnabled = 1;
  myIMU.settings.gyroEnabled  = 1;
  myIMU.settings.accelRange   = 2;     // +/-2g
  myIMU.settings.gyroRange    = 2000;  // +/-2000 dps

  // Many versions accept 52 for ODR; if yours doesn't, it will still run and you can rely on delay(20).
  // myIMU.settings.accelODR = 52;
  // myIMU.settings.gyroODR  = 52;

  if (myIMU.begin() != 0) {
    Serial.println("IMU error");
    oledShowBoot("IMU ERROR", "Check power");
    while (1);
  }
  oledShowBoot("IMU OK", "Loading model...");

  // Load model
  tflModel = tflite::GetModel(model);   // model[] comes from model.h
  if (tflModel->version() != TFLITE_SCHEMA_VERSION) {
    Serial.println("Model schema mismatch!");
    oledShowBoot("MODEL ERROR", "Schema mismatch");
    while (1);
  }

  // Interpreter
  tflInterpreter = new tflite::MicroInterpreter(
    tflModel, tflOpsResolver, tensorArena, tensorArenaSize, &tflErrorReporter
  );

  if (tflInterpreter->AllocateTensors() != kTfLiteOk) {
    Serial.println("AllocateTensors failed! Increase tensorArenaSize.");
    oledShowBoot("TFLM ERROR", "Alloc failed");
    while (1);
  }

  tflInputTensor  = tflInterpreter->input(0);
  tflOutputTensor = tflInterpreter->output(0);

  // Sanity prints
  Serial.print("Input type: ");  Serial.println(tflInputTensor->type);   // should be kTfLiteInt8
  Serial.print("Output type: "); Serial.println(tflOutputTensor->type);  // should be kTfLiteInt8

  Serial.print("Input dims: ");
  for (int i = 0; i < tflInputTensor->dims->size; i++) {
    Serial.print(tflInputTensor->dims->data[i]);
    Serial.print(" ");
  }
  Serial.println();

  // Optional: check expected size
  // Some models show dims [1,100,6] => total elements 600
  // We still fill 600 sequential values.
  oledShowBoot("Ready", "Move to classify");
}

void loop() {
  static int samplesRead = kNumSamples;

  // Wait for significant motion (optional gate)
  while (samplesRead == kNumSamples) {
    float aX = myIMU.readFloatAccelX();
    float aY = myIMU.readFloatAccelY();
    float aZ = myIMU.readFloatAccelZ();
    float aSum = fabs(aX) + fabs(aY) + fabs(aZ);

    if (aSum >= accelerationThreshold) {
      samplesRead = 0;
      break;
    }
    delay(10);
  }

  // Collect one full window
  while (samplesRead < kNumSamples) {
    float x[kNumAxes];
    x[0] = myIMU.readFloatAccelX();
    x[1] = myIMU.readFloatAccelY();
    x[2] = myIMU.readFloatAccelZ();
    x[3] = myIMU.readFloatGyroX();
    x[4] = myIMU.readFloatGyroY();
    x[5] = myIMU.readFloatGyroZ();

    // Normalize like training: (x - mean) / std
    float xn[kNumAxes];
    for (int k = 0; k < kNumAxes; k++) {
      xn[k] = (x[k] - MEAN[k]) / STD_[k];
    }

    // Quantize and store in INT8 input tensor
    const float in_scale = tflInputTensor->params.scale;
    const int   in_zp    = tflInputTensor->params.zero_point;

    int base = samplesRead * kNumAxes;
    for (int k = 0; k < kNumAxes; k++) {
      tflInputTensor->data.int8[base + k] = quantize_to_int8(xn[k], in_scale, in_zp);
    }

    samplesRead++;

    // Keep sampling close to training rate (~50Hz)
    delay(20);

    if (samplesRead == kNumSamples) {
      // Run inference
      if (tflInterpreter->Invoke() != kTfLiteOk) {
        Serial.println("Invoke failed!");
        oledShowBoot("TFLM ERROR", "Invoke failed");
        while (1);
      }

      // Dequantize outputs
      const float out_scale = tflOutputTensor->params.scale;
      const int   out_zp    = tflOutputTensor->params.zero_point;

      int bestIdx = 0;
      float bestVal = -1e9;

      Serial.println("---- Scores ----");
      for (int i = 0; i < (int)NUM_GESTURES; i++) {
        int8_t q = tflOutputTensor->data.int8[i];
        float score = dequantize_from_int8(q, out_scale, out_zp);

        Serial.print(GESTURES[i]);
        Serial.print(": ");
        Serial.println(score, 6);

        if (score > bestVal) {
          bestVal = score;
          bestIdx = i;
        }
      }
      Serial.println();

      // bestVal is a dequantized output. If your final layer is softmax, it *should* be 0..1-ish.
      oledShowResult(GESTURES[bestIdx], bestVal);

      // Reset for next motion
      samplesRead = kNumSamples;
    }
  }
}

/*
HOW TO FILL MEAN[] and STD_[] ON WINDOWS:

1) In your training folder (where norm_params.npz exists), create print_norm.py:

   import numpy as np
   d = np.load("norm_params.npz")
   mean = d["mean"].reshape(-1)
   std  = d["std"].reshape(-1)
   print("MEAN:", mean.tolist())
   print("STD :", std.tolist())

2) Run:
   python print_norm.py

3) Copy the printed lists into MEAN[] and STD_[] above (same order Ax Ay Az Gx Gy Gz).
*/
