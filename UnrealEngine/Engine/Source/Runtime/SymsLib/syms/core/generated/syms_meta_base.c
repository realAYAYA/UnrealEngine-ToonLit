// Copyright Epic Games, Inc. All Rights Reserved.
// generated
#ifndef _SYMS_META_BASE_C
#define _SYMS_META_BASE_C
//~ generated from code at syms/metaprogram/syms_metaprogram_serial.c:1150
SYMS_API SYMS_U32
syms_address_size_from_arch(SYMS_Arch v){
SYMS_U32 result = 0;
switch (v){
default: break;
case SYMS_Arch_X64: result = 64; break;
case SYMS_Arch_X86: result = 32; break;
case SYMS_Arch_ARM: result = 64; break;
case SYMS_Arch_ARM32: result = 32; break;
case SYMS_Arch_PPC64: result = 64; break;
case SYMS_Arch_PPC: result = 32; break;
case SYMS_Arch_IA64: result = 64; break;
}
return(result);
}

//~ generated from code at syms/metaprogram/syms_metaprogram_serial.c:1607
#endif
