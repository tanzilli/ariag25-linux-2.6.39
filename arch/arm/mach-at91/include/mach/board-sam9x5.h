/*
 *  Board-specific header file for the AT91SAM9x5 Evaluation Kit family
 *
 *  Copyright (C) 2010 Atmel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */

/*
 * board revision encoding
 *
 *  ATAG_SN    lower 32 bits
 *     0-4   cpu_module_board_id         5 bits
 *     5-9   cpu_module_vendor_id        5 bits
 *     10-14 display_module_board_id     5 bits
 *     15-19 display_module_vendor_id    5 bits
 *     20-24 mother_board_id             5 bits
 *     25-29 mother_board_vendor_id      5 bits
 *     30-31 reserved for future use     2 bits
 *
 * rev: stands for revision code letter: the 'B' in "B1" revision code for
 *      instance coded as a increment from 'A' starting at 0x0: 0x0 means 'A',
 *      0x1 means 'B', etc.
 *
 * rev_id: stands for revision code identifier ;  it is a number: the '1' in
 *         "B1" revision code for instance: coded as a increment from '0'
 *         starting at 0x0: 0x0 means '0', 0x1 means '1', etc.)
 *
 *  ATAG_REV
 *     0-4   cpu_module_board_rev        5 bits
 *     5-9   display_module_board_rev    5 bits
 *     10-14 mother_module_board_rev     5 bits
 *     15-17 cpu_module_board_rev_id     3 bits
 *     18-20 display_module_board_rev_id 3 bits
 *     21-23 mother_module_board_rev_id  3 bits
 *     24-31 reserved for future use     8 bits
 *
 * OWI sands for One Wire Information
 * The information comes form the 1-wire component on each board
 * and is encoded in ATAGs: both system_serial_low and system_rev
 */

#define CM_REV_OFFSET		0
#define CM_REV_SIZE		5
#define CM_REV_ID_OFFSET	15
#define CM_REV_ID_SIZE		3
#define DM_REV_OFFSET		5
#define DM_REV_SIZE		5
#define DM_REV_ID_OFFSET	18
#define DM_REV_ID_SIZE		3
#define EK_REV_OFFSET		10
#define EK_REV_SIZE		5
#define EK_REV_ID_OFFSET	21
#define EK_REV_ID_SIZE		3

/* Bit manipulation macros */
#define OWI_BIT(name) \
        (1 << name##_OFFSET)
#define OWI_BF(name,value) \
        (((value) & ((1 << name##_SIZE) - 1)) << name##_OFFSET)
#define OWI_BFEXT(name,value) \
        (((value) >> name##_OFFSET) & ((1 << name##_SIZE) - 1))
#define OWI_BFINS(name,value,old) \
        ( ((old) & ~(((1 << name##_SIZE) - 1) << name##_OFFSET)) \
          | SPI_BF(name,value))

#define cm_rev()	OWI_BFEXT(CM_REV, system_rev)
#define dm_rev()	OWI_BFEXT(DM_REV, system_rev)
#define ek_rev()	OWI_BFEXT(EK_REV, system_rev)

#define cm_is_revA()	(cm_rev() == 0)
#define cm_is_revB()	(cm_rev() == ('B' - 'A'))

#define ek_is_revA()	(ek_rev() == 0)
#define ek_is_revB()	(ek_rev() == ('B' - 'A'))

/* Configuration of CPU Module useful for mother board */
#define CM_CONFIG_SPI0_ENABLE	(1 <<  0)
#define CM_CONFIG_SPI1_ENABLE	(1 <<  1)
#define CM_CONFIG_I2C0_ENABLE	(1 <<  2)
#define CM_CONFIG_I2C1_ENABLE	(1 <<  3)


/* CPU Module prototypes */
void __init cm_map_io(void);
void __init cm_init_irq(void);
void __init cm_board_init(u32 *cm_config);
