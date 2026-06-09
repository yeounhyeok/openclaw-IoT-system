// OpenClaw Physical Hand - Wokwi demo + Arduino Uno R4 WiFi remote control
// Onboard BLE: Android app writes short text commands to the command characteristic.
// HC-05: Android app writes the same text commands over Bluetooth Classic SPP.
// WiFi: Uno R4 WiFi connects to the local network and reports connection state.
// WSS: Home Assistant WebSocket command path over WiFi.
// HTTP: Home Assistant REST API status path over WiFi.
// MQTT: optional Home Assistant command path over WiFi.
//
// UNO R4 WiFi cannot keep Bluetooth and WiFi active at the same time.
// For simultaneous WiFi + Android Bluetooth control, use HC-05 on Serial1.

#include <Servo.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT11.h>   // DHT11 library: readTemperatureHumidity(temp, humi) -> 0 on success
#include "arduino_secrets.h"

// Built-in BLE demo mode. Set to 0 when using WiFi/MQTT mode.
#ifndef ENABLE_BLE
#define ENABLE_BLE 0
#endif

// HC-05 Bluetooth Classic UART mode. Works with WiFi/MQTT because it uses Serial1.
#ifndef ENABLE_HC05
#define ENABLE_HC05 1
#endif

// WiFi demo mode.
#ifndef ENABLE_WIFI
#define ENABLE_WIFI 1
#endif

// Set ENABLE_HA_HTTP to 1 to connect directly to Home Assistant REST API.
#ifndef ENABLE_HA_HTTP
#define ENABLE_HA_HTTP 1
#endif

// Set ENABLE_HA_WS to 1 for faster Home Assistant command events over WebSocket.
#ifndef ENABLE_HA_WS
#define ENABLE_HA_WS 1
#endif

// Cloudflare/NPM public endpoint uses HTTPS on port 443.
#ifndef HA_USE_SSL
#define HA_USE_SSL 1
#endif

// Set ENABLE_MQTT to 1 if Home Assistant must control the Arduino over WiFi.
#ifndef ENABLE_MQTT
#define ENABLE_MQTT 0
#endif

#if ENABLE_BLE && (ENABLE_WIFI || ENABLE_HA_HTTP || ENABLE_HA_WS || ENABLE_MQTT)
#error "UNO R4 WiFi cannot use built-in BLE and WiFi at the same time. Use HC-05 on Serial1 for simultaneous Bluetooth + WiFi."
#endif

#if (ENABLE_HA_HTTP || ENABLE_HA_WS) && !ENABLE_WIFI
#error "ENABLE_HA_HTTP requires ENABLE_WIFI 1."
#endif

#if ENABLE_HA_WS && !ENABLE_HA_HTTP
#error "ENABLE_HA_WS currently requires ENABLE_HA_HTTP 1 for status posting and helper reset."
#endif

#if ENABLE_MQTT && !ENABLE_WIFI
#error "ENABLE_MQTT requires ENABLE_WIFI 1."
#endif

#if ENABLE_BLE
#include <ArduinoBLE.h>
#endif

#if ENABLE_WIFI || ENABLE_HA_HTTP || ENABLE_HA_WS || ENABLE_MQTT
#include <WiFiS3.h>
#include <string.h>
#endif

#if ENABLE_MQTT
#include <ArduinoMqttClient.h>
#endif

#define DHTPIN 2
#define CDS_SENSOR_PIN A0
#define LCD_DARK_THRESHOLD 300

#define ALARM_BUTTON 3
#define RGB_G 5
#define RGB_B 6
#define BUZZER_PIN 8
#define SERVO_PC_PIN 9
#define PIR_PIN 11
#define RGB_R 12

#define HC05_BAUD 9600
#define HA_HTTP_TIMEOUT_MS 1200
#define HA_WS_READ_TIMEOUT_MS 250
#define HA_FAST_POLL_MS 500
#define HA_COMMAND_POLL_MS 1500
#define HA_STATUS_POST_MS 30000
#define HA_WS_RECONNECT_MS 5000
#define HA_WS_PING_MS 30000
#define HA_LOCAL_ALARM_GRACE_MS 1500
#define PC_SERVO_PRESS_MS 420
#define PC_SERVO_RECOVER_MS 200
#define PC_SERVO_COOLDOWN_MS 180
#define BEEP_ON_MS 90
#define BEEP_OFF_MS 90
#define PC_SERVO_IDLE 0
#define PC_SERVO_PRESSING 1
#define PC_SERVO_RECOVERING 2
#define PC_SERVO_COOLDOWN 3

#if ENABLE_WIFI || ENABLE_HA_HTTP || ENABLE_HA_WS || ENABLE_MQTT
#ifndef SECRET_WIFI_SSID
#define SECRET_WIFI_SSID "YOUR_WIFI_SSID"
#endif
#ifndef SECRET_WIFI_PASSWORD
#define SECRET_WIFI_PASSWORD "YOUR_WIFI_PASSWORD"
#endif

char WIFI_SSID[] = SECRET_WIFI_SSID;
char WIFI_PASSWORD[] = SECRET_WIFI_PASSWORD;
#endif

#if ENABLE_HA_HTTP
#ifndef SECRET_HA_HOST
#define SECRET_HA_HOST "ha.yeoun.org"
#endif
#ifndef SECRET_HA_PORT
#define SECRET_HA_PORT 443
#endif
#ifndef SECRET_HA_TOKEN
#define SECRET_HA_TOKEN "YOUR_HOME_ASSISTANT_LONG_LIVED_ACCESS_TOKEN"
#endif

char HA_HOST[] = SECRET_HA_HOST; // Home Assistant IP/domain, without http:// or https://
int HA_PORT = SECRET_HA_PORT;
char HA_TOKEN[] = SECRET_HA_TOKEN;

const char HA_STATUS_ENTITY[] = "sensor.openclaw_status";
const char HA_MOTION_ENTITY[] = "binary_sensor.openclaw_motion";
const char HA_COMMAND_ENTITY[] = "input_text.openclaw_command";
const char HA_ALARM_ENTITY[] = "input_boolean.openclaw_alarm";
const char HA_PC_POWER_ENTITY[] = "input_boolean.openclaw_pc_power";
const char HA_PC_POWER_BUTTON_ENTITY[] = "input_button.openclaw_pc_power";
#endif

#if ENABLE_MQTT
char MQTT_BROKER[] = "192.168.0.10"; // Home Assistant / Mosquitto broker IP or domain
int MQTT_PORT = 1883;
char MQTT_USER[] = "YOUR_MQTT_USER";
char MQTT_PASSWORD[] = "YOUR_MQTT_PASSWORD";

const char MQTT_CLIENT_ID[] = "openclaw-r4";
const char MQTT_COMMAND_TOPIC[] = "openclaw/cmd";
const char MQTT_STATUS_TOPIC[] = "openclaw/status";
const char MQTT_MOTION_TOPIC[] = "openclaw/sensor/motion";
#endif

Servo pcPowerServo;
LiquidCrystal_I2C lcd(0x27, 16, 2);
DHT11 dht11(DHTPIN);

bool alarmOn = false;
bool lcdBacklightOn = true;
unsigned long lastSensorRead = 0;
unsigned long lastStatusPublish = 0;
unsigned long lastBlePollLog = 0;
unsigned long lastHc05ByteAt = 0;
unsigned long lastWifiStatusLog = 0;
unsigned long lastWifiAttempt = 0;
unsigned long lastHaStatusPost = 0;
unsigned long lastHaCommandPoll = 0;
unsigned long lastHaFastPoll = 0;
unsigned long lastHaWsReconnect = 0;
unsigned long lastHaWsPing = 0;
unsigned long lastMqttAttempt = 0;
unsigned long lastAlarmButtonEvent = 0;
unsigned long lastLocalAlarmChangeAt = 0;
unsigned long pcPowerServoPhaseAt = 0;
unsigned long beepPhaseAt = 0;
int lastPirState = LOW;
int lastAlarmButtonState = HIGH;
float lastTemperature = NAN;
float lastHumidity = NAN;
bool wifiWasConnected = false;
String hc05Buffer = "";
String lastHaCommand = "";
String lastHaAlarmState = "";
String lastHaPcPowerButtonState = "";
bool pendingHaStatusPost = false;
bool pendingHaMotionOn = false;
bool pendingHaAlarmSync = false;
bool pendingHaPcPowerReset = false;
bool beepToneOn = false;
bool pcPowerServoQueued = false;
byte pcPowerServoState = PC_SERVO_IDLE;
byte pcPowerBeepsAfterServo = 0;
byte beepPulsesRemaining = 0;
byte haFastPollStep = 0;
volatile byte alarmButtonInterruptCount = 0;
bool haWsSubscribed = false;
int haWsNextId = 1;

#if ENABLE_BLE
BLEService openClawService("19b10000-e8f2-537e-4f6c-d104768a1214");
BLEStringCharacteristic commandCharacteristic(
  "19b10001-e8f2-537e-4f6c-d104768a1214",
  BLEWrite,
  40
);
BLEStringCharacteristic statusCharacteristic(
  "19b10002-e8f2-537e-4f6c-d104768a1214",
  BLERead | BLENotify,
  96
);
#endif

#if ENABLE_MQTT
WiFiClient wifiClient;
MqttClient mqttClient(wifiClient);
#endif

#if ENABLE_HA_WS
#if HA_USE_SSL
WiFiSSLClient haWsClient;
#else
WiFiClient haWsClient;
#endif
#endif

void setRgb(bool r, bool g, bool b) {
  digitalWrite(RGB_R, r ? HIGH : LOW);
  digitalWrite(RGB_G, g ? HIGH : LOW);
  digitalWrite(RGB_B, b ? HIGH : LOW);
}

void beep(int count) {
  if (count <= 0) {
    return;
  }

  byte requested = (byte)(count > 8 ? 8 : count);
  if (beepToneOn) {
    noTone(BUZZER_PIN);
    beepToneOn = false;
  }
  beepPulsesRemaining = requested;
  beepPhaseAt = millis() - BEEP_OFF_MS;
}

void stopBeeper() {
  if (beepToneOn) {
    noTone(BUZZER_PIN);
  }
  beepToneOn = false;
  beepPulsesRemaining = 0;
  beepPhaseAt = 0;
}

void pollBeeper() {
  if (beepPulsesRemaining == 0 && !beepToneOn) {
    return;
  }

  unsigned long now = millis();
  if (!beepToneOn && now - beepPhaseAt >= BEEP_OFF_MS) {
    tone(BUZZER_PIN, 1200);
    beepToneOn = true;
    beepPhaseAt = now;
  } else if (beepToneOn && now - beepPhaseAt >= BEEP_ON_MS) {
    noTone(BUZZER_PIN);
    beepToneOn = false;
    beepPhaseAt = now;
    if (beepPulsesRemaining > 0) {
      beepPulsesRemaining--;
    }
    if (beepPulsesRemaining == 0) {
      beepPhaseAt = 0;
    }
  }
}

void pressServoOnce(Servo &servo) {
  if (pcPowerServoState != PC_SERVO_IDLE) {
    pcPowerServoQueued = true;
    Serial.println("PC power servo busy, queued once");
    return;
  }

  stopBeeper();
  servo.write(90);
  pcPowerServoState = PC_SERVO_PRESSING;
  pcPowerServoPhaseAt = millis();
}

bool isPcPowerServoBusy() {
  return pcPowerServoState != PC_SERVO_IDLE;
}

void pollPcPowerServo() {
  if (pcPowerServoState == PC_SERVO_IDLE) {
    return;
  }

  unsigned long now = millis();
  if (pcPowerServoState == PC_SERVO_PRESSING && now - pcPowerServoPhaseAt >= PC_SERVO_PRESS_MS) {
    pcPowerServo.write(0);
    pcPowerServoState = PC_SERVO_RECOVERING;
    pcPowerServoPhaseAt = now;
    return;
  }

  if (pcPowerServoState == PC_SERVO_RECOVERING && now - pcPowerServoPhaseAt >= PC_SERVO_RECOVER_MS) {
    setRgb(alarmOn, !alarmOn, alarmOn); // purple if alarm armed, green otherwise
    pcPowerServoState = PC_SERVO_COOLDOWN;
    pcPowerServoPhaseAt = now;
    return;
  }

  if (pcPowerServoState == PC_SERVO_COOLDOWN && now - pcPowerServoPhaseAt >= PC_SERVO_COOLDOWN_MS) {
    pcPowerServoState = PC_SERVO_IDLE;
    if (pcPowerServoQueued) {
      pcPowerServoQueued = false;
      pressServoOnce(pcPowerServo);
      pcPowerBeepsAfterServo = 2;
      return;
    }

    if (pcPowerBeepsAfterServo > 0) {
      beep(pcPowerBeepsAfterServo);
      pcPowerBeepsAfterServo = 0;
    }
  }
}

String buildStatusMessage() {
  String status = "alarm=";
  status += alarmOn ? "ON" : "OFF";
  status += ",motion=";
  status += lastPirState == HIGH ? "ON" : "OFF";

#if ENABLE_HC05
  status += ",bt=HC05";
#endif

#if ENABLE_WIFI || ENABLE_HA_HTTP || ENABLE_MQTT
  status += ",wifi=";
  status += WiFi.status() == WL_CONNECTED ? "ON" : "OFF";
#endif

  if (!isnan(lastTemperature) && !isnan(lastHumidity)) {
    status += ",temp=";
    status += String(lastTemperature, 1);
    status += ",humidity=";
    status += String(lastHumidity, 0);
  }

  return status;
}

void updateRemoteStatus();

void pressPcPowerByRemoteCommand(const char *source) {
  Serial.print("PC power servo command from ");
  Serial.println(source);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("PC Power Press");
  lcd.setCursor(0, 1);
  lcd.print(source);
  lcd.print(" cmd");

  setRgb(false, false, true); // blue: remote command executing
  pressServoOnce(pcPowerServo);
  pcPowerBeepsAfterServo = 2;
}

void setAlarm(bool enabled, bool pressPcPower, const char *source) {
  alarmOn = enabled;

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(alarmOn ? "Alarm ON" : "Alarm OFF");
  lcd.setCursor(0, 1);

  if (pressPcPower) {
    lcd.print("PC Power Press");
    setRgb(false, false, true); // blue: PC power command executing
    pressServoOnce(pcPowerServo);
    pcPowerBeepsAfterServo = 2;
  } else {
    lcd.print(source);
    lcd.print(" cmd");
  }

  if (alarmOn) {
    setRgb(true, false, true); // purple: alarm armed
    beep(3);
  } else {
    setRgb(false, true, false); // green: normal
    beep(1);
  }
}

void toggleAlarm() {
  setAlarm(!alarmOn, false, "Button");
}

void onAlarmButtonInterrupt() {
  if (alarmButtonInterruptCount < 10) {
    alarmButtonInterruptCount++;
  }
}

void handleAlarmButtonEvent() {
  Serial.println("ALARM button event");
  toggleAlarm();
  lastLocalAlarmChangeAt = millis();
  lastHaAlarmState = alarmOn ? "on" : "off";
#if ENABLE_HA_HTTP
  pendingHaAlarmSync = true;
#endif
  updateRemoteStatus();
}

void pollAlarmButton() {
  int currentState = digitalRead(ALARM_BUTTON);
  bool fallingEdge = lastAlarmButtonState == HIGH && currentState == LOW;

  noInterrupts();
  byte interruptCount = alarmButtonInterruptCount;
  alarmButtonInterruptCount = 0;
  interrupts();

  bool interruptEdge = interruptCount > 0;
  unsigned long now = millis();

  if ((fallingEdge || interruptEdge) && now - lastAlarmButtonEvent > 250) {
    lastAlarmButtonEvent = now;
    handleAlarmButtonEvent();
  }

  lastAlarmButtonState = currentState;
}

bool handleRemoteCommand(String command, const char *source) {
  command.trim();
  command.toUpperCase();
  command.replace(' ', '_');
  command.replace('-', '_');

  if (command == "PC_POWER" || command == "PC" || command == "POWER") {
    pressPcPowerByRemoteCommand(source);
  } else if (command == "ALARM_TOGGLE" || command == "ALARM") {
    setAlarm(!alarmOn, false, source);
  } else if (command == "ALARM_ON") {
    setAlarm(true, false, source);
  } else if (command == "ALARM_OFF") {
    setAlarm(false, false, source);
  } else if (command == "STATUS") {
    // Only refresh the BLE/MQTT status payload.
  } else {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Unknown cmd");
    lcd.setCursor(0, 1);
    lcd.print(command.substring(0, 16));
    setRgb(true, false, false);
    beep(1);
    return false;
  }

  Serial.print(source);
  Serial.print(" command: ");
  Serial.println(command);
  updateRemoteStatus();
  return true;
}

void updateLcdBacklightByCdsSensor() {
  int lightValue = analogRead(CDS_SENSOR_PIN);

  // Real wiring: 5V -- CDS -- A0 -- 10k resistor -- GND (voltage divider).
  // Bright -> CDS resistance low -> A0 high value.
  // Dark   -> CDS resistance high -> A0 low value.
  if (lightValue < LCD_DARK_THRESHOLD && lcdBacklightOn) {
    lcd.noBacklight();
    lcdBacklightOn = false;
  } else if (lightValue >= LCD_DARK_THRESHOLD && !lcdBacklightOn) {
    lcd.backlight();
    lcdBacklightOn = true;
  }
}

void showTemperatureHumidity() {
  // DHT11 (Dhruba Saha) library: readTemperatureHumidity(temp, humi) returns 0 on success.
  int temperature = 0, humidity = 0;
  int result = dht11.readTemperatureHumidity(temperature, humidity);
  if (result == 0) {
    lastTemperature = (float)temperature;
    lastHumidity = (float)humidity;
    Serial.print("temperature:");
    Serial.print(temperature);
    Serial.print(" humidity:");
    Serial.println(humidity);
  } else {
    Serial.print("DHT Error: ");
    Serial.println(DHT11::getErrorString(result));
  }

  // No valid reading yet (still warming up on first seconds, or DATA not on pin 2).
  if (isnan(lastTemperature) || isnan(lastHumidity)) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("DHT warming up");
    return;
  }

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("T:");
  lcd.print(lastTemperature, 1);
  lcd.print("C H:");
  lcd.print(lastHumidity, 0);
  lcd.print("%");

  lcd.setCursor(0, 1);
  lcd.print(alarmOn ? "Alarm:ON" : "Alarm:OFF");
}

#if ENABLE_BLE
void setupBle() {
  if (!BLE.begin()) {
    Serial.println("BLE start failed");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("BLE failed");
    setRgb(true, false, false);
    return;
  }

  BLE.setLocalName("OpenClaw-R4");
  BLE.setDeviceName("OpenClaw-R4");
  BLE.setAdvertisedService(openClawService);

  openClawService.addCharacteristic(commandCharacteristic);
  openClawService.addCharacteristic(statusCharacteristic);
  BLE.addService(openClawService);

  commandCharacteristic.setValue("");
  statusCharacteristic.setValue(buildStatusMessage());

  BLE.advertise();
  Serial.println("BLE advertising: OpenClaw-R4");
}

void pollBle() {
  BLE.poll();

  if (commandCharacteristic.written()) {
    handleRemoteCommand(commandCharacteristic.value(), "BLE");
  }

  if (millis() - lastBlePollLog > 15000) {
    BLEDevice central = BLE.central();
    if (central) {
      Serial.print("BLE central connected: ");
      Serial.println(central.address());
    }
    lastBlePollLog = millis();
  }
}
#endif

#if ENABLE_HC05
void setupHc05() {
  Serial1.begin(HC05_BAUD);
  Serial.println("HC-05 Bluetooth UART ready on Serial1");
  Serial1.println("OpenClaw ready");
}

void sendHc05Status(const String &status) {
  Serial1.print("STATUS ");
  Serial1.println(status);
}

void processHc05Buffer() {
  hc05Buffer.trim();
  if (hc05Buffer.length() == 0) {
    return;
  }

  String command = hc05Buffer;
  hc05Buffer = "";
  handleRemoteCommand(command, "HC05");
}

void pollHc05() {
  while (Serial1.available() > 0) {
    char ch = (char)Serial1.read();
    lastHc05ByteAt = millis();

    if (ch == '\n' || ch == '\r' || ch == ';') {
      processHc05Buffer();
    } else if (hc05Buffer.length() < 40) {
      hc05Buffer += ch;
    } else {
      hc05Buffer = "";
      Serial.println("HC-05 command too long, cleared buffer");
    }
  }

  if (hc05Buffer.length() > 0 && millis() - lastHc05ByteAt > 200) {
    processHc05Buffer();
  }
}
#endif

#if ENABLE_WIFI || ENABLE_HA_HTTP || ENABLE_MQTT
bool hasWifiCredentials() {
  return strlen(WIFI_SSID) > 0 && strcmp(WIFI_SSID, "YOUR_WIFI_SSID") != 0;
}

void setupWifi() {
  if (!hasWifiCredentials()) {
    Serial.println("WiFi skipped: fill WIFI_SSID/WIFI_PASSWORD first");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WiFi config need");
    lcd.setCursor(0, 1);
    lcd.print("BT still ready");
    return;
  }

  Serial.print("Connecting WiFi: ");
  Serial.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  lastWifiAttempt = millis();
}

void pollWifi() {
  if (!hasWifiCredentials()) {
    return;
  }

  int status = WiFi.status();
  bool connected = status == WL_CONNECTED;

  if (!connected && millis() - lastWifiAttempt > 10000) {
    Serial.print("Reconnecting WiFi: ");
    Serial.println(WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    lastWifiAttempt = millis();
  }

  if (connected && !wifiWasConnected) {
    IPAddress ip = WiFi.localIP();
    Serial.print("WiFi connected, IP=");
    Serial.println(ip);

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WiFi connected");
    lcd.setCursor(0, 1);
    lcd.print(ip);
  } else if (!connected && wifiWasConnected) {
    Serial.println("WiFi disconnected");
  }

  if (connected && millis() - lastWifiStatusLog > 30000) {
    Serial.print("WiFi RSSI=");
    Serial.println(WiFi.RSSI());
    lastWifiStatusLog = millis();
  }

  wifiWasConnected = connected;
}
#endif

#if ENABLE_HA_HTTP
bool hasHaHttpConfig() {
  return strlen(HA_HOST) > 0
    && strcmp(HA_HOST, "192.168.45.10") != 0
    && strcmp(HA_HOST, "YOUR_HOME_ASSISTANT_IP") != 0
    && strlen(HA_TOKEN) > 0
    && strcmp(HA_TOKEN, "YOUR_HOME_ASSISTANT_LONG_LIVED_ACCESS_TOKEN") != 0;
}

String jsonEscape(String value) {
  value.replace("\\", "\\\\");
  value.replace("\"", "\\\"");
  value.replace("\n", "\\n");
  value.replace("\r", "");
  return value;
}

bool readHaHttpResponse(Client &client, int &statusCode, String &body) {
  unsigned long deadline = millis() + HA_HTTP_TIMEOUT_MS;

  while (!client.available() && millis() < deadline) {
    delay(5);
  }

  if (!client.available()) {
    Serial.println("HA HTTP timeout waiting for response");
    return false;
  }

  String statusLine = client.readStringUntil('\n');
  statusLine.trim();
  int firstSpace = statusLine.indexOf(' ');
  int secondSpace = statusLine.indexOf(' ', firstSpace + 1);
  if (firstSpace > 0 && secondSpace > firstSpace) {
    statusCode = statusLine.substring(firstSpace + 1, secondSpace).toInt();
  } else {
    statusCode = 0;
  }

  while (client.connected() && millis() < deadline) {
    String header = client.readStringUntil('\n');
    header.trim();
    if (header.length() == 0) {
      break;
    }
  }

  body = "";
  while (millis() < deadline) {
    while (client.available()) {
      body += (char)client.read();
      if (body.length() > 900) {
        body.remove(0, body.length() - 900);
      }
    }
    if (!client.connected()) {
      break;
    }
    delay(5);
  }

  return statusCode >= 200 && statusCode < 300;
}

bool sendHaHttpRequest(const char *method, const String &path, const String &payload, int &statusCode, String &body) {
  if (!hasHaHttpConfig()) {
    Serial.println("HA HTTP skipped: fill HA_HOST/HA_TOKEN first");
    return false;
  }

  if (WiFi.status() != WL_CONNECTED) {
    return false;
  }

#if HA_USE_SSL
  WiFiSSLClient client;
#else
  WiFiClient client;
#endif

  client.setTimeout(HA_HTTP_TIMEOUT_MS);
  if (!client.connect(HA_HOST, HA_PORT)) {
    Serial.println("HA HTTP connect failed");
    return false;
  }

  client.print(method);
  client.print(" ");
  client.print(path);
  client.println(" HTTP/1.1");
  client.print("Host: ");
  client.print(HA_HOST);
  client.print(":");
  client.println(HA_PORT);
  client.print("Authorization: Bearer ");
  client.println(HA_TOKEN);
  client.println("Content-Type: application/json");
  client.println("Connection: close");
  client.print("Content-Length: ");
  client.println(payload.length());
  client.println();
  client.print(payload);

  bool ok = readHaHttpResponse(client, statusCode, body);
  client.stop();

  if (!ok) {
    Serial.print("HA HTTP ");
    Serial.print(method);
    Serial.print(" ");
    Serial.print(path);
    Serial.print(" failed, status=");
    Serial.println(statusCode);
  }

  return ok;
}

String buildHaStatusPayload() {
  String payload = "{\"state\":\"online\",\"attributes\":{";
  payload += "\"alarm\":\"";
  payload += alarmOn ? "on" : "off";
  payload += "\",\"motion\":\"";
  payload += lastPirState == HIGH ? "on" : "off";
  payload += "\",\"bluetooth\":\"hc05\"";

  if (WiFi.status() == WL_CONNECTED) {
    payload += ",\"wifi_ip\":\"";
    payload += WiFi.localIP().toString();
    payload += "\",\"wifi_rssi\":";
    payload += WiFi.RSSI();
  }

  if (!isnan(lastTemperature) && !isnan(lastHumidity)) {
    payload += ",\"temperature\":";
    payload += String(lastTemperature, 1);
    payload += ",\"humidity\":";
    payload += String(lastHumidity, 0);
  }

  payload += ",\"friendly_name\":\"OpenClaw Status\"}}";
  return payload;
}

void postHaStatus() {
  int statusCode = 0;
  String body = "";
  String path = String("/api/states/") + HA_STATUS_ENTITY;
  if (sendHaHttpRequest("POST", path, buildHaStatusPayload(), statusCode, body)) {
    Serial.println("HA status posted");
  }
}

void postHaMotion(bool detected) {
  int statusCode = 0;
  String body = "";
  String path = String("/api/states/") + HA_MOTION_ENTITY;
  String payload = "{\"state\":\"";
  payload += detected ? "on" : "off";
  payload += "\",\"attributes\":{\"device_class\":\"motion\",\"friendly_name\":\"OpenClaw Motion\"}}";
  sendHaHttpRequest("POST", path, payload, statusCode, body);
}

String extractJsonStringState(const String &body) {
  int stateKey = body.indexOf("\"state\"");
  if (stateKey < 0) {
    return "";
  }

  int colon = body.indexOf(':', stateKey);
  int firstQuote = body.indexOf('"', colon + 1);
  int secondQuote = body.indexOf('"', firstQuote + 1);
  if (colon < 0 || firstQuote < 0 || secondQuote < 0) {
    return "";
  }

  return body.substring(firstQuote + 1, secondQuote);
}

String extractJsonStringValue(const String &body, const char *key, int startAt) {
  String quotedKey = "\"";
  quotedKey += key;
  quotedKey += "\"";

  int keyIndex = body.indexOf(quotedKey, startAt);
  if (keyIndex < 0) {
    return "";
  }

  int colon = body.indexOf(':', keyIndex + quotedKey.length());
  int firstQuote = body.indexOf('"', colon + 1);
  if (colon < 0 || firstQuote < 0) {
    return "";
  }

  String value = "";
  bool escaped = false;
  for (int i = firstQuote + 1; i < body.length(); i++) {
    char ch = body.charAt(i);
    if (escaped) {
      value += ch;
      escaped = false;
    } else if (ch == '\\') {
      escaped = true;
    } else if (ch == '"') {
      break;
    } else {
      value += ch;
    }
  }

  return value;
}

void clearHaCommandEntity() {
  int statusCode = 0;
  String body = "";
  String payload = "{\"entity_id\":\"";
  payload += HA_COMMAND_ENTITY;
  payload += "\",\"value\":\"\"}";
  sendHaHttpRequest("POST", "/api/services/input_text/set_value", payload, statusCode, body);
}

void setHaInputBoolean(const char *entityId, bool enabled) {
  int statusCode = 0;
  String body = "";
  String payload = "{\"entity_id\":\"";
  payload += entityId;
  payload += "\"}";
  sendHaHttpRequest(
    "POST",
    enabled ? "/api/services/input_boolean/turn_on" : "/api/services/input_boolean/turn_off",
    payload,
    statusCode,
    body
  );
}

#if ENABLE_HA_WS
void sendHaWsFrame(byte opcode, const String &payload) {
  if (!haWsClient.connected()) {
    return;
  }

  unsigned int length = payload.length();
  byte mask[4] = { 0x12, 0x34, 0x56, 0x78 };

  haWsClient.write((byte)(0x80 | opcode));
  if (length <= 125) {
    haWsClient.write((byte)(0x80 | length));
  } else {
    haWsClient.write((byte)(0x80 | 126));
    haWsClient.write((byte)((length >> 8) & 0xFF));
    haWsClient.write((byte)(length & 0xFF));
  }

  haWsClient.write(mask, 4);
  for (unsigned int i = 0; i < length; i++) {
    haWsClient.write((byte)(payload.charAt(i) ^ mask[i % 4]));
  }
}

void sendHaWsText(const String &message) {
  sendHaWsFrame(0x1, message);
}

void sendHaWsPing() {
  sendHaWsFrame(0x9, "");
}

void sendHaWsPong(const String &payload) {
  sendHaWsFrame(0xA, payload);
}

void sendHaWsAuth() {
  String auth = "{\"type\":\"auth\",\"access_token\":\"";
  auth += jsonEscape(String(HA_TOKEN));
  auth += "\"}";
  sendHaWsText(auth);
  Serial.println("HA WS auth sent");
}

void subscribeHaWsStateChanges() {
  String request = "{\"id\":";
  request += haWsNextId++;
  request += ",\"type\":\"subscribe_events\",\"event_type\":\"state_changed\"}";
  sendHaWsText(request);
  haWsSubscribed = true;
  Serial.println("HA WS subscribed to state_changed");
}

bool callHaWsInputBoolean(const char *entityId, bool enabled) {
  if (!haWsClient.connected() || !haWsSubscribed) {
    return false;
  }

  String request = "{\"id\":";
  request += haWsNextId++;
  request += ",\"type\":\"call_service\",\"domain\":\"input_boolean\",\"service\":\"";
  request += enabled ? "turn_on" : "turn_off";
  request += "\",\"service_data\":{\"entity_id\":\"";
  request += jsonEscape(String(entityId));
  request += "\"}}";

  sendHaWsText(request);
  Serial.print("HA WS call_service input_boolean.");
  Serial.println(enabled ? "turn_on" : "turn_off");
  return true;
}

void handleHaWsStateChanged(const String &message) {
  int entityIndex = message.indexOf("\"entity_id\"");
  if (entityIndex < 0) {
    return;
  }

  String entityId = extractJsonStringValue(message, "entity_id", entityIndex);
  int newStateIndex = message.indexOf("\"new_state\"", entityIndex);
  String state = extractJsonStringValue(message, "state", newStateIndex);
  state.trim();

  if (entityId == HA_ALARM_ENTITY) {
    if (state == "on" || state == "off") {
      bool requestedAlarm = state == "on";
      bool localAlarmSyncWindow = pendingHaAlarmSync
        || (millis() - lastLocalAlarmChangeAt < HA_LOCAL_ALARM_GRACE_MS);

      if (localAlarmSyncWindow && requestedAlarm != alarmOn) {
        Serial.println("HA WS alarm event ignored during local button sync");
        return;
      }

      lastHaAlarmState = state;
      if (requestedAlarm != alarmOn) {
        setAlarm(requestedAlarm, false, "HAWS");
      }
    }
  } else if (entityId == HA_PC_POWER_ENTITY) {
    if (state == "on") {
      Serial.println("HA WS PC power request");
      pressPcPowerByRemoteCommand("HAWS");
      pendingHaPcPowerReset = true;
    }
  } else if (entityId == HA_PC_POWER_BUTTON_ENTITY) {
    if (state.length() > 0 && state != "unknown" && state != "unavailable"
        && state != lastHaPcPowerButtonState) {
      lastHaPcPowerButtonState = state;
      Serial.println("HA WS PC power button request");
      pressPcPowerByRemoteCommand("HAWS-BUTTON");
    }
  } else if (entityId == HA_COMMAND_ENTITY) {
    if (state.length() > 0 && state != "unknown" && state != "unavailable") {
      if (handleRemoteCommand(state, "HAWS")) {
        clearHaCommandEntity();
      }
    }
  }
}

void handleHaWsMessage(const String &message) {
  if (message.indexOf("\"type\":\"auth_required\"") >= 0) {
    sendHaWsAuth();
  } else if (message.indexOf("\"type\":\"auth_ok\"") >= 0) {
    Serial.println("HA WS auth ok");
    subscribeHaWsStateChanges();
    pendingHaPcPowerReset = true;
  } else if (message.indexOf("\"type\":\"auth_invalid\"") >= 0) {
    Serial.println("HA WS auth invalid");
    haWsClient.stop();
  } else if (message.indexOf("\"type\":\"event\"") >= 0
      && message.indexOf("\"event_type\":\"state_changed\"") >= 0) {
    handleHaWsStateChanged(message);
  }
}

void connectHaWs() {
  if (!hasHaHttpConfig() || WiFi.status() != WL_CONNECTED) {
    return;
  }

  unsigned long now = millis();
  if (haWsClient.connected() || now - lastHaWsReconnect < HA_WS_RECONNECT_MS) {
    return;
  }

  Serial.println("Connecting HA WS");
  haWsClient.stop();
  haWsClient.setTimeout(HA_HTTP_TIMEOUT_MS);
  if (!haWsClient.connect(HA_HOST, HA_PORT)) {
    Serial.println("HA WS TCP connect failed");
    lastHaWsReconnect = now;
    return;
  }

  haWsClient.println("GET /api/websocket HTTP/1.1");
  haWsClient.print("Host: ");
  haWsClient.print(HA_HOST);
  haWsClient.print(":");
  haWsClient.println(HA_PORT);
  haWsClient.println("Upgrade: websocket");
  haWsClient.println("Connection: Upgrade");
  haWsClient.println("Sec-WebSocket-Key: b3BlbmNsYXctaW90LTAwMQ==");
  haWsClient.println("Sec-WebSocket-Version: 13");
  haWsClient.println();

  String statusLine = haWsClient.readStringUntil('\n');
  statusLine.trim();
  while (haWsClient.connected()) {
    String header = haWsClient.readStringUntil('\n');
    header.trim();
    if (header.length() == 0) {
      break;
    }
  }

  haWsSubscribed = false;
  lastHaWsReconnect = now;

  if (statusLine.indexOf("101") >= 0 && haWsClient.connected()) {
    Serial.println("HA WS connected");
  } else {
    Serial.print("HA WS handshake failed: ");
    Serial.println(statusLine);
    haWsClient.stop();
  }
}

bool readHaWsFrame(String &message) {
  if (!haWsClient.connected() || haWsClient.available() < 2) {
    return false;
  }

  int first = haWsClient.read();
  int second = haWsClient.read();
  if (first < 0 || second < 0) {
    return false;
  }

  byte opcode = first & 0x0F;
  bool masked = (second & 0x80) != 0;
  unsigned long length = second & 0x7F;

  if (length == 126) {
    while (haWsClient.available() < 2) {
      if (!haWsClient.connected()) return false;
      delay(1);
    }
    length = ((unsigned long)haWsClient.read() << 8) | haWsClient.read();
  } else if (length == 127) {
    length = 0;
    for (int i = 0; i < 8; i++) {
      while (!haWsClient.available()) {
        if (!haWsClient.connected()) return false;
        delay(1);
      }
      byte part = haWsClient.read();
      if (i >= 4) {
        length = (length << 8) | part;
      }
    }
  }

  byte mask[4] = {0, 0, 0, 0};
  if (masked) {
    for (int i = 0; i < 4; i++) {
      while (!haWsClient.available()) {
        if (!haWsClient.connected()) return false;
        delay(1);
      }
      mask[i] = haWsClient.read();
    }
  }

  message = "";
  unsigned long deadline = millis() + HA_WS_READ_TIMEOUT_MS;
  for (unsigned long i = 0; i < length; i++) {
    while (!haWsClient.available()) {
      if (!haWsClient.connected() || millis() > deadline) return false;
      delay(1);
    }

    char ch = (char)haWsClient.read();
    if (masked) {
      ch = (char)(ch ^ mask[i % 4]);
    }
    if (message.length() < 2500) {
      message += ch;
    }
  }

  if (opcode == 0x8) {
    Serial.println("HA WS closed by server");
    haWsClient.stop();
    return false;
  }
  if (opcode == 0x9) {
    sendHaWsPong(message);
    return false;
  }
  if (opcode != 0x1) {
    return false;
  }

  return true;
}

void pollHaWs() {
  if (!haWsClient.connected()) {
    connectHaWs();
    return;
  }

  for (byte i = 0; i < 4 && haWsClient.available() >= 2; i++) {
    String message = "";
    if (readHaWsFrame(message)) {
      handleHaWsMessage(message);
    } else {
      break;
    }
  }

  if (millis() - lastHaWsPing > HA_WS_PING_MS) {
    sendHaWsPing();
    lastHaWsPing = millis();
  }
}
#endif

void pollHaCommand() {
  int statusCode = 0;
  String body = "";
  String path = String("/api/states/") + HA_COMMAND_ENTITY;

  if (!sendHaHttpRequest("GET", path, "", statusCode, body)) {
    return;
  }

  String command = extractJsonStringState(body);
  command.trim();
  if (command.length() == 0 || command == "unknown" || command == "unavailable") {
    return;
  }

  if (command != lastHaCommand) {
    lastHaCommand = command;
    if (handleRemoteCommand(command, "HAHTTP")) {
      clearHaCommandEntity();
    }
  }
}

void pollHaAlarmToggle() {
  int statusCode = 0;
  String body = "";
  String path = String("/api/states/") + HA_ALARM_ENTITY;

  if (!sendHaHttpRequest("GET", path, "", statusCode, body)) {
    return;
  }

  String state = extractJsonStringState(body);
  state.trim();
  if (state != "on" && state != "off") {
    return;
  }

  if (lastHaAlarmState.length() == 0) {
    lastHaAlarmState = state;
    if ((state == "on") != alarmOn) {
      setAlarm(state == "on", false, "HA");
    }
    return;
  }

  if (state != lastHaAlarmState) {
    lastHaAlarmState = state;
    setAlarm(state == "on", false, "HA");
  }
}

void pollHaPcPowerToggle() {
  int statusCode = 0;
  String body = "";
  String path = String("/api/states/") + HA_PC_POWER_ENTITY;

  if (!sendHaHttpRequest("GET", path, "", statusCode, body)) {
    return;
  }

  String state = extractJsonStringState(body);
  state.trim();
  if (state != "on") {
    return;
  }

  pressPcPowerByRemoteCommand("HA");
  setHaInputBoolean(HA_PC_POWER_ENTITY, false);
}

void pollHaPcPowerButton() {
  int statusCode = 0;
  String body = "";
  String path = String("/api/states/") + HA_PC_POWER_BUTTON_ENTITY;

  if (!sendHaHttpRequest("GET", path, "", statusCode, body)) {
    return;
  }

  String state = extractJsonStringState(body);
  state.trim();
  if (state.length() == 0 || state == "unknown" || state == "unavailable") {
    return;
  }

  if (lastHaPcPowerButtonState.length() == 0) {
    lastHaPcPowerButtonState = state;
    return;
  }

  if (state != lastHaPcPowerButtonState) {
    lastHaPcPowerButtonState = state;
    pressPcPowerByRemoteCommand("HA-BUTTON");
  }
}

void pollHaHttp() {
  if (!hasHaHttpConfig() || WiFi.status() != WL_CONNECTED) {
    return;
  }

  unsigned long now = millis();

  if (pendingHaPcPowerReset) {
#if ENABLE_HA_WS
    if (callHaWsInputBoolean(HA_PC_POWER_ENTITY, false)) {
      pendingHaPcPowerReset = false;
      return;
    }
#endif
    setHaInputBoolean(HA_PC_POWER_ENTITY, false);
    pendingHaPcPowerReset = false;
    return;
  }

  if (pendingHaAlarmSync) {
#if ENABLE_HA_WS
    if (callHaWsInputBoolean(HA_ALARM_ENTITY, alarmOn)) {
      lastHaAlarmState = alarmOn ? "on" : "off";
      pendingHaAlarmSync = false;
      return;
    }
#endif
    setHaInputBoolean(HA_ALARM_ENTITY, alarmOn);
    lastHaAlarmState = alarmOn ? "on" : "off";
    pendingHaAlarmSync = false;
    return;
  }

  if (pendingHaMotionOn) {
    postHaMotion(true);
    pendingHaMotionOn = false;
    return;
  }

  if (pendingHaStatusPost) {
    postHaStatus();
    pendingHaStatusPost = false;
    lastHaStatusPost = now;
    return;
  }

#if !ENABLE_HA_WS
  if (now - lastHaFastPoll >= HA_FAST_POLL_MS) {
    if (haFastPollStep % 2 == 0) {
      pollHaAlarmToggle();
    } else if (haFastPollStep % 4 == 1) {
      pollHaPcPowerToggle();
    } else {
      pollHaPcPowerButton();
    }
    haFastPollStep++;
    lastHaFastPoll = now;
    return;
  }

  if (now - lastHaCommandPoll >= HA_COMMAND_POLL_MS) {
    pollHaCommand();
    lastHaCommandPoll = now;
    return;
  }
#endif

  if (now - lastHaStatusPost >= HA_STATUS_POST_MS) {
    postHaStatus();
    lastHaStatusPost = now;
  }
}
#endif

#if ENABLE_MQTT
void publishMqttMessage(const char *topic, const String &payload) {
  if (!mqttClient.connected()) {
    return;
  }

  mqttClient.beginMessage(topic);
  mqttClient.print(payload);
  mqttClient.endMessage();
}

void onMqttMessage(int messageSize) {
  String command = "";
  while (mqttClient.available()) {
    command += (char)mqttClient.read();
  }

  if (messageSize > 0) {
    handleRemoteCommand(command, "MQTT");
  }
}

void setupMqtt() {
  mqttClient.setId(MQTT_CLIENT_ID);
  if (strlen(MQTT_USER) > 0 && strcmp(MQTT_USER, "YOUR_MQTT_USER") != 0) {
    mqttClient.setUsernamePassword(MQTT_USER, MQTT_PASSWORD);
  }
  mqttClient.onMessage(onMqttMessage);
}

void pollMqtt() {
  if (!hasWifiCredentials() || WiFi.status() != WL_CONNECTED) {
    return;
  }

  if (!mqttClient.connected()) {
    if (millis() - lastMqttAttempt > 5000) {
      Serial.print("Connecting MQTT: ");
      Serial.println(MQTT_BROKER);
      if (mqttClient.connect(MQTT_BROKER, MQTT_PORT)) {
        mqttClient.subscribe(MQTT_COMMAND_TOPIC);
        publishMqttMessage(MQTT_STATUS_TOPIC, buildStatusMessage());
        Serial.println("MQTT connected");
      } else {
        Serial.print("MQTT failed, error=");
        Serial.println(mqttClient.connectError());
      }
      lastMqttAttempt = millis();
    }
    return;
  }

  mqttClient.poll();
}
#endif

void updateRemoteStatus() {
  String status = buildStatusMessage();

#if ENABLE_BLE
  statusCharacteristic.setValue(status);
#endif

#if ENABLE_HC05
  sendHc05Status(status);
#endif

#if ENABLE_MQTT
  publishMqttMessage(MQTT_STATUS_TOPIC, status);
#endif
}

void setup() {
  Serial.begin(9600); // open Serial Monitor at 9600 baud to see DHT readings / error codes

  pinMode(RGB_R, OUTPUT);
  pinMode(RGB_G, OUTPUT);
  pinMode(RGB_B, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  pinMode(ALARM_BUTTON, INPUT_PULLUP);
  pinMode(PIR_PIN, INPUT);
  lastAlarmButtonState = digitalRead(ALARM_BUTTON);

  int alarmInterrupt = digitalPinToInterrupt(ALARM_BUTTON);
  if (alarmInterrupt >= 0) {
    attachInterrupt(alarmInterrupt, onAlarmButtonInterrupt, FALLING);
  } else {
    Serial.println("ALARM button interrupt unavailable; using fast polling");
  }

  pcPowerServo.attach(SERVO_PC_PIN);
  pcPowerServo.write(0);

  lcd.init();
  lcd.backlight();
  // DHT11 library needs no begin().
  // CDS (photoresistor) on A0 in a voltage divider controls LCD backlight for eco mode.

  setRgb(false, true, false);
  lcd.setCursor(0, 0);
  lcd.print("OpenClaw Hand");
  lcd.setCursor(0, 1);
  lcd.print("Ready");
  beep(1);

#if ENABLE_BLE
  setupBle();
#endif

#if ENABLE_HC05
  setupHc05();
#endif

#if ENABLE_WIFI || ENABLE_HA_HTTP || ENABLE_MQTT
  setupWifi();
#endif

#if ENABLE_MQTT
  setupMqtt();
#endif
}

void loop() {
  pollPcPowerServo();
  pollBeeper();
  pollAlarmButton();

  if (isPcPowerServoBusy()) {
    return;
  }

  updateLcdBacklightByCdsSensor();

  int pirState = digitalRead(PIR_PIN);
  if (pirState == HIGH && lastPirState == LOW) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Motion detected");
    lcd.setCursor(0, 1);
    lcd.print(alarmOn ? "Alarm triggered" : "BT/WiFi alert");
    setRgb(false, false, true);
    if (alarmOn) beep(5); else beep(1);

#if ENABLE_MQTT
    publishMqttMessage(MQTT_MOTION_TOPIC, "ON");
#endif

#if ENABLE_HA_HTTP
    pendingHaMotionOn = true;
#endif
  }
  lastPirState = pirState;

#if ENABLE_BLE
  pollBle();
#endif

#if ENABLE_HC05
  pollHc05();
#endif

#if ENABLE_WIFI || ENABLE_HA_HTTP || ENABLE_MQTT
  pollWifi();
#endif

#if ENABLE_HA_WS
  pollHaWs();
#endif

  pollPcPowerServo();
  pollBeeper();

  if (isPcPowerServoBusy()) {
    return;
  }

#if ENABLE_HA_HTTP
  pollHaHttp();
#endif

#if ENABLE_MQTT
  pollMqtt();
#endif

  if (millis() - lastSensorRead > 3000) {
    showTemperatureHumidity();
    lastSensorRead = millis();
  }

  if (millis() - lastStatusPublish > 10000) {
    updateRemoteStatus();
    lastStatusPublish = millis();
  }
}
