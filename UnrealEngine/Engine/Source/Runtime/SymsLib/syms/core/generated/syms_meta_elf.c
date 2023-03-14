// Copyright Epic Games, Inc. All Rights Reserved.
// generated
#ifndef _SYMS_META_ELF_C
#define _SYMS_META_ELF_C
//~ generated from code at syms/metaprogram/syms_metaprogram_serial.c:1150
SYMS_API SYMS_Arch
syms_arch_from_elf_machine_type(SYMS_ElfMachineKind v){
SYMS_Arch result = SYMS_Arch_Null;
switch (v){
default: break;
case SYMS_ElfMachineKind_386: result = SYMS_Arch_X86; break;
case SYMS_ElfMachineKind_PPC: result = SYMS_Arch_PPC; break;
case SYMS_ElfMachineKind_PPC64: result = SYMS_Arch_PPC64; break;
case SYMS_ElfMachineKind_ARM: result = SYMS_Arch_ARM32; break;
case SYMS_ElfMachineKind_IA_64: result = SYMS_Arch_IA64; break;
case SYMS_ElfMachineKind_X86_64: result = SYMS_Arch_X64; break;
case SYMS_ElfMachineKind_AARCH64: result = SYMS_Arch_ARM; break;
}
return(result);
}

//~ generated from code at syms/metaprogram/syms_metaprogram_serial.c:1607
#endif
