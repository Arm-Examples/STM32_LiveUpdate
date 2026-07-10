# STM32_LiveUpdate
This repository demonstrates a live firmware update. The MCU keeps running, switches from one complete firmware image to another image in the other flash bank, and preserves selected RAM state across the switch.

To demonstrate the procedure, this repository contains an example for [Keil Studio](https://www.keil.arm.com/)
that runs on the [NUCLEO-L476RG](https://www.st.com/en/evaluation-tools/nucleo-l476rg.html) board with an [STM32L476RG](https://www.st.com/en/microcontrollers-microprocessors/stm32l476rg.html) MCU. It uses [Arm Compiler for Embedded](https://developer.arm.com/Tools%20and%20Software/Arm%20Compiler%20for%20Embedded) and [CMSIS-Toolbox](https://open-cmsis-pack.github.io/cmsis-toolbox/) projects.
The same idea can be used on many devices, but the bank mapping, flash layout, and remap register handling must be adapted to the exact device family.

## Quick Start

1. Install
   [Keil Studio for VS Code](https://marketplace.visualstudio.com/items?itemName=Arm.keil-studio-pack)
   from the VS Code marketplace.
2. Clone this repository, for example using
   [Git in VS Code](https://code.visualstudio.com/docs/sourcecontrol/intro-to-git),
   or download the ZIP file. Then open the repository root folder in VS Code.
3. Connect the NUCLEO-L476RG board to the PC.
4. Open the
   [CMSIS View](https://mdk-packs.github.io/vscode-cmsis-csolution/) in VS Code
   and select
   [`v0/test_v0.csolution.yml`](v0/test_v0.csolution.yml) as the active
   solution.
5. Let the related tools and software packs download and install. Review
   progress with **View > Output > CMSIS Solution**.
6. Build and load version 0 with the CMSIS View
   [action buttons](https://mdk-packs.github.io/vscode-cmsis-csolution/build/).
   This programs the first image at `0x08000000`.
7. Select [`v1/test_v1.csolution.yml`](v1/test_v1.csolution.yml) as the active
   solution.
8. Build and load version 1. This solution loads the second image at
   `0x08080000`.
9. Open the ST-LINK virtual COM port at 115200 baud, reset the board, and press
   the user button to switch from version 0 to version 1 without reset.

## What This Example Shows

The application has two versions of a firmware to demonstrate the switch mechanism:

- `v0`: counts seconds and prints them on USART2.
- `v1`: adds minute counting and prints `MM:SS`.

Both versions are complete firmware images. Version 0 is programmed into flash bank 1 at `0x08000000`. Version 1 is programmed into flash bank 2 at `0x08080000`. While version 0 is running, pressing the user button calls the switch code. If the image in the other bank is valid and has a higher version number, execution moves to that image without a reset. The second counter is kept alive across the switch.

The example also copies exception vector handlers to RAM for faster execution. However, this is not the focus of the switch mechanism example. So it is not covered in the documentation of the example.

## Making the initial version updateable


### Version Number in Flash

To determine if a flash bank has a newer version of the firmware, a version info in flash at a fixed address is required.

In the example a 32bit variable is put in the .version section. This section gets located after the vector table (behind the `ER_RESET`). Together with a signature from the switch code, this is the way to identify a valid firmware.
```
  ER_VERSION +0
  {
    switchcode.o( .rodata.*, +FIRST )
    * ( .version )
  }
```

### Switch Code

Implement the switch code, which must be the same in each version, and locate it at a fixed address. This code will actually jump to the library startup label __main of the new version after the switch. With this, the Stack Pointer is setup and the not preserved variables get initialized.
The switch code also contains the `NMI_Handler()`, which, depending on its trigger sources, could occur at any time.

In the example, this is the SwitchCode.c module. It uses a table with constant data, which starts with the signature. There is also the entry point to the application, normally it is the `__main` label. The `SwitchEntry()` function does the switch if it finds a newer version in the other flash bank.

The table with the signature was already placed with the version information. And the code is placed with the scatter file behind the `ER_VERSION`, which has a fixed size. So, the location for the SwitchCode is also fix:
```
  ER_SWITCHCODE +0   {  ; load address = execution address
    switchcode.o( .text.* )
  }
```

### Preserved Variables

When thinking about future versions, what variables of this version should be preserved during a live update? These variables should be collected in a block that gets located at some location that can still be expanded for future preserved variables.

In this example, these variables are in the preserved_v0.c module and located with the scatter file at the beginning of the internal RAM:
```
  RW_PRESERVED_V0 0x20000000 0x00000100  {
    preserved_v0.o (+ZI +RW)
  }
```
To make the preserved variables available in the application, add extern definitions in a header file and include this where required.


### Switch Variable

As the new version will run from the `__main` label, the main application may not need to initialize certain things again, like the clock and the gpio pins. For this, one preserved variable is set before the switch and is then used to make this decision. This variable also needs to be manually initialized to 0 in the reset handler, in case an updated version runs after power-on.

In the example, the variable is called “Switched”.


### Symdefs file

To make it possible to use preserved variables at the same location in a future version, the linker can create a symdefs file. After building, the file needs to be edited to contain only the preserved variables.

The example uses the linker option:

```
--symdefs=preserved.o
```
and contains these variables after removing all not-preserved data:
```
0x20000000 D LastMilliseconds
0x20000004 D LongestMilliseconds
0x20000008 D Milliseconds
0x2000000c D Seconds
0x20000010 D Switched
0x20000014 D Tick
```

### Doing the Switch

When a new firmware version has been programmed to the other bank, switching is done by setting the Switch variable to 1 and calling the `SwitchEntry()` function.

The example simulates the by wating for the pressing of the user button on the board.

## Prepare a future version to be switched to


### Modify the application

As required, add new functionallity and/or fix bugs.


### Add/Remove files related to the switching

The switch code shall be constant. So, remove its source from the project and include the object file from the initial version build instead. Add also the symdefs file from the previous build to the project. This makes the preserved variables available at the same locations. And replace the module for the new preserved variables with a new one. 

- Replace SwitchCode.c with SwitchCode.o from the initial version build
- Add preserved.o (symdefs file from previous version) to the project
- Replace preserved_v0.c with preserved_v1.c


### Previously preserved variables

To modify the scatter file, check the actual size of the preserved variables block in the mapfile of the previous version and rename/change the related region, so that it covers the variables in the symdefs file.

For the example, there is in the mapfile:
```
Execution Region RW_PRESERVED_V0 (Exec base: 0x20000000, Load base: 0x08002fc8, Size: 0x00000018, Max: 0x00000100, ABSOLUTE)
```
In the new scatter file, RW_PRESERVED_V0 becomes:
```
  RW_PRESERVED 0x20000000 EMPTY UNINIT 0x00000018  {
  }
```

### Add new preserved variables

For the new preserved variables, add a new region to the scatter file and put the variables again in a common block. And this new block should be located right after the previously preserved variables region, so that the block covering the symdefs area can be extended in the next version. The new variables also need an extern definition in the header file.

For the example, the following is added right after RW_PRESERVED:
```
  RW_PRESERVED_V1 +0    {
    preserved_v1.o (+ZI +RW)
  }
```
The new version also counts minutes. So, the declaration of this is now in preserved_v1.c. And an extern definition is added to preserved.h.


### Handling of the Switch variable

In case the new version starts up after a switch, the variable is 1, and the software can skip initialization of all preserved resources, which are also hardware peripherals. Most notably, this is the clock setup.
But in case of booting after a power-on reset, where the Switch variable is 0, all this initialization needs to be done. In addition to that, all preserved variables from the previous version have to be manually initialized.


### Update symdefs file

Every next version should preserve the variables that the previous version had already preserved and that this version added to the preserved variables. For that, the new version needs all preserved variables in the symdefs file. To do that, delete the symdefs file in the current version output folder, build new and edit it to contain just the preserved variables. The next version then builds with this file.

For the example, preserved.o in the current version output folder was deleted. After a rebuild and edit, it contains:
```
0x20000000 D LastMilliseconds
0x20000004 D LongestMilliseconds
0x20000008 D Milliseconds
0x2000000c D Seconds
0x20000010 D Switched
0x20000014 D Tick
0x20000018 D Minutes
```
