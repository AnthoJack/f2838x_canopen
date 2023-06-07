# F2838x_canopen

The objective of this repository is to provide a CANopen stack to run on the TMS320f2838x Texas Instrument microcontroller

## Important folders and files

- **f2838x_canopen.c**: Main source file for the project. Contains the `main` function as well as the timer ISR function for CANopenNode realtime periodic interrupt-thread
- **CANopenNode/**: CANopenNode git submodule. Ideally, files in this folder should not be modified
- **canOpenC2000/**: Folder for files required by CANopenNode but that can be modified
    - **CO_driver_target.h**: Driver structures definition and configuration defaults override. Created based on the one from *CANopenNode/examples/* as some structure members are actually required from outside the driver implementation, despite CANopenNode claiming to be "object-oriented" (which would imply better encapsulation)
    - **CO_driver.c**: Implementation of the driver functions
    - **CO_storage.c/h**: Functions necessary for CANopenNode to store the Object Dictionnary to flash memory. Not implemented. Copied from *CANopenNode/examples*
    - **OD.c/h**: Object Dictionnary definitions and implementation
- **2838x_X_lnk_cm.cmd**: Linker scripts to define the memory attribution for the different code sections
- **device/**: Driverlib files to initialise the CM core
- **device/driverlib_cm**: Driverlib files with functions to manage the different devices of the uC through the CM core

## Microcontroller

The F2838x uC is part of the C2000 microcontroller family. It posesses 2 C28x (proprietary cores from TI) and a Cortex-M0 core

### CM (Cortex-M0) Core

The Communication Manager core is an ARM Cortex-M0 core and has access to the CAN module. After a reset, this core is disabled and it has to be activated by CPU0. From CCS however, it can be activated by the debugger by selecting it in the list of core and launching the program

### CAN module

The CAN module of the f2838x uC is based on the DCAN IP. This IP is based on a RAM memory that stores up to 32 *Message Objects* containing informations on CAN frames that should be received or transmitted like the identifier, DLC, data and mask (to filter the received frames). Message objects can be configured so an interrupt is sent once a CAN frame is received or transmitted.

Message objects aren't accesible directly and have to be accessed via one of the memory mapped interface registers provided by the device. Configuration, transmission and data reception can be done using the driverlib functions but updating DLCs and identifiers has no attributed driverlib function and has to be done by using the interface registers manually

The CAN module may also be configured to send interrupts when an error is detected or when its status changes

## CANopenNode

CANopenNode is a CANopen open-source stack. It handles the features of the CiA301 CANopen protocol as well as some extensions. Effectively used features are defined at compile time using defines from the configuration files (see below)

### CAN module driver API

CANopenNode is written to be generic and provides a *CO_driver.h* file with function prototypes that need to be implemented for it to communicate with the CAN module. Functions can be implemented in a *CO_driver.c* file and a *CO_driver_target.h* file can be used to declare structures internal to the driver. Both files are examplified in the *examples/* folder and it is a good idea to use those as reference as the APi is sadly not as encapsulated as one might expect: some fields of the declared structures are indeed only used in the driver but others are used in the main code and are thus mandatory. 

The internal code of CANopenNode relies on *OD objects* for each CANopen feature (NMT, EMERGENCY, SDO, PDO...). Those objects then declare transmission (*tx*) and reception (*rx*) objects depending on their needs to send and receive CAN frames. the amount of transmission and reception object is given to the driver as well as 2 arrays to store them at init time: one for *rx* objects and one for *tx* objects. As long as the amount of objects isn't superior to 32, the CAN module's message objects can be used to handle the objects and the data they send/receive. Both types of objects are initialised by calling the corresponding CAN driver functions `CO_CANrxBufferInit` or `CO_CANtxBufferInit`. The decision was made to assign all the reception (rx) objects (`CANrxBuffer_t`) before the transmission (tx) objects  (`CANtxBuffer_t`) in the message objects of the CAN module as it allows for fast matching of *tx/rx object* \<-> *message object*. Of course this assumes the number of OD objects won't change after initialisation

When an interrupt is generated because a message object has received a frame or because it has sent its frame, the index of the message object that generated the IRQ can be retrieved. With the previously explained attribution of the OD objects to the message objects, it is then simple to deduce if the corresponding OD object is *tx* (number > #*rx object*) or *rx* (and thus which object array it is found in) and the index of the OD object in its list (*rx object* -> array index = message object index, *tx object* -> array index = message object index - #*rx objects*)

The `CANtxBuffer_t` structure must contain a `data` field. This field is used by the CANopenNode code to transfer data to a *tx* object prior to calling the `CO_CANsend` function which signals the driver that the data must be sent. Once `CO_CANsend` is called, the corresponding message object is accessed to check if it is currently already trying to transmit another frame. If such is the case, the function returns an error. If not, the data and other frame informations are sent to the corresponding message object, transmission is requested and the `bufferFull` field of the object's structure is set to `true` to signal that the message object is occupied. Once the transmission interrupt for this object is generated, the flag is set to `false` again

`CANrxBuffer_t` objects contain a callback function that is given at initialisation along with a pointer to the OD object. When a CAN frame is received and is attributed to the object, the callback function must be called and given the object pointer as well as a pointer to the message. This message pointer can be of any type but the callback function must be able to retrieve the identifier, DLC and data of the message. To this end, it uses the `CO_CANrxMsg_readIdent(msg)`, `CO_CANrxMsg_readDLC(msg)` and `CO_CANrxMsg_readData(msg)` macros which must be defined in the *CO_driver_target.h* file

For a bit to be recovered from the CAN interface, its state is assessed for a certain number of ticks from the CAN module clock, referred to as *time quantas*. During this time, some operations ensuring the reliability of the communication are performed on the CAN line. The amount of time quantas necessary for one bit is called "bit time" and is configurable when configuring the bitrate of the CAN module. The `CAN_setBitRate` function is provided by the driverlib and requires the last argument to be a bit time between 8 and 25. This value must be chosen so that "*The resulting Bit time (1/Bit rate) \[is an] integer multiple of the CAN clock period*" ([F2838x Technical Reference manual](https://www.ti.com/lit/pdf/spruii0), Section 30.12.2.1, Page 3355). The term "resulting bit time" here seems to be refering to the period of one *time quanta* for the CAN module (To be verified). If such is the case, then the frequency of the CAN module must be an integer multiple of the product of the bitrate and the bit time:

$$
q_p = a * clk_p \leftrightarrow
\frac{1}{q_f} = \frac{a}{clk_f} \leftrightarrow
qf = \frac{clk_f}{a} \leftrightarrow
a * q_f = clk_f, a \in \natnums* \\
$$
$$
q_{p/f}:\ period/frequency\ of\ time\ quanta \\
clk_{p/f}:\ period/frequency\ of\ CAN\ module\ clock \\
$$

This property can be checked with:

$$
clk_f\ mod\ q_f = 0 \\
$$
$$
q_f = bitrate * bit\_time
$$

Note:

* No specific configuration of the clock tree was made so the CAN module clock frequency should be equal to the CM clock frequency (125MHz)
* CANopenNode only supports the following bitrates (in bps): 10K, 20K, 50K, 125K, 250K, 500K, 800K, 1M

The *CanBitTimeVerif.xlsx* spreadsheet was created to rapidly check which bit times supported by the driverlib are useable for the bitrates supported by CANopenNode. As shown inside it, the available options are not legion and it could be interesting to use the clocktree to provide the CAN module with a frequency that results in more adequate options. The calculations are based on the CAN clock frequency which can be modified in the spreadsheet

Filtering the received data frames is done by the CAN module according to the following rule: `(receivedIdent xor msgObjIdent) and msgObjMask = 0`. This means that certain bits of the received frame's identifier can differ from the message object's identifier if the mask marks them as "don't care" (0)

### Stack usage

The stack relies on threaded functions to manage timed (like OD management) or asynchronous (like CAN reception) tasks. In this context, those are handled using interrupts. One such interrupt is the CAN interrupt mentionned in the driver section. It is called when the CAN module raises an IRQ and performs adequate processing depending on the cause. The second interrupt is raised by a timer every millisecond and handles the processing of the R/TPDOs

### OD

CANopenEditor is an Object Dictionnary editor capable of generating source code for CANopenNode to use to define the OD structures. The generated file also configures the amount of each CANopenNode OD object (NMT, PDO,...). This allows users to create custom ODs depending on the CANopen profile they would like to use for their project. However, generating a file with this utility doesn't seem to be foolproof as trying to compile the project using a custom one lead to build errors so some tinkering might be necessary

### Configuration

Core features of the CANopen protocol (defined in the CiA 301 document and implemented in the *CANopenNode/301* folder) are configured in the *canOpenC2000/OD.h* file. A basic *OD.c/h* file has been recovered from *CANopenNode/examples*. Other CANopen feature extensions (defined in the CiA 30X documents) have been implemented in the corresponding *CANopenNode/30X* folders and can be used by the stack if the configuration says so

Currently disabled extensions of CANopen (Default configuration overrides are made in the *CO_driver_target.h* file):

* LSS (CiA 305)
    * Allows the CANopen nodes in a network to be configured by other nodes. For testing and in simple networks, it might be simpler to leave this one out
    * Disabled by setting `#define CO_CONFIG_LSS 0`
* LED (CiA 303)
    * Allows for a LED to be controlled through CANopen
    * Disabled by setting `#define CO_CONFIG_LEDS 0`

## CCS

### C2000Ware and driverlib

C2000Ware is a collection of example projects, documentation and driver code (driverlib) for the C2000

Driverlib is driver code to be used to access, manage and configure the chip's devices. For this uC, since two different kinds of core exist, make sure to always use the examples and driverlib for the CM core

#### Install

In CCS, the C2000Ware package can be downloaded in the Resource Explorer. Go into *View* > *Resource* Explorer and then, on the left of the opened window, open the *C2000 real-time microcontrollers* > *Embedded Software* folder. Hovering over *C2000Ware-{version number}* will make three dots appear; click on it and select install to install the package

### Project

Repository contains all the files necessary to open it as a project in CCS. However the paths in the configuration might not correspond to the ones on your drive. Right-click on the project name and select *Properties* to adapt it to your hierarchy

#### Program sections attribution

The *2838x_RAM_lnk_cm.cmd* file declares RAM blocks and flash banks available and assigns the program sections to them. If some section takes too much space, it will be split and put on separate blocks. Assigning multiple possible blocks for a section is done by listing them separated by "|". In the *View* > *Memory Allocation* view, the different banks and blocks are listed and the associated drop-down menu shows the sections that were allocated on it during the last build. Hovering over the blocks shows the amount of used and available memory and the sections show the amount of "units" (bytes apparently) they occupy

![CM core memory blocks diagram](docImages/CM_memory.png)
CM core memory blocks diagram

#### Heap/stack memory allocation

The heap (.sysmem) and stack (.stack) sections' size isn't deduced but set in the linker options. In the project's properties, go to *CCS Build* > *Arm Linker* > *Basic Options* and set the desired value for the stack and heap in the penultimate and ante-penultimate fields respectively. In this project, a heap size increase was necessary to provide enough memory for all the OD objects allocated by CANopenNode

#### Project creation

When creating a new driverlib project, it is a good idea to start from an empty driverlib project such as the one available in the C2000Ware resource explorer example folder. In the Resource Explorer, go to the C2000Ware folder and then look for *English* > *Devices* > *F2838XD* > *F28388D* > *Examples* > *Driverlib Empty CM Example CCS Project*. Click on the three dots that appear next to it when hovering over it and select *Import to CCS IDE*

Once the import is finished, add the necessary folders containing the code that you want to compile. In this case, the folder containing the CANopenNode code, the folder containing the CANopenNode driver implementation and the code with the `main` function (See hierarchy description above)

Go to the Project Explorer, locate the *driverlib_cm.lib* file and remove it. In the project properties go to *C/C++ General* > *Paths and Symbols* and go to the *Source Location* tab. Click on *Add Folder ...* and add the *device/driverlib_cm* folder from the project's hierarchy. In the *Includes* tab, make sure to include the folders you have imported into the hierarchy

CANopenNode also contains C99 constructs (such as for-loop variable declaration) that won't play nicely with the default C89 option of the compiler. In the project properties, go into *CCS Build* > *Arm Compiler* > *Advanced Options* > *Language Options* and in the field in front of *C Dialect*, select *Compile program in C99 mode*

When compiling CANopenNode, make sure the "examples" folder is excluded from the build as the code it contains might cause errors since it isn't meant to actually be used. Right-click on the folder and choose *Exclude from Build*

## Current problems and limitations

The stack requires critical section protection to guarantee some data transfers are made safely between the User, stack and CAN module but I wasn't able to find how to implement them on the uC. It could also be that because the stack uses interrupts instead of actual threads, critical section isn't desireable since blocking an ISR is out of the question

Current driver only allows tx objects to send one frame to their dedicated message object. If new data is sent to the message object before the previous is sent, an error occurs and the new data is not memorised by the driver

Support for syncPDOs in the driver is not guaranteed to function as is (see comments above related driver function)

Current program stops working when initialising the CAN module RAM. Once the request is made to initialise it, the program enters a loop supposed to wait for the RAM initialisation to be finished by reading a register value but the loop is never left