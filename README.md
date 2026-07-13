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

Both versions are complete firmware images. Version 0 is programmed into flash bank 0 at `0x08000000`. Version 1 is programmed into flash bank 1 at `0x08080000`. While version 0 is running, pressing the user button calls the switch code. If the image in the other bank is valid and has a higher version number, execution moves to that image without a reset. The second counter is kept alive across the switch.

The example also copies exception vector handlers to RAM for faster execution. However, this is not the focus of the switch mechanism example. So it is not covered in the documentation of the example.

## Core Idea

The live update works by keeping a few things stable between all firmware versions:

1. Each image contains a version/signature block at a fixed location.
2. Each image contains identical switch code at a fixed location.
3. Variables that must survive the update are placed at fixed RAM addresses.
4. The switch code remaps the flash banks, points `VTOR` to the new vector table
   in SRAM, and jumps to the C runtime startup entry of the new image.

The jump target is the new image's `__main`, not its `main()`. This is important: the C runtime for the new image still initializes the new image's
data before entering `main()`. Preserved variables are excluded from that normal initialization path.


## Making the initial version updateable


### Version Number in Flash

To determine whether a flash bank contains a newer firmware version, the flash bank must store version information at a fixed address.

In this example, a 32-bit variable is stored in the .version section. The linker places this section after the vector table, following `ER_RESET`. Software uses the version information together with the switch code signature to verify that the firmware is valid.
```
  ER_VERSION +0
  {
    switchcode.o( .rodata.*, +FIRST )
    * ( .version )
  }
```

### Switch Code

Implement the switch code so that it is identical in every firmware version, and place it at a fixed address. After the firmware switch, the code branches to the startup label `__main` in the new firmware version. The startup code sets up the stack pointer and initializes the variables that are not preserved across the switch.

The switch code also includes `NMI_Handler()`, because an NMI can occur at any time, depending on its trigger source.

In this example, the SwitchCode.c module contains a table of constant data. The first entry in the table is the firmware signature. The table also contains the application entry point, which is typically the `__main` label. The `SwitchEntry()` function switches to the firmware in the other flash bank if it detects a newer firmware version.

The signature table is already placed with the version information. The scatter file places the code from `SwitchCode.c` immediately after `ER_VERSION`, which has a fixed size. As a result, the switch code is always located at a fixed address:
```
  ER_SWITCHCODE +0   {  ; load address = execution address
    switchcode.o( .text.* )
  }
```

### Preserved Variables

When thinking about future firmware versions, identify the variables that must be preserved during a live update. Group these variables in a dedicated memory block and place the block in a reserved location with enough space for additional preserved variables in later versions.

In this example, the preserved variables are defined in `preserved_v0.c`. The scatter file places them at the beginning of the internal RAM.
```
  RW_PRESERVED_V0 0x20000000 0x00000100  {
    preserved_v0.o (+ZI +RW)
  }
```
To make the preserved variables available to the application, add extern declarations to a header file and include the header file where required.


### Switch Variable

Because the new firmware starts at the `__main` label, the application does not need to reinitialize components such as the clock and GPIO pins after a live update. Before the switch, set a preserved variable that indicates whether this initialization should be skipped. Initialize this variable to 0 in the reset handler so that a normal boot after power-on performs the required initialization.

In this example, the preserved variable is named `Switched`.


### Symdefs file

To use preserved variables at the same addresses in future firmware versions, generate a symdefs file with the linker. After the build completes, edit the file to contain only the preserved variable symbols.

The example uses the following linker option to generate the symdefs file:

```
--symdefs=preserved.o
```
After removing all symbols except the preserved variable symbols, the file contains:
```
0x20000000 D LastMilliseconds
0x20000004 D LongestMilliseconds
0x20000008 D Milliseconds
0x2000000c D Seconds
0x20000010 D Switched
0x20000014 D Tick
```

### Doing the Switch

After programming a new firmware version to the other flash bank, set the Switch variable to 1 and call the `SwitchEntry()` function to perform the switch.

The example simulates this by waiting for the user to press the User button on the board.

## Prepare a future version to be switched to


### Modify the application

Update the firmware by adding new functionality or fixing bugs, as required.


### Add/Remove files related to the switching

Keep the switch code unchanged. Remove its source file from the project and include the object file from the initial firmware build instead. Add the symdefs file from the previous build to the project to use the preserved variables at the same addresses. Replace the previous preserved-variable module with the module for the new firmware version.

- Replace `SwitchCode.c` with `SwitchCode.o` from the initial version build
- Add `preserved.o` (symdefs file from previous version) to the project
- Replace `preserved_v0.c` with `preserved_v1.c`


### Previously preserved variables

Check the size of the preserved-variable block in the previous version's map file. Update the corresponding region in the scatter file by renaming it, if required, and adjusting its size so that it covers all preserved variables defined in the symdefs file.

For this example, the map file contains:
```
Execution Region RW_PRESERVED_V0 (Exec base: 0x20000000, Load base: 0x08002fc8, Size: 0x00000018, Max: 0x00000100, ABSOLUTE)
```
In the new scatter file, `RW_PRESERVED_V0` becomes the following:
```
  RW_PRESERVED 0x20000000 EMPTY UNINIT 0x00000018  {
  }
```

### Add new preserved variables

For the new preserved variables, add a new region to the scatter file and group the variables in a common block. Place the new block immediately after the region that contains the previously preserved variables. This layout allows the region defined by the symdefs file to expand in future firmware versions. Add extern declarations for the new variables to the header file.

In this example, the following region is added immediately after `RW_PRESERVED`:
```
  RW_PRESERVED_V1 +0    {
    preserved_v1.o (+ZI +RW)
  }
```
The new firmware version also counts minutes. The minute counter is declared in `preserved_v1.c`, and `preserved.h` contains the corresponding extern declaration.


### Handling of the Switch variable

If the new firmware starts after a live update, the Switched variable is set to 1. The software skips initialization of preserved resources, including hardware peripherals such as the clock.

If the firmware starts after a power-on reset, the Switched variable is 0. The software initializes all preserved resources and manually initializes the preserved variables from the previous firmware version.

### Update symdefs file

Each new firmware version must preserve the variables from the previous firmware version as well as any new preserved variables that it introduces. To achieve this, the symdefs file must contain all preserved variable symbols.

Delete the existing symdefs file from the current build output directory, rebuild the project, and edit the generated file so that it contains only the preserved variable symbols. Use this updated symdefs file when you build the next firmware version.

In this example, preserved.o was deleted from the current build output directory. After rebuilding the project, the regenerated object file contains the following symbols:
```
0x20000000 D LastMilliseconds
0x20000004 D LongestMilliseconds
0x20000008 D Milliseconds
0x2000000c D Seconds
0x20000010 D Switched
0x20000014 D Tick
0x20000018 D Minutes
```
