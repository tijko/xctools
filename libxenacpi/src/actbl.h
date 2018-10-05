/******************************************************************************
 *
 * Name: actbl.h - Basic ACPI Table Definitions
 *
 *****************************************************************************/

/*
 * Copyright (C) 2000 - 2011, Intel Corp.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    substantially similar to the "NO WARRANTY" disclaimer below
 *    ("Disclaimer") and any redistribution must be conditioned upon
 *    including a substantially similar Disclaimer requirement for further
 *    binary redistribution.
 * 3. Neither the names of the above-listed copyright holders nor the names
 *    of any contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGES.
 */

#ifndef __ACTBL_H__
#define __ACTBL_H__


/*******************************************************************************
 *
 * Fundamental ACPI tables
 *
 * This file contains definitions for the ACPI tables that are directly consumed
 * by ACPICA. All other tables are consumed by the OS-dependent ACPI-related
 * device drivers and other OS support code.
 *
 * The RSDP and FACS do not use the common ACPI table header. All other ACPI
 * tables use the header.
 *
 ******************************************************************************/


/*
 * Values for description table header signatures for tables defined in this
 * file. Useful because they make it more difficult to inadvertently type in
 * the wrong signature.
 */
#define ACPI_SIG_DSDT           "DSDT"      /* Differentiated System Description Table */
#define ACPI_SIG_FADT           "FACP"      /* Fixed ACPI Description Table */
#define ACPI_SIG_FACS           "FACS"      /* Firmware ACPI Control Structure */
#define ACPI_SIG_PSDT           "PSDT"      /* Persistent System Description Table */
#define ACPI_SIG_RSDP           "RSD PTR "  /* Root System Description Pointer */
#define ACPI_SIG_RSDT           "RSDT"      /* Root System Description Table */
#define ACPI_SIG_XSDT           "XSDT"      /* Extended  System Description Table */
#define ACPI_SIG_SSDT           "SSDT"      /* Secondary System Description Table */
#define ACPI_RSDP_NAME          "RSDP"      /* Short name for RSDP, not signature */


/*
 * All tables and structures must be byte-packed to match the ACPI
 * specification, since the tables are provided by the system BIOS
 */
#pragma pack(1)

/*
 * Note about bitfields: The uint8_t type is used for bitfields in ACPI tables.
 * This is the only type that is even remotely portable. Anything else is not
 * portable, so do not use any other bitfield types.
 */


/*******************************************************************************
 *
 * Master ACPI Table Header. This common header is used by all ACPI tables
 * except the RSDP and FACS.
 *
 ******************************************************************************/

typedef struct acpi_table_header
{
    char                    Signature[ACPI_NAME_SIZE];          /* ASCII table signature */
    uint32_t                Length;                             /* Length of table in bytes, including this header */
    uint8_t                 Revision;                           /* ACPI Specification minor version # */
    uint8_t                 Checksum;                           /* To make sum of entire table == 0 */
    char                    OemId[ACPI_OEM_ID_SIZE];            /* ASCII OEM identification */
    char                    OemTableId[ACPI_OEM_TABLE_ID_SIZE]; /* ASCII OEM table identification */
    uint32_t                OemRevision;                        /* OEM revision number */
    char                    AslCompilerId[ACPI_NAME_SIZE];      /* ASCII ASL compiler vendor ID */
    uint32_t                AslCompilerRevision;                /* ASL compiler version */

} ACPI_TABLE_HEADER;


/*******************************************************************************
 *
 * GAS - Generic Address Structure (ACPI 2.0+)
 *
 * Note: Since this structure is used in the ACPI tables, it is byte aligned.
 * If misaliged access is not supported by the hardware, accesses to the
 * 64-bit Address field must be performed with care.
 *
 ******************************************************************************/

typedef struct acpi_generic_address
{
    uint8_t                 SpaceId;                /* Address space where struct or register exists */
    uint8_t                 BitWidth;               /* Size in bits of given register */
    uint8_t                 BitOffset;              /* Bit offset within the register */
    uint8_t                 AccessWidth;            /* Minimum Access size (ACPI 3.0) */
    uint64_t                Address;                /* 64-bit address of struct or register */

} ACPI_GENERIC_ADDRESS;


/*******************************************************************************
 *
 * RSDP - Root System Description Pointer (Signature is "RSD PTR ")
 *        Version 2
 *
 ******************************************************************************/

typedef struct acpi_table_rsdp
{
    char                    Signature[8];               /* ACPI signature, contains "RSD PTR " */
    uint8_t                 Checksum;                   /* ACPI 1.0 checksum */
    char                    OemId[ACPI_OEM_ID_SIZE];    /* OEM identification */
    uint8_t                 Revision;                   /* Must be (0) for ACPI 1.0 or (2) for ACPI 2.0+ */
    uint32_t                RsdtPhysicalAddress;        /* 32-bit physical address of the RSDT */
    uint32_t                Length;                     /* Table length in bytes, including header (ACPI 2.0+) */
    uint64_t                XsdtPhysicalAddress;        /* 64-bit physical address of the XSDT (ACPI 2.0+) */
    uint8_t                 ExtendedChecksum;           /* Checksum of entire table (ACPI 2.0+) */
    uint8_t                 Reserved[3];                /* Reserved, must be zero */

} ACPI_TABLE_RSDP;

/* Standalone struct for the ACPI 1.0 RSDP */

typedef struct acpi_rsdp_common
{
    char                    Signature[8];
    uint8_t                 Checksum;
    char                    OemId[ACPI_OEM_ID_SIZE];
    uint8_t                 Revision;
    uint32_t                RsdtPhysicalAddress;

} ACPI_RSDP_COMMON;

/* Standalone struct for the extended part of the RSDP (ACPI 2.0+) */

typedef struct acpi_rsdp_extension
{
    uint32_t                Length;
    uint64_t                XsdtPhysicalAddress;
    uint8_t                 ExtendedChecksum;
    uint8_t                 Reserved[3];

} ACPI_RSDP_EXTENSION;


/*******************************************************************************
 *
 * RSDT/XSDT - Root System Description Tables
 *             Version 1 (both)
 *
 ******************************************************************************/

typedef struct acpi_table_rsdt
{
    ACPI_TABLE_HEADER       Header;                 /* Common ACPI table header */
    uint32_t                TableOffsetEntry[1];    /* Array of pointers to ACPI tables */

} ACPI_TABLE_RSDT;

typedef struct acpi_table_xsdt
{
    ACPI_TABLE_HEADER       Header;                 /* Common ACPI table header */
    uint64_t                TableOffsetEntry[1];    /* Array of pointers to ACPI tables */

} ACPI_TABLE_XSDT;


/*******************************************************************************
 *
 * FACS - Firmware ACPI Control Structure (FACS)
 *
 ******************************************************************************/

typedef struct acpi_table_facs
{
    char                    Signature[4];           /* ASCII table signature */
    uint32_t                Length;                 /* Length of structure, in bytes */
    uint32_t                HardwareSignature;      /* Hardware configuration signature */
    uint32_t                FirmwareWakingVector;   /* 32-bit physical address of the Firmware Waking Vector */
    uint32_t                GlobalLock;             /* Global Lock for shared hardware resources */
    uint32_t                Flags;
    uint64_t                XFirmwareWakingVector;  /* 64-bit version of the Firmware Waking Vector (ACPI 2.0+) */
    uint8_t                 Version;                /* Version of this table (ACPI 2.0+) */
    uint8_t                 Reserved[3];            /* Reserved, must be zero */
    uint32_t                OspmFlags;              /* Flags to be set by OSPM (ACPI 4.0) */
    uint8_t                 Reserved1[24];          /* Reserved, must be zero */

} ACPI_TABLE_FACS;

/* Masks for GlobalLock flag field above */

#define ACPI_GLOCK_PENDING          (1)             /* 00: Pending global lock ownership */
#define ACPI_GLOCK_OWNED            (1<<1)          /* 01: Global lock is owned */

/* Masks for Flags field above  */

#define ACPI_FACS_S4_BIOS_PRESENT   (1)             /* 00: S4BIOS support is present */
#define ACPI_FACS_64BIT_WAKE        (1<<1)          /* 01: 64-bit wake vector supported (ACPI 4.0) */

/* Masks for OspmFlags field above */

#define ACPI_FACS_64BIT_ENVIRONMENT (1)             /* 00: 64-bit wake environment is required (ACPI 4.0) */


/*******************************************************************************
 *
 * FADT - Fixed ACPI Description Table (Signature "FACP")
 *        Version 4
 *
 ******************************************************************************/

/* Fields common to all versions of the FADT */

typedef struct acpi_table_fadt
{
    ACPI_TABLE_HEADER       Header;             /* Common ACPI table header */
    uint32_t                Facs;               /* 32-bit physical address of FACS */
    uint32_t                Dsdt;               /* 32-bit physical address of DSDT */
    uint8_t                 Model;              /* System Interrupt Model (ACPI 1.0) - not used in ACPI 2.0+ */
    uint8_t                 PreferredProfile;   /* Conveys preferred power management profile to OSPM. */
    uint16_t                SciInterrupt;       /* System vector of SCI interrupt */
    uint32_t                SmiCommand;         /* 32-bit Port address of SMI command port */
    uint8_t                 AcpiEnable;         /* Value to write to smi_cmd to enable ACPI */
    uint8_t                 AcpiDisable;        /* Value to write to smi_cmd to disable ACPI */
    uint8_t                 S4BiosRequest;      /* Value to write to SMI CMD to enter S4BIOS state */
    uint8_t                 PstateControl;      /* Processor performance state control*/
    uint32_t                Pm1aEventBlock;     /* 32-bit Port address of Power Mgt 1a Event Reg Blk */
    uint32_t                Pm1bEventBlock;     /* 32-bit Port address of Power Mgt 1b Event Reg Blk */
    uint32_t                Pm1aControlBlock;   /* 32-bit Port address of Power Mgt 1a Control Reg Blk */
    uint32_t                Pm1bControlBlock;   /* 32-bit Port address of Power Mgt 1b Control Reg Blk */
    uint32_t                Pm2ControlBlock;    /* 32-bit Port address of Power Mgt 2 Control Reg Blk */
    uint32_t                PmTimerBlock;       /* 32-bit Port address of Power Mgt Timer Ctrl Reg Blk */
    uint32_t                Gpe0Block;          /* 32-bit Port address of General Purpose Event 0 Reg Blk */
    uint32_t                Gpe1Block;          /* 32-bit Port address of General Purpose Event 1 Reg Blk */
    uint8_t                 Pm1EventLength;     /* Byte Length of ports at Pm1xEventBlock */
    uint8_t                 Pm1ControlLength;   /* Byte Length of ports at Pm1xControlBlock */
    uint8_t                 Pm2ControlLength;   /* Byte Length of ports at Pm2ControlBlock */
    uint8_t                 PmTimerLength;      /* Byte Length of ports at PmTimerBlock */
    uint8_t                 Gpe0BlockLength;    /* Byte Length of ports at Gpe0Block */
    uint8_t                 Gpe1BlockLength;    /* Byte Length of ports at Gpe1Block */
    uint8_t                 Gpe1Base;           /* Offset in GPE number space where GPE1 events start */
    uint8_t                 CstControl;         /* Support for the _CST object and C States change notification */
    uint16_t                C2Latency;          /* Worst case HW latency to enter/exit C2 state */
    uint16_t                C3Latency;          /* Worst case HW latency to enter/exit C3 state */
    uint16_t                FlushSize;          /* Processor's memory cache line width, in bytes */
    uint16_t                FlushStride;        /* Number of flush strides that need to be read */
    uint8_t                 DutyOffset;         /* Processor duty cycle index in processor's P_CNT reg */
    uint8_t                 DutyWidth;          /* Processor duty cycle value bit width in P_CNT register */
    uint8_t                 DayAlarm;           /* Index to day-of-month alarm in RTC CMOS RAM */
    uint8_t                 MonthAlarm;         /* Index to month-of-year alarm in RTC CMOS RAM */
    uint8_t                 Century;            /* Index to century in RTC CMOS RAM */
    uint16_t                BootFlags;          /* IA-PC Boot Architecture Flags (see below for individual flags) */
    uint8_t                 Reserved;           /* Reserved, must be zero */
    uint32_t                Flags;              /* Miscellaneous flag bits (see below for individual flags) */
    ACPI_GENERIC_ADDRESS    ResetRegister;      /* 64-bit address of the Reset register */
    uint8_t                 ResetValue;         /* Value to write to the ResetRegister port to reset the system */
    uint8_t                 Reserved4[3];       /* Reserved, must be zero */
    uint64_t                XFacs;              /* 64-bit physical address of FACS */
    uint64_t                XDsdt;              /* 64-bit physical address of DSDT */
    ACPI_GENERIC_ADDRESS    XPm1aEventBlock;    /* 64-bit Extended Power Mgt 1a Event Reg Blk address */
    ACPI_GENERIC_ADDRESS    XPm1bEventBlock;    /* 64-bit Extended Power Mgt 1b Event Reg Blk address */
    ACPI_GENERIC_ADDRESS    XPm1aControlBlock;  /* 64-bit Extended Power Mgt 1a Control Reg Blk address */
    ACPI_GENERIC_ADDRESS    XPm1bControlBlock;  /* 64-bit Extended Power Mgt 1b Control Reg Blk address */
    ACPI_GENERIC_ADDRESS    XPm2ControlBlock;   /* 64-bit Extended Power Mgt 2 Control Reg Blk address */
    ACPI_GENERIC_ADDRESS    XPmTimerBlock;      /* 64-bit Extended Power Mgt Timer Ctrl Reg Blk address */
    ACPI_GENERIC_ADDRESS    XGpe0Block;         /* 64-bit Extended General Purpose Event 0 Reg Blk address */
    ACPI_GENERIC_ADDRESS    XGpe1Block;         /* 64-bit Extended General Purpose Event 1 Reg Blk address */

} ACPI_TABLE_FADT;


/* Masks for FADT Boot Architecture Flags (BootFlags) */

#define ACPI_FADT_LEGACY_DEVICES    (1)         /* 00: [V2] System has LPC or ISA bus devices */
#define ACPI_FADT_8042              (1<<1)      /* 01: [V3] System has an 8042 controller on port 60/64 */
#define ACPI_FADT_NO_VGA            (1<<2)      /* 02: [V4] It is not safe to probe for VGA hardware */
#define ACPI_FADT_NO_MSI            (1<<3)      /* 03: [V4] Message Signaled Interrupts (MSI) must not be enabled */
#define ACPI_FADT_NO_ASPM           (1<<4)      /* 04: [V4] PCIe ASPM control must not be enabled */

/* Masks for FADT flags */

#define ACPI_FADT_WBINVD            (1)         /* 00: [V1] The wbinvd instruction works properly */
#define ACPI_FADT_WBINVD_FLUSH      (1<<1)      /* 01: [V1] wbinvd flushes but does not invalidate caches */
#define ACPI_FADT_C1_SUPPORTED      (1<<2)      /* 02: [V1] All processors support C1 state */
#define ACPI_FADT_C2_MP_SUPPORTED   (1<<3)      /* 03: [V1] C2 state works on MP system */
#define ACPI_FADT_POWER_BUTTON      (1<<4)      /* 04: [V1] Power button is handled as a control method device */
#define ACPI_FADT_SLEEP_BUTTON      (1<<5)      /* 05: [V1] Sleep button is handled as a control method device */
#define ACPI_FADT_FIXED_RTC         (1<<6)      /* 06: [V1] RTC wakeup status not in fixed register space */
#define ACPI_FADT_S4_RTC_WAKE       (1<<7)      /* 07: [V1] RTC alarm can wake system from S4 */
#define ACPI_FADT_32BIT_TIMER       (1<<8)      /* 08: [V1] ACPI timer width is 32-bit (0=24-bit) */
#define ACPI_FADT_DOCKING_SUPPORTED (1<<9)      /* 09: [V1] Docking supported */
#define ACPI_FADT_RESET_REGISTER    (1<<10)     /* 10: [V2] System reset via the FADT RESET_REG supported */
#define ACPI_FADT_SEALED_CASE       (1<<11)     /* 11: [V3] No internal expansion capabilities and case is sealed */
#define ACPI_FADT_HEADLESS          (1<<12)     /* 12: [V3] No local video capabilities or local input devices */
#define ACPI_FADT_SLEEP_TYPE        (1<<13)     /* 13: [V3] Must execute native instruction after writing  SLP_TYPx register */
#define ACPI_FADT_PCI_EXPRESS_WAKE  (1<<14)     /* 14: [V4] System supports PCIEXP_WAKE (STS/EN) bits (ACPI 3.0) */
#define ACPI_FADT_PLATFORM_CLOCK    (1<<15)     /* 15: [V4] OSPM should use platform-provided timer (ACPI 3.0) */
#define ACPI_FADT_S4_RTC_VALID      (1<<16)     /* 16: [V4] Contents of RTC_STS valid after S4 wake (ACPI 3.0) */
#define ACPI_FADT_REMOTE_POWER_ON   (1<<17)     /* 17: [V4] System is compatible with remote power on (ACPI 3.0) */
#define ACPI_FADT_APIC_CLUSTER      (1<<18)     /* 18: [V4] All local APICs must use cluster model (ACPI 3.0) */
#define ACPI_FADT_APIC_PHYSICAL     (1<<19)     /* 19: [V4] All local xAPICs must use physical dest mode (ACPI 3.0) */


/* Values for PreferredProfile (Prefered Power Management Profiles) */

enum AcpiPreferedPmProfiles
{
    PM_UNSPECIFIED          = 0,
    PM_DESKTOP              = 1,
    PM_MOBILE               = 2,
    PM_WORKSTATION          = 3,
    PM_ENTERPRISE_SERVER    = 4,
    PM_SOHO_SERVER          = 5,
    PM_APPLIANCE_PC         = 6
};


/* Reset to default packing */

#pragma pack()


/*
 * Internal table-related structures
 */
typedef union acpi_name_union
{
    uint32_t                        Integer;
    char                            Ascii[4];

} ACPI_NAME_UNION;


/* Internal ACPI Table Descriptor. One per ACPI table. */

typedef struct acpi_table_desc
{
    ACPI_PHYSICAL_ADDRESS           Address;
    ACPI_TABLE_HEADER               *Pointer;
    uint32_t                        Length;     /* Length fixed at 32 bits */
    ACPI_NAME_UNION                 Signature;
    ACPI_OWNER_ID                   OwnerId;
    uint8_t                         Flags;

} ACPI_TABLE_DESC;

/* Masks for Flags field above */

#define ACPI_TABLE_ORIGIN_UNKNOWN       (0)
#define ACPI_TABLE_ORIGIN_MAPPED        (1)
#define ACPI_TABLE_ORIGIN_ALLOCATED     (2)
#define ACPI_TABLE_ORIGIN_OVERRIDE      (4)
#define ACPI_TABLE_ORIGIN_MASK          (7)
#define ACPI_TABLE_IS_LOADED            (8)


/*
 * Get the remaining ACPI tables
 */
/**
#include "actbl1.h"
#include "actbl2.h"
**/

/* Macros used to generate offsets to specific table fields */

#define ACPI_FADT_OFFSET(f)             (uint8_t) ACPI_OFFSET (ACPI_TABLE_FADT, f)

/*
 * Sizes of the various flavors of FADT. We need to look closely
 * at the FADT length because the version number essentially tells
 * us nothing because of many BIOS bugs where the version does not
 * match the expected length. In other words, the length of the
 * FADT is the bottom line as to what the version really is.
 *
 * For reference, the values below are as follows:
 *     FADT V1  size: 0x74
 *     FADT V2  size: 0x84
 *     FADT V3+ size: 0xF4
 */
#define ACPI_FADT_V1_SIZE       (uint32_t) (ACPI_FADT_OFFSET (Flags) + 4)
#define ACPI_FADT_V2_SIZE       (uint32_t) (ACPI_FADT_OFFSET (Reserved4[0]) + 3)
#define ACPI_FADT_V3_SIZE       (uint32_t) (sizeof (ACPI_TABLE_FADT))

#endif /* __ACTBL_H__ */
