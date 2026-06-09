# OpenClaw Physical Hand - Arduino 회로 프로젝트

제안서의 `OpenClaw Physical Hand: Arduino 기반 원격 버튼 제어 봇`을 실제로 구현하기 위한 Arduino 회로/펌웨어 프로젝트입니다.

> 이 프로젝트는 **실제 하드웨어 직접 제작**을 기준으로 정리되어 있습니다.
> `diagram.json`은 Wokwi에서 동작을 미리 확인하고 싶을 때 쓰는 보조 시뮬레이션 파일이며,
> CDS 부분만 Wokwi에 단순 2핀 CDS 부품이 없어 모듈형 광센서로 대체되어 있습니다.
> 실제 제작에서는 아래 설명대로 **다리 2개짜리 CDS 조도센서 + 분압저항** 구성을 사용합니다.

## 프로젝트 개요

- 메인 보드: Arduino Uno R4 WiFi + HC-05 Bluetooth 모듈
- 입력 디바이스 4종 / 출력 디바이스 4종으로 과제 조건(입력 3종↑ · 출력 3종↑) 충족
- 핵심 기능
  - 버튼으로 알람 토글
  - PIR로 사람 움직임 감지 → 알림/알람
  - DHT22로 온습도 표시
  - **CDS 조도센서로 LCD 백라이트 자동 절전(주변이 어두워지면 백라이트 OFF)**
  - 서보모터로 PC 전원 버튼 누르기

## 포함 파일

- `sketch/sketch.ino` : Arduino 펌웨어 (메인 산출물)
- `README.md` : 회로 설명 / 핀맵 / 배선 / 동작 설명
- `docs/circuit_schematic.png` : 전체 연결 schematic 이미지 (보고서/HWP 첨부용)
- `docs/cds_divider.png` : CDS 조도센서 분압 회로 상세도
- `docs/*.svg` : 위 이미지의 벡터 버전 (확대해도 안 깨짐)
- `docs/OpenClaw.fzz` : **Fritzing 프로젝트 파일** (Fritzing에서 바로 열기)
- `docs/OpenClaw.fz` : 위 fzz 안의 스케치 XML (수정/검수용)
- `tools/make_schematic.py` : schematic 이미지 생성 스크립트
- `tools/make_fritzing.py` : Fritzing `.fzz` 생성 스크립트
- `diagram.json` : (선택) Wokwi 미리보기용 회로 배치 파일

## 회로도 (이미지)

보고서/발표 자료에 바로 붙일 수 있는 회로 이미지가 `docs/`에 있습니다.

- `docs/circuit_schematic.png` : Arduino Uno 중심 전체 연결도 (입력 4종 / 출력 4종이 한눈에)
- `docs/cds_divider.png` : 다리 2개짜리 CDS + 10kΩ 분압 회로 상세도

> 이미지는 Python `schemdraw`로 생성했습니다. 수정이 필요하면 아래처럼 재생성하면 됩니다.
> ```
> pip install schemdraw matplotlib
> python tools/make_schematic.py
> ```

## Fritzing 프로젝트 (docs/OpenClaw.fzz)

Fritzing에서 바로 열 수 있는 회로 파일도 포함되어 있습니다.

- 열기: Fritzing 실행 → File → Open → `docs/OpenClaw.fzz`
- 포함 부품(모두 Fritzing 기본 내장 core 부품): Arduino Uno, CDS(photoresistor) + 10k 분압, RGB용 LED 3개 + 220Ω 3개, 푸시버튼 2개, 피에조 부저, 서보 2개
- 배선은 핀맵표와 동일하게 연결되어 있습니다(점대점 배선). 열고 나서 부품을 드래그해 보기 좋게 정렬하면 됩니다.
- **참고**: DHT22 / PIR / I2C 1602 LCD 는 Fritzing 기본 core 부품에 없어서(별도 라이브러리 필요) 이 fzz에는 빠져 있습니다. 세 부품을 포함한 전체 구성은 `docs/circuit_schematic.png`(전체 연결도)와 `diagram.json`(Wokwi)에서 확인할 수 있습니다.
- 생성/수정: `python tools/make_fritzing.py` (core 부품의 moduleId·connectorId를 코드에서 직접 지정)

> 이 fzz는 Fritzing core 부품의 공식 moduleId/connectorId를 사용해 생성했고 XML 유효성은 검증했지만,
> 생성 환경에 Fritzing이 없어 화면 렌더링까지는 확인하지 못했습니다.
> 열었을 때 부품이 겹쳐 보이면 드래그로 정렬하면 되고, 혹시 누락/오류가 보이면 알려주세요.

## 입력 디바이스 4종

| # | 디바이스 | 연결 핀 | 역할 |
|---|----------|---------|------|
| 1 | DHT22 온습도 센서 | D2 | 온도/습도 환경값 입력 |
| 2 | Push Button | D3(ALARM) | 알람 토글 |
| 3 | PIR 인체감지 센서 | D11 | 사람 움직임 감지 입력 |
| 4 | **CDS 조도센서 (다리 2개)** | A0 | 주변 밝기 입력 → LCD 백라이트 자동 절전 |

## 출력 디바이스 4종

| # | 디바이스 | 연결 핀 | 역할 |
|---|----------|---------|------|
| 1 | Servo Motor | D9(PC 전원) | PC 전원 버튼 누르기 |
| 2 | 16×2 I2C LCD | A4(SDA), A5(SCL) | 온습도·알람·감지 상태 표시 |
| 3 | RGB LED | D12(R), D5(G), D6(B) | 정상/동작/이벤트/오류/알람 상태 표시 |
| 4 | Piezo Buzzer | D8 | 알림음 / 알람음 출력 |

## 전체 핀맵 (Arduino Uno 기준)

| 핀 | 연결 |
|----|------|
| D2 | DHT22 DATA |
| D3 | ALARM 버튼 (INPUT_PULLUP, 반대편 GND) |
| D5 | RGB LED G (220Ω 경유) |
| D6 | RGB LED B (220Ω 경유) |
| D8 | Buzzer (+) |
| D9 | PC 전원 서보 신호 |
| D11 | PIR OUT |
| D12 | RGB LED R (220Ω 경유) |
| A0 | CDS 분압 노드 |
| A4 / A5 | LCD I2C SDA / SCL |
| 5V / GND | 공통 전원 / 접지 |

## CDS 조도센서 배선 (실제 제작 — 중요)

다리 2개짜리 CDS(광저항)는 단독으로 아날로그 값을 못 만들기 때문에 **분압 저항과 함께 분압 회로**로 연결합니다.

```
5V ──[ CDS ]──┬──[ 10kΩ ]── GND
              │
              └──> A0
```

- 밝을 때 : CDS 저항 ↓ → A0 전압(값) ↑
- 어두울 때 : CDS 저항 ↑ → A0 전압(값) ↓
- 코드에서 `analogRead(A0)` 값이 `LCD_DARK_THRESHOLD(=300)`보다 낮아지면 어두운 것으로 판단

> 분압 저항은 10kΩ을 기준으로 하며, 사용하는 CDS 사양에 따라 4.7k~10k 범위로 조정하면 됩니다.
> A0와 GND 사이에 CDS를 두는 반대 배선을 쓰면 밝고/어두움의 값 방향이 뒤집히므로,
> 그 경우 코드의 비교 부등호만 바꿔주면 됩니다.

## LCD 백라이트 자동 절전 기능 (핵심 기능)

스토리: 불을 끄고 잘 때 1602 LCD 백라이트가 너무 밝으면 눈이 부실 수 있으므로, 주변이 어두워지면 LCD 백라이트만 자동으로 끕니다.

- 사용 센서: CDS 조도센서 (다리 2개, A0 분압)
- 어두움 감지: `lcd.noBacklight();` 실행 → 배경 불빛 OFF
- 밝음 감지: `lcd.backlight();` 실행 → 배경 불빛 ON
- LCD 글씨/상태 정보는 유지되고, **배경 불빛만** 꺼지는 방식
- 시연 방법: 손으로 CDS를 가리면 LCD 백라이트가 꺼지고, 손을 떼면 다시 켜짐
- 친환경 IoT 포인트: 사용하지 않는 상황에서 표시장치 전력 소모와 빛 공해를 줄임

## 버튼 / PIR / RGB / 부저 동작

- `ALARM TOGGLE` 버튼 : 알람 ON/OFF 토글 (ON이면 부저 3회, RGB 보라)
- PIR 감지 : 움직임이 잡히면 LCD에 표시, 알람 ON 상태면 부저 5회 경보
- RGB LED 상태색
  - 초록 : 정상/대기
  - 보라 : 알람 ON
  - 파랑 : 원격(PC 전원) 명령 실행 / 감지 이벤트
  - 빨강 : DHT 읽기 오류

## 통신 조건 설명

실제 제작에서는 Arduino Uno R4 WiFi를 사용한다고 설명하면 됩니다.

- WiFi: Uno R4 WiFi 내장 무선 기능으로 Home Assistant WSS/HTTP 연동
- Bluetooth: HC-05 모듈로 Android 앱 근거리 제어
- PC 전원 누르기: Android Bluetooth 또는 Home Assistant HTTP 명령을 받으면 PC 전원 서보가 동작

현재 `sketch/sketch.ino`에는 Uno R4 WiFi의 내장 BLE 코드, HC-05 Bluetooth UART 코드, WiFi 코드, Home Assistant WebSocket/HTTP 코드가 모두 들어가 있습니다. 단, Uno R4 WiFi의 내장 Bluetooth와 내장 WiFi는 같은 무선 모듈/안테나를 공유하므로 같은 순간에 둘을 동시에 켜는 방식은 사용할 수 없습니다.

WiFi와 Android Bluetooth 제어를 한 번에 시연하려면 HC-05 모듈을 추가하면 됩니다. 이 경우 Arduino는 내장 WiFi로 공유기/Home Assistant에 붙고, HC-05는 `Serial1`로 Android 명령을 전달합니다.

### Arduino 설정 플래그

`sketch/sketch.ino` 상단에서 통신 기능을 켜고 끕니다.

```cpp
#define ENABLE_BLE 0
#define ENABLE_HC05 1
#define ENABLE_WIFI 1
#define ENABLE_HA_HTTP 1
#define ENABLE_HA_WS 1
#define HA_USE_SSL 1
#define ENABLE_MQTT 0
```

- `ENABLE_BLE 1`: Uno R4 WiFi 내장 BLE로 Android 제어 사용
- `ENABLE_BLE 0`: Wokwi / 일반 Uno 시뮬레이션용
- `ENABLE_HC05 1`: HC-05 Bluetooth Classic 모듈로 Android 제어 사용
- `ENABLE_HC05 0`: HC-05 미사용
- `ENABLE_WIFI 1`: WiFi 공유기 연결 및 IP/RSSI 상태 확인
- `ENABLE_WIFI 0`: WiFi 연결 미사용
- `ENABLE_HA_HTTP 1`: Home Assistant HTTP REST API 연동 사용
- `ENABLE_HA_HTTP 0`: Home Assistant HTTP 미사용
- `ENABLE_HA_WS 1`: Home Assistant WebSocket으로 명령 이벤트 수신
- `ENABLE_HA_WS 0`: WebSocket 미사용, HTTP polling fallback 사용
- `HA_USE_SSL 1`: `https://` endpoint 사용
- `HA_USE_SSL 0`: 로컬 `http://` endpoint 사용
- `ENABLE_MQTT 1`: Home Assistant MQTT 명령/상태 연동 사용
- `ENABLE_MQTT 0`: MQTT 미사용

시연 모드는 아래처럼 나눕니다.

| 시연 | `ENABLE_BLE` | `ENABLE_HC05` | `ENABLE_WIFI` | `ENABLE_HA_HTTP` | `ENABLE_HA_WS` | `ENABLE_MQTT` |
|------|--------------|---------------|---------------|------------------|----------------|---------------|
| WiFi + HC-05 + HA WSS/HTTP | `0` | `1` | `1` | `1` | `1` | `0` |
| WiFi + HC-05 + HA HTTP polling | `0` | `1` | `1` | `1` | `0` | `0` |
| WiFi + HC-05만 | `0` | `1` | `1` | `0` | `0` | `0` |
| 내장 BLE 단독 시연 | `1` | `0` | `0` | `0` | `0` | `0` |
| WiFi 연결/IP 확인 | `0` | `0` | `1` | `0` | `0` | `0` |
| Home Assistant MQTT 제어 | `0` | `0` | `1` | `0` | `0` | `1` |
| Wokwi / 일반 Uno 시뮬레이션 | `0` | `0` | `0` | `0` | `0` | `0` |

즉, HC-05를 붙이면 과제에서 "WiFi와 Bluetooth가 한 번에 된다"를 같은 업로드로 보여줄 수 있습니다. 단, HC-05는 BLE가 아니라 Bluetooth Classic SPP입니다.

### Android 앱 연동값

내장 BLE를 사용할 때 Android 앱은 BLE Peripheral 이름 `OpenClaw-R4`를 찾아 연결합니다.

| 항목 | 값 |
|------|----|
| Service UUID | `19b10000-e8f2-537e-4f6c-d104768a1214` |
| Command Characteristic UUID | `19b10001-e8f2-537e-4f6c-d104768a1214` |
| Status Characteristic UUID | `19b10002-e8f2-537e-4f6c-d104768a1214` |

Android 앱은 Command Characteristic에 UTF-8 문자열을 write 하면 됩니다.

| 명령 | 동작 |
|------|------|
| `PC_POWER` | PC 전원 버튼 서보 1회 누름 |
| `ALARM_TOGGLE` | 알람 ON/OFF 토글 |
| `ALARM_ON` | 알람 ON |
| `ALARM_OFF` | 알람 OFF |
| `STATUS` | 상태값 갱신 |

Status Characteristic은 아래 같은 문자열을 read/notify 합니다.

```text
alarm=ON,motion=OFF,temp=25.0,humidity=55
```

HC-05를 사용할 때 Android 앱은 BLE GATT가 아니라 Bluetooth Classic SPP로 연결합니다.

| 항목 | 값 |
|------|----|
| 장치 이름 | 보통 `HC-05` |
| 기본 PIN | 보통 `1234` 또는 `0000` |
| SPP UUID | `00001101-0000-1000-8000-00805F9B34FB` |
| 전송 방식 | BluetoothSocket output stream에 문자열 전송 |

Android 앱은 아래 명령 문자열을 HC-05로 보내면 됩니다. 줄바꿈(`\n`) 또는 세미콜론(`;`)을 붙여 보내면 가장 안정적입니다.

```text
PC_POWER
ALARM_TOGGLE
ALARM_ON
ALARM_OFF
STATUS
```

예:

```text
PC_POWER\n
```

### HC-05 배선

Arduino 코드는 HC-05를 `Serial1`로 읽습니다. Uno R4 WiFi의 USB Serial과 `D0/D1` 하드웨어 Serial은 분리되어 있어서, USB로 Serial Monitor를 보면서 HC-05를 `D0/D1`에 연결해도 됩니다.

| HC-05 핀 | Arduino Uno R4 WiFi | 설명 |
|----------|---------------------|------|
| `VCC` | `5V` | 일반 HC-05 breakout 보드는 5V 입력 가능 |
| `GND` | `GND` | 공통 접지 |
| `TXD` | `D0 / RX1` | HC-05가 보내는 데이터 → Arduino 수신 |
| `RXD` | `D1 / TX1` | Arduino가 보내는 데이터 → HC-05 수신 |
| `EN` / `KEY` | 연결 안 함 | AT 설정 모드가 필요할 때만 사용 |
| `STATE` | 연결 안 함 | 연결 상태 확인용, 필수 아님 |

주의할 점:

- `TXD`와 `RXD`는 교차 연결합니다. `TXD -> RX1`, `RXD -> TX1`입니다.
- HC-05 breakout 보드는 `VCC`는 5V를 받아도, `RXD` 신호 입력은 3.3V 권장인 경우가 많습니다.
- Arduino `D1/TX1`에서 HC-05 `RXD`로 가는 선에는 전압 분배를 넣는 것이 안전합니다.
- 전압 분배 예: Arduino `D1/TX1` -- `1kΩ` -- HC-05 `RXD` -- `2kΩ` -- GND.
- HC-05 `TXD`에서 Arduino `D0/RX1`로 가는 선은 보통 바로 연결해도 Arduino가 HIGH로 인식합니다.
- 코드 기본 UART 속도는 `9600` baud입니다. 대부분 HC-05 기본 통신 속도와 같습니다.

### Home Assistant WSS/HTTP 연동값

MQTT broker 없이 Home Assistant API로 직접 연결합니다. HA → Arduino 명령은 `wss://ha.yeoun.org/api/websocket` 상시 연결로 받고, Arduino → HA helper 제어도 가능하면 WebSocket `call_service`로 되돌려 보냅니다. 온습도/상태 엔티티 업로드만 REST API `POST /api/states/...`를 느리게 사용합니다. Home Assistant API는 `Authorization: Bearer TOKEN` 인증이 필요합니다.

현재 Arduino 코드는 공개 엔드포인트 `https://ha.yeoun.org` 기준입니다.

민감정보는 `sketch/arduino_secrets.h`에 넣습니다. GitHub에는 올라가지 않도록 제외되어 있으니, 새 환경에서는 `sketch/arduino_secrets.example.h`를 `sketch/arduino_secrets.h`로 복사한 뒤 값을 채웁니다.

```cpp
#define SECRET_WIFI_SSID "SK_1194_2.4G"
#define SECRET_WIFI_PASSWORD "YOUR_WIFI_PASSWORD"

#define SECRET_HA_HOST "ha.yeoun.org"
#define SECRET_HA_PORT 443
#define SECRET_HA_TOKEN "YOUR_HOME_ASSISTANT_LONG_LIVED_ACCESS_TOKEN"
```

로컬 IP로 직접 붙이고 싶으면 `SECRET_HA_HOST`를 로컬 IP로 바꾸고, `SECRET_HA_PORT 8123`, `HA_USE_SSL 0`으로 바꿉니다.

Arduino가 사용하는 HA 엔티티는 다음과 같습니다.

| 엔티티 | 방향 | 역할 |
|--------|------|------|
| `sensor.openclaw_status` | Arduino → HA | 알람/감지/온습도/WiFi 상태 |
| `binary_sensor.openclaw_motion` | Arduino → HA | PIR 움직임 감지 |
| `input_boolean.openclaw_alarm` | HA → Arduino | 대시보드 토글로 알람 ON/OFF |
| `input_boolean.openclaw_pc_power` | HA → Arduino | 대시보드 토글로 PC 전원 서보 1회 누름 |
| `input_button.openclaw_pc_power` | HA → Arduino | 호환용 버튼 helper. 누른 시간이 바뀌면 PC 전원 서보 1회 누름 |
| `input_text.openclaw_command` | HA → Arduino | 예비/디버깅용 문자열 명령 |

HA에서 해야 할 일:

1. 프로필에서 Long-Lived Access Token 생성
2. 설정 → 기기 및 서비스 → Helpers에서 Toggle helper 생성
3. Toggle helper 엔티티 ID를 `input_boolean.openclaw_alarm`으로 맞춤
4. Toggle helper 생성
5. Toggle helper 엔티티 ID를 `input_boolean.openclaw_pc_power`로 맞춤
6. 선택: Text helper를 `input_text.openclaw_command`로 만들면 `PC_POWER`, `ALARM_ON` 같은 문자열 명령도 직접 테스트 가능

Arduino는 `input_boolean.openclaw_alarm` 상태를 따라 알람을 켜고 끕니다. PC 전원은 `input_boolean.openclaw_pc_power`가 `on`이 되면 서보를 1회 누른 뒤 다시 `off`로 돌립니다. 기존에 `input_button.openclaw_pc_power`를 만들어 둔 경우도 호환되며, 버튼 state timestamp가 바뀔 때 서보를 1회 누릅니다. 권장값은 `input_boolean.openclaw_pc_power`입니다. 이미 `on`에 머물러 있으면 다음 `turn_on`에서 `state_changed` 이벤트가 안 나므로 서보가 다시 움직이지 않습니다.

WebSocket이 켜져 있으면 HA 토글/버튼 변경은 `state_changed` 이벤트로 즉시 수신합니다. PC power helper reset과 물리 알람 버튼의 HA 동기화는 WebSocket `call_service`를 우선 사용하고, WS가 끊겼을 때만 HTTP로 fallback합니다. HTTP 요청은 온습도/상태 업로드, PIR 상태 전송, fallback helper reset에만 사용하며 명령 직후에는 즉시 status POST를 하지 않습니다. `ENABLE_HA_WS 0`으로 끄면 기존 HTTP polling fallback을 사용합니다.

PC 전원 서보와 부저는 `delay()`로 loop를 막지 않는 비동기 방식입니다. 따라서 HA에서 PC 전원 버튼을 빠르게 다시 눌러도 WebSocket 수신과 helper reset이 계속 처리됩니다.

물리 알람 버튼을 누른 직후에는 로컬 상태를 우선합니다. HA에서 늦게 도착한 이전 WebSocket 이벤트가 알람을 다시 되돌리는 것을 막기 위해 짧은 동기화 구간 동안 반대 상태 이벤트를 무시합니다.

Uno R4 WiFi에서 외부 인터럽트는 D2/D3만 지원합니다. D2는 DHT 센서가 사용하므로, 짧은 버튼 클릭을 안정적으로 잡기 위해 ALARM 버튼은 D3에 연결하고 RGB 빨강은 D12로 옮겼습니다.

### Home Assistant MQTT 연동값

HTTP 대신 MQTT를 사용할 때만 이 옵션을 켭니다. 현재 기본 연결 방식은 HTTP입니다.

```cpp
#define ENABLE_HA_HTTP 0
#define ENABLE_MQTT 1
```

외부 인터넷 접속은 Arduino가 직접 인터넷 API에 붙는 방식보다, 사용자가 인터넷에서 Home Assistant에 접속하고 Home Assistant가 내부 MQTT로 Arduino를 제어하는 구조가 안전합니다.

### 작업 전에 준비할 것

1. Arduino IDE에서 보드를 `Arduino UNO R4 WiFi`로 선택
2. 라이브러리 매니저에서 `LiquidCrystal_I2C`, `DHT11` 설치
3. 동시 시연이 필요하면 HC-05를 `D0/D1`에 연결
4. Android 앱에서 Bluetooth Classic SPP 연결 후 문자열 명령 전송 구현
5. WiFi 시연을 위해 `ENABLE_BLE 0`, `ENABLE_HC05 1`, `ENABLE_WIFI 1`, `ENABLE_HA_HTTP 1`, `ENABLE_HA_WS 1`, `HA_USE_SSL 1`로 둠
6. `sketch/arduino_secrets.h`에 WiFi 비밀번호와 Home Assistant Long-Lived Access Token 입력
7. HA에서 `input_boolean.openclaw_alarm`, `input_boolean.openclaw_pc_power` helper 생성
8. 실제 보드 업로드 전 Wokwi만 돌릴 때는 `ENABLE_BLE 0`, `ENABLE_HC05 0`, `ENABLE_WIFI 0`, `ENABLE_HA_HTTP 0`, `ENABLE_MQTT 0`으로 변경

## 필요 라이브러리 (Arduino IDE)

- `Servo` (기본 내장)
- `Wire` (기본 내장)
- `LiquidCrystal_I2C`
- `DHT11`
- `ArduinoMqttClient` (Home Assistant MQTT 연동을 켤 때만 필요, HTTP 방식에는 불필요)
- `ArduinoBLE` (내장 BLE 단독 시연을 켤 때만 필요)

## 업로드 방법

1. Arduino IDE에서 위 라이브러리 설치
2. 위 핀맵·배선표대로 회로 구성 (CDS는 분압 회로로 연결)
3. `sketch/sketch.ino` 열기
4. 보드를 Arduino Uno (또는 Uno R4 WiFi)로 선택 후 업로드

## (선택) Wokwi에서 미리 보기

1. https://wokwi.com 접속 → Arduino Uno 새 프로젝트 생성
2. `diagram.json` 내용 교체
3. `sketch/sketch.ino` 내용 교체 후 Run
4. 단, Wokwi에는 2핀 CDS가 없어 광센서 모듈로 대체되어 있음 (동작/시연은 동일)
