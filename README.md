# catena4630-test1

This sketch demonstrates the functionality of the MCCI Catena 4630 Air-Quality Sensor.

The Catena 4630 has the following features.

- Bosch BME280 temperature/humidity/pressure sensor

- IDT ZMOD4410 gas sensor

- Interface to external Plantower PMS7003 particulate matter sensor, including 5V boost regulator and dedicated connector.

## Functions performed by this sketch

This sketch has the following features.

- During startup, the sketch initializes the serial port and sets the PMS7003 into standby. It also reads registers 0x88 through 0x8B from the ZMOD4410.

- During operation, the sketch monitors the PM2.5 sensor and records the data transmitted by the sensor.

- The sketch uses the [Catena Arduino Platform](https://github.com/mcci-catena/Catena-Arduino-Platform.git), and therefore the basic provisioning commands from the platform are always availble while the sketch is running. This also allows user commands to be added if desired.

- Futhermore, the `McciCatena::cPollableObject` paradigm is used to simplify the coordination of the activities described above.

## Key classes

### `cPM7003` 

This class models the low-level hardware of PMS7003 PM2.5 sensor. The class is partially abstract, in that it expects a wrapper class to provide virtual overrides for power control and the GPIOs. It uses the `cPollableObject` paradigm to drive accumulation of data from the sensor.

### `c4630_PMS` Catena 4630 Particulate matter sensor

This class provides the high-level interface to the sensor.  There are several operating modes:

- run mode, where the 5V supply is always on, and the library gets data automatically over the serial port.
- sleep mode, where the 5V supply is always on, but the library gets data on demand as triggered by the application. The latency from demand to measurement is 30 seconds.
- timed sleep mode, where the 5V supply is always on, but the library gets data periodically based on a timer firing. The timer may not fire any more often than every 30 seconds.
- power-down mode, where the 5V supply is normally off; the library gets data on demand as triggered by the app.

The class uses the "sleep" command (rather than the SET pin) to put the sensor in low-power mode.

### `cTimer` simple periodic timer class

This class simplifies the coding of periodic events driven from the Aduino `loop()` routine.
