# MCCI PMS7003 Library

This library provides a structured interface to a Plantower PMS7003 particulate matter sensor. An example demonstrates the functionality of the PMS7003 with the MCCI Catena 4630 Air-Quality Sensor.

<!-- TOC depthFrom:2 updateOnSave:true -->

- [Key classes](#key-classes)
	- [`cPMS7003Hal`](#cpms7003hal)
	- [`cPMS7003Hal_4630`](#cpms7003hal_4630)
	- [`cPMS7003`](#cpms7003)
- [Integration with Catena 4630](#integration-with-catena-4630)
- [Example Sketch: Catena4630-pm7003-demo](#example-sketch-catena4630-pm7003-demo)
	- [Functions performed by this sketch](#functions-performed-by-this-sketch)
	- [Commands](#commands)
		- [`begin`](#begin)
		- [`debugmask`](#debugmask)
		- [`end`](#end)
		- [`hwsleep`](#hwsleep)
		- [`measure`](#measure)
		- [`normal`](#normal)
		- [`off`](#off)
		- [`passive`](#passive)
		- [`reset`](#reset)
		- [`sleep`](#sleep)
		- [`stats`](#stats)
		- [`wake`](#wake)

<!-- /TOC -->
## Key classes

### `cPMS7003Hal`

This is the abstract class which provides the HAL for the `cPMS7003` library. This is not modified for porting, unless we discover that it is insufficiently abstract.

### `cPMS7003Hal_4630`

This is the concrete HAL implementation for using the library on the Catena 4630. If porting to another platform, you will want to copy this (with a different name), and modify the method functions as appropriate.

### `cPMS7003`

This class models the low-level hardware of PMS7003 PM2.5 sensor. The class is partially abstract, in that it expects a wrapper class to provide virtual overrides for power control and the GPIOs. It uses the `cPollableObject` paradigm to drive accumulation of data from the sensor.

It models the device with an FSM shown in the next figure.

<!--
See source in assets/PMS7003_state.plantuml
-->
[![**FSM for PMS7003 low-level driver**](http://www.plantuml.com/plantuml/png/TLJVRzis47uM_ufxQe4rjfpjOPYn5WqS3-iGfu7jhi2MOM6aaqrC93N-Y6eiszy-YfALIna_YRhxxllkE_vuRnqtpikQvQyPM-dIrpZFRbQRxsUpx_uCIOVJfkOgGjXFNXLEQ3LdsKqN_BIw0eCL6bG5WjNUX4-b3HG30rHMJbd65hev6B7Rkr2vGGHU55esN1s4si7LXyNoUhE4IjGXurYsTwwnytcUxuJR-_jfmEQoACIau0uLpnVnCxamfHTAJq8hBeZAe7FXtLwR8B95OmwmqjhW6QmMYm-mqARHXdVjPx_u-W3Vpiv_OEnQExJKZv1yXnQ4WlNoOdMvdCeIk47OeS9GZYNS7w3XrdMjkxsmVTz2ESqojqUmQ1xG07VG-GWkshOTPnYz77_EWAiITeRVt_Tao3A3rjUH-zFwoauPUvqpjXSb0ih67Q14MnZHT3OiOxz_ymTC9k_0kezLnLZy384r_ktHkXlTelceO0xuy9d_XOozb5BE77BOUgD0A_YrB6yLG3mQS3wRIGjEI3IG5ezY5btKs7P0naYev-RMOc0pNPIGG-Ic9P5nxPKACtXYX1owtDrwkLbkxs0l9hzztSobReaNvqqq0hggpb8hBzMEWoIohrx1VzNoYt2d2i8EF8VtQ1QlpkZZ7vOzRTLdRgHl6CkEox7hw56jqDfu1j_2Z81eSPIynlQIEnv4ISsFmMMORmvltep1XEHQ6ydhqoJltWQMq211aWRlnYsWa9xvvtPJ8_Rz3Ui44wCBMKRGov0RRgrum0sgyWWSGa6IgYHjrRgbY8adKnilmQrwAZWFZAGdGgbAi96aa2N576qGgzilbrfXptHKDX7LoOKQSlyo6gHYROZwg4Onr44Ybiw9y3LzCXAcs6U3Fi4RnCbveRlHHoZqzFvJK7e0XbkLGcb_A-JMct6GmoyvVYzFGCLS7422V6OjzP_A3iH5ZTo0x3Co1Czk5KIvOp3g9IJfpEdrUtmStX3j7Sl-PMnuEPBtDQwIRFeAFeHtBB0UdwFteJylw_y1)](https://www.plantuml.com/plantuml/svg/TLJVRzis47uM_ufxQe4rjfpjOPYn5WqS3-iGfu7jhi2MOM6aaqrC93N-Y6eiszy-YfALIna_YRhxxllkE_vuRnqtpikQvQyPM-dIrpZFRbQRxsUpx_uCIOVJfkOgGjXFNXLEQ3LdsKqN_BIw0eCL6bG5WjNUX4-b3HG30rHMJbd65hev6B7Rkr2vGGHU55esN1s4si7LXyNoUhE4IjGXurYsTwwnytcUxuJR-_jfmEQoACIau0uLpnVnCxamfHTAJq8hBeZAe7FXtLwR8B95OmwmqjhW6QmMYm-mqARHXdVjPx_u-W3Vpiv_OEnQExJKZv1yXnQ4WlNoOdMvdCeIk47OeS9GZYNS7w3XrdMjkxsmVTz2ESqojqUmQ1xG07VG-GWkshOTPnYz77_EWAiITeRVt_Tao3A3rjUH-zFwoauPUvqpjXSb0ih67Q14MnZHT3OiOxz_ymTC9k_0kezLnLZy384r_ktHkXlTelceO0xuy9d_XOozb5BE77BOUgD0A_YrB6yLG3mQS3wRIGjEI3IG5ezY5btKs7P0naYev-RMOc0pNPIGG-Ic9P5nxPKACtXYX1owtDrwkLbkxs0l9hzztSobReaNvqqq0hggpb8hBzMEWoIohrx1VzNoYt2d2i8EF8VtQ1QlpkZZ7vOzRTLdRgHl6CkEox7hw56jqDfu1j_2Z81eSPIynlQIEnv4ISsFmMMORmvltep1XEHQ6ydhqoJltWQMq211aWRlnYsWa9xvvtPJ8_Rz3Ui44wCBMKRGov0RRgrum0sgyWWSGa6IgYHjrRgbY8adKnilmQrwAZWFZAGdGgbAi96aa2N576qGgzilbrfXptHKDX7LoOKQSlyo6gHYROZwg4Onr44Ybiw9y3LzCXAcs6U3Fi4RnCbveRlHHoZqzFvJK7e0XbkLGcb_A-JMct6GmoyvVYzFGCLS7422V6OjzP_A3iH5ZTo0x3Co1Czk5KIvOp3g9IJfpEdrUtmStX3j7Sl-PMnuEPBtDQwIRFeAFeHtBB0UdwFteJylw_y1 "Click for SVG version")

Features of this FSM:

1. The PMS7003 is initially powered down.
2. When a wake event is received from the application, the FSM powers up the PMS7003 and takes it through a wakeup sequence. Data received while in the warmup state is indicated up to the client, but tagged as warming up.
3. Once the library has finished the warmup seqeunce, it allows the client to operat the PMS7003 in "normal" mode (where the PMS7003 sends data spontaneously as it sees fit), or in "passive" mode (where the application must request a measurement).
4. At any time, the client may power down or reset the PMS7003.
5. While the PMS7003 is active, the client may select a low-power sleep mode, eithe via a hardware sleep (using the SET pin) or a software sleep (using a command).
6. Waking up the PMS7003 from sleep is the same as starting from power off; it must go through a warmup cycle. However, the timing for waking from sleep is much more deterministic. Starting from power off takes anywhere from 5 to 45 seconds (empirically determined); starting from sleep takes about 3 seconds to the first warmup message, and about 12 seconds to full operation.

## Integration with Catena 4630

The Catena 4630 has the following features.

- LoRaWAN-compatible radio and software

- Bosch BME280 temperature/humidity/pressure sensor

- IDT ZMOD4410 gas sensor

- Interface to external Plantower PMS7003 particulate matter sensor, including 5V boost regulator and dedicated connector.

## Example Sketch: Catena4630-pm7003-demo

The `catena4630-pm7003-demo` example sketch only shows local use of the PMS7003, but it initializes the radio and BME280 so as to minimize power in standby.

### Functions performed by this sketch

This sketch has the following features.

- During startup, the sketch initializes the PMS7003 and sets up a commmand line environment allowing you to experiment with various features of the PMS7003 driver.

- The sketch also initializes the LoRaWAN radio and the BME200 sensors to put them in a low-power state.

- During operation, the sketch monitors the PMS7003 and displays the data received from the PMS7003 on the serial monitor.

- The sketch uses the [Catena Arduino Platform](https://github.com/mcci-catena/Catena-Arduino-Platform.git), and therefore the basic provisioning commands from the platform are always availble while the sketch is running. This also allows user commands to be added if desired.

- The `McciCatena::cPollableObject` paradigm is used to simplify the coordination of the activities described above.

### Commands

In addition to the [default commands](https://github.com/mcci-catena/Catena-Arduino-Platform#command-summary) provided by the library, the sketch provides the following commands:

#### `begin`

Call the the `cPMS7003.begin()` method. Note that the `setup()` routine already calls this; this is only provided for test purposes (after you use the `end` command, you must use the `begin` command to restart).

#### `debugmask`

Get or set the debug mask, which controls the verbosity of debug output from the library.

To get the debug mask, enter command `debugmask` on a line by itself.

To set the debug mask, enter <code>debugmask <em>number</em></code>, where number is a C-style number indicating the value. For example, `debugmask 0x31` is the same as `debugmask 49` -- it turns on bits 0, 4, and 5.

The following bits are defined.

Bit  |   Mask     | Name | Description
:---:|:----------:|------|------------
  0  | 0x00000001 | kError | Enables error messages
  1  | 0x00000002 | kWarning | Enables warning messages (none are defined at present)
  2  | 0x00000004 | kTrace   | Enables trace messages. This specifically causes the FSM transitions to be displayed.
  3  | 0x00000008 | kInfo    | Enables informational messages (none are defined at present)
  4  | 0x00000010 | kTxData  | Enable display of data sent by the library to the PMS7003
  5  | 0x00000020 | kRxDiscard | Enable display of discarded receive data bytes

#### `end`

Invoke the `cPMS7003::end()` method. This shuts down the sensor, the library and the HAL.

#### `hwsleep`

Request that the library put the PMS7003 to sleep using the `SET` pin. According to the documentation, the `SET` pin has a pullup, so this takes static power (but causes the sensor to stop the fan).  Exit sleep using the [`wake`](#wake) command.

#### `measure`

In passive mode, the `measure` command triggers a single measurement request. The library doesn't retry, and the PMS7003 often misses the first `measure` command after entering passive mode. (We're considering improving the lower-level API, in which case this command may get updated.)

#### `normal`

Switch the sensor to normal mode. In normal mode, the PMS7003 sends measurments periodicaly. (The datasheet claims every 2 seconds, but on our sensors it seems to be every 0.8 seconds.)

#### `off`

Turn the sensor off. Use the [`wake`](#wake) command to power it up.

#### `passive`

Put the sensor into passive mode. In passive mode, the sensor keeps the fan on, but doesn't send measurements. 

If the `passive` command is entered while the sensor is warming up, the library deleays switching to passive mode until the sensor is awake.

#### `reset`

Reset the PMS7003 using the hardware reset pin. This requires a warmup for recovery.

#### `sleep`

Put the sensor into sleep mode using a software command. This saves the power from driving the `SET` pin low, but it's not clear whether the PMS7003 power is as low duing software sleep as during hardware sleep.

#### `stats`

Display the receive statistics. The library keeps track of spurious characters and messages; this is an easy way to get access.

#### `wake`

Bring up the PMS7003. This event is abstract -- it requests the library to do whatever's needed (powering up the PMS7003, waking it up, etc.) to get the PMS7003 to normal state.

