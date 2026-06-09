---
name: openclaw-ha-control
description: Use when controlling or monitoring the OpenClaw Arduino IoT device through Home Assistant at ha.yeoun.org, including PC power servo control, alarm state, motion, temperature, humidity, WiFi status, REST service calls, and WebSocket state observation.
---

# OpenClaw Home Assistant Control

Use this skill when the user asks to control or inspect the OpenClaw Arduino device through Home Assistant.

## Connection

- Home Assistant base URL: `https://ha.yeoun.org`
- REST API base: `https://ha.yeoun.org/api`
- WebSocket API: `wss://ha.yeoun.org/api/websocket`
- Authentication: use a Home Assistant long-lived access token from the agent secret store or environment, preferably `HA_TOKEN` or `HOME_ASSISTANT_TOKEN`.
- Never write, print, commit, or log the token.

REST requests require:

```http
Authorization: Bearer <HA_TOKEN>
Content-Type: application/json
```

## Entities

Read-only status from Arduino:

- `sensor.openclaw_status`: main status entity. State is usually `online`; attributes include `light`, `alarm`, `motion`, `bluetooth`, `wifi_ip`, `wifi_rssi`, `temperature`, and `humidity`.
- `binary_sensor.openclaw_motion`: PIR motion state.

Controls from Home Assistant to Arduino:

- `input_boolean.openclaw_alarm`: alarm enable/disable toggle.
- `input_boolean.openclaw_pc_power`: momentary PC power request. Turn it `on`; Arduino presses the servo once and resets it to `off`.
- `input_text.openclaw_command`: fallback/debug text command.

Supported text commands:

- `PC_POWER`
- `ALARM_TOGGLE`
- `ALARM_ON`
- `ALARM_OFF`
- `STATUS`

## Natural Language Mapping

- "PC 켜", "PC 꺼", "컴퓨터 전원 눌러", "전원 버튼 눌러" -> turn on `input_boolean.openclaw_pc_power`.
- "알람 켜", "보안 켜" -> turn on `input_boolean.openclaw_alarm`.
- "알람 꺼", "보안 꺼" -> turn off `input_boolean.openclaw_alarm`.
- "상태 확인", "오픈클로 상태", "온습도", "온도", "습도", "움직임 감지됐어?" -> read `sensor.openclaw_status` and, if motion matters, `binary_sensor.openclaw_motion`.

If a PC power request is ambiguous, confirm once before pressing. This command physically actuates a servo connected to a power button.

## REST Workflow

Read main status:

```http
GET https://ha.yeoun.org/api/states/sensor.openclaw_status
```

Read temperature and humidity:

```http
GET https://ha.yeoun.org/api/states/sensor.openclaw_status
```

Use these response fields:

```json
{
  "state": "online",
  "attributes": {
    "temperature": 28,
    "humidity": 47
  }
}
```

Read motion:

```http
GET https://ha.yeoun.org/api/states/binary_sensor.openclaw_motion
```

Press PC power once:

```http
POST https://ha.yeoun.org/api/services/input_boolean/turn_on

{"entity_id":"input_boolean.openclaw_pc_power"}
```

Turn alarm on:

```http
POST https://ha.yeoun.org/api/services/input_boolean/turn_on

{"entity_id":"input_boolean.openclaw_alarm"}
```

Turn alarm off:

```http
POST https://ha.yeoun.org/api/services/input_boolean/turn_off

{"entity_id":"input_boolean.openclaw_alarm"}
```

Fallback command path:

```http
POST https://ha.yeoun.org/api/services/input_text/set_value

{"entity_id":"input_text.openclaw_command","value":"PC_POWER"}
```

## WebSocket Workflow

Use WebSocket for live state observation when the agent needs immediate updates.

1. Connect to `wss://ha.yeoun.org/api/websocket`.
2. Wait for `{"type":"auth_required"}`.
3. Send `{"type":"auth","access_token":"<HA_TOKEN>"}`.
4. Wait for `{"type":"auth_ok"}`.
5. Subscribe to state changes:

```json
{"id":1,"type":"subscribe_events","event_type":"state_changed"}
```

Watch for these `entity_id` values in event data:

- `sensor.openclaw_status`
- `binary_sensor.openclaw_motion`
- `input_boolean.openclaw_alarm`
- `input_boolean.openclaw_pc_power`
- `input_text.openclaw_command`

For commands, REST service calls are simplest. If the agent already has an authenticated WebSocket open, it may send Home Assistant `call_service` messages directly.

Press PC power over WebSocket:

```json
{"id":2,"type":"call_service","domain":"input_boolean","service":"turn_on","service_data":{"entity_id":"input_boolean.openclaw_pc_power"}}
```

Turn alarm off over WebSocket:

```json
{"id":3,"type":"call_service","domain":"input_boolean","service":"turn_off","service_data":{"entity_id":"input_boolean.openclaw_alarm"}}
```

Use WebSocket to confirm the state changed or to stream updates.

When receiving `sensor.openclaw_status` events, temperature and humidity are in `event.data.new_state.attributes.temperature` and `event.data.new_state.attributes.humidity`.

## Response Style

- Answer the user in Korean unless they ask otherwise.
- For control commands, state exactly what was sent and the resulting HA state if available.
- For status, summarize only useful fields: online/offline, temperature, humidity, alarm, motion, WiFi IP, RSSI.
- For temperature/humidity-only requests, answer briefly like `현재 온도 28도, 습도 47%입니다.`
- If HA returns `401` or `403`, say the token/auth path is wrong. If it returns `404`, say the entity ID or helper is missing/mismatched.

## Safety Notes

- Do not expose the HA token.
- Do not assume `input_boolean.openclaw_pc_power` means "turn PC on"; it presses the physical power button once, so it can also shut the PC down.
- Do not use MQTT for the current OpenClaw control path unless the project code is changed back to MQTT. This build uses HA REST plus WebSocket.
- If Cloudflare Access is added later, API calls will also need Access credentials or a bypass rule for trusted clients.
