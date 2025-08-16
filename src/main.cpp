#include <Arduino.h>
#include <Adafruit_ADS1X15.h>
#include <EEPROM.h>

#define EEPROM_SIZE 64  // 64 bytes is safe, can be 4-4096 (multiple of 4)
#define ADDR_MAX_VALUE 0
#define ADDR_DELAY 4    // Next available address after 2-byte int16_t

#define LED_BUILTIN 0
#define Buzzer 16
Adafruit_ADS1115 ads;

// Pin connected to the ALERT/RDY signal for new sample notification
constexpr int NEW_READING_READY_PIN = 19;
constexpr int BREAKER_PIN = 15;

int16_t MAX_VALUE_IN_SAFE_RANGE = 800;
uint32_t DELAY_OF_CUTOFF = 30000;

volatile bool new_data = false;
bool powerState = true;

#ifndef IRAM_ATTR
#define IRAM_ATTR
#endif

void IRAM_ATTR NewDataReadyISR() {
    new_data = true;
}

// Function prototypes
bool isNewDataAvailable();
int16_t getMeasuredCurrentInCT_peak();
int16_t currentValue_peakToRms(int16_t peakValue);
bool checkIfInSafeRange(int16_t rmsVal);
void cutOffPower();
void turnOnPower();
void adcErrorBlinking();

// EEPROM functions
int16_t readMaxValueFromEEPROM();
void writeMaxValueToEEPROM(int16_t value);
uint32_t readDelayFromEEPROM();
void writeDelayToEEPROM(uint32_t value);

void setup() {
    pinMode(BREAKER_PIN, OUTPUT);
    pinMode(LED_BUILTIN, OUTPUT);
    pinMode(Buzzer, OUTPUT);
    digitalWrite(BREAKER_PIN, !powerState);

    Serial.begin(250000);
    Serial.println("Hello!");

    // Initialize EEPROM
    EEPROM.begin(EEPROM_SIZE);
    
    // Load saved values or defaults
    MAX_VALUE_IN_SAFE_RANGE = readMaxValueFromEEPROM();
    DELAY_OF_CUTOFF = readDelayFromEEPROM();

    Serial.println("Getting differential reading from AIN0 (P) and AIN1 (N)");
    Serial.println("ADC Range: +/- 6.144V (1 bit = 0.1875mV/ADS1115)");

    ads.setGain(GAIN_SIXTEEN);
    ads.setDataRate(RATE_ADS1115_860SPS);

    if (!ads.begin()) {
        Serial.println("Failed to initialize ADS.");
        while (1){
            adcErrorBlinking();
        };
    }

    pinMode(NEW_READING_READY_PIN, INPUT);
    attachInterrupt(digitalPinToInterrupt(NEW_READING_READY_PIN), NewDataReadyISR, FALLING);

    // Start continuous conversions
    ads.startADCReading(ADS1X15_REG_CONFIG_MUX_DIFF_0_1, true);
}

void loop() {
    if (!powerState) {
        uint32_t start = millis();
        while (millis() - start < DELAY_OF_CUTOFF) {
            delay(1);
            digitalWrite(LED_BUILTIN, ((millis() / 500) % 2 == 0));
            digitalWrite(Buzzer, ((millis() / 1000) % 2 == 0));
        }
        digitalWrite(Buzzer, 0);
        turnOnPower();
    } else {
        digitalWrite(Buzzer, 0);
        digitalWrite(LED_BUILTIN, ((millis() / 500) % 2 == 0));
    }

    if (isNewDataAvailable()) {
        uint16_t newPeak = getMeasuredCurrentInCT_peak();
        uint16_t newRms = currentValue_peakToRms(newPeak);

        if (!checkIfInSafeRange(newRms)) {
            cutOffPower();
        }
    }

    //uint32_t endOfMeasurement = millis();




    /*Serial.println(".........................");
    Serial.print("s1: "); Serial.println(millis() - stage1);
    Serial.print("s2: "); Serial.println(millis() - stage2);
    Serial.print("s3: "); Serial.println(millis() - lastTimeInSafeRange);
    Serial.println(".........................");*/
    //Serial.print("d: "); Serial.println(endOfMeasurement - startOfMeasurement);
    //Serial.print("powerState: "); Serial.println(powerState);
   // Serial.println(".........................");

}





bool isNewDataAvailable() {
    return new_data;
}

int16_t getMeasuredCurrentInCT_peak() {
    int16_t x = ads.getLastConversionResults();
    x = abs(x);
    new_data = false;
    return x;
}

int16_t currentValue_peakToRms(int16_t peakValue) {
    int16_t rmsVal = int16_t(((float)peakValue) / sqrt(2));
    return rmsVal;
}

bool checkIfInSafeRange(int16_t rmsVal) {
    return (rmsVal < MAX_VALUE_IN_SAFE_RANGE);
}

void cutOffPower() {
    powerState = false;
    digitalWrite(BREAKER_PIN, !powerState);
}

void turnOnPower() {
    powerState = true;
    digitalWrite(BREAKER_PIN, !powerState);
}

void adcErrorBlinking() {
    // Error blinking pattern
    digitalWrite(LED_BUILTIN, false);
    delay(100);
    digitalWrite(LED_BUILTIN, true);
    delay(200);
    digitalWrite(LED_BUILTIN, false);
    delay(100);
    digitalWrite(LED_BUILTIN, true);
    delay(200);
    digitalWrite(LED_BUILTIN, false);
    delay(700);
}

// EEPROM functions
int16_t readMaxValueFromEEPROM() {
    int16_t value;
    EEPROM.get(ADDR_MAX_VALUE, value);
    if (value == -1 || value == 0xFFFF) { // Uninitialized check
        writeMaxValueToEEPROM(800);
        return 800;
    }
    return value;
}

void writeMaxValueToEEPROM(int16_t value) {
    EEPROM.put(ADDR_MAX_VALUE, value);
    EEPROM.commit();
}

uint32_t readDelayFromEEPROM() {
    uint32_t value;
    EEPROM.get(ADDR_DELAY, value);
    if (value == 0xFFFFFFFF) { // Uninitialized check
        writeDelayToEEPROM(30000);
        return 30000;
    }
    return value;
}

void writeDelayToEEPROM(uint32_t value) {
    EEPROM.put(ADDR_DELAY, value);
    EEPROM.commit();
}