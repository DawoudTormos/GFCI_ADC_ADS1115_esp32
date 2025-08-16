#include <Arduino.h>
#include <Adafruit_ADS1X15.h>

#define LED_BUILTIN 0
#define Buzzer 16
Adafruit_ADS1115 ads;

// Pin connected to the ALERT/RDY signal for new sample notification
constexpr int NEW_READING_READY_PIN = 19;
constexpr int BREAKER_PIN = 15;

// Deafult values of these eeprom variables
/*
constexpr int16_t MAX_VALUE_IN_SAFE_RANGE = 800;
constexpr uint32_t DELAY_OF_CUTOFF = 30000;
*/

int16_t MAX_VALUE_IN_SAFE_RANGE = 800;
uint32_t DELAY_OF_CUTOFF = 30000;
//float multiplier = 0.0078125F; // ADS1115 @ +/- 6.144V gain (16-bit results)

volatile bool new_data = false;//new_data availabilty. it is changed by an interrupt sent by the module


bool powerState = true;//The state of the power.


#ifndef IRAM_ATTR
#define IRAM_ATTR
#endif

void IRAM_ATTR NewDataReadyISR() {
    new_data = true;
}
bool isNewDataavailable();
int16_t getMeasuredCurrentInCT_peak();
int16_t currentValue_peakToRms(int16_t peakValue);
bool checkIfInSafeRange(int16_t rmsVal);
void cutOffPower();
void turnOnPower();
void adcErrorBlinking();


void setup() {
    pinMode(BREAKER_PIN, OUTPUT);
    pinMode(LED_BUILTIN, OUTPUT);
    pinMode(Buzzer, OUTPUT);
    digitalWrite(BREAKER_PIN, !powerState);

    Serial.begin(250000);
    Serial.println("Hello!");

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

    // Initialize stage timers
}



void loop() {
    
    if (!powerState) {
        uint32_t start = millis();
        while (millis() - start < 30000) {
            delay(1);
            digitalWrite(LED_BUILTIN, ((millis() / 500) % 2 == 0));
            digitalWrite(Buzzer, ((millis() / 1000) % 2 == 0));
        }
        digitalWrite(Buzzer, 0);
        turnOnPower();
    }else{
        digitalWrite(Buzzer, 0);
        digitalWrite(LED_BUILTIN, ((millis() / 500) % 2 == 0));

    }


    //uint32_t startOfMeasurement = millis();

    if(isNewDataavailable()){
       uint16_t newPeak = getMeasuredCurrentInCT_peak();
       uint16_t newRms = currentValue_peakToRms(newPeak);

       if(!checkIfInSafeRange(newRms)){
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





bool isNewDataavailable(){
    return new_data;
}

int16_t getMeasuredCurrentInCT_peak(){
    int16_t x = ads.getLastConversionResults();
    x = abs(x);
    new_data = false;
    return x;
}

int16_t currentValue_peakToRms(int16_t peakValue){
    int16_t rmsVal = int16_t( ((float) peakValue) / sqrt(2));
    return rmsVal;
}

bool checkIfInSafeRange(int16_t rmsVal){
    return (rmsVal < MAX_VALUE_IN_SAFE_RANGE );
}

void cutOffPower(){
    powerState = false;
    digitalWrite(BREAKER_PIN, !powerState);
}

void turnOnPower(){
    powerState = true;
    digitalWrite(BREAKER_PIN, !powerState);
}







void adcErrorBlinking(){
        //showing this adc error in a pattern of blinking
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