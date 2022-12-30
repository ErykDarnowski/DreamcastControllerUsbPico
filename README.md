# DreamcastControllerUsbPico

Dreamcast Controller to USB Gamepad Converter using Raspberry Pi Pico

Goals for this project:
- Detect and interact with the following:
  - Controller
  - VMU
  - Jump pack
- Setup USB HID Gamepad that supports vibration
- Setup some other USB device for VMU access
- Create whatever Linux/Windows drivers are necessary to communicate with all devices (probably will just use libusb)
- Interface with emulator such as Redream for controller, jump pack, and VMU

Refer to the [releases](https://github.com/OrangeFox86/DreamcastControllerUsbPico/releases) page for current progress. Refer to the [issues](https://github.com/OrangeFox86/DreamcastControllerUsbPico/issues) tab for things left to be implemented and known bugs.

## Why the RP2040 is a Game Changer (and what makes this project different from others)

To emulate a bespoke bus such as the Maple Bus on an MCU, one would usually either need to add extra hardware or bit bang the interface. This is not true with the RP2040 and its PIO. Think of it as several extra small processors on the side using a special machine language purpose-built for handling I/O. This means communication can be offloaded to the PIO and only check on them after an interrupt is activated or a timeout has elapsed. Check out [maple_in.pio](src/hal/MapleBus/maple_in.pio) and [maple_out.pio](src/hal/MapleBus/maple_out.pio) to see the PIO code.

Luckily, the RP2040 comes with 2 PIO blocks each with 4 separate state machines. This means that the RP2040 can easily emulate 4 separate controller interfaces, each at full speed!

---

# Quick Installation Guide

## Connecting the Hardware

The Dreamcast uses 36-ohm resistors and small fuses in series between the 8 chip I/O and the 8 controller port I/O. This is only done as a failsafe in case of hardware or software malfunction. In my implementation, I have decided to use lower value, 10-ohm resistors because some resistance is built into the Pico's outputs already. There is some weirdness with plugging in a VMU without a battery installed which gets worse with higher resistor values. I'm also using littelfuse 1/16-amp quick burning fuses, and none of them have blown during my tests. They're mostly optional, but I'd suggest using higher value resistors if they are omitted as a safety measure. Anything higher than 100-ohm starts to cause communication errors though due to capacitances on the I/O.

This is generally the setup I have been testing with:

<p align="center">
  <img src="images/schematic.png?raw=true" alt="Schematic"/>
</p>

For reference, the following is the pinout for the Dreamcast controller port. Take note that many other sources found online refer to one of the ground pins as a connection sense, but the Dreamcast controller port module has both of these ground pins hard wired together. As such, this project doesn't rely on any such hardware sense line. Instead, the detection of a connected device is performed by polling the bus until a response is received, just as a real Dreamcast would.

<p align="center">
  <img src="images/Dreamcast_Port.png?raw=true" alt="Dreamcast Port"/>
</p>

## Build Instructions (for Linux and Windows)

If running under Windows, install [WSL](https://docs.microsoft.com/en-us/windows/wsl/install) and your desired flavor of Linux. I recommend using Ubuntu 20.04 as that is what I have used for development. Then the steps below may be run within your WSL instance.

1. Install git, cmake, and gcc-arm-none-eabi compiler by running the following commands
```bash
sudo apt update
sudo apt -y install git cmake gcc-arm-none-eabi
```
2. (optional) In order to run and debug tests, install standard gcc compilers and gdb by running the following
```bash
sudo apt -y install build-essential gdb
```
3. Clone this repo then cd into the created directory
```bash
git clone https://github.com/Tails86/DreamcastControllerUsbPico.git
cd DreamcastControllerUsbPico
```
4. Pull down the pico SDK (this is optional if you have PICO_SDK_PATH set in your environment which points to the SDK somewhere on your system)
```bash
git submodule update --recursive --init
```
5. (optional) Build and run tests - this runs core lib unit tests locally
```bash
./run_tests.sh
```
6. Execute the build script
```bash
./build.sh
```

After build completes, the binary should be located at `dist/main.uf2`. Pre-built release binaries may be found [here](https://github.com/OrangeFox86/DreamcastControllerUsbPico/releases).

This project may be opened in vscode. In vscode, the default shortcut `ctrl+shift+b` will build the project. The default shortcut `F5` will run tests with gdb for local debugging. Open the terminal tab after executing tests with debugging to see the results.

## Loading the UF2 Binary

Hold the BOOTSEL button on the Pico while plugging the USB connection into your PC. A drive with a FAT partition labeled RPI-RP2 should pop up on your system. Open this drive, and then copy the main.uf2 file here. The Pico should then automatically load the binary into flash and run it. For more information, refer to the official [Raspberry Pi Pico documentation](https://www.raspberrypi.com/documentation/microcontrollers/raspberry-pi-pico.html#documentation).

---

# Maple Bus Implementation

Refer to documentation [here](https://dreamcast.wiki/Maple_bus) for general information about the Maple Bus.

## Interfacing with the PIO State Machines

The [MapleBus class](src/hal/MapleBus/MapleBus.hpp) operates as the interface between the microcontroller's code and the PIO state machines, [maple_in.pio](src/hal/MapleBus/maple_in.pio) and [maple_out.pio](src/hal/MapleBus/maple_out.pio).

Using 2 separate PIO blocks for reading and writing is necessary because each PIO block can only hold up to 32 instructions, and this interface is too complex to fit both read and write into a single block. Therefore, the write state machine is completely stopped before starting the read state machine for the targeted bus. Switching state machines is fast enough that there shouldn't be a problem. Testing showed the handoff always occurs within 1 microsecond after bringing the bus back to neutral. A device on the Maple Bus starts responding some time after 50 microseconds from the point of the bus going neutral after an end sequence. This ensures that a response is always captured.

The following lays out the phases of the state machine handled within the MapleBus class.

<p align="center">
  <img src="images/MapleBus_Class_State_Machine.png?raw=true" alt="MapleBus Class State Machine"/>
</p>

### PIO Data Handoff

When the write method is called, data is loaded into the Direct Memory Access (DMA) channel designated for use with the maple_out state machine in the MapleBus instance. The DMA will automatically load data onto the TX FIFO of the output PIO state machine so it won't stall waiting for more data.

The first 32-bit word loaded onto the output DMA is how many transmission bits will follow. In order for the state machine to process things properly, `(x - 8) % 32 == 0 && x >= 40` must be true where x is the value of that first 32-bit word i.e. every word is 32 bits long and at least a frame word (32 bits) plus a CRC byte (8 bits) are in the packet. This value needs to be loaded with byte order flipped because byte swap is enabled in the DMA so that all other words are written in the correct byte order. The rest of the data loaded into DMA is the entirety of a single packet as a uint32 array. The last uint32 value holds the 8-bit CRC.

A blocking IRQ is triggered once the maple_out state machine completes the transfer. This then allows MapleBus to stop the maple_out state machine and start the maple_in state machine.

A Direct Memory Access (DMA) channel is setup to automatically pop items off of the RX FIFO of the maple_in state machine so that the maple_in state machine doesn't stall while reading. Once the IRQ is triggered by the maple_in state machine, MapleBus stops the state machine and reads from data in the DMA.

## Generating Maple Bus Output

The [maple_out PIO state machine](src/hal/MapleBus/maple_out.pio) handles Maple Bus output. Data is generated by following the signal definition [here](https://dreamcast.wiki/Maple_bus#Maple_Bus_Signals).

## Sampling Maple Bus Input

The [maple_in PIO state machine](src/hal/MapleBus/maple_in.pio) handles Maple Bus input. Some concessions had to be made in order to handle all input operations within the 32 instruction set limit of the input PIO block. The following are the most notable limitations.
- Only a standard data packet may be sampled
    - The Maple Bus protocol has different types of packets depending on how many times B pulses in the start sequence, but those packets are ignored in this implementation
- The full end sequence is not sampled
    - The packet length in the frame word plus the CRC are relied upon during post-processing in order to verify that the received packet is valid

### Sampling the Start Sequence

The input PIO state machine will wait until **A** transitions LOW and then count how many times **B** toggles LOW then HIGH while making sure **A** doesn't transition HIGH until after **B** transitions HIGH. If the toggle count isn't 4, then the state machine keeps waiting. Otherwise, the state machine signals the application with a non-blocking IRQ and continues to the next phase where data bits are sampled.

### Sampling Data Bits

For each bit, the state machine first waits for the designated clock to be HIGH before proceeding. Then once this line transitions to LOW, the state of the designated data line is sampled. State transitions of the designated data line are ignored except for the case when sensing the end sequence is required as described in the next section.

### Sampling the End Sequence

Whenever **A** is designated as the clock, the input PIO state machine will detect when **B** toggles HIGH then LOW while **A** remains HIGH. It is assumed that this is the beginning of the end sequence since this is not a normal behavior during data transmission. The state machine will then block on an IRQ so that the application can handle the received data.

---

# Appendix A: Abbreviations and Definitions

- `0x` Prefix: The following value is hex format
- Byte: Data consisting of 8 consecutive bits
- DMA: Direct Memory Access
- LSB: Least Significant Byte
- LSb: Least Significant bit
- MSB: Most Significant Byte
- MSb: Most Significant bit
- Nibble: Data consisting of 4 consecutive bits
- PIO: Programmable Input/Output
- SDCK: Serial Data and Clock I/O
- Word: Data consisting of 32 consecutive bits

---

# External Resources

**Maple Bus Resources**

https://archive.org/details/MaplePatent/page/n7/mode/1up

http://mc.pp.se/dc/maplebus.html and http://mc.pp.se/dc/controller.html

https://tech-en.netlify.app/articles/en540236/index.html

https://www.raphnet.net/programmation/dreamcast_usb/index_en.php

https://web.archive.org/web/20100425041817/http://www.maushammer.com/vmu.html

https://hackaday.io/project/170365-blueretro/log/180790-evolution-of-segas-io-interface-from-sg-1000-to-saturn

https://segaretro.org/History_of_the_Sega_Dreamcast/Development
