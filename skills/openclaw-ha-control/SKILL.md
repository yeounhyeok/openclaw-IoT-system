---
name: openclaw-ha-control
description: Use when controlling or monitoring the OpenClaw Arduino IoT device through Home Assistant at ha.yeoun.org, including PC power servo control, alarm state, motion, temperature, humidity, WiFi status, REST service calls, and WebSocket state observation.
version: 1.0.0
author: Yeounhyeok / Hermes Agent
license: MIT
platforms: [linux, macos, windows]
metadata:
  hermes:
    tags: [home-assistant, arduino, iot, openclaw, smart-home, websocket, rest]
    related_skills: [pc-power-control, yeoun-cloudflare-tunnel-npm, openhue]
---

# OpenClaw Home Assistant Control

Use this skill when the user asks to control or inspect the OpenClaw Arduino device through Home Assistant.

## Connection

- Home Assistant base URL: `https://ha.yeoun.org`
- REST API base: `https://ha.yeoun.org/api`
- WebSocket API: `wss://ha.yeoun.org/api/websocket`
- Authentication: use a Home Assistant long-lived access token from the agent secret store or environment, preferably `HA_TOKEN` or `HOME_ASSISTANT_TOKEN`. In this Hermes profile, `HOME_ASSISTANT_TOKEN` may be set in `~/.hermes/.env`.
- Never write, print, commit, or log the token.
- Arduino command handling is WebSocket-first. REST `/api/states` is mainly used by Arduino for slow status/temperature/humidity uploads.

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
- `input_boolean.openclaw_pc_power`: momentary PC power request if the toggle helper exists. Turn it `on`; Arduino presses the servo once and resets it to `off`.
- `input_button.openclaw_pc_power`: current working PC power helper. Pressing it changes the timestamp and triggers the servo once; prefer this path when available.
- `input_text.openclaw_command`: fallback/debug text command.

Supported text commands:

- `PC_POWER`
- `ALARM_TOGGLE`
- `ALARM_ON`
- `ALARM_OFF`
- `STATUS`

## Natural Language Mapping

- "PC 켜", "PC 꺼", "컴퓨터 전원 눌러", "전원 버튼 눌러" -> press `input_button.openclaw_pc_power` via `openclaw-pc-power` (current working path).
- "알람 켜", "보안 켜" -> send `ALARM_ON` through `input_text.openclaw_command` and sync `input_boolean.openclaw_alarm`.
- "알람 꺼", "보안 꺼" -> send `ALARM_OFF` through `input_text.openclaw_command` and sync `input_boolean.openclaw_alarm`.
- "상태 확인", "오픈클로 상태", "온습도", "온도", "습도", "움직임 감지됐어?" -> read `sensor.openclaw_status` and, if motion matters, `binary_sensor.openclaw_motion`.

If a PC power request is ambiguous, confirm once before pressing. This command physically actuates a servo connected to a power button.

## Agent Quick Commands

Use the installed one-command wrappers first for fast responses. They load `~/.hermes/.env` themselves and never print the token.

| Intent | Command |
|---|---|
| Full status | `openclaw-status` or `openclaw status` |
| Temperature/humidity | `openclaw-temp` or `openclaw temp` |
| Motion | `openclaw-motion` or `openclaw motion` |
| Alarm on | `openclaw-alarm-on` or `openclaw alarm-on` |
| Alarm off | `openclaw-alarm-off` or `openclaw alarm-off` |
| PC power servo press | `openclaw-pc-power` or `openclaw pc-power` |
| Compatibility button press | `openclaw-pc-power-button` or `openclaw pc-power-button` |
| Fallback text command | `openclaw command PC_POWER` / `ALARM_TOGGLE` / `ALARM_ON` / `ALARM_OFF` / `STATUS` |

Implementation lives at `skills/openclaw-ha-control/scripts/openclaw`. Optional local wrappers may point to this script. The script sets `User-Agent: curl/8.5.0` because Cloudflare may block Python urllib's default User-Agent with `403 error code: 1010`.


PC power commands physically press the power button once. If user intent is ambiguous, confirm once before running `openclaw-pc-power`.

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

Press PC power once using the preferred toggle helper:

```http
POST https://ha.yeoun.org/api/services/input_boolean/turn_on

{"entity_id":"input_boolean.openclaw_pc_power"}
```

Press PC power once using the compatibility button helper:

```http
POST https://ha.yeoun.org/api/services/input_button/press

{"entity_id":"input_button.openclaw_pc_power"}
```

Turn alarm on/off. This firmware path is most reliable through `input_text.openclaw_command`; also sync the helper state for dashboard/UI consistency:

```http
POST https://ha.yeoun.org/api/services/input_text/set_value

{"entity_id":"input_text.openclaw_command","value":"ALARM_ON"}
```

```http
POST https://ha.yeoun.org/api/services/input_text/set_value

{"entity_id":"input_text.openclaw_command","value":"ALARM_OFF"}
```

Optional helper sync:

```http
POST https://ha.yeoun.org/api/services/input_boolean/turn_on

{"entity_id":"input_boolean.openclaw_alarm"}
```

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
- `input_button.openclaw_pc_power`
- `input_text.openclaw_command`

For commands, REST service calls are simplest. If the agent already has an authenticated WebSocket open, it may send Home Assistant `call_service` messages directly.

Press PC power over WebSocket:

```json
{"id":2,"type":"call_service","domain":"input_boolean","service":"turn_on","service_data":{"entity_id":"input_boolean.openclaw_pc_power"}}
```

Press PC power over WebSocket using the compatibility button helper:

```json
{"id":3,"type":"call_service","domain":"input_button","service":"press","service_data":{"entity_id":"input_button.openclaw_pc_power"}}
```

Turn alarm off over WebSocket:

```json
{"id":4,"type":"call_service","domain":"input_boolean","service":"turn_off","service_data":{"entity_id":"input_boolean.openclaw_alarm"}}
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
