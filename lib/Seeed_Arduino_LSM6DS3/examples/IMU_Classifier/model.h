// Auto-generated configuration file
// Generated from Python preprocessing

#ifndef CONFIG_H
#define CONFIG_H

// Model configuration
#define NUM_CLASSES 5
#define NUM_FEATURES 6
#define WINDOW_SIZE 128
#define SAMPLING_RATE 50

// Normalization parameters (from training data)
const float SENSOR_MEAN[NUM_FEATURES] = {
    0.066785f, -0.230932f, -0.508558f, -0.724521f, 1.679773f, 0.282399f
};

const float SENSOR_STD[NUM_FEATURES] = {
    0.613383f, 0.318781f, 0.426112f, 35.380800f, 15.206224f, 11.514720f
};

// Quantization parameters (from TFLite model)
const float INPUT_SCALE = 0.14997722f;
const int8_t INPUT_ZERO_POINT = 7;
const float OUTPUT_SCALE = 0.00390625f;
const int8_t OUTPUT_ZERO_POINT = -128;

// Activity labels
const char* ACTIVITY_LABELS[NUM_CLASSES] = {
    "nail_biting", "knuckles_cracking", "hand_tapping", "sitting", "standing"
};

// Sensor column order (for reference)
// 0: Ax, 1: Ay, 2: Az, 3: Gx, 4: Gy, 5: Gz

#endif // CONFIG_H
