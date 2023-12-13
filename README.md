# DreamcastControllerUsbPico

Dreamcast Controller to USB Gamepad Converter using Raspberry Pi Pico

---

## Build Instructions (for Linux and Windows)

In order to do this, get Linux (for smooth operation **USE Ubuntu 20.04** - it's been used for development) running somehow (Native / Virtual Box / WSL / etc.).

1. Install `git`, `cmake`, and the `gcc-arm-none-eabi` compiler:
```bash
sudo apt update
sudo apt upgrade
sudo apt -y install git cmake gcc-arm-none-eabi
```

2. (optional) In order to run and debug tests, install standard gcc compilers and `gdb`:
```bash
sudo apt -y install build-essential gdb
```

3. Clone the repo, `cd` into it and checkout the `single-controller` branch:
```bash
git clone https://github.com/Tails86/DreamcastControllerUsbPico.git
cd DreamcastControllerUsbPico
git checkout single-controller
```

4. Pull down the pico SDK (this project now uses a fork of the SDK - it will no longer compile under the standard one)
```bash
git submodule update --recursive --init
```

\*If you have issues building, the easiest way to correct any submodule synchronization issue is to delete the `ext/pico-sdk` directory (`rm -rf ext/pico-sdk`), and then re-run the above command.

5. (optional) Build and run tests - this runs core lib unit tests locally
```bash
./run_tests.sh
```

6. Execute the build script
```bash
./build.sh
```

After build completes, binaries should be located under `dist/`.

\*In order to clean up files between builds and such run: `./very_clean.sh`.
\*This project may be opened in *VS Code*.
In VS Code, the default shortcut (<kbd>Ctrl</kbd> + <kbd>Shift</kbd> + <kbd>b</kbd> will
build the project. The <kbd>F5</kbd> shortcut will run tests with `gdb` for local debugging.
Open the terminal tab after executing tests with debugging to see the results.

### Combined

```bash
sudo apt update
sudo apt upgrade
sudo apt -y install git gdb cmake build-essential gcc-arm-none-eabi

git clone https://github.com/Tails86/DreamcastControllerUsbPico.git
cd DreamcastControllerUsbPico
git checkout single-controller

git submodule update --recursive --init
```

## Loading the UF2 Binary

Hold the BOOTSEL button on the Pico while plugging the USB connection into your PC. A drive with a FAT partition labeled RPI-RP2 should pop up on your system. Open this drive, and then copy the desired uf2 file for either host or client operation here. The Pico should then automatically load the binary into flash and run it. For more information, refer to the official [Raspberry Pi Pico documentation](https://www.raspberrypi.com/documentation/microcontrollers/raspberry-pi-pico.html#documentation).

## Helpful Tips

### Host Mode Tips

- The LED on the Pico board may be used for quick status - when connected to USB, it should remain on when no button is pressed on any controller and turn off once a button is pressed.
- The included file `formatted_storage.bin` may be used to delete and format a VMU attached to a controller when this project is used in host mode. For example, rename this file vmu0.bin and copy to DC-Memory drive when a VMU is inserted into the upper slot of Player 1's controller.
- A serial device shows up on the PC once attached - open serial terminal (BAUD and other settings don't matter), type `h`, and then press enter to see available instructions.
