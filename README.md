# STM32_LiveUpdate
Example for a firmware update process without resetting the system switching flash banks and fully compatible images

This example is using the Arm Compiler for Embedded and runs on the NUCLEO-L476RG. On the board is the STM32L476RG MCU. This device has swapable flash banks, which are utilized for seamless firmware version switching.
In addition to switching firmware versions, the example also copies interrupt vector handlers in RAM, which is not discussed here.

The example counts seconds with the help of the SysTick interrupt and outputs this via USART.



## Prepare version 0 to be updated somewhen


### Version Number in Flash

If booting the bank with latest version can’t be fixed somehow, a version info in flash at a fixed address helps to boot the latest version by doing a forced switch on demand.

In the example a 32bit variable is put in the .version section which is together with a signature to help identify valid firmware, located after the vector table (behind the `ER_RESET`).
```
  ER_VERSION +0
  {
    switchcode.o( .rodata.*, +FIRST )
    * ( .version )
  }
```

### Switch Code

Implement the switch code, which must be the same in each version and locate it at a fixed address. This code will actually jump to the library startup label __main of the new version after the switch. With this, the Stack Pointer is setup and the not (yet) preserved variables in future version get initialized.
The switch code also contains the `NMI_Handler()`, which depending on its trigger sources, could occur any time.
In the example, this is the SwitchCode.c module. It uses a table with constant data. This is the information where to find information about the handler code to be copied. Also there is he entry point to the application, normally its `__main` label. The `SwitchEntry()` function does the switch, if it finds a newer version in the other flash bank.

The table and the code are placed with the scatter file behind the `ER_VERSION`, which has a fixed size. So, the location for the SwitchCode is also fix:
```
  ER_SWITCHCODE +0   {  ; load address = execution address
    switchcode.o( .text.* )
  }
```

### Preserved Variables

When thinking about future versions, what variables of this version should be preserved during a live update? That variables should be collected in a block, that gets located at some location, that can still be expanded for future preserved variables.

In this example, that variables are in the preserved_v0.c module and located with the scatter file at the beginning of the internal RAM:
```
  RW_PRESERVED_V0 0x20000000 0x00000100  {
    preserved_v0.o (+ZI +RW)
  }
```
To make the preserved variables available in the application, add extern definitions in a header file and include this where required.


### Switch Variable

As the new version will run from the `__main` label, the main application may not need to initialize certain things again, like clock and gpio pins. For this, one preserved variable is set before the switch and is then used to make this decision. This variable also needs to be manually initialized to 0 in the reset handler, in case an updated version runs after power-on. 

In the example the variable is called “Switched”.


### Symdefs file

To make it possible to use preserved variables at the same location, the linker can create a symdefs file. After building, the file needs to be edited, to contain only the preserved variables.

In the example used linker option:

```
--symdefs=preserved.o
```
and contains that variables after removing all not preserved data:
```
0x20000000 D LastMilliseconds
0x20000004 D LongestMilliseconds
0x20000008 D Milliseconds
0x2000000c D Seconds
0x20000010 D Switched
0x20000014 D Tick
```

### Doing the Switch

When a new firmware version was programmed to the other bank, switching is done by setting the Switch variable to 1 and call the `SwitchEntry()` function.



## Prepare version 1 to be switched to


### Modify the application

As required, add new functionallity and/or fix bugs.


### Add/Remove files related to the switching

The switch code shall be constant. So, remove its source from the project and include the object file from the version 0 build instead. Add also the symdefs file from the previous build to the project. This makes the preserved variables available at the same locations. And replace the module for the new preserved variables with a new one. 

- Replace SwitchCode.c with SwitchCode.o from version 0 build
- Add preserved.o (symdefs file from previous version) to the project
- Replace preserved_v0.c with preserved_v1.c


### Previously preserve variables

To modify the scatter file, check the actual size of the preserved variables block in the mapfile of the previous version and rename/change the related region, so that it covers the variables in the symdefs file.

For the example, there is in the mapfile:
```
Execution Region RW_PRESERVED_V0 (Exec base: 0x20000000, Load base: 0x08002fc8, Size: 0x00000018, Max: 0x00000100, ABSOLUTE)
```
In the new scatter file RW_PRESERVED_V0 becomes:
```
  RW_PRESERVED 0x20000000 EMPTY UNINIT 0x00000018  {
  }
```

### Add new preserved variables

For the new preserved variables, add a new region to the scatter file and put the variables again in a common block. And this new block should be located right after the previously preserved variables region, so that the block covering the symdefs area can be extended in the next version. The new variable also needs an extern definition in the header file.

For the example, the following is added right after RW_PRESERVED:
```
  RW_PRESERVED_V1 +0    {
    preserved_v1.o (+ZI +RW)
  }
```
The new version also counts minutes. So, the declaration of this is now in preserved_v1.c and an extern definition is added to preserved.h.


### Handling of the Switch variable

In case the new version starts up after a switch, the variable is 1 and the software can skip initialization of all preserved resources, which are also hardware peripherals. Most notably, this is clock setup.
But in case of booting after a power-on reset, where Switch variable is 0, all this initialization needs to be done. In addition to that, all from the previous versions preserved variables have to be manually initialized.


### Update symdefs file

Every next version should preserve the variables, that the previous version did already preserve and that this version added to the preserved variables. For that, the new version needs all preserved variables in the symdefs file. To do that, delete the symdefs file in the current version output folder, build new and edit it to contain just the preserved variables. The next version then build with this file.

For the example, preserved.o in the current version output folder was deleted. After rebuild and edit it contains:
```
0x20000000 D LastMilliseconds
0x20000004 D LongestMilliseconds
0x20000008 D Milliseconds
0x2000000c D Seconds
0x20000010 D Switched
0x20000014 D Tick
0x20000018 D Minutes
```


## Testing the example

- connect the NUCLEO-L476RG to the PC
- open the project in the v0 folder in Keil Studio
- build and then flash the project, this is written to flash bank0
- Open the project in the v1 folder in Keil Studio
- build and then flash the project, this is written to flash bank1
- in VSCode open the Serial Monitor and connect it to the STLink USART
- reset the board an you should see seconds printed out
- press the User Buuton on the board, this will trigger the switch
- v1 prints minutes as well while seconds value was preserved