# catena4630-pms7003-lora

This sketch is a fully-worked air-quality sensor based on the Catena 4630 and the PMS7003.

Data is read and transmitted from the PMS7003 every six minutes (although it tries to run every 30 seconds for the first ten measurements after each reboot).

<!-- markdownlint-disable MD033 -->
<!-- markdownlint-capture -->
<!-- markdownlint-disable -->
<!-- TOC depthFrom:2 updateOnSave:true -->

- [Functions performed by this sketch](#functions-performed-by-this-sketch)
- [Commands](#commands)
	- [`debugmask`](#debugmask)
	- [`run`, `stop`](#run-stop)
	- [`stats`](#stats)
	- [`wake`](#wake)

<!-- /TOC -->
<!-- markdownlint-restore -->
<!-- Due to a bug in Markdown TOC, the table is formatted incorrectly if tab indentation is set other than 4. Due to another bug, this comment must be *after* the TOC entry. -->

## Functions performed by this sketch

This sketch has the following features.

- During startup, the sketch initializes the PMS7003.

- If the device is provisioned as a LoRaWAN device, it enters the measurement loop.

- Every measurement cycle, the sketch powers up the PMS7003. It then takes a sequence of 10 measurements. Data is gathered from the atmospheric PM serias and the dust series. For each series, outliers are discarded using an IQR1.5 filter, and then the remaining data is averaged.

- Current environmental conditions are read from the BME280.

- Data is prepared using port 1 format 0x20, and transmitted to the network.

- The sketch uses the [Catena Arduino Platform](https://github.com/mcci-catena/Catena-Arduino-Platform.git), and therefore the basic provisioning commands from the platform are always availble while the sketch is running. This also allows user commands to be added if desired.

- The `McciCatena::cPollableObject` paradigm is used to simplify the coordination of the activities described above.

## Commands

In addition to the [default commands](https://github.com/mcci-catena/Catena-Arduino-Platform#command-summary) provided by the library, the sketch provides the following commands:

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

### `run`, `stop`

Start or stop the measurement loop. After boot, the measurement loop is enabled by default.

### `stats`

Display the receive statistics. The library keeps track of spurious characters and messages; this is an easy way to get access.

### `wake`

Bring up the PMS7003. This event is abstract -- it requests the library to do whatever's needed (powering up the PMS7003, waking it up, etc.) to get the PMS7003 to normal state.
