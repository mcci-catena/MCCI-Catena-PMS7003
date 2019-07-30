# Example Sketch: Catena4630-pm7003-demo

The `catena4630-pm7003-demo` example sketch allows interactive use of the PMS7003.

<!-- markdownlint-disable MD033 -->
<!-- markdownlint-capture -->
<!-- markdownlint-disable -->
<!-- TOC depthFrom:2 updateOnSave:true -->

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
<!-- markdownlint-restore -->
<!-- Due to a bug in Markdown TOC, the table is formatted incorrectly if tab indentation is set other than 4. Due to another bug, this comment must be *after* the TOC entry. -->

## Functions performed by this sketch

This sketch has the following features.

- During startup, the sketch initializes the PMS7003 and sets up a commmand line environment allowing you to experiment with various features of the PMS7003 driver.

- The sketch also initializes the LoRaWAN radio and the BME200 sensors to put them in a low-power state.

- During operation, the sketch monitors the PMS7003 and displays the data received from the PMS7003 on the serial monitor.

- The sketch uses the [Catena Arduino Platform](https://github.com/mcci-catena/Catena-Arduino-Platform.git), and therefore the basic provisioning commands from the platform are always availble while the sketch is running. This also allows user commands to be added if desired.

- The `McciCatena::cPollableObject` paradigm is used to simplify the coordination of the activities described above.

## Commands

In addition to the [default commands](https://github.com/mcci-catena/Catena-Arduino-Platform#command-summary) provided by the library, the sketch provides the following commands:

### `begin`

Call the the `cPMS7003.begin()` method. Note that the `setup()` routine already calls this; this is only provided for test purposes (after you use the `end` command, you must use the `begin` command to restart).

### `debugmask`

Get or set the debug mask, which controls the verbosity of debug output from the library.

To get the debug mask, enter command `debugmask` on a line by itself.

To set the debug mask, enter <code>debugmask <em><u>number</u></em></code>, where *number* is a C-style number indicating the value. For example, `debugmask 0x31` is the same as `debugmask 49` -- it turns on bits 0, 4, and 5.

The following bits are defined.

Bit  |   Mask     |  Name        | Description
:---:|:----------:|--------------|------------
  0  | 0x00000001 | `kError`     | Enables error messages
  1  | 0x00000002 | `kWarning`   | Enables warning messages (none are defined at present)
  2  | 0x00000004 | `kTrace`     | Enables trace messages. This specifically causes the FSM transitions to be displayed.
  3  | 0x00000008 | `kInfo`      | Enables informational messages (none are defined at present)
  4  | 0x00000010 | `kTxData`    | Enable display of data sent by the library to the PMS7003
  5  | 0x00000020 | `kRxDiscard` | Enable display of discarded receive data bytes

### `end`

Invoke the `cPMS7003::end()` method. This shuts down the sensor, the library and the HAL.

### `hwsleep`

Request that the library put the PMS7003 to sleep using the `SET` pin. According to the documentation, the `SET` pin has a pullup, so this takes static power (but causes the sensor to stop the fan).  Exit sleep using the [`wake`](#wake) command.

### `measure`

In passive mode, the `measure` command triggers a single measurement request. The library doesn't retry, and the PMS7003 often misses the first `measure` command after entering passive mode. (We're considering improving the lower-level API, in which case this command may get updated.)

### `normal`

Switch the sensor to normal mode. In normal mode, the PMS7003 sends measurments periodicaly. (The datasheet claims every 2 seconds, but on our sensors it seems to be every 0.8 seconds.)

### `off`

Turn the sensor off. Use the [`wake`](#wake) command to power it up.

### `passive`

Put the sensor into passive mode. In passive mode, the sensor keeps the fan on, but doesn't send measurements.

If the `passive` command is entered while the sensor is warming up, the library deleays switching to passive mode until the sensor is awake.

### `reset`

Reset the PMS7003 using the hardware reset pin. This requires a warmup for recovery.

### `sleep`

Put the sensor into sleep mode using a software command. This saves the power from driving the `SET` pin low, but it's not clear whether the PMS7003 power is as low duing software sleep as during hardware sleep.

### `stats`

Display the receive statistics. The library keeps track of spurious characters and messages; this is an easy way to get access.

### `wake`

Bring up the PMS7003. This event is abstract -- it requests the library to do whatever's needed (powering up the PMS7003, waking it up, etc.) to get the PMS7003 to normal state.
