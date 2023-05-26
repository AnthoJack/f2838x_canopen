MEMORY
{
   /* Flash sectors */
   CMBANK0_RESETISR : origin = 0x00200000, length = 0x00000008 /* Boot to Flash Entry Point */
   CMBANK0_SECTOR0  : origin = 0x00200008, length = 0x00003FF8
   CMBANK0_SECTOR1  : origin = 0x00204000, length = 0x00004000
   CMBANK0_SECTOR2  : origin = 0x00208000, length = 0x00004000
   CMBANK0_SECTOR3  : origin = 0x0020C000, length = 0x00004000
   CMBANK0_SECTOR4  : origin = 0x00210000, length = 0x00010000
   CMBANK0_SECTOR5  : origin = 0x00220000, length = 0x00010000
   CMBANK0_SECTOR6  : origin = 0x00230000, length = 0x00010000
   CMBANK0_SECTOR7  : origin = 0x00240000, length = 0x00010000
   CMBANK0_SECTOR8  : origin = 0x00250000, length = 0x00010000
   CMBANK0_SECTOR9  : origin = 0x00260000, length = 0x00010000
   CMBANK0_SECTOR10 : origin = 0x00270000, length = 0x00004000
   CMBANK0_SECTOR11 : origin = 0x00274000, length = 0x00004000
   CMBANK0_SECTOR12 : origin = 0x00278000, length = 0x00004000
   CMBANK0_SECTOR13 : origin = 0x0027C000, length = 0x00004000

   C1RAM            : origin = 0x1FFFC000, length = 0x00002000
   C0RAM            : origin = 0x1FFFE000, length = 0x00002000

   BOOT_RSVD        : origin = 0x20000000, length = 0x00000800 /* Part of S0, BOOT rom will use this for stack */
   S0RAM            : origin = 0x20000800, length = 0x00003800
   S1RAM            : origin = 0x20004000, length = 0x00004000
   S2RAM            : origin = 0x20008000, length = 0x00004000
   S3RAM            : origin = 0x2000C000, length = 0x00004000
   E0RAM            : origin = 0x20010000, length = 0x00004000

   CPU1TOCMMSGRAM0  : origin = 0x20080000, length = 0x00000800
   CPU1TOCMMSGRAM1  : origin = 0x20080800, length = 0x00000800
   CMTOCPU1MSGRAM0  : origin = 0x20082000, length = 0x00000800
   CMTOCPU1MSGRAM1  : origin = 0x20082800, length = 0x00000800
   CPU2TOCMMSGRAM0  : origin = 0x20084000, length = 0x00000800
   CPU2TOCMMSGRAM1  : origin = 0x20084800, length = 0x00000800
   CMTOCPU2MSGRAM0  : origin = 0x20086000, length = 0x00000800
   CMTOCPU2MSGRAM1  : origin = 0x20086800, length = 0x00000800
}

SECTIONS
{
   .resetisr        : > CMBANK0_RESETISR, ALIGN(16)
   .vftable         : > CMBANK0_SECTOR0, ALIGN(16)   /* Application placed vector table in Flash*/
   .vtable          : > S0RAM             /* Application placed vector table in RAM*/
   .text            : >> CMBANK0_SECTOR0 | CMBANK0_SECTOR1, ALIGN(16)
   .cinit           : > CMBANK0_SECTOR0, ALIGN(16)
   .pinit           : >> CMBANK0_SECTOR0 | CMBANK0_SECTOR1, ALIGN(16)
   .switch          : >> CMBANK0_SECTOR0 | CMBANK0_SECTOR1, ALIGN(16)
   .sysmem          : > S2RAM

   .stack           : > C1RAM
   .ebss            : > C1RAM
   .econst          : >> CMBANK0_SECTOR0 | CMBANK0_SECTOR1, ALIGN(16)
   .esysmem         : > C1RAM
   .data            : > S3RAM
   .bss             : > S3RAM
   .const           : >> CMBANK0_SECTOR0 | CMBANK0_SECTOR1, ALIGN(16)

    MSGRAM_CM_TO_CPU1 : > CMTOCPU1MSGRAM0, type=NOINIT
    MSGRAM_CM_TO_CPU2 : > CMTOCPU2MSGRAM0, type=NOINIT
    MSGRAM_CPU1_TO_CM : > CPU1TOCMMSGRAM0, type=NOINIT
    MSGRAM_CPU2_TO_CM : > CPU2TOCMMSGRAM0, type=NOINIT

    .TI.ramfunc : {} LOAD = CMBANK0_SECTOR0,
                     RUN = C0RAM,
                     LOAD_START(RamfuncsLoadStart),
                     LOAD_SIZE(RamfuncsLoadSize),
                     LOAD_END(RamfuncsLoadEnd),
                     RUN_START(RamfuncsRunStart),
                     RUN_SIZE(RamfuncsRunSize),
                     RUN_END(RamfuncsRunEnd),
                     ALIGN(16)
}

/*
//===========================================================================
// End of file.
//===========================================================================
*/
