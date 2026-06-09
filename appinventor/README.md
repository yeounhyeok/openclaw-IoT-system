# OpenClaw App Inventor Remote

Import `OpenClawRemote.aia` in MIT App Inventor:

1. Open <https://ai2.appinventor.mit.edu>
2. Select `Projects`
3. Select `Import project (.aia) from my computer`
4. Upload `OpenClawRemote.aia`
5. Pair Android with `HC-05` in Android Bluetooth settings
6. Build/install the app and connect to `HC-05`

The app sends these Bluetooth Classic SPP commands to Arduino:

```text
PC_POWER;
ALARM_TOGGLE;
ALARM_ON;
ALARM_OFF;
STATUS;
```

The semicolon is intentional. The Arduino HC-05 parser treats `;`, `\n`, and `\r` as command terminators.
