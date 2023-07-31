// Copyright Epic Games, Inc. All Rights Reserved.
/* date = February 15th 2022 11:27 am */

#ifndef SYMS_REGS_HELPERS_H
#define SYMS_REGS_HELPERS_H

SYMS_API SYMS_U64 syms_reg_count_from_arch(SYMS_Arch arch);
SYMS_API SYMS_String8 *syms_reg_name_table_from_arch(SYMS_Arch arch);
SYMS_API SYMS_RegSection *syms_reg_section_table_from_arch(SYMS_Arch arch);

SYMS_API SYMS_RegSection syms_reg_section_from_reg_id(SYMS_Arch arch, SYMS_RegID reg_id);

#endif // SYMS_REGS_HELPERS_H
