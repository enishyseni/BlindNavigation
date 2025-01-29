/********************************************************************
 * Example: 4 mmWave Sensors (Pulse Input) + 4 Vibration Motors (PWM)
 * Board: ESP32 (e.g., DevKitC)
 *
 * - Each sensor outputs a pulse whose width (in microseconds)
 *   correlates to the distance to an obstacle.
 * - We read that pulse using pulseIn() on the input pins.
 * - We convert the pulse width to a distance (cm) with a hypothetical
 *   scale factor. (Adjust to your sensor's datasheet!)
 * - We set a distance threshold for triggering vibration.
 * - We scale the vibration intensity (PWM duty cycle) based on distance.
 ********************************************************************/

#include "Arduino.h"

// ------------------- PIN ASSIGNMENTS -------------------
#define SENSOR_PIN_A 4  // Digital input for sensor A
#define SENSOR_PIN_B 16 // Digital input for sensor B
#define SENSOR_PIN_C 17 // Digital input for sensor C
#define SENSOR_PIN_D 18 // Digital input for sensor D

#define MOTOR_PIN_A 25 // PWM output for motor A
#define MOTOR_PIN_B 26 // PWM output for motor B
#define MOTOR_PIN_C 27 // PWM output for motor C
#define MOTOR_PIN_D 14 // PWM output for motor D

// ------------------- PWM SETTINGS ----------------------
// We use the ESP32 LEDC peripheral to do hardware PWM.
#define LEDC_CHANNEL_A 0
#define LEDC_CHANNEL_B 1
#define LEDC_CHANNEL_C 2
#define LEDC_CHANNEL_D 3

#define LEDC_TIMER_BIT 8    // resolution of PWM (0-255)
#define LEDC_BASE_FREQ 5000 // 5 kHz PWM frequency (adjust as needed)

// ------------------- SENSOR / DISTANCE PARAMS ---------
const float DIST_SCALE_FACTOR = 0.0343f;
// This constant is an example for ultrasonic-like pulses
// (e.g., 1 microsecond per ~0.0343 cm).
// For mmWave or other sensors, consult the datasheet for how to convert
// pulse width to distance. Update accordingly.

// If the sensor's pulse logic is different, you'll need to adapt.

// Distance thresholds and range for scaling
const float DIST_THRESHOLD = 100.0; // cm
const float DIST_MIN = 20.0;        // cm
const float DIST_MAX = 200.0;       // cm

// ------------------- SETUP ----------------------------
void setup()
{
    Serial.begin(115200);
    delay(100);

    // Configure sensor pins as inputs
    pinMode(SENSOR_PIN_A, INPUT);
    pinMode(SENSOR_PIN_B, INPUT);
    pinMode(SENSOR_PIN_C, INPUT);
    pinMode(SENSOR_PIN_D, INPUT);

    // Setup LEDC channels for motors
    ledcSetup(LEDC_CHANNEL_A, LEDC_BASE_FREQ, LEDC_TIMER_BIT);
    ledcSetup(LEDC_CHANNEL_B, LEDC_BASE_FREQ, LEDC_TIMER_BIT);
    ledcSetup(LEDC_CHANNEL_C, LEDC_BASE_FREQ, LEDC_TIMER_BIT);
    ledcSetup(LEDC_CHANNEL_D, LEDC_BASE_FREQ, LEDC_TIMER_BIT);

    // Attach these channels to motor pins
    ledcAttachPin(MOTOR_PIN_A, LEDC_CHANNEL_A);
    ledcAttachPin(MOTOR_PIN_B, LEDC_CHANNEL_B);
    ledcAttachPin(MOTOR_PIN_C, LEDC_CHANNEL_C);
    ledcAttachPin(MOTOR_PIN_D, LEDC_CHANNEL_D);

    // Initially, set all motors OFF (0 duty)
    ledcWrite(LEDC_CHANNEL_A, 0);
    ledcWrite(LEDC_CHANNEL_B, 0);
    ledcWrite(LEDC_CHANNEL_C, 0);
    ledcWrite(LEDC_CHANNEL_D, 0);

    Serial.println("ESP32 mmWave (pulse) + Vibration Motor System Initialized.");
}

// ------------------- MAIN LOOP -------------------------
void loop()
{
    // 1. Read pulse widths from each sensor (blocking sequentially)
    float distanceA = readDistance(SENSOR_PIN_A);
    float distanceB = readDistance(SENSOR_PIN_B);
    float distanceC = readDistance(SENSOR_PIN_C);
    float distanceD = readDistance(SENSOR_PIN_D);

    // 2. Compute PWM duty cycle for each motor
    uint8_t dutyA = computeMotorDuty(distanceA);
    uint8_t dutyB = computeMotorDuty(distanceB);
    uint8_t dutyC = computeMotorDuty(distanceC);
    uint8_t dutyD = computeMotorDuty(distanceD);

    // 3. Write PWM duty to motors
    ledcWrite(LEDC_CHANNEL_A, dutyA);
    ledcWrite(LEDC_CHANNEL_B, dutyB);
    ledcWrite(LEDC_CHANNEL_C, dutyC);
    ledcWrite(LEDC_CHANNEL_D, dutyD);

    // 4. Debug printing (optional)
    Serial.print("Distances (cm): A=");
    Serial.print(distanceA, 1);
    Serial.print(" B=");
    Serial.print(distanceB, 1);
    Serial.print(" C=");
    Serial.print(distanceC, 1);
    Serial.print(" D=");
    Serial.print(distanceD, 1);
    Serial.print(" | Duty: A=");
    Serial.print(dutyA);
    Serial.print(" B=");
    Serial.print(dutyB);
    Serial.print(" C=");
    Serial.print(dutyC);
    Serial.print(" D=");
    Serial.println(dutyD);

    delay(200); // small delay
}

// -------------------------------------------------------
// readDistance(pin)
// - Reads a pulse on the given pin
// - Converts the pulse width to a distance (in cm) using
//   a hypothetical scale factor. Adjust for your sensor.
// -------------------------------------------------------
float readDistance(int pin)
{
    // pulseIn: measures the time (in microseconds) for which
    // the pin stays HIGH
    //
    // If your sensor requires "pulseIn(pin, HIGH, timeout)" or
    // "pulseIn(pin, LOW, timeout)", adapt accordingly.
    unsigned long duration = pulseIn(pin, HIGH, 30000UL); // 30ms timeout

    if (duration == 0)
    {
        // Timed out or no pulse detected
        return 999.0;
    }

    // Convert microseconds to distance.
    // E.g., if using an ultrasonic-like approach:
    // distance (cm) = (duration / 2) * 0.0343
    // But for mmWave or custom pulses, consult your sensor’s docs.
    // Example for a typical ultrasonic-like formula:
    float dist = (duration * DIST_SCALE_FACTOR) / 2.0f;

    return dist;
}

// -------------------------------------------------------
// computeMotorDuty(distance)
// - Returns a PWM duty cycle (0-255) for the vibration motor
// - If distance < DIST_THRESHOLD, scale the intensity
//   so that closer = stronger vibration.
// -------------------------------------------------------
uint8_t computeMotorDuty(float dist)
{
    if (dist > DIST_THRESHOLD)
    {
        // No vibration if above threshold
        return 0;
    }

    // Constrain distance between DIST_MIN and DIST_THRESHOLD
    float clamped = dist;
    if (clamped < DIST_MIN)
        clamped = DIST_MIN;
    if (clamped > DIST_THRESHOLD)
        clamped = DIST_THRESHOLD;

    // Invert so that smaller dist = higher duty
    // Simple linear mapping:
    float ratio = (DIST_THRESHOLD - clamped) / (DIST_THRESHOLD - DIST_MIN);
    int duty = (int)(ratio * 255);
    if (duty < 0)
        duty = 0;
    if (duty > 255)
        duty = 255;

    return (uint8_t)duty;
}
