/* CODE FOR LM35DT & PWM fan
LM35DT basics https://arduinogetstarted.com/tutorials/arduino-lm35-temperature-sensor
PWM fan with LM35DT https://www.youtube.com/watch?v=A0j7BsLcnxg&t=1403s
*/

#include <Arduino.h>
#include <TFT_eSPI.h>

TFT_eSPI tft = TFT_eSPI();

#define PIN_LM35  1 // 1 pin for LM35DT, for reading temp
#define TACH_PIN  2 // for reading fan speed
#define PWM_PIN   3 // for controlling fan speed
#define ON_BUTTON 21 //turn on system

//PWM settings  
const int PWM_FREQ = 25000;    // 25 kHz frequency for computer fans
const int PWM_RESOLUTION = 8;  // 8-bit resolution (0-255)

// Temperature control settings (configurable)
float TEMP_FAN_ON = 35.0;      // Temperature to start fan (Celsius)
float TEMP_FULL_SPEED = 60.0;  // Temperature for full speed (Celsius)

//Startup settings 
const unsigned long STARTUP_TIME = 45000;   // 45 seconds
unsigned long fanStartTime = 0;
bool wait = false;
bool startupPhase = false;

// Tachometer variables
volatile unsigned long tachPulseCount = 0; // Count of tachometer pulses
unsigned long lastTachTime = 0; // Last time tachometer was read
const unsigned long TACH_SAMPLE_TIME = 1000;  // Sample period in milliseconds

// System variables
float currentTemp = 0.0;
int fanSpeedPercent = 0;
bool fanFailure = false;
bool manualMode = false;
bool systemOn = false;
bool previousButtonState = HIGH;
int dutyCycle = 0;

// Interrupt service routine for tachometer
void IRAM_ATTR tachISR() {
  tachPulseCount = tachPulseCount + 1; // Increment pulse count on each tachometer pulse
}

void setup() {
  Serial.begin(115200);

  // Initialise display
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);

  //LM35DT 
  analogReadResolution(12);   // ESP32-S3 ADC resolution

  //Button
  pinMode(ON_BUTTON, INPUT_PULLUP); // Set button pin as input with pull-up resistor 

  //PWM fan 
  ledcAttach(PWM_PIN, PWM_FREQ, PWM_RESOLUTION); // attach PWM channel to pin
  ledcWrite(PWM_PIN, 0); // set initial duty cycle to 0 (fan off)

  // Tachometer fan 
  pinMode(TACH_PIN, INPUT_PULLUP); // Set tachometer pin as input with pull-up resistor
  attachInterrupt(digitalPinToInterrupt(TACH_PIN), tachISR, FALLING); // Attach interrupt to tachometer pin on falling edge

  lastTachTime = millis();   // Initialize timing
 
}

void loop() {
  // ---------------------
  // 1. Check on/off button
  //----------------------
    bool buttonState = digitalRead(ON_BUTTON);

  // Detect button being pressed
  if (buttonState == LOW && previousButtonState == HIGH) {
    systemOn = !systemOn;

    // If system has just been turned ON
    if (systemOn) {
      fanStartTime = millis();
      startupPhase = true;
      Serial.println("System ON - starting 45 second fan ramp");
    }

    // If system has been turned OFF
    else {
      startupPhase = false;
      Serial.println("System OFF");
      dutyCycle = 0;
      ledcWrite(PWM_PIN, 0);
    }

    delay(50); // simple button debounce
  }
  previousButtonState = buttonState;

//---------------------
//2. read temperature
//---------------------
  // Read LM35 output voltage in millivolts
  int milliVolt = analogReadMilliVolts(PIN_LM35);

  // LM35 output = 10 mV per °C
  float tempC = milliVolt / 10.0;
 
  //---------------------
  //3. Control fan speed
  //---------------------
  // if system is on, gradually turn on fan until max for 45 seconds then control PWM based on temperature
if (systemOn) {

    unsigned long elapsedTime = millis() - fanStartTime;
    // FIRST 45 SECONDS
    if (elapsedTime < STARTUP_TIME) {
      startupPhase = true;
      dutyCycle = (elapsedTime * 255) / STARTUP_TIME;
    }

    else {
      startupPhase = false;
      // Below 35 C
      if (currentTemp < TEMP_FAN_ON) {
        dutyCycle = 0;
      }
      // Between 35 C and 60 C
      else if (currentTemp < TEMP_FULL_SPEED) {
        dutyCycle =
          (int)((currentTemp - TEMP_FAN_ON) * 255.0 / (TEMP_FULL_SPEED - TEMP_FAN_ON));
      }
      // Above 60 C
      else {
        dutyCycle = 255;
      }

    }
//----------------------
// 4. CALCULATE FAN SPEED %
//----------------------
  fanSpeedPercent = map(dutyCycle, 0, 255, 0, 100);

//---------------------
// 5. CALCULATE FAN RPM
//----------------------
  unsigned long currentTime = millis();
  if (currentTime - lastTachTime >= TACH_SAMPLE_TIME) {
    unsigned long rpm =
      (tachPulseCount * 60000)/(TACH_SAMPLE_TIME * 2);

    Serial.print("Temperature: ");
    Serial.print(currentTemp);
    Serial.print(" C | PWM: ");
    Serial.print(dutyCycle);
    Serial.print(" | Speed: ");
    Serial.print(fanSpeedPercent);
    Serial.print("% | RPM: ");
    Serial.println(rpm);

    tachPulseCount = 0;
    lastTachTime = currentTime;
  }

//---------------------
// 6. DISPLAY
//---------------------

  tft.fillScreen(TFT_BLACK);
  tft.setCursor(10, 10);
  tft.print("Temperature:");
  tft.setCursor(10, 40);
  tft.print(currentTemp);
  tft.print(" C");
  tft.setCursor(10, 70);
  tft.print("Fan: ");
  tft.print(fanSpeedPercent);
  tft.print("%");
  tft.setCursor(10, 100);
  if (!systemOn) {
    tft.print("System OFF");
  }
  else if (startupPhase) {
    tft.print("Fan startup");
  }

  else {
    tft.print("Temp control");
  }

  delay(100);
}
}