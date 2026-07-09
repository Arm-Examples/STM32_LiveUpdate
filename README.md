# STM32 Live Update Example

This repository demonstrates a live firmware update on STM32: the MCU keeps
running, switches from one complete firmware image to another image in the other
flash bank, and preserves selected RAM state across the switch.

This repository contains an example for [Keil Studio](https://www.keil.arm.com/)
that runs on the
[NUCLEO-L476RG](https://www.st.com/en/evaluation-tools/nucleo-l476rg.html)
board with an
[STM32L476RG](https://www.st.com/en/microcontrollers-microprocessors/stm32l476rg.html)
MCU. It uses
[Arm Compiler for Embedded](https://developer.arm.com/Tools%20and%20Software/Arm%20Compiler%20for%20Embedded)
and [CMSIS-Toolbox](https://open-cmsis-pack.github.io/cmsis-toolbox/)
projects. The same idea can be used on many STM32 devices, but the bank
mapping, flash layout, and remap register handling must be adapted to the exact
device family.

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

The application has two versions:

- `v0`: counts seconds and prints them on USART2.
- `v1`: adds minute counting and prints `MM:SS`.

Both versions are complete firmware images. Version 0 is programmed into flash
bank 1 at `0x08000000`. Version 1 is programmed into flash bank 2 at
`0x08080000`. While version 0 is running, pressing the user button calls the
switch code. If the image in the other bank is valid and has a higher version
number, execution moves to that image without a reset. The second counter is
kept alive across the switch.

## Core Idea

The live update works by keeping a few things stable between all firmware
versions:

1. Each image contains a version/signature block at a fixed location.
2. Each image contains identical switch code at a fixed location.
3. Interrupt vectors and selected interrupt handlers are copied to SRAM before
   the switch.
4. Variables that must survive the update are placed at fixed RAM addresses.
5. The switch code remaps the flash banks, points `VTOR` to the new vector table
   in SRAM, and jumps to the C runtime startup entry of the new image.

The jump target is the new image's `__main`, not its `main()`. This is
important: the C runtime for the new image still initializes the new image's
data before entering `main()`. Preserved variables are excluded from that normal
initialization path.

## Flash Layout

Both projects are linked as if they execute at `0x08000000`:

```text
0x08000000  active image address
0x08080000  inactive image address on STM32L476RG dual-bank flash
```

The debugger configuration for `v1` loads the binary at `0x08080000`:

```yaml
load-offset: 0x08080000
```

At run time the STM32 flash remap bit decides which physical bank appears at
`0x08000000`. The switch code toggles `SYSCFG->MEMRMP` bit
`SYSCFG_MEMRMP_FB_MODE` so the other physical bank becomes the active bank.

## Image Signature And Version

Each image places a small metadata block directly after the reset/vector region.
The block is emitted by `v0/SwitchCode.c`:

```c
__attribute__( ( used ) ) uint32_t * const Signature[ ] =
{
  ( void * )0xABABABAB,
  ...
  ( uint32_t * const )__main
};
```

The application version is stored in the `.version` section:

```c
__attribute__( ( section( ".version" ), used ) ) const uint32_t Version = VERSION;
```

The scatter file places the signature and version at a fixed address:

```text
ER_VERSION +0
{
  switchcode.o( .rodata.*, +FIRST )
  * ( .version )
}
```

`SwitchEntry()` reads the signature from the other bank. If the magic value is
present and the other version is newer, the switch is allowed.

## Fixed Switch Code

The switch code must be byte-for-byte compatible between firmware versions. In
this example, version 0 builds `SwitchCode.c`; version 1 links the object file
from version 0 instead of compiling a new copy:

```yaml
files:
  - file: ../v0/out/test_v0/Version_0/preserved.o
  - file: SwitchCode.o
  - file: preserved_v1.c
```

The scatter file keeps the switch code at a fixed location after the version
block:

```text
ER_SWITCHCODE +0
{
  switchcode.o( .text.* )
}
```

Keeping this code fixed matters because the running image must be able to find
and execute the same switch sequence regardless of which version is active.

## Interrupts During The Switch

The CPU cannot safely fetch interrupt vectors or handlers from flash while the
flash bank mapping is being changed. This example handles that by copying the
vector table and selected interrupt handler code to SRAM.

Version 0 uses:

```text
RW_VECTORS   0x10000000
RW_HANDLERS  0x10000400
```

Version 1 uses:

```text
RW_VECTORS_1   0x10004000
RW_HANDLERS_1  0x10004400
```

`HAL_IncTick()` is placed in a `.handler_code` section so the SysTick path can
keep running from SRAM:

```c
__attribute__( ( section( ".handler_code" ) ) ) void HAL_IncTick( void )
```

Before switching, `SwitchEntry()` copies the new image's handler code and vector
table into the SRAM area for that image, disables interrupts briefly, updates
`SCB->VTOR`, toggles the bank remap bit, re-enables interrupts, and jumps to the
new image's `__main`.

## Preserved RAM State

Live update is only useful if selected runtime state survives the image switch.
This example preserves the time counters and a switch flag.

Version 0 owns the first preserved block at the start of SRAM:

```text
RW_PRESERVED_V0 0x20000000 0x00000100
{
  preserved_v0.o (+ZI +RW)
}
```

`v0/preserved_v0.c` defines:

```c
uint32_t Switched;
volatile uint32_t Tick;
uint32_t LastMilliseconds, LongestMilliseconds;
volatile uint32_t Milliseconds, Seconds;
```

After building version 0, the linker-generated symdefs file is reduced to only
the variables that must remain fixed:

```text
0x20000000 D LastMilliseconds
0x20000004 D LongestMilliseconds
0x20000008 D Milliseconds
0x2000000c D Seconds
0x20000010 D Switched
0x20000014 D Tick
```

Version 1 links that symdefs file so those symbols keep the same addresses.
Then it reserves the old block and adds a new preserved block after it:

```text
RW_PRESERVED 0x20000000 EMPTY UNINIT 0x00000018
{
}

RW_PRESERVED_v1 +0
{
  preserved_v1.o (+ZI +RW)
}
```

Version 1 adds:

```c
volatile uint32_t Minutes = VERSION;
```

For the next version, the new symdefs file must include both the original
preserved variables and `Minutes`.

## Startup After A Live Switch

The `Switched` variable tells the new image whether it was entered by a live
switch or by a normal reset.

On a live switch, version 1 skips hardware setup that must not be repeated, such
as full clock initialization. It only refreshes the software-side state that is
needed after the jump, such as `SystemCoreClock`, USART2, and stdio.

On a normal reset, version 1 performs the full HAL, clock, GPIO, SysTick, and
USART initialization path and initializes any preserved state that does not
already contain valid live state.

## Repository Layout

```text
v0/
  test_v0.csolution.yml       CMSIS solution for version 0
  test_v0.cproject.yml        CMSIS project for version 0
  test_v0.sct                 Arm scatter file for version 0
  SwitchCode.c                fixed switch/signature code
  preserved_v0.c              version 0 preserved variables
  preserved.h                 preserved variable declarations

v1/
  test_v1.csolution.yml       CMSIS solution for version 1
  test_v1.cproject.yml        CMSIS project for version 1
  test_v1.sct                 Arm scatter file for version 1
  preserved_v1.c              new version 1 preserved variables
  preserved.h                 preserved variable declarations
```

Key files:

- [`v0/test_v0.csolution.yml`](v0/test_v0.csolution.yml): CMSIS solution for
  version 0.
- [`v0/test_v0.sct`](v0/test_v0.sct): flash, RAM, vector, handler, and preserved
  variable layout for version 0.
- [`v0/SwitchCode.c`](v0/SwitchCode.c): image signature and live switch entry
  code.
- [`v0/preserved_v0.c`](v0/preserved_v0.c): preserved variables introduced by
  version 0.
- [`v1/test_v1.csolution.yml`](v1/test_v1.csolution.yml): CMSIS solution for
  version 1, including the `0x08080000` load offset.
- [`v1/test_v1.sct`](v1/test_v1.sct): version 1 memory layout, including the
  reserved version 0 preserved-RAM block.
- [`v1/preserved_v1.c`](v1/preserved_v1.c): preserved variables added by
  version 1.

## Build And Run

1. Connect the NUCLEO-L476RG board.
2. Open the workspace or the `v0` CMSIS solution in Keil Studio / VS Code.
3. Build and flash `v0`. It is programmed to flash bank 1.
4. Build `v1`. It uses `SwitchCode.o` and the preserved-symbol file produced by
   the `v0` build.
5. Flash `v1`. Its solution loads the binary at `0x08080000`, flash bank 2.
6. Open the serial monitor for the ST-LINK virtual COM port at 115200 baud.
7. Reset the board. Version 0 prints seconds.
8. Press the user button. The board switches to version 1 without reset.
9. Version 1 prints minutes and seconds, with the seconds value preserved from
   version 0.

Expected serial behavior:

```text
now running version 0
it counts seconds ...
01
02
03

<press user button>

now running version 1
it counts minutes and seconds
00:04
00:05
```

## Porting Notes

To use this pattern on another STM32 device:

- Check whether the device has dual-bank flash, bank remapping, or another safe
  way to make the new image visible at the linked execution address.
- Adapt the flash bank addresses and the register sequence used in
  `SwitchEntry()`.
- Keep the image signature, version block, and switch code at fixed addresses in
  every image.
- Keep the switch code ABI stable. A new image must be able to call the old
  switch code correctly.
- Move any interrupt handlers that may run during the switch into RAM.
- Treat preserved RAM as an ABI. Never reorder or shrink old preserved
  variables; append new preserved variables after the old block.
- Rebuild and curate the symdefs file for each released version so the next
  version can preserve the same variables at the same addresses.

This is an example, not a complete production bootloader. A production updater
should also add image authentication, CRC/hash validation, rollback policy,
power-fail handling while programming flash, and a recovery path for invalid
images.
