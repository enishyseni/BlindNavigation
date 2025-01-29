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

// ------------------- PWM (LEDC) SETTINGS ----------------
#define LEDC_CHANNEL_A 0
#define LEDC_CHANNEL_B 1
#define LEDC_CHANNEL_C 2
#define LEDC_CHANNEL_D 3

#define LEDC_TIMER_BIT 8    // 8-bit resolution (0-255)
#define LEDC_BASE_FREQ 5000 // 5 kHz PWM frequency

// ------------------- DISTANCE RANGE ---------------------
// Distances in centimeters
const float DIST_FAR_LIMIT = 600.0f; // 600 cm = 6 m (start vibrating)
const float DIST_NEAR_LIMIT = 30.0f; // 30 cm = highest vibration
// If distance > 600 => no vibration
// If distance < 30  => full vibration
// In between => linear scale

// ------------------- VIBRATION LIMITS (PWM) -------------
const int VIBRATION_MIN = 10;  // minimal non-zero vibration duty at 6 m
const int VIBRATION_MAX = 255; // maximum vibration duty at 30 cm

// ------------------- PULSE -> DISTANCE SCALING ----------
/*
 * Many mmWave or ultrasonic-like sensors use a relationship between
 * pulse width and distance. For an ultrasonic sensor, you might see
 * distance (cm) = (pulseWidth_us / 2) * 0.0343.
 * Adapt DIST_SCALE_FACTOR for your sensor’s specs.
 */
const float DIST_SCALE_FACTOR = 0.0343f;

// ------------------- SETUP ------------------------------
void setup()
{
    Serial.begin(115200);
    delay(100);

    // Configure sensor pins as inputs
    pinMode(SENSOR_PIN_A, INPUT);
    pinMode(SENSOR_PIN_B, INPUT);
    pinMode(SENSOR_PIN_C, INPUT);
    pinMode(SENSOR_PIN_D, INPUT);

    // Setup LEDC channels for motors (PWM)
    ledcSetup(LEDC_CHANNEL_A, LEDC_BASE_FREQ, LEDC_TIMER_BIT);
    ledcSetup(LEDC_CHANNEL_B, LEDC_BASE_FREQ, LEDC_TIMER_BIT);
    ledcSetup(LEDC_CHANNEL_C, LEDC_BASE_FREQ, LEDC_TIMER_BIT);
    ledcSetup(LEDC_CHANNEL_D, LEDC_BASE_FREQ, LEDC_TIMER_BIT);

    // Attach channels to motor pins
    ledcAttachPin(MOTOR_PIN_A, LEDC_CHANNEL_A);
    ledcAttachPin(MOTOR_PIN_B, LEDC_CHANNEL_B);
    ledcAttachPin(MOTOR_PIN_C, LEDC_CHANNEL_C);
    ledcAttachPin(MOTOR_PIN_D, LEDC_CHANNEL_D);

    // Initialize motors OFF
    ledcWrite(LEDC_CHANNEL_A, 0);
    ledcWrite(LEDC_CHANNEL_B, 0);
    ledcWrite(LEDC_CHANNEL_C, 0);
    ledcWrite(LEDC_CHANNEL_D, 0);

    Serial.println("ESP32 mmWave Pulse + Vibration: 6m->30cm scale");
}

// ------------------- MAIN LOOP --------------------------
void loop()
{
    // 1. Read distance from each sensor
    float distA = readDistance(SENSOR_PIN_A);
    float distB = readDistance(SENSOR_PIN_B);
    float distC = readDistance(SENSOR_PIN_C);
    float distD = readDistance(SENSOR_PIN_D);

    // 2. Compute vibration duty
    uint8_t dutyA = computeMotorDuty(distA);
    uint8_t dutyB = computeMotorDuty(distB);
    uint8_t dutyC = computeMotorDuty(distC);
    uint8_t dutyD = computeMotorDuty(distD);

    // 3. Write PWM to motors
    ledcWrite(LEDC_CHANNEL_A, dutyA);
    ledcWrite(LEDC_CHANNEL_B, dutyB);
    ledcWrite(LEDC_CHANNEL_C, dutyC);
    ledcWrite(LEDC_CHANNEL_D, dutyD);

    // 4. Debug printing (optional)
    Serial.print("Dist(cm): A=");
    Serial.print(distA, 1);
    Serial.print(" B=");
    Serial.print(distB, 1);
    Serial.print(" C=");
    Serial.print(distC, 1);
    Serial.print(" D=");
    Serial.print(distD, 1);

    Serial.print(" | Duty: A=");
    Serial.print(dutyA);
    Serial.print(" B=");
    Serial.print(dutyB);
    Serial.print(" C=");
    Serial.print(dutyC);
    Serial.print(" D=");
    Serial.println(dutyD);

    delay(200);
}

// ------------------- readDistance() ---------------------
/*
 * Reads a pulse on the given pin using pulseIn().
 * This is a blocking call and returns 0 if timed out (no pulse).
 * Distances beyond sensor range might also return 0 or very large pulses.
 */
float readDistance(int pin)
{
    // Timeout after 30ms. Adjust if needed for your sensor max range.
    unsigned long duration = pulseIn(pin, HIGH, 30000UL);

    if (duration == 0)
    {
        // Timed out / no pulse => treat as "very far"
        return 9999.0;
    }

    // Convert microseconds to distance in cm
    // (typical ultrasonic formula: dist = (duration * 0.0343)/2 )
    // For mmWave with pulse output, adapt as per your sensor datasheet.
    float dist = (duration * DIST_SCALE_FACTOR) / 2.0f;

    return dist;
}

// ------------------- computeMotorDuty() -----------------
/*
 * Maps distance to a PWM duty [0..255]:
 *  - > 600 cm => 0 (no vibration)
 *  - = 600 cm => minimal vibration (VIBRATION_MIN)
 *  - = 30 cm  => max vibration (255)
 *  - < 30 cm  => clamp to 255
 *  - In-between => linear mapping from 600..30 to 1..255
 */
uint8_t computeMotorDuty(float dist)
{
    // If distance is beyond 6 m, no vibration
    if (dist >= DIST_FAR_LIMIT)
    {
        return 0;
    }
    // If distance is less than or equal to 30 cm, full vibration
    if (dist <= DIST_NEAR_LIMIT)
    {
        return VIBRATION_MAX;
    }

    // Now linearly map from 600 cm -> ~1..(some minimal) up to 255.
    // We'll define the scale so that 600 cm -> 1 and 30 cm -> 255.
    // If you want a different "lowest possible vibration," adjust below.
    float ratio = (DIST_FAR_LIMIT - dist) / (DIST_FAR_LIMIT - DIST_NEAR_LIMIT);
    // ratio = 0 at dist=600, ratio = 1 at dist=30.

    // Map ratio (0..1) to 1..255:
    int duty = (int)(ratio * (VIBRATION_MAX - 1) + 1);

    // Ensure bounds
    if (duty < 1)
        duty = 1;
    if (duty > 255)
        duty = 255;
    return (uint8_t)duty;
}
