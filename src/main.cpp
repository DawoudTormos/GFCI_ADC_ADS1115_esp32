#include <Arduino.h>
#include <Adafruit_ADS1X15.h>
#include <EEPROM.h>
#include <WiFi.h>
#include <WebServer.h>

#define EEPROM_SIZE 64
#define ADDR_MAX_VALUE 0
#define ADDR_DELAY 4
#define REST_API_PORT 80

// Safety-related constants
#define MIN_SAFE_CURRENT 0
#define MAX_SAFE_CURRENT 2500
#define MIN_DELAY_MS 5000
#define MAX_DELAY_MS 300000

#define LED_BUILTIN 0
#define Buzzer 16
Adafruit_ADS1115 ads;

// WiFi and Web Server
WebServer server(REST_API_PORT);

// Pins
constexpr int NEW_READING_READY_PIN = 19;
constexpr int BREAKER_PIN = 15;

// WiFi Credentials - REPLACE with your network details
const char* ssid = "Zahi";
const char* password = "test12345678";

// Static IP configuration
IPAddress staticIP(192, 168, 77, 226);
IPAddress gateway(192, 168, 77, 1);
IPAddress subnet(255, 255, 255, 0);

// Shared variables (use volatile for cross-core access)
volatile int16_t MAX_VALUE_IN_SAFE_RANGE = 800;
volatile uint32_t DELAY_OF_CUTOFF = 30000;

volatile bool new_data = false;
volatile bool powerState = true;

#ifndef IRAM_ATTR
#define IRAM_ATTR
#endif

int16_t cachedLastConversionResult = 0;

void IRAM_ATTR NewDataReadyISR() {
    new_data = true;
    cachedLastConversionResult = ads.getLastConversionResults();
}
int16_t getCachedLastConversionResult(){
    return cachedLastConversionResult;
}

// Function prototypes for core 1
bool isNewDataavailable();
int16_t getMeasuredCurrentInCT_peak_InCache();
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

// REST API Task (runs on core 0)
void restApiTask(void* pvParameters) {
    // Configure WiFi as client
    WiFi.mode(WIFI_STA);
    WiFi.config(staticIP, gateway, subnet);
    
    Serial.print("Connecting to WiFi");
    WiFi.begin(ssid, password);
    
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    
    Serial.println("\nConnected! IP: " + WiFi.localIP().toString());

    // API Endpoints
    server.on("/", HTTP_GET, []() {
        server.send(200, "text/plain", "CFGI Device Ready");
    });
    
    server.on("/status", HTTP_GET, []() {
        String status = "Power: " + String(powerState ? "ON" : "OFF") + "\n";
        status += "Max Current: " + String(MAX_VALUE_IN_SAFE_RANGE) + " \n";
        status += "Cutoff Delay: " + String(DELAY_OF_CUTOFF) + " ms";
        server.send(200, "text/plain", status);
    });

    // POST endpoint to update MaxValue in EEPROM
    server.on("/maxvalue", HTTP_POST, []() {
        if (server.hasArg("value")) {
            int newValue = server.arg("value").toInt();
            
            // Validate input using safety constants
            if (newValue >= MIN_SAFE_CURRENT && newValue <= MAX_SAFE_CURRENT) {
                writeMaxValueToEEPROM(newValue);
                server.send(200, "application/json",
                    "{\"success\":true,\"message\":\"MaxValue updated to " + String(newValue) + "\",\"value\":" + String(MAX_VALUE_IN_SAFE_RANGE) + "}");
            } else {
                server.send(400, "application/json",
                    "{\"success\":false,\"error\":\"Invalid value. Must be between " + String(MIN_SAFE_CURRENT) + "-" + String(MAX_SAFE_CURRENT) + "\"}");
            }
        } else {
            server.send(400, "application/json",
                "{\"success\":false,\"error\":\"Missing 'value' parameter\"}");
        }
    });

    // POST endpoint to update Delay in EEPROM
    server.on("/delay", HTTP_POST, []() {
        if (server.hasArg("value")) {
            uint32_t newDelay = server.arg("value").toInt();
            
            // Validate input using safety constants
            if (newDelay >= MIN_DELAY_MS && newDelay <= MAX_DELAY_MS) {
                writeDelayToEEPROM(newDelay);
                server.send(200, "application/json",
                    "{\"success\":true,\"message\":\"Delay updated to " + String(newDelay) + " ms\",\"value\":" + String(DELAY_OF_CUTOFF) + "}");
            } else {
                server.send(400, "application/json",
                    "{\"success\":false,\"error\":\"Invalid delay. Must be between " + String(MIN_DELAY_MS) + "-" + String(MAX_DELAY_MS) + " ms\"}");
            }
        } else {
            server.send(400, "application/json",
                "{\"success\":false,\"error\":\"Missing 'value' parameter\"}");
        }
    });

    server.begin();
    Serial.println("REST API started on core " + String(xPortGetCoreID()));

    for (;;) {
        server.handleClient();
        delay(1);
    }
}

void setup() {
    // Setup hardware
    pinMode(BREAKER_PIN, OUTPUT);
    pinMode(LED_BUILTIN, OUTPUT);
    pinMode(Buzzer, OUTPUT);
    digitalWrite(BREAKER_PIN, false); // Start with power off
    Wire.setClock(400000);        // 4x faster I2C

    // Start serial
    Serial.begin(250000);
    Serial.println("Starting CFGI Device...");


    // Initialize EEPROM
    EEPROM.begin(EEPROM_SIZE);
    MAX_VALUE_IN_SAFE_RANGE = readMaxValueFromEEPROM();
    DELAY_OF_CUTOFF = readDelayFromEEPROM();

    Serial.println("Current Monitoring Initialized");

    // Configure ADC
    ads.setGain(GAIN_SIXTEEN);
    ads.setDataRate(RATE_ADS1115_860SPS);

    if (!ads.begin()) {
        Serial.println("ADC Init Failed!");
        while (1) adcErrorBlinking();
    }

    // Setup interrupt
    pinMode(NEW_READING_READY_PIN, INPUT);
    attachInterrupt(digitalPinToInterrupt(NEW_READING_READY_PIN), NewDataReadyISR, FALLING);

    // Start ADC
    ads.startADCReading(ADS1X15_REG_CONFIG_MUX_DIFF_0_1, true);
    
    // Start REST API on core 0
    xTaskCreatePinnedToCore(
        restApiTask,    // Task function
        "REST API",     // Name
        50000,          // Stack size
        NULL,           // Parameters
        1,              // Priority
        NULL,           // Task handle
        0               // Core 0
    );
    
    Serial.println("Main loop running on core " + String(xPortGetCoreID()));
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

    if (isNewDataavailable()) {
        uint16_t newPeak = getMeasuredCurrentInCT_peak_InCache();
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





bool isNewDataavailable() {
    return new_data;
}

int16_t getMeasuredCurrentInCT_peak_InCache() {
    int16_t x = getCachedLastConversionResult();
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
    delay(3000);
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
    MAX_VALUE_IN_SAFE_RANGE = value;
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
    DELAY_OF_CUTOFF = value;
}