// Copyright Epic Games, Inc. All Rights Reserved.
// generated
#ifndef _SYMS_META_DWARF_CFI_H
#define _SYMS_META_DWARF_CFI_H
//~ generated from code at syms/metaprogram/syms_metaprogram_serial.c:885
typedef SYMS_U8 SYMS_DwCFA;
enum{
//  kind1
SYMS_DwCFA_NOP = 0x0,
SYMS_DwCFA_SET_LOC = 0x1,
SYMS_DwCFA_ADVANCE_LOC1 = 0x2,
SYMS_DwCFA_ADVANCE_LOC2 = 0x3,
SYMS_DwCFA_ADVANCE_LOC4 = 0x4,
SYMS_DwCFA_OFFSET_EXT = 0x5,
SYMS_DwCFA_RESTORE_EXT = 0x6,
SYMS_DwCFA_UNDEFINED = 0x7,
SYMS_DwCFA_SAME_VALUE = 0x8,
SYMS_DwCFA_REGISTER = 0x9,
SYMS_DwCFA_REMEMBER_STATE = 0xA,
SYMS_DwCFA_RESTORE_STATE = 0xB,
SYMS_DwCFA_DEF_CFA = 0xC,
SYMS_DwCFA_DEF_CFA_REGISTER = 0xD,
SYMS_DwCFA_DEF_CFA_OFFSET = 0xE,
SYMS_DwCFA_DEF_CFA_EXPR = 0xF,
SYMS_DwCFA_EXPR = 0x10,
SYMS_DwCFA_OFFSET_EXT_SF = 0x11,
SYMS_DwCFA_DEF_CFA_SF = 0x12,
SYMS_DwCFA_DEF_CFA_OFFSET_SF = 0x13,
SYMS_DwCFA_VAL_OFFSET = 0x14,
SYMS_DwCFA_VAL_OFFSET_SF = 0x15,
SYMS_DwCFA_VAL_EXPR = 0x16,
//  kind2
SYMS_DwCFA_ADVANCE_LOC = 0x40,
SYMS_DwCFA_OFFSET = 0x80,
SYMS_DwCFA_RESTORE = 0xC0,
SYMS_DwCFA_COUNT = 26
};
typedef SYMS_U8 SYMS_DwCFADetail;
enum{
SYMS_DwCFADetail_OPL_KIND1 = SYMS_DwCFA_VAL_EXPR,
SYMS_DwCFADetail_OPL_KIND2 = SYMS_DwCFA_RESTORE,
SYMS_DwCFADetail_COUNT = 2
};
typedef SYMS_U8 SYMS_DwCFAMask;
enum{
//  kind1:  opcode: [0,5] zeroes:[6,7]; kind2:  operand:[0,5] opcode:[6,7] 
SYMS_DwCFAMask_HI_OPCODE = 0xC0,
SYMS_DwCFAMask_OPERAND = 0x3F,
SYMS_DwCFAMask_COUNT = 2
};

//~ generated from code at syms/metaprogram/syms_metaprogram_serial.c:1588
SYMS_C_LINKAGE_BEGIN
SYMS_C_LINKAGE_END

//~ generated from code at syms/metaprogram/syms_metaprogram_serial.c:1694
#endif
