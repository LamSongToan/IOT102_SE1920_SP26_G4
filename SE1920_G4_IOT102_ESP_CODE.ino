/************************************************************
 SMART HOME - ESP32 ALL-IN-ONE CONTROLLER
 -----------------------------------------------------------
 Responsibilities:
 1. Read sensors
 2. Process logic (AUTO / MANUAL)
 3. Control actuators
 4. Connect to WiFi + Blynk
************************************************************/


/******************** BLYNK CONFIG *************************/
#define BLYNK_TEMPLATE_ID "TMPL25Up7F9m0"
#define BLYNK_TEMPLATE_NAME "Smart Home Group2"
#define BLYNK_AUTH_TOKEN "yriFx32G9VRly2iVgl0CBN9SGsqGnPtJ"


/******************** LIBRARIES ****************************/
#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <OneWire.h>
#include <DallasTemperature.h>


/******************** WIFI *********************************/
char ssid[] = "123";
char pass[] = "12345678";


/******************** DEBUG *******************************/
bool DEBUG_MODE = true;

/******************** CONNECTION STATUS ********************/
bool blynkConnected = false;
unsigned long lastReconnectAttempt = 0;
const unsigned long RECONNECT_INTERVAL = 5000;

/******************** SYSTEM TIMING ***********************/
const unsigned long DATA_INTERVAL = 1000;
unsigned long lastRun = 0;


/******************** SENSOR PINS *************************/

// PIR Motion Sensor
#define PIR_PIN 13  // digital input, stable

// MQ2 Gas Sensor (analog)
#define MQ2_PIN 36  // ADC1, INPUT ONLY (perfect)

// LDR Sensors (4x)
#define LDR1 32  // ADC1
#define LDR2 33  // ADC1
#define LDR3 34  // ADC1
#define LDR4 35  // ADC1

// Temperature DS18B20
#define ONE_WIRE_BUS 4  // digital, OK (remember 4.7k resistor)
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature DS18B20(&oneWire);


/******************** OUTPUT PINS *************************/

// Room Light LED
#define GREEN_LED 14  // safe GPIO

// RGB LED (status)
#define RGB_R 25  // PWM capable
#define RGB_G 26  // PWM capable
#define RGB_B 27  // PWM capable

// Buzzer
#define BUZZER_RELAY 18  // stable output

// Relay Module (Fan power / main control)
#define FAN_RELAY 19  // stable output

// Fan Driver (L9110 module)
#define FAN_INA 21  // direction control
#define FAN_INB 22  // direction control


/******************** THRESHOLDS **************************/
int LIGHT_THRESHOLD = 40;
int GAS_THRESHOLD = 600;
float TEMPERATURE_THRESHOLD = 30;


/******************** SYSTEM VARIABLES ********************/
String status = "NORMAL";
int mode = 0;

int fanOverride = 0;
int lightOverride = 0;
int buzzerOverride = 1;


/******************** SENSOR VALUES ***********************/
int motion = 0;
int gasValue = 0;
float tempC = 0;
int brightness = 0;


/******************** GAS WARNING *************************/
bool gasIconState = false;
bool gasAlertSent = false;
unsigned long lastBlink = 0;
const int BLINK_INTERVAL = 500;


/************************************************************
 SETUP
************************************************************/
void setup() {

  Serial.begin(9600);

  WiFi.begin(ssid, pass);
  Blynk.config(BLYNK_AUTH_TOKEN);
  DS18B20.begin();

  pinMode(PIR_PIN, INPUT);

  pinMode(LDR1, INPUT);
  pinMode(LDR2, INPUT);
  pinMode(LDR3, INPUT);
  pinMode(LDR4, INPUT);

  pinMode(GREEN_LED, OUTPUT);

  pinMode(RGB_R, OUTPUT);
  pinMode(RGB_G, OUTPUT);
  pinMode(RGB_B, OUTPUT);

  pinMode(FAN_RELAY, OUTPUT);
  pinMode(BUZZER_RELAY, OUTPUT);

  pinMode(FAN_INA, OUTPUT);
  pinMode(FAN_INB, OUTPUT);

  debugPrint("ESP32 Smart Home Ready");
}


/************************************************************
 MAIN LOOP
************************************************************/
void loop() {

  handleConnection();

  if (blynkConnected) {
    Blynk.run();
  }

  if (millis() - lastRun >= DATA_INTERVAL) {

    lastRun = millis();

    readSensors();

    processSystemLogic();

    controlActuators();

    // Only send if connected
    if (blynkConnected) {
      sendToBlynk();
      runWarningSystems();
    }

    runDebugMonitor();
  }
}


/************************************************************
 READ SENSORS
************************************************************/
void readSensors() {

  motion = digitalRead(PIR_PIN);

  gasValue = analogRead(MQ2_PIN);

  DS18B20.requestTemperatures();
  tempC = DS18B20.getTempCByIndex(0);

  int l1 = analogRead(LDR1);
  int l2 = analogRead(LDR2);
  int l3 = analogRead(LDR3);
  int l4 = analogRead(LDR4);

  // Convert to percentage (0–100)
  int p1 = map(l1, 0, 4095, 0, 100);
  int p2 = map(l2, 0, 4095, 0, 100);
  int p3 = map(l3, 0, 4095, 0, 100);
  int p4 = map(l4, 0, 4095, 0, 100);

  int avg = (p1 + p2 + p3 + p4) / 4;
  brightness = avg;
}


/************************************************************
 SYSTEM LOGIC
************************************************************/
void processSystemLogic() {

  if (gasValue > GAS_THRESHOLD) {
    status = "GAS";
  }

  else if (motion == HIGH) {
    status = "MOTION";
  }

  else {
    status = "NORMAL";
  }
}


/************************************************************
 CONTROL ACTUATORS
************************************************************/
void controlActuators() {

  // ================= GAS MODE =================
  if (status == "GAS") {

    // RGB = RED
    digitalWrite(RGB_R, HIGH);
    digitalWrite(RGB_G, LOW);
    digitalWrite(RGB_B, LOW);

    // TURN OFF LIGHT
    digitalWrite(GREEN_LED, LOW);

    // Stop fan
    digitalWrite(FAN_RELAY, LOW);
    digitalWrite(FAN_INA, LOW);
    digitalWrite(FAN_INB, LOW);

    // Buzzer
    digitalWrite(BUZZER_RELAY, buzzerOverride);
  }

  // ================= MOTION MODE =================
  else if (status == "MOTION") {

    // RGB = BLUE
    digitalWrite(RGB_R, LOW);
    digitalWrite(RGB_G, LOW);
    digitalWrite(RGB_B, HIGH);

    // Fan control (temperature based)
    if (tempC > TEMPERATURE_THRESHOLD) {
      digitalWrite(FAN_RELAY, HIGH);
      digitalWrite(FAN_INA, LOW);
      digitalWrite(FAN_INB, HIGH);
    } else {
      digitalWrite(FAN_RELAY, LOW);
      digitalWrite(FAN_INA, LOW);
      digitalWrite(FAN_INB, LOW);
    }

    // Light control (brightness based)
    if (brightness < LIGHT_THRESHOLD) {
      digitalWrite(GREEN_LED, HIGH);
    } else {
      digitalWrite(GREEN_LED, LOW);
    }
  }

  // ================= NORMAL MODE =================
  else {

    // RGB = GREEN
    digitalWrite(RGB_R, LOW);
    digitalWrite(RGB_G, HIGH);
    digitalWrite(RGB_B, LOW);

    digitalWrite(GREEN_LED, LOW);
    digitalWrite(BUZZER_RELAY, LOW);

    digitalWrite(FAN_RELAY, LOW);
    digitalWrite(FAN_INA, LOW);
    digitalWrite(FAN_INB, LOW);
  }


  // ================= MANUAL MODE =================
  if (mode == 1) {

    // Manual fan
    if (fanOverride == 1) {
      digitalWrite(FAN_RELAY, HIGH);
      digitalWrite(FAN_INA, LOW);
      digitalWrite(FAN_INB, HIGH);
    } else {
      digitalWrite(FAN_RELAY, LOW);
      digitalWrite(FAN_INA, LOW);
      digitalWrite(FAN_INB, LOW);
    }

    // Manual light
    if (lightOverride == 1) {
      digitalWrite(GREEN_LED, HIGH);
    } else {
      digitalWrite(GREEN_LED, LOW);
    }
  }
}

/************************************************************
 HANDLE CONNECTION
************************************************************/
void handleConnection() {

  if (Blynk.connected()) {

    if (!blynkConnected) {
      debugPrint("Blynk Connected!");
      blynkConnected = true;
    }

  } else {

    if (blynkConnected) {
      debugPrint("Blynk Disconnected! -> Switching to LOCAL MODE");
      // RGB = WHITE
      digitalWrite(RGB_R, HIGH);
      digitalWrite(RGB_G, HIGH);
      digitalWrite(RGB_B, HIGH);
      blynkConnected = false;
    }

    // Try reconnect every 5s
    if (millis() - lastReconnectAttempt > RECONNECT_INTERVAL) {

      debugPrint("Trying to reconnect...");
      // RGB = YELLOW
      digitalWrite(RGB_R, HIGH);
      digitalWrite(RGB_G, HIGH);
      digitalWrite(RGB_B, LOW);
      Blynk.connect();

      lastReconnectAttempt = millis();
    }
  }
}

/************************************************************
 SEND DATA TO BLYNK
************************************************************/
void sendToBlynk() {

  Blynk.virtualWrite(V2, gasValue);
  Blynk.virtualWrite(V3, tempC);
  Blynk.virtualWrite(V4, motion);
  Blynk.virtualWrite(V7, brightness);
  Blynk.virtualWrite(V10, status);
}


/************************************************************
 WARNING SYSTEMS
************************************************************/
void runWarningSystems() {

  // GAS WARNING
  if (gasValue > GAS_THRESHOLD) {

    if (millis() - lastBlink > BLINK_INTERVAL) {
      gasIconState = !gasIconState;
      Blynk.virtualWrite(V5, gasIconState);
      lastBlink = millis();
    }

    if (!gasAlertSent) {
      Blynk.logEvent("gas_alert", "⚠️ Gas leak detected!");
      gasAlertSent = true;
    }
  }

  else {
    gasAlertSent = false;
    Blynk.virtualWrite(V5, 0);
  }

  // TEMP WARNING
  Blynk.virtualWrite(V6, tempC > TEMPERATURE_THRESHOLD);
}


/************************************************************
 DEBUG MONITOR
************************************************************/
void runDebugMonitor() {

  if (!DEBUG_MODE) return;

  Serial.println("------ SYSTEM STATUS ------");

  Serial.print("Motion: ");
  Serial.println(motion);
  Serial.print("Gas: ");
  Serial.println(gasValue);
  Serial.print("Temp: ");
  Serial.println(tempC);
  Serial.print("Brightness: ");
  Serial.print(brightness);
  Serial.println("%");
  Serial.print("Status: ");
  Serial.println(status);

  Serial.print("Mode: ");
  Serial.println(mode == 0 ? "AUTO" : "MANUAL");

  Serial.println("---------------------------");
}


/************************************************************
 DEBUG PRINT
************************************************************/
void debugPrint(String msg) {

  if (!DEBUG_MODE) return;
  Serial.println(msg);
}


/************************************************************
 BLYNK CONTROLS
************************************************************/
BLYNK_WRITE(V0) {
  lightOverride = param.asInt();
}
BLYNK_WRITE(V1) {
  fanOverride = param.asInt();
}
BLYNK_WRITE(V8) {
  mode = param.asInt();
}
BLYNK_WRITE(V9) {
  buzzerOverride = param.asInt();
}