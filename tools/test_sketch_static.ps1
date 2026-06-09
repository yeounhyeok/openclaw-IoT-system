param(
  [string]$SketchPath = "sketch/sketch.ino"
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path -LiteralPath $SketchPath)) {
  throw "Sketch file not found: $SketchPath"
}

$sketch = Get-Content -Raw -LiteralPath $SketchPath

function Assert-Contains {
  param(
    [string]$Text,
    [string]$Pattern,
    [string]$Message
  )

  if ($Text -notmatch [regex]::Escape($Pattern)) {
    throw $Message
  }
}

$requiredSnippets = @(
  @{ Pattern = "#define ENABLE_BLE 0"; Message = "Built-in BLE should be disabled by default for simultaneous WiFi + HC-05 mode." },
  @{ Pattern = "#define ENABLE_HC05 1"; Message = "HC-05 Bluetooth UART should be enabled by default for simultaneous WiFi + Bluetooth control." },
  @{ Pattern = "#define ENABLE_WIFI 1"; Message = "WiFi should be enabled by default for simultaneous WiFi + HC-05 mode." },
  @{ Pattern = "#define ENABLE_HA_HTTP 1"; Message = "Home Assistant HTTP should be enabled by default." },
  @{ Pattern = "#define ENABLE_HA_WS 1"; Message = "Home Assistant WebSocket command path should be enabled by default." },
  @{ Pattern = "#define HA_USE_SSL 1"; Message = "Home Assistant HTTPS should be enabled by default." },
  @{ Pattern = "#define ENABLE_MQTT 0"; Message = "MQTT should stay opt-in until WiFi/broker credentials are filled." },
  @{ Pattern = "#error ""UNO R4 WiFi cannot use built-in BLE and WiFi at the same time."; Message = "Compile-time guard for built-in BLE/WiFi mutual exclusion is missing." },
  @{ Pattern = "#error ""ENABLE_HA_HTTP requires ENABLE_WIFI 1."; Message = "Compile-time guard for HA HTTP requiring WiFi is missing." },
  @{ Pattern = "#error ""ENABLE_HA_WS currently requires ENABLE_HA_HTTP 1"; Message = "Compile-time guard for HA WS requiring HA HTTP is missing." },
  @{ Pattern = "#error ""ENABLE_MQTT requires ENABLE_WIFI 1."; Message = "Compile-time guard for MQTT requiring WiFi is missing." },
  @{ Pattern = "#include <ArduinoBLE.h>"; Message = "ArduinoBLE include is missing." },
  @{ Pattern = "#include <WiFiS3.h>"; Message = "WiFiS3 include is missing." },
  @{ Pattern = "#include <ArduinoMqttClient.h>"; Message = "ArduinoMqttClient include is missing." },
  @{ Pattern = "OpenClaw-R4"; Message = "BLE device name is missing." },
  @{ Pattern = "19b10000-e8f2-537e-4f6c-d104768a1214"; Message = "BLE service UUID is missing." },
  @{ Pattern = "19b10001-e8f2-537e-4f6c-d104768a1214"; Message = "BLE command characteristic UUID is missing." },
  @{ Pattern = "19b10002-e8f2-537e-4f6c-d104768a1214"; Message = "BLE status characteristic UUID is missing." },
  @{ Pattern = "openclaw/cmd"; Message = "MQTT command topic is missing." },
  @{ Pattern = "openclaw/status"; Message = "MQTT status topic is missing." },
  @{ Pattern = "openclaw/sensor/motion"; Message = "MQTT motion topic is missing." },
  @{ Pattern = "#define HC05_BAUD 9600"; Message = "HC-05 UART baud setting is missing." },
  @{ Pattern = "#define ALARM_BUTTON 3"; Message = "Alarm button should use Uno R4 interrupt-capable pin D3." },
  @{ Pattern = "#define RGB_R 12"; Message = "RGB red should move off D3 so D3 can be used for alarm interrupt." },
  @{ Pattern = "void setupHc05()"; Message = "HC-05 setup function is missing." },
  @{ Pattern = "void pollHc05()"; Message = "HC-05 polling function is missing." },
  @{ Pattern = "Serial1.begin(HC05_BAUD);"; Message = "HC-05 should use Serial1." },
  @{ Pattern = "handleRemoteCommand(command, ""HC05"");"; Message = "HC-05 should use the shared command handler." },
  @{ Pattern = "void setupWifi()"; Message = "WiFi setup function is missing." },
  @{ Pattern = "void pollWifi()"; Message = "WiFi polling function is missing." },
  @{ Pattern = "#include ""arduino_secrets.h"""; Message = "Secrets header include is missing." },
  @{ Pattern = "#define SECRET_HA_HOST ""ha.yeoun.org"""; Message = "Default Home Assistant public host is missing." },
  @{ Pattern = "#define SECRET_HA_PORT 443"; Message = "Default Home Assistant HTTPS port is missing." },
  @{ Pattern = "char HA_TOKEN[] = SECRET_HA_TOKEN;"; Message = "Home Assistant token should come from secrets." },
  @{ Pattern = "sensor.openclaw_status"; Message = "Home Assistant status entity is missing." },
  @{ Pattern = "binary_sensor.openclaw_motion"; Message = "Home Assistant motion entity is missing." },
  @{ Pattern = "input_text.openclaw_command"; Message = "Home Assistant command helper entity is missing." },
  @{ Pattern = "input_boolean.openclaw_alarm"; Message = "Home Assistant alarm toggle helper entity is missing." },
  @{ Pattern = "input_boolean.openclaw_pc_power"; Message = "Home Assistant PC power toggle helper entity is missing." },
  @{ Pattern = "void postHaStatus()"; Message = "Home Assistant status POST function is missing." },
  @{ Pattern = "void pollHaCommand()"; Message = "Home Assistant command polling function is missing." },
  @{ Pattern = "void pollHaAlarmToggle()"; Message = "Home Assistant alarm toggle polling function is missing." },
  @{ Pattern = "void pollHaPcPowerToggle()"; Message = "Home Assistant PC power toggle polling function is missing." },
  @{ Pattern = "setHaInputBoolean(HA_PC_POWER_ENTITY, false);"; Message = "PC power toggle should reset itself after triggering servo." },
  @{ Pattern = "Authorization: Bearer "; Message = "Home Assistant Bearer token header is missing." },
  @{ Pattern = "WiFiSSLClient client;"; Message = "HTTPS mode should use WiFiSSLClient." },
  @{ Pattern = "WiFiSSLClient haWsClient;"; Message = "Home Assistant WSS client is missing." },
  @{ Pattern = "void pollHaWs()"; Message = "Home Assistant WebSocket polling function is missing." },
  @{ Pattern = "void handleHaWsStateChanged"; Message = "Home Assistant WebSocket state_changed handler is missing." },
  @{ Pattern = "void sendHaWsFrame"; Message = "Manual WebSocket frame sender is missing." },
  @{ Pattern = "bool readHaWsFrame"; Message = "Manual WebSocket frame reader is missing." },
  @{ Pattern = "subscribe_events"; Message = "Home Assistant WebSocket should subscribe to state_changed events." },
  @{ Pattern = "call_service"; Message = "Home Assistant WebSocket should support bidirectional service calls." },
  @{ Pattern = "bool callHaWsInputBoolean"; Message = "Home Assistant WebSocket input_boolean service helper is missing." },
  @{ Pattern = "Sec-WebSocket-Key"; Message = "Home Assistant WebSocket handshake header is missing." },
  @{ Pattern = "/api/websocket"; Message = "Home Assistant WebSocket endpoint is missing." },
  @{ Pattern = "#define HA_HTTP_TIMEOUT_MS 1200"; Message = "HA HTTP timeout should be bounded for responsive local controls." },
  @{ Pattern = "#define HA_FAST_POLL_MS 500"; Message = "HA fast polling interval should be 500 ms." },
  @{ Pattern = "pendingHaAlarmSync = true;"; Message = "Local alarm button should queue HA alarm sync instead of blocking on HTTP." },
  @{ Pattern = "HA WS alarm event ignored during local button sync"; Message = "Local alarm button should ignore stale HA WS alarm events during sync." },
  @{ Pattern = "pendingHaPcPowerReset = true;"; Message = "PC power helper reset should be queued after WS trigger." },
  @{ Pattern = "void pollAlarmButton()"; Message = "Alarm button should use edge polling to catch short presses." },
  @{ Pattern = "volatile byte alarmButtonInterruptCount = 0;"; Message = "Alarm button interrupt should be latched as a count for short presses." },
  @{ Pattern = "bool interruptEdge = interruptCount > 0;"; Message = "Alarm button interrupt event should not require the button to still be held." },
  @{ Pattern = "if (alarmInterrupt >= 0)"; Message = "Interrupt availability check should be compatible with Uno R4 core." },
  @{ Pattern = "attachInterrupt(alarmInterrupt, onAlarmButtonInterrupt, FALLING);"; Message = "Alarm button interrupt hook is missing." },
  @{ Pattern = "lastAlarmButtonEvent > 250"; Message = "Alarm button debounce guard is missing." },
  @{ Pattern = "pendingHaMotionOn = true;"; Message = "PIR should queue HA motion update instead of blocking on HTTP." },
  @{ Pattern = "pendingHaStatusPost = true;"; Message = "Status updates should be queued instead of posted synchronously." },
  @{ Pattern = "bool handleRemoteCommand(String command, const char *source)"; Message = "Shared remote command handler is missing." }
)

foreach ($item in $requiredSnippets) {
  Assert-Contains -Text $sketch -Pattern $item.Pattern -Message $item.Message
}

$requiredCommands = @(
  "PC_POWER",
  "ALARM_TOGGLE",
  "ALARM_ON",
  "ALARM_OFF",
  "STATUS"
)

foreach ($command in $requiredCommands) {
  Assert-Contains -Text $sketch -Pattern $command -Message "Remote command is missing: $command"
}

Write-Host "Static sketch tests passed for $SketchPath"
