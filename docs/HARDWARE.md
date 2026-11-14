# Hardware and firmware

Each firmware directory is an independent Arduino sketch. Its `.ino` filename matches the directory so it can be opened directly in Arduino IDE. Review board voltage, wiring, library compatibility, and power requirements before connecting components. All three sketches use 9600 baud.

| Sketch | Components and pins | Libraries | Serial commands |
| --- | --- | --- | --- |
| `firmware/dock_barrier/dock_barrier.ino` | I2C LCD at `0x27` (16×2), red LED 2, green LED 3, servo 9 | Wire, LiquidCrystal_I2C, Servo | Newline-terminated `1`: available/open at 90°; `0`: occupied/closed at 0° |
| `firmware/pir_alarm/pir_alarm.ino` | PIR 8, LED 2, buzzer 7 | Arduino core | Emits `MOTION`/`TIMEOUT`; accepts newline-terminated `OK`, `ALERT`, `RESET`, `ERR` |
| `firmware/rfid_reader/rfid_reader.ino` | MFRC522 with SS 10, RST 9; remaining SPI pins depend on board | SPI, MFRC522 | Emits `CODE:<UID>`; newline-terminated `GET` retrieves last UID |

## Upload and connect

1. Open the chosen `.ino` in Arduino IDE and install the listed libraries and board package.
2. Select the actual board and port, verify the sketch, then upload it.
3. Close Serial Monitor before connecting the desktop application to the port.
4. Select the matching hardware workflow in the desktop UI and enter/select the real port. Existing Windows defaults use `COM3`; Linux device paths usually start with `/dev/ttyACM` or `/dev/ttyUSB`.
5. For the existing authorized badge shortcut, configure `FISHING_PORT_AUTHORIZED_RFID` in the desktop process environment. No UID is embedded in the firmware.

`dock_barrier` replaces the original `inchirah+yasmine.ino/sketch_may4a` directory. The rename describes the sketch's contents; it does not change its protocol or assign contributor credit.

Firmware compilation and physical hardware operation have not been verified in the available environment. These sketches are prototypes and should not control a real access barrier without appropriate engineering and safety validation.
