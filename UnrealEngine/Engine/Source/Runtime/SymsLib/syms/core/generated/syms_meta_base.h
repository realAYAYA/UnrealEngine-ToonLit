// Copyright Epic Games, Inc. All Rights Reserved.
// generated
#ifndef _SYMS_META_BASE_H
#define _SYMS_META_BASE_H
//~ generated from code at syms/metaprogram/syms_metaprogram_serial.c:885
typedef enum SYMS_Arch{
SYMS_Arch_Null,
SYMS_Arch_X64,
SYMS_Arch_X86,
SYMS_Arch_ARM,
SYMS_Arch_ARM32,
SYMS_Arch_PPC64,
SYMS_Arch_PPC,
SYMS_Arch_IA64,
SYMS_Arch_COUNT = 8
} SYMS_Arch;
typedef enum SYMS_OperatingSystem{
SYMS_OperatingSystem_Null,
SYMS_OperatingSystem_Win,
SYMS_OperatingSystem_Linux,
SYMS_OperatingSystem_Mac,
SYMS_OperatingSystem_COUNT = 4
} SYMS_OperatingSystem;
typedef enum SYMS_Language{
SYMS_Language_Null,
//  NOTE(rjf): C and C++
SYMS_Language_C89,
SYMS_Language_C99,
SYMS_Language_C11,
SYMS_Language_C,
SYMS_Language_CPlusPlus,
SYMS_Language_CPlusPlus03,
SYMS_Language_CPlusPlus11,
SYMS_Language_CPlusPlus14,
SYMS_Language_CSharp,
//  NOTE(rjf): Apple languages
SYMS_Language_ObjectiveC,
SYMS_Language_ObjectiveCPlusPlus,
SYMS_Language_Swift,
//  NOTE(rjf): Super Cool Kids languages
SYMS_Language_Java,
SYMS_Language_JavaScript,
SYMS_Language_Python,
SYMS_Language_Go,
SYMS_Language_Rust,
SYMS_Language_Julia,
//  NOTE(rjf): Community-Torn-By-GC languages
SYMS_Language_D,
//  NOTE(rjf): Languages that everyone jokes about being old, or their derivatives
SYMS_Language_Fortran,
SYMS_Language_Fortran77,
SYMS_Language_Fortran90,
SYMS_Language_Fortran95,
SYMS_Language_Fortran03,
SYMS_Language_Fortran08,
SYMS_Language_Pascal,
SYMS_Language_Pascal83,
SYMS_Language_Ada83,
SYMS_Language_Ada95,
SYMS_Language_Cobol,
SYMS_Language_Cobol74,
SYMS_Language_Cobol85,
SYMS_Language_Modula2,
SYMS_Language_Modula3,
//  NOTE(rjf): ASM, or other bytecode
SYMS_Language_MASM,
SYMS_Language_ILASM,
//  NOTE(rjf): IL ASM (Common Language Runtime)
SYMS_Language_MSIL,
//  NOTE(rjf): "Has 'Basic' in the name" languages
SYMS_Language_Basic,
SYMS_Language_VisualBasic,
//  NOTE(rjf): Languages I would've never heard about, were it not for DWARF/PDB specs
SYMS_Language_Link,
SYMS_Language_CVTRES,
SYMS_Language_CVTPGD,
SYMS_Language_UPC,
SYMS_Language_PLI,
SYMS_Language_Dylan,
SYMS_Language_BLISS,
//  NOTE(rjf): GPU Stuff
SYMS_Language_HLSL,
SYMS_Language_OpenCL,
SYMS_Language_RenderScript,
//  NOTE(rjf): Misc
SYMS_Language_Haskell,
SYMS_Language_OCaml,
SYMS_Language_MipsAssembler,
SYMS_Language_GoogleRenderScript,
SYMS_Language_SunAssembler,
SYMS_Language_BorlandDelphi,
SYMS_Language_COUNT = 56
} SYMS_Language;
typedef enum SYMS_FileFormat{
SYMS_FileFormat_Null,
SYMS_FileFormat_PE,
SYMS_FileFormat_ELF,
SYMS_FileFormat_MACH,
SYMS_FileFormat_PDB,
SYMS_FileFormat_DWARF,
SYMS_FileFormat_COUNT = 6
} SYMS_FileFormat;
typedef enum SYMS_ChecksumAlgorithm{
SYMS_ChecksumAlgorithm_Null,
SYMS_ChecksumAlgorithm_MD5,
SYMS_ChecksumAlgorithm_SHA1,
SYMS_ChecksumAlgorithm_SHA256,
SYMS_ChecksumAlgorithm_CRC32_IEEE_802_3,
SYMS_ChecksumAlgorithm_COUNT = 5
} SYMS_ChecksumAlgorithm;

//~ generated from code at syms/metaprogram/syms_metaprogram_serial.c:1133
SYMS_C_LINKAGE_BEGIN
SYMS_API SYMS_U32 syms_address_size_from_arch(SYMS_Arch v);
SYMS_C_LINKAGE_END

//~ generated from code at syms/metaprogram/syms_metaprogram_serial.c:1588
SYMS_C_LINKAGE_BEGIN
SYMS_C_LINKAGE_END

//~ generated from code at syms/metaprogram/syms_metaprogram_serial.c:1694
#endif
