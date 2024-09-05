#include <Arduino.h>
#include <Adafruit_ADS1X15.h>

// Function declaration
int myFunction(int, int);

#define LED_BUILTIN 0
#define Buzzer 16
Adafruit_ADS1115 ads;

// Pin connected to the ALERT/RDY signal for new sample notification
constexpr int READY_PIN = 19;
constexpr int BREAKER_PIN = 15;

constexpr int16_t STAGE1_MIN = -400;
constexpr int16_t STAGE2_MIN = -600;
constexpr int16_t STAGE3_MIN = -800;

constexpr int16_t STAGE1_MAX = 400;
constexpr int16_t STAGE2_MAX = 600;
constexpr int16_t STAGE3_MAX = 800;

constexpr uint32_t STAGE1_DELAY = 100;
constexpr uint32_t STAGE2_DELAY = 75;
constexpr uint32_t STAGE3_DELAY = 50;

float multiplier = 0.0078125F; // ADS1115 @ +/- 6.144V gain (16-bit results)

volatile bool new_data = false;
uint32_t stage1, stage2, stage3;
uint32_t count = 0;
bool state = true;
int16_t mmin = 0;
int16_t mmax = 0;

#ifndef IRAM_ATTR
#define IRAM_ATTR
#endif

void IRAM_ATTR NewDataReadyISR() {
    new_data = true;
}

void setup() {
    pinMode(BREAKER_PIN, OUTPUT);
    pinMode(LED_BUILTIN, OUTPUT);
    pinMode(Buzzer, OUTPUT);
    digitalWrite(BREAKER_PIN, !state);

    Serial.begin(250000);
    Serial.println("Hello!");

    Serial.println("Getting differential reading from AIN0 (P) and AIN1 (N)");
    Serial.println("ADC Range: +/- 6.144V (1 bit = 0.1875mV/ADS1115)");

    ads.setGain(GAIN_SIXTEEN);
    ads.setDataRate(RATE_ADS1115_860SPS);

    if (!ads.begin()) {
        Serial.println("Failed to initialize ADS.");
        while (1);
    }

    pinMode(READY_PIN, INPUT);
    attachInterrupt(digitalPinToInterrupt(READY_PIN), NewDataReadyISR, FALLING);

    // Start continuous conversions
    ads.startADCReading(ADS1X15_REG_CONFIG_MUX_DIFF_0_1, true);

    // Initialize stage timers
    stage1 = millis();
    stage2 = millis();
    stage3 = millis();
}



void loop() {
    
    if (!state) {
        digitalWrite(BREAKER_PIN, !state);
        uint32_t s = millis();
        while (millis() - s < 10000) {
            digitalWrite(LED_BUILTIN, ((millis() / 500) % 2 == 0));
            digitalWrite(Buzzer, ((millis() / 1000) % 2 == 0));
        }
            digitalWrite(Buzzer, 0);

        state = true;
        mmax = 0;
        mmin = 0;
        digitalWrite(BREAKER_PIN, !state);
        stage1 = millis();
        stage2 = millis();
        stage3 = millis();
    }

    digitalWrite(LED_BUILTIN, ((millis() / 500) % 2 == 0));

    uint32_t s = millis();
    while (count < 8) {
        if (new_data) {
            count++;
            int16_t x = ads.getLastConversionResults();
            if (x < mmin) {
                mmin = x;
            } else if (x > mmax) {
                mmax = x;
            }
            new_data = false;
        }
    }
    uint32_t e = millis();

    mmax = mmax / sqrt(2);
    mmin = mmin / sqrt(2);

    if (mmax < STAGE1_MAX && mmin > STAGE1_MIN) stage1 = millis();
    if (mmax < STAGE2_MAX && mmin > STAGE2_MIN) stage2 = millis();
    if (mmax < STAGE3_MAX && mmin > STAGE3_MIN) stage3 = millis();

    if ((millis() - stage1 > STAGE1_DELAY) || 
        (millis() - stage2 > STAGE2_DELAY) || 
        (millis() - stage3 > STAGE3_DELAY)) {
        state = false;
    }

    /*Serial.println(".........................");
    Serial.print("s1: "); Serial.println(millis() - stage1);
    Serial.print("s2: "); Serial.println(millis() - stage2);
    Serial.print("s3: "); Serial.println(millis() - stage3);
    Serial.println(".........................");*/
    Serial.print("d: "); Serial.println(e - s);
    Serial.print("mmax: "); Serial.println(mmax);
    Serial.print("mmin: "); Serial.println(mmin);
    Serial.print("state: "); Serial.println(state);
    Serial.println(".........................");

    count = 0;
    mmax = 0;
    mmin = 0;
}

// Function definition
int myFunction(int x, int y) {
    return x + y;
}