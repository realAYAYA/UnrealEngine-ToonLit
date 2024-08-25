// Copyright Epic Games, Inc. All Rights Reserved.
// generated
#ifndef _SYMS_META_COFF_C
#define _SYMS_META_COFF_C
//~ generated from code at syms/metaprogram/syms_metaprogram_serial.c:1150
SYMS_API SYMS_Arch
syms_arch_from_coff_machine_type(SYMS_CoffMachineType v){
SYMS_Arch result = SYMS_Arch_Null;
switch (v){
default: break;
case SYMS_CoffMachineType_X86: result = SYMS_Arch_X86; break;
case SYMS_CoffMachineType_X64: result = SYMS_Arch_X64; break;
case SYMS_CoffMachineType_ARM: result = SYMS_Arch_ARM32; break;
case SYMS_CoffMachineType_ARM64: result = SYMS_Arch_ARM; break;
case SYMS_CoffMachineType_IA64: result = SYMS_Arch_IA64; break;
case SYMS_CoffMachineType_POWERPC: result = SYMS_Arch_PPC; break;
}
return(result);
}
SYMS_API SYMS_U32
syms_coff_reloc_size_for_x64(SYMS_CoffRelocTypeX64 v){
SYMS_U32 result = 0;
switch (v){
default: break;
case SYMS_CoffRelocTypeX64_ABS: result = 4; break;
case SYMS_CoffRelocTypeX64_ADDR64: result = 8; break;
case SYMS_CoffRelocTypeX64_ADDR32: result = 4; break;
case SYMS_CoffRelocTypeX64_ADDR32NB: result = 4; break;
case SYMS_CoffRelocTypeX64_REL32: result = 4; break;
case SYMS_CoffRelocTypeX64_REL32_1: result = 4; break;
case SYMS_CoffRelocTypeX64_REL32_2: result = 4; break;
case SYMS_CoffRelocTypeX64_REL32_3: result = 4; break;
case SYMS_CoffRelocTypeX64_REL32_4: result = 4; break;
case SYMS_CoffRelocTypeX64_REL32_5: result = 4; break;
case SYMS_CoffRelocTypeX64_SECTION: result = 2; break;
case SYMS_CoffRelocTypeX64_SECREL: result = 4; break;
case SYMS_CoffRelocTypeX64_SECREL7: result = 1; break;
case SYMS_CoffRelocTypeX64_TOKEN: result = 999999; break;
case SYMS_CoffRelocTypeX64_SREL32: result = 4; break;
case SYMS_CoffRelocTypeX64_PAIR: result = 999999; break;
case SYMS_CoffRelocTypeX64_SSPAN32: result = 4; break;
}
return(result);
}
SYMS_API SYMS_U32
syms_coff_reloc_size_for_x86(SYMS_CoffRelocTypeX86 v){
SYMS_U32 result = 0;
switch (v){
default: break;
case SYMS_CoffRelocTypeX86_ABS: result = 4; break;
case SYMS_CoffRelocTypeX86_DIR16: result = 2; break;
case SYMS_CoffRelocTypeX86_REL16: result = 2; break;
case SYMS_CoffRelocTypeX86_UNKNOWN0: result = 0; break;
case SYMS_CoffRelocTypeX86_UNKNOWN2: result = 0; break;
case SYMS_CoffRelocTypeX86_UNKNOWN3: result = 0; break;
case SYMS_CoffRelocTypeX86_DIR32: result = 4; break;
case SYMS_CoffRelocTypeX86_DIR32NB: result = 4; break;
case SYMS_CoffRelocTypeX86_SEG12: result = 0; break;
case SYMS_CoffRelocTypeX86_SECTION: result = 2; break;
case SYMS_CoffRelocTypeX86_SECREL: result = 4; break;
case SYMS_CoffRelocTypeX86_TOKEN: result = 4; break;
case SYMS_CoffRelocTypeX86_SECREL7: result = 1; break;
case SYMS_CoffRelocTypeX86_UNKNOWN4: result = 0; break;
case SYMS_CoffRelocTypeX86_UNKNOWN5: result = 0; break;
case SYMS_CoffRelocTypeX86_UNKNOWN6: result = 0; break;
case SYMS_CoffRelocTypeX86_UNKNOWN7: result = 0; break;
case SYMS_CoffRelocTypeX86_UNKNOWN8: result = 0; break;
case SYMS_CoffRelocTypeX86_UNKNOWN9: result = 0; break;
case SYMS_CoffRelocTypeX86_REL32: result = 4; break;
}
return(result);
}

//~ generated from code at syms/metaprogram/syms_metaprogram_serial.c:1607
#endif
