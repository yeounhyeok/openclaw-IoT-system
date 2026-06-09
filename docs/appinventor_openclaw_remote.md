# OpenClaw MIT App Inventor Bluetooth Remote

이 문서는 Android 휴대폰에서 HC-05 Bluetooth Classic SPP로 OpenClaw Arduino를 조작하는 MIT App Inventor 앱 설계입니다.

Home Assistant 앱은 인터넷 제어용이고, 이 앱은 과제의 Bluetooth 근거리 제어용입니다.

## 전제

- Arduino: `ENABLE_HC05 1`
- HC-05 baud: `9600`
- Android 설정에서 HC-05를 먼저 페어링
- 기본 PIN은 보통 `1234` 또는 `0000`
- App Inventor: <https://ai2.appinventor.mit.edu>

Arduino가 받는 명령은 줄바꿈(`\n`)으로 끝나면 가장 안정적입니다.

```text
PC_POWER
ALARM_TOGGLE
ALARM_ON
ALARM_OFF
STATUS
```

## Designer 구성

프로젝트 이름 예시: `OpenClawRemote`

### Screen1

권장 속성:

| 속성 | 값 |
|---|---|
| `AppName` | `OpenClaw Remote` |
| `Title` | `OpenClaw Remote` |
| `Sizing` | `Responsive` |
| `AlignHorizontal` | `Center` |

### Visible Components

| 컴포넌트 | 이름 | 주요 속성 |
|---|---|---|
| `Label` | `LabelTitle` | `Text`: `OpenClaw Remote` |
| `Label` | `LabelConnection` | `Text`: `Disconnected` |
| `ListPicker` | `ListPickerConnect` | `Text`: `Connect HC-05` |
| `Button` | `ButtonPcPower` | `Text`: `PC Power` |
| `Button` | `ButtonAlarmToggle` | `Text`: `Alarm Toggle` |
| `Button` | `ButtonAlarmOn` | `Text`: `Alarm On` |
| `Button` | `ButtonAlarmOff` | `Text`: `Alarm Off` |
| `Button` | `ButtonStatus` | `Text`: `Status` |
| `TextBox` | `TextBoxCustom` | `Hint`: `Custom command` |
| `Button` | `ButtonSendCustom` | `Text`: `Send` |
| `Label` | `LabelLastCommand` | `Text`: `Last command: -` |
| `Label` | `LabelStatus` | `Text`: `Status: -` |

배치는 `VerticalArrangement` 안에 넣고, 버튼들은 `HorizontalArrangement`로 2개씩 묶으면 시연 화면이 깔끔합니다.

### Non-visible Components

| 컴포넌트 | 이름 | 주요 속성 |
|---|---|---|
| `BluetoothClient` | `BluetoothClient1` | `DelimiterByte`: `10` |
| `Notifier` | `Notifier1` | 기본값 |
| `Clock` | `ClockStatus` | `TimerInterval`: `300`, `TimerEnabled`: `true` |

`DelimiterByte 10`은 newline `\n`입니다. Arduino가 `Serial1.println()`으로 보내는 상태 응답을 줄 단위로 읽기 위한 설정입니다.

## Blocks 구성

### 1. 연결 목록 표시

`ListPickerConnect.BeforePicking`

```text
set ListPickerConnect.Elements to BluetoothClient1.AddressesAndNames
```

### 2. HC-05 연결

`ListPickerConnect.AfterPicking`

```text
if BluetoothClient1.Connect(ListPickerConnect.Selection)
then
  set LabelConnection.Text to join "Connected: " ListPickerConnect.Selection
  set ListPickerConnect.Text to "Reconnect"
  call Notifier1.ShowAlert("HC-05 connected")
else
  set LabelConnection.Text to "Connection failed"
  call Notifier1.ShowAlert("HC-05 connection failed")
```

연결 실패 시 Android Bluetooth 설정에서 HC-05가 먼저 페어링되어 있는지 확인합니다.

### 3. 공통 명령 전송 Procedure

Procedure 이름: `SendCommand`

인자: `command`

```text
if BluetoothClient1.IsConnected
then
  call BluetoothClient1.SendText(join command "\n")
  set LabelLastCommand.Text to join "Last command: " command
else
  call Notifier1.ShowAlert("Connect HC-05 first")
```

### 4. 버튼 이벤트

`ButtonPcPower.Click`

```text
call SendCommand("PC_POWER")
```

`ButtonAlarmToggle.Click`

```text
call SendCommand("ALARM_TOGGLE")
```

`ButtonAlarmOn.Click`

```text
call SendCommand("ALARM_ON")
```

`ButtonAlarmOff.Click`

```text
call SendCommand("ALARM_OFF")
```

`ButtonStatus.Click`

```text
call SendCommand("STATUS")
```

`ButtonSendCustom.Click`

```text
call SendCommand(TextBoxCustom.Text)
```

### 5. Arduino 상태 수신

`ClockStatus.Timer`

```text
if BluetoothClient1.IsConnected
then
  if BluetoothClient1.BytesAvailableToReceive > 0
  then
    set LabelStatus.Text to BluetoothClient1.ReceiveText(-1)
```

`ReceiveText(-1)`은 `DelimiterByte`를 만날 때까지 읽습니다. `BluetoothClient1.DelimiterByte`가 `10`이어야 합니다.

## 시연 플로우

1. Android Bluetooth 설정에서 `HC-05` 페어링
2. OpenClaw 앱 실행
3. `Connect HC-05` 클릭
4. 목록에서 `HC-05` 선택
5. `Status` 클릭해서 `STATUS alarm=...` 응답 확인
6. `Alarm On`, `Alarm Off`, `PC Power` 버튼 시연

## 문제 해결

| 증상 | 원인/해결 |
|---|---|
| 목록에 HC-05가 안 보임 | Android 설정에서 먼저 페어링 |
| 연결 실패 | HC-05 전원/GND/TX/RX 확인, PIN `1234`/`0000` 확인 |
| 버튼 눌러도 반응 없음 | Arduino `ENABLE_HC05 1`, HC-05 baud `9600`, RX/TX 교차 연결 확인 |
| 글자가 깨짐 | `BluetoothClient1.DelimiterByte 10`, 전송 문자열 끝 `\n`, baud `9600` 확인 |
| STATUS가 안 보임 | `ClockStatus.TimerEnabled true`, `TimerInterval 300`, `ReceiveText(-1)` 확인 |

## 제출 설명 문장

> Android 앱은 MIT App Inventor로 구현했으며, HC-05 Bluetooth Classic SPP를 통해 `PC_POWER`, `ALARM_ON`, `ALARM_OFF`, `STATUS` 등의 문자열 명령을 Arduino `Serial1`로 전송한다. Arduino는 같은 명령 처리 함수를 Home Assistant WebSocket과 HC-05 양쪽에서 공유하므로, 인터넷 제어와 근거리 Bluetooth 제어가 동일한 동작을 수행한다.
