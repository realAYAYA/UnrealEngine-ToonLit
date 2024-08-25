// Copyright Epic Games, Inc. All Rights Reserved.
// generated
#ifndef _SYMS_META_SERIAL_EXT_C
#define _SYMS_META_SERIAL_EXT_C

////////////////////////////////
#if defined(SYMS_ENABLE_BASE_SERIAL_INFO)

//~ generated from code at syms/metaprogram/syms_metaprogram_serial.c:1091
// syms_enum_index_from_arch - skipped identity mapping
// syms_enum_index_from_operating_system - skipped identity mapping
// syms_enum_index_from_language - skipped identity mapping
// syms_enum_index_from_fileformat - skipped identity mapping
// syms_enum_index_from_checksum_algorithm - skipped identity mapping

//~ generated from code at syms/metaprogram/syms_metaprogram_serial.c:1322
SYMS_SerialValue _syms_serial_members_for_SYMS_Arch[] = {
{ { (SYMS_U8*)"NULL", 4 }, (SYMS_U64)SYMS_Arch_Null },
{ { (SYMS_U8*)"X64", 3 }, (SYMS_U64)SYMS_Arch_X64 },
{ { (SYMS_U8*)"X86", 3 }, (SYMS_U64)SYMS_Arch_X86 },
{ { (SYMS_U8*)"ARM", 3 }, (SYMS_U64)SYMS_Arch_ARM },
{ { (SYMS_U8*)"ARM32", 5 }, (SYMS_U64)SYMS_Arch_ARM32 },
{ { (SYMS_U8*)"PPC64", 5 }, (SYMS_U64)SYMS_Arch_PPC64 },
{ { (SYMS_U8*)"PPC", 3 }, (SYMS_U64)SYMS_Arch_PPC },
{ { (SYMS_U8*)"IA64", 4 }, (SYMS_U64)SYMS_Arch_IA64 },
};
SYMS_SerialValue _syms_serial_members_for_SYMS_OperatingSystem[] = {
{ { (SYMS_U8*)"Null", 4 }, (SYMS_U64)SYMS_OperatingSystem_Null },
{ { (SYMS_U8*)"Windows", 7 }, (SYMS_U64)SYMS_OperatingSystem_Win },
{ { (SYMS_U8*)"Linux", 5 }, (SYMS_U64)SYMS_OperatingSystem_Linux },
{ { (SYMS_U8*)"Mac", 3 }, (SYMS_U64)SYMS_OperatingSystem_Mac },
};
SYMS_SerialValue _syms_serial_members_for_SYMS_Language[] = {
{ { (SYMS_U8*)"Null", 4 }, (SYMS_U64)SYMS_Language_Null },
{ { (SYMS_U8*)"C89", 3 }, (SYMS_U64)SYMS_Language_C89 },
{ { (SYMS_U8*)"C99", 3 }, (SYMS_U64)SYMS_Language_C99 },
{ { (SYMS_U8*)"C11", 3 }, (SYMS_U64)SYMS_Language_C11 },
{ { (SYMS_U8*)"C", 1 }, (SYMS_U64)SYMS_Language_C },
{ { (SYMS_U8*)"C++", 3 }, (SYMS_U64)SYMS_Language_CPlusPlus },
{ { (SYMS_U8*)"C++03", 5 }, (SYMS_U64)SYMS_Language_CPlusPlus03 },
{ { (SYMS_U8*)"C++11", 5 }, (SYMS_U64)SYMS_Language_CPlusPlus11 },
{ { (SYMS_U8*)"C++14", 5 }, (SYMS_U64)SYMS_Language_CPlusPlus14 },
{ { (SYMS_U8*)"C#", 2 }, (SYMS_U64)SYMS_Language_CSharp },
{ { (SYMS_U8*)"Objective-C", 11 }, (SYMS_U64)SYMS_Language_ObjectiveC },
{ { (SYMS_U8*)"Objective-C++", 13 }, (SYMS_U64)SYMS_Language_ObjectiveCPlusPlus },
{ { (SYMS_U8*)"Swift", 5 }, (SYMS_U64)SYMS_Language_Swift },
{ { (SYMS_U8*)"Java", 4 }, (SYMS_U64)SYMS_Language_Java },
{ { (SYMS_U8*)"JavaScript", 10 }, (SYMS_U64)SYMS_Language_JavaScript },
{ { (SYMS_U8*)"Python", 6 }, (SYMS_U64)SYMS_Language_Python },
{ { (SYMS_U8*)"Go", 2 }, (SYMS_U64)SYMS_Language_Go },
{ { (SYMS_U8*)"Rust", 4 }, (SYMS_U64)SYMS_Language_Rust },
{ { (SYMS_U8*)"Julia", 5 }, (SYMS_U64)SYMS_Language_Julia },
{ { (SYMS_U8*)"D", 1 }, (SYMS_U64)SYMS_Language_D },
{ { (SYMS_U8*)"Fortran", 7 }, (SYMS_U64)SYMS_Language_Fortran },
{ { (SYMS_U8*)"Fortran77", 9 }, (SYMS_U64)SYMS_Language_Fortran77 },
{ { (SYMS_U8*)"Fortran90", 9 }, (SYMS_U64)SYMS_Language_Fortran90 },
{ { (SYMS_U8*)"Fortran95", 9 }, (SYMS_U64)SYMS_Language_Fortran95 },
{ { (SYMS_U8*)"Fortran03", 9 }, (SYMS_U64)SYMS_Language_Fortran03 },
{ { (SYMS_U8*)"Fortran08", 9 }, (SYMS_U64)SYMS_Language_Fortran08 },
{ { (SYMS_U8*)"Pascal", 6 }, (SYMS_U64)SYMS_Language_Pascal },
{ { (SYMS_U8*)"Pascal83", 8 }, (SYMS_U64)SYMS_Language_Pascal83 },
{ { (SYMS_U8*)"Ada83", 5 }, (SYMS_U64)SYMS_Language_Ada83 },
{ { (SYMS_U8*)"Ada95", 5 }, (SYMS_U64)SYMS_Language_Ada95 },
{ { (SYMS_U8*)"Cobol", 5 }, (SYMS_U64)SYMS_Language_Cobol },
{ { (SYMS_U8*)"Cobol74", 7 }, (SYMS_U64)SYMS_Language_Cobol74 },
{ { (SYMS_U8*)"Cobol85", 7 }, (SYMS_U64)SYMS_Language_Cobol85 },
{ { (SYMS_U8*)"Modula-2", 8 }, (SYMS_U64)SYMS_Language_Modula2 },
{ { (SYMS_U8*)"Modula-3", 8 }, (SYMS_U64)SYMS_Language_Modula3 },
{ { (SYMS_U8*)"MASM", 4 }, (SYMS_U64)SYMS_Language_MASM },
{ { (SYMS_U8*)"ILASM", 5 }, (SYMS_U64)SYMS_Language_ILASM },
{ { (SYMS_U8*)"MSIL", 4 }, (SYMS_U64)SYMS_Language_MSIL },
{ { (SYMS_U8*)"Basic", 5 }, (SYMS_U64)SYMS_Language_Basic },
{ { (SYMS_U8*)"VisualBasic", 11 }, (SYMS_U64)SYMS_Language_VisualBasic },
{ { (SYMS_U8*)"Link", 4 }, (SYMS_U64)SYMS_Language_Link },
{ { (SYMS_U8*)"CVTRES", 6 }, (SYMS_U64)SYMS_Language_CVTRES },
{ { (SYMS_U8*)"CVTPGD", 6 }, (SYMS_U64)SYMS_Language_CVTPGD },
{ { (SYMS_U8*)"Unified Parallel C", 18 }, (SYMS_U64)SYMS_Language_UPC },
{ { (SYMS_U8*)"PLI", 3 }, (SYMS_U64)SYMS_Language_PLI },
{ { (SYMS_U8*)"Dylan", 5 }, (SYMS_U64)SYMS_Language_Dylan },
{ { (SYMS_U8*)"BLISS", 5 }, (SYMS_U64)SYMS_Language_BLISS },
{ { (SYMS_U8*)"HLSL", 4 }, (SYMS_U64)SYMS_Language_HLSL },
{ { (SYMS_U8*)"OpenCL", 6 }, (SYMS_U64)SYMS_Language_OpenCL },
{ { (SYMS_U8*)"RenderScript", 12 }, (SYMS_U64)SYMS_Language_RenderScript },
{ { (SYMS_U8*)"Haskell", 7 }, (SYMS_U64)SYMS_Language_Haskell },
{ { (SYMS_U8*)"OCaml", 5 }, (SYMS_U64)SYMS_Language_OCaml },
{ { (SYMS_U8*)"MipsAssembler", 13 }, (SYMS_U64)SYMS_Language_MipsAssembler },
{ { (SYMS_U8*)"GoogleRenderScript", 18 }, (SYMS_U64)SYMS_Language_GoogleRenderScript },
{ { (SYMS_U8*)"SunAssembler", 12 }, (SYMS_U64)SYMS_Language_SunAssembler },
{ { (SYMS_U8*)"BorlandDelphi", 13 }, (SYMS_U64)SYMS_Language_BorlandDelphi },
};
SYMS_SerialValue _syms_serial_members_for_SYMS_FileFormat[] = {
{ { (SYMS_U8*)"NULL", 4 }, (SYMS_U64)SYMS_FileFormat_Null },
{ { (SYMS_U8*)"PE", 2 }, (SYMS_U64)SYMS_FileFormat_PE },
{ { (SYMS_U8*)"ELF", 3 }, (SYMS_U64)SYMS_FileFormat_ELF },
{ { (SYMS_U8*)"MACH", 4 }, (SYMS_U64)SYMS_FileFormat_MACH },
{ { (SYMS_U8*)"PDB", 3 }, (SYMS_U64)SYMS_FileFormat_PDB },
{ { (SYMS_U8*)"DWARF", 5 }, (SYMS_U64)SYMS_FileFormat_DWARF },
};
SYMS_SerialValue _syms_serial_members_for_SYMS_ChecksumAlgorithm[] = {
{ { (SYMS_U8*)"Null", 4 }, (SYMS_U64)SYMS_ChecksumAlgorithm_Null },
{ { (SYMS_U8*)"MD5", 3 }, (SYMS_U64)SYMS_ChecksumAlgorithm_MD5 },
{ { (SYMS_U8*)"SHA1", 4 }, (SYMS_U64)SYMS_ChecksumAlgorithm_SHA1 },
{ { (SYMS_U8*)"SHA256", 6 }, (SYMS_U64)SYMS_ChecksumAlgorithm_SHA256 },
{ { (SYMS_U8*)"CRC32_IEEE_802_3", 16 }, (SYMS_U64)SYMS_ChecksumAlgorithm_CRC32_IEEE_802_3 },
};

//~ generated from code at syms/metaprogram/syms_metaprogram_serial.c:1458
SYMS_SerialType _syms_serial_type_SYMS_S8 = {
{(SYMS_U8*)"SYMS_S8", 7}, SYMS_SerialTypeKind_Integer, 0, 0, 1, 0
};
SYMS_SerialType _syms_serial_type_SYMS_S16 = {
{(SYMS_U8*)"SYMS_S16", 8}, SYMS_SerialTypeKind_Integer, 0, 0, 2, 0
};
SYMS_SerialType _syms_serial_type_SYMS_S32 = {
{(SYMS_U8*)"SYMS_S32", 8}, SYMS_SerialTypeKind_Integer, 0, 0, 4, 0
};
SYMS_SerialType _syms_serial_type_SYMS_S64 = {
{(SYMS_U8*)"SYMS_S64", 8}, SYMS_SerialTypeKind_Integer, 0, 0, 8, 0
};
SYMS_SerialType _syms_serial_type_SYMS_U8 = {
{(SYMS_U8*)"SYMS_U8", 7}, SYMS_SerialTypeKind_Character, 0, 0, 1, 0
};
SYMS_SerialType _syms_serial_type_SYMS_U16 = {
{(SYMS_U8*)"SYMS_U16", 8}, SYMS_SerialTypeKind_UnsignedInteger, 0, 0, 2, 0
};
SYMS_SerialType _syms_serial_type_SYMS_U32 = {
{(SYMS_U8*)"SYMS_U32", 8}, SYMS_SerialTypeKind_UnsignedInteger, 0, 0, 4, 0
};
SYMS_SerialType _syms_serial_type_SYMS_U64 = {
{(SYMS_U8*)"SYMS_U64", 8}, SYMS_SerialTypeKind_UnsignedInteger, 0, 0, 8, 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvSectionIndex = {
{(SYMS_U8*)"SYMS_CvSectionIndex", 19}, SYMS_SerialTypeKind_UnsignedInteger, 0, 0, 2, 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvModIndex = {
{(SYMS_U8*)"SYMS_CvModIndex", 15}, SYMS_SerialTypeKind_UnsignedInteger, 0, 0, 2, 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvTypeIndex = {
{(SYMS_U8*)"SYMS_CvTypeIndex", 16}, SYMS_SerialTypeKind_UnsignedInteger, 0, 0, 4, 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvTypeId = {
{(SYMS_U8*)"SYMS_CvTypeId", 13}, SYMS_SerialTypeKind_UnsignedInteger, 0, 0, 4, 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvItemId = {
{(SYMS_U8*)"SYMS_CvItemId", 13}, SYMS_SerialTypeKind_UnsignedInteger, 0, 0, 4, 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvReg = {
{(SYMS_U8*)"SYMS_CvReg", 10}, SYMS_SerialTypeKind_UnsignedInteger, 0, 0, 2, 0
};
SYMS_SerialType _syms_serial_type_SYMS_PdbBinaryAnnotation = {
{(SYMS_U8*)"SYMS_PdbBinaryAnnotation", 24}, SYMS_SerialTypeKind_BinaryAnnotation, 0, 0, 1, 0
};
SYMS_SerialType _syms_serial_type_SYMS_PdbNumeric = {
{(SYMS_U8*)"SYMS_PdbNumeric", 15}, SYMS_SerialTypeKind_UnsignedInteger, 0, 0, 1, 0
};
SYMS_SerialType _syms_serial_type_SYMS_Arch = {
{(SYMS_U8*)"SYMS_Arch", 9}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_Arch), _syms_serial_members_for_SYMS_Arch, sizeof(SYMS_Arch), syms_enum_index_from_value_identity
};
SYMS_SerialType _syms_serial_type_SYMS_OperatingSystem = {
{(SYMS_U8*)"SYMS_OperatingSystem", 20}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_OperatingSystem), _syms_serial_members_for_SYMS_OperatingSystem, sizeof(SYMS_OperatingSystem), syms_enum_index_from_value_identity
};
SYMS_SerialType _syms_serial_type_SYMS_Language = {
{(SYMS_U8*)"SYMS_Language", 13}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_Language), _syms_serial_members_for_SYMS_Language, sizeof(SYMS_Language), syms_enum_index_from_value_identity
};
SYMS_SerialType _syms_serial_type_SYMS_FileFormat = {
{(SYMS_U8*)"SYMS_FileFormat", 15}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_FileFormat), _syms_serial_members_for_SYMS_FileFormat, sizeof(SYMS_FileFormat), syms_enum_index_from_value_identity
};
SYMS_SerialType _syms_serial_type_SYMS_ChecksumAlgorithm = {
{(SYMS_U8*)"SYMS_ChecksumAlgorithm", 22}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_ChecksumAlgorithm), _syms_serial_members_for_SYMS_ChecksumAlgorithm, sizeof(SYMS_ChecksumAlgorithm), syms_enum_index_from_value_identity
};

#endif // defined(SYMS_ENABLE_BASE_SERIAL_INFO)


////////////////////////////////
#if defined(SYMS_ENABLE_COFF_SERIAL_INFO)

//~ generated from code at syms/metaprogram/syms_metaprogram_serial.c:1091
SYMS_API SYMS_U64
syms_enum_index_from_coffmachinetype(SYMS_U64 v){
SYMS_U64 result = 0;
switch ((SYMS_U16)v){
default: break;
case SYMS_CoffMachineType_UNKNOWN: result = 0; break;
case SYMS_CoffMachineType_X86: result = 1; break;
case SYMS_CoffMachineType_X64: result = 2; break;
case SYMS_CoffMachineType_ARM33: result = 3; break;
case SYMS_CoffMachineType_ARM: result = 4; break;
case SYMS_CoffMachineType_ARM64: result = 5; break;
case SYMS_CoffMachineType_ARMNT: result = 6; break;
case SYMS_CoffMachineType_EBC: result = 7; break;
case SYMS_CoffMachineType_IA64: result = 8; break;
case SYMS_CoffMachineType_M32R: result = 9; break;
case SYMS_CoffMachineType_MIPS16: result = 10; break;
case SYMS_CoffMachineType_MIPSFPU: result = 11; break;
case SYMS_CoffMachineType_MIPSFPU16: result = 12; break;
case SYMS_CoffMachineType_POWERPC: result = 13; break;
case SYMS_CoffMachineType_POWERPCFP: result = 14; break;
case SYMS_CoffMachineType_R4000: result = 15; break;
case SYMS_CoffMachineType_RISCV32: result = 16; break;
case SYMS_CoffMachineType_RISCV64: result = 17; break;
case SYMS_CoffMachineType_RISCV128: result = 18; break;
case SYMS_CoffMachineType_SH3: result = 19; break;
case SYMS_CoffMachineType_SH3DSP: result = 20; break;
case SYMS_CoffMachineType_SH4: result = 21; break;
case SYMS_CoffMachineType_SH5: result = 22; break;
case SYMS_CoffMachineType_THUMB: result = 23; break;
case SYMS_CoffMachineType_WCEMIPSV2: result = 24; break;
}
return(result);
}
SYMS_API SYMS_U64
syms_enum_index_from_coffsectionalign(SYMS_U64 v){
SYMS_U64 result = 0;
switch ((SYMS_U32)v){
default: break;
case SYMS_CoffSectionAlign_1BYTES: result = 0; break;
case SYMS_CoffSectionAlign_2BYTES: result = 1; break;
case SYMS_CoffSectionAlign_4BYTES: result = 2; break;
case SYMS_CoffSectionAlign_8BYTES: result = 3; break;
case SYMS_CoffSectionAlign_16BYTES: result = 4; break;
case SYMS_CoffSectionAlign_32BYTES: result = 5; break;
case SYMS_CoffSectionAlign_64BYTES: result = 6; break;
case SYMS_CoffSectionAlign_128BYTES: result = 7; break;
case SYMS_CoffSectionAlign_256BYTES: result = 8; break;
case SYMS_CoffSectionAlign_512BYTES: result = 9; break;
case SYMS_CoffSectionAlign_1024BYTES: result = 10; break;
case SYMS_CoffSectionAlign_2048BYTES: result = 11; break;
case SYMS_CoffSectionAlign_4096BYTES: result = 12; break;
case SYMS_CoffSectionAlign_8192BYTES: result = 13; break;
}
return(result);
}
// syms_enum_index_from_coffreloctypex64 - skipped identity mapping
SYMS_API SYMS_U64
syms_enum_index_from_coffreloctypex86(SYMS_U64 v){
SYMS_U64 result = 0;
switch ((SYMS_U16)v){
default: break;
case SYMS_CoffRelocTypeX86_ABS: result = 0; break;
case SYMS_CoffRelocTypeX86_DIR16: result = 1; break;
case SYMS_CoffRelocTypeX86_REL16: result = 2; break;
case SYMS_CoffRelocTypeX86_UNKNOWN0: result = 3; break;
case SYMS_CoffRelocTypeX86_UNKNOWN2: result = 4; break;
case SYMS_CoffRelocTypeX86_UNKNOWN3: result = 5; break;
case SYMS_CoffRelocTypeX86_DIR32: result = 6; break;
case SYMS_CoffRelocTypeX86_DIR32NB: result = 7; break;
case SYMS_CoffRelocTypeX86_SEG12: result = 8; break;
case SYMS_CoffRelocTypeX86_SECTION: result = 9; break;
case SYMS_CoffRelocTypeX86_SECREL: result = 10; break;
case SYMS_CoffRelocTypeX86_TOKEN: result = 11; break;
case SYMS_CoffRelocTypeX86_SECREL7: result = 12; break;
case SYMS_CoffRelocTypeX86_UNKNOWN4: result = 13; break;
case SYMS_CoffRelocTypeX86_UNKNOWN5: result = 14; break;
case SYMS_CoffRelocTypeX86_UNKNOWN6: result = 15; break;
case SYMS_CoffRelocTypeX86_UNKNOWN7: result = 16; break;
case SYMS_CoffRelocTypeX86_UNKNOWN8: result = 17; break;
case SYMS_CoffRelocTypeX86_UNKNOWN9: result = 18; break;
case SYMS_CoffRelocTypeX86_REL32: result = 19; break;
}
return(result);
}
SYMS_API SYMS_U64
syms_enum_index_from_coffreloctypearm(SYMS_U64 v){
SYMS_U64 result = 0;
switch ((SYMS_U16)v){
default: break;
case SYMS_CoffRelocTypeARM_ABS: result = 0; break;
case SYMS_CoffRelocTypeARM_ADDR32: result = 1; break;
case SYMS_CoffRelocTypeARM_ADDR32NB: result = 2; break;
case SYMS_CoffRelocTypeARM_BRANCH24: result = 3; break;
case SYMS_CoffRelocTypeARM_BRANCH11: result = 4; break;
case SYMS_CoffRelocTypeARM_UNKNOWN1: result = 5; break;
case SYMS_CoffRelocTypeARM_UNKNOWN2: result = 6; break;
case SYMS_CoffRelocTypeARM_UNKNOWN3: result = 7; break;
case SYMS_CoffRelocTypeARM_UNKNOWN4: result = 8; break;
case SYMS_CoffRelocTypeARM_UNKNOWN5: result = 9; break;
case SYMS_CoffRelocTypeARM_REL32: result = 10; break;
case SYMS_CoffRelocTypeARM_SECTION: result = 11; break;
case SYMS_CoffRelocTypeARM_SECREL: result = 12; break;
case SYMS_CoffRelocTypeARM_MOV32: result = 13; break;
case SYMS_CoffRelocTypeARM_THUMB_MOV32: result = 14; break;
case SYMS_CoffRelocTypeARM_THUMB_BRANCH20: result = 15; break;
case SYMS_CoffRelocTypeARM_UNUSED: result = 16; break;
case SYMS_CoffRelocTypeARM_THUMB_BRANCH24: result = 17; break;
case SYMS_CoffRelocTypeARM_THUMB_BLX23: result = 18; break;
case SYMS_CoffRelocTypeARM_PAIR: result = 19; break;
}
return(result);
}
SYMS_API SYMS_U64
syms_enum_index_from_coffreloctypearm64(SYMS_U64 v){
SYMS_U64 result = 0;
switch ((SYMS_U16)v){
default: break;
case SYMS_CoffRelocTypeARM64_ABS: result = 0; break;
case SYMS_CoffRelocTypeARM64_ADDR32: result = 1; break;
case SYMS_CoffRelocTypeARM64_ADDR32NB: result = 2; break;
case SYMS_CoffRelocTypeARM64_BRANCH26: result = 3; break;
case SYMS_CoffRelocTypeARM64_PAGEBASE_REL21: result = 4; break;
case SYMS_CoffRelocTypeARM64_REL21: result = 5; break;
case SYMS_CoffRelocTypeARM64_PAGEOFFSET_12A: result = 6; break;
case SYMS_CoffRelocTypeARM64_SECREL: result = 7; break;
case SYMS_CoffRelocTypeARM64_SECREL_LOW12A: result = 8; break;
case SYMS_CoffRelocTypeARM64_SECREL_HIGH12A: result = 9; break;
case SYMS_CoffRelocTypeARM64_SECREL_LOW12L: result = 10; break;
case SYMS_CoffRelocTypeARM64_TOKEN: result = 11; break;
case SYMS_CoffRelocTypeARM64_SECTION: result = 12; break;
case SYMS_CoffRelocTypeARM64_ADDR64: result = 13; break;
case SYMS_CoffRelocTypeARM64_BRANCH19: result = 14; break;
case SYMS_CoffRelocTypeARM64_BRANCH14: result = 15; break;
case SYMS_CoffRelocTypeARM64_REL32: result = 16; break;
}
return(result);
}
// syms_enum_index_from_coffsymtype - skipped identity mapping
SYMS_API SYMS_U64
syms_enum_index_from_coffsymstorageclass(SYMS_U64 v){
SYMS_U64 result = 0;
switch ((SYMS_U8)v){
default: break;
case SYMS_CoffSymStorageClass_END_OF_FUNCTION: result = 0; break;
case SYMS_CoffSymStorageClass_NULL: result = 1; break;
case SYMS_CoffSymStorageClass_AUTOMATIC: result = 2; break;
case SYMS_CoffSymStorageClass_EXTERNAL: result = 3; break;
case SYMS_CoffSymStorageClass_STATIC: result = 4; break;
case SYMS_CoffSymStorageClass_REGISTER: result = 5; break;
case SYMS_CoffSymStorageClass_EXTERNAL_DEF: result = 6; break;
case SYMS_CoffSymStorageClass_LABEL: result = 7; break;
case SYMS_CoffSymStorageClass_UNDEFINED_LABEL: result = 8; break;
case SYMS_CoffSymStorageClass_MEMBER_OF_STRUCT: result = 9; break;
case SYMS_CoffSymStorageClass_ARGUMENT: result = 10; break;
case SYMS_CoffSymStorageClass_STRUCT_TAG: result = 11; break;
case SYMS_CoffSymStorageClass_MEMBER_OF_UNION: result = 12; break;
case SYMS_CoffSymStorageClass_UNION_TAG: result = 13; break;
case SYMS_CoffSymStorageClass_TYPE_DEFINITION: result = 14; break;
case SYMS_CoffSymStorageClass_UNDEFINED_STATIC: result = 15; break;
case SYMS_CoffSymStorageClass_ENUM_TAG: result = 16; break;
case SYMS_CoffSymStorageClass_MEMBER_OF_ENUM: result = 17; break;
case SYMS_CoffSymStorageClass_REGISTER_PARAM: result = 18; break;
case SYMS_CoffSymStorageClass_BIT_FIELD: result = 19; break;
case SYMS_CoffSymStorageClass_BLOCK: result = 20; break;
case SYMS_CoffSymStorageClass_FUNCTION: result = 21; break;
case SYMS_CoffSymStorageClass_END_OF_STRUCT: result = 22; break;
case SYMS_CoffSymStorageClass_FILE: result = 23; break;
case SYMS_CoffSymStorageClass_SECTION: result = 24; break;
case SYMS_CoffSymStorageClass_WEAK_EXTERNAL: result = 25; break;
case SYMS_CoffSymStorageClass_CLR_TOKEN: result = 26; break;
}
return(result);
}
SYMS_API SYMS_U64
syms_enum_index_from_coffsymsecnumber(SYMS_U64 v){
SYMS_U64 result = 0;
switch ((SYMS_U16)v){
default: break;
case SYMS_CoffSymSecNumber_NUMBER_UNDEFINED: result = 0; break;
case SYMS_CoffSymSecNumber_ABSOLUTE: result = 1; break;
case SYMS_CoffSymSecNumber_DEBUG: result = 2; break;
}
return(result);
}
SYMS_API SYMS_U64
syms_enum_index_from_coffsymdtype(SYMS_U64 v){
SYMS_U64 result = 0;
switch ((SYMS_U8)v){
default: break;
case SYMS_CoffSymDType_NULL: result = 0; break;
case SYMS_CoffSymDType_PTR: result = 1; break;
case SYMS_CoffSymDType_FUNC: result = 2; break;
case SYMS_CoffSymDType_ARRAY: result = 3; break;
}
return(result);
}
SYMS_API SYMS_U64
syms_enum_index_from_coff_weak_ext_type(SYMS_U64 v){
SYMS_U64 result = 0;
switch ((SYMS_U32)v){
default: break;
case SYMS_CoffWeakExtType_NOLIBRARY: result = 0; break;
case SYMS_CoffWeakExtType_SEARCH_LIBRARY: result = 1; break;
case SYMS_CoffWeakExtType_SEARCH_ALIAS: result = 2; break;
}
return(result);
}
// syms_enum_index_from_coff_import_header_type - skipped identity mapping
// syms_enum_index_from_coff_import_header_name_type - skipped identity mapping
// syms_enum_index_from_coff_comdat_select_type - skipped identity mapping

//~ generated from code at syms/metaprogram/syms_metaprogram_serial.c:1322
SYMS_SerialFlag _syms_serial_members_for_SYMS_CoffFlags[] = {
{ { (SYMS_U8*)"RELOC_STRIPPED", 14 }, &_syms_serial_type_SYMS_U16, 0x1, 0 },
{ { (SYMS_U8*)"EXECUTABLE_IMAGE", 16 }, &_syms_serial_type_SYMS_U16, 0x1, 1 },
{ { (SYMS_U8*)"LINE_NUMS_STRIPPED", 18 }, &_syms_serial_type_SYMS_U16, 0x1, 2 },
{ { (SYMS_U8*)"SYM_STRIPPED", 12 }, &_syms_serial_type_SYMS_U16, 0x1, 3 },
{ { (SYMS_U8*)"RESERVED_0", 10 }, &_syms_serial_type_SYMS_U16, 0x1, 4 },
{ { (SYMS_U8*)"LARGE_ADDRESS_AWARE", 19 }, &_syms_serial_type_SYMS_U16, 0x1, 5 },
{ { (SYMS_U8*)"RESERVED_1", 10 }, &_syms_serial_type_SYMS_U16, 0x1, 6 },
{ { (SYMS_U8*)"RESERVED_2", 10 }, &_syms_serial_type_SYMS_U16, 0x1, 7 },
{ { (SYMS_U8*)"32BIT_MACHINE", 13 }, &_syms_serial_type_SYMS_U16, 0x1, 8 },
{ { (SYMS_U8*)"DEBUG_STRIPPED", 14 }, &_syms_serial_type_SYMS_U16, 0x1, 9 },
{ { (SYMS_U8*)"REMOVABLE_RUN_FROM_SWAP", 23 }, &_syms_serial_type_SYMS_U16, 0x1, 10 },
{ { (SYMS_U8*)"NET_RUN_FROM_SWAP", 17 }, &_syms_serial_type_SYMS_U16, 0x1, 11 },
{ { (SYMS_U8*)"SYSTEM", 6 }, &_syms_serial_type_SYMS_U16, 0x1, 12 },
{ { (SYMS_U8*)"DLL", 3 }, &_syms_serial_type_SYMS_U16, 0x1, 13 },
{ { (SYMS_U8*)"UP_SYSTEM_ONLY", 14 }, &_syms_serial_type_SYMS_U16, 0x1, 14 },
{ { (SYMS_U8*)"BYTES_RESERVED_HI", 17 }, &_syms_serial_type_SYMS_U16, 0x1, 15 },
};
SYMS_SerialValue _syms_serial_members_for_SYMS_CoffMachineType[] = {
{ { (SYMS_U8*)"UNKNOWN", 7 }, (SYMS_U64)SYMS_CoffMachineType_UNKNOWN },
{ { (SYMS_U8*)"X86", 3 }, (SYMS_U64)SYMS_CoffMachineType_X86 },
{ { (SYMS_U8*)"X64", 3 }, (SYMS_U64)SYMS_CoffMachineType_X64 },
{ { (SYMS_U8*)"ARM33", 5 }, (SYMS_U64)SYMS_CoffMachineType_ARM33 },
{ { (SYMS_U8*)"ARM", 3 }, (SYMS_U64)SYMS_CoffMachineType_ARM },
{ { (SYMS_U8*)"ARM64", 5 }, (SYMS_U64)SYMS_CoffMachineType_ARM64 },
{ { (SYMS_U8*)"ARMNT", 5 }, (SYMS_U64)SYMS_CoffMachineType_ARMNT },
{ { (SYMS_U8*)"EBC", 3 }, (SYMS_U64)SYMS_CoffMachineType_EBC },
{ { (SYMS_U8*)"IA64", 4 }, (SYMS_U64)SYMS_CoffMachineType_IA64 },
{ { (SYMS_U8*)"M32R", 4 }, (SYMS_U64)SYMS_CoffMachineType_M32R },
{ { (SYMS_U8*)"MIPS16", 6 }, (SYMS_U64)SYMS_CoffMachineType_MIPS16 },
{ { (SYMS_U8*)"MIPSFPU", 7 }, (SYMS_U64)SYMS_CoffMachineType_MIPSFPU },
{ { (SYMS_U8*)"MIPSFPU16", 9 }, (SYMS_U64)SYMS_CoffMachineType_MIPSFPU16 },
{ { (SYMS_U8*)"POWERPC", 7 }, (SYMS_U64)SYMS_CoffMachineType_POWERPC },
{ { (SYMS_U8*)"POWERPCFP", 9 }, (SYMS_U64)SYMS_CoffMachineType_POWERPCFP },
{ { (SYMS_U8*)"R4000", 5 }, (SYMS_U64)SYMS_CoffMachineType_R4000 },
{ { (SYMS_U8*)"RISCV32", 7 }, (SYMS_U64)SYMS_CoffMachineType_RISCV32 },
{ { (SYMS_U8*)"RISCV64", 7 }, (SYMS_U64)SYMS_CoffMachineType_RISCV64 },
{ { (SYMS_U8*)"RISCV128", 8 }, (SYMS_U64)SYMS_CoffMachineType_RISCV128 },
{ { (SYMS_U8*)"SH3", 3 }, (SYMS_U64)SYMS_CoffMachineType_SH3 },
{ { (SYMS_U8*)"SH3DSP", 6 }, (SYMS_U64)SYMS_CoffMachineType_SH3DSP },
{ { (SYMS_U8*)"SH4", 3 }, (SYMS_U64)SYMS_CoffMachineType_SH4 },
{ { (SYMS_U8*)"SH5", 3 }, (SYMS_U64)SYMS_CoffMachineType_SH5 },
{ { (SYMS_U8*)"THUMB", 5 }, (SYMS_U64)SYMS_CoffMachineType_THUMB },
{ { (SYMS_U8*)"WCEMIPSV2", 9 }, (SYMS_U64)SYMS_CoffMachineType_WCEMIPSV2 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_CoffHeader[] = {
{ {(SYMS_U8*)"machine", 7}, &_syms_serial_type_SYMS_CoffMachineType, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"section_count", 13}, &_syms_serial_type_SYMS_U16, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"time_date_stamp", 15}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"pointer_to_symbol_table", 23}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"number_of_symbols", 17}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"size_of_optional_header", 23}, &_syms_serial_type_SYMS_U16, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"flags", 5}, &_syms_serial_type_SYMS_CoffFlags, SYMS_SerialWidthKind_Null, 0 },
};
SYMS_SerialValue _syms_serial_members_for_SYMS_CoffSectionAlign[] = {
{ { (SYMS_U8*)"1BYTES", 6 }, (SYMS_U64)SYMS_CoffSectionAlign_1BYTES },
{ { (SYMS_U8*)"2BYTES", 6 }, (SYMS_U64)SYMS_CoffSectionAlign_2BYTES },
{ { (SYMS_U8*)"4BYTES", 6 }, (SYMS_U64)SYMS_CoffSectionAlign_4BYTES },
{ { (SYMS_U8*)"8BYTES", 6 }, (SYMS_U64)SYMS_CoffSectionAlign_8BYTES },
{ { (SYMS_U8*)"16BYTES", 7 }, (SYMS_U64)SYMS_CoffSectionAlign_16BYTES },
{ { (SYMS_U8*)"32BYTES", 7 }, (SYMS_U64)SYMS_CoffSectionAlign_32BYTES },
{ { (SYMS_U8*)"64BYTES", 7 }, (SYMS_U64)SYMS_CoffSectionAlign_64BYTES },
{ { (SYMS_U8*)"128BYTES", 8 }, (SYMS_U64)SYMS_CoffSectionAlign_128BYTES },
{ { (SYMS_U8*)"256BYTES", 8 }, (SYMS_U64)SYMS_CoffSectionAlign_256BYTES },
{ { (SYMS_U8*)"512BYTES", 8 }, (SYMS_U64)SYMS_CoffSectionAlign_512BYTES },
{ { (SYMS_U8*)"1024BYTES", 9 }, (SYMS_U64)SYMS_CoffSectionAlign_1024BYTES },
{ { (SYMS_U8*)"2048BYTES", 9 }, (SYMS_U64)SYMS_CoffSectionAlign_2048BYTES },
{ { (SYMS_U8*)"4096BYTES", 9 }, (SYMS_U64)SYMS_CoffSectionAlign_4096BYTES },
{ { (SYMS_U8*)"8192BYTES", 9 }, (SYMS_U64)SYMS_CoffSectionAlign_8192BYTES },
};
SYMS_SerialFlag _syms_serial_members_for_SYMS_CoffSectionFlags[] = {
{ { (SYMS_U8*)"TYPE_NO_PAD", 11 }, &_syms_serial_type_SYMS_U32, 0x1, 3 },
{ { (SYMS_U8*)"Contains Code", 13 }, &_syms_serial_type_SYMS_U32, 0x1, 5 },
{ { (SYMS_U8*)"Initialized Data", 16 }, &_syms_serial_type_SYMS_U32, 0x1, 6 },
{ { (SYMS_U8*)"Uninitialized Data", 18 }, &_syms_serial_type_SYMS_U32, 0x1, 7 },
{ { (SYMS_U8*)"LNK_OTHER", 9 }, &_syms_serial_type_SYMS_U32, 0x1, 8 },
{ { (SYMS_U8*)"LNK_INFO", 8 }, &_syms_serial_type_SYMS_U32, 0x1, 9 },
{ { (SYMS_U8*)"LNK_REMOVE", 10 }, &_syms_serial_type_SYMS_U32, 0x1, 11 },
{ { (SYMS_U8*)"COMDAT", 6 }, &_syms_serial_type_SYMS_U32, 0x1, 12 },
{ { (SYMS_U8*)"GPREL", 5 }, &_syms_serial_type_SYMS_U32, 0x1, 15 },
{ { (SYMS_U8*)"16-bit MODE", 11 }, &_syms_serial_type_SYMS_U32, 0x1, 17 },
{ { (SYMS_U8*)"Locked", 6 }, &_syms_serial_type_SYMS_U32, 0x1, 18 },
{ { (SYMS_U8*)"Preload", 7 }, &_syms_serial_type_SYMS_U32, 0x1, 19 },
{ { (SYMS_U8*)"ALIGN", 5 }, &_syms_serial_type_SYMS_CoffSectionAlign, 0xf, 20 },
{ { (SYMS_U8*)"NRELOC OVFL", 11 }, &_syms_serial_type_SYMS_U32, 0x1, 24 },
{ { (SYMS_U8*)"Discardable", 11 }, &_syms_serial_type_SYMS_U32, 0x1, 25 },
{ { (SYMS_U8*)"Cannot be Cached", 16 }, &_syms_serial_type_SYMS_U32, 0x1, 26 },
{ { (SYMS_U8*)"Not Pageable", 12 }, &_syms_serial_type_SYMS_U32, 0x1, 27 },
{ { (SYMS_U8*)"Shared", 6 }, &_syms_serial_type_SYMS_U32, 0x1, 28 },
{ { (SYMS_U8*)"Execute", 7 }, &_syms_serial_type_SYMS_U32, 0x1, 29 },
{ { (SYMS_U8*)"Read", 4 }, &_syms_serial_type_SYMS_U32, 0x1, 30 },
{ { (SYMS_U8*)"Write", 5 }, &_syms_serial_type_SYMS_U32, 0x1, 31 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_CoffSectionHeader[] = {
{ {(SYMS_U8*)"name", 4}, &_syms_serial_type_SYMS_U8, SYMS_SerialWidthKind_Fixed, 8 },
{ {(SYMS_U8*)"virt_size", 9}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"virt_off", 8}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"file_size", 9}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"file_off", 8}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"relocs_file_off", 15}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"lines_file_off", 14}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"reloc_count", 11}, &_syms_serial_type_SYMS_U16, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"line_count", 10}, &_syms_serial_type_SYMS_U16, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"flags", 5}, &_syms_serial_type_SYMS_CoffSectionFlags, SYMS_SerialWidthKind_Null, 0 },
};
SYMS_SerialValue _syms_serial_members_for_SYMS_CoffRelocTypeX64[] = {
{ { (SYMS_U8*)"ABS", 3 }, (SYMS_U64)SYMS_CoffRelocTypeX64_ABS },
{ { (SYMS_U8*)"ADDR64", 6 }, (SYMS_U64)SYMS_CoffRelocTypeX64_ADDR64 },
{ { (SYMS_U8*)"ADDR32", 6 }, (SYMS_U64)SYMS_CoffRelocTypeX64_ADDR32 },
{ { (SYMS_U8*)"ADDR32NB", 8 }, (SYMS_U64)SYMS_CoffRelocTypeX64_ADDR32NB },
{ { (SYMS_U8*)"REL32", 5 }, (SYMS_U64)SYMS_CoffRelocTypeX64_REL32 },
{ { (SYMS_U8*)"REL32_1", 7 }, (SYMS_U64)SYMS_CoffRelocTypeX64_REL32_1 },
{ { (SYMS_U8*)"REL32_2", 7 }, (SYMS_U64)SYMS_CoffRelocTypeX64_REL32_2 },
{ { (SYMS_U8*)"REL32_3", 7 }, (SYMS_U64)SYMS_CoffRelocTypeX64_REL32_3 },
{ { (SYMS_U8*)"REL32_4", 7 }, (SYMS_U64)SYMS_CoffRelocTypeX64_REL32_4 },
{ { (SYMS_U8*)"REL32_5", 7 }, (SYMS_U64)SYMS_CoffRelocTypeX64_REL32_5 },
{ { (SYMS_U8*)"SECTION", 7 }, (SYMS_U64)SYMS_CoffRelocTypeX64_SECTION },
{ { (SYMS_U8*)"SECREL", 6 }, (SYMS_U64)SYMS_CoffRelocTypeX64_SECREL },
{ { (SYMS_U8*)"SECREL7", 7 }, (SYMS_U64)SYMS_CoffRelocTypeX64_SECREL7 },
{ { (SYMS_U8*)"TOKEN", 5 }, (SYMS_U64)SYMS_CoffRelocTypeX64_TOKEN },
{ { (SYMS_U8*)"SREL32", 6 }, (SYMS_U64)SYMS_CoffRelocTypeX64_SREL32 },
{ { (SYMS_U8*)"PAIR", 4 }, (SYMS_U64)SYMS_CoffRelocTypeX64_PAIR },
{ { (SYMS_U8*)"SSPAN32", 7 }, (SYMS_U64)SYMS_CoffRelocTypeX64_SSPAN32 },
};
SYMS_SerialValue _syms_serial_members_for_SYMS_CoffRelocTypeX86[] = {
{ { (SYMS_U8*)"ABS", 3 }, (SYMS_U64)SYMS_CoffRelocTypeX86_ABS },
{ { (SYMS_U8*)"DIR16", 5 }, (SYMS_U64)SYMS_CoffRelocTypeX86_DIR16 },
{ { (SYMS_U8*)"REL16", 5 }, (SYMS_U64)SYMS_CoffRelocTypeX86_REL16 },
{ { (SYMS_U8*)"UNKNOWN0", 8 }, (SYMS_U64)SYMS_CoffRelocTypeX86_UNKNOWN0 },
{ { (SYMS_U8*)"UNKNOWN2", 8 }, (SYMS_U64)SYMS_CoffRelocTypeX86_UNKNOWN2 },
{ { (SYMS_U8*)"UNKNOWN3", 8 }, (SYMS_U64)SYMS_CoffRelocTypeX86_UNKNOWN3 },
{ { (SYMS_U8*)"DIR32", 5 }, (SYMS_U64)SYMS_CoffRelocTypeX86_DIR32 },
{ { (SYMS_U8*)"DIR32NB", 7 }, (SYMS_U64)SYMS_CoffRelocTypeX86_DIR32NB },
{ { (SYMS_U8*)"SEG12", 5 }, (SYMS_U64)SYMS_CoffRelocTypeX86_SEG12 },
{ { (SYMS_U8*)"SECTION", 7 }, (SYMS_U64)SYMS_CoffRelocTypeX86_SECTION },
{ { (SYMS_U8*)"SECREL", 6 }, (SYMS_U64)SYMS_CoffRelocTypeX86_SECREL },
{ { (SYMS_U8*)"TOKEN", 5 }, (SYMS_U64)SYMS_CoffRelocTypeX86_TOKEN },
{ { (SYMS_U8*)"SECREL7", 7 }, (SYMS_U64)SYMS_CoffRelocTypeX86_SECREL7 },
{ { (SYMS_U8*)"UNKNOWN4", 8 }, (SYMS_U64)SYMS_CoffRelocTypeX86_UNKNOWN4 },
{ { (SYMS_U8*)"UNKNOWN5", 8 }, (SYMS_U64)SYMS_CoffRelocTypeX86_UNKNOWN5 },
{ { (SYMS_U8*)"UNKNOWN6", 8 }, (SYMS_U64)SYMS_CoffRelocTypeX86_UNKNOWN6 },
{ { (SYMS_U8*)"UNKNOWN7", 8 }, (SYMS_U64)SYMS_CoffRelocTypeX86_UNKNOWN7 },
{ { (SYMS_U8*)"UNKNOWN8", 8 }, (SYMS_U64)SYMS_CoffRelocTypeX86_UNKNOWN8 },
{ { (SYMS_U8*)"UNKNOWN9", 8 }, (SYMS_U64)SYMS_CoffRelocTypeX86_UNKNOWN9 },
{ { (SYMS_U8*)"REL32", 5 }, (SYMS_U64)SYMS_CoffRelocTypeX86_REL32 },
};
SYMS_SerialValue _syms_serial_members_for_SYMS_CoffRelocTypeARM[] = {
{ { (SYMS_U8*)"ABS", 3 }, (SYMS_U64)SYMS_CoffRelocTypeARM_ABS },
{ { (SYMS_U8*)"ADDR32", 6 }, (SYMS_U64)SYMS_CoffRelocTypeARM_ADDR32 },
{ { (SYMS_U8*)"ADDR32NB", 8 }, (SYMS_U64)SYMS_CoffRelocTypeARM_ADDR32NB },
{ { (SYMS_U8*)"BRANCH24", 8 }, (SYMS_U64)SYMS_CoffRelocTypeARM_BRANCH24 },
{ { (SYMS_U8*)"BRANCH11", 8 }, (SYMS_U64)SYMS_CoffRelocTypeARM_BRANCH11 },
{ { (SYMS_U8*)"UNKNOWN1", 8 }, (SYMS_U64)SYMS_CoffRelocTypeARM_UNKNOWN1 },
{ { (SYMS_U8*)"UNKNOWN2", 8 }, (SYMS_U64)SYMS_CoffRelocTypeARM_UNKNOWN2 },
{ { (SYMS_U8*)"UNKNOWN3", 8 }, (SYMS_U64)SYMS_CoffRelocTypeARM_UNKNOWN3 },
{ { (SYMS_U8*)"UNKNOWN4", 8 }, (SYMS_U64)SYMS_CoffRelocTypeARM_UNKNOWN4 },
{ { (SYMS_U8*)"UNKNOWN5", 8 }, (SYMS_U64)SYMS_CoffRelocTypeARM_UNKNOWN5 },
{ { (SYMS_U8*)"REL32", 5 }, (SYMS_U64)SYMS_CoffRelocTypeARM_REL32 },
{ { (SYMS_U8*)"SECTION", 7 }, (SYMS_U64)SYMS_CoffRelocTypeARM_SECTION },
{ { (SYMS_U8*)"SECREL", 6 }, (SYMS_U64)SYMS_CoffRelocTypeARM_SECREL },
{ { (SYMS_U8*)"MOV32", 5 }, (SYMS_U64)SYMS_CoffRelocTypeARM_MOV32 },
{ { (SYMS_U8*)"THUMB_MOV32", 11 }, (SYMS_U64)SYMS_CoffRelocTypeARM_THUMB_MOV32 },
{ { (SYMS_U8*)"THUMB_BRANCH20", 14 }, (SYMS_U64)SYMS_CoffRelocTypeARM_THUMB_BRANCH20 },
{ { (SYMS_U8*)"UNUSED", 6 }, (SYMS_U64)SYMS_CoffRelocTypeARM_UNUSED },
{ { (SYMS_U8*)"THUMB_BRANCH24", 14 }, (SYMS_U64)SYMS_CoffRelocTypeARM_THUMB_BRANCH24 },
{ { (SYMS_U8*)"THUMB_BLX23", 11 }, (SYMS_U64)SYMS_CoffRelocTypeARM_THUMB_BLX23 },
{ { (SYMS_U8*)"PAIR", 4 }, (SYMS_U64)SYMS_CoffRelocTypeARM_PAIR },
};
SYMS_SerialValue _syms_serial_members_for_SYMS_CoffRelocTypeARM64[] = {
{ { (SYMS_U8*)"ABS", 3 }, (SYMS_U64)SYMS_CoffRelocTypeARM64_ABS },
{ { (SYMS_U8*)"ADDR32", 6 }, (SYMS_U64)SYMS_CoffRelocTypeARM64_ADDR32 },
{ { (SYMS_U8*)"ADDR32NB", 8 }, (SYMS_U64)SYMS_CoffRelocTypeARM64_ADDR32NB },
{ { (SYMS_U8*)"BRANCH26", 8 }, (SYMS_U64)SYMS_CoffRelocTypeARM64_BRANCH26 },
{ { (SYMS_U8*)"PAGEBASE_REL21", 14 }, (SYMS_U64)SYMS_CoffRelocTypeARM64_PAGEBASE_REL21 },
{ { (SYMS_U8*)"REL21", 5 }, (SYMS_U64)SYMS_CoffRelocTypeARM64_REL21 },
{ { (SYMS_U8*)"PAGEOFFSET_12A", 14 }, (SYMS_U64)SYMS_CoffRelocTypeARM64_PAGEOFFSET_12A },
{ { (SYMS_U8*)"SECREL", 6 }, (SYMS_U64)SYMS_CoffRelocTypeARM64_SECREL },
{ { (SYMS_U8*)"SECREL_LOW12A", 13 }, (SYMS_U64)SYMS_CoffRelocTypeARM64_SECREL_LOW12A },
{ { (SYMS_U8*)"SECREL_HIGH12A", 14 }, (SYMS_U64)SYMS_CoffRelocTypeARM64_SECREL_HIGH12A },
{ { (SYMS_U8*)"SECREL_LOW12L", 13 }, (SYMS_U64)SYMS_CoffRelocTypeARM64_SECREL_LOW12L },
{ { (SYMS_U8*)"TOKEN", 5 }, (SYMS_U64)SYMS_CoffRelocTypeARM64_TOKEN },
{ { (SYMS_U8*)"SECTION", 7 }, (SYMS_U64)SYMS_CoffRelocTypeARM64_SECTION },
{ { (SYMS_U8*)"ADDR64", 6 }, (SYMS_U64)SYMS_CoffRelocTypeARM64_ADDR64 },
{ { (SYMS_U8*)"BRANCH19", 8 }, (SYMS_U64)SYMS_CoffRelocTypeARM64_BRANCH19 },
{ { (SYMS_U8*)"BRANCH14", 8 }, (SYMS_U64)SYMS_CoffRelocTypeARM64_BRANCH14 },
{ { (SYMS_U8*)"REL32", 5 }, (SYMS_U64)SYMS_CoffRelocTypeARM64_REL32 },
};
SYMS_SerialValue _syms_serial_members_for_SYMS_CoffSymType[] = {
{ { (SYMS_U8*)"NULL", 4 }, (SYMS_U64)SYMS_CoffSymType_NULL },
{ { (SYMS_U8*)"VOID", 4 }, (SYMS_U64)SYMS_CoffSymType_VOID },
{ { (SYMS_U8*)"CHAR", 4 }, (SYMS_U64)SYMS_CoffSymType_CHAR },
{ { (SYMS_U8*)"SHORT", 5 }, (SYMS_U64)SYMS_CoffSymType_SHORT },
{ { (SYMS_U8*)"INT", 3 }, (SYMS_U64)SYMS_CoffSymType_INT },
{ { (SYMS_U8*)"LONG", 4 }, (SYMS_U64)SYMS_CoffSymType_LONG },
{ { (SYMS_U8*)"FLOAT", 5 }, (SYMS_U64)SYMS_CoffSymType_FLOAT },
{ { (SYMS_U8*)"DOUBLE", 6 }, (SYMS_U64)SYMS_CoffSymType_DOUBLE },
{ { (SYMS_U8*)"STRUCT", 6 }, (SYMS_U64)SYMS_CoffSymType_STRUCT },
{ { (SYMS_U8*)"UNION", 5 }, (SYMS_U64)SYMS_CoffSymType_UNION },
{ { (SYMS_U8*)"ENUM", 4 }, (SYMS_U64)SYMS_CoffSymType_ENUM },
{ { (SYMS_U8*)"MOE", 3 }, (SYMS_U64)SYMS_CoffSymType_MOE },
{ { (SYMS_U8*)"BYTE", 4 }, (SYMS_U64)SYMS_CoffSymType_BYTE },
{ { (SYMS_U8*)"WORD", 4 }, (SYMS_U64)SYMS_CoffSymType_WORD },
{ { (SYMS_U8*)"UINT", 4 }, (SYMS_U64)SYMS_CoffSymType_UINT },
{ { (SYMS_U8*)"DWORD", 5 }, (SYMS_U64)SYMS_CoffSymType_DWORD },
};
SYMS_SerialValue _syms_serial_members_for_SYMS_CoffSymStorageClass[] = {
{ { (SYMS_U8*)"END_OF_FUNCTION", 15 }, (SYMS_U64)SYMS_CoffSymStorageClass_END_OF_FUNCTION },
{ { (SYMS_U8*)"NULL", 4 }, (SYMS_U64)SYMS_CoffSymStorageClass_NULL },
{ { (SYMS_U8*)"AUTOMATIC", 9 }, (SYMS_U64)SYMS_CoffSymStorageClass_AUTOMATIC },
{ { (SYMS_U8*)"EXTERNAL", 8 }, (SYMS_U64)SYMS_CoffSymStorageClass_EXTERNAL },
{ { (SYMS_U8*)"STATIC", 6 }, (SYMS_U64)SYMS_CoffSymStorageClass_STATIC },
{ { (SYMS_U8*)"REGISTER", 8 }, (SYMS_U64)SYMS_CoffSymStorageClass_REGISTER },
{ { (SYMS_U8*)"EXTERNAL_DEF", 12 }, (SYMS_U64)SYMS_CoffSymStorageClass_EXTERNAL_DEF },
{ { (SYMS_U8*)"LABEL", 5 }, (SYMS_U64)SYMS_CoffSymStorageClass_LABEL },
{ { (SYMS_U8*)"UNDEFINED_LABEL", 15 }, (SYMS_U64)SYMS_CoffSymStorageClass_UNDEFINED_LABEL },
{ { (SYMS_U8*)"MEMBER_OF_STRUCT", 16 }, (SYMS_U64)SYMS_CoffSymStorageClass_MEMBER_OF_STRUCT },
{ { (SYMS_U8*)"ARGUMENT", 8 }, (SYMS_U64)SYMS_CoffSymStorageClass_ARGUMENT },
{ { (SYMS_U8*)"STRUCT_TAG", 10 }, (SYMS_U64)SYMS_CoffSymStorageClass_STRUCT_TAG },
{ { (SYMS_U8*)"MEMBER_OF_UNION", 15 }, (SYMS_U64)SYMS_CoffSymStorageClass_MEMBER_OF_UNION },
{ { (SYMS_U8*)"UNION_TAG", 9 }, (SYMS_U64)SYMS_CoffSymStorageClass_UNION_TAG },
{ { (SYMS_U8*)"TYPE_DEFINITION", 15 }, (SYMS_U64)SYMS_CoffSymStorageClass_TYPE_DEFINITION },
{ { (SYMS_U8*)"UNDEFINED_STATIC", 16 }, (SYMS_U64)SYMS_CoffSymStorageClass_UNDEFINED_STATIC },
{ { (SYMS_U8*)"ENUM_TAG", 8 }, (SYMS_U64)SYMS_CoffSymStorageClass_ENUM_TAG },
{ { (SYMS_U8*)"MEMBER_OF_ENUM", 14 }, (SYMS_U64)SYMS_CoffSymStorageClass_MEMBER_OF_ENUM },
{ { (SYMS_U8*)"REGISTER_PARAM", 14 }, (SYMS_U64)SYMS_CoffSymStorageClass_REGISTER_PARAM },
{ { (SYMS_U8*)"BIT_FIELD", 9 }, (SYMS_U64)SYMS_CoffSymStorageClass_BIT_FIELD },
{ { (SYMS_U8*)"BLOCK", 5 }, (SYMS_U64)SYMS_CoffSymStorageClass_BLOCK },
{ { (SYMS_U8*)"FUNCTION", 8 }, (SYMS_U64)SYMS_CoffSymStorageClass_FUNCTION },
{ { (SYMS_U8*)"END_OF_STRUCT", 13 }, (SYMS_U64)SYMS_CoffSymStorageClass_END_OF_STRUCT },
{ { (SYMS_U8*)"FILE", 4 }, (SYMS_U64)SYMS_CoffSymStorageClass_FILE },
{ { (SYMS_U8*)"SECTION", 7 }, (SYMS_U64)SYMS_CoffSymStorageClass_SECTION },
{ { (SYMS_U8*)"WEAK_EXTERNAL", 13 }, (SYMS_U64)SYMS_CoffSymStorageClass_WEAK_EXTERNAL },
{ { (SYMS_U8*)"CLR_TOKEN", 9 }, (SYMS_U64)SYMS_CoffSymStorageClass_CLR_TOKEN },
};
SYMS_SerialValue _syms_serial_members_for_SYMS_CoffSymSecNumber[] = {
{ { (SYMS_U8*)"NUMBER_UNDEFINED", 16 }, (SYMS_U64)SYMS_CoffSymSecNumber_NUMBER_UNDEFINED },
{ { (SYMS_U8*)"ABSOLUTE", 8 }, (SYMS_U64)SYMS_CoffSymSecNumber_ABSOLUTE },
{ { (SYMS_U8*)"DEBUG", 5 }, (SYMS_U64)SYMS_CoffSymSecNumber_DEBUG },
};
SYMS_SerialValue _syms_serial_members_for_SYMS_CoffSymDType[] = {
{ { (SYMS_U8*)"NULL", 4 }, (SYMS_U64)SYMS_CoffSymDType_NULL },
{ { (SYMS_U8*)"PTR", 3 }, (SYMS_U64)SYMS_CoffSymDType_PTR },
{ { (SYMS_U8*)"FUNC", 4 }, (SYMS_U64)SYMS_CoffSymDType_FUNC },
{ { (SYMS_U8*)"ARRAY", 5 }, (SYMS_U64)SYMS_CoffSymDType_ARRAY },
};
SYMS_SerialValue _syms_serial_members_for_SYMS_CoffWeakExtType[] = {
{ { (SYMS_U8*)"NOLIBRARY", 9 }, (SYMS_U64)SYMS_CoffWeakExtType_NOLIBRARY },
{ { (SYMS_U8*)"SEARCH_LIBRARY", 14 }, (SYMS_U64)SYMS_CoffWeakExtType_SEARCH_LIBRARY },
{ { (SYMS_U8*)"SEARCH_ALIAS", 12 }, (SYMS_U64)SYMS_CoffWeakExtType_SEARCH_ALIAS },
};
SYMS_SerialValue _syms_serial_members_for_SYMS_CoffImportHeaderType[] = {
{ { (SYMS_U8*)"CODE", 4 }, (SYMS_U64)SYMS_CoffImportHeaderType_CODE },
{ { (SYMS_U8*)"DATA", 4 }, (SYMS_U64)SYMS_CoffImportHeaderType_DATA },
{ { (SYMS_U8*)"CONST", 5 }, (SYMS_U64)SYMS_CoffImportHeaderType_CONST },
};
SYMS_SerialValue _syms_serial_members_for_SYMS_CoffImportHeaderNameType[] = {
{ { (SYMS_U8*)"ORDINAL", 7 }, (SYMS_U64)SYMS_CoffImportHeaderNameType_ORDINAL },
{ { (SYMS_U8*)"NAME", 4 }, (SYMS_U64)SYMS_CoffImportHeaderNameType_NAME },
{ { (SYMS_U8*)"NAME_NOPREFIX", 13 }, (SYMS_U64)SYMS_CoffImportHeaderNameType_NAME_NOPREFIX },
{ { (SYMS_U8*)"UNDECORATE", 10 }, (SYMS_U64)SYMS_CoffImportHeaderNameType_UNDECORATE },
};
SYMS_SerialValue _syms_serial_members_for_SYMS_CoffComdatSelectType[] = {
{ { (SYMS_U8*)"NULL", 4 }, (SYMS_U64)SYMS_CoffComdatSelectType_NULL },
{ { (SYMS_U8*)"NODUPLICATES", 12 }, (SYMS_U64)SYMS_CoffComdatSelectType_NODUPLICATES },
{ { (SYMS_U8*)"ANY", 3 }, (SYMS_U64)SYMS_CoffComdatSelectType_ANY },
{ { (SYMS_U8*)"SAME_SIZE", 9 }, (SYMS_U64)SYMS_CoffComdatSelectType_SAME_SIZE },
{ { (SYMS_U8*)"EXACT_MATCH", 11 }, (SYMS_U64)SYMS_CoffComdatSelectType_EXACT_MATCH },
{ { (SYMS_U8*)"ASSOCIATIVE", 11 }, (SYMS_U64)SYMS_CoffComdatSelectType_ASSOCIATIVE },
{ { (SYMS_U8*)"LARGEST", 7 }, (SYMS_U64)SYMS_CoffComdatSelectType_LARGEST },
};

//~ generated from code at syms/metaprogram/syms_metaprogram_serial.c:1458
SYMS_SerialType _syms_serial_type_SYMS_CoffFlags = {
{(SYMS_U8*)"SYMS_CoffFlags", 14}, SYMS_SerialTypeKind_Flags, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CoffFlags), _syms_serial_members_for_SYMS_CoffFlags, sizeof(SYMS_CoffFlags), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CoffMachineType = {
{(SYMS_U8*)"SYMS_CoffMachineType", 20}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CoffMachineType), _syms_serial_members_for_SYMS_CoffMachineType, sizeof(SYMS_CoffMachineType), syms_enum_index_from_coffmachinetype
};
SYMS_SerialType _syms_serial_type_SYMS_CoffHeader = {
{(SYMS_U8*)"CoffHeader", 10}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CoffHeader), _syms_serial_members_for_SYMS_CoffHeader, sizeof(SYMS_CoffHeader), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CoffSectionAlign = {
{(SYMS_U8*)"SYMS_CoffSectionAlign", 21}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CoffSectionAlign), _syms_serial_members_for_SYMS_CoffSectionAlign, sizeof(SYMS_CoffSectionAlign), syms_enum_index_from_coffsectionalign
};
SYMS_SerialType _syms_serial_type_SYMS_CoffSectionFlags = {
{(SYMS_U8*)"SYMS_CoffSectionFlags", 21}, SYMS_SerialTypeKind_Flags, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CoffSectionFlags), _syms_serial_members_for_SYMS_CoffSectionFlags, sizeof(SYMS_CoffSectionFlags), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CoffSectionHeader = {
{(SYMS_U8*)"CoffSectionHeader", 17}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CoffSectionHeader), _syms_serial_members_for_SYMS_CoffSectionHeader, sizeof(SYMS_CoffSectionHeader), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CoffRelocTypeX64 = {
{(SYMS_U8*)"SYMS_CoffRelocTypeX64", 21}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CoffRelocTypeX64), _syms_serial_members_for_SYMS_CoffRelocTypeX64, sizeof(SYMS_CoffRelocTypeX64), syms_enum_index_from_value_identity
};
SYMS_SerialType _syms_serial_type_SYMS_CoffRelocTypeX86 = {
{(SYMS_U8*)"SYMS_CoffRelocTypeX86", 21}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CoffRelocTypeX86), _syms_serial_members_for_SYMS_CoffRelocTypeX86, sizeof(SYMS_CoffRelocTypeX86), syms_enum_index_from_coffreloctypex86
};
SYMS_SerialType _syms_serial_type_SYMS_CoffRelocTypeARM = {
{(SYMS_U8*)"SYMS_CoffRelocTypeARM", 21}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CoffRelocTypeARM), _syms_serial_members_for_SYMS_CoffRelocTypeARM, sizeof(SYMS_CoffRelocTypeARM), syms_enum_index_from_coffreloctypearm
};
SYMS_SerialType _syms_serial_type_SYMS_CoffRelocTypeARM64 = {
{(SYMS_U8*)"SYMS_CoffRelocTypeARM64", 23}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CoffRelocTypeARM64), _syms_serial_members_for_SYMS_CoffRelocTypeARM64, sizeof(SYMS_CoffRelocTypeARM64), syms_enum_index_from_coffreloctypearm64
};
SYMS_SerialType _syms_serial_type_SYMS_CoffSymType = {
{(SYMS_U8*)"SYMS_CoffSymType", 16}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CoffSymType), _syms_serial_members_for_SYMS_CoffSymType, sizeof(SYMS_CoffSymType), syms_enum_index_from_value_identity
};
SYMS_SerialType _syms_serial_type_SYMS_CoffSymStorageClass = {
{(SYMS_U8*)"SYMS_CoffSymStorageClass", 24}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CoffSymStorageClass), _syms_serial_members_for_SYMS_CoffSymStorageClass, sizeof(SYMS_CoffSymStorageClass), syms_enum_index_from_coffsymstorageclass
};
SYMS_SerialType _syms_serial_type_SYMS_CoffSymSecNumber = {
{(SYMS_U8*)"SYMS_CoffSymSecNumber", 21}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CoffSymSecNumber), _syms_serial_members_for_SYMS_CoffSymSecNumber, sizeof(SYMS_CoffSymSecNumber), syms_enum_index_from_coffsymsecnumber
};
SYMS_SerialType _syms_serial_type_SYMS_CoffSymDType = {
{(SYMS_U8*)"SYMS_CoffSymDType", 17}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CoffSymDType), _syms_serial_members_for_SYMS_CoffSymDType, sizeof(SYMS_CoffSymDType), syms_enum_index_from_coffsymdtype
};
SYMS_SerialType _syms_serial_type_SYMS_CoffWeakExtType = {
{(SYMS_U8*)"SYMS_CoffWeakExtType", 20}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CoffWeakExtType), _syms_serial_members_for_SYMS_CoffWeakExtType, sizeof(SYMS_CoffWeakExtType), syms_enum_index_from_coff_weak_ext_type
};
SYMS_SerialType _syms_serial_type_SYMS_CoffImportHeaderType = {
{(SYMS_U8*)"SYMS_CoffImportHeaderType", 25}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CoffImportHeaderType), _syms_serial_members_for_SYMS_CoffImportHeaderType, sizeof(SYMS_CoffImportHeaderType), syms_enum_index_from_value_identity
};
SYMS_SerialType _syms_serial_type_SYMS_CoffImportHeaderNameType = {
{(SYMS_U8*)"SYMS_CoffImportHeaderNameType", 29}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CoffImportHeaderNameType), _syms_serial_members_for_SYMS_CoffImportHeaderNameType, sizeof(SYMS_CoffImportHeaderNameType), syms_enum_index_from_value_identity
};
SYMS_SerialType _syms_serial_type_SYMS_CoffComdatSelectType = {
{(SYMS_U8*)"SYMS_CoffComdatSelectType", 25}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CoffComdatSelectType), _syms_serial_members_for_SYMS_CoffComdatSelectType, sizeof(SYMS_CoffComdatSelectType), syms_enum_index_from_value_identity
};

#endif // defined(SYMS_ENABLE_COFF_SERIAL_INFO)


////////////////////////////////
#if defined(SYMS_ENABLE_CV_SERIAL_INFO)

//~ generated from code at syms/metaprogram/syms_metaprogram_serial.c:1091
// syms_enum_index_from_cvbasicpointerkind - skipped identity mapping
SYMS_API SYMS_U64
syms_enum_index_from_cvarch(SYMS_U64 v){
SYMS_U64 result = 0;
switch ((SYMS_U16)v){
default: break;
case SYMS_CvArch_8080: result = 0; break;
case SYMS_CvArch_8086: result = 1; break;
case SYMS_CvArch_80286: result = 2; break;
case SYMS_CvArch_80386: result = 3; break;
case SYMS_CvArch_80486: result = 4; break;
case SYMS_CvArch_PENTIUM: result = 5; break;
case SYMS_CvArch_PENTIUMII: result = 6; break;
case SYMS_CvArch_PENTIUMIII: result = 8; break;
case SYMS_CvArch_MIPS: result = 9; break;
case SYMS_CvArch_MIPS16: result = 11; break;
case SYMS_CvArch_MIPS32: result = 12; break;
case SYMS_CvArch_MIPS64: result = 13; break;
case SYMS_CvArch_MIPSI: result = 14; break;
case SYMS_CvArch_MIPSII: result = 15; break;
case SYMS_CvArch_MIPSIII: result = 16; break;
case SYMS_CvArch_MIPSIV: result = 17; break;
case SYMS_CvArch_MIPSV: result = 18; break;
case SYMS_CvArch_M68000: result = 19; break;
case SYMS_CvArch_M68010: result = 20; break;
case SYMS_CvArch_M68020: result = 21; break;
case SYMS_CvArch_M68030: result = 22; break;
case SYMS_CvArch_M68040: result = 23; break;
case SYMS_CvArch_ALPHA: result = 24; break;
case SYMS_CvArch_ALPHA_21164: result = 26; break;
case SYMS_CvArch_ALPHA_21164A: result = 27; break;
case SYMS_CvArch_ALPHA_21264: result = 28; break;
case SYMS_CvArch_ALPHA_21364: result = 29; break;
case SYMS_CvArch_PPC601: result = 30; break;
case SYMS_CvArch_PPC603: result = 31; break;
case SYMS_CvArch_PPC604: result = 32; break;
case SYMS_CvArch_PPC620: result = 33; break;
case SYMS_CvArch_PPCFP: result = 34; break;
case SYMS_CvArch_PPCBE: result = 35; break;
case SYMS_CvArch_SH3: result = 36; break;
case SYMS_CvArch_SH3E: result = 37; break;
case SYMS_CvArch_SH3DSP: result = 38; break;
case SYMS_CvArch_SH4: result = 39; break;
case SYMS_CvArch_SHMEDIA: result = 40; break;
case SYMS_CvArch_ARM3: result = 41; break;
case SYMS_CvArch_ARM4: result = 42; break;
case SYMS_CvArch_ARM4T: result = 43; break;
case SYMS_CvArch_ARM5: result = 44; break;
case SYMS_CvArch_ARM5T: result = 45; break;
case SYMS_CvArch_ARM6: result = 46; break;
case SYMS_CvArch_ARM_XMAC: result = 47; break;
case SYMS_CvArch_ARM_WMMX: result = 48; break;
case SYMS_CvArch_ARM7: result = 49; break;
case SYMS_CvArch_OMNI: result = 50; break;
case SYMS_CvArch_IA64: result = 51; break;
case SYMS_CvArch_IA64_2: result = 53; break;
case SYMS_CvArch_CEE: result = 54; break;
case SYMS_CvArch_AM33: result = 55; break;
case SYMS_CvArch_M32R: result = 56; break;
case SYMS_CvArch_TRICORE: result = 57; break;
case SYMS_CvArch_X64: result = 58; break;
case SYMS_CvArch_EBC: result = 60; break;
case SYMS_CvArch_THUMB: result = 61; break;
case SYMS_CvArch_ARMNT: result = 62; break;
case SYMS_CvArch_ARM64: result = 63; break;
case SYMS_CvArch_D3D11_SHADER: result = 64; break;
}
return(result);
}
SYMS_API SYMS_U64
syms_enum_index_from_cv_all_reg(SYMS_U64 v){
SYMS_U64 result = 0;
switch ((SYMS_U16)v){
default: break;
case SYMS_CvAllReg_ERR: result = 0; break;
case SYMS_CvAllReg_TEB: result = 1; break;
case SYMS_CvAllReg_TIMER: result = 2; break;
case SYMS_CvAllReg_EFAD1: result = 3; break;
case SYMS_CvAllReg_EFAD2: result = 4; break;
case SYMS_CvAllReg_EFAD3: result = 5; break;
case SYMS_CvAllReg_VFRAME: result = 6; break;
case SYMS_CvAllReg_HANDLE: result = 7; break;
case SYMS_CvAllReg_PARAMS: result = 8; break;
case SYMS_CvAllReg_LOCALS: result = 9; break;
case SYMS_CvAllReg_TID: result = 10; break;
case SYMS_CvAllReg_ENV: result = 11; break;
case SYMS_CvAllReg_CMDLN: result = 12; break;
}
return(result);
}
SYMS_API SYMS_U64
syms_enum_index_from_cvregx86(SYMS_U64 v){
SYMS_U64 result = 0;
switch ((SYMS_U16)v){
default: break;
case SYMS_CvRegx86_NONE: result = 0; break;
case SYMS_CvRegx86_AL: result = 1; break;
case SYMS_CvRegx86_CL: result = 2; break;
case SYMS_CvRegx86_DL: result = 3; break;
case SYMS_CvRegx86_BL: result = 4; break;
case SYMS_CvRegx86_AH: result = 5; break;
case SYMS_CvRegx86_CH: result = 6; break;
case SYMS_CvRegx86_DH: result = 7; break;
case SYMS_CvRegx86_BH: result = 8; break;
case SYMS_CvRegx86_AX: result = 9; break;
case SYMS_CvRegx86_CX: result = 10; break;
case SYMS_CvRegx86_DX: result = 11; break;
case SYMS_CvRegx86_BX: result = 12; break;
case SYMS_CvRegx86_SP: result = 13; break;
case SYMS_CvRegx86_BP: result = 14; break;
case SYMS_CvRegx86_SI: result = 15; break;
case SYMS_CvRegx86_DI: result = 16; break;
case SYMS_CvRegx86_EAX: result = 17; break;
case SYMS_CvRegx86_ECX: result = 18; break;
case SYMS_CvRegx86_EDX: result = 19; break;
case SYMS_CvRegx86_EBX: result = 20; break;
case SYMS_CvRegx86_ESP: result = 21; break;
case SYMS_CvRegx86_EBP: result = 22; break;
case SYMS_CvRegx86_ESI: result = 23; break;
case SYMS_CvRegx86_EDI: result = 24; break;
case SYMS_CvRegx86_ES: result = 25; break;
case SYMS_CvRegx86_CS: result = 26; break;
case SYMS_CvRegx86_SS: result = 27; break;
case SYMS_CvRegx86_DS: result = 28; break;
case SYMS_CvRegx86_FS: result = 29; break;
case SYMS_CvRegx86_GS: result = 30; break;
case SYMS_CvRegx86_IP: result = 31; break;
case SYMS_CvRegx86_FLAGS: result = 32; break;
case SYMS_CvRegx86_EIP: result = 33; break;
case SYMS_CvRegx86_EFLAGS: result = 34; break;
case SYMS_CvRegx86_MM0: result = 35; break;
case SYMS_CvRegx86_MM1: result = 36; break;
case SYMS_CvRegx86_MM2: result = 37; break;
case SYMS_CvRegx86_MM3: result = 38; break;
case SYMS_CvRegx86_MM4: result = 39; break;
case SYMS_CvRegx86_MM5: result = 40; break;
case SYMS_CvRegx86_MM6: result = 41; break;
case SYMS_CvRegx86_MM7: result = 42; break;
case SYMS_CvRegx86_XMM0: result = 43; break;
case SYMS_CvRegx86_XMM1: result = 44; break;
case SYMS_CvRegx86_XMM2: result = 45; break;
case SYMS_CvRegx86_XMM3: result = 46; break;
case SYMS_CvRegx86_XMM4: result = 47; break;
case SYMS_CvRegx86_XMM5: result = 48; break;
case SYMS_CvRegx86_XMM6: result = 49; break;
case SYMS_CvRegx86_XMM7: result = 50; break;
case SYMS_CvRegx86_XMM00: result = 51; break;
case SYMS_CvRegx86_XMM01: result = 52; break;
case SYMS_CvRegx86_XMM02: result = 53; break;
case SYMS_CvRegx86_XMM03: result = 54; break;
case SYMS_CvRegx86_XMM10: result = 55; break;
case SYMS_CvRegx86_XMM11: result = 56; break;
case SYMS_CvRegx86_XMM12: result = 57; break;
case SYMS_CvRegx86_XMM13: result = 58; break;
case SYMS_CvRegx86_XMM20: result = 59; break;
case SYMS_CvRegx86_XMM21: result = 60; break;
case SYMS_CvRegx86_XMM22: result = 61; break;
case SYMS_CvRegx86_XMM23: result = 62; break;
case SYMS_CvRegx86_XMM30: result = 63; break;
case SYMS_CvRegx86_XMM31: result = 64; break;
case SYMS_CvRegx86_XMM32: result = 65; break;
case SYMS_CvRegx86_XMM33: result = 66; break;
case SYMS_CvRegx86_XMM40: result = 67; break;
case SYMS_CvRegx86_XMM41: result = 68; break;
case SYMS_CvRegx86_XMM42: result = 69; break;
case SYMS_CvRegx86_XMM43: result = 70; break;
case SYMS_CvRegx86_XMM50: result = 71; break;
case SYMS_CvRegx86_XMM51: result = 72; break;
case SYMS_CvRegx86_XMM52: result = 73; break;
case SYMS_CvRegx86_XMM53: result = 74; break;
case SYMS_CvRegx86_XMM60: result = 75; break;
case SYMS_CvRegx86_XMM61: result = 76; break;
case SYMS_CvRegx86_XMM62: result = 77; break;
case SYMS_CvRegx86_XMM63: result = 78; break;
case SYMS_CvRegx86_XMM70: result = 79; break;
case SYMS_CvRegx86_XMM71: result = 80; break;
case SYMS_CvRegx86_XMM72: result = 81; break;
case SYMS_CvRegx86_XMM73: result = 82; break;
case SYMS_CvRegx86_XMM0L: result = 83; break;
case SYMS_CvRegx86_XMM1L: result = 84; break;
case SYMS_CvRegx86_XMM2L: result = 85; break;
case SYMS_CvRegx86_XMM3L: result = 86; break;
case SYMS_CvRegx86_XMM4L: result = 87; break;
case SYMS_CvRegx86_XMM5L: result = 88; break;
case SYMS_CvRegx86_XMM6L: result = 89; break;
case SYMS_CvRegx86_XMM7L: result = 90; break;
case SYMS_CvRegx86_XMM0H: result = 91; break;
case SYMS_CvRegx86_XMM1H: result = 92; break;
case SYMS_CvRegx86_XMM2H: result = 93; break;
case SYMS_CvRegx86_XMM3H: result = 94; break;
case SYMS_CvRegx86_XMM4H: result = 95; break;
case SYMS_CvRegx86_XMM5H: result = 96; break;
case SYMS_CvRegx86_XMM6H: result = 97; break;
case SYMS_CvRegx86_XMM7H: result = 98; break;
case SYMS_CvRegx86_YMM0: result = 99; break;
case SYMS_CvRegx86_YMM1: result = 100; break;
case SYMS_CvRegx86_YMM2: result = 101; break;
case SYMS_CvRegx86_YMM3: result = 102; break;
case SYMS_CvRegx86_YMM4: result = 103; break;
case SYMS_CvRegx86_YMM5: result = 104; break;
case SYMS_CvRegx86_YMM6: result = 105; break;
case SYMS_CvRegx86_YMM7: result = 106; break;
case SYMS_CvRegx86_YMM0H: result = 107; break;
case SYMS_CvRegx86_YMM1H: result = 108; break;
case SYMS_CvRegx86_YMM2H: result = 109; break;
case SYMS_CvRegx86_YMM3H: result = 110; break;
case SYMS_CvRegx86_YMM4H: result = 111; break;
case SYMS_CvRegx86_YMM5H: result = 112; break;
case SYMS_CvRegx86_YMM6H: result = 113; break;
case SYMS_CvRegx86_YMM7H: result = 114; break;
case SYMS_CvRegx86_YMM0I0: result = 115; break;
case SYMS_CvRegx86_YMM0I1: result = 116; break;
case SYMS_CvRegx86_YMM0I2: result = 117; break;
case SYMS_CvRegx86_YMM0I3: result = 118; break;
case SYMS_CvRegx86_YMM1I0: result = 119; break;
case SYMS_CvRegx86_YMM1I1: result = 120; break;
case SYMS_CvRegx86_YMM1I2: result = 121; break;
case SYMS_CvRegx86_YMM1I3: result = 122; break;
case SYMS_CvRegx86_YMM2I0: result = 123; break;
case SYMS_CvRegx86_YMM2I1: result = 124; break;
case SYMS_CvRegx86_YMM2I2: result = 125; break;
case SYMS_CvRegx86_YMM2I3: result = 126; break;
case SYMS_CvRegx86_YMM3I0: result = 127; break;
case SYMS_CvRegx86_YMM3I1: result = 128; break;
case SYMS_CvRegx86_YMM3I2: result = 129; break;
case SYMS_CvRegx86_YMM3I3: result = 130; break;
case SYMS_CvRegx86_YMM4I0: result = 131; break;
case SYMS_CvRegx86_YMM4I1: result = 132; break;
case SYMS_CvRegx86_YMM4I2: result = 133; break;
case SYMS_CvRegx86_YMM4I3: result = 134; break;
case SYMS_CvRegx86_YMM5I0: result = 135; break;
case SYMS_CvRegx86_YMM5I1: result = 136; break;
case SYMS_CvRegx86_YMM5I2: result = 137; break;
case SYMS_CvRegx86_YMM5I3: result = 138; break;
case SYMS_CvRegx86_YMM6I0: result = 139; break;
case SYMS_CvRegx86_YMM6I1: result = 140; break;
case SYMS_CvRegx86_YMM6I2: result = 141; break;
case SYMS_CvRegx86_YMM6I3: result = 142; break;
case SYMS_CvRegx86_YMM7I0: result = 143; break;
case SYMS_CvRegx86_YMM7I1: result = 144; break;
case SYMS_CvRegx86_YMM7I2: result = 145; break;
case SYMS_CvRegx86_YMM7I3: result = 146; break;
case SYMS_CvRegx86_YMM0F0: result = 147; break;
case SYMS_CvRegx86_YMM0F1: result = 148; break;
case SYMS_CvRegx86_YMM0F2: result = 149; break;
case SYMS_CvRegx86_YMM0F3: result = 150; break;
case SYMS_CvRegx86_YMM0F4: result = 151; break;
case SYMS_CvRegx86_YMM0F5: result = 152; break;
case SYMS_CvRegx86_YMM0F6: result = 153; break;
case SYMS_CvRegx86_YMM0F7: result = 154; break;
case SYMS_CvRegx86_YMM1F0: result = 155; break;
case SYMS_CvRegx86_YMM1F1: result = 156; break;
case SYMS_CvRegx86_YMM1F2: result = 157; break;
case SYMS_CvRegx86_YMM1F3: result = 158; break;
case SYMS_CvRegx86_YMM1F4: result = 159; break;
case SYMS_CvRegx86_YMM1F5: result = 160; break;
case SYMS_CvRegx86_YMM1F6: result = 161; break;
case SYMS_CvRegx86_YMM1F7: result = 162; break;
case SYMS_CvRegx86_YMM2F0: result = 163; break;
case SYMS_CvRegx86_YMM2F1: result = 164; break;
case SYMS_CvRegx86_YMM2F2: result = 165; break;
case SYMS_CvRegx86_YMM2F3: result = 166; break;
case SYMS_CvRegx86_YMM2F4: result = 167; break;
case SYMS_CvRegx86_YMM2F5: result = 168; break;
case SYMS_CvRegx86_YMM2F6: result = 169; break;
case SYMS_CvRegx86_YMM2F7: result = 170; break;
case SYMS_CvRegx86_YMM3F0: result = 171; break;
case SYMS_CvRegx86_YMM3F1: result = 172; break;
case SYMS_CvRegx86_YMM3F2: result = 173; break;
case SYMS_CvRegx86_YMM3F3: result = 174; break;
case SYMS_CvRegx86_YMM3F4: result = 175; break;
case SYMS_CvRegx86_YMM3F5: result = 176; break;
case SYMS_CvRegx86_YMM3F6: result = 177; break;
case SYMS_CvRegx86_YMM3F7: result = 178; break;
case SYMS_CvRegx86_YMM4F0: result = 179; break;
case SYMS_CvRegx86_YMM4F1: result = 180; break;
case SYMS_CvRegx86_YMM4F2: result = 181; break;
case SYMS_CvRegx86_YMM4F3: result = 182; break;
case SYMS_CvRegx86_YMM4F4: result = 183; break;
case SYMS_CvRegx86_YMM4F5: result = 184; break;
case SYMS_CvRegx86_YMM4F6: result = 185; break;
case SYMS_CvRegx86_YMM4F7: result = 186; break;
case SYMS_CvRegx86_YMM5F0: result = 187; break;
case SYMS_CvRegx86_YMM5F1: result = 188; break;
case SYMS_CvRegx86_YMM5F2: result = 189; break;
case SYMS_CvRegx86_YMM5F3: result = 190; break;
case SYMS_CvRegx86_YMM5F4: result = 191; break;
case SYMS_CvRegx86_YMM5F5: result = 192; break;
case SYMS_CvRegx86_YMM5F6: result = 193; break;
case SYMS_CvRegx86_YMM5F7: result = 194; break;
case SYMS_CvRegx86_YMM6F0: result = 195; break;
case SYMS_CvRegx86_YMM6F1: result = 196; break;
case SYMS_CvRegx86_YMM6F2: result = 197; break;
case SYMS_CvRegx86_YMM6F3: result = 198; break;
case SYMS_CvRegx86_YMM6F4: result = 199; break;
case SYMS_CvRegx86_YMM6F5: result = 200; break;
case SYMS_CvRegx86_YMM6F6: result = 201; break;
case SYMS_CvRegx86_YMM6F7: result = 202; break;
case SYMS_CvRegx86_YMM7F0: result = 203; break;
case SYMS_CvRegx86_YMM7F1: result = 204; break;
case SYMS_CvRegx86_YMM7F2: result = 205; break;
case SYMS_CvRegx86_YMM7F3: result = 206; break;
case SYMS_CvRegx86_YMM7F4: result = 207; break;
case SYMS_CvRegx86_YMM7F5: result = 208; break;
case SYMS_CvRegx86_YMM7F6: result = 209; break;
case SYMS_CvRegx86_YMM7F7: result = 210; break;
case SYMS_CvRegx86_YMM0D0: result = 211; break;
case SYMS_CvRegx86_YMM0D1: result = 212; break;
case SYMS_CvRegx86_YMM0D2: result = 213; break;
case SYMS_CvRegx86_YMM0D3: result = 214; break;
case SYMS_CvRegx86_YMM1D0: result = 215; break;
case SYMS_CvRegx86_YMM1D1: result = 216; break;
case SYMS_CvRegx86_YMM1D2: result = 217; break;
case SYMS_CvRegx86_YMM1D3: result = 218; break;
case SYMS_CvRegx86_YMM2D0: result = 219; break;
case SYMS_CvRegx86_YMM2D1: result = 220; break;
case SYMS_CvRegx86_YMM2D2: result = 221; break;
case SYMS_CvRegx86_YMM2D3: result = 222; break;
case SYMS_CvRegx86_YMM3D0: result = 223; break;
case SYMS_CvRegx86_YMM3D1: result = 224; break;
case SYMS_CvRegx86_YMM3D2: result = 225; break;
case SYMS_CvRegx86_YMM3D3: result = 226; break;
case SYMS_CvRegx86_YMM4D0: result = 227; break;
case SYMS_CvRegx86_YMM4D1: result = 228; break;
case SYMS_CvRegx86_YMM4D2: result = 229; break;
case SYMS_CvRegx86_YMM4D3: result = 230; break;
case SYMS_CvRegx86_YMM5D0: result = 231; break;
case SYMS_CvRegx86_YMM5D1: result = 232; break;
case SYMS_CvRegx86_YMM5D2: result = 233; break;
case SYMS_CvRegx86_YMM5D3: result = 234; break;
case SYMS_CvRegx86_YMM6D0: result = 235; break;
case SYMS_CvRegx86_YMM6D1: result = 236; break;
case SYMS_CvRegx86_YMM6D2: result = 237; break;
case SYMS_CvRegx86_YMM6D3: result = 238; break;
case SYMS_CvRegx86_YMM7D0: result = 239; break;
case SYMS_CvRegx86_YMM7D1: result = 240; break;
case SYMS_CvRegx86_YMM7D2: result = 241; break;
case SYMS_CvRegx86_YMM7D3: result = 242; break;
}
return(result);
}
SYMS_API SYMS_U64
syms_enum_index_from_cvregx64(SYMS_U64 v){
SYMS_U64 result = 0;
switch ((SYMS_U16)v){
default: break;
case SYMS_CvRegx64_NONE: result = 0; break;
case SYMS_CvRegx64_AL: result = 1; break;
case SYMS_CvRegx64_CL: result = 2; break;
case SYMS_CvRegx64_DL: result = 3; break;
case SYMS_CvRegx64_BL: result = 4; break;
case SYMS_CvRegx64_AH: result = 5; break;
case SYMS_CvRegx64_CH: result = 6; break;
case SYMS_CvRegx64_DH: result = 7; break;
case SYMS_CvRegx64_BH: result = 8; break;
case SYMS_CvRegx64_AX: result = 9; break;
case SYMS_CvRegx64_CX: result = 10; break;
case SYMS_CvRegx64_DX: result = 11; break;
case SYMS_CvRegx64_BX: result = 12; break;
case SYMS_CvRegx64_SP: result = 13; break;
case SYMS_CvRegx64_BP: result = 14; break;
case SYMS_CvRegx64_SI: result = 15; break;
case SYMS_CvRegx64_DI: result = 16; break;
case SYMS_CvRegx64_EAX: result = 17; break;
case SYMS_CvRegx64_ECX: result = 18; break;
case SYMS_CvRegx64_EDX: result = 19; break;
case SYMS_CvRegx64_EBX: result = 20; break;
case SYMS_CvRegx64_ESP: result = 21; break;
case SYMS_CvRegx64_EBP: result = 22; break;
case SYMS_CvRegx64_ESI: result = 23; break;
case SYMS_CvRegx64_EDI: result = 24; break;
case SYMS_CvRegx64_ES: result = 25; break;
case SYMS_CvRegx64_CS: result = 26; break;
case SYMS_CvRegx64_SS: result = 27; break;
case SYMS_CvRegx64_DS: result = 28; break;
case SYMS_CvRegx64_FS: result = 29; break;
case SYMS_CvRegx64_GS: result = 30; break;
case SYMS_CvRegx64_FLAGS: result = 31; break;
case SYMS_CvRegx64_RIP: result = 32; break;
case SYMS_CvRegx64_EFLAGS: result = 33; break;
case SYMS_CvRegx64_CR0: result = 34; break;
case SYMS_CvRegx64_CR1: result = 35; break;
case SYMS_CvRegx64_CR2: result = 36; break;
case SYMS_CvRegx64_CR3: result = 37; break;
case SYMS_CvRegx64_CR4: result = 38; break;
case SYMS_CvRegx64_CR8: result = 39; break;
case SYMS_CvRegx64_DR0: result = 40; break;
case SYMS_CvRegx64_DR1: result = 41; break;
case SYMS_CvRegx64_DR2: result = 42; break;
case SYMS_CvRegx64_DR3: result = 43; break;
case SYMS_CvRegx64_DR4: result = 44; break;
case SYMS_CvRegx64_DR5: result = 45; break;
case SYMS_CvRegx64_DR6: result = 46; break;
case SYMS_CvRegx64_DR7: result = 47; break;
case SYMS_CvRegx64_DR8: result = 48; break;
case SYMS_CvRegx64_DR9: result = 49; break;
case SYMS_CvRegx64_DR10: result = 50; break;
case SYMS_CvRegx64_DR11: result = 51; break;
case SYMS_CvRegx64_DR12: result = 52; break;
case SYMS_CvRegx64_DR13: result = 53; break;
case SYMS_CvRegx64_DR14: result = 54; break;
case SYMS_CvRegx64_DR15: result = 55; break;
case SYMS_CvRegx64_GDTR: result = 56; break;
case SYMS_CvRegx64_GDTL: result = 57; break;
case SYMS_CvRegx64_IDTR: result = 58; break;
case SYMS_CvRegx64_IDTL: result = 59; break;
case SYMS_CvRegx64_LDTR: result = 60; break;
case SYMS_CvRegx64_TR: result = 61; break;
case SYMS_CvRegx64_ST0: result = 62; break;
case SYMS_CvRegx64_ST1: result = 63; break;
case SYMS_CvRegx64_ST2: result = 64; break;
case SYMS_CvRegx64_ST3: result = 65; break;
case SYMS_CvRegx64_ST4: result = 66; break;
case SYMS_CvRegx64_ST5: result = 67; break;
case SYMS_CvRegx64_ST6: result = 68; break;
case SYMS_CvRegx64_ST7: result = 69; break;
case SYMS_CvRegx64_CTRL: result = 70; break;
case SYMS_CvRegx64_STAT: result = 71; break;
case SYMS_CvRegx64_TAG: result = 72; break;
case SYMS_CvRegx64_FPIP: result = 73; break;
case SYMS_CvRegx64_FPCS: result = 74; break;
case SYMS_CvRegx64_FPDO: result = 75; break;
case SYMS_CvRegx64_FPDS: result = 76; break;
case SYMS_CvRegx64_ISEM: result = 77; break;
case SYMS_CvRegx64_FPEIP: result = 78; break;
case SYMS_CvRegx64_FPEDO: result = 79; break;
case SYMS_CvRegx64_MM0: result = 80; break;
case SYMS_CvRegx64_MM1: result = 81; break;
case SYMS_CvRegx64_MM2: result = 82; break;
case SYMS_CvRegx64_MM3: result = 83; break;
case SYMS_CvRegx64_MM4: result = 84; break;
case SYMS_CvRegx64_MM5: result = 85; break;
case SYMS_CvRegx64_MM6: result = 86; break;
case SYMS_CvRegx64_MM7: result = 87; break;
case SYMS_CvRegx64_XMM0: result = 88; break;
case SYMS_CvRegx64_XMM1: result = 89; break;
case SYMS_CvRegx64_XMM2: result = 90; break;
case SYMS_CvRegx64_XMM3: result = 91; break;
case SYMS_CvRegx64_XMM4: result = 92; break;
case SYMS_CvRegx64_XMM5: result = 93; break;
case SYMS_CvRegx64_XMM6: result = 94; break;
case SYMS_CvRegx64_XMM7: result = 95; break;
case SYMS_CvRegx64_XMM0_0: result = 96; break;
case SYMS_CvRegx64_XMM0_1: result = 97; break;
case SYMS_CvRegx64_XMM0_2: result = 98; break;
case SYMS_CvRegx64_XMM0_3: result = 99; break;
case SYMS_CvRegx64_XMM1_0: result = 100; break;
case SYMS_CvRegx64_XMM1_1: result = 101; break;
case SYMS_CvRegx64_XMM1_2: result = 102; break;
case SYMS_CvRegx64_XMM1_3: result = 103; break;
case SYMS_CvRegx64_XMM2_0: result = 104; break;
case SYMS_CvRegx64_XMM2_1: result = 105; break;
case SYMS_CvRegx64_XMM2_2: result = 106; break;
case SYMS_CvRegx64_XMM2_3: result = 107; break;
case SYMS_CvRegx64_XMM3_0: result = 108; break;
case SYMS_CvRegx64_XMM3_1: result = 109; break;
case SYMS_CvRegx64_XMM3_2: result = 110; break;
case SYMS_CvRegx64_XMM3_3: result = 111; break;
case SYMS_CvRegx64_XMM4_0: result = 112; break;
case SYMS_CvRegx64_XMM4_1: result = 113; break;
case SYMS_CvRegx64_XMM4_2: result = 114; break;
case SYMS_CvRegx64_XMM4_3: result = 115; break;
case SYMS_CvRegx64_XMM5_0: result = 116; break;
case SYMS_CvRegx64_XMM5_1: result = 117; break;
case SYMS_CvRegx64_XMM5_2: result = 118; break;
case SYMS_CvRegx64_XMM5_3: result = 119; break;
case SYMS_CvRegx64_XMM6_0: result = 120; break;
case SYMS_CvRegx64_XMM6_1: result = 121; break;
case SYMS_CvRegx64_XMM6_2: result = 122; break;
case SYMS_CvRegx64_XMM6_3: result = 123; break;
case SYMS_CvRegx64_XMM7_0: result = 124; break;
case SYMS_CvRegx64_XMM7_1: result = 125; break;
case SYMS_CvRegx64_XMM7_2: result = 126; break;
case SYMS_CvRegx64_XMM7_3: result = 127; break;
case SYMS_CvRegx64_XMM0L: result = 128; break;
case SYMS_CvRegx64_XMM1L: result = 129; break;
case SYMS_CvRegx64_XMM2L: result = 130; break;
case SYMS_CvRegx64_XMM3L: result = 131; break;
case SYMS_CvRegx64_XMM4L: result = 132; break;
case SYMS_CvRegx64_XMM5L: result = 133; break;
case SYMS_CvRegx64_XMM6L: result = 134; break;
case SYMS_CvRegx64_XMM7L: result = 135; break;
case SYMS_CvRegx64_XMM0H: result = 136; break;
case SYMS_CvRegx64_XMM1H: result = 137; break;
case SYMS_CvRegx64_XMM2H: result = 138; break;
case SYMS_CvRegx64_XMM3H: result = 139; break;
case SYMS_CvRegx64_XMM4H: result = 140; break;
case SYMS_CvRegx64_XMM5H: result = 141; break;
case SYMS_CvRegx64_XMM6H: result = 142; break;
case SYMS_CvRegx64_XMM7H: result = 143; break;
case SYMS_CvRegx64_MXCSR: result = 144; break;
case SYMS_CvRegx64_EMM0L: result = 145; break;
case SYMS_CvRegx64_EMM1L: result = 146; break;
case SYMS_CvRegx64_EMM2L: result = 147; break;
case SYMS_CvRegx64_EMM3L: result = 148; break;
case SYMS_CvRegx64_EMM4L: result = 149; break;
case SYMS_CvRegx64_EMM5L: result = 150; break;
case SYMS_CvRegx64_EMM6L: result = 151; break;
case SYMS_CvRegx64_EMM7L: result = 152; break;
case SYMS_CvRegx64_EMM0H: result = 153; break;
case SYMS_CvRegx64_EMM1H: result = 154; break;
case SYMS_CvRegx64_EMM2H: result = 155; break;
case SYMS_CvRegx64_EMM3H: result = 156; break;
case SYMS_CvRegx64_EMM4H: result = 157; break;
case SYMS_CvRegx64_EMM5H: result = 158; break;
case SYMS_CvRegx64_EMM6H: result = 159; break;
case SYMS_CvRegx64_EMM7H: result = 160; break;
case SYMS_CvRegx64_MM00: result = 161; break;
case SYMS_CvRegx64_MM01: result = 162; break;
case SYMS_CvRegx64_MM10: result = 163; break;
case SYMS_CvRegx64_MM11: result = 164; break;
case SYMS_CvRegx64_MM20: result = 165; break;
case SYMS_CvRegx64_MM21: result = 166; break;
case SYMS_CvRegx64_MM30: result = 167; break;
case SYMS_CvRegx64_MM31: result = 168; break;
case SYMS_CvRegx64_MM40: result = 169; break;
case SYMS_CvRegx64_MM41: result = 170; break;
case SYMS_CvRegx64_MM50: result = 171; break;
case SYMS_CvRegx64_MM51: result = 172; break;
case SYMS_CvRegx64_MM60: result = 173; break;
case SYMS_CvRegx64_MM61: result = 174; break;
case SYMS_CvRegx64_MM70: result = 175; break;
case SYMS_CvRegx64_MM71: result = 176; break;
case SYMS_CvRegx64_XMM8: result = 177; break;
case SYMS_CvRegx64_XMM9: result = 178; break;
case SYMS_CvRegx64_XMM10: result = 179; break;
case SYMS_CvRegx64_XMM11: result = 180; break;
case SYMS_CvRegx64_XMM12: result = 181; break;
case SYMS_CvRegx64_XMM13: result = 182; break;
case SYMS_CvRegx64_XMM14: result = 183; break;
case SYMS_CvRegx64_XMM15: result = 184; break;
case SYMS_CvRegx64_XMM8_0: result = 185; break;
case SYMS_CvRegx64_XMM8_1: result = 186; break;
case SYMS_CvRegx64_XMM8_2: result = 187; break;
case SYMS_CvRegx64_XMM8_3: result = 188; break;
case SYMS_CvRegx64_XMM9_0: result = 189; break;
case SYMS_CvRegx64_XMM9_1: result = 190; break;
case SYMS_CvRegx64_XMM9_2: result = 191; break;
case SYMS_CvRegx64_XMM9_3: result = 192; break;
case SYMS_CvRegx64_XMM10_0: result = 193; break;
case SYMS_CvRegx64_XMM10_1: result = 194; break;
case SYMS_CvRegx64_XMM10_2: result = 195; break;
case SYMS_CvRegx64_XMM10_3: result = 196; break;
case SYMS_CvRegx64_XMM11_0: result = 197; break;
case SYMS_CvRegx64_XMM11_1: result = 198; break;
case SYMS_CvRegx64_XMM11_2: result = 199; break;
case SYMS_CvRegx64_XMM11_3: result = 200; break;
case SYMS_CvRegx64_XMM12_0: result = 201; break;
case SYMS_CvRegx64_XMM12_1: result = 202; break;
case SYMS_CvRegx64_XMM12_2: result = 203; break;
case SYMS_CvRegx64_XMM12_3: result = 204; break;
case SYMS_CvRegx64_XMM13_0: result = 205; break;
case SYMS_CvRegx64_XMM13_1: result = 206; break;
case SYMS_CvRegx64_XMM13_2: result = 207; break;
case SYMS_CvRegx64_XMM13_3: result = 208; break;
case SYMS_CvRegx64_XMM14_0: result = 209; break;
case SYMS_CvRegx64_XMM14_1: result = 210; break;
case SYMS_CvRegx64_XMM14_2: result = 211; break;
case SYMS_CvRegx64_XMM14_3: result = 212; break;
case SYMS_CvRegx64_XMM15_0: result = 213; break;
case SYMS_CvRegx64_XMM15_1: result = 214; break;
case SYMS_CvRegx64_XMM15_2: result = 215; break;
case SYMS_CvRegx64_XMM15_3: result = 216; break;
case SYMS_CvRegx64_XMM8L: result = 217; break;
case SYMS_CvRegx64_XMM9L: result = 218; break;
case SYMS_CvRegx64_XMM10L: result = 219; break;
case SYMS_CvRegx64_XMM11L: result = 220; break;
case SYMS_CvRegx64_XMM12L: result = 221; break;
case SYMS_CvRegx64_XMM13L: result = 222; break;
case SYMS_CvRegx64_XMM14L: result = 223; break;
case SYMS_CvRegx64_XMM15L: result = 224; break;
case SYMS_CvRegx64_XMM8H: result = 225; break;
case SYMS_CvRegx64_XMM9H: result = 226; break;
case SYMS_CvRegx64_XMM10H: result = 227; break;
case SYMS_CvRegx64_XMM11H: result = 228; break;
case SYMS_CvRegx64_XMM12H: result = 229; break;
case SYMS_CvRegx64_XMM13H: result = 230; break;
case SYMS_CvRegx64_XMM14H: result = 231; break;
case SYMS_CvRegx64_XMM15H: result = 232; break;
case SYMS_CvRegx64_EMM8L: result = 233; break;
case SYMS_CvRegx64_EMM9L: result = 234; break;
case SYMS_CvRegx64_EMM10L: result = 235; break;
case SYMS_CvRegx64_EMM11L: result = 236; break;
case SYMS_CvRegx64_EMM12L: result = 237; break;
case SYMS_CvRegx64_EMM13L: result = 238; break;
case SYMS_CvRegx64_EMM14L: result = 239; break;
case SYMS_CvRegx64_EMM15L: result = 240; break;
case SYMS_CvRegx64_EMM8H: result = 241; break;
case SYMS_CvRegx64_EMM9H: result = 242; break;
case SYMS_CvRegx64_EMM10H: result = 243; break;
case SYMS_CvRegx64_EMM11H: result = 244; break;
case SYMS_CvRegx64_EMM12H: result = 245; break;
case SYMS_CvRegx64_EMM13H: result = 246; break;
case SYMS_CvRegx64_EMM14H: result = 247; break;
case SYMS_CvRegx64_EMM15H: result = 248; break;
case SYMS_CvRegx64_SIL: result = 249; break;
case SYMS_CvRegx64_DIL: result = 250; break;
case SYMS_CvRegx64_BPL: result = 251; break;
case SYMS_CvRegx64_SPL: result = 252; break;
case SYMS_CvRegx64_RAX: result = 253; break;
case SYMS_CvRegx64_RBX: result = 254; break;
case SYMS_CvRegx64_RCX: result = 255; break;
case SYMS_CvRegx64_RDX: result = 256; break;
case SYMS_CvRegx64_RSI: result = 257; break;
case SYMS_CvRegx64_RDI: result = 258; break;
case SYMS_CvRegx64_RBP: result = 259; break;
case SYMS_CvRegx64_RSP: result = 260; break;
case SYMS_CvRegx64_R8: result = 261; break;
case SYMS_CvRegx64_R9: result = 262; break;
case SYMS_CvRegx64_R10: result = 263; break;
case SYMS_CvRegx64_R11: result = 264; break;
case SYMS_CvRegx64_R12: result = 265; break;
case SYMS_CvRegx64_R13: result = 266; break;
case SYMS_CvRegx64_R14: result = 267; break;
case SYMS_CvRegx64_R15: result = 268; break;
case SYMS_CvRegx64_R8B: result = 269; break;
case SYMS_CvRegx64_R9B: result = 270; break;
case SYMS_CvRegx64_R10B: result = 271; break;
case SYMS_CvRegx64_R11B: result = 272; break;
case SYMS_CvRegx64_R12B: result = 273; break;
case SYMS_CvRegx64_R13B: result = 274; break;
case SYMS_CvRegx64_R14B: result = 275; break;
case SYMS_CvRegx64_R15B: result = 276; break;
case SYMS_CvRegx64_R8W: result = 277; break;
case SYMS_CvRegx64_R9W: result = 278; break;
case SYMS_CvRegx64_R10W: result = 279; break;
case SYMS_CvRegx64_R11W: result = 280; break;
case SYMS_CvRegx64_R12W: result = 281; break;
case SYMS_CvRegx64_R13W: result = 282; break;
case SYMS_CvRegx64_R14W: result = 283; break;
case SYMS_CvRegx64_R15W: result = 284; break;
case SYMS_CvRegx64_R8D: result = 285; break;
case SYMS_CvRegx64_R9D: result = 286; break;
case SYMS_CvRegx64_R10D: result = 287; break;
case SYMS_CvRegx64_R11D: result = 288; break;
case SYMS_CvRegx64_R12D: result = 289; break;
case SYMS_CvRegx64_R13D: result = 290; break;
case SYMS_CvRegx64_R14D: result = 291; break;
case SYMS_CvRegx64_R15D: result = 292; break;
case SYMS_CvRegx64_YMM0: result = 293; break;
case SYMS_CvRegx64_YMM1: result = 294; break;
case SYMS_CvRegx64_YMM2: result = 295; break;
case SYMS_CvRegx64_YMM3: result = 296; break;
case SYMS_CvRegx64_YMM4: result = 297; break;
case SYMS_CvRegx64_YMM5: result = 298; break;
case SYMS_CvRegx64_YMM6: result = 299; break;
case SYMS_CvRegx64_YMM7: result = 300; break;
case SYMS_CvRegx64_YMM8: result = 301; break;
case SYMS_CvRegx64_YMM9: result = 302; break;
case SYMS_CvRegx64_YMM10: result = 303; break;
case SYMS_CvRegx64_YMM11: result = 304; break;
case SYMS_CvRegx64_YMM12: result = 305; break;
case SYMS_CvRegx64_YMM13: result = 306; break;
case SYMS_CvRegx64_YMM14: result = 307; break;
case SYMS_CvRegx64_YMM15: result = 308; break;
case SYMS_CvRegx64_YMM0H: result = 309; break;
case SYMS_CvRegx64_YMM1H: result = 310; break;
case SYMS_CvRegx64_YMM2H: result = 311; break;
case SYMS_CvRegx64_YMM3H: result = 312; break;
case SYMS_CvRegx64_YMM4H: result = 313; break;
case SYMS_CvRegx64_YMM5H: result = 314; break;
case SYMS_CvRegx64_YMM6H: result = 315; break;
case SYMS_CvRegx64_YMM7H: result = 316; break;
case SYMS_CvRegx64_YMM8H: result = 317; break;
case SYMS_CvRegx64_YMM9H: result = 318; break;
case SYMS_CvRegx64_YMM10H: result = 319; break;
case SYMS_CvRegx64_YMM11H: result = 320; break;
case SYMS_CvRegx64_YMM12H: result = 321; break;
case SYMS_CvRegx64_YMM13H: result = 322; break;
case SYMS_CvRegx64_YMM14H: result = 323; break;
case SYMS_CvRegx64_YMM15H: result = 324; break;
case SYMS_CvRegx64_XMM0IL: result = 325; break;
case SYMS_CvRegx64_XMM1IL: result = 326; break;
case SYMS_CvRegx64_XMM2IL: result = 327; break;
case SYMS_CvRegx64_XMM3IL: result = 328; break;
case SYMS_CvRegx64_XMM4IL: result = 329; break;
case SYMS_CvRegx64_XMM5IL: result = 330; break;
case SYMS_CvRegx64_XMM6IL: result = 331; break;
case SYMS_CvRegx64_XMM7IL: result = 332; break;
case SYMS_CvRegx64_XMM8IL: result = 333; break;
case SYMS_CvRegx64_XMM9IL: result = 334; break;
case SYMS_CvRegx64_XMM10IL: result = 335; break;
case SYMS_CvRegx64_XMM11IL: result = 336; break;
case SYMS_CvRegx64_XMM12IL: result = 337; break;
case SYMS_CvRegx64_XMM13IL: result = 338; break;
case SYMS_CvRegx64_XMM14IL: result = 339; break;
case SYMS_CvRegx64_XMM15IL: result = 340; break;
case SYMS_CvRegx64_XMM0IH: result = 341; break;
case SYMS_CvRegx64_XMM1IH: result = 342; break;
case SYMS_CvRegx64_XMM2IH: result = 343; break;
case SYMS_CvRegx64_XMM3IH: result = 344; break;
case SYMS_CvRegx64_XMM4IH: result = 345; break;
case SYMS_CvRegx64_XMM5IH: result = 346; break;
case SYMS_CvRegx64_XMM6IH: result = 347; break;
case SYMS_CvRegx64_XMM7IH: result = 348; break;
case SYMS_CvRegx64_XMM8IH: result = 349; break;
case SYMS_CvRegx64_XMM9IH: result = 350; break;
case SYMS_CvRegx64_XMM10IH: result = 351; break;
case SYMS_CvRegx64_XMM11IH: result = 352; break;
case SYMS_CvRegx64_XMM12IH: result = 353; break;
case SYMS_CvRegx64_XMM13IH: result = 354; break;
case SYMS_CvRegx64_XMM14IH: result = 355; break;
case SYMS_CvRegx64_XMM15IH: result = 356; break;
case SYMS_CvRegx64_YMM0I0: result = 357; break;
case SYMS_CvRegx64_YMM0I1: result = 358; break;
case SYMS_CvRegx64_YMM0I2: result = 359; break;
case SYMS_CvRegx64_YMM0I3: result = 360; break;
case SYMS_CvRegx64_YMM1I0: result = 361; break;
case SYMS_CvRegx64_YMM1I1: result = 362; break;
case SYMS_CvRegx64_YMM1I2: result = 363; break;
case SYMS_CvRegx64_YMM1I3: result = 364; break;
case SYMS_CvRegx64_YMM2I0: result = 365; break;
case SYMS_CvRegx64_YMM2I1: result = 366; break;
case SYMS_CvRegx64_YMM2I2: result = 367; break;
case SYMS_CvRegx64_YMM2I3: result = 368; break;
case SYMS_CvRegx64_YMM3I0: result = 369; break;
case SYMS_CvRegx64_YMM3I1: result = 370; break;
case SYMS_CvRegx64_YMM3I2: result = 371; break;
case SYMS_CvRegx64_YMM3I3: result = 372; break;
case SYMS_CvRegx64_YMM4I0: result = 373; break;
case SYMS_CvRegx64_YMM4I1: result = 374; break;
case SYMS_CvRegx64_YMM4I2: result = 375; break;
case SYMS_CvRegx64_YMM4I3: result = 376; break;
case SYMS_CvRegx64_YMM5I0: result = 377; break;
case SYMS_CvRegx64_YMM5I1: result = 378; break;
case SYMS_CvRegx64_YMM5I2: result = 379; break;
case SYMS_CvRegx64_YMM5I3: result = 380; break;
case SYMS_CvRegx64_YMM6I0: result = 381; break;
case SYMS_CvRegx64_YMM6I1: result = 382; break;
case SYMS_CvRegx64_YMM6I2: result = 383; break;
case SYMS_CvRegx64_YMM6I3: result = 384; break;
case SYMS_CvRegx64_YMM7I0: result = 385; break;
case SYMS_CvRegx64_YMM7I1: result = 386; break;
case SYMS_CvRegx64_YMM7I2: result = 387; break;
case SYMS_CvRegx64_YMM7I3: result = 388; break;
case SYMS_CvRegx64_YMM8I0: result = 389; break;
case SYMS_CvRegx64_YMM8I1: result = 390; break;
case SYMS_CvRegx64_YMM8I2: result = 391; break;
case SYMS_CvRegx64_YMM8I3: result = 392; break;
case SYMS_CvRegx64_YMM9I0: result = 393; break;
case SYMS_CvRegx64_YMM9I1: result = 394; break;
case SYMS_CvRegx64_YMM9I2: result = 395; break;
case SYMS_CvRegx64_YMM9I3: result = 396; break;
case SYMS_CvRegx64_YMM10I0: result = 397; break;
case SYMS_CvRegx64_YMM10I1: result = 398; break;
case SYMS_CvRegx64_YMM10I2: result = 399; break;
case SYMS_CvRegx64_YMM10I3: result = 400; break;
case SYMS_CvRegx64_YMM11I0: result = 401; break;
case SYMS_CvRegx64_YMM11I1: result = 402; break;
case SYMS_CvRegx64_YMM11I2: result = 403; break;
case SYMS_CvRegx64_YMM11I3: result = 404; break;
case SYMS_CvRegx64_YMM12I0: result = 405; break;
case SYMS_CvRegx64_YMM12I1: result = 406; break;
case SYMS_CvRegx64_YMM12I2: result = 407; break;
case SYMS_CvRegx64_YMM12I3: result = 408; break;
case SYMS_CvRegx64_YMM13I0: result = 409; break;
case SYMS_CvRegx64_YMM13I1: result = 410; break;
case SYMS_CvRegx64_YMM13I2: result = 411; break;
case SYMS_CvRegx64_YMM13I3: result = 412; break;
case SYMS_CvRegx64_YMM14I0: result = 413; break;
case SYMS_CvRegx64_YMM14I1: result = 414; break;
case SYMS_CvRegx64_YMM14I2: result = 415; break;
case SYMS_CvRegx64_YMM14I3: result = 416; break;
case SYMS_CvRegx64_YMM15I0: result = 417; break;
case SYMS_CvRegx64_YMM15I1: result = 418; break;
case SYMS_CvRegx64_YMM15I2: result = 419; break;
case SYMS_CvRegx64_YMM15I3: result = 420; break;
case SYMS_CvRegx64_YMM0F0: result = 421; break;
case SYMS_CvRegx64_YMM0F1: result = 422; break;
case SYMS_CvRegx64_YMM0F2: result = 423; break;
case SYMS_CvRegx64_YMM0F3: result = 424; break;
case SYMS_CvRegx64_YMM0F4: result = 425; break;
case SYMS_CvRegx64_YMM0F5: result = 426; break;
case SYMS_CvRegx64_YMM0F6: result = 427; break;
case SYMS_CvRegx64_YMM0F7: result = 428; break;
case SYMS_CvRegx64_YMM1F0: result = 429; break;
case SYMS_CvRegx64_YMM1F1: result = 430; break;
case SYMS_CvRegx64_YMM1F2: result = 431; break;
case SYMS_CvRegx64_YMM1F3: result = 432; break;
case SYMS_CvRegx64_YMM1F4: result = 433; break;
case SYMS_CvRegx64_YMM1F5: result = 434; break;
case SYMS_CvRegx64_YMM1F6: result = 435; break;
case SYMS_CvRegx64_YMM1F7: result = 436; break;
case SYMS_CvRegx64_YMM2F0: result = 437; break;
case SYMS_CvRegx64_YMM2F1: result = 438; break;
case SYMS_CvRegx64_YMM2F2: result = 439; break;
case SYMS_CvRegx64_YMM2F3: result = 440; break;
case SYMS_CvRegx64_YMM2F4: result = 441; break;
case SYMS_CvRegx64_YMM2F5: result = 442; break;
case SYMS_CvRegx64_YMM2F6: result = 443; break;
case SYMS_CvRegx64_YMM2F7: result = 444; break;
case SYMS_CvRegx64_YMM3F0: result = 445; break;
case SYMS_CvRegx64_YMM3F1: result = 446; break;
case SYMS_CvRegx64_YMM3F2: result = 447; break;
case SYMS_CvRegx64_YMM3F3: result = 448; break;
case SYMS_CvRegx64_YMM3F4: result = 449; break;
case SYMS_CvRegx64_YMM3F5: result = 450; break;
case SYMS_CvRegx64_YMM3F6: result = 451; break;
case SYMS_CvRegx64_YMM3F7: result = 452; break;
case SYMS_CvRegx64_YMM4F0: result = 453; break;
case SYMS_CvRegx64_YMM4F1: result = 454; break;
case SYMS_CvRegx64_YMM4F2: result = 455; break;
case SYMS_CvRegx64_YMM4F3: result = 456; break;
case SYMS_CvRegx64_YMM4F4: result = 457; break;
case SYMS_CvRegx64_YMM4F5: result = 458; break;
case SYMS_CvRegx64_YMM4F6: result = 459; break;
case SYMS_CvRegx64_YMM4F7: result = 460; break;
case SYMS_CvRegx64_YMM5F0: result = 461; break;
case SYMS_CvRegx64_YMM5F1: result = 462; break;
case SYMS_CvRegx64_YMM5F2: result = 463; break;
case SYMS_CvRegx64_YMM5F3: result = 464; break;
case SYMS_CvRegx64_YMM5F4: result = 465; break;
case SYMS_CvRegx64_YMM5F5: result = 466; break;
case SYMS_CvRegx64_YMM5F6: result = 467; break;
case SYMS_CvRegx64_YMM5F7: result = 468; break;
case SYMS_CvRegx64_YMM6F0: result = 469; break;
case SYMS_CvRegx64_YMM6F1: result = 470; break;
case SYMS_CvRegx64_YMM6F2: result = 471; break;
case SYMS_CvRegx64_YMM6F3: result = 472; break;
case SYMS_CvRegx64_YMM6F4: result = 473; break;
case SYMS_CvRegx64_YMM6F5: result = 474; break;
case SYMS_CvRegx64_YMM6F6: result = 475; break;
case SYMS_CvRegx64_YMM6F7: result = 476; break;
case SYMS_CvRegx64_YMM7F0: result = 477; break;
case SYMS_CvRegx64_YMM7F1: result = 478; break;
case SYMS_CvRegx64_YMM7F2: result = 479; break;
case SYMS_CvRegx64_YMM7F3: result = 480; break;
case SYMS_CvRegx64_YMM7F4: result = 481; break;
case SYMS_CvRegx64_YMM7F5: result = 482; break;
case SYMS_CvRegx64_YMM7F6: result = 483; break;
case SYMS_CvRegx64_YMM7F7: result = 484; break;
case SYMS_CvRegx64_YMM8F0: result = 485; break;
case SYMS_CvRegx64_YMM8F1: result = 486; break;
case SYMS_CvRegx64_YMM8F2: result = 487; break;
case SYMS_CvRegx64_YMM8F3: result = 488; break;
case SYMS_CvRegx64_YMM8F4: result = 489; break;
case SYMS_CvRegx64_YMM8F5: result = 490; break;
case SYMS_CvRegx64_YMM8F6: result = 491; break;
case SYMS_CvRegx64_YMM8F7: result = 492; break;
case SYMS_CvRegx64_YMM9F0: result = 493; break;
case SYMS_CvRegx64_YMM9F1: result = 494; break;
case SYMS_CvRegx64_YMM9F2: result = 495; break;
case SYMS_CvRegx64_YMM9F3: result = 496; break;
case SYMS_CvRegx64_YMM9F4: result = 497; break;
case SYMS_CvRegx64_YMM9F5: result = 498; break;
case SYMS_CvRegx64_YMM9F6: result = 499; break;
case SYMS_CvRegx64_YMM9F7: result = 500; break;
case SYMS_CvRegx64_YMM10F0: result = 501; break;
case SYMS_CvRegx64_YMM10F1: result = 502; break;
case SYMS_CvRegx64_YMM10F2: result = 503; break;
case SYMS_CvRegx64_YMM10F3: result = 504; break;
case SYMS_CvRegx64_YMM10F4: result = 505; break;
case SYMS_CvRegx64_YMM10F5: result = 506; break;
case SYMS_CvRegx64_YMM10F6: result = 507; break;
case SYMS_CvRegx64_YMM10F7: result = 508; break;
case SYMS_CvRegx64_YMM11F0: result = 509; break;
case SYMS_CvRegx64_YMM11F1: result = 510; break;
case SYMS_CvRegx64_YMM11F2: result = 511; break;
case SYMS_CvRegx64_YMM11F3: result = 512; break;
case SYMS_CvRegx64_YMM11F4: result = 513; break;
case SYMS_CvRegx64_YMM11F5: result = 514; break;
case SYMS_CvRegx64_YMM11F6: result = 515; break;
case SYMS_CvRegx64_YMM11F7: result = 516; break;
case SYMS_CvRegx64_YMM12F0: result = 517; break;
case SYMS_CvRegx64_YMM12F1: result = 518; break;
case SYMS_CvRegx64_YMM12F2: result = 519; break;
case SYMS_CvRegx64_YMM12F3: result = 520; break;
case SYMS_CvRegx64_YMM12F4: result = 521; break;
case SYMS_CvRegx64_YMM12F5: result = 522; break;
case SYMS_CvRegx64_YMM12F6: result = 523; break;
case SYMS_CvRegx64_YMM12F7: result = 524; break;
case SYMS_CvRegx64_YMM13F0: result = 525; break;
case SYMS_CvRegx64_YMM13F1: result = 526; break;
case SYMS_CvRegx64_YMM13F2: result = 527; break;
case SYMS_CvRegx64_YMM13F3: result = 528; break;
case SYMS_CvRegx64_YMM13F4: result = 529; break;
case SYMS_CvRegx64_YMM13F5: result = 530; break;
case SYMS_CvRegx64_YMM13F6: result = 531; break;
case SYMS_CvRegx64_YMM13F7: result = 532; break;
case SYMS_CvRegx64_YMM14F0: result = 533; break;
case SYMS_CvRegx64_YMM14F1: result = 534; break;
case SYMS_CvRegx64_YMM14F2: result = 535; break;
case SYMS_CvRegx64_YMM14F3: result = 536; break;
case SYMS_CvRegx64_YMM14F4: result = 537; break;
case SYMS_CvRegx64_YMM14F5: result = 538; break;
case SYMS_CvRegx64_YMM14F6: result = 539; break;
case SYMS_CvRegx64_YMM14F7: result = 540; break;
case SYMS_CvRegx64_YMM15F0: result = 541; break;
case SYMS_CvRegx64_YMM15F1: result = 542; break;
case SYMS_CvRegx64_YMM15F2: result = 543; break;
case SYMS_CvRegx64_YMM15F3: result = 544; break;
case SYMS_CvRegx64_YMM15F4: result = 545; break;
case SYMS_CvRegx64_YMM15F5: result = 546; break;
case SYMS_CvRegx64_YMM15F6: result = 547; break;
case SYMS_CvRegx64_YMM15F7: result = 548; break;
case SYMS_CvRegx64_YMM0D0: result = 549; break;
case SYMS_CvRegx64_YMM0D1: result = 550; break;
case SYMS_CvRegx64_YMM0D2: result = 551; break;
case SYMS_CvRegx64_YMM0D3: result = 552; break;
case SYMS_CvRegx64_YMM1D0: result = 553; break;
case SYMS_CvRegx64_YMM1D1: result = 554; break;
case SYMS_CvRegx64_YMM1D2: result = 555; break;
case SYMS_CvRegx64_YMM1D3: result = 556; break;
case SYMS_CvRegx64_YMM2D0: result = 557; break;
case SYMS_CvRegx64_YMM2D1: result = 558; break;
case SYMS_CvRegx64_YMM2D2: result = 559; break;
case SYMS_CvRegx64_YMM2D3: result = 560; break;
case SYMS_CvRegx64_YMM3D0: result = 561; break;
case SYMS_CvRegx64_YMM3D1: result = 562; break;
case SYMS_CvRegx64_YMM3D2: result = 563; break;
case SYMS_CvRegx64_YMM3D3: result = 564; break;
case SYMS_CvRegx64_YMM4D0: result = 565; break;
case SYMS_CvRegx64_YMM4D1: result = 566; break;
case SYMS_CvRegx64_YMM4D2: result = 567; break;
case SYMS_CvRegx64_YMM4D3: result = 568; break;
case SYMS_CvRegx64_YMM5D0: result = 569; break;
case SYMS_CvRegx64_YMM5D1: result = 570; break;
case SYMS_CvRegx64_YMM5D2: result = 571; break;
case SYMS_CvRegx64_YMM5D3: result = 572; break;
case SYMS_CvRegx64_YMM6D0: result = 573; break;
case SYMS_CvRegx64_YMM6D1: result = 574; break;
case SYMS_CvRegx64_YMM6D2: result = 575; break;
case SYMS_CvRegx64_YMM6D3: result = 576; break;
case SYMS_CvRegx64_YMM7D0: result = 577; break;
case SYMS_CvRegx64_YMM7D1: result = 578; break;
case SYMS_CvRegx64_YMM7D2: result = 579; break;
case SYMS_CvRegx64_YMM7D3: result = 580; break;
case SYMS_CvRegx64_YMM8D0: result = 581; break;
case SYMS_CvRegx64_YMM8D1: result = 582; break;
case SYMS_CvRegx64_YMM8D2: result = 583; break;
case SYMS_CvRegx64_YMM8D3: result = 584; break;
case SYMS_CvRegx64_YMM9D0: result = 585; break;
case SYMS_CvRegx64_YMM9D1: result = 586; break;
case SYMS_CvRegx64_YMM9D2: result = 587; break;
case SYMS_CvRegx64_YMM9D3: result = 588; break;
case SYMS_CvRegx64_YMM10D0: result = 589; break;
case SYMS_CvRegx64_YMM10D1: result = 590; break;
case SYMS_CvRegx64_YMM10D2: result = 591; break;
case SYMS_CvRegx64_YMM10D3: result = 592; break;
case SYMS_CvRegx64_YMM11D0: result = 593; break;
case SYMS_CvRegx64_YMM11D1: result = 594; break;
case SYMS_CvRegx64_YMM11D2: result = 595; break;
case SYMS_CvRegx64_YMM11D3: result = 596; break;
case SYMS_CvRegx64_YMM12D0: result = 597; break;
case SYMS_CvRegx64_YMM12D1: result = 598; break;
case SYMS_CvRegx64_YMM12D2: result = 599; break;
case SYMS_CvRegx64_YMM12D3: result = 600; break;
case SYMS_CvRegx64_YMM13D0: result = 601; break;
case SYMS_CvRegx64_YMM13D1: result = 602; break;
case SYMS_CvRegx64_YMM13D2: result = 603; break;
case SYMS_CvRegx64_YMM13D3: result = 604; break;
case SYMS_CvRegx64_YMM14D0: result = 605; break;
case SYMS_CvRegx64_YMM14D1: result = 606; break;
case SYMS_CvRegx64_YMM14D2: result = 607; break;
case SYMS_CvRegx64_YMM14D3: result = 608; break;
case SYMS_CvRegx64_YMM15D0: result = 609; break;
case SYMS_CvRegx64_YMM15D1: result = 610; break;
case SYMS_CvRegx64_YMM15D2: result = 611; break;
case SYMS_CvRegx64_YMM15D3: result = 612; break;
}
return(result);
}
SYMS_API SYMS_U64
syms_enum_index_from_cvsignature(SYMS_U64 v){
SYMS_U64 result = 0;
switch ((SYMS_U32)v){
default: break;
case SYMS_CvSignature_C6: result = 0; break;
case SYMS_CvSignature_C7: result = 1; break;
case SYMS_CvSignature_C11: result = 2; break;
case SYMS_CvSignature_C13: result = 3; break;
case SYMS_CvSignature_RESERVED: result = 4; break;
}
return(result);
}
SYMS_API SYMS_U64
syms_enum_index_from_cvsymkind(SYMS_U64 v){
SYMS_U64 result = 0;
switch ((SYMS_U16)v){
default: break;
case SYMS_CvSymKind_NULL: result = 0; break;
case SYMS_CvSymKind_COMPILE: result = 1; break;
case SYMS_CvSymKind_REGISTER_16t: result = 2; break;
case SYMS_CvSymKind_CONSTANT_16t: result = 3; break;
case SYMS_CvSymKind_UDT_16t: result = 4; break;
case SYMS_CvSymKind_SSEARCH: result = 5; break;
case SYMS_CvSymKind_END: result = 6; break;
case SYMS_CvSymKind_SKIP: result = 7; break;
case SYMS_CvSymKind_CVRESERVE: result = 8; break;
case SYMS_CvSymKind_OBJNAME_ST: result = 9; break;
case SYMS_CvSymKind_ENDARG: result = 10; break;
case SYMS_CvSymKind_COBOLUDT_16t: result = 11; break;
case SYMS_CvSymKind_MANYREG_16t: result = 12; break;
case SYMS_CvSymKind_RETURN: result = 13; break;
case SYMS_CvSymKind_ENTRYTHIS: result = 14; break;
case SYMS_CvSymKind_BPREL16: result = 15; break;
case SYMS_CvSymKind_LDATA16: result = 16; break;
case SYMS_CvSymKind_GDATA16: result = 17; break;
case SYMS_CvSymKind_PUB16: result = 18; break;
case SYMS_CvSymKind_LPROC16: result = 19; break;
case SYMS_CvSymKind_GPROC16: result = 20; break;
case SYMS_CvSymKind_THUNK16: result = 21; break;
case SYMS_CvSymKind_BLOCK16: result = 22; break;
case SYMS_CvSymKind_WITH16: result = 23; break;
case SYMS_CvSymKind_LABEL16: result = 24; break;
case SYMS_CvSymKind_CEXMODEL16: result = 25; break;
case SYMS_CvSymKind_VFTABLE16: result = 26; break;
case SYMS_CvSymKind_REGREL16: result = 27; break;
case SYMS_CvSymKind_BPREL32_16t: result = 28; break;
case SYMS_CvSymKind_LDATA32_16t: result = 29; break;
case SYMS_CvSymKind_GDATA32_16t: result = 30; break;
case SYMS_CvSymKind_PUB32_16t: result = 31; break;
case SYMS_CvSymKind_LPROC32_16t: result = 32; break;
case SYMS_CvSymKind_GPROC32_16t: result = 33; break;
case SYMS_CvSymKind_THUNK32_ST: result = 34; break;
case SYMS_CvSymKind_BLOCK32_ST: result = 35; break;
case SYMS_CvSymKind_WITH32_ST: result = 36; break;
case SYMS_CvSymKind_LABEL32_ST: result = 37; break;
case SYMS_CvSymKind_CEXMODEL32: result = 38; break;
case SYMS_CvSymKind_VFTABLE32_16t: result = 39; break;
case SYMS_CvSymKind_REGREL32_16t: result = 40; break;
case SYMS_CvSymKind_LTHREAD32_16t: result = 41; break;
case SYMS_CvSymKind_GTHREAD32_16t: result = 42; break;
case SYMS_CvSymKind_SLINK32: result = 43; break;
case SYMS_CvSymKind_LPROCMIPS_16t: result = 44; break;
case SYMS_CvSymKind_GPROCMIPS_16t: result = 45; break;
case SYMS_CvSymKind_PROCREF_ST: result = 46; break;
case SYMS_CvSymKind_DATAREF_ST: result = 47; break;
case SYMS_CvSymKind_ALIGN: result = 48; break;
case SYMS_CvSymKind_LPROCREF_ST: result = 49; break;
case SYMS_CvSymKind_OEM: result = 50; break;
case SYMS_CvSymKind_TI16_MAX: result = 51; break;
case SYMS_CvSymKind_CONSTANT_ST: result = 52; break;
case SYMS_CvSymKind_UDT_ST: result = 53; break;
case SYMS_CvSymKind_COBOLUDT_ST: result = 54; break;
case SYMS_CvSymKind_MANYREG_ST: result = 55; break;
case SYMS_CvSymKind_BPREL32_ST: result = 56; break;
case SYMS_CvSymKind_LDATA32_ST: result = 57; break;
case SYMS_CvSymKind_GDATA32_ST: result = 58; break;
case SYMS_CvSymKind_PUB32_ST: result = 59; break;
case SYMS_CvSymKind_LPROC32_ST: result = 60; break;
case SYMS_CvSymKind_GPROC32_ST: result = 61; break;
case SYMS_CvSymKind_VFTABLE32: result = 62; break;
case SYMS_CvSymKind_REGREL32_ST: result = 63; break;
case SYMS_CvSymKind_LTHREAD32_ST: result = 64; break;
case SYMS_CvSymKind_GTHREAD32_ST: result = 65; break;
case SYMS_CvSymKind_LPROCMIPS_ST: result = 66; break;
case SYMS_CvSymKind_GPROCMIPS_ST: result = 67; break;
case SYMS_CvSymKind_FRAMEPROC: result = 68; break;
case SYMS_CvSymKind_COMPILE2_ST: result = 69; break;
case SYMS_CvSymKind_MANYREG2_ST: result = 70; break;
case SYMS_CvSymKind_LPROCIA64_ST: result = 71; break;
case SYMS_CvSymKind_GPROCIA64_ST: result = 72; break;
case SYMS_CvSymKind_LOCALSLOT_ST: result = 73; break;
case SYMS_CvSymKind_PARAMSLOT_ST: result = 74; break;
case SYMS_CvSymKind_ANNOTATION: result = 75; break;
case SYMS_CvSymKind_GMANPROC_ST: result = 76; break;
case SYMS_CvSymKind_LMANPROC_ST: result = 77; break;
case SYMS_CvSymKind_RESERVED1: result = 78; break;
case SYMS_CvSymKind_RESERVED2: result = 79; break;
case SYMS_CvSymKind_RESERVED3: result = 80; break;
case SYMS_CvSymKind_RESERVED4: result = 81; break;
case SYMS_CvSymKind_LMANDATA_ST: result = 82; break;
case SYMS_CvSymKind_GMANDATA_ST: result = 83; break;
case SYMS_CvSymKind_MANFRAMEREL_ST: result = 84; break;
case SYMS_CvSymKind_MANREGISTER_ST: result = 85; break;
case SYMS_CvSymKind_MANSLOT_ST: result = 86; break;
case SYMS_CvSymKind_MANMANYREG_ST: result = 87; break;
case SYMS_CvSymKind_MANREGREL_ST: result = 88; break;
case SYMS_CvSymKind_MANMANYREG2_ST: result = 89; break;
case SYMS_CvSymKind_MANTYPREF: result = 90; break;
case SYMS_CvSymKind_UNAMESPACE_ST: result = 91; break;
case SYMS_CvSymKind_ST_MAX: result = 92; break;
case SYMS_CvSymKind_OBJNAME: result = 93; break;
case SYMS_CvSymKind_THUNK32: result = 94; break;
case SYMS_CvSymKind_BLOCK32: result = 95; break;
case SYMS_CvSymKind_WITH32: result = 96; break;
case SYMS_CvSymKind_LABEL32: result = 97; break;
case SYMS_CvSymKind_REGISTER: result = 98; break;
case SYMS_CvSymKind_CONSTANT: result = 99; break;
case SYMS_CvSymKind_UDT: result = 100; break;
case SYMS_CvSymKind_COBOLUDT: result = 101; break;
case SYMS_CvSymKind_MANYREG: result = 102; break;
case SYMS_CvSymKind_BPREL32: result = 103; break;
case SYMS_CvSymKind_LDATA32: result = 104; break;
case SYMS_CvSymKind_GDATA32: result = 105; break;
case SYMS_CvSymKind_PUB32: result = 106; break;
case SYMS_CvSymKind_LPROC32: result = 107; break;
case SYMS_CvSymKind_GPROC32: result = 108; break;
case SYMS_CvSymKind_REGREL32: result = 109; break;
case SYMS_CvSymKind_LTHREAD32: result = 110; break;
case SYMS_CvSymKind_GTHREAD32: result = 111; break;
case SYMS_CvSymKind_LPROCMIPS: result = 112; break;
case SYMS_CvSymKind_GPROCMIPS: result = 113; break;
case SYMS_CvSymKind_COMPILE2: result = 114; break;
case SYMS_CvSymKind_MANYREG2: result = 115; break;
case SYMS_CvSymKind_LPROCIA64: result = 116; break;
case SYMS_CvSymKind_GPROCIA64: result = 117; break;
case SYMS_CvSymKind_LOCALSLOT: result = 118; break;
case SYMS_CvSymKind_PARAMSLOT: result = 119; break;
case SYMS_CvSymKind_LMANDATA: result = 120; break;
case SYMS_CvSymKind_GMANDATA: result = 121; break;
case SYMS_CvSymKind_MANFRAMEREL: result = 122; break;
case SYMS_CvSymKind_MANREGISTER: result = 123; break;
case SYMS_CvSymKind_MANSLOT: result = 124; break;
case SYMS_CvSymKind_MANMANYREG: result = 125; break;
case SYMS_CvSymKind_MANREGREL: result = 126; break;
case SYMS_CvSymKind_MANMANYREG2: result = 127; break;
case SYMS_CvSymKind_UNAMESPACE: result = 128; break;
case SYMS_CvSymKind_PROCREF: result = 129; break;
case SYMS_CvSymKind_DATAREF: result = 130; break;
case SYMS_CvSymKind_LPROCREF: result = 131; break;
case SYMS_CvSymKind_ANNOTATIONREF: result = 132; break;
case SYMS_CvSymKind_TOKENREF: result = 133; break;
case SYMS_CvSymKind_GMANPROC: result = 134; break;
case SYMS_CvSymKind_LMANPROC: result = 135; break;
case SYMS_CvSymKind_TRAMPOLINE: result = 136; break;
case SYMS_CvSymKind_MANCONSTANT: result = 137; break;
case SYMS_CvSymKind_ATTR_FRAMEREL: result = 138; break;
case SYMS_CvSymKind_ATTR_REGISTER: result = 139; break;
case SYMS_CvSymKind_ATTR_REGREL: result = 140; break;
case SYMS_CvSymKind_ATTR_MANYREG: result = 141; break;
case SYMS_CvSymKind_SEPCODE: result = 142; break;
case SYMS_CvSymKind_DEFRANGE_2005: result = 143; break;
case SYMS_CvSymKind_DEFRANGE2_2005: result = 144; break;
case SYMS_CvSymKind_SECTION: result = 145; break;
case SYMS_CvSymKind_COFFGROUP: result = 146; break;
case SYMS_CvSymKind_EXPORT: result = 147; break;
case SYMS_CvSymKind_CALLSITEINFO: result = 148; break;
case SYMS_CvSymKind_FRAMECOOKIE: result = 149; break;
case SYMS_CvSymKind_DISCARDED: result = 150; break;
case SYMS_CvSymKind_COMPILE3: result = 151; break;
case SYMS_CvSymKind_ENVBLOCK: result = 152; break;
case SYMS_CvSymKind_LOCAL: result = 153; break;
case SYMS_CvSymKind_DEFRANGE: result = 154; break;
case SYMS_CvSymKind_DEFRANGE_SUBFIELD: result = 155; break;
case SYMS_CvSymKind_DEFRANGE_REGISTER: result = 156; break;
case SYMS_CvSymKind_DEFRANGE_FRAMEPOINTER_REL: result = 157; break;
case SYMS_CvSymKind_DEFRANGE_SUBFIELD_REGISTER: result = 158; break;
case SYMS_CvSymKind_DEFRANGE_FRAMEPOINTER_REL_FULL_SCOPE: result = 159; break;
case SYMS_CvSymKind_DEFRANGE_REGISTER_REL: result = 160; break;
case SYMS_CvSymKind_LPROC32_ID: result = 161; break;
case SYMS_CvSymKind_GPROC32_ID: result = 162; break;
case SYMS_CvSymKind_LPROCMIPS_ID: result = 163; break;
case SYMS_CvSymKind_GPROCMIPS_ID: result = 164; break;
case SYMS_CvSymKind_LPROCIA64_ID: result = 165; break;
case SYMS_CvSymKind_GPROCIA64_ID: result = 166; break;
case SYMS_CvSymKind_BUILDINFO: result = 167; break;
case SYMS_CvSymKind_INLINESITE: result = 168; break;
case SYMS_CvSymKind_INLINESITE_END: result = 169; break;
case SYMS_CvSymKind_PROC_ID_END: result = 170; break;
case SYMS_CvSymKind_DEFRANGE_HLSL: result = 171; break;
case SYMS_CvSymKind_GDATA_HLSL: result = 172; break;
case SYMS_CvSymKind_LDATA_HLSL: result = 173; break;
case SYMS_CvSymKind_FILESTATIC: result = 174; break;
case SYMS_CvSymKind_LPROC32_DPC: result = 175; break;
case SYMS_CvSymKind_LPROC32_DPC_ID: result = 176; break;
case SYMS_CvSymKind_DEFRANGE_DPC_PTR_TAG: result = 177; break;
case SYMS_CvSymKind_DPC_SYM_TAG_MAP: result = 178; break;
case SYMS_CvSymKind_ARMSWITCHTABLE: result = 179; break;
case SYMS_CvSymKind_CALLEES: result = 180; break;
case SYMS_CvSymKind_CALLERS: result = 181; break;
case SYMS_CvSymKind_POGODATA: result = 182; break;
case SYMS_CvSymKind_INLINESITE2: result = 183; break;
case SYMS_CvSymKind_HEAPALLOCSITE: result = 184; break;
case SYMS_CvSymKind_MOD_TYPEREF: result = 185; break;
case SYMS_CvSymKind_REF_MINIPDB: result = 186; break;
case SYMS_CvSymKind_PDBMAP: result = 187; break;
case SYMS_CvSymKind_GDATA_HLSL32: result = 188; break;
case SYMS_CvSymKind_LDATA_HLSL32: result = 189; break;
case SYMS_CvSymKind_GDATA_HLSL32_EX: result = 190; break;
case SYMS_CvSymKind_LDATA_HLSL32_EX: result = 191; break;
case SYMS_CvSymKind_FASTLINK: result = 192; break;
case SYMS_CvSymKind_INLINEES: result = 193; break;
}
return(result);
}
// syms_enum_index_from_cv_generic_style - skipped identity mapping
// syms_enum_index_from_cvlanguage - skipped identity mapping
// syms_enum_index_from_cvencodedframeptrreg - skipped identity mapping
// syms_enum_index_from_cv_thunk_ordinal - skipped identity mapping
// syms_enum_index_from_cvtrampolinekind - skipped identity mapping
// syms_enum_index_from_cvframecookiekind - skipped identity mapping
// syms_enum_index_from_cv_b_a_opcode - skipped identity mapping
// syms_enum_index_from_cv_arm_switch_type - skipped identity mapping
// syms_enum_index_from_cv_discarded_type - skipped identity mapping
SYMS_API SYMS_U64
syms_enum_index_from_cvleaf(SYMS_U64 v){
SYMS_U64 result = 0;
switch ((SYMS_U16)v){
default: break;
case SYMS_CvLeaf_MODIFIER_16t: result = 0; break;
case SYMS_CvLeaf_POINTER_16t: result = 1; break;
case SYMS_CvLeaf_ARRAY_16t: result = 2; break;
case SYMS_CvLeaf_CLASS_16t: result = 3; break;
case SYMS_CvLeaf_STRUCTURE_16t: result = 4; break;
case SYMS_CvLeaf_UNION_16t: result = 5; break;
case SYMS_CvLeaf_ENUM_16t: result = 6; break;
case SYMS_CvLeaf_PROCEDURE_16t: result = 7; break;
case SYMS_CvLeaf_MFUNCTION_16t: result = 8; break;
case SYMS_CvLeaf_VTSHAPE: result = 9; break;
case SYMS_CvLeaf_COBOL0_16t: result = 10; break;
case SYMS_CvLeaf_COBOL1: result = 11; break;
case SYMS_CvLeaf_BARRAY_16t: result = 12; break;
case SYMS_CvLeaf_LABEL: result = 13; break;
case SYMS_CvLeaf_NULL: result = 14; break;
case SYMS_CvLeaf_NOTTRAN: result = 15; break;
case SYMS_CvLeaf_DIMARRAY_16t: result = 16; break;
case SYMS_CvLeaf_VFTPATH_16t: result = 17; break;
case SYMS_CvLeaf_PRECOMP_16t: result = 18; break;
case SYMS_CvLeaf_ENDPRECOMP: result = 19; break;
case SYMS_CvLeaf_OEM_16t: result = 20; break;
case SYMS_CvLeaf_TYPESERVER_ST: result = 21; break;
case SYMS_CvLeaf_SKIP_16t: result = 22; break;
case SYMS_CvLeaf_ARGLIST_16t: result = 23; break;
case SYMS_CvLeaf_DEFARG_16t: result = 24; break;
case SYMS_CvLeaf_LIST: result = 25; break;
case SYMS_CvLeaf_FIELDLIST_16t: result = 26; break;
case SYMS_CvLeaf_DERIVED_16t: result = 27; break;
case SYMS_CvLeaf_BITFIELD_16t: result = 28; break;
case SYMS_CvLeaf_METHODLIST_16t: result = 29; break;
case SYMS_CvLeaf_DIMCONU_16t: result = 30; break;
case SYMS_CvLeaf_DIMCONLU_16t: result = 31; break;
case SYMS_CvLeaf_DIMVARU_16t: result = 32; break;
case SYMS_CvLeaf_DIMVARLU_16t: result = 33; break;
case SYMS_CvLeaf_REFSYM: result = 34; break;
case SYMS_CvLeaf_BCLASS_16t: result = 35; break;
case SYMS_CvLeaf_VBCLASS_16t: result = 36; break;
case SYMS_CvLeaf_IVBCLASS_16t: result = 37; break;
case SYMS_CvLeaf_ENUMERATE_ST: result = 38; break;
case SYMS_CvLeaf_FRIENDFCN_16t: result = 39; break;
case SYMS_CvLeaf_INDEX_16t: result = 40; break;
case SYMS_CvLeaf_MEMBER_16t: result = 41; break;
case SYMS_CvLeaf_STMEMBER_16t: result = 42; break;
case SYMS_CvLeaf_METHOD_16t: result = 43; break;
case SYMS_CvLeaf_NESTTYPE_16t: result = 44; break;
case SYMS_CvLeaf_VFUNCTAB_16t: result = 45; break;
case SYMS_CvLeaf_FRIENDCLS_16t: result = 46; break;
case SYMS_CvLeaf_ONEMETHOD_16t: result = 47; break;
case SYMS_CvLeaf_VFUNCOFF_16t: result = 48; break;
case SYMS_CvLeaf_TI16_MAX: result = 49; break;
case SYMS_CvLeaf_MODIFIER: result = 50; break;
case SYMS_CvLeaf_POINTER: result = 51; break;
case SYMS_CvLeaf_ARRAY_ST: result = 52; break;
case SYMS_CvLeaf_CLASS_ST: result = 53; break;
case SYMS_CvLeaf_STRUCTURE_ST: result = 54; break;
case SYMS_CvLeaf_UNION_ST: result = 55; break;
case SYMS_CvLeaf_ENUM_ST: result = 56; break;
case SYMS_CvLeaf_PROCEDURE: result = 57; break;
case SYMS_CvLeaf_MFUNCTION: result = 58; break;
case SYMS_CvLeaf_COBOL0: result = 59; break;
case SYMS_CvLeaf_BARRAY: result = 60; break;
case SYMS_CvLeaf_DIMARRAY_ST: result = 61; break;
case SYMS_CvLeaf_VFTPATH: result = 62; break;
case SYMS_CvLeaf_PRECOMP_ST: result = 63; break;
case SYMS_CvLeaf_OEM: result = 64; break;
case SYMS_CvLeaf_ALIAS_ST: result = 65; break;
case SYMS_CvLeaf_OEM2: result = 66; break;
case SYMS_CvLeaf_SKIP: result = 67; break;
case SYMS_CvLeaf_ARGLIST: result = 68; break;
case SYMS_CvLeaf_DEFARG_ST: result = 69; break;
case SYMS_CvLeaf_FIELDLIST: result = 70; break;
case SYMS_CvLeaf_DERIVED: result = 71; break;
case SYMS_CvLeaf_BITFIELD: result = 72; break;
case SYMS_CvLeaf_METHODLIST: result = 73; break;
case SYMS_CvLeaf_DIMCONU: result = 74; break;
case SYMS_CvLeaf_DIMCONLU: result = 75; break;
case SYMS_CvLeaf_DIMVARU: result = 76; break;
case SYMS_CvLeaf_DIMVARLU: result = 77; break;
case SYMS_CvLeaf_BCLASS: result = 78; break;
case SYMS_CvLeaf_VBCLASS: result = 79; break;
case SYMS_CvLeaf_IVBCLASS: result = 80; break;
case SYMS_CvLeaf_FRIENDFCN_ST: result = 81; break;
case SYMS_CvLeaf_INDEX: result = 82; break;
case SYMS_CvLeaf_MEMBER_ST: result = 83; break;
case SYMS_CvLeaf_STMEMBER_ST: result = 84; break;
case SYMS_CvLeaf_METHOD_ST: result = 85; break;
case SYMS_CvLeaf_NESTTYPE_ST: result = 86; break;
case SYMS_CvLeaf_VFUNCTAB: result = 87; break;
case SYMS_CvLeaf_FRIENDCLS: result = 88; break;
case SYMS_CvLeaf_ONEMETHOD_ST: result = 89; break;
case SYMS_CvLeaf_VFUNCOFF: result = 90; break;
case SYMS_CvLeaf_NESTTYPEEX_ST: result = 91; break;
case SYMS_CvLeaf_MEMBERMODIFY_ST: result = 92; break;
case SYMS_CvLeaf_MANAGED_ST: result = 93; break;
case SYMS_CvLeaf_ST_MAX: result = 94; break;
case SYMS_CvLeaf_TYPESERVER: result = 95; break;
case SYMS_CvLeaf_ENUMERATE: result = 96; break;
case SYMS_CvLeaf_ARRAY: result = 97; break;
case SYMS_CvLeaf_CLASS: result = 98; break;
case SYMS_CvLeaf_STRUCTURE: result = 99; break;
case SYMS_CvLeaf_UNION: result = 100; break;
case SYMS_CvLeaf_ENUM: result = 101; break;
case SYMS_CvLeaf_DIMARRAY: result = 102; break;
case SYMS_CvLeaf_PRECOMP: result = 103; break;
case SYMS_CvLeaf_ALIAS: result = 104; break;
case SYMS_CvLeaf_DEFARG: result = 105; break;
case SYMS_CvLeaf_FRIENDFCN: result = 106; break;
case SYMS_CvLeaf_MEMBER: result = 107; break;
case SYMS_CvLeaf_STMEMBER: result = 108; break;
case SYMS_CvLeaf_METHOD: result = 109; break;
case SYMS_CvLeaf_NESTTYPE: result = 110; break;
case SYMS_CvLeaf_ONEMETHOD: result = 111; break;
case SYMS_CvLeaf_NESTTYPEEX: result = 112; break;
case SYMS_CvLeaf_MEMBERMODIFY: result = 113; break;
case SYMS_CvLeaf_MANAGED: result = 114; break;
case SYMS_CvLeaf_TYPESERVER2: result = 115; break;
case SYMS_CvLeaf_STRIDED_ARRAY: result = 116; break;
case SYMS_CvLeaf_HLSL: result = 117; break;
case SYMS_CvLeaf_MODIFIER_EX: result = 118; break;
case SYMS_CvLeaf_INTERFACE: result = 119; break;
case SYMS_CvLeaf_BINTERFACE: result = 120; break;
case SYMS_CvLeaf_VECTOR: result = 121; break;
case SYMS_CvLeaf_MATRIX: result = 122; break;
case SYMS_CvLeaf_VFTABLE: result = 123; break;
case SYMS_CvLeaf_TYPE_LAST: result = 125; break;
case SYMS_CvLeaf_FUNC_ID: result = 127; break;
case SYMS_CvLeaf_MFUNC_ID: result = 128; break;
case SYMS_CvLeaf_BUILDINFO: result = 129; break;
case SYMS_CvLeaf_SUBSTR_LIST: result = 130; break;
case SYMS_CvLeaf_STRING_ID: result = 131; break;
case SYMS_CvLeaf_UDT_SRC_LINE: result = 132; break;
case SYMS_CvLeaf_UDT_MOD_SRC_LINE: result = 133; break;
case SYMS_CvLeaf_CLASSPTR: result = 134; break;
case SYMS_CvLeaf_CLASSPTR2: result = 135; break;
case SYMS_CvLeaf_ID_LAST: result = 136; break;
case SYMS_CvLeaf_CHAR: result = 139; break;
case SYMS_CvLeaf_SHORT: result = 140; break;
case SYMS_CvLeaf_USHORT: result = 141; break;
case SYMS_CvLeaf_LONG: result = 142; break;
case SYMS_CvLeaf_ULONG: result = 143; break;
case SYMS_CvLeaf_FLOAT32: result = 144; break;
case SYMS_CvLeaf_FLOAT64: result = 145; break;
case SYMS_CvLeaf_FLOAT80: result = 146; break;
case SYMS_CvLeaf_FLOAT128: result = 147; break;
case SYMS_CvLeaf_QUADWORD: result = 148; break;
case SYMS_CvLeaf_UQUADWORD: result = 149; break;
case SYMS_CvLeaf_FLOAT48: result = 150; break;
case SYMS_CvLeaf_COMPLEX32: result = 151; break;
case SYMS_CvLeaf_COMPLEX64: result = 152; break;
case SYMS_CvLeaf_COMPLEX80: result = 153; break;
case SYMS_CvLeaf_COMPLEX128: result = 154; break;
case SYMS_CvLeaf_VARSTRING: result = 155; break;
case SYMS_CvLeaf_OCTWORD: result = 156; break;
case SYMS_CvLeaf_UOCTWORD: result = 157; break;
case SYMS_CvLeaf_DECIMAL: result = 158; break;
case SYMS_CvLeaf_DATE: result = 159; break;
case SYMS_CvLeaf_UTF8STRING: result = 160; break;
case SYMS_CvLeaf_FLOAT16: result = 161; break;
case SYMS_CvLeaf_PAD0: result = 162; break;
case SYMS_CvLeaf_PAD1: result = 163; break;
case SYMS_CvLeaf_PAD2: result = 164; break;
case SYMS_CvLeaf_PAD3: result = 165; break;
case SYMS_CvLeaf_PAD4: result = 166; break;
case SYMS_CvLeaf_PAD5: result = 167; break;
case SYMS_CvLeaf_PAD6: result = 168; break;
case SYMS_CvLeaf_PAD7: result = 169; break;
case SYMS_CvLeaf_PAD8: result = 170; break;
case SYMS_CvLeaf_PAD9: result = 171; break;
case SYMS_CvLeaf_PAD10: result = 172; break;
case SYMS_CvLeaf_PAD11: result = 173; break;
case SYMS_CvLeaf_PAD12: result = 174; break;
case SYMS_CvLeaf_PAD13: result = 175; break;
case SYMS_CvLeaf_PAD14: result = 176; break;
case SYMS_CvLeaf_PAD15: result = 177; break;
}
return(result);
}
// syms_enum_index_from_cvhfakind - skipped identity mapping
// syms_enum_index_from_cvmocomudtkind - skipped identity mapping
// syms_enum_index_from_cvpointerkind - skipped identity mapping
SYMS_API SYMS_U64
syms_enum_index_from_cvpointermode(SYMS_U64 v){
SYMS_U64 result = 0;
switch ((SYMS_U8)v){
default: break;
case SYMS_CvPointerMode_PTR: result = 0; break;
case SYMS_CvPointerMode_REF: result = 1; break;
case SYMS_CvPointerMode_PMEM: result = 3; break;
case SYMS_CvPointerMode_PMFUNC: result = 4; break;
case SYMS_CvPointerMode_RVREF: result = 5; break;
case SYMS_CvPointerMode_RESERVED: result = 6; break;
}
return(result);
}
// syms_enum_index_from_cv_member_pointer_kind - skipped identity mapping
// syms_enum_index_from_cvvirtualtableshape - skipped identity mapping
// syms_enum_index_from_cvmethodprop - skipped identity mapping
// syms_enum_index_from_cvmemberaccess - skipped identity mapping
SYMS_API SYMS_U64
syms_enum_index_from_cvlabelkind(SYMS_U64 v){
SYMS_U64 result = 0;
switch ((SYMS_U16)v){
default: break;
case SYMS_CvLabelKind_NEAR: result = 0; break;
case SYMS_CvLabelKind_FAR: result = 1; break;
}
return(result);
}
// syms_enum_index_from_cvcallkind - skipped identity mapping
SYMS_API SYMS_U64
syms_enum_index_from_cvsubsectionkind(SYMS_U64 v){
SYMS_U64 result = 0;
switch ((SYMS_U32)v){
default: break;
case SYMS_CvSubSectionKind_SYMBOLS: result = 0; break;
case SYMS_CvSubSectionKind_LINES: result = 1; break;
case SYMS_CvSubSectionKind_STRINGTABLE: result = 2; break;
case SYMS_CvSubSectionKind_FILECHKSMS: result = 3; break;
case SYMS_CvSubSectionKind_FRAMEDATA: result = 4; break;
case SYMS_CvSubSectionKind_INLINEELINES: result = 5; break;
case SYMS_CvSubSectionKind_CROSSSCOPEIMPORTS: result = 6; break;
case SYMS_CvSubSectionKind_CROSSSCOPEEXPORTS: result = 7; break;
case SYMS_CvSubSectionKind_IL_LINES: result = 8; break;
case SYMS_CvSubSectionKind_FUNC_MDTOKEN_MAP: result = 9; break;
case SYMS_CvSubSectionKind_TYPE_MDTOKEN_MAP: result = 10; break;
case SYMS_CvSubSectionKind_MERGED_ASSEMBLY_INPUT: result = 11; break;
case SYMS_CvSubSectionKind_COFF_SYMBOL_RVA: result = 12; break;
case SYMS_CvSubSectionKind_XFG_HASH_TYPE: result = 13; break;
case SYMS_CvSubSectionKind_XFG_HASH_VRITUAL: result = 14; break;
}
return(result);
}
// syms_enum_index_from_cv_checksum_kind - skipped identity mapping
// syms_enum_index_from_cv_inlinee_source_line_sig - skipped identity mapping

//~ generated from code at syms/metaprogram/syms_metaprogram_serial.c:1150
SYMS_API SYMS_SerialType*
syms_type_info_from_sym_kind(SYMS_CvSymKind v){
SYMS_SerialType* result = 0;
switch (v){
default: break;
case SYMS_CvSymKind_LTHREAD32:case SYMS_CvSymKind_GTHREAD32:
result = &_syms_serial_type_SYMS_CvThread32; break;
case SYMS_CvSymKind_COMPILE: result = &_syms_serial_type_SYMS_CvCompile; break;
case SYMS_CvSymKind_END: result = &_syms_serial_type_SYMS_CvEnd; break;
case SYMS_CvSymKind_COMPILE2: result = &_syms_serial_type_SYMS_CvCompile2; break;
case SYMS_CvSymKind_COMPILE3: result = &_syms_serial_type_SYMS_CvCompile3; break;
case SYMS_CvSymKind_OBJNAME: result = &_syms_serial_type_SYMS_CvObjname; break;
case SYMS_CvSymKind_UNAMESPACE: result = &_syms_serial_type_SYMS_CvUNamespace; break;
case SYMS_CvSymKind_PROCREF:case SYMS_CvSymKind_LPROCREF:case SYMS_CvSymKind_DATAREF:
result = &_syms_serial_type_SYMS_CvRef2; break;
case SYMS_CvSymKind_SEPCODE: result = &_syms_serial_type_SYMS_CvSepcode; break;
case SYMS_CvSymKind_LOCALSLOT: result = &_syms_serial_type_SYMS_CvSlotsym32; break;
case SYMS_CvSymKind_POGODATA: result = &_syms_serial_type_SYMS_CvPogoInfo; break;
case SYMS_CvSymKind_MANYREG: result = &_syms_serial_type_SYMS_CvManyreg; break;
case SYMS_CvSymKind_MANYREG2: result = &_syms_serial_type_SYMS_CvManyreg2; break;
case SYMS_CvSymKind_FRAMEPROC: result = &_syms_serial_type_SYMS_CvFrameproc; break;
case SYMS_CvSymKind_THUNK32: result = &_syms_serial_type_SYMS_CvThunk32; break;
case SYMS_CvSymKind_BLOCK32: result = &_syms_serial_type_SYMS_CvBlock32; break;
case SYMS_CvSymKind_LABEL32: result = &_syms_serial_type_SYMS_CvLabel32; break;
case SYMS_CvSymKind_CONSTANT: result = &_syms_serial_type_SYMS_CvConstant; break;
case SYMS_CvSymKind_UDT: result = &_syms_serial_type_SYMS_CvUDT; break;
case SYMS_CvSymKind_TRAMPOLINE: result = &_syms_serial_type_SYMS_CvTrampoline; break;
case SYMS_CvSymKind_SECTION: result = &_syms_serial_type_SYMS_CvSection; break;
case SYMS_CvSymKind_COFFGROUP: result = &_syms_serial_type_SYMS_CvCoffGroup; break;
case SYMS_CvSymKind_EXPORT: result = &_syms_serial_type_SYMS_CvExport; break;
case SYMS_CvSymKind_CALLSITEINFO: result = &_syms_serial_type_SYMS_CvCallSiteInfo; break;
case SYMS_CvSymKind_FRAMECOOKIE: result = &_syms_serial_type_SYMS_CvFrameCookie; break;
case SYMS_CvSymKind_ENVBLOCK: result = &_syms_serial_type_SYMS_CvEnvblock; break;
case SYMS_CvSymKind_LOCAL: result = &_syms_serial_type_SYMS_CvLocal; break;
case SYMS_CvSymKind_DEFRANGE: result = &_syms_serial_type_SYMS_CvDefrange; break;
case SYMS_CvSymKind_DEFRANGE_SUBFIELD: result = &_syms_serial_type_SYMS_CvDefrangeSubfield; break;
case SYMS_CvSymKind_DEFRANGE_REGISTER: result = &_syms_serial_type_SYMS_CvDefrangeRegister; break;
case SYMS_CvSymKind_DEFRANGE_FRAMEPOINTER_REL: result = &_syms_serial_type_SYMS_CvDefrangeFramepointerRel; break;
case SYMS_CvSymKind_DEFRANGE_SUBFIELD_REGISTER: result = &_syms_serial_type_SYMS_CvDefrangeSubfieldRegister; break;
case SYMS_CvSymKind_DEFRANGE_FRAMEPOINTER_REL_FULL_SCOPE: result = &_syms_serial_type_SYMS_CvDefrangeFramepointerRelFullScope; break;
case SYMS_CvSymKind_DEFRANGE_REGISTER_REL: result = &_syms_serial_type_SYMS_CvDefrangeRegisterRel; break;
case SYMS_CvSymKind_LDATA32:case SYMS_CvSymKind_GDATA32:
result = &_syms_serial_type_SYMS_CvData32; break;
case SYMS_CvSymKind_PUB32: result = &_syms_serial_type_SYMS_CvPubsym32; break;
case SYMS_CvSymKind_GPROC16: result = &_syms_serial_type_SYMS_CvGProc16; break;
case SYMS_CvSymKind_GPROC32_16t: result = &_syms_serial_type_SYMS_CvGProc3216t; break;
case SYMS_CvSymKind_LPROC32:case SYMS_CvSymKind_GPROC32:
result = &_syms_serial_type_SYMS_CvProc32; break;
case SYMS_CvSymKind_REGREL32: result = &_syms_serial_type_SYMS_CvRegrel32; break;
case SYMS_CvSymKind_BUILDINFO: result = &_syms_serial_type_SYMS_CvBuildInfo; break;
case SYMS_CvSymKind_CALLEES:case SYMS_CvSymKind_CALLERS:
result = &_syms_serial_type_SYMS_CvFunctionList; break;
case SYMS_CvSymKind_INLINESITE: result = &_syms_serial_type_SYMS_CvInlineSite; break;
case SYMS_CvSymKind_INLINESITE2: result = &_syms_serial_type_SYMS_CvInlineSite2; break;
case SYMS_CvSymKind_INLINESITE_END: result = &_syms_serial_type_SYMS_CvInlineSiteEnd; break;
case SYMS_CvSymKind_INLINEES: result = &_syms_serial_type_SYMS_CvInlinees; break;
case SYMS_CvSymKind_FILESTATIC: result = &_syms_serial_type_SYMS_CvFileStatic; break;
case SYMS_CvSymKind_HEAPALLOCSITE: result = &_syms_serial_type_SYMS_CvHeapAllocSite; break;
case SYMS_CvSymKind_FASTLINK: result = &_syms_serial_type_SYMS_CvFastLink; break;
case SYMS_CvSymKind_ARMSWITCHTABLE: result = &_syms_serial_type_SYMS_CvArmSwitchTable; break;
case SYMS_CvSymKind_REF_MINIPDB: result = &_syms_serial_type_SYMS_CvRefMiniPdb; break;
}
return(result);
}
SYMS_API SYMS_SerialType*
syms_type_info_from_cv_leaf(SYMS_CvLeaf v){
SYMS_SerialType* result = 0;
switch (v){
default: break;
case SYMS_CvLeaf_PRECOMP: result = &_syms_serial_type_SYMS_CvLeafPreComp; break;
case SYMS_CvLeaf_TYPESERVER: result = &_syms_serial_type_SYMS_CvLeafTypeServer; break;
case SYMS_CvLeaf_TYPESERVER2: result = &_syms_serial_type_SYMS_CvLeafTypeServer2; break;
case SYMS_CvLeaf_BUILDINFO: result = &_syms_serial_type_SYMS_CvLeafBuildInfo; break;
case SYMS_CvLeaf_SKIP_16t: result = &_syms_serial_type_SYMS_CvLeafSkip_16t; break;
case SYMS_CvLeaf_SKIP: result = &_syms_serial_type_SYMS_CvLeafSkip; break;
case SYMS_CvLeaf_VTSHAPE: result = &_syms_serial_type_SYMS_CvLeafVTShape; break;
case SYMS_CvLeaf_LABEL: result = &_syms_serial_type_SYMS_CvLeafLabel; break;
case SYMS_CvLeaf_MODIFIER: result = &_syms_serial_type_SYMS_CvLeafModifier; break;
case SYMS_CvLeaf_POINTER: result = &_syms_serial_type_SYMS_CvLeafPointer; break;
case SYMS_CvLeaf_PROCEDURE: result = &_syms_serial_type_SYMS_CvLeafProcedure; break;
case SYMS_CvLeaf_MFUNCTION: result = &_syms_serial_type_SYMS_CvLeafMFunction; break;
case SYMS_CvLeaf_ARGLIST: result = &_syms_serial_type_SYMS_CvLeafArgList; break;
case SYMS_CvLeaf_BITFIELD: result = &_syms_serial_type_SYMS_CvLeafBitField; break;
case SYMS_CvLeaf_INDEX: result = &_syms_serial_type_SYMS_CvLeafIndex; break;
case SYMS_CvLeaf_ARRAY: result = &_syms_serial_type_SYMS_CvLeafArray; break;
case SYMS_CvLeaf_CLASS:case SYMS_CvLeaf_STRUCTURE:case SYMS_CvLeaf_INTERFACE:
result = &_syms_serial_type_SYMS_CvLeafStruct; break;
case SYMS_CvLeaf_UNION: result = &_syms_serial_type_SYMS_CvLeafUnion; break;
case SYMS_CvLeaf_ENUM: result = &_syms_serial_type_SYMS_CvLeafEnum; break;
case SYMS_CvLeaf_ALIAS: result = &_syms_serial_type_SYMS_CvLeafAlias; break;
case SYMS_CvLeaf_MEMBER: result = &_syms_serial_type_SYMS_CvLeafMember; break;
case SYMS_CvLeaf_STMEMBER: result = &_syms_serial_type_SYMS_CvLeafStMember; break;
case SYMS_CvLeaf_METHOD: result = &_syms_serial_type_SYMS_CvLeafMethod; break;
case SYMS_CvLeaf_ONEMETHOD: result = &_syms_serial_type_SYMS_CvLeafOneMethod; break;
case SYMS_CvLeaf_ENUMERATE: result = &_syms_serial_type_SYMS_CvLeafEnumerate; break;
case SYMS_CvLeaf_NESTTYPE: result = &_syms_serial_type_SYMS_CvLeafNestType; break;
case SYMS_CvLeaf_NESTTYPEEX: result = &_syms_serial_type_SYMS_CvLeafNestTypeEx; break;
case SYMS_CvLeaf_BCLASS: result = &_syms_serial_type_SYMS_CvLeafBClass; break;
case SYMS_CvLeaf_VBCLASS:case SYMS_CvLeaf_IVBCLASS:
result = &_syms_serial_type_SYMS_CvLeafVBClass; break;
case SYMS_CvLeaf_VFUNCTAB: result = &_syms_serial_type_SYMS_CvLeafVFuncTab; break;
case SYMS_CvLeaf_VFUNCOFF: result = &_syms_serial_type_SYMS_CvLeafVFuncOff; break;
case SYMS_CvLeaf_VFTABLE: result = &_syms_serial_type_SYMS_CvLeafVFTable; break;
case SYMS_CvLeaf_VFTPATH: result = &_syms_serial_type_SYMS_CvLeafVFPath; break;
case SYMS_CvLeaf_FUNC_ID: result = &_syms_serial_type_SYMS_CvLeafFuncId; break;
case SYMS_CvLeaf_MFUNC_ID: result = &_syms_serial_type_SYMS_CvLeafMFuncId; break;
case SYMS_CvLeaf_STRING_ID: result = &_syms_serial_type_SYMS_CvLeafStringId; break;
case SYMS_CvLeaf_UDT_SRC_LINE: result = &_syms_serial_type_SYMS_CvLeafUDTSrcLine; break;
case SYMS_CvLeaf_UDT_MOD_SRC_LINE: result = &_syms_serial_type_SYMS_CvLeafModSrcLine; break;
case SYMS_CvLeaf_CLASSPTR:case SYMS_CvLeaf_CLASSPTR2:
result = &_syms_serial_type_SYMS_CvLeafClassPtr; break;
}
return(result);
}

//~ generated from code at syms/metaprogram/syms_metaprogram_serial.c:1322
static SYMS_SerialField _syms_serial_members_for_SYMS_CvGuid[] = {
{ {(SYMS_U8*)"data1", 5}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"data2", 5}, &_syms_serial_type_SYMS_U16, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"data3", 5}, &_syms_serial_type_SYMS_U16, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"data4", 5}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"data5", 5}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
};
SYMS_SerialValue _syms_serial_members_for_SYMS_CvBasicPointerKind[] = {
{ { (SYMS_U8*)"VALUE", 5 }, (SYMS_U64)SYMS_CvBasicPointerKind_VALUE },
{ { (SYMS_U8*)"16BIT", 5 }, (SYMS_U64)SYMS_CvBasicPointerKind_16BIT },
{ { (SYMS_U8*)"FAR_16BIT", 9 }, (SYMS_U64)SYMS_CvBasicPointerKind_FAR_16BIT },
{ { (SYMS_U8*)"HUGE_16BIT", 10 }, (SYMS_U64)SYMS_CvBasicPointerKind_HUGE_16BIT },
{ { (SYMS_U8*)"32BIT", 5 }, (SYMS_U64)SYMS_CvBasicPointerKind_32BIT },
{ { (SYMS_U8*)"16_32BIT", 8 }, (SYMS_U64)SYMS_CvBasicPointerKind_16_32BIT },
{ { (SYMS_U8*)"64BIT", 5 }, (SYMS_U64)SYMS_CvBasicPointerKind_64BIT },
};
SYMS_SerialValue _syms_serial_members_for_SYMS_CvArch[] = {
{ { (SYMS_U8*)"8080", 4 }, (SYMS_U64)SYMS_CvArch_8080 },
{ { (SYMS_U8*)"8086", 4 }, (SYMS_U64)SYMS_CvArch_8086 },
{ { (SYMS_U8*)"80286", 5 }, (SYMS_U64)SYMS_CvArch_80286 },
{ { (SYMS_U8*)"80386", 5 }, (SYMS_U64)SYMS_CvArch_80386 },
{ { (SYMS_U8*)"80486", 5 }, (SYMS_U64)SYMS_CvArch_80486 },
{ { (SYMS_U8*)"PENTIUM", 7 }, (SYMS_U64)SYMS_CvArch_PENTIUM },
{ { (SYMS_U8*)"PENTIUMII", 9 }, (SYMS_U64)SYMS_CvArch_PENTIUMII },
{ { (SYMS_U8*)"PENTIUMPRO", 10 }, (SYMS_U64)SYMS_CvArch_PENTIUMPRO },
{ { (SYMS_U8*)"PENTIUMIII", 10 }, (SYMS_U64)SYMS_CvArch_PENTIUMIII },
{ { (SYMS_U8*)"MIPS", 4 }, (SYMS_U64)SYMS_CvArch_MIPS },
{ { (SYMS_U8*)"MIPSR4000", 9 }, (SYMS_U64)SYMS_CvArch_MIPSR4000 },
{ { (SYMS_U8*)"MIPS16", 6 }, (SYMS_U64)SYMS_CvArch_MIPS16 },
{ { (SYMS_U8*)"MIPS32", 6 }, (SYMS_U64)SYMS_CvArch_MIPS32 },
{ { (SYMS_U8*)"MIPS64", 6 }, (SYMS_U64)SYMS_CvArch_MIPS64 },
{ { (SYMS_U8*)"MIPSI", 5 }, (SYMS_U64)SYMS_CvArch_MIPSI },
{ { (SYMS_U8*)"MIPSII", 6 }, (SYMS_U64)SYMS_CvArch_MIPSII },
{ { (SYMS_U8*)"MIPSIII", 7 }, (SYMS_U64)SYMS_CvArch_MIPSIII },
{ { (SYMS_U8*)"MIPSIV", 6 }, (SYMS_U64)SYMS_CvArch_MIPSIV },
{ { (SYMS_U8*)"MIPSV", 5 }, (SYMS_U64)SYMS_CvArch_MIPSV },
{ { (SYMS_U8*)"M68000", 6 }, (SYMS_U64)SYMS_CvArch_M68000 },
{ { (SYMS_U8*)"M68010", 6 }, (SYMS_U64)SYMS_CvArch_M68010 },
{ { (SYMS_U8*)"M68020", 6 }, (SYMS_U64)SYMS_CvArch_M68020 },
{ { (SYMS_U8*)"M68030", 6 }, (SYMS_U64)SYMS_CvArch_M68030 },
{ { (SYMS_U8*)"M68040", 6 }, (SYMS_U64)SYMS_CvArch_M68040 },
{ { (SYMS_U8*)"ALPHA", 5 }, (SYMS_U64)SYMS_CvArch_ALPHA },
{ { (SYMS_U8*)"ALPHA_21064", 11 }, (SYMS_U64)SYMS_CvArch_ALPHA_21064 },
{ { (SYMS_U8*)"ALPHA_21164", 11 }, (SYMS_U64)SYMS_CvArch_ALPHA_21164 },
{ { (SYMS_U8*)"ALPHA_21164A", 12 }, (SYMS_U64)SYMS_CvArch_ALPHA_21164A },
{ { (SYMS_U8*)"ALPHA_21264", 11 }, (SYMS_U64)SYMS_CvArch_ALPHA_21264 },
{ { (SYMS_U8*)"ALPHA_21364", 11 }, (SYMS_U64)SYMS_CvArch_ALPHA_21364 },
{ { (SYMS_U8*)"PPC601", 6 }, (SYMS_U64)SYMS_CvArch_PPC601 },
{ { (SYMS_U8*)"PPC603", 6 }, (SYMS_U64)SYMS_CvArch_PPC603 },
{ { (SYMS_U8*)"PPC604", 6 }, (SYMS_U64)SYMS_CvArch_PPC604 },
{ { (SYMS_U8*)"PPC620", 6 }, (SYMS_U64)SYMS_CvArch_PPC620 },
{ { (SYMS_U8*)"PPCFP", 5 }, (SYMS_U64)SYMS_CvArch_PPCFP },
{ { (SYMS_U8*)"PPCBE", 5 }, (SYMS_U64)SYMS_CvArch_PPCBE },
{ { (SYMS_U8*)"SH3", 3 }, (SYMS_U64)SYMS_CvArch_SH3 },
{ { (SYMS_U8*)"SH3E", 4 }, (SYMS_U64)SYMS_CvArch_SH3E },
{ { (SYMS_U8*)"SH3DSP", 6 }, (SYMS_U64)SYMS_CvArch_SH3DSP },
{ { (SYMS_U8*)"SH4", 3 }, (SYMS_U64)SYMS_CvArch_SH4 },
{ { (SYMS_U8*)"SHMEDIA", 7 }, (SYMS_U64)SYMS_CvArch_SHMEDIA },
{ { (SYMS_U8*)"ARM3", 4 }, (SYMS_U64)SYMS_CvArch_ARM3 },
{ { (SYMS_U8*)"ARM4", 4 }, (SYMS_U64)SYMS_CvArch_ARM4 },
{ { (SYMS_U8*)"ARM4T", 5 }, (SYMS_U64)SYMS_CvArch_ARM4T },
{ { (SYMS_U8*)"ARM5", 4 }, (SYMS_U64)SYMS_CvArch_ARM5 },
{ { (SYMS_U8*)"ARM5T", 5 }, (SYMS_U64)SYMS_CvArch_ARM5T },
{ { (SYMS_U8*)"ARM6", 4 }, (SYMS_U64)SYMS_CvArch_ARM6 },
{ { (SYMS_U8*)"ARM_XMAC", 8 }, (SYMS_U64)SYMS_CvArch_ARM_XMAC },
{ { (SYMS_U8*)"ARM_WMMX", 8 }, (SYMS_U64)SYMS_CvArch_ARM_WMMX },
{ { (SYMS_U8*)"ARM7", 4 }, (SYMS_U64)SYMS_CvArch_ARM7 },
{ { (SYMS_U8*)"OMNI", 4 }, (SYMS_U64)SYMS_CvArch_OMNI },
{ { (SYMS_U8*)"IA64", 4 }, (SYMS_U64)SYMS_CvArch_IA64 },
{ { (SYMS_U8*)"IA64_1", 6 }, (SYMS_U64)SYMS_CvArch_IA64_1 },
{ { (SYMS_U8*)"IA64_2", 6 }, (SYMS_U64)SYMS_CvArch_IA64_2 },
{ { (SYMS_U8*)"CEE", 3 }, (SYMS_U64)SYMS_CvArch_CEE },
{ { (SYMS_U8*)"AM33", 4 }, (SYMS_U64)SYMS_CvArch_AM33 },
{ { (SYMS_U8*)"M32R", 4 }, (SYMS_U64)SYMS_CvArch_M32R },
{ { (SYMS_U8*)"TRICORE", 7 }, (SYMS_U64)SYMS_CvArch_TRICORE },
{ { (SYMS_U8*)"X64", 3 }, (SYMS_U64)SYMS_CvArch_X64 },
{ { (SYMS_U8*)"AMD64", 5 }, (SYMS_U64)SYMS_CvArch_AMD64 },
{ { (SYMS_U8*)"EBC", 3 }, (SYMS_U64)SYMS_CvArch_EBC },
{ { (SYMS_U8*)"THUMB", 5 }, (SYMS_U64)SYMS_CvArch_THUMB },
{ { (SYMS_U8*)"ARMNT", 5 }, (SYMS_U64)SYMS_CvArch_ARMNT },
{ { (SYMS_U8*)"ARM64", 5 }, (SYMS_U64)SYMS_CvArch_ARM64 },
{ { (SYMS_U8*)"D3D11_SHADER", 12 }, (SYMS_U64)SYMS_CvArch_D3D11_SHADER },
};
SYMS_SerialValue _syms_serial_members_for_SYMS_CvAllReg[] = {
{ { (SYMS_U8*)"ERR", 3 }, (SYMS_U64)SYMS_CvAllReg_ERR },
{ { (SYMS_U8*)"TEB", 3 }, (SYMS_U64)SYMS_CvAllReg_TEB },
{ { (SYMS_U8*)"TIMER", 5 }, (SYMS_U64)SYMS_CvAllReg_TIMER },
{ { (SYMS_U8*)"EFAD1", 5 }, (SYMS_U64)SYMS_CvAllReg_EFAD1 },
{ { (SYMS_U8*)"EFAD2", 5 }, (SYMS_U64)SYMS_CvAllReg_EFAD2 },
{ { (SYMS_U8*)"EFAD3", 5 }, (SYMS_U64)SYMS_CvAllReg_EFAD3 },
{ { (SYMS_U8*)"VFRAME", 6 }, (SYMS_U64)SYMS_CvAllReg_VFRAME },
{ { (SYMS_U8*)"HANDLE", 6 }, (SYMS_U64)SYMS_CvAllReg_HANDLE },
{ { (SYMS_U8*)"PARAMS", 6 }, (SYMS_U64)SYMS_CvAllReg_PARAMS },
{ { (SYMS_U8*)"LOCALS", 6 }, (SYMS_U64)SYMS_CvAllReg_LOCALS },
{ { (SYMS_U8*)"TID", 3 }, (SYMS_U64)SYMS_CvAllReg_TID },
{ { (SYMS_U8*)"ENV", 3 }, (SYMS_U64)SYMS_CvAllReg_ENV },
{ { (SYMS_U8*)"CMDLN", 5 }, (SYMS_U64)SYMS_CvAllReg_CMDLN },
};
SYMS_SerialValue _syms_serial_members_for_SYMS_CvRegx86[] = {
{ { (SYMS_U8*)"NONE", 4 }, (SYMS_U64)SYMS_CvRegx86_NONE },
{ { (SYMS_U8*)"AL", 2 }, (SYMS_U64)SYMS_CvRegx86_AL },
{ { (SYMS_U8*)"CL", 2 }, (SYMS_U64)SYMS_CvRegx86_CL },
{ { (SYMS_U8*)"DL", 2 }, (SYMS_U64)SYMS_CvRegx86_DL },
{ { (SYMS_U8*)"BL", 2 }, (SYMS_U64)SYMS_CvRegx86_BL },
{ { (SYMS_U8*)"AH", 2 }, (SYMS_U64)SYMS_CvRegx86_AH },
{ { (SYMS_U8*)"CH", 2 }, (SYMS_U64)SYMS_CvRegx86_CH },
{ { (SYMS_U8*)"DH", 2 }, (SYMS_U64)SYMS_CvRegx86_DH },
{ { (SYMS_U8*)"BH", 2 }, (SYMS_U64)SYMS_CvRegx86_BH },
{ { (SYMS_U8*)"AX", 2 }, (SYMS_U64)SYMS_CvRegx86_AX },
{ { (SYMS_U8*)"CX", 2 }, (SYMS_U64)SYMS_CvRegx86_CX },
{ { (SYMS_U8*)"DX", 2 }, (SYMS_U64)SYMS_CvRegx86_DX },
{ { (SYMS_U8*)"BX", 2 }, (SYMS_U64)SYMS_CvRegx86_BX },
{ { (SYMS_U8*)"SP", 2 }, (SYMS_U64)SYMS_CvRegx86_SP },
{ { (SYMS_U8*)"BP", 2 }, (SYMS_U64)SYMS_CvRegx86_BP },
{ { (SYMS_U8*)"SI", 2 }, (SYMS_U64)SYMS_CvRegx86_SI },
{ { (SYMS_U8*)"DI", 2 }, (SYMS_U64)SYMS_CvRegx86_DI },
{ { (SYMS_U8*)"EAX", 3 }, (SYMS_U64)SYMS_CvRegx86_EAX },
{ { (SYMS_U8*)"ECX", 3 }, (SYMS_U64)SYMS_CvRegx86_ECX },
{ { (SYMS_U8*)"EDX", 3 }, (SYMS_U64)SYMS_CvRegx86_EDX },
{ { (SYMS_U8*)"EBX", 3 }, (SYMS_U64)SYMS_CvRegx86_EBX },
{ { (SYMS_U8*)"ESP", 3 }, (SYMS_U64)SYMS_CvRegx86_ESP },
{ { (SYMS_U8*)"EBP", 3 }, (SYMS_U64)SYMS_CvRegx86_EBP },
{ { (SYMS_U8*)"ESI", 3 }, (SYMS_U64)SYMS_CvRegx86_ESI },
{ { (SYMS_U8*)"EDI", 3 }, (SYMS_U64)SYMS_CvRegx86_EDI },
{ { (SYMS_U8*)"ES", 2 }, (SYMS_U64)SYMS_CvRegx86_ES },
{ { (SYMS_U8*)"CS", 2 }, (SYMS_U64)SYMS_CvRegx86_CS },
{ { (SYMS_U8*)"SS", 2 }, (SYMS_U64)SYMS_CvRegx86_SS },
{ { (SYMS_U8*)"DS", 2 }, (SYMS_U64)SYMS_CvRegx86_DS },
{ { (SYMS_U8*)"FS", 2 }, (SYMS_U64)SYMS_CvRegx86_FS },
{ { (SYMS_U8*)"GS", 2 }, (SYMS_U64)SYMS_CvRegx86_GS },
{ { (SYMS_U8*)"IP", 2 }, (SYMS_U64)SYMS_CvRegx86_IP },
{ { (SYMS_U8*)"FLAGS", 5 }, (SYMS_U64)SYMS_CvRegx86_FLAGS },
{ { (SYMS_U8*)"EIP", 3 }, (SYMS_U64)SYMS_CvRegx86_EIP },
{ { (SYMS_U8*)"EFLAGS", 6 }, (SYMS_U64)SYMS_CvRegx86_EFLAGS },
{ { (SYMS_U8*)"MM0", 3 }, (SYMS_U64)SYMS_CvRegx86_MM0 },
{ { (SYMS_U8*)"MM1", 3 }, (SYMS_U64)SYMS_CvRegx86_MM1 },
{ { (SYMS_U8*)"MM2", 3 }, (SYMS_U64)SYMS_CvRegx86_MM2 },
{ { (SYMS_U8*)"MM3", 3 }, (SYMS_U64)SYMS_CvRegx86_MM3 },
{ { (SYMS_U8*)"MM4", 3 }, (SYMS_U64)SYMS_CvRegx86_MM4 },
{ { (SYMS_U8*)"MM5", 3 }, (SYMS_U64)SYMS_CvRegx86_MM5 },
{ { (SYMS_U8*)"MM6", 3 }, (SYMS_U64)SYMS_CvRegx86_MM6 },
{ { (SYMS_U8*)"MM7", 3 }, (SYMS_U64)SYMS_CvRegx86_MM7 },
{ { (SYMS_U8*)"XMM0", 4 }, (SYMS_U64)SYMS_CvRegx86_XMM0 },
{ { (SYMS_U8*)"XMM1", 4 }, (SYMS_U64)SYMS_CvRegx86_XMM1 },
{ { (SYMS_U8*)"XMM2", 4 }, (SYMS_U64)SYMS_CvRegx86_XMM2 },
{ { (SYMS_U8*)"XMM3", 4 }, (SYMS_U64)SYMS_CvRegx86_XMM3 },
{ { (SYMS_U8*)"XMM4", 4 }, (SYMS_U64)SYMS_CvRegx86_XMM4 },
{ { (SYMS_U8*)"XMM5", 4 }, (SYMS_U64)SYMS_CvRegx86_XMM5 },
{ { (SYMS_U8*)"XMM6", 4 }, (SYMS_U64)SYMS_CvRegx86_XMM6 },
{ { (SYMS_U8*)"XMM7", 4 }, (SYMS_U64)SYMS_CvRegx86_XMM7 },
{ { (SYMS_U8*)"XMM00", 5 }, (SYMS_U64)SYMS_CvRegx86_XMM00 },
{ { (SYMS_U8*)"XMM01", 5 }, (SYMS_U64)SYMS_CvRegx86_XMM01 },
{ { (SYMS_U8*)"XMM02", 5 }, (SYMS_U64)SYMS_CvRegx86_XMM02 },
{ { (SYMS_U8*)"XMM03", 5 }, (SYMS_U64)SYMS_CvRegx86_XMM03 },
{ { (SYMS_U8*)"XMM10", 5 }, (SYMS_U64)SYMS_CvRegx86_XMM10 },
{ { (SYMS_U8*)"XMM11", 5 }, (SYMS_U64)SYMS_CvRegx86_XMM11 },
{ { (SYMS_U8*)"XMM12", 5 }, (SYMS_U64)SYMS_CvRegx86_XMM12 },
{ { (SYMS_U8*)"XMM13", 5 }, (SYMS_U64)SYMS_CvRegx86_XMM13 },
{ { (SYMS_U8*)"XMM20", 5 }, (SYMS_U64)SYMS_CvRegx86_XMM20 },
{ { (SYMS_U8*)"XMM21", 5 }, (SYMS_U64)SYMS_CvRegx86_XMM21 },
{ { (SYMS_U8*)"XMM22", 5 }, (SYMS_U64)SYMS_CvRegx86_XMM22 },
{ { (SYMS_U8*)"XMM23", 5 }, (SYMS_U64)SYMS_CvRegx86_XMM23 },
{ { (SYMS_U8*)"XMM30", 5 }, (SYMS_U64)SYMS_CvRegx86_XMM30 },
{ { (SYMS_U8*)"XMM31", 5 }, (SYMS_U64)SYMS_CvRegx86_XMM31 },
{ { (SYMS_U8*)"XMM32", 5 }, (SYMS_U64)SYMS_CvRegx86_XMM32 },
{ { (SYMS_U8*)"XMM33", 5 }, (SYMS_U64)SYMS_CvRegx86_XMM33 },
{ { (SYMS_U8*)"XMM40", 5 }, (SYMS_U64)SYMS_CvRegx86_XMM40 },
{ { (SYMS_U8*)"XMM41", 5 }, (SYMS_U64)SYMS_CvRegx86_XMM41 },
{ { (SYMS_U8*)"XMM42", 5 }, (SYMS_U64)SYMS_CvRegx86_XMM42 },
{ { (SYMS_U8*)"XMM43", 5 }, (SYMS_U64)SYMS_CvRegx86_XMM43 },
{ { (SYMS_U8*)"XMM50", 5 }, (SYMS_U64)SYMS_CvRegx86_XMM50 },
{ { (SYMS_U8*)"XMM51", 5 }, (SYMS_U64)SYMS_CvRegx86_XMM51 },
{ { (SYMS_U8*)"XMM52", 5 }, (SYMS_U64)SYMS_CvRegx86_XMM52 },
{ { (SYMS_U8*)"XMM53", 5 }, (SYMS_U64)SYMS_CvRegx86_XMM53 },
{ { (SYMS_U8*)"XMM60", 5 }, (SYMS_U64)SYMS_CvRegx86_XMM60 },
{ { (SYMS_U8*)"XMM61", 5 }, (SYMS_U64)SYMS_CvRegx86_XMM61 },
{ { (SYMS_U8*)"XMM62", 5 }, (SYMS_U64)SYMS_CvRegx86_XMM62 },
{ { (SYMS_U8*)"XMM63", 5 }, (SYMS_U64)SYMS_CvRegx86_XMM63 },
{ { (SYMS_U8*)"XMM70", 5 }, (SYMS_U64)SYMS_CvRegx86_XMM70 },
{ { (SYMS_U8*)"XMM71", 5 }, (SYMS_U64)SYMS_CvRegx86_XMM71 },
{ { (SYMS_U8*)"XMM72", 5 }, (SYMS_U64)SYMS_CvRegx86_XMM72 },
{ { (SYMS_U8*)"XMM73", 5 }, (SYMS_U64)SYMS_CvRegx86_XMM73 },
{ { (SYMS_U8*)"XMM0L", 5 }, (SYMS_U64)SYMS_CvRegx86_XMM0L },
{ { (SYMS_U8*)"XMM1L", 5 }, (SYMS_U64)SYMS_CvRegx86_XMM1L },
{ { (SYMS_U8*)"XMM2L", 5 }, (SYMS_U64)SYMS_CvRegx86_XMM2L },
{ { (SYMS_U8*)"XMM3L", 5 }, (SYMS_U64)SYMS_CvRegx86_XMM3L },
{ { (SYMS_U8*)"XMM4L", 5 }, (SYMS_U64)SYMS_CvRegx86_XMM4L },
{ { (SYMS_U8*)"XMM5L", 5 }, (SYMS_U64)SYMS_CvRegx86_XMM5L },
{ { (SYMS_U8*)"XMM6L", 5 }, (SYMS_U64)SYMS_CvRegx86_XMM6L },
{ { (SYMS_U8*)"XMM7L", 5 }, (SYMS_U64)SYMS_CvRegx86_XMM7L },
{ { (SYMS_U8*)"XMM0H", 5 }, (SYMS_U64)SYMS_CvRegx86_XMM0H },
{ { (SYMS_U8*)"XMM1H", 5 }, (SYMS_U64)SYMS_CvRegx86_XMM1H },
{ { (SYMS_U8*)"XMM2H", 5 }, (SYMS_U64)SYMS_CvRegx86_XMM2H },
{ { (SYMS_U8*)"XMM3H", 5 }, (SYMS_U64)SYMS_CvRegx86_XMM3H },
{ { (SYMS_U8*)"XMM4H", 5 }, (SYMS_U64)SYMS_CvRegx86_XMM4H },
{ { (SYMS_U8*)"XMM5H", 5 }, (SYMS_U64)SYMS_CvRegx86_XMM5H },
{ { (SYMS_U8*)"XMM6H", 5 }, (SYMS_U64)SYMS_CvRegx86_XMM6H },
{ { (SYMS_U8*)"XMM7H", 5 }, (SYMS_U64)SYMS_CvRegx86_XMM7H },
{ { (SYMS_U8*)"YMM0", 4 }, (SYMS_U64)SYMS_CvRegx86_YMM0 },
{ { (SYMS_U8*)"YMM1", 4 }, (SYMS_U64)SYMS_CvRegx86_YMM1 },
{ { (SYMS_U8*)"YMM2", 4 }, (SYMS_U64)SYMS_CvRegx86_YMM2 },
{ { (SYMS_U8*)"YMM3", 4 }, (SYMS_U64)SYMS_CvRegx86_YMM3 },
{ { (SYMS_U8*)"YMM4", 4 }, (SYMS_U64)SYMS_CvRegx86_YMM4 },
{ { (SYMS_U8*)"YMM5", 4 }, (SYMS_U64)SYMS_CvRegx86_YMM5 },
{ { (SYMS_U8*)"YMM6", 4 }, (SYMS_U64)SYMS_CvRegx86_YMM6 },
{ { (SYMS_U8*)"YMM7", 4 }, (SYMS_U64)SYMS_CvRegx86_YMM7 },
{ { (SYMS_U8*)"YMM0H", 5 }, (SYMS_U64)SYMS_CvRegx86_YMM0H },
{ { (SYMS_U8*)"YMM1H", 5 }, (SYMS_U64)SYMS_CvRegx86_YMM1H },
{ { (SYMS_U8*)"YMM2H", 5 }, (SYMS_U64)SYMS_CvRegx86_YMM2H },
{ { (SYMS_U8*)"YMM3H", 5 }, (SYMS_U64)SYMS_CvRegx86_YMM3H },
{ { (SYMS_U8*)"YMM4H", 5 }, (SYMS_U64)SYMS_CvRegx86_YMM4H },
{ { (SYMS_U8*)"YMM5H", 5 }, (SYMS_U64)SYMS_CvRegx86_YMM5H },
{ { (SYMS_U8*)"YMM6H", 5 }, (SYMS_U64)SYMS_CvRegx86_YMM6H },
{ { (SYMS_U8*)"YMM7H", 5 }, (SYMS_U64)SYMS_CvRegx86_YMM7H },
{ { (SYMS_U8*)"YMM0I0", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM0I0 },
{ { (SYMS_U8*)"YMM0I1", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM0I1 },
{ { (SYMS_U8*)"YMM0I2", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM0I2 },
{ { (SYMS_U8*)"YMM0I3", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM0I3 },
{ { (SYMS_U8*)"YMM1I0", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM1I0 },
{ { (SYMS_U8*)"YMM1I1", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM1I1 },
{ { (SYMS_U8*)"YMM1I2", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM1I2 },
{ { (SYMS_U8*)"YMM1I3", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM1I3 },
{ { (SYMS_U8*)"YMM2I0", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM2I0 },
{ { (SYMS_U8*)"YMM2I1", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM2I1 },
{ { (SYMS_U8*)"YMM2I2", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM2I2 },
{ { (SYMS_U8*)"YMM2I3", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM2I3 },
{ { (SYMS_U8*)"YMM3I0", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM3I0 },
{ { (SYMS_U8*)"YMM3I1", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM3I1 },
{ { (SYMS_U8*)"YMM3I2", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM3I2 },
{ { (SYMS_U8*)"YMM3I3", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM3I3 },
{ { (SYMS_U8*)"YMM4I0", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM4I0 },
{ { (SYMS_U8*)"YMM4I1", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM4I1 },
{ { (SYMS_U8*)"YMM4I2", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM4I2 },
{ { (SYMS_U8*)"YMM4I3", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM4I3 },
{ { (SYMS_U8*)"YMM5I0", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM5I0 },
{ { (SYMS_U8*)"YMM5I1", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM5I1 },
{ { (SYMS_U8*)"YMM5I2", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM5I2 },
{ { (SYMS_U8*)"YMM5I3", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM5I3 },
{ { (SYMS_U8*)"YMM6I0", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM6I0 },
{ { (SYMS_U8*)"YMM6I1", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM6I1 },
{ { (SYMS_U8*)"YMM6I2", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM6I2 },
{ { (SYMS_U8*)"YMM6I3", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM6I3 },
{ { (SYMS_U8*)"YMM7I0", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM7I0 },
{ { (SYMS_U8*)"YMM7I1", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM7I1 },
{ { (SYMS_U8*)"YMM7I2", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM7I2 },
{ { (SYMS_U8*)"YMM7I3", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM7I3 },
{ { (SYMS_U8*)"YMM0F0", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM0F0 },
{ { (SYMS_U8*)"YMM0F1", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM0F1 },
{ { (SYMS_U8*)"YMM0F2", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM0F2 },
{ { (SYMS_U8*)"YMM0F3", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM0F3 },
{ { (SYMS_U8*)"YMM0F4", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM0F4 },
{ { (SYMS_U8*)"YMM0F5", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM0F5 },
{ { (SYMS_U8*)"YMM0F6", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM0F6 },
{ { (SYMS_U8*)"YMM0F7", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM0F7 },
{ { (SYMS_U8*)"YMM1F0", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM1F0 },
{ { (SYMS_U8*)"YMM1F1", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM1F1 },
{ { (SYMS_U8*)"YMM1F2", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM1F2 },
{ { (SYMS_U8*)"YMM1F3", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM1F3 },
{ { (SYMS_U8*)"YMM1F4", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM1F4 },
{ { (SYMS_U8*)"YMM1F5", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM1F5 },
{ { (SYMS_U8*)"YMM1F6", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM1F6 },
{ { (SYMS_U8*)"YMM1F7", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM1F7 },
{ { (SYMS_U8*)"YMM2F0", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM2F0 },
{ { (SYMS_U8*)"YMM2F1", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM2F1 },
{ { (SYMS_U8*)"YMM2F2", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM2F2 },
{ { (SYMS_U8*)"YMM2F3", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM2F3 },
{ { (SYMS_U8*)"YMM2F4", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM2F4 },
{ { (SYMS_U8*)"YMM2F5", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM2F5 },
{ { (SYMS_U8*)"YMM2F6", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM2F6 },
{ { (SYMS_U8*)"YMM2F7", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM2F7 },
{ { (SYMS_U8*)"YMM3F0", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM3F0 },
{ { (SYMS_U8*)"YMM3F1", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM3F1 },
{ { (SYMS_U8*)"YMM3F2", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM3F2 },
{ { (SYMS_U8*)"YMM3F3", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM3F3 },
{ { (SYMS_U8*)"YMM3F4", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM3F4 },
{ { (SYMS_U8*)"YMM3F5", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM3F5 },
{ { (SYMS_U8*)"YMM3F6", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM3F6 },
{ { (SYMS_U8*)"YMM3F7", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM3F7 },
{ { (SYMS_U8*)"YMM4F0", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM4F0 },
{ { (SYMS_U8*)"YMM4F1", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM4F1 },
{ { (SYMS_U8*)"YMM4F2", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM4F2 },
{ { (SYMS_U8*)"YMM4F3", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM4F3 },
{ { (SYMS_U8*)"YMM4F4", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM4F4 },
{ { (SYMS_U8*)"YMM4F5", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM4F5 },
{ { (SYMS_U8*)"YMM4F6", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM4F6 },
{ { (SYMS_U8*)"YMM4F7", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM4F7 },
{ { (SYMS_U8*)"YMM5F0", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM5F0 },
{ { (SYMS_U8*)"YMM5F1", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM5F1 },
{ { (SYMS_U8*)"YMM5F2", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM5F2 },
{ { (SYMS_U8*)"YMM5F3", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM5F3 },
{ { (SYMS_U8*)"YMM5F4", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM5F4 },
{ { (SYMS_U8*)"YMM5F5", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM5F5 },
{ { (SYMS_U8*)"YMM5F6", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM5F6 },
{ { (SYMS_U8*)"YMM5F7", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM5F7 },
{ { (SYMS_U8*)"YMM6F0", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM6F0 },
{ { (SYMS_U8*)"YMM6F1", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM6F1 },
{ { (SYMS_U8*)"YMM6F2", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM6F2 },
{ { (SYMS_U8*)"YMM6F3", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM6F3 },
{ { (SYMS_U8*)"YMM6F4", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM6F4 },
{ { (SYMS_U8*)"YMM6F5", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM6F5 },
{ { (SYMS_U8*)"YMM6F6", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM6F6 },
{ { (SYMS_U8*)"YMM6F7", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM6F7 },
{ { (SYMS_U8*)"YMM7F0", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM7F0 },
{ { (SYMS_U8*)"YMM7F1", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM7F1 },
{ { (SYMS_U8*)"YMM7F2", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM7F2 },
{ { (SYMS_U8*)"YMM7F3", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM7F3 },
{ { (SYMS_U8*)"YMM7F4", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM7F4 },
{ { (SYMS_U8*)"YMM7F5", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM7F5 },
{ { (SYMS_U8*)"YMM7F6", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM7F6 },
{ { (SYMS_U8*)"YMM7F7", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM7F7 },
{ { (SYMS_U8*)"YMM0D0", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM0D0 },
{ { (SYMS_U8*)"YMM0D1", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM0D1 },
{ { (SYMS_U8*)"YMM0D2", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM0D2 },
{ { (SYMS_U8*)"YMM0D3", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM0D3 },
{ { (SYMS_U8*)"YMM1D0", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM1D0 },
{ { (SYMS_U8*)"YMM1D1", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM1D1 },
{ { (SYMS_U8*)"YMM1D2", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM1D2 },
{ { (SYMS_U8*)"YMM1D3", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM1D3 },
{ { (SYMS_U8*)"YMM2D0", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM2D0 },
{ { (SYMS_U8*)"YMM2D1", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM2D1 },
{ { (SYMS_U8*)"YMM2D2", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM2D2 },
{ { (SYMS_U8*)"YMM2D3", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM2D3 },
{ { (SYMS_U8*)"YMM3D0", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM3D0 },
{ { (SYMS_U8*)"YMM3D1", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM3D1 },
{ { (SYMS_U8*)"YMM3D2", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM3D2 },
{ { (SYMS_U8*)"YMM3D3", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM3D3 },
{ { (SYMS_U8*)"YMM4D0", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM4D0 },
{ { (SYMS_U8*)"YMM4D1", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM4D1 },
{ { (SYMS_U8*)"YMM4D2", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM4D2 },
{ { (SYMS_U8*)"YMM4D3", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM4D3 },
{ { (SYMS_U8*)"YMM5D0", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM5D0 },
{ { (SYMS_U8*)"YMM5D1", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM5D1 },
{ { (SYMS_U8*)"YMM5D2", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM5D2 },
{ { (SYMS_U8*)"YMM5D3", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM5D3 },
{ { (SYMS_U8*)"YMM6D0", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM6D0 },
{ { (SYMS_U8*)"YMM6D1", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM6D1 },
{ { (SYMS_U8*)"YMM6D2", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM6D2 },
{ { (SYMS_U8*)"YMM6D3", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM6D3 },
{ { (SYMS_U8*)"YMM7D0", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM7D0 },
{ { (SYMS_U8*)"YMM7D1", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM7D1 },
{ { (SYMS_U8*)"YMM7D2", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM7D2 },
{ { (SYMS_U8*)"YMM7D3", 6 }, (SYMS_U64)SYMS_CvRegx86_YMM7D3 },
};
SYMS_SerialValue _syms_serial_members_for_SYMS_CvRegx64[] = {
{ { (SYMS_U8*)"NONE", 4 }, (SYMS_U64)SYMS_CvRegx64_NONE },
{ { (SYMS_U8*)"AL", 2 }, (SYMS_U64)SYMS_CvRegx64_AL },
{ { (SYMS_U8*)"CL", 2 }, (SYMS_U64)SYMS_CvRegx64_CL },
{ { (SYMS_U8*)"DL", 2 }, (SYMS_U64)SYMS_CvRegx64_DL },
{ { (SYMS_U8*)"BL", 2 }, (SYMS_U64)SYMS_CvRegx64_BL },
{ { (SYMS_U8*)"AH", 2 }, (SYMS_U64)SYMS_CvRegx64_AH },
{ { (SYMS_U8*)"CH", 2 }, (SYMS_U64)SYMS_CvRegx64_CH },
{ { (SYMS_U8*)"DH", 2 }, (SYMS_U64)SYMS_CvRegx64_DH },
{ { (SYMS_U8*)"BH", 2 }, (SYMS_U64)SYMS_CvRegx64_BH },
{ { (SYMS_U8*)"AX", 2 }, (SYMS_U64)SYMS_CvRegx64_AX },
{ { (SYMS_U8*)"CX", 2 }, (SYMS_U64)SYMS_CvRegx64_CX },
{ { (SYMS_U8*)"DX", 2 }, (SYMS_U64)SYMS_CvRegx64_DX },
{ { (SYMS_U8*)"BX", 2 }, (SYMS_U64)SYMS_CvRegx64_BX },
{ { (SYMS_U8*)"SP", 2 }, (SYMS_U64)SYMS_CvRegx64_SP },
{ { (SYMS_U8*)"BP", 2 }, (SYMS_U64)SYMS_CvRegx64_BP },
{ { (SYMS_U8*)"SI", 2 }, (SYMS_U64)SYMS_CvRegx64_SI },
{ { (SYMS_U8*)"DI", 2 }, (SYMS_U64)SYMS_CvRegx64_DI },
{ { (SYMS_U8*)"EAX", 3 }, (SYMS_U64)SYMS_CvRegx64_EAX },
{ { (SYMS_U8*)"ECX", 3 }, (SYMS_U64)SYMS_CvRegx64_ECX },
{ { (SYMS_U8*)"EDX", 3 }, (SYMS_U64)SYMS_CvRegx64_EDX },
{ { (SYMS_U8*)"EBX", 3 }, (SYMS_U64)SYMS_CvRegx64_EBX },
{ { (SYMS_U8*)"ESP", 3 }, (SYMS_U64)SYMS_CvRegx64_ESP },
{ { (SYMS_U8*)"EBP", 3 }, (SYMS_U64)SYMS_CvRegx64_EBP },
{ { (SYMS_U8*)"ESI", 3 }, (SYMS_U64)SYMS_CvRegx64_ESI },
{ { (SYMS_U8*)"EDI", 3 }, (SYMS_U64)SYMS_CvRegx64_EDI },
{ { (SYMS_U8*)"ES", 2 }, (SYMS_U64)SYMS_CvRegx64_ES },
{ { (SYMS_U8*)"CS", 2 }, (SYMS_U64)SYMS_CvRegx64_CS },
{ { (SYMS_U8*)"SS", 2 }, (SYMS_U64)SYMS_CvRegx64_SS },
{ { (SYMS_U8*)"DS", 2 }, (SYMS_U64)SYMS_CvRegx64_DS },
{ { (SYMS_U8*)"FS", 2 }, (SYMS_U64)SYMS_CvRegx64_FS },
{ { (SYMS_U8*)"GS", 2 }, (SYMS_U64)SYMS_CvRegx64_GS },
{ { (SYMS_U8*)"FLAGS", 5 }, (SYMS_U64)SYMS_CvRegx64_FLAGS },
{ { (SYMS_U8*)"RIP", 3 }, (SYMS_U64)SYMS_CvRegx64_RIP },
{ { (SYMS_U8*)"EFLAGS", 6 }, (SYMS_U64)SYMS_CvRegx64_EFLAGS },
{ { (SYMS_U8*)"CR0", 3 }, (SYMS_U64)SYMS_CvRegx64_CR0 },
{ { (SYMS_U8*)"CR1", 3 }, (SYMS_U64)SYMS_CvRegx64_CR1 },
{ { (SYMS_U8*)"CR2", 3 }, (SYMS_U64)SYMS_CvRegx64_CR2 },
{ { (SYMS_U8*)"CR3", 3 }, (SYMS_U64)SYMS_CvRegx64_CR3 },
{ { (SYMS_U8*)"CR4", 3 }, (SYMS_U64)SYMS_CvRegx64_CR4 },
{ { (SYMS_U8*)"CR8", 3 }, (SYMS_U64)SYMS_CvRegx64_CR8 },
{ { (SYMS_U8*)"DR0", 3 }, (SYMS_U64)SYMS_CvRegx64_DR0 },
{ { (SYMS_U8*)"DR1", 3 }, (SYMS_U64)SYMS_CvRegx64_DR1 },
{ { (SYMS_U8*)"DR2", 3 }, (SYMS_U64)SYMS_CvRegx64_DR2 },
{ { (SYMS_U8*)"DR3", 3 }, (SYMS_U64)SYMS_CvRegx64_DR3 },
{ { (SYMS_U8*)"DR4", 3 }, (SYMS_U64)SYMS_CvRegx64_DR4 },
{ { (SYMS_U8*)"DR5", 3 }, (SYMS_U64)SYMS_CvRegx64_DR5 },
{ { (SYMS_U8*)"DR6", 3 }, (SYMS_U64)SYMS_CvRegx64_DR6 },
{ { (SYMS_U8*)"DR7", 3 }, (SYMS_U64)SYMS_CvRegx64_DR7 },
{ { (SYMS_U8*)"DR8", 3 }, (SYMS_U64)SYMS_CvRegx64_DR8 },
{ { (SYMS_U8*)"DR9", 3 }, (SYMS_U64)SYMS_CvRegx64_DR9 },
{ { (SYMS_U8*)"DR10", 4 }, (SYMS_U64)SYMS_CvRegx64_DR10 },
{ { (SYMS_U8*)"DR11", 4 }, (SYMS_U64)SYMS_CvRegx64_DR11 },
{ { (SYMS_U8*)"DR12", 4 }, (SYMS_U64)SYMS_CvRegx64_DR12 },
{ { (SYMS_U8*)"DR13", 4 }, (SYMS_U64)SYMS_CvRegx64_DR13 },
{ { (SYMS_U8*)"DR14", 4 }, (SYMS_U64)SYMS_CvRegx64_DR14 },
{ { (SYMS_U8*)"DR15", 4 }, (SYMS_U64)SYMS_CvRegx64_DR15 },
{ { (SYMS_U8*)"GDTR", 4 }, (SYMS_U64)SYMS_CvRegx64_GDTR },
{ { (SYMS_U8*)"GDTL", 4 }, (SYMS_U64)SYMS_CvRegx64_GDTL },
{ { (SYMS_U8*)"IDTR", 4 }, (SYMS_U64)SYMS_CvRegx64_IDTR },
{ { (SYMS_U8*)"IDTL", 4 }, (SYMS_U64)SYMS_CvRegx64_IDTL },
{ { (SYMS_U8*)"LDTR", 4 }, (SYMS_U64)SYMS_CvRegx64_LDTR },
{ { (SYMS_U8*)"TR", 2 }, (SYMS_U64)SYMS_CvRegx64_TR },
{ { (SYMS_U8*)"ST0", 3 }, (SYMS_U64)SYMS_CvRegx64_ST0 },
{ { (SYMS_U8*)"ST1", 3 }, (SYMS_U64)SYMS_CvRegx64_ST1 },
{ { (SYMS_U8*)"ST2", 3 }, (SYMS_U64)SYMS_CvRegx64_ST2 },
{ { (SYMS_U8*)"ST3", 3 }, (SYMS_U64)SYMS_CvRegx64_ST3 },
{ { (SYMS_U8*)"ST4", 3 }, (SYMS_U64)SYMS_CvRegx64_ST4 },
{ { (SYMS_U8*)"ST5", 3 }, (SYMS_U64)SYMS_CvRegx64_ST5 },
{ { (SYMS_U8*)"ST6", 3 }, (SYMS_U64)SYMS_CvRegx64_ST6 },
{ { (SYMS_U8*)"ST7", 3 }, (SYMS_U64)SYMS_CvRegx64_ST7 },
{ { (SYMS_U8*)"CTRL", 4 }, (SYMS_U64)SYMS_CvRegx64_CTRL },
{ { (SYMS_U8*)"STAT", 4 }, (SYMS_U64)SYMS_CvRegx64_STAT },
{ { (SYMS_U8*)"TAG", 3 }, (SYMS_U64)SYMS_CvRegx64_TAG },
{ { (SYMS_U8*)"FPIP", 4 }, (SYMS_U64)SYMS_CvRegx64_FPIP },
{ { (SYMS_U8*)"FPCS", 4 }, (SYMS_U64)SYMS_CvRegx64_FPCS },
{ { (SYMS_U8*)"FPDO", 4 }, (SYMS_U64)SYMS_CvRegx64_FPDO },
{ { (SYMS_U8*)"FPDS", 4 }, (SYMS_U64)SYMS_CvRegx64_FPDS },
{ { (SYMS_U8*)"ISEM", 4 }, (SYMS_U64)SYMS_CvRegx64_ISEM },
{ { (SYMS_U8*)"FPEIP", 5 }, (SYMS_U64)SYMS_CvRegx64_FPEIP },
{ { (SYMS_U8*)"FPEDO", 5 }, (SYMS_U64)SYMS_CvRegx64_FPEDO },
{ { (SYMS_U8*)"MM0", 3 }, (SYMS_U64)SYMS_CvRegx64_MM0 },
{ { (SYMS_U8*)"MM1", 3 }, (SYMS_U64)SYMS_CvRegx64_MM1 },
{ { (SYMS_U8*)"MM2", 3 }, (SYMS_U64)SYMS_CvRegx64_MM2 },
{ { (SYMS_U8*)"MM3", 3 }, (SYMS_U64)SYMS_CvRegx64_MM3 },
{ { (SYMS_U8*)"MM4", 3 }, (SYMS_U64)SYMS_CvRegx64_MM4 },
{ { (SYMS_U8*)"MM5", 3 }, (SYMS_U64)SYMS_CvRegx64_MM5 },
{ { (SYMS_U8*)"MM6", 3 }, (SYMS_U64)SYMS_CvRegx64_MM6 },
{ { (SYMS_U8*)"MM7", 3 }, (SYMS_U64)SYMS_CvRegx64_MM7 },
{ { (SYMS_U8*)"XMM0", 4 }, (SYMS_U64)SYMS_CvRegx64_XMM0 },
{ { (SYMS_U8*)"XMM1", 4 }, (SYMS_U64)SYMS_CvRegx64_XMM1 },
{ { (SYMS_U8*)"XMM2", 4 }, (SYMS_U64)SYMS_CvRegx64_XMM2 },
{ { (SYMS_U8*)"XMM3", 4 }, (SYMS_U64)SYMS_CvRegx64_XMM3 },
{ { (SYMS_U8*)"XMM4", 4 }, (SYMS_U64)SYMS_CvRegx64_XMM4 },
{ { (SYMS_U8*)"XMM5", 4 }, (SYMS_U64)SYMS_CvRegx64_XMM5 },
{ { (SYMS_U8*)"XMM6", 4 }, (SYMS_U64)SYMS_CvRegx64_XMM6 },
{ { (SYMS_U8*)"XMM7", 4 }, (SYMS_U64)SYMS_CvRegx64_XMM7 },
{ { (SYMS_U8*)"XMM0_0", 6 }, (SYMS_U64)SYMS_CvRegx64_XMM0_0 },
{ { (SYMS_U8*)"XMM0_1", 6 }, (SYMS_U64)SYMS_CvRegx64_XMM0_1 },
{ { (SYMS_U8*)"XMM0_2", 6 }, (SYMS_U64)SYMS_CvRegx64_XMM0_2 },
{ { (SYMS_U8*)"XMM0_3", 6 }, (SYMS_U64)SYMS_CvRegx64_XMM0_3 },
{ { (SYMS_U8*)"XMM1_0", 6 }, (SYMS_U64)SYMS_CvRegx64_XMM1_0 },
{ { (SYMS_U8*)"XMM1_1", 6 }, (SYMS_U64)SYMS_CvRegx64_XMM1_1 },
{ { (SYMS_U8*)"XMM1_2", 6 }, (SYMS_U64)SYMS_CvRegx64_XMM1_2 },
{ { (SYMS_U8*)"XMM1_3", 6 }, (SYMS_U64)SYMS_CvRegx64_XMM1_3 },
{ { (SYMS_U8*)"XMM2_0", 6 }, (SYMS_U64)SYMS_CvRegx64_XMM2_0 },
{ { (SYMS_U8*)"XMM2_1", 6 }, (SYMS_U64)SYMS_CvRegx64_XMM2_1 },
{ { (SYMS_U8*)"XMM2_2", 6 }, (SYMS_U64)SYMS_CvRegx64_XMM2_2 },
{ { (SYMS_U8*)"XMM2_3", 6 }, (SYMS_U64)SYMS_CvRegx64_XMM2_3 },
{ { (SYMS_U8*)"XMM3_0", 6 }, (SYMS_U64)SYMS_CvRegx64_XMM3_0 },
{ { (SYMS_U8*)"XMM3_1", 6 }, (SYMS_U64)SYMS_CvRegx64_XMM3_1 },
{ { (SYMS_U8*)"XMM3_2", 6 }, (SYMS_U64)SYMS_CvRegx64_XMM3_2 },
{ { (SYMS_U8*)"XMM3_3", 6 }, (SYMS_U64)SYMS_CvRegx64_XMM3_3 },
{ { (SYMS_U8*)"XMM4_0", 6 }, (SYMS_U64)SYMS_CvRegx64_XMM4_0 },
{ { (SYMS_U8*)"XMM4_1", 6 }, (SYMS_U64)SYMS_CvRegx64_XMM4_1 },
{ { (SYMS_U8*)"XMM4_2", 6 }, (SYMS_U64)SYMS_CvRegx64_XMM4_2 },
{ { (SYMS_U8*)"XMM4_3", 6 }, (SYMS_U64)SYMS_CvRegx64_XMM4_3 },
{ { (SYMS_U8*)"XMM5_0", 6 }, (SYMS_U64)SYMS_CvRegx64_XMM5_0 },
{ { (SYMS_U8*)"XMM5_1", 6 }, (SYMS_U64)SYMS_CvRegx64_XMM5_1 },
{ { (SYMS_U8*)"XMM5_2", 6 }, (SYMS_U64)SYMS_CvRegx64_XMM5_2 },
{ { (SYMS_U8*)"XMM5_3", 6 }, (SYMS_U64)SYMS_CvRegx64_XMM5_3 },
{ { (SYMS_U8*)"XMM6_0", 6 }, (SYMS_U64)SYMS_CvRegx64_XMM6_0 },
{ { (SYMS_U8*)"XMM6_1", 6 }, (SYMS_U64)SYMS_CvRegx64_XMM6_1 },
{ { (SYMS_U8*)"XMM6_2", 6 }, (SYMS_U64)SYMS_CvRegx64_XMM6_2 },
{ { (SYMS_U8*)"XMM6_3", 6 }, (SYMS_U64)SYMS_CvRegx64_XMM6_3 },
{ { (SYMS_U8*)"XMM7_0", 6 }, (SYMS_U64)SYMS_CvRegx64_XMM7_0 },
{ { (SYMS_U8*)"XMM7_1", 6 }, (SYMS_U64)SYMS_CvRegx64_XMM7_1 },
{ { (SYMS_U8*)"XMM7_2", 6 }, (SYMS_U64)SYMS_CvRegx64_XMM7_2 },
{ { (SYMS_U8*)"XMM7_3", 6 }, (SYMS_U64)SYMS_CvRegx64_XMM7_3 },
{ { (SYMS_U8*)"XMM0L", 5 }, (SYMS_U64)SYMS_CvRegx64_XMM0L },
{ { (SYMS_U8*)"XMM1L", 5 }, (SYMS_U64)SYMS_CvRegx64_XMM1L },
{ { (SYMS_U8*)"XMM2L", 5 }, (SYMS_U64)SYMS_CvRegx64_XMM2L },
{ { (SYMS_U8*)"XMM3L", 5 }, (SYMS_U64)SYMS_CvRegx64_XMM3L },
{ { (SYMS_U8*)"XMM4L", 5 }, (SYMS_U64)SYMS_CvRegx64_XMM4L },
{ { (SYMS_U8*)"XMM5L", 5 }, (SYMS_U64)SYMS_CvRegx64_XMM5L },
{ { (SYMS_U8*)"XMM6L", 5 }, (SYMS_U64)SYMS_CvRegx64_XMM6L },
{ { (SYMS_U8*)"XMM7L", 5 }, (SYMS_U64)SYMS_CvRegx64_XMM7L },
{ { (SYMS_U8*)"XMM0H", 5 }, (SYMS_U64)SYMS_CvRegx64_XMM0H },
{ { (SYMS_U8*)"XMM1H", 5 }, (SYMS_U64)SYMS_CvRegx64_XMM1H },
{ { (SYMS_U8*)"XMM2H", 5 }, (SYMS_U64)SYMS_CvRegx64_XMM2H },
{ { (SYMS_U8*)"XMM3H", 5 }, (SYMS_U64)SYMS_CvRegx64_XMM3H },
{ { (SYMS_U8*)"XMM4H", 5 }, (SYMS_U64)SYMS_CvRegx64_XMM4H },
{ { (SYMS_U8*)"XMM5H", 5 }, (SYMS_U64)SYMS_CvRegx64_XMM5H },
{ { (SYMS_U8*)"XMM6H", 5 }, (SYMS_U64)SYMS_CvRegx64_XMM6H },
{ { (SYMS_U8*)"XMM7H", 5 }, (SYMS_U64)SYMS_CvRegx64_XMM7H },
{ { (SYMS_U8*)"MXCSR", 5 }, (SYMS_U64)SYMS_CvRegx64_MXCSR },
{ { (SYMS_U8*)"EMM0L", 5 }, (SYMS_U64)SYMS_CvRegx64_EMM0L },
{ { (SYMS_U8*)"EMM1L", 5 }, (SYMS_U64)SYMS_CvRegx64_EMM1L },
{ { (SYMS_U8*)"EMM2L", 5 }, (SYMS_U64)SYMS_CvRegx64_EMM2L },
{ { (SYMS_U8*)"EMM3L", 5 }, (SYMS_U64)SYMS_CvRegx64_EMM3L },
{ { (SYMS_U8*)"EMM4L", 5 }, (SYMS_U64)SYMS_CvRegx64_EMM4L },
{ { (SYMS_U8*)"EMM5L", 5 }, (SYMS_U64)SYMS_CvRegx64_EMM5L },
{ { (SYMS_U8*)"EMM6L", 5 }, (SYMS_U64)SYMS_CvRegx64_EMM6L },
{ { (SYMS_U8*)"EMM7L", 5 }, (SYMS_U64)SYMS_CvRegx64_EMM7L },
{ { (SYMS_U8*)"EMM0H", 5 }, (SYMS_U64)SYMS_CvRegx64_EMM0H },
{ { (SYMS_U8*)"EMM1H", 5 }, (SYMS_U64)SYMS_CvRegx64_EMM1H },
{ { (SYMS_U8*)"EMM2H", 5 }, (SYMS_U64)SYMS_CvRegx64_EMM2H },
{ { (SYMS_U8*)"EMM3H", 5 }, (SYMS_U64)SYMS_CvRegx64_EMM3H },
{ { (SYMS_U8*)"EMM4H", 5 }, (SYMS_U64)SYMS_CvRegx64_EMM4H },
{ { (SYMS_U8*)"EMM5H", 5 }, (SYMS_U64)SYMS_CvRegx64_EMM5H },
{ { (SYMS_U8*)"EMM6H", 5 }, (SYMS_U64)SYMS_CvRegx64_EMM6H },
{ { (SYMS_U8*)"EMM7H", 5 }, (SYMS_U64)SYMS_CvRegx64_EMM7H },
{ { (SYMS_U8*)"MM00", 4 }, (SYMS_U64)SYMS_CvRegx64_MM00 },
{ { (SYMS_U8*)"MM01", 4 }, (SYMS_U64)SYMS_CvRegx64_MM01 },
{ { (SYMS_U8*)"MM10", 4 }, (SYMS_U64)SYMS_CvRegx64_MM10 },
{ { (SYMS_U8*)"MM11", 4 }, (SYMS_U64)SYMS_CvRegx64_MM11 },
{ { (SYMS_U8*)"MM20", 4 }, (SYMS_U64)SYMS_CvRegx64_MM20 },
{ { (SYMS_U8*)"MM21", 4 }, (SYMS_U64)SYMS_CvRegx64_MM21 },
{ { (SYMS_U8*)"MM30", 4 }, (SYMS_U64)SYMS_CvRegx64_MM30 },
{ { (SYMS_U8*)"MM31", 4 }, (SYMS_U64)SYMS_CvRegx64_MM31 },
{ { (SYMS_U8*)"MM40", 4 }, (SYMS_U64)SYMS_CvRegx64_MM40 },
{ { (SYMS_U8*)"MM41", 4 }, (SYMS_U64)SYMS_CvRegx64_MM41 },
{ { (SYMS_U8*)"MM50", 4 }, (SYMS_U64)SYMS_CvRegx64_MM50 },
{ { (SYMS_U8*)"MM51", 4 }, (SYMS_U64)SYMS_CvRegx64_MM51 },
{ { (SYMS_U8*)"MM60", 4 }, (SYMS_U64)SYMS_CvRegx64_MM60 },
{ { (SYMS_U8*)"MM61", 4 }, (SYMS_U64)SYMS_CvRegx64_MM61 },
{ { (SYMS_U8*)"MM70", 4 }, (SYMS_U64)SYMS_CvRegx64_MM70 },
{ { (SYMS_U8*)"MM71", 4 }, (SYMS_U64)SYMS_CvRegx64_MM71 },
{ { (SYMS_U8*)"XMM8", 4 }, (SYMS_U64)SYMS_CvRegx64_XMM8 },
{ { (SYMS_U8*)"XMM9", 4 }, (SYMS_U64)SYMS_CvRegx64_XMM9 },
{ { (SYMS_U8*)"XMM10", 5 }, (SYMS_U64)SYMS_CvRegx64_XMM10 },
{ { (SYMS_U8*)"XMM11", 5 }, (SYMS_U64)SYMS_CvRegx64_XMM11 },
{ { (SYMS_U8*)"XMM12", 5 }, (SYMS_U64)SYMS_CvRegx64_XMM12 },
{ { (SYMS_U8*)"XMM13", 5 }, (SYMS_U64)SYMS_CvRegx64_XMM13 },
{ { (SYMS_U8*)"XMM14", 5 }, (SYMS_U64)SYMS_CvRegx64_XMM14 },
{ { (SYMS_U8*)"XMM15", 5 }, (SYMS_U64)SYMS_CvRegx64_XMM15 },
{ { (SYMS_U8*)"XMM8_0", 6 }, (SYMS_U64)SYMS_CvRegx64_XMM8_0 },
{ { (SYMS_U8*)"XMM8_1", 6 }, (SYMS_U64)SYMS_CvRegx64_XMM8_1 },
{ { (SYMS_U8*)"XMM8_2", 6 }, (SYMS_U64)SYMS_CvRegx64_XMM8_2 },
{ { (SYMS_U8*)"XMM8_3", 6 }, (SYMS_U64)SYMS_CvRegx64_XMM8_3 },
{ { (SYMS_U8*)"XMM9_0", 6 }, (SYMS_U64)SYMS_CvRegx64_XMM9_0 },
{ { (SYMS_U8*)"XMM9_1", 6 }, (SYMS_U64)SYMS_CvRegx64_XMM9_1 },
{ { (SYMS_U8*)"XMM9_2", 6 }, (SYMS_U64)SYMS_CvRegx64_XMM9_2 },
{ { (SYMS_U8*)"XMM9_3", 6 }, (SYMS_U64)SYMS_CvRegx64_XMM9_3 },
{ { (SYMS_U8*)"XMM10_0", 7 }, (SYMS_U64)SYMS_CvRegx64_XMM10_0 },
{ { (SYMS_U8*)"XMM10_1", 7 }, (SYMS_U64)SYMS_CvRegx64_XMM10_1 },
{ { (SYMS_U8*)"XMM10_2", 7 }, (SYMS_U64)SYMS_CvRegx64_XMM10_2 },
{ { (SYMS_U8*)"XMM10_3", 7 }, (SYMS_U64)SYMS_CvRegx64_XMM10_3 },
{ { (SYMS_U8*)"XMM11_0", 7 }, (SYMS_U64)SYMS_CvRegx64_XMM11_0 },
{ { (SYMS_U8*)"XMM11_1", 7 }, (SYMS_U64)SYMS_CvRegx64_XMM11_1 },
{ { (SYMS_U8*)"XMM11_2", 7 }, (SYMS_U64)SYMS_CvRegx64_XMM11_2 },
{ { (SYMS_U8*)"XMM11_3", 7 }, (SYMS_U64)SYMS_CvRegx64_XMM11_3 },
{ { (SYMS_U8*)"XMM12_0", 7 }, (SYMS_U64)SYMS_CvRegx64_XMM12_0 },
{ { (SYMS_U8*)"XMM12_1", 7 }, (SYMS_U64)SYMS_CvRegx64_XMM12_1 },
{ { (SYMS_U8*)"XMM12_2", 7 }, (SYMS_U64)SYMS_CvRegx64_XMM12_2 },
{ { (SYMS_U8*)"XMM12_3", 7 }, (SYMS_U64)SYMS_CvRegx64_XMM12_3 },
{ { (SYMS_U8*)"XMM13_0", 7 }, (SYMS_U64)SYMS_CvRegx64_XMM13_0 },
{ { (SYMS_U8*)"XMM13_1", 7 }, (SYMS_U64)SYMS_CvRegx64_XMM13_1 },
{ { (SYMS_U8*)"XMM13_2", 7 }, (SYMS_U64)SYMS_CvRegx64_XMM13_2 },
{ { (SYMS_U8*)"XMM13_3", 7 }, (SYMS_U64)SYMS_CvRegx64_XMM13_3 },
{ { (SYMS_U8*)"XMM14_0", 7 }, (SYMS_U64)SYMS_CvRegx64_XMM14_0 },
{ { (SYMS_U8*)"XMM14_1", 7 }, (SYMS_U64)SYMS_CvRegx64_XMM14_1 },
{ { (SYMS_U8*)"XMM14_2", 7 }, (SYMS_U64)SYMS_CvRegx64_XMM14_2 },
{ { (SYMS_U8*)"XMM14_3", 7 }, (SYMS_U64)SYMS_CvRegx64_XMM14_3 },
{ { (SYMS_U8*)"XMM15_0", 7 }, (SYMS_U64)SYMS_CvRegx64_XMM15_0 },
{ { (SYMS_U8*)"XMM15_1", 7 }, (SYMS_U64)SYMS_CvRegx64_XMM15_1 },
{ { (SYMS_U8*)"XMM15_2", 7 }, (SYMS_U64)SYMS_CvRegx64_XMM15_2 },
{ { (SYMS_U8*)"XMM15_3", 7 }, (SYMS_U64)SYMS_CvRegx64_XMM15_3 },
{ { (SYMS_U8*)"XMM8L", 5 }, (SYMS_U64)SYMS_CvRegx64_XMM8L },
{ { (SYMS_U8*)"XMM9L", 5 }, (SYMS_U64)SYMS_CvRegx64_XMM9L },
{ { (SYMS_U8*)"XMM10L", 6 }, (SYMS_U64)SYMS_CvRegx64_XMM10L },
{ { (SYMS_U8*)"XMM11L", 6 }, (SYMS_U64)SYMS_CvRegx64_XMM11L },
{ { (SYMS_U8*)"XMM12L", 6 }, (SYMS_U64)SYMS_CvRegx64_XMM12L },
{ { (SYMS_U8*)"XMM13L", 6 }, (SYMS_U64)SYMS_CvRegx64_XMM13L },
{ { (SYMS_U8*)"XMM14L", 6 }, (SYMS_U64)SYMS_CvRegx64_XMM14L },
{ { (SYMS_U8*)"XMM15L", 6 }, (SYMS_U64)SYMS_CvRegx64_XMM15L },
{ { (SYMS_U8*)"XMM8H", 5 }, (SYMS_U64)SYMS_CvRegx64_XMM8H },
{ { (SYMS_U8*)"XMM9H", 5 }, (SYMS_U64)SYMS_CvRegx64_XMM9H },
{ { (SYMS_U8*)"XMM10H", 6 }, (SYMS_U64)SYMS_CvRegx64_XMM10H },
{ { (SYMS_U8*)"XMM11H", 6 }, (SYMS_U64)SYMS_CvRegx64_XMM11H },
{ { (SYMS_U8*)"XMM12H", 6 }, (SYMS_U64)SYMS_CvRegx64_XMM12H },
{ { (SYMS_U8*)"XMM13H", 6 }, (SYMS_U64)SYMS_CvRegx64_XMM13H },
{ { (SYMS_U8*)"XMM14H", 6 }, (SYMS_U64)SYMS_CvRegx64_XMM14H },
{ { (SYMS_U8*)"XMM15H", 6 }, (SYMS_U64)SYMS_CvRegx64_XMM15H },
{ { (SYMS_U8*)"EMM8L", 5 }, (SYMS_U64)SYMS_CvRegx64_EMM8L },
{ { (SYMS_U8*)"EMM9L", 5 }, (SYMS_U64)SYMS_CvRegx64_EMM9L },
{ { (SYMS_U8*)"EMM10L", 6 }, (SYMS_U64)SYMS_CvRegx64_EMM10L },
{ { (SYMS_U8*)"EMM11L", 6 }, (SYMS_U64)SYMS_CvRegx64_EMM11L },
{ { (SYMS_U8*)"EMM12L", 6 }, (SYMS_U64)SYMS_CvRegx64_EMM12L },
{ { (SYMS_U8*)"EMM13L", 6 }, (SYMS_U64)SYMS_CvRegx64_EMM13L },
{ { (SYMS_U8*)"EMM14L", 6 }, (SYMS_U64)SYMS_CvRegx64_EMM14L },
{ { (SYMS_U8*)"EMM15L", 6 }, (SYMS_U64)SYMS_CvRegx64_EMM15L },
{ { (SYMS_U8*)"EMM8H", 5 }, (SYMS_U64)SYMS_CvRegx64_EMM8H },
{ { (SYMS_U8*)"EMM9H", 5 }, (SYMS_U64)SYMS_CvRegx64_EMM9H },
{ { (SYMS_U8*)"EMM10H", 6 }, (SYMS_U64)SYMS_CvRegx64_EMM10H },
{ { (SYMS_U8*)"EMM11H", 6 }, (SYMS_U64)SYMS_CvRegx64_EMM11H },
{ { (SYMS_U8*)"EMM12H", 6 }, (SYMS_U64)SYMS_CvRegx64_EMM12H },
{ { (SYMS_U8*)"EMM13H", 6 }, (SYMS_U64)SYMS_CvRegx64_EMM13H },
{ { (SYMS_U8*)"EMM14H", 6 }, (SYMS_U64)SYMS_CvRegx64_EMM14H },
{ { (SYMS_U8*)"EMM15H", 6 }, (SYMS_U64)SYMS_CvRegx64_EMM15H },
{ { (SYMS_U8*)"SIL", 3 }, (SYMS_U64)SYMS_CvRegx64_SIL },
{ { (SYMS_U8*)"DIL", 3 }, (SYMS_U64)SYMS_CvRegx64_DIL },
{ { (SYMS_U8*)"BPL", 3 }, (SYMS_U64)SYMS_CvRegx64_BPL },
{ { (SYMS_U8*)"SPL", 3 }, (SYMS_U64)SYMS_CvRegx64_SPL },
{ { (SYMS_U8*)"RAX", 3 }, (SYMS_U64)SYMS_CvRegx64_RAX },
{ { (SYMS_U8*)"RBX", 3 }, (SYMS_U64)SYMS_CvRegx64_RBX },
{ { (SYMS_U8*)"RCX", 3 }, (SYMS_U64)SYMS_CvRegx64_RCX },
{ { (SYMS_U8*)"RDX", 3 }, (SYMS_U64)SYMS_CvRegx64_RDX },
{ { (SYMS_U8*)"RSI", 3 }, (SYMS_U64)SYMS_CvRegx64_RSI },
{ { (SYMS_U8*)"RDI", 3 }, (SYMS_U64)SYMS_CvRegx64_RDI },
{ { (SYMS_U8*)"RBP", 3 }, (SYMS_U64)SYMS_CvRegx64_RBP },
{ { (SYMS_U8*)"RSP", 3 }, (SYMS_U64)SYMS_CvRegx64_RSP },
{ { (SYMS_U8*)"R8", 2 }, (SYMS_U64)SYMS_CvRegx64_R8 },
{ { (SYMS_U8*)"R9", 2 }, (SYMS_U64)SYMS_CvRegx64_R9 },
{ { (SYMS_U8*)"R10", 3 }, (SYMS_U64)SYMS_CvRegx64_R10 },
{ { (SYMS_U8*)"R11", 3 }, (SYMS_U64)SYMS_CvRegx64_R11 },
{ { (SYMS_U8*)"R12", 3 }, (SYMS_U64)SYMS_CvRegx64_R12 },
{ { (SYMS_U8*)"R13", 3 }, (SYMS_U64)SYMS_CvRegx64_R13 },
{ { (SYMS_U8*)"R14", 3 }, (SYMS_U64)SYMS_CvRegx64_R14 },
{ { (SYMS_U8*)"R15", 3 }, (SYMS_U64)SYMS_CvRegx64_R15 },
{ { (SYMS_U8*)"R8B", 3 }, (SYMS_U64)SYMS_CvRegx64_R8B },
{ { (SYMS_U8*)"R9B", 3 }, (SYMS_U64)SYMS_CvRegx64_R9B },
{ { (SYMS_U8*)"R10B", 4 }, (SYMS_U64)SYMS_CvRegx64_R10B },
{ { (SYMS_U8*)"R11B", 4 }, (SYMS_U64)SYMS_CvRegx64_R11B },
{ { (SYMS_U8*)"R12B", 4 }, (SYMS_U64)SYMS_CvRegx64_R12B },
{ { (SYMS_U8*)"R13B", 4 }, (SYMS_U64)SYMS_CvRegx64_R13B },
{ { (SYMS_U8*)"R14B", 4 }, (SYMS_U64)SYMS_CvRegx64_R14B },
{ { (SYMS_U8*)"R15B", 4 }, (SYMS_U64)SYMS_CvRegx64_R15B },
{ { (SYMS_U8*)"R8W", 3 }, (SYMS_U64)SYMS_CvRegx64_R8W },
{ { (SYMS_U8*)"R9W", 3 }, (SYMS_U64)SYMS_CvRegx64_R9W },
{ { (SYMS_U8*)"R10W", 4 }, (SYMS_U64)SYMS_CvRegx64_R10W },
{ { (SYMS_U8*)"R11W", 4 }, (SYMS_U64)SYMS_CvRegx64_R11W },
{ { (SYMS_U8*)"R12W", 4 }, (SYMS_U64)SYMS_CvRegx64_R12W },
{ { (SYMS_U8*)"R13W", 4 }, (SYMS_U64)SYMS_CvRegx64_R13W },
{ { (SYMS_U8*)"R14W", 4 }, (SYMS_U64)SYMS_CvRegx64_R14W },
{ { (SYMS_U8*)"R15W", 4 }, (SYMS_U64)SYMS_CvRegx64_R15W },
{ { (SYMS_U8*)"R8D", 3 }, (SYMS_U64)SYMS_CvRegx64_R8D },
{ { (SYMS_U8*)"R9D", 3 }, (SYMS_U64)SYMS_CvRegx64_R9D },
{ { (SYMS_U8*)"R10D", 4 }, (SYMS_U64)SYMS_CvRegx64_R10D },
{ { (SYMS_U8*)"R11D", 4 }, (SYMS_U64)SYMS_CvRegx64_R11D },
{ { (SYMS_U8*)"R12D", 4 }, (SYMS_U64)SYMS_CvRegx64_R12D },
{ { (SYMS_U8*)"R13D", 4 }, (SYMS_U64)SYMS_CvRegx64_R13D },
{ { (SYMS_U8*)"R14D", 4 }, (SYMS_U64)SYMS_CvRegx64_R14D },
{ { (SYMS_U8*)"R15D", 4 }, (SYMS_U64)SYMS_CvRegx64_R15D },
{ { (SYMS_U8*)"YMM0", 4 }, (SYMS_U64)SYMS_CvRegx64_YMM0 },
{ { (SYMS_U8*)"YMM1", 4 }, (SYMS_U64)SYMS_CvRegx64_YMM1 },
{ { (SYMS_U8*)"YMM2", 4 }, (SYMS_U64)SYMS_CvRegx64_YMM2 },
{ { (SYMS_U8*)"YMM3", 4 }, (SYMS_U64)SYMS_CvRegx64_YMM3 },
{ { (SYMS_U8*)"YMM4", 4 }, (SYMS_U64)SYMS_CvRegx64_YMM4 },
{ { (SYMS_U8*)"YMM5", 4 }, (SYMS_U64)SYMS_CvRegx64_YMM5 },
{ { (SYMS_U8*)"YMM6", 4 }, (SYMS_U64)SYMS_CvRegx64_YMM6 },
{ { (SYMS_U8*)"YMM7", 4 }, (SYMS_U64)SYMS_CvRegx64_YMM7 },
{ { (SYMS_U8*)"YMM8", 4 }, (SYMS_U64)SYMS_CvRegx64_YMM8 },
{ { (SYMS_U8*)"YMM9", 4 }, (SYMS_U64)SYMS_CvRegx64_YMM9 },
{ { (SYMS_U8*)"YMM10", 5 }, (SYMS_U64)SYMS_CvRegx64_YMM10 },
{ { (SYMS_U8*)"YMM11", 5 }, (SYMS_U64)SYMS_CvRegx64_YMM11 },
{ { (SYMS_U8*)"YMM12", 5 }, (SYMS_U64)SYMS_CvRegx64_YMM12 },
{ { (SYMS_U8*)"YMM13", 5 }, (SYMS_U64)SYMS_CvRegx64_YMM13 },
{ { (SYMS_U8*)"YMM14", 5 }, (SYMS_U64)SYMS_CvRegx64_YMM14 },
{ { (SYMS_U8*)"YMM15", 5 }, (SYMS_U64)SYMS_CvRegx64_YMM15 },
{ { (SYMS_U8*)"YMM0H", 5 }, (SYMS_U64)SYMS_CvRegx64_YMM0H },
{ { (SYMS_U8*)"YMM1H", 5 }, (SYMS_U64)SYMS_CvRegx64_YMM1H },
{ { (SYMS_U8*)"YMM2H", 5 }, (SYMS_U64)SYMS_CvRegx64_YMM2H },
{ { (SYMS_U8*)"YMM3H", 5 }, (SYMS_U64)SYMS_CvRegx64_YMM3H },
{ { (SYMS_U8*)"YMM4H", 5 }, (SYMS_U64)SYMS_CvRegx64_YMM4H },
{ { (SYMS_U8*)"YMM5H", 5 }, (SYMS_U64)SYMS_CvRegx64_YMM5H },
{ { (SYMS_U8*)"YMM6H", 5 }, (SYMS_U64)SYMS_CvRegx64_YMM6H },
{ { (SYMS_U8*)"YMM7H", 5 }, (SYMS_U64)SYMS_CvRegx64_YMM7H },
{ { (SYMS_U8*)"YMM8H", 5 }, (SYMS_U64)SYMS_CvRegx64_YMM8H },
{ { (SYMS_U8*)"YMM9H", 5 }, (SYMS_U64)SYMS_CvRegx64_YMM9H },
{ { (SYMS_U8*)"YMM10H", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM10H },
{ { (SYMS_U8*)"YMM11H", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM11H },
{ { (SYMS_U8*)"YMM12H", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM12H },
{ { (SYMS_U8*)"YMM13H", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM13H },
{ { (SYMS_U8*)"YMM14H", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM14H },
{ { (SYMS_U8*)"YMM15H", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM15H },
{ { (SYMS_U8*)"XMM0IL", 6 }, (SYMS_U64)SYMS_CvRegx64_XMM0IL },
{ { (SYMS_U8*)"XMM1IL", 6 }, (SYMS_U64)SYMS_CvRegx64_XMM1IL },
{ { (SYMS_U8*)"XMM2IL", 6 }, (SYMS_U64)SYMS_CvRegx64_XMM2IL },
{ { (SYMS_U8*)"XMM3IL", 6 }, (SYMS_U64)SYMS_CvRegx64_XMM3IL },
{ { (SYMS_U8*)"XMM4IL", 6 }, (SYMS_U64)SYMS_CvRegx64_XMM4IL },
{ { (SYMS_U8*)"XMM5IL", 6 }, (SYMS_U64)SYMS_CvRegx64_XMM5IL },
{ { (SYMS_U8*)"XMM6IL", 6 }, (SYMS_U64)SYMS_CvRegx64_XMM6IL },
{ { (SYMS_U8*)"XMM7IL", 6 }, (SYMS_U64)SYMS_CvRegx64_XMM7IL },
{ { (SYMS_U8*)"XMM8IL", 6 }, (SYMS_U64)SYMS_CvRegx64_XMM8IL },
{ { (SYMS_U8*)"XMM9IL", 6 }, (SYMS_U64)SYMS_CvRegx64_XMM9IL },
{ { (SYMS_U8*)"XMM10IL", 7 }, (SYMS_U64)SYMS_CvRegx64_XMM10IL },
{ { (SYMS_U8*)"XMM11IL", 7 }, (SYMS_U64)SYMS_CvRegx64_XMM11IL },
{ { (SYMS_U8*)"XMM12IL", 7 }, (SYMS_U64)SYMS_CvRegx64_XMM12IL },
{ { (SYMS_U8*)"XMM13IL", 7 }, (SYMS_U64)SYMS_CvRegx64_XMM13IL },
{ { (SYMS_U8*)"XMM14IL", 7 }, (SYMS_U64)SYMS_CvRegx64_XMM14IL },
{ { (SYMS_U8*)"XMM15IL", 7 }, (SYMS_U64)SYMS_CvRegx64_XMM15IL },
{ { (SYMS_U8*)"XMM0IH", 6 }, (SYMS_U64)SYMS_CvRegx64_XMM0IH },
{ { (SYMS_U8*)"XMM1IH", 6 }, (SYMS_U64)SYMS_CvRegx64_XMM1IH },
{ { (SYMS_U8*)"XMM2IH", 6 }, (SYMS_U64)SYMS_CvRegx64_XMM2IH },
{ { (SYMS_U8*)"XMM3IH", 6 }, (SYMS_U64)SYMS_CvRegx64_XMM3IH },
{ { (SYMS_U8*)"XMM4IH", 6 }, (SYMS_U64)SYMS_CvRegx64_XMM4IH },
{ { (SYMS_U8*)"XMM5IH", 6 }, (SYMS_U64)SYMS_CvRegx64_XMM5IH },
{ { (SYMS_U8*)"XMM6IH", 6 }, (SYMS_U64)SYMS_CvRegx64_XMM6IH },
{ { (SYMS_U8*)"XMM7IH", 6 }, (SYMS_U64)SYMS_CvRegx64_XMM7IH },
{ { (SYMS_U8*)"XMM8IH", 6 }, (SYMS_U64)SYMS_CvRegx64_XMM8IH },
{ { (SYMS_U8*)"XMM9IH", 6 }, (SYMS_U64)SYMS_CvRegx64_XMM9IH },
{ { (SYMS_U8*)"XMM10IH", 7 }, (SYMS_U64)SYMS_CvRegx64_XMM10IH },
{ { (SYMS_U8*)"XMM11IH", 7 }, (SYMS_U64)SYMS_CvRegx64_XMM11IH },
{ { (SYMS_U8*)"XMM12IH", 7 }, (SYMS_U64)SYMS_CvRegx64_XMM12IH },
{ { (SYMS_U8*)"XMM13IH", 7 }, (SYMS_U64)SYMS_CvRegx64_XMM13IH },
{ { (SYMS_U8*)"XMM14IH", 7 }, (SYMS_U64)SYMS_CvRegx64_XMM14IH },
{ { (SYMS_U8*)"XMM15IH", 7 }, (SYMS_U64)SYMS_CvRegx64_XMM15IH },
{ { (SYMS_U8*)"YMM0I0", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM0I0 },
{ { (SYMS_U8*)"YMM0I1", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM0I1 },
{ { (SYMS_U8*)"YMM0I2", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM0I2 },
{ { (SYMS_U8*)"YMM0I3", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM0I3 },
{ { (SYMS_U8*)"YMM1I0", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM1I0 },
{ { (SYMS_U8*)"YMM1I1", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM1I1 },
{ { (SYMS_U8*)"YMM1I2", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM1I2 },
{ { (SYMS_U8*)"YMM1I3", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM1I3 },
{ { (SYMS_U8*)"YMM2I0", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM2I0 },
{ { (SYMS_U8*)"YMM2I1", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM2I1 },
{ { (SYMS_U8*)"YMM2I2", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM2I2 },
{ { (SYMS_U8*)"YMM2I3", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM2I3 },
{ { (SYMS_U8*)"YMM3I0", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM3I0 },
{ { (SYMS_U8*)"YMM3I1", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM3I1 },
{ { (SYMS_U8*)"YMM3I2", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM3I2 },
{ { (SYMS_U8*)"YMM3I3", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM3I3 },
{ { (SYMS_U8*)"YMM4I0", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM4I0 },
{ { (SYMS_U8*)"YMM4I1", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM4I1 },
{ { (SYMS_U8*)"YMM4I2", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM4I2 },
{ { (SYMS_U8*)"YMM4I3", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM4I3 },
{ { (SYMS_U8*)"YMM5I0", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM5I0 },
{ { (SYMS_U8*)"YMM5I1", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM5I1 },
{ { (SYMS_U8*)"YMM5I2", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM5I2 },
{ { (SYMS_U8*)"YMM5I3", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM5I3 },
{ { (SYMS_U8*)"YMM6I0", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM6I0 },
{ { (SYMS_U8*)"YMM6I1", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM6I1 },
{ { (SYMS_U8*)"YMM6I2", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM6I2 },
{ { (SYMS_U8*)"YMM6I3", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM6I3 },
{ { (SYMS_U8*)"YMM7I0", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM7I0 },
{ { (SYMS_U8*)"YMM7I1", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM7I1 },
{ { (SYMS_U8*)"YMM7I2", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM7I2 },
{ { (SYMS_U8*)"YMM7I3", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM7I3 },
{ { (SYMS_U8*)"YMM8I0", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM8I0 },
{ { (SYMS_U8*)"YMM8I1", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM8I1 },
{ { (SYMS_U8*)"YMM8I2", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM8I2 },
{ { (SYMS_U8*)"YMM8I3", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM8I3 },
{ { (SYMS_U8*)"YMM9I0", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM9I0 },
{ { (SYMS_U8*)"YMM9I1", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM9I1 },
{ { (SYMS_U8*)"YMM9I2", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM9I2 },
{ { (SYMS_U8*)"YMM9I3", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM9I3 },
{ { (SYMS_U8*)"YMM10I0", 7 }, (SYMS_U64)SYMS_CvRegx64_YMM10I0 },
{ { (SYMS_U8*)"YMM10I1", 7 }, (SYMS_U64)SYMS_CvRegx64_YMM10I1 },
{ { (SYMS_U8*)"YMM10I2", 7 }, (SYMS_U64)SYMS_CvRegx64_YMM10I2 },
{ { (SYMS_U8*)"YMM10I3", 7 }, (SYMS_U64)SYMS_CvRegx64_YMM10I3 },
{ { (SYMS_U8*)"YMM11I0", 7 }, (SYMS_U64)SYMS_CvRegx64_YMM11I0 },
{ { (SYMS_U8*)"YMM11I1", 7 }, (SYMS_U64)SYMS_CvRegx64_YMM11I1 },
{ { (SYMS_U8*)"YMM11I2", 7 }, (SYMS_U64)SYMS_CvRegx64_YMM11I2 },
{ { (SYMS_U8*)"YMM11I3", 7 }, (SYMS_U64)SYMS_CvRegx64_YMM11I3 },
{ { (SYMS_U8*)"YMM12I0", 7 }, (SYMS_U64)SYMS_CvRegx64_YMM12I0 },
{ { (SYMS_U8*)"YMM12I1", 7 }, (SYMS_U64)SYMS_CvRegx64_YMM12I1 },
{ { (SYMS_U8*)"YMM12I2", 7 }, (SYMS_U64)SYMS_CvRegx64_YMM12I2 },
{ { (SYMS_U8*)"YMM12I3", 7 }, (SYMS_U64)SYMS_CvRegx64_YMM12I3 },
{ { (SYMS_U8*)"YMM13I0", 7 }, (SYMS_U64)SYMS_CvRegx64_YMM13I0 },
{ { (SYMS_U8*)"YMM13I1", 7 }, (SYMS_U64)SYMS_CvRegx64_YMM13I1 },
{ { (SYMS_U8*)"YMM13I2", 7 }, (SYMS_U64)SYMS_CvRegx64_YMM13I2 },
{ { (SYMS_U8*)"YMM13I3", 7 }, (SYMS_U64)SYMS_CvRegx64_YMM13I3 },
{ { (SYMS_U8*)"YMM14I0", 7 }, (SYMS_U64)SYMS_CvRegx64_YMM14I0 },
{ { (SYMS_U8*)"YMM14I1", 7 }, (SYMS_U64)SYMS_CvRegx64_YMM14I1 },
{ { (SYMS_U8*)"YMM14I2", 7 }, (SYMS_U64)SYMS_CvRegx64_YMM14I2 },
{ { (SYMS_U8*)"YMM14I3", 7 }, (SYMS_U64)SYMS_CvRegx64_YMM14I3 },
{ { (SYMS_U8*)"YMM15I0", 7 }, (SYMS_U64)SYMS_CvRegx64_YMM15I0 },
{ { (SYMS_U8*)"YMM15I1", 7 }, (SYMS_U64)SYMS_CvRegx64_YMM15I1 },
{ { (SYMS_U8*)"YMM15I2", 7 }, (SYMS_U64)SYMS_CvRegx64_YMM15I2 },
{ { (SYMS_U8*)"YMM15I3", 7 }, (SYMS_U64)SYMS_CvRegx64_YMM15I3 },
{ { (SYMS_U8*)"YMM0F0", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM0F0 },
{ { (SYMS_U8*)"YMM0F1", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM0F1 },
{ { (SYMS_U8*)"YMM0F2", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM0F2 },
{ { (SYMS_U8*)"YMM0F3", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM0F3 },
{ { (SYMS_U8*)"YMM0F4", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM0F4 },
{ { (SYMS_U8*)"YMM0F5", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM0F5 },
{ { (SYMS_U8*)"YMM0F6", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM0F6 },
{ { (SYMS_U8*)"YMM0F7", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM0F7 },
{ { (SYMS_U8*)"YMM1F0", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM1F0 },
{ { (SYMS_U8*)"YMM1F1", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM1F1 },
{ { (SYMS_U8*)"YMM1F2", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM1F2 },
{ { (SYMS_U8*)"YMM1F3", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM1F3 },
{ { (SYMS_U8*)"YMM1F4", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM1F4 },
{ { (SYMS_U8*)"YMM1F5", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM1F5 },
{ { (SYMS_U8*)"YMM1F6", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM1F6 },
{ { (SYMS_U8*)"YMM1F7", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM1F7 },
{ { (SYMS_U8*)"YMM2F0", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM2F0 },
{ { (SYMS_U8*)"YMM2F1", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM2F1 },
{ { (SYMS_U8*)"YMM2F2", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM2F2 },
{ { (SYMS_U8*)"YMM2F3", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM2F3 },
{ { (SYMS_U8*)"YMM2F4", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM2F4 },
{ { (SYMS_U8*)"YMM2F5", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM2F5 },
{ { (SYMS_U8*)"YMM2F6", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM2F6 },
{ { (SYMS_U8*)"YMM2F7", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM2F7 },
{ { (SYMS_U8*)"YMM3F0", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM3F0 },
{ { (SYMS_U8*)"YMM3F1", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM3F1 },
{ { (SYMS_U8*)"YMM3F2", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM3F2 },
{ { (SYMS_U8*)"YMM3F3", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM3F3 },
{ { (SYMS_U8*)"YMM3F4", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM3F4 },
{ { (SYMS_U8*)"YMM3F5", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM3F5 },
{ { (SYMS_U8*)"YMM3F6", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM3F6 },
{ { (SYMS_U8*)"YMM3F7", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM3F7 },
{ { (SYMS_U8*)"YMM4F0", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM4F0 },
{ { (SYMS_U8*)"YMM4F1", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM4F1 },
{ { (SYMS_U8*)"YMM4F2", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM4F2 },
{ { (SYMS_U8*)"YMM4F3", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM4F3 },
{ { (SYMS_U8*)"YMM4F4", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM4F4 },
{ { (SYMS_U8*)"YMM4F5", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM4F5 },
{ { (SYMS_U8*)"YMM4F6", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM4F6 },
{ { (SYMS_U8*)"YMM4F7", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM4F7 },
{ { (SYMS_U8*)"YMM5F0", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM5F0 },
{ { (SYMS_U8*)"YMM5F1", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM5F1 },
{ { (SYMS_U8*)"YMM5F2", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM5F2 },
{ { (SYMS_U8*)"YMM5F3", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM5F3 },
{ { (SYMS_U8*)"YMM5F4", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM5F4 },
{ { (SYMS_U8*)"YMM5F5", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM5F5 },
{ { (SYMS_U8*)"YMM5F6", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM5F6 },
{ { (SYMS_U8*)"YMM5F7", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM5F7 },
{ { (SYMS_U8*)"YMM6F0", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM6F0 },
{ { (SYMS_U8*)"YMM6F1", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM6F1 },
{ { (SYMS_U8*)"YMM6F2", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM6F2 },
{ { (SYMS_U8*)"YMM6F3", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM6F3 },
{ { (SYMS_U8*)"YMM6F4", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM6F4 },
{ { (SYMS_U8*)"YMM6F5", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM6F5 },
{ { (SYMS_U8*)"YMM6F6", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM6F6 },
{ { (SYMS_U8*)"YMM6F7", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM6F7 },
{ { (SYMS_U8*)"YMM7F0", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM7F0 },
{ { (SYMS_U8*)"YMM7F1", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM7F1 },
{ { (SYMS_U8*)"YMM7F2", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM7F2 },
{ { (SYMS_U8*)"YMM7F3", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM7F3 },
{ { (SYMS_U8*)"YMM7F4", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM7F4 },
{ { (SYMS_U8*)"YMM7F5", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM7F5 },
{ { (SYMS_U8*)"YMM7F6", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM7F6 },
{ { (SYMS_U8*)"YMM7F7", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM7F7 },
{ { (SYMS_U8*)"YMM8F0", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM8F0 },
{ { (SYMS_U8*)"YMM8F1", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM8F1 },
{ { (SYMS_U8*)"YMM8F2", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM8F2 },
{ { (SYMS_U8*)"YMM8F3", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM8F3 },
{ { (SYMS_U8*)"YMM8F4", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM8F4 },
{ { (SYMS_U8*)"YMM8F5", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM8F5 },
{ { (SYMS_U8*)"YMM8F6", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM8F6 },
{ { (SYMS_U8*)"YMM8F7", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM8F7 },
{ { (SYMS_U8*)"YMM9F0", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM9F0 },
{ { (SYMS_U8*)"YMM9F1", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM9F1 },
{ { (SYMS_U8*)"YMM9F2", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM9F2 },
{ { (SYMS_U8*)"YMM9F3", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM9F3 },
{ { (SYMS_U8*)"YMM9F4", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM9F4 },
{ { (SYMS_U8*)"YMM9F5", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM9F5 },
{ { (SYMS_U8*)"YMM9F6", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM9F6 },
{ { (SYMS_U8*)"YMM9F7", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM9F7 },
{ { (SYMS_U8*)"YMM10F0", 7 }, (SYMS_U64)SYMS_CvRegx64_YMM10F0 },
{ { (SYMS_U8*)"YMM10F1", 7 }, (SYMS_U64)SYMS_CvRegx64_YMM10F1 },
{ { (SYMS_U8*)"YMM10F2", 7 }, (SYMS_U64)SYMS_CvRegx64_YMM10F2 },
{ { (SYMS_U8*)"YMM10F3", 7 }, (SYMS_U64)SYMS_CvRegx64_YMM10F3 },
{ { (SYMS_U8*)"YMM10F4", 7 }, (SYMS_U64)SYMS_CvRegx64_YMM10F4 },
{ { (SYMS_U8*)"YMM10F5", 7 }, (SYMS_U64)SYMS_CvRegx64_YMM10F5 },
{ { (SYMS_U8*)"YMM10F6", 7 }, (SYMS_U64)SYMS_CvRegx64_YMM10F6 },
{ { (SYMS_U8*)"YMM10F7", 7 }, (SYMS_U64)SYMS_CvRegx64_YMM10F7 },
{ { (SYMS_U8*)"YMM11F0", 7 }, (SYMS_U64)SYMS_CvRegx64_YMM11F0 },
{ { (SYMS_U8*)"YMM11F1", 7 }, (SYMS_U64)SYMS_CvRegx64_YMM11F1 },
{ { (SYMS_U8*)"YMM11F2", 7 }, (SYMS_U64)SYMS_CvRegx64_YMM11F2 },
{ { (SYMS_U8*)"YMM11F3", 7 }, (SYMS_U64)SYMS_CvRegx64_YMM11F3 },
{ { (SYMS_U8*)"YMM11F4", 7 }, (SYMS_U64)SYMS_CvRegx64_YMM11F4 },
{ { (SYMS_U8*)"YMM11F5", 7 }, (SYMS_U64)SYMS_CvRegx64_YMM11F5 },
{ { (SYMS_U8*)"YMM11F6", 7 }, (SYMS_U64)SYMS_CvRegx64_YMM11F6 },
{ { (SYMS_U8*)"YMM11F7", 7 }, (SYMS_U64)SYMS_CvRegx64_YMM11F7 },
{ { (SYMS_U8*)"YMM12F0", 7 }, (SYMS_U64)SYMS_CvRegx64_YMM12F0 },
{ { (SYMS_U8*)"YMM12F1", 7 }, (SYMS_U64)SYMS_CvRegx64_YMM12F1 },
{ { (SYMS_U8*)"YMM12F2", 7 }, (SYMS_U64)SYMS_CvRegx64_YMM12F2 },
{ { (SYMS_U8*)"YMM12F3", 7 }, (SYMS_U64)SYMS_CvRegx64_YMM12F3 },
{ { (SYMS_U8*)"YMM12F4", 7 }, (SYMS_U64)SYMS_CvRegx64_YMM12F4 },
{ { (SYMS_U8*)"YMM12F5", 7 }, (SYMS_U64)SYMS_CvRegx64_YMM12F5 },
{ { (SYMS_U8*)"YMM12F6", 7 }, (SYMS_U64)SYMS_CvRegx64_YMM12F6 },
{ { (SYMS_U8*)"YMM12F7", 7 }, (SYMS_U64)SYMS_CvRegx64_YMM12F7 },
{ { (SYMS_U8*)"YMM13F0", 7 }, (SYMS_U64)SYMS_CvRegx64_YMM13F0 },
{ { (SYMS_U8*)"YMM13F1", 7 }, (SYMS_U64)SYMS_CvRegx64_YMM13F1 },
{ { (SYMS_U8*)"YMM13F2", 7 }, (SYMS_U64)SYMS_CvRegx64_YMM13F2 },
{ { (SYMS_U8*)"YMM13F3", 7 }, (SYMS_U64)SYMS_CvRegx64_YMM13F3 },
{ { (SYMS_U8*)"YMM13F4", 7 }, (SYMS_U64)SYMS_CvRegx64_YMM13F4 },
{ { (SYMS_U8*)"YMM13F5", 7 }, (SYMS_U64)SYMS_CvRegx64_YMM13F5 },
{ { (SYMS_U8*)"YMM13F6", 7 }, (SYMS_U64)SYMS_CvRegx64_YMM13F6 },
{ { (SYMS_U8*)"YMM13F7", 7 }, (SYMS_U64)SYMS_CvRegx64_YMM13F7 },
{ { (SYMS_U8*)"YMM14F0", 7 }, (SYMS_U64)SYMS_CvRegx64_YMM14F0 },
{ { (SYMS_U8*)"YMM14F1", 7 }, (SYMS_U64)SYMS_CvRegx64_YMM14F1 },
{ { (SYMS_U8*)"YMM14F2", 7 }, (SYMS_U64)SYMS_CvRegx64_YMM14F2 },
{ { (SYMS_U8*)"YMM14F3", 7 }, (SYMS_U64)SYMS_CvRegx64_YMM14F3 },
{ { (SYMS_U8*)"YMM14F4", 7 }, (SYMS_U64)SYMS_CvRegx64_YMM14F4 },
{ { (SYMS_U8*)"YMM14F5", 7 }, (SYMS_U64)SYMS_CvRegx64_YMM14F5 },
{ { (SYMS_U8*)"YMM14F6", 7 }, (SYMS_U64)SYMS_CvRegx64_YMM14F6 },
{ { (SYMS_U8*)"YMM14F7", 7 }, (SYMS_U64)SYMS_CvRegx64_YMM14F7 },
{ { (SYMS_U8*)"YMM15F0", 7 }, (SYMS_U64)SYMS_CvRegx64_YMM15F0 },
{ { (SYMS_U8*)"YMM15F1", 7 }, (SYMS_U64)SYMS_CvRegx64_YMM15F1 },
{ { (SYMS_U8*)"YMM15F2", 7 }, (SYMS_U64)SYMS_CvRegx64_YMM15F2 },
{ { (SYMS_U8*)"YMM15F3", 7 }, (SYMS_U64)SYMS_CvRegx64_YMM15F3 },
{ { (SYMS_U8*)"YMM15F4", 7 }, (SYMS_U64)SYMS_CvRegx64_YMM15F4 },
{ { (SYMS_U8*)"YMM15F5", 7 }, (SYMS_U64)SYMS_CvRegx64_YMM15F5 },
{ { (SYMS_U8*)"YMM15F6", 7 }, (SYMS_U64)SYMS_CvRegx64_YMM15F6 },
{ { (SYMS_U8*)"YMM15F7", 7 }, (SYMS_U64)SYMS_CvRegx64_YMM15F7 },
{ { (SYMS_U8*)"YMM0D0", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM0D0 },
{ { (SYMS_U8*)"YMM0D1", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM0D1 },
{ { (SYMS_U8*)"YMM0D2", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM0D2 },
{ { (SYMS_U8*)"YMM0D3", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM0D3 },
{ { (SYMS_U8*)"YMM1D0", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM1D0 },
{ { (SYMS_U8*)"YMM1D1", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM1D1 },
{ { (SYMS_U8*)"YMM1D2", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM1D2 },
{ { (SYMS_U8*)"YMM1D3", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM1D3 },
{ { (SYMS_U8*)"YMM2D0", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM2D0 },
{ { (SYMS_U8*)"YMM2D1", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM2D1 },
{ { (SYMS_U8*)"YMM2D2", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM2D2 },
{ { (SYMS_U8*)"YMM2D3", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM2D3 },
{ { (SYMS_U8*)"YMM3D0", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM3D0 },
{ { (SYMS_U8*)"YMM3D1", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM3D1 },
{ { (SYMS_U8*)"YMM3D2", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM3D2 },
{ { (SYMS_U8*)"YMM3D3", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM3D3 },
{ { (SYMS_U8*)"YMM4D0", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM4D0 },
{ { (SYMS_U8*)"YMM4D1", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM4D1 },
{ { (SYMS_U8*)"YMM4D2", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM4D2 },
{ { (SYMS_U8*)"YMM4D3", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM4D3 },
{ { (SYMS_U8*)"YMM5D0", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM5D0 },
{ { (SYMS_U8*)"YMM5D1", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM5D1 },
{ { (SYMS_U8*)"YMM5D2", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM5D2 },
{ { (SYMS_U8*)"YMM5D3", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM5D3 },
{ { (SYMS_U8*)"YMM6D0", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM6D0 },
{ { (SYMS_U8*)"YMM6D1", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM6D1 },
{ { (SYMS_U8*)"YMM6D2", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM6D2 },
{ { (SYMS_U8*)"YMM6D3", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM6D3 },
{ { (SYMS_U8*)"YMM7D0", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM7D0 },
{ { (SYMS_U8*)"YMM7D1", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM7D1 },
{ { (SYMS_U8*)"YMM7D2", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM7D2 },
{ { (SYMS_U8*)"YMM7D3", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM7D3 },
{ { (SYMS_U8*)"YMM8D0", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM8D0 },
{ { (SYMS_U8*)"YMM8D1", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM8D1 },
{ { (SYMS_U8*)"YMM8D2", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM8D2 },
{ { (SYMS_U8*)"YMM8D3", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM8D3 },
{ { (SYMS_U8*)"YMM9D0", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM9D0 },
{ { (SYMS_U8*)"YMM9D1", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM9D1 },
{ { (SYMS_U8*)"YMM9D2", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM9D2 },
{ { (SYMS_U8*)"YMM9D3", 6 }, (SYMS_U64)SYMS_CvRegx64_YMM9D3 },
{ { (SYMS_U8*)"YMM10D0", 7 }, (SYMS_U64)SYMS_CvRegx64_YMM10D0 },
{ { (SYMS_U8*)"YMM10D1", 7 }, (SYMS_U64)SYMS_CvRegx64_YMM10D1 },
{ { (SYMS_U8*)"YMM10D2", 7 }, (SYMS_U64)SYMS_CvRegx64_YMM10D2 },
{ { (SYMS_U8*)"YMM10D3", 7 }, (SYMS_U64)SYMS_CvRegx64_YMM10D3 },
{ { (SYMS_U8*)"YMM11D0", 7 }, (SYMS_U64)SYMS_CvRegx64_YMM11D0 },
{ { (SYMS_U8*)"YMM11D1", 7 }, (SYMS_U64)SYMS_CvRegx64_YMM11D1 },
{ { (SYMS_U8*)"YMM11D2", 7 }, (SYMS_U64)SYMS_CvRegx64_YMM11D2 },
{ { (SYMS_U8*)"YMM11D3", 7 }, (SYMS_U64)SYMS_CvRegx64_YMM11D3 },
{ { (SYMS_U8*)"YMM12D0", 7 }, (SYMS_U64)SYMS_CvRegx64_YMM12D0 },
{ { (SYMS_U8*)"YMM12D1", 7 }, (SYMS_U64)SYMS_CvRegx64_YMM12D1 },
{ { (SYMS_U8*)"YMM12D2", 7 }, (SYMS_U64)SYMS_CvRegx64_YMM12D2 },
{ { (SYMS_U8*)"YMM12D3", 7 }, (SYMS_U64)SYMS_CvRegx64_YMM12D3 },
{ { (SYMS_U8*)"YMM13D0", 7 }, (SYMS_U64)SYMS_CvRegx64_YMM13D0 },
{ { (SYMS_U8*)"YMM13D1", 7 }, (SYMS_U64)SYMS_CvRegx64_YMM13D1 },
{ { (SYMS_U8*)"YMM13D2", 7 }, (SYMS_U64)SYMS_CvRegx64_YMM13D2 },
{ { (SYMS_U8*)"YMM13D3", 7 }, (SYMS_U64)SYMS_CvRegx64_YMM13D3 },
{ { (SYMS_U8*)"YMM14D0", 7 }, (SYMS_U64)SYMS_CvRegx64_YMM14D0 },
{ { (SYMS_U8*)"YMM14D1", 7 }, (SYMS_U64)SYMS_CvRegx64_YMM14D1 },
{ { (SYMS_U8*)"YMM14D2", 7 }, (SYMS_U64)SYMS_CvRegx64_YMM14D2 },
{ { (SYMS_U8*)"YMM14D3", 7 }, (SYMS_U64)SYMS_CvRegx64_YMM14D3 },
{ { (SYMS_U8*)"YMM15D0", 7 }, (SYMS_U64)SYMS_CvRegx64_YMM15D0 },
{ { (SYMS_U8*)"YMM15D1", 7 }, (SYMS_U64)SYMS_CvRegx64_YMM15D1 },
{ { (SYMS_U8*)"YMM15D2", 7 }, (SYMS_U64)SYMS_CvRegx64_YMM15D2 },
{ { (SYMS_U8*)"YMM15D3", 7 }, (SYMS_U64)SYMS_CvRegx64_YMM15D3 },
};
SYMS_SerialValue _syms_serial_members_for_SYMS_CvSignature[] = {
{ { (SYMS_U8*)"C6", 2 }, (SYMS_U64)SYMS_CvSignature_C6 },
{ { (SYMS_U8*)"C7", 2 }, (SYMS_U64)SYMS_CvSignature_C7 },
{ { (SYMS_U8*)"C11", 3 }, (SYMS_U64)SYMS_CvSignature_C11 },
{ { (SYMS_U8*)"C13", 3 }, (SYMS_U64)SYMS_CvSignature_C13 },
{ { (SYMS_U8*)"RESERVED", 8 }, (SYMS_U64)SYMS_CvSignature_RESERVED },
};
SYMS_SerialValue _syms_serial_members_for_SYMS_CvSymKind[] = {
{ { (SYMS_U8*)"NULL", 4 }, (SYMS_U64)SYMS_CvSymKind_NULL },
{ { (SYMS_U8*)"COMPILE", 7 }, (SYMS_U64)SYMS_CvSymKind_COMPILE },
{ { (SYMS_U8*)"REGISTER_16T", 12 }, (SYMS_U64)SYMS_CvSymKind_REGISTER_16t },
{ { (SYMS_U8*)"CONSTANT_16T", 12 }, (SYMS_U64)SYMS_CvSymKind_CONSTANT_16t },
{ { (SYMS_U8*)"UDT_16T", 7 }, (SYMS_U64)SYMS_CvSymKind_UDT_16t },
{ { (SYMS_U8*)"SSEARCH", 7 }, (SYMS_U64)SYMS_CvSymKind_SSEARCH },
{ { (SYMS_U8*)"END", 3 }, (SYMS_U64)SYMS_CvSymKind_END },
{ { (SYMS_U8*)"SKIP", 4 }, (SYMS_U64)SYMS_CvSymKind_SKIP },
{ { (SYMS_U8*)"CVRESERVE", 9 }, (SYMS_U64)SYMS_CvSymKind_CVRESERVE },
{ { (SYMS_U8*)"OBJNAME_ST", 10 }, (SYMS_U64)SYMS_CvSymKind_OBJNAME_ST },
{ { (SYMS_U8*)"ENDARG", 6 }, (SYMS_U64)SYMS_CvSymKind_ENDARG },
{ { (SYMS_U8*)"COBOLUDT_16T", 12 }, (SYMS_U64)SYMS_CvSymKind_COBOLUDT_16t },
{ { (SYMS_U8*)"MANYREG_16T", 11 }, (SYMS_U64)SYMS_CvSymKind_MANYREG_16t },
{ { (SYMS_U8*)"RETURN", 6 }, (SYMS_U64)SYMS_CvSymKind_RETURN },
{ { (SYMS_U8*)"ENTRYTHIS", 9 }, (SYMS_U64)SYMS_CvSymKind_ENTRYTHIS },
{ { (SYMS_U8*)"BPREL16", 7 }, (SYMS_U64)SYMS_CvSymKind_BPREL16 },
{ { (SYMS_U8*)"LDATA16", 7 }, (SYMS_U64)SYMS_CvSymKind_LDATA16 },
{ { (SYMS_U8*)"GDATA16", 7 }, (SYMS_U64)SYMS_CvSymKind_GDATA16 },
{ { (SYMS_U8*)"PUB16", 5 }, (SYMS_U64)SYMS_CvSymKind_PUB16 },
{ { (SYMS_U8*)"LPROC16", 7 }, (SYMS_U64)SYMS_CvSymKind_LPROC16 },
{ { (SYMS_U8*)"GPROC16", 7 }, (SYMS_U64)SYMS_CvSymKind_GPROC16 },
{ { (SYMS_U8*)"THUNK16", 7 }, (SYMS_U64)SYMS_CvSymKind_THUNK16 },
{ { (SYMS_U8*)"BLOCK16", 7 }, (SYMS_U64)SYMS_CvSymKind_BLOCK16 },
{ { (SYMS_U8*)"WITH16", 6 }, (SYMS_U64)SYMS_CvSymKind_WITH16 },
{ { (SYMS_U8*)"LABEL16", 7 }, (SYMS_U64)SYMS_CvSymKind_LABEL16 },
{ { (SYMS_U8*)"CEXMODEL16", 10 }, (SYMS_U64)SYMS_CvSymKind_CEXMODEL16 },
{ { (SYMS_U8*)"VFTABLE16", 9 }, (SYMS_U64)SYMS_CvSymKind_VFTABLE16 },
{ { (SYMS_U8*)"REGREL16", 8 }, (SYMS_U64)SYMS_CvSymKind_REGREL16 },
{ { (SYMS_U8*)"BPREL32_16T", 11 }, (SYMS_U64)SYMS_CvSymKind_BPREL32_16t },
{ { (SYMS_U8*)"LDATA32_16T", 11 }, (SYMS_U64)SYMS_CvSymKind_LDATA32_16t },
{ { (SYMS_U8*)"GDATA32_16T", 11 }, (SYMS_U64)SYMS_CvSymKind_GDATA32_16t },
{ { (SYMS_U8*)"PUB32_16T", 9 }, (SYMS_U64)SYMS_CvSymKind_PUB32_16t },
{ { (SYMS_U8*)"LPROC32_16T", 11 }, (SYMS_U64)SYMS_CvSymKind_LPROC32_16t },
{ { (SYMS_U8*)"GPROC32_16T", 11 }, (SYMS_U64)SYMS_CvSymKind_GPROC32_16t },
{ { (SYMS_U8*)"THUNK32_ST", 10 }, (SYMS_U64)SYMS_CvSymKind_THUNK32_ST },
{ { (SYMS_U8*)"BLOCK32_ST", 10 }, (SYMS_U64)SYMS_CvSymKind_BLOCK32_ST },
{ { (SYMS_U8*)"WITH32_ST", 9 }, (SYMS_U64)SYMS_CvSymKind_WITH32_ST },
{ { (SYMS_U8*)"LABEL32_ST", 10 }, (SYMS_U64)SYMS_CvSymKind_LABEL32_ST },
{ { (SYMS_U8*)"CEXMODEL32", 10 }, (SYMS_U64)SYMS_CvSymKind_CEXMODEL32 },
{ { (SYMS_U8*)"VFTABLE32_16T", 13 }, (SYMS_U64)SYMS_CvSymKind_VFTABLE32_16t },
{ { (SYMS_U8*)"REGREL32_16T", 12 }, (SYMS_U64)SYMS_CvSymKind_REGREL32_16t },
{ { (SYMS_U8*)"LTHREAD32_16T", 13 }, (SYMS_U64)SYMS_CvSymKind_LTHREAD32_16t },
{ { (SYMS_U8*)"GTHREAD32_16T", 13 }, (SYMS_U64)SYMS_CvSymKind_GTHREAD32_16t },
{ { (SYMS_U8*)"SLINK32", 7 }, (SYMS_U64)SYMS_CvSymKind_SLINK32 },
{ { (SYMS_U8*)"LPROCMIPS_16T", 13 }, (SYMS_U64)SYMS_CvSymKind_LPROCMIPS_16t },
{ { (SYMS_U8*)"GPROCMIPS_16T", 13 }, (SYMS_U64)SYMS_CvSymKind_GPROCMIPS_16t },
{ { (SYMS_U8*)"PROCREF_ST", 10 }, (SYMS_U64)SYMS_CvSymKind_PROCREF_ST },
{ { (SYMS_U8*)"DATAREF_ST", 10 }, (SYMS_U64)SYMS_CvSymKind_DATAREF_ST },
{ { (SYMS_U8*)"ALIGN", 5 }, (SYMS_U64)SYMS_CvSymKind_ALIGN },
{ { (SYMS_U8*)"LPROCREF_ST", 11 }, (SYMS_U64)SYMS_CvSymKind_LPROCREF_ST },
{ { (SYMS_U8*)"OEM", 3 }, (SYMS_U64)SYMS_CvSymKind_OEM },
{ { (SYMS_U8*)"TI16_MAX", 8 }, (SYMS_U64)SYMS_CvSymKind_TI16_MAX },
{ { (SYMS_U8*)"CONSTANT_ST", 11 }, (SYMS_U64)SYMS_CvSymKind_CONSTANT_ST },
{ { (SYMS_U8*)"UDT_ST", 6 }, (SYMS_U64)SYMS_CvSymKind_UDT_ST },
{ { (SYMS_U8*)"COBOLUDT_ST", 11 }, (SYMS_U64)SYMS_CvSymKind_COBOLUDT_ST },
{ { (SYMS_U8*)"MANYREG_ST", 10 }, (SYMS_U64)SYMS_CvSymKind_MANYREG_ST },
{ { (SYMS_U8*)"BPREL32_ST", 10 }, (SYMS_U64)SYMS_CvSymKind_BPREL32_ST },
{ { (SYMS_U8*)"LDATA32_ST", 10 }, (SYMS_U64)SYMS_CvSymKind_LDATA32_ST },
{ { (SYMS_U8*)"GDATA32_ST", 10 }, (SYMS_U64)SYMS_CvSymKind_GDATA32_ST },
{ { (SYMS_U8*)"PUB32_ST", 8 }, (SYMS_U64)SYMS_CvSymKind_PUB32_ST },
{ { (SYMS_U8*)"LPROC32_ST", 10 }, (SYMS_U64)SYMS_CvSymKind_LPROC32_ST },
{ { (SYMS_U8*)"GPROC32_ST", 10 }, (SYMS_U64)SYMS_CvSymKind_GPROC32_ST },
{ { (SYMS_U8*)"VFTABLE32", 9 }, (SYMS_U64)SYMS_CvSymKind_VFTABLE32 },
{ { (SYMS_U8*)"REGREL32_ST", 11 }, (SYMS_U64)SYMS_CvSymKind_REGREL32_ST },
{ { (SYMS_U8*)"LTHREAD32_ST", 12 }, (SYMS_U64)SYMS_CvSymKind_LTHREAD32_ST },
{ { (SYMS_U8*)"GTHREAD32_ST", 12 }, (SYMS_U64)SYMS_CvSymKind_GTHREAD32_ST },
{ { (SYMS_U8*)"LPROCMIPS_ST", 12 }, (SYMS_U64)SYMS_CvSymKind_LPROCMIPS_ST },
{ { (SYMS_U8*)"GPROCMIPS_ST", 12 }, (SYMS_U64)SYMS_CvSymKind_GPROCMIPS_ST },
{ { (SYMS_U8*)"FRAMEPROC", 9 }, (SYMS_U64)SYMS_CvSymKind_FRAMEPROC },
{ { (SYMS_U8*)"COMPILE2_ST", 11 }, (SYMS_U64)SYMS_CvSymKind_COMPILE2_ST },
{ { (SYMS_U8*)"MANYREG2_ST", 11 }, (SYMS_U64)SYMS_CvSymKind_MANYREG2_ST },
{ { (SYMS_U8*)"LPROCIA64_ST", 12 }, (SYMS_U64)SYMS_CvSymKind_LPROCIA64_ST },
{ { (SYMS_U8*)"GPROCIA64_ST", 12 }, (SYMS_U64)SYMS_CvSymKind_GPROCIA64_ST },
{ { (SYMS_U8*)"LOCALSLOT_ST", 12 }, (SYMS_U64)SYMS_CvSymKind_LOCALSLOT_ST },
{ { (SYMS_U8*)"PARAMSLOT_ST", 12 }, (SYMS_U64)SYMS_CvSymKind_PARAMSLOT_ST },
{ { (SYMS_U8*)"ANNOTATION", 10 }, (SYMS_U64)SYMS_CvSymKind_ANNOTATION },
{ { (SYMS_U8*)"GMANPROC_ST", 11 }, (SYMS_U64)SYMS_CvSymKind_GMANPROC_ST },
{ { (SYMS_U8*)"LMANPROC_ST", 11 }, (SYMS_U64)SYMS_CvSymKind_LMANPROC_ST },
{ { (SYMS_U8*)"RESERVED1", 9 }, (SYMS_U64)SYMS_CvSymKind_RESERVED1 },
{ { (SYMS_U8*)"RESERVED2", 9 }, (SYMS_U64)SYMS_CvSymKind_RESERVED2 },
{ { (SYMS_U8*)"RESERVED3", 9 }, (SYMS_U64)SYMS_CvSymKind_RESERVED3 },
{ { (SYMS_U8*)"RESERVED4", 9 }, (SYMS_U64)SYMS_CvSymKind_RESERVED4 },
{ { (SYMS_U8*)"LMANDATA_ST", 11 }, (SYMS_U64)SYMS_CvSymKind_LMANDATA_ST },
{ { (SYMS_U8*)"GMANDATA_ST", 11 }, (SYMS_U64)SYMS_CvSymKind_GMANDATA_ST },
{ { (SYMS_U8*)"MANFRAMEREL_ST", 14 }, (SYMS_U64)SYMS_CvSymKind_MANFRAMEREL_ST },
{ { (SYMS_U8*)"MANREGISTER_ST", 14 }, (SYMS_U64)SYMS_CvSymKind_MANREGISTER_ST },
{ { (SYMS_U8*)"MANSLOT_ST", 10 }, (SYMS_U64)SYMS_CvSymKind_MANSLOT_ST },
{ { (SYMS_U8*)"MANMANYREG_ST", 13 }, (SYMS_U64)SYMS_CvSymKind_MANMANYREG_ST },
{ { (SYMS_U8*)"MANREGREL_ST", 12 }, (SYMS_U64)SYMS_CvSymKind_MANREGREL_ST },
{ { (SYMS_U8*)"MANMANYREG2_ST", 14 }, (SYMS_U64)SYMS_CvSymKind_MANMANYREG2_ST },
{ { (SYMS_U8*)"MANTYPREF", 9 }, (SYMS_U64)SYMS_CvSymKind_MANTYPREF },
{ { (SYMS_U8*)"UNAMESPACE_ST", 13 }, (SYMS_U64)SYMS_CvSymKind_UNAMESPACE_ST },
{ { (SYMS_U8*)"ST_MAX", 6 }, (SYMS_U64)SYMS_CvSymKind_ST_MAX },
{ { (SYMS_U8*)"OBJNAME", 7 }, (SYMS_U64)SYMS_CvSymKind_OBJNAME },
{ { (SYMS_U8*)"THUNK32", 7 }, (SYMS_U64)SYMS_CvSymKind_THUNK32 },
{ { (SYMS_U8*)"BLOCK32", 7 }, (SYMS_U64)SYMS_CvSymKind_BLOCK32 },
{ { (SYMS_U8*)"WITH32", 6 }, (SYMS_U64)SYMS_CvSymKind_WITH32 },
{ { (SYMS_U8*)"LABEL32", 7 }, (SYMS_U64)SYMS_CvSymKind_LABEL32 },
{ { (SYMS_U8*)"REGISTER", 8 }, (SYMS_U64)SYMS_CvSymKind_REGISTER },
{ { (SYMS_U8*)"CONSTANT", 8 }, (SYMS_U64)SYMS_CvSymKind_CONSTANT },
{ { (SYMS_U8*)"UDT", 3 }, (SYMS_U64)SYMS_CvSymKind_UDT },
{ { (SYMS_U8*)"COBOLUDT", 8 }, (SYMS_U64)SYMS_CvSymKind_COBOLUDT },
{ { (SYMS_U8*)"MANYREG", 7 }, (SYMS_U64)SYMS_CvSymKind_MANYREG },
{ { (SYMS_U8*)"BPREL32", 7 }, (SYMS_U64)SYMS_CvSymKind_BPREL32 },
{ { (SYMS_U8*)"LDATA32", 7 }, (SYMS_U64)SYMS_CvSymKind_LDATA32 },
{ { (SYMS_U8*)"GDATA32", 7 }, (SYMS_U64)SYMS_CvSymKind_GDATA32 },
{ { (SYMS_U8*)"PUB32", 5 }, (SYMS_U64)SYMS_CvSymKind_PUB32 },
{ { (SYMS_U8*)"LPROC32", 7 }, (SYMS_U64)SYMS_CvSymKind_LPROC32 },
{ { (SYMS_U8*)"GPROC32", 7 }, (SYMS_U64)SYMS_CvSymKind_GPROC32 },
{ { (SYMS_U8*)"REGREL32", 8 }, (SYMS_U64)SYMS_CvSymKind_REGREL32 },
{ { (SYMS_U8*)"LTHREAD32", 9 }, (SYMS_U64)SYMS_CvSymKind_LTHREAD32 },
{ { (SYMS_U8*)"GTHREAD32", 9 }, (SYMS_U64)SYMS_CvSymKind_GTHREAD32 },
{ { (SYMS_U8*)"LPROCMIPS", 9 }, (SYMS_U64)SYMS_CvSymKind_LPROCMIPS },
{ { (SYMS_U8*)"GPROCMIPS", 9 }, (SYMS_U64)SYMS_CvSymKind_GPROCMIPS },
{ { (SYMS_U8*)"COMPILE2", 8 }, (SYMS_U64)SYMS_CvSymKind_COMPILE2 },
{ { (SYMS_U8*)"MANYREG2", 8 }, (SYMS_U64)SYMS_CvSymKind_MANYREG2 },
{ { (SYMS_U8*)"LPROCIA64", 9 }, (SYMS_U64)SYMS_CvSymKind_LPROCIA64 },
{ { (SYMS_U8*)"GPROCIA64", 9 }, (SYMS_U64)SYMS_CvSymKind_GPROCIA64 },
{ { (SYMS_U8*)"LOCALSLOT", 9 }, (SYMS_U64)SYMS_CvSymKind_LOCALSLOT },
{ { (SYMS_U8*)"PARAMSLOT", 9 }, (SYMS_U64)SYMS_CvSymKind_PARAMSLOT },
{ { (SYMS_U8*)"LMANDATA", 8 }, (SYMS_U64)SYMS_CvSymKind_LMANDATA },
{ { (SYMS_U8*)"GMANDATA", 8 }, (SYMS_U64)SYMS_CvSymKind_GMANDATA },
{ { (SYMS_U8*)"MANFRAMEREL", 11 }, (SYMS_U64)SYMS_CvSymKind_MANFRAMEREL },
{ { (SYMS_U8*)"MANREGISTER", 11 }, (SYMS_U64)SYMS_CvSymKind_MANREGISTER },
{ { (SYMS_U8*)"MANSLOT", 7 }, (SYMS_U64)SYMS_CvSymKind_MANSLOT },
{ { (SYMS_U8*)"MANMANYREG", 10 }, (SYMS_U64)SYMS_CvSymKind_MANMANYREG },
{ { (SYMS_U8*)"MANREGREL", 9 }, (SYMS_U64)SYMS_CvSymKind_MANREGREL },
{ { (SYMS_U8*)"MANMANYREG2", 11 }, (SYMS_U64)SYMS_CvSymKind_MANMANYREG2 },
{ { (SYMS_U8*)"UNAMESPACE", 10 }, (SYMS_U64)SYMS_CvSymKind_UNAMESPACE },
{ { (SYMS_U8*)"PROCREF", 7 }, (SYMS_U64)SYMS_CvSymKind_PROCREF },
{ { (SYMS_U8*)"DATAREF", 7 }, (SYMS_U64)SYMS_CvSymKind_DATAREF },
{ { (SYMS_U8*)"LPROCREF", 8 }, (SYMS_U64)SYMS_CvSymKind_LPROCREF },
{ { (SYMS_U8*)"ANNOTATIONREF", 13 }, (SYMS_U64)SYMS_CvSymKind_ANNOTATIONREF },
{ { (SYMS_U8*)"TOKENREF", 8 }, (SYMS_U64)SYMS_CvSymKind_TOKENREF },
{ { (SYMS_U8*)"GMANPROC", 8 }, (SYMS_U64)SYMS_CvSymKind_GMANPROC },
{ { (SYMS_U8*)"LMANPROC", 8 }, (SYMS_U64)SYMS_CvSymKind_LMANPROC },
{ { (SYMS_U8*)"TRAMPOLINE", 10 }, (SYMS_U64)SYMS_CvSymKind_TRAMPOLINE },
{ { (SYMS_U8*)"MANCONSTANT", 11 }, (SYMS_U64)SYMS_CvSymKind_MANCONSTANT },
{ { (SYMS_U8*)"ATTR_FRAMEREL", 13 }, (SYMS_U64)SYMS_CvSymKind_ATTR_FRAMEREL },
{ { (SYMS_U8*)"ATTR_REGISTER", 13 }, (SYMS_U64)SYMS_CvSymKind_ATTR_REGISTER },
{ { (SYMS_U8*)"ATTR_REGREL", 11 }, (SYMS_U64)SYMS_CvSymKind_ATTR_REGREL },
{ { (SYMS_U8*)"ATTR_MANYREG", 12 }, (SYMS_U64)SYMS_CvSymKind_ATTR_MANYREG },
{ { (SYMS_U8*)"SEPCODE", 7 }, (SYMS_U64)SYMS_CvSymKind_SEPCODE },
{ { (SYMS_U8*)"DEFRANGE_2005", 13 }, (SYMS_U64)SYMS_CvSymKind_DEFRANGE_2005 },
{ { (SYMS_U8*)"DEFRANGE2_2005", 14 }, (SYMS_U64)SYMS_CvSymKind_DEFRANGE2_2005 },
{ { (SYMS_U8*)"SECTION", 7 }, (SYMS_U64)SYMS_CvSymKind_SECTION },
{ { (SYMS_U8*)"COFFGROUP", 9 }, (SYMS_U64)SYMS_CvSymKind_COFFGROUP },
{ { (SYMS_U8*)"EXPORT", 6 }, (SYMS_U64)SYMS_CvSymKind_EXPORT },
{ { (SYMS_U8*)"CALLSITEINFO", 12 }, (SYMS_U64)SYMS_CvSymKind_CALLSITEINFO },
{ { (SYMS_U8*)"FRAMECOOKIE", 11 }, (SYMS_U64)SYMS_CvSymKind_FRAMECOOKIE },
{ { (SYMS_U8*)"DISCARDED", 9 }, (SYMS_U64)SYMS_CvSymKind_DISCARDED },
{ { (SYMS_U8*)"COMPILE3", 8 }, (SYMS_U64)SYMS_CvSymKind_COMPILE3 },
{ { (SYMS_U8*)"ENVBLOCK", 8 }, (SYMS_U64)SYMS_CvSymKind_ENVBLOCK },
{ { (SYMS_U8*)"LOCAL", 5 }, (SYMS_U64)SYMS_CvSymKind_LOCAL },
{ { (SYMS_U8*)"DEFRANGE", 8 }, (SYMS_U64)SYMS_CvSymKind_DEFRANGE },
{ { (SYMS_U8*)"DEFRANGE_SUBFIELD", 17 }, (SYMS_U64)SYMS_CvSymKind_DEFRANGE_SUBFIELD },
{ { (SYMS_U8*)"DEFRANGE_REGISTER", 17 }, (SYMS_U64)SYMS_CvSymKind_DEFRANGE_REGISTER },
{ { (SYMS_U8*)"DEFRANGE_FRAMEPOINTER_REL", 25 }, (SYMS_U64)SYMS_CvSymKind_DEFRANGE_FRAMEPOINTER_REL },
{ { (SYMS_U8*)"DEFRANGE_SUBFIELD_REGISTER", 26 }, (SYMS_U64)SYMS_CvSymKind_DEFRANGE_SUBFIELD_REGISTER },
{ { (SYMS_U8*)"DEFRANGE_FRAMEPOINTER_REL_FULL_SCOPE", 36 }, (SYMS_U64)SYMS_CvSymKind_DEFRANGE_FRAMEPOINTER_REL_FULL_SCOPE },
{ { (SYMS_U8*)"DEFRANGE_REGISTER_REL", 21 }, (SYMS_U64)SYMS_CvSymKind_DEFRANGE_REGISTER_REL },
{ { (SYMS_U8*)"LPROC32_ID", 10 }, (SYMS_U64)SYMS_CvSymKind_LPROC32_ID },
{ { (SYMS_U8*)"GPROC32_ID", 10 }, (SYMS_U64)SYMS_CvSymKind_GPROC32_ID },
{ { (SYMS_U8*)"LPROCMIPS_ID", 12 }, (SYMS_U64)SYMS_CvSymKind_LPROCMIPS_ID },
{ { (SYMS_U8*)"GPROCMIPS_ID", 12 }, (SYMS_U64)SYMS_CvSymKind_GPROCMIPS_ID },
{ { (SYMS_U8*)"LPROCIA64_ID", 12 }, (SYMS_U64)SYMS_CvSymKind_LPROCIA64_ID },
{ { (SYMS_U8*)"GPROCIA64_ID", 12 }, (SYMS_U64)SYMS_CvSymKind_GPROCIA64_ID },
{ { (SYMS_U8*)"BUILDINFO", 9 }, (SYMS_U64)SYMS_CvSymKind_BUILDINFO },
{ { (SYMS_U8*)"INLINESITE", 10 }, (SYMS_U64)SYMS_CvSymKind_INLINESITE },
{ { (SYMS_U8*)"INLINESITE_END", 14 }, (SYMS_U64)SYMS_CvSymKind_INLINESITE_END },
{ { (SYMS_U8*)"PROC_ID_END", 11 }, (SYMS_U64)SYMS_CvSymKind_PROC_ID_END },
{ { (SYMS_U8*)"DEFRANGE_HLSL", 13 }, (SYMS_U64)SYMS_CvSymKind_DEFRANGE_HLSL },
{ { (SYMS_U8*)"GDATA_HLSL", 10 }, (SYMS_U64)SYMS_CvSymKind_GDATA_HLSL },
{ { (SYMS_U8*)"LDATA_HLSL", 10 }, (SYMS_U64)SYMS_CvSymKind_LDATA_HLSL },
{ { (SYMS_U8*)"FILESTATIC", 10 }, (SYMS_U64)SYMS_CvSymKind_FILESTATIC },
{ { (SYMS_U8*)"LPROC32_DPC", 11 }, (SYMS_U64)SYMS_CvSymKind_LPROC32_DPC },
{ { (SYMS_U8*)"LPROC32_DPC_ID", 14 }, (SYMS_U64)SYMS_CvSymKind_LPROC32_DPC_ID },
{ { (SYMS_U8*)"DEFRANGE_DPC_PTR_TAG", 20 }, (SYMS_U64)SYMS_CvSymKind_DEFRANGE_DPC_PTR_TAG },
{ { (SYMS_U8*)"DPC_SYM_TAG_MAP", 15 }, (SYMS_U64)SYMS_CvSymKind_DPC_SYM_TAG_MAP },
{ { (SYMS_U8*)"ARMSWITCHTABLE", 14 }, (SYMS_U64)SYMS_CvSymKind_ARMSWITCHTABLE },
{ { (SYMS_U8*)"CALLEES", 7 }, (SYMS_U64)SYMS_CvSymKind_CALLEES },
{ { (SYMS_U8*)"CALLERS", 7 }, (SYMS_U64)SYMS_CvSymKind_CALLERS },
{ { (SYMS_U8*)"POGODATA", 8 }, (SYMS_U64)SYMS_CvSymKind_POGODATA },
{ { (SYMS_U8*)"INLINESITE2", 11 }, (SYMS_U64)SYMS_CvSymKind_INLINESITE2 },
{ { (SYMS_U8*)"HEAPALLOCSITE", 13 }, (SYMS_U64)SYMS_CvSymKind_HEAPALLOCSITE },
{ { (SYMS_U8*)"MOD_TYPEREF", 11 }, (SYMS_U64)SYMS_CvSymKind_MOD_TYPEREF },
{ { (SYMS_U8*)"REF_MINIPDB", 11 }, (SYMS_U64)SYMS_CvSymKind_REF_MINIPDB },
{ { (SYMS_U8*)"PDBMAP", 6 }, (SYMS_U64)SYMS_CvSymKind_PDBMAP },
{ { (SYMS_U8*)"GDATA_HLSL32", 12 }, (SYMS_U64)SYMS_CvSymKind_GDATA_HLSL32 },
{ { (SYMS_U8*)"LDATA_HLSL32", 12 }, (SYMS_U64)SYMS_CvSymKind_LDATA_HLSL32 },
{ { (SYMS_U8*)"GDATA_HLSL32_EX", 15 }, (SYMS_U64)SYMS_CvSymKind_GDATA_HLSL32_EX },
{ { (SYMS_U8*)"LDATA_HLSL32_EX", 15 }, (SYMS_U64)SYMS_CvSymKind_LDATA_HLSL32_EX },
{ { (SYMS_U8*)"FASTLINK", 8 }, (SYMS_U64)SYMS_CvSymKind_FASTLINK },
{ { (SYMS_U8*)"INLINEES", 8 }, (SYMS_U64)SYMS_CvSymKind_INLINEES },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_CvAnnotation[] = {
{ {(SYMS_U8*)"off", 3}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"seg", 3}, &_syms_serial_type_SYMS_U16, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"count", 5}, &_syms_serial_type_SYMS_U16, SYMS_SerialWidthKind_Null, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_CvRegister[] = {
{ {(SYMS_U8*)"type", 4}, &_syms_serial_type_SYMS_CvTypeId, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"reg", 3}, &_syms_serial_type_SYMS_U16, SYMS_SerialWidthKind_Null, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_CvBPRelSym32[] = {
{ {(SYMS_U8*)"off", 3}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"itype", 5}, &_syms_serial_type_SYMS_CvTypeId, SYMS_SerialWidthKind_Null, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_CvVPathSym32[] = {
{ {(SYMS_U8*)"root", 4}, &_syms_serial_type_SYMS_CvTypeId, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"path", 4}, &_syms_serial_type_SYMS_CvTypeId, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"off", 3}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"seg", 3}, &_syms_serial_type_SYMS_U16, SYMS_SerialWidthKind_Null, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_CvOEM[] = {
{ {(SYMS_U8*)"id", 2}, &_syms_serial_type_SYMS_CvGuid, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"itype", 5}, &_syms_serial_type_SYMS_CvTypeId, SYMS_SerialWidthKind_Null, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_CvSLink32[] = {
{ {(SYMS_U8*)"frame_size", 10}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"offset", 6}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"reg", 3}, &_syms_serial_type_SYMS_U16, SYMS_SerialWidthKind_Null, 0 },
};
SYMS_SerialValue _syms_serial_members_for_SYMS_CvGenericStyle[] = {
{ { (SYMS_U8*)"VOID", 4 }, (SYMS_U64)SYMS_CvGenericStyle_VOID },
{ { (SYMS_U8*)"REG", 3 }, (SYMS_U64)SYMS_CvGenericStyle_REG },
{ { (SYMS_U8*)"ICAN", 4 }, (SYMS_U64)SYMS_CvGenericStyle_ICAN },
{ { (SYMS_U8*)"ICAF", 4 }, (SYMS_U64)SYMS_CvGenericStyle_ICAF },
{ { (SYMS_U8*)"IRAN", 4 }, (SYMS_U64)SYMS_CvGenericStyle_IRAN },
{ { (SYMS_U8*)"IRAF", 4 }, (SYMS_U64)SYMS_CvGenericStyle_IRAF },
{ { (SYMS_U8*)"UNUSED", 6 }, (SYMS_U64)SYMS_CvGenericStyle_UNUSED },
};
SYMS_SerialFlag _syms_serial_members_for_SYMS_CvGenericFlags[] = {
{ { (SYMS_U8*)"CSTYLE", 6 }, &_syms_serial_type_SYMS_U16, 0x1, 0 },
{ { (SYMS_U8*)"RSCLEAN", 7 }, &_syms_serial_type_SYMS_U16, 0x1, 1 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_CvReturn[] = {
{ {(SYMS_U8*)"flags", 5}, &_syms_serial_type_SYMS_CvGenericFlags, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"style", 5}, &_syms_serial_type_SYMS_CvGenericStyle, SYMS_SerialWidthKind_Null, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_CvStartSearch[] = {
{ {(SYMS_U8*)"start_symbol", 12}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"segment", 7}, &_syms_serial_type_SYMS_U16, SYMS_SerialWidthKind_Null, 0 },
};
SYMS_SerialValue _syms_serial_members_for_SYMS_CvLanguage[] = {
{ { (SYMS_U8*)"C", 1 }, (SYMS_U64)SYMS_CvLanguage_C },
{ { (SYMS_U8*)"C++", 3 }, (SYMS_U64)SYMS_CvLanguage_CXX },
{ { (SYMS_U8*)"FORTRAN", 7 }, (SYMS_U64)SYMS_CvLanguage_FORTRAN },
{ { (SYMS_U8*)"MASM", 4 }, (SYMS_U64)SYMS_CvLanguage_MASM },
{ { (SYMS_U8*)"PASCAL", 6 }, (SYMS_U64)SYMS_CvLanguage_PASCAL },
{ { (SYMS_U8*)"BASIC", 5 }, (SYMS_U64)SYMS_CvLanguage_BASIC },
{ { (SYMS_U8*)"COBOL", 5 }, (SYMS_U64)SYMS_CvLanguage_COBOL },
{ { (SYMS_U8*)"LINK", 4 }, (SYMS_U64)SYMS_CvLanguage_LINK },
{ { (SYMS_U8*)"CVTRES", 6 }, (SYMS_U64)SYMS_CvLanguage_CVTRES },
{ { (SYMS_U8*)"CVTPGD", 6 }, (SYMS_U64)SYMS_CvLanguage_CVTPGD },
{ { (SYMS_U8*)"CSHARP", 6 }, (SYMS_U64)SYMS_CvLanguage_CSHARP },
{ { (SYMS_U8*)"VB", 2 }, (SYMS_U64)SYMS_CvLanguage_VB },
{ { (SYMS_U8*)"ILASM", 5 }, (SYMS_U64)SYMS_CvLanguage_ILASM },
{ { (SYMS_U8*)"JAVA", 4 }, (SYMS_U64)SYMS_CvLanguage_JAVA },
{ { (SYMS_U8*)"JSCRIPT", 7 }, (SYMS_U64)SYMS_CvLanguage_JSCRIPT },
{ { (SYMS_U8*)"MSIL", 4 }, (SYMS_U64)SYMS_CvLanguage_MSIL },
{ { (SYMS_U8*)"HLSL", 4 }, (SYMS_U64)SYMS_CvLanguage_HLSL },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_CvThread32[] = {
{ {(SYMS_U8*)"itype", 5}, &_syms_serial_type_SYMS_CvTypeId, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"tls_off", 7}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"tls_seg", 7}, &_syms_serial_type_SYMS_U16, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"name", 4}, &_syms_serial_type_SYMS_U8, SYMS_SerialWidthKind_NullTerminated, 0 },
};
SYMS_SerialFlag _syms_serial_members_for_SYMS_CvCompileFlags[] = {
{ { (SYMS_U8*)"Language", 8 }, &_syms_serial_type_SYMS_CvLanguage, 0xff, 0 },
{ { (SYMS_U8*)"Pcode", 5 }, &_syms_serial_type_SYMS_U32, 0x1, 8 },
{ { (SYMS_U8*)"FloatPrec", 9 }, &_syms_serial_type_SYMS_U8, 0x3, 9 },
{ { (SYMS_U8*)"FloatPkg", 8 }, &_syms_serial_type_SYMS_U8, 0x3, 11 },
{ { (SYMS_U8*)"AmbientData", 11 }, &_syms_serial_type_SYMS_U8, 0x7, 13 },
{ { (SYMS_U8*)"AmbientCode", 11 }, &_syms_serial_type_SYMS_U8, 0x7, 16 },
{ { (SYMS_U8*)"Mode", 4 }, &_syms_serial_type_SYMS_U32, 0x1, 19 },
{ { (SYMS_U8*)"pad", 3 }, &_syms_serial_type_SYMS_U8, 0xf, 20 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_CvCompile[] = {
{ {(SYMS_U8*)"machine", 7}, &_syms_serial_type_SYMS_U8, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"flags", 5}, &_syms_serial_type_SYMS_CvCompileFlags, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"ver_str", 7}, &_syms_serial_type_SYMS_U8, SYMS_SerialWidthKind_NullTerminated, 0 },
};
SYMS_SerialFlag _syms_serial_members_for_SYMS_CvCompile2Flags[] = {
{ { (SYMS_U8*)"Language", 8 }, &_syms_serial_type_SYMS_CvLanguage, 0xff, 0 },
{ { (SYMS_U8*)"Compiled for edit and continue", 30 }, &_syms_serial_type_SYMS_U32, 0x1, 8 },
{ { (SYMS_U8*)"Does not have debug info", 24 }, &_syms_serial_type_SYMS_U32, 0x1, 9 },
{ { (SYMS_U8*)"Compiled with /LTCG", 19 }, &_syms_serial_type_SYMS_U32, 0x1, 10 },
{ { (SYMS_U8*)"Compiled with /bzalign", 22 }, &_syms_serial_type_SYMS_U32, 0x1, 11 },
{ { (SYMS_U8*)"Managed code present", 20 }, &_syms_serial_type_SYMS_U32, 0x1, 12 },
{ { (SYMS_U8*)"Compiled with /GS", 17 }, &_syms_serial_type_SYMS_U32, 0x1, 13 },
{ { (SYMS_U8*)"Compiled with /hotpatch", 23 }, &_syms_serial_type_SYMS_U32, 0x1, 14 },
{ { (SYMS_U8*)"Converted by CVTCIL", 19 }, &_syms_serial_type_SYMS_U32, 0x1, 15 },
{ { (SYMS_U8*)"MSIL Module", 11 }, &_syms_serial_type_SYMS_U32, 0x1, 16 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_CvCompile2[] = {
{ {(SYMS_U8*)"flags", 5}, &_syms_serial_type_SYMS_CvCompile2Flags, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"machine", 7}, &_syms_serial_type_SYMS_CvArch, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"ver_fe_major", 12}, &_syms_serial_type_SYMS_U16, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"ver_fe_minor", 12}, &_syms_serial_type_SYMS_U16, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"ver_fe_build", 12}, &_syms_serial_type_SYMS_U16, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"ver_major", 9}, &_syms_serial_type_SYMS_U16, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"ver_minor", 9}, &_syms_serial_type_SYMS_U16, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"ver_build", 9}, &_syms_serial_type_SYMS_U16, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"ver_str", 7}, &_syms_serial_type_SYMS_U8, SYMS_SerialWidthKind_NullTerminated, 0 },
};
SYMS_SerialFlag _syms_serial_members_for_SYMS_CvCompile3Flags[] = {
{ { (SYMS_U8*)"Language", 8 }, &_syms_serial_type_SYMS_CvLanguage, 0xff, 0 },
{ { (SYMS_U8*)"Compiled for edit and continue", 30 }, &_syms_serial_type_SYMS_U32, 0x1, 8 },
{ { (SYMS_U8*)"Does not have debug info", 24 }, &_syms_serial_type_SYMS_U32, 0x1, 9 },
{ { (SYMS_U8*)"Compiled with /LTCG", 19 }, &_syms_serial_type_SYMS_U32, 0x1, 10 },
{ { (SYMS_U8*)"Compiled with /bzalign", 22 }, &_syms_serial_type_SYMS_U32, 0x1, 11 },
{ { (SYMS_U8*)"Managed code present", 20 }, &_syms_serial_type_SYMS_U32, 0x1, 12 },
{ { (SYMS_U8*)"Compiled with /GS", 17 }, &_syms_serial_type_SYMS_U32, 0x1, 13 },
{ { (SYMS_U8*)"Compiled with /hotpatch", 23 }, &_syms_serial_type_SYMS_U32, 0x1, 14 },
{ { (SYMS_U8*)"Converted by CVTCIL", 19 }, &_syms_serial_type_SYMS_U32, 0x1, 15 },
{ { (SYMS_U8*)"MSIL Module", 11 }, &_syms_serial_type_SYMS_U32, 0x1, 16 },
{ { (SYMS_U8*)"Compiled with /sdl", 18 }, &_syms_serial_type_SYMS_U32, 0x1, 17 },
{ { (SYMS_U8*)"Compiled with pgo", 17 }, &_syms_serial_type_SYMS_U32, 0x1, 18 },
{ { (SYMS_U8*)".EXP Module", 11 }, &_syms_serial_type_SYMS_U32, 0x1, 19 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_CvCompile3[] = {
{ {(SYMS_U8*)"flags", 5}, &_syms_serial_type_SYMS_CvCompile3Flags, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"machine", 7}, &_syms_serial_type_SYMS_CvArch, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"Frontend Version (Major)", 24}, &_syms_serial_type_SYMS_U16, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"Frontend Version (Minor)", 24}, &_syms_serial_type_SYMS_U16, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"Frontend Version (Build)", 24}, &_syms_serial_type_SYMS_U16, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"Frontend Version (QFE)", 22}, &_syms_serial_type_SYMS_U16, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"Backend Version (Major)", 23}, &_syms_serial_type_SYMS_U16, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"Backend Version (Minor)", 23}, &_syms_serial_type_SYMS_U16, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"Backend Version (Build)", 23}, &_syms_serial_type_SYMS_U16, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"Backend Version (QFE)", 21}, &_syms_serial_type_SYMS_U16, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"Version String", 14}, &_syms_serial_type_SYMS_U8, SYMS_SerialWidthKind_NullTerminated, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_CvObjname[] = {
{ {(SYMS_U8*)"sig", 3}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"name", 4}, &_syms_serial_type_SYMS_U8, SYMS_SerialWidthKind_NullTerminated, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_CvUNamespace[] = {
{ {(SYMS_U8*)"name", 4}, &_syms_serial_type_SYMS_U8, SYMS_SerialWidthKind_NullTerminated, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_CvRef2[] = {
{ {(SYMS_U8*)"sum_name", 8}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"sym_off", 7}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"imod", 4}, &_syms_serial_type_SYMS_CvModIndex, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"name", 4}, &_syms_serial_type_SYMS_U8, SYMS_SerialWidthKind_NullTerminated, 0 },
};
SYMS_SerialFlag _syms_serial_members_for_SYMS_CvSepcodeFlags[] = {
{ { (SYMS_U8*)"IS_LEXICAL_SCOPE", 16 }, &_syms_serial_type_SYMS_U32, 0x1, 0 },
{ { (SYMS_U8*)"RETURNS_TO_PARENT", 17 }, &_syms_serial_type_SYMS_U32, 0x1, 1 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_CvSepcode[] = {
{ {(SYMS_U8*)"parent", 6}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"end", 3}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"len", 3}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"flags", 5}, &_syms_serial_type_SYMS_CvSepcodeFlags, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"sec_off", 7}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"sec_parent_off", 14}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"sec", 3}, &_syms_serial_type_SYMS_U16, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"sec_parent", 10}, &_syms_serial_type_SYMS_U16, SYMS_SerialWidthKind_Null, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_CvSlotsym32[] = {
{ {(SYMS_U8*)"slot_index", 10}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"type", 4}, &_syms_serial_type_SYMS_CvTypeId, SYMS_SerialWidthKind_Null, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_CvPogoInfo[] = {
{ {(SYMS_U8*)"invocations", 11}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"dynamic_inst_count", 18}, &_syms_serial_type_SYMS_U64, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"static_inst_count", 17}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"post_inline_static_inst_count", 29}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_CvManyreg[] = {
{ {(SYMS_U8*)"type", 4}, &_syms_serial_type_SYMS_CvTypeId, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"count", 5}, &_syms_serial_type_SYMS_U8, SYMS_SerialWidthKind_Null, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_CvManyreg2[] = {
{ {(SYMS_U8*)"type", 4}, &_syms_serial_type_SYMS_CvTypeId, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"count", 5}, &_syms_serial_type_SYMS_U16, SYMS_SerialWidthKind_Null, 0 },
};
SYMS_SerialValue _syms_serial_members_for_SYMS_CvEncodedFramePtrReg[] = {
{ { (SYMS_U8*)"NONE", 4 }, (SYMS_U64)SYMS_CvEncodedFramePtrReg_None },
{ { (SYMS_U8*)"STACKPTR", 8 }, (SYMS_U64)SYMS_CvEncodedFramePtrReg_StackPtr },
{ { (SYMS_U8*)"FRAMEPTR", 8 }, (SYMS_U64)SYMS_CvEncodedFramePtrReg_FramePtr },
{ { (SYMS_U8*)"BASEPTR", 7 }, (SYMS_U64)SYMS_CvEncodedFramePtrReg_BasePtr },
};
SYMS_SerialFlag _syms_serial_members_for_SYMS_CvFrameprocFlags[] = {
{ { (SYMS_U8*)"USESALLOCA", 10 }, &_syms_serial_type_SYMS_U32, 0x1, 0 },
{ { (SYMS_U8*)"USESSETJMP", 10 }, &_syms_serial_type_SYMS_U32, 0x1, 1 },
{ { (SYMS_U8*)"USESLONGJMP", 11 }, &_syms_serial_type_SYMS_U32, 0x1, 2 },
{ { (SYMS_U8*)"USESINLASM", 10 }, &_syms_serial_type_SYMS_U32, 0x1, 3 },
{ { (SYMS_U8*)"USESEH", 6 }, &_syms_serial_type_SYMS_U32, 0x1, 4 },
{ { (SYMS_U8*)"INLINE", 6 }, &_syms_serial_type_SYMS_U32, 0x1, 5 },
{ { (SYMS_U8*)"HASSEH", 6 }, &_syms_serial_type_SYMS_U32, 0x1, 6 },
{ { (SYMS_U8*)"NAKED", 5 }, &_syms_serial_type_SYMS_U32, 0x1, 7 },
{ { (SYMS_U8*)"HASSECURITYCHECKS", 17 }, &_syms_serial_type_SYMS_U32, 0x1, 8 },
{ { (SYMS_U8*)"ASYNCEH", 7 }, &_syms_serial_type_SYMS_U32, 0x1, 9 },
{ { (SYMS_U8*)"GSNOSTACKORDERING", 17 }, &_syms_serial_type_SYMS_U32, 0x1, 10 },
{ { (SYMS_U8*)"WASINLINED", 10 }, &_syms_serial_type_SYMS_U32, 0x1, 11 },
{ { (SYMS_U8*)"GSCHECK", 7 }, &_syms_serial_type_SYMS_U32, 0x1, 12 },
{ { (SYMS_U8*)"SAFEBUFFERS", 11 }, &_syms_serial_type_SYMS_U32, 0x1, 13 },
{ { (SYMS_U8*)"LOCALBASEPOINTER", 16 }, &_syms_serial_type_SYMS_CvEncodedFramePtrReg, 0x3, 14 },
{ { (SYMS_U8*)"PARAMBASEPOINTER", 16 }, &_syms_serial_type_SYMS_CvEncodedFramePtrReg, 0x3, 16 },
{ { (SYMS_U8*)"POGOON", 6 }, &_syms_serial_type_SYMS_U32, 0x1, 18 },
{ { (SYMS_U8*)"POGOCOUNTSVALID", 15 }, &_syms_serial_type_SYMS_U32, 0x1, 19 },
{ { (SYMS_U8*)"OPTSPEED", 8 }, &_syms_serial_type_SYMS_U32, 0x1, 20 },
{ { (SYMS_U8*)"HASCFG", 6 }, &_syms_serial_type_SYMS_U32, 0x1, 21 },
{ { (SYMS_U8*)"HASCFW", 6 }, &_syms_serial_type_SYMS_U32, 0x1, 22 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_CvFrameproc[] = {
{ {(SYMS_U8*)"frame_size", 10}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"pad_size", 8}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"pad_off", 7}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"save_reg_size", 13}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"eh_off", 6}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"eh_sec", 6}, &_syms_serial_type_SYMS_CvSectionIndex, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"flags", 5}, &_syms_serial_type_SYMS_CvFrameprocFlags, SYMS_SerialWidthKind_Null, 0 },
};
SYMS_SerialValue _syms_serial_members_for_SYMS_CvThunkOrdinal[] = {
{ { (SYMS_U8*)"NOTYPE", 6 }, (SYMS_U64)SYMS_CvThunkOrdinal_NOTYPE },
{ { (SYMS_U8*)"ADJUSTOR", 8 }, (SYMS_U64)SYMS_CvThunkOrdinal_ADJUSTOR },
{ { (SYMS_U8*)"VCALL", 5 }, (SYMS_U64)SYMS_CvThunkOrdinal_VCALL },
{ { (SYMS_U8*)"PCODE", 5 }, (SYMS_U64)SYMS_CvThunkOrdinal_PCODE },
{ { (SYMS_U8*)"LOAD", 4 }, (SYMS_U64)SYMS_CvThunkOrdinal_LOAD },
{ { (SYMS_U8*)"TRAMP_INCREMENTAL", 17 }, (SYMS_U64)SYMS_CvThunkOrdinal_TRAMP_INCREMENTAL },
{ { (SYMS_U8*)"TRAMP_BRANCHISLAND", 18 }, (SYMS_U64)SYMS_CvThunkOrdinal_TRAMP_BRANCHISLAND },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_CvThunk32[] = {
{ {(SYMS_U8*)"parent", 6}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"end", 3}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"next", 4}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"off", 3}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"sec", 3}, &_syms_serial_type_SYMS_U16, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"len", 3}, &_syms_serial_type_SYMS_U16, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"ord", 3}, &_syms_serial_type_SYMS_CvThunkOrdinal, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"name", 4}, &_syms_serial_type_SYMS_U8, SYMS_SerialWidthKind_NullTerminated, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_CvBlock32[] = {
{ {(SYMS_U8*)"par", 3}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"end", 3}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"len", 3}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"off", 3}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"sec", 3}, &_syms_serial_type_SYMS_U16, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"name", 4}, &_syms_serial_type_SYMS_U8, SYMS_SerialWidthKind_NullTerminated, 0 },
};
SYMS_SerialFlag _syms_serial_members_for_SYMS_CvProcFlags[] = {
{ { (SYMS_U8*)"NOFPO", 5 }, &_syms_serial_type_SYMS_U8, 0x1, 0 },
{ { (SYMS_U8*)"INT_RETURN", 10 }, &_syms_serial_type_SYMS_U8, 0x1, 1 },
{ { (SYMS_U8*)"FAR_RETURN", 10 }, &_syms_serial_type_SYMS_U8, 0x1, 2 },
{ { (SYMS_U8*)"NEVER_RETURN", 12 }, &_syms_serial_type_SYMS_U8, 0x1, 3 },
{ { (SYMS_U8*)"NOTREACHED", 10 }, &_syms_serial_type_SYMS_U8, 0x1, 4 },
{ { (SYMS_U8*)"CUSTOM_CALL", 11 }, &_syms_serial_type_SYMS_U8, 0x1, 5 },
{ { (SYMS_U8*)"NOINLINE", 8 }, &_syms_serial_type_SYMS_U8, 0x1, 6 },
{ { (SYMS_U8*)"OPTDBGINFO", 10 }, &_syms_serial_type_SYMS_U8, 0x1, 7 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_CvLabel32[] = {
{ {(SYMS_U8*)"off", 3}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"sec", 3}, &_syms_serial_type_SYMS_U16, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"flags", 5}, &_syms_serial_type_SYMS_CvProcFlags, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"name", 4}, &_syms_serial_type_SYMS_U8, SYMS_SerialWidthKind_NullTerminated, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_CvConstant[] = {
{ {(SYMS_U8*)"itype", 5}, &_syms_serial_type_SYMS_CvTypeId, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"num", 3}, &_syms_serial_type_SYMS_PdbNumeric, SYMS_SerialWidthKind_PdbNumeric, 0 },
{ {(SYMS_U8*)"name", 4}, &_syms_serial_type_SYMS_U8, SYMS_SerialWidthKind_NullTerminated, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_CvUDT[] = {
{ {(SYMS_U8*)"itype", 5}, &_syms_serial_type_SYMS_CvTypeId, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"name", 4}, &_syms_serial_type_SYMS_U8, SYMS_SerialWidthKind_NullTerminated, 0 },
};
SYMS_SerialValue _syms_serial_members_for_SYMS_CvTrampolineKind[] = {
{ { (SYMS_U8*)"INCREMENTAL", 11 }, (SYMS_U64)SYMS_CvTrampolineKind_INCREMENTAL },
{ { (SYMS_U8*)"BRANCH_ISLAND", 13 }, (SYMS_U64)SYMS_CvTrampolineKind_BRANCH_ISLAND },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_CvTrampoline[] = {
{ {(SYMS_U8*)"type", 4}, &_syms_serial_type_SYMS_CvTrampolineKind, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"thunk_size", 10}, &_syms_serial_type_SYMS_U16, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"thunk_sec_off", 13}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"target_sec_off", 14}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"thunk_sec", 9}, &_syms_serial_type_SYMS_CvSectionIndex, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"target_sec", 10}, &_syms_serial_type_SYMS_CvSectionIndex, SYMS_SerialWidthKind_Null, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_CvSection[] = {
{ {(SYMS_U8*)"sec_index", 9}, &_syms_serial_type_SYMS_U16, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"align", 5}, &_syms_serial_type_SYMS_U8, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"pad", 3}, &_syms_serial_type_SYMS_U8, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"rva", 3}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"size", 4}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"characteristics", 15}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"name", 4}, &_syms_serial_type_SYMS_U8, SYMS_SerialWidthKind_NullTerminated, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_CvCoffGroup[] = {
{ {(SYMS_U8*)"size", 4}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"characteristics", 15}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"off", 3}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"sec", 3}, &_syms_serial_type_SYMS_U16, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"name", 4}, &_syms_serial_type_SYMS_U8, SYMS_SerialWidthKind_NullTerminated, 0 },
};
SYMS_SerialFlag _syms_serial_members_for_SYMS_CvExportFlags[] = {
{ { (SYMS_U8*)"CONSTANT", 8 }, &_syms_serial_type_SYMS_U16, 0x1, 0 },
{ { (SYMS_U8*)"DATA", 4 }, &_syms_serial_type_SYMS_U16, 0x1, 1 },
{ { (SYMS_U8*)"PRIVATE", 7 }, &_syms_serial_type_SYMS_U16, 0x1, 2 },
{ { (SYMS_U8*)"NONAME", 6 }, &_syms_serial_type_SYMS_U16, 0x1, 3 },
{ { (SYMS_U8*)"ORDINAL", 7 }, &_syms_serial_type_SYMS_U16, 0x1, 4 },
{ { (SYMS_U8*)"FORWARDER", 9 }, &_syms_serial_type_SYMS_U16, 0x1, 5 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_CvExport[] = {
{ {(SYMS_U8*)"ordinal", 7}, &_syms_serial_type_SYMS_U16, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"flags", 5}, &_syms_serial_type_SYMS_CvExportFlags, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"name", 4}, &_syms_serial_type_SYMS_U8, SYMS_SerialWidthKind_NullTerminated, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_CvCallSiteInfo[] = {
{ {(SYMS_U8*)"off", 3}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"sec", 3}, &_syms_serial_type_SYMS_U16, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"pad", 3}, &_syms_serial_type_SYMS_U16, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"itype", 5}, &_syms_serial_type_SYMS_CvTypeId, SYMS_SerialWidthKind_Null, 0 },
};
SYMS_SerialValue _syms_serial_members_for_SYMS_CvFrameCookieKind[] = {
{ { (SYMS_U8*)"COPY", 4 }, (SYMS_U64)SYMS_CvFrameCookieKind_COPY },
{ { (SYMS_U8*)"XOR_SP", 6 }, (SYMS_U64)SYMS_CvFrameCookieKind_XOR_SP },
{ { (SYMS_U8*)"XOR_BP", 6 }, (SYMS_U64)SYMS_CvFrameCookieKind_XOR_BP },
{ { (SYMS_U8*)"XOR_R13", 7 }, (SYMS_U64)SYMS_CvFrameCookieKind_XOR_R13 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_CvFrameCookie[] = {
{ {(SYMS_U8*)"off", 3}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"reg", 3}, &_syms_serial_type_SYMS_CvReg, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"kind", 4}, &_syms_serial_type_SYMS_CvFrameCookieKind, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"flags", 5}, &_syms_serial_type_SYMS_U8, SYMS_SerialWidthKind_Null, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_CvEnvblock[] = {
{ {(SYMS_U8*)"flags", 5}, &_syms_serial_type_SYMS_U8, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"rgsz", 4}, &_syms_serial_type_SYMS_U8, SYMS_SerialWidthKind_SequenceNullTerminated, 0 },
};
SYMS_SerialFlag _syms_serial_members_for_SYMS_CvLocalFlags[] = {
{ { (SYMS_U8*)"PARAM", 5 }, &_syms_serial_type_SYMS_U16, 0x1, 0 },
{ { (SYMS_U8*)"ADDR_TAKEN", 10 }, &_syms_serial_type_SYMS_U16, 0x1, 1 },
{ { (SYMS_U8*)"COMPGEN", 7 }, &_syms_serial_type_SYMS_U16, 0x1, 2 },
{ { (SYMS_U8*)"AGGREGATE", 9 }, &_syms_serial_type_SYMS_U16, 0x1, 3 },
{ { (SYMS_U8*)"PARTOF_ARGGREGATE", 17 }, &_syms_serial_type_SYMS_U16, 0x1, 4 },
{ { (SYMS_U8*)"ALIASED", 7 }, &_syms_serial_type_SYMS_U16, 0x1, 5 },
{ { (SYMS_U8*)"ALIAS", 5 }, &_syms_serial_type_SYMS_U16, 0x1, 6 },
{ { (SYMS_U8*)"RETVAL", 6 }, &_syms_serial_type_SYMS_U16, 0x1, 7 },
{ { (SYMS_U8*)"OPTOUT", 6 }, &_syms_serial_type_SYMS_U16, 0x1, 8 },
{ { (SYMS_U8*)"GLOBAL", 6 }, &_syms_serial_type_SYMS_U16, 0x1, 9 },
{ { (SYMS_U8*)"STATIC", 6 }, &_syms_serial_type_SYMS_U16, 0x1, 10 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_CvLocal[] = {
{ {(SYMS_U8*)"itype", 5}, &_syms_serial_type_SYMS_CvTypeId, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"flags", 5}, &_syms_serial_type_SYMS_CvLocalFlags, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"name", 4}, &_syms_serial_type_SYMS_U8, SYMS_SerialWidthKind_NullTerminated, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_CvLvarAddrRange[] = {
{ {(SYMS_U8*)"off", 3}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"sec", 3}, &_syms_serial_type_SYMS_U16, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"len", 3}, &_syms_serial_type_SYMS_U16, SYMS_SerialWidthKind_Null, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_CvLvarAddrGap[] = {
{ {(SYMS_U8*)"off", 3}, &_syms_serial_type_SYMS_U16, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"len", 3}, &_syms_serial_type_SYMS_U16, SYMS_SerialWidthKind_Null, 0 },
};
SYMS_SerialFlag _syms_serial_members_for_SYMS_CvRangeAttribs[] = {
{ { (SYMS_U8*)"MAYBE", 5 }, &_syms_serial_type_SYMS_U16, 0x1, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_CvDefrange[] = {
{ {(SYMS_U8*)"program", 7}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"range", 5}, &_syms_serial_type_SYMS_CvLvarAddrRange, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"gaps", 4}, &_syms_serial_type_SYMS_CvLvarAddrGap, SYMS_SerialWidthKind_RestOfStream, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_CvDefrangeSubfield[] = {
{ {(SYMS_U8*)"program", 7}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"off_in_parent", 13}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"range", 5}, &_syms_serial_type_SYMS_CvLvarAddrRange, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"gaps", 4}, &_syms_serial_type_SYMS_CvLvarAddrGap, SYMS_SerialWidthKind_RestOfStream, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_CvDefrangeRegister[] = {
{ {(SYMS_U8*)"reg", 3}, &_syms_serial_type_SYMS_CvReg, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"attribs", 7}, &_syms_serial_type_SYMS_CvRangeAttribs, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"range", 5}, &_syms_serial_type_SYMS_CvLvarAddrRange, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"gaps", 4}, &_syms_serial_type_SYMS_CvLvarAddrGap, SYMS_SerialWidthKind_RestOfStream, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_CvDefrangeFramepointerRel[] = {
{ {(SYMS_U8*)"off", 3}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"range", 5}, &_syms_serial_type_SYMS_CvLvarAddrRange, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"gaps", 4}, &_syms_serial_type_SYMS_CvLvarAddrGap, SYMS_SerialWidthKind_RestOfStream, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_CvDefrangeSubfieldRegister[] = {
{ {(SYMS_U8*)"reg", 3}, &_syms_serial_type_SYMS_CvReg, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"attribs", 7}, &_syms_serial_type_SYMS_CvRangeAttribs, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"off_parent", 10}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"range", 5}, &_syms_serial_type_SYMS_CvLvarAddrRange, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"gaps", 4}, &_syms_serial_type_SYMS_CvLvarAddrGap, SYMS_SerialWidthKind_RestOfStream, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_CvDefrangeFramepointerRelFullScope[] = {
{ {(SYMS_U8*)"off", 3}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
};
SYMS_SerialFlag _syms_serial_members_for_SYMS_CvDefrangeRegisterRelFlags[] = {
{ { (SYMS_U8*)"SPILLED_OUT_UDT_MEMBER", 22 }, &_syms_serial_type_SYMS_U16, 0x1, 0 },
{ { (SYMS_U8*)"UNUSED_1", 8 }, &_syms_serial_type_SYMS_U16, 0x1, 1 },
{ { (SYMS_U8*)"UNUSED_2", 8 }, &_syms_serial_type_SYMS_U16, 0x1, 2 },
{ { (SYMS_U8*)"UNUSED_3", 8 }, &_syms_serial_type_SYMS_U16, 0x1, 3 },
{ { (SYMS_U8*)"OFFSET_PARENT", 13 }, &_syms_serial_type_SYMS_U16, 0xfff, 4 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_CvDefrangeRegisterRel[] = {
{ {(SYMS_U8*)"reg", 3}, &_syms_serial_type_SYMS_CvReg, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"flags", 5}, &_syms_serial_type_SYMS_CvDefrangeRegisterRelFlags, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"reg_off", 7}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"range", 5}, &_syms_serial_type_SYMS_CvLvarAddrRange, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"gaps", 4}, &_syms_serial_type_SYMS_CvLvarAddrGap, SYMS_SerialWidthKind_RestOfStream, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_CvData32[] = {
{ {(SYMS_U8*)"itype", 5}, &_syms_serial_type_SYMS_CvTypeId, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"sec_off", 7}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"sec", 3}, &_syms_serial_type_SYMS_CvSectionIndex, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"name", 4}, &_syms_serial_type_SYMS_U8, SYMS_SerialWidthKind_NullTerminated, 0 },
};
SYMS_SerialFlag _syms_serial_members_for_SYMS_CvPubsymFlags[] = {
{ { (SYMS_U8*)"CODE", 4 }, &_syms_serial_type_SYMS_U32, 0x1, 0 },
{ { (SYMS_U8*)"FUNCTION", 8 }, &_syms_serial_type_SYMS_U32, 0x1, 1 },
{ { (SYMS_U8*)"MANAGED_CODE", 12 }, &_syms_serial_type_SYMS_U32, 0x1, 2 },
{ { (SYMS_U8*)"MSIL", 4 }, &_syms_serial_type_SYMS_U32, 0x1, 3 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_CvPubsym32[] = {
{ {(SYMS_U8*)"flags", 5}, &_syms_serial_type_SYMS_CvPubsymFlags, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"off", 3}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"sec", 3}, &_syms_serial_type_SYMS_U16, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"name", 4}, &_syms_serial_type_SYMS_U8, SYMS_SerialWidthKind_NullTerminated, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_CvProc32[] = {
{ {(SYMS_U8*)"parent", 6}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"end", 3}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"next", 4}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"len", 3}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"dbg_start", 9}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"dbg_end", 7}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"itype", 5}, &_syms_serial_type_SYMS_CvTypeId, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"off", 3}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"sec", 3}, &_syms_serial_type_SYMS_U16, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"flags", 5}, &_syms_serial_type_SYMS_CvProcFlags, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"name", 4}, &_syms_serial_type_SYMS_U8, SYMS_SerialWidthKind_NullTerminated, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_CvRegrel32[] = {
{ {(SYMS_U8*)"reg_off", 7}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"itype", 5}, &_syms_serial_type_SYMS_CvTypeId, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"reg", 3}, &_syms_serial_type_SYMS_CvReg, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"name", 4}, &_syms_serial_type_SYMS_U8, SYMS_SerialWidthKind_NullTerminated, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_CvBuildInfo[] = {
{ {(SYMS_U8*)"id", 2}, &_syms_serial_type_SYMS_CvItemId, SYMS_SerialWidthKind_Null, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_CvFunctionList[] = {
{ {(SYMS_U8*)"count", 5}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"func", 4}, &_syms_serial_type_SYMS_CvTypeId, SYMS_SerialWidthKind_Array, 0 },
{ {(SYMS_U8*)"invocations", 11}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Array, 0 },
};
SYMS_SerialValue _syms_serial_members_for_SYMS_CvBAOpcode[] = {
{ { (SYMS_U8*)"Invalid", 7 }, (SYMS_U64)SYMS_CvBAOpcode_Invalid },
{ { (SYMS_U8*)"CodeOffset", 10 }, (SYMS_U64)SYMS_CvBAOpcode_CodeOffset },
{ { (SYMS_U8*)"ChangeCodeOffsetBase", 20 }, (SYMS_U64)SYMS_CvBAOpcode_ChangeCodeOffsetBase },
{ { (SYMS_U8*)"ChangeCodeOffset", 16 }, (SYMS_U64)SYMS_CvBAOpcode_ChangeCodeOffset },
{ { (SYMS_U8*)"ChangeCodeLength", 16 }, (SYMS_U64)SYMS_CvBAOpcode_ChangeCodeLength },
{ { (SYMS_U8*)"ChangeFile", 10 }, (SYMS_U64)SYMS_CvBAOpcode_ChangeFile },
{ { (SYMS_U8*)"ChangeLineOffset", 16 }, (SYMS_U64)SYMS_CvBAOpcode_ChangeLineOffset },
{ { (SYMS_U8*)"ChangeLineEndDelta", 18 }, (SYMS_U64)SYMS_CvBAOpcode_ChangeLineEndDelta },
{ { (SYMS_U8*)"ChangeRangeKind", 15 }, (SYMS_U64)SYMS_CvBAOpcode_ChangeRangeKind },
{ { (SYMS_U8*)"ChangeColumnStart", 17 }, (SYMS_U64)SYMS_CvBAOpcode_ChangeColumnStart },
{ { (SYMS_U8*)"ChangeColumnEndDelta", 20 }, (SYMS_U64)SYMS_CvBAOpcode_ChangeColumnEndDelta },
{ { (SYMS_U8*)"ChangeCodeOffsetAndLineOffset", 29 }, (SYMS_U64)SYMS_CvBAOpcode_ChangeCodeOffsetAndLineOffset },
{ { (SYMS_U8*)"ChangeCodeLengthAndCodeOffset", 29 }, (SYMS_U64)SYMS_CvBAOpcode_ChangeCodeLengthAndCodeOffset },
{ { (SYMS_U8*)"ChangeColumnEnd", 15 }, (SYMS_U64)SYMS_CvBAOpcode_ChangeColumnEnd },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_CvInlineSite[] = {
{ {(SYMS_U8*)"parent_offset", 13}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"end_offset", 10}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"inlinee", 7}, &_syms_serial_type_SYMS_CvItemId, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"binaryAnnotations", 17}, &_syms_serial_type_SYMS_PdbBinaryAnnotation, SYMS_SerialWidthKind_RestOfStream, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_CvInlineSite2[] = {
{ {(SYMS_U8*)"parent_offset", 13}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"end_offset", 10}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"inlinee", 7}, &_syms_serial_type_SYMS_CvItemId, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"invocations", 11}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"binaryAnnotations", 17}, &_syms_serial_type_SYMS_PdbBinaryAnnotation, SYMS_SerialWidthKind_RestOfStream, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_CvInlinees[] = {
{ {(SYMS_U8*)"count", 5}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"desc", 4}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Array, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_CvFileStatic[] = {
{ {(SYMS_U8*)"itype", 5}, &_syms_serial_type_SYMS_CvTypeId, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"mod_offset", 10}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"flags", 5}, &_syms_serial_type_SYMS_CvLocalFlags, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"name", 4}, &_syms_serial_type_SYMS_U8, SYMS_SerialWidthKind_NullTerminated, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_CvHeapAllocSite[] = {
{ {(SYMS_U8*)"off", 3}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"sec", 3}, &_syms_serial_type_SYMS_U16, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"call_inst_len", 13}, &_syms_serial_type_SYMS_U16, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"itype", 5}, &_syms_serial_type_SYMS_CvTypeId, SYMS_SerialWidthKind_Null, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_CvLocalVarAttr[] = {
{ {(SYMS_U8*)"off", 3}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"seg", 3}, &_syms_serial_type_SYMS_U16, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"flags", 5}, &_syms_serial_type_SYMS_CvLocalFlags, SYMS_SerialWidthKind_Null, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_CvFramerel[] = {
{ {(SYMS_U8*)"off", 3}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"itype", 5}, &_syms_serial_type_SYMS_CvTypeId, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"attr", 4}, &_syms_serial_type_SYMS_CvLocalVarAttr, SYMS_SerialWidthKind_Null, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_CvAttrReg[] = {
{ {(SYMS_U8*)"itype", 5}, &_syms_serial_type_SYMS_CvTypeId, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"attr", 4}, &_syms_serial_type_SYMS_CvLocalVarAttr, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"reg", 3}, &_syms_serial_type_SYMS_U16, SYMS_SerialWidthKind_Null, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_CvAttrRegrel[] = {
{ {(SYMS_U8*)"off", 3}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"itype", 5}, &_syms_serial_type_SYMS_CvTypeId, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"reg", 3}, &_syms_serial_type_SYMS_U16, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"attr", 4}, &_syms_serial_type_SYMS_CvLocalVarAttr, SYMS_SerialWidthKind_Null, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_CvAttrManyreg[] = {
{ {(SYMS_U8*)"itype", 5}, &_syms_serial_type_SYMS_CvTypeId, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"attr", 4}, &_syms_serial_type_SYMS_CvLocalVarAttr, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"reg_count", 9}, &_syms_serial_type_SYMS_U8, SYMS_SerialWidthKind_Null, 0 },
};
SYMS_SerialFlag _syms_serial_members_for_SYMS_CvFastLinkFlags[] = {
{ { (SYMS_U8*)"IS_GLOBAL_DATA", 14 }, &_syms_serial_type_SYMS_U16, 0x1, 0 },
{ { (SYMS_U8*)"IS_DATA", 7 }, &_syms_serial_type_SYMS_U16, 0x1, 1 },
{ { (SYMS_U8*)"IS_UDT", 6 }, &_syms_serial_type_SYMS_U16, 0x1, 2 },
{ { (SYMS_U8*)"UNUSED_1", 8 }, &_syms_serial_type_SYMS_U16, 0x1, 3 },
{ { (SYMS_U8*)"IS_CONST", 8 }, &_syms_serial_type_SYMS_U16, 0x1, 4 },
{ { (SYMS_U8*)"UNUSED_2", 8 }, &_syms_serial_type_SYMS_U16, 0x1, 5 },
{ { (SYMS_U8*)"IS_NAMESPACE", 12 }, &_syms_serial_type_SYMS_U16, 0x1, 6 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_CvFastLink[] = {
{ {(SYMS_U8*)"itype", 5}, &_syms_serial_type_SYMS_CvTypeId, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"flags", 5}, &_syms_serial_type_SYMS_CvFastLinkFlags, SYMS_SerialWidthKind_Null, 0 },
};
SYMS_SerialValue _syms_serial_members_for_SYMS_CvArmSwitchType[] = {
{ { (SYMS_U8*)"INT1", 4 }, (SYMS_U64)SYMS_CvArmSwitchType_INT1 },
{ { (SYMS_U8*)"UINT1", 5 }, (SYMS_U64)SYMS_CvArmSwitchType_UINT1 },
{ { (SYMS_U8*)"INT2", 4 }, (SYMS_U64)SYMS_CvArmSwitchType_INT2 },
{ { (SYMS_U8*)"UINT2", 5 }, (SYMS_U64)SYMS_CvArmSwitchType_UINT2 },
{ { (SYMS_U8*)"INT4", 4 }, (SYMS_U64)SYMS_CvArmSwitchType_INT4 },
{ { (SYMS_U8*)"UINT5", 5 }, (SYMS_U64)SYMS_CvArmSwitchType_UINT5 },
{ { (SYMS_U8*)"POINTER", 7 }, (SYMS_U64)SYMS_CvArmSwitchType_POINTER },
{ { (SYMS_U8*)"UINT1SHL1", 9 }, (SYMS_U64)SYMS_CvArmSwitchType_UINT1SHL1 },
{ { (SYMS_U8*)"UINT2SHL1", 9 }, (SYMS_U64)SYMS_CvArmSwitchType_UINT2SHL1 },
{ { (SYMS_U8*)"INT1SSHL1", 9 }, (SYMS_U64)SYMS_CvArmSwitchType_INT1SSHL1 },
{ { (SYMS_U8*)"INT2SSHL1", 9 }, (SYMS_U64)SYMS_CvArmSwitchType_INT2SSHL1 },
{ { (SYMS_U8*)"TBB", 3 }, (SYMS_U64)SYMS_CvArmSwitchType_TBB },
{ { (SYMS_U8*)"TBH", 3 }, (SYMS_U64)SYMS_CvArmSwitchType_TBH },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_CvArmSwitchTable[] = {
{ {(SYMS_U8*)"off_base", 8}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"sec_base", 8}, &_syms_serial_type_SYMS_U16, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"switch_type", 11}, &_syms_serial_type_SYMS_CvArmSwitchType, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"off_branch", 10}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"off_table", 9}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"sec_branch", 10}, &_syms_serial_type_SYMS_U16, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"sec_table", 9}, &_syms_serial_type_SYMS_U16, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"entry_count", 11}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
};
SYMS_SerialFlag _syms_serial_members_for_SYMS_CvRefMiniPdbFlags[] = {
{ { (SYMS_U8*)"LOCAL", 5 }, &_syms_serial_type_SYMS_U16, 0x1, 0 },
{ { (SYMS_U8*)"DATA", 4 }, &_syms_serial_type_SYMS_U16, 0x1, 1 },
{ { (SYMS_U8*)"UDT", 3 }, &_syms_serial_type_SYMS_U16, 0x1, 2 },
{ { (SYMS_U8*)"LABEL", 5 }, &_syms_serial_type_SYMS_U16, 0x1, 3 },
{ { (SYMS_U8*)"CONST", 5 }, &_syms_serial_type_SYMS_U16, 0x1, 4 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_CvRefMiniPdb[] = {
{ {(SYMS_U8*)"data", 4}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"imod", 4}, &_syms_serial_type_SYMS_U16, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"flags", 5}, &_syms_serial_type_SYMS_CvRefMiniPdbFlags, SYMS_SerialWidthKind_Null, 0 },
};
SYMS_SerialFlag _syms_serial_members_for_SYMS_CvModTypeRefFlags[] = {
{ { (SYMS_U8*)"NONE", 4 }, &_syms_serial_type_SYMS_U32, 0x1, 0 },
{ { (SYMS_U8*)"REF_TMPCT", 9 }, &_syms_serial_type_SYMS_U32, 0x1, 1 },
{ { (SYMS_U8*)"OWN_TMPCT", 9 }, &_syms_serial_type_SYMS_U32, 0x1, 2 },
{ { (SYMS_U8*)"OWN_TMR", 7 }, &_syms_serial_type_SYMS_U32, 0x1, 3 },
{ { (SYMS_U8*)"OWN_TM", 6 }, &_syms_serial_type_SYMS_U32, 0x1, 4 },
{ { (SYMS_U8*)"REF_TM", 6 }, &_syms_serial_type_SYMS_U32, 0x1, 5 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_CvModTypeRef[] = {
{ {(SYMS_U8*)"flags", 5}, &_syms_serial_type_SYMS_CvModTypeRefFlags, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"word0", 5}, &_syms_serial_type_SYMS_U16, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"word1", 5}, &_syms_serial_type_SYMS_U16, SYMS_SerialWidthKind_Null, 0 },
};
SYMS_SerialValue _syms_serial_members_for_SYMS_CvDiscardedType[] = {
{ { (SYMS_U8*)"UNKNOWN", 7 }, (SYMS_U64)SYMS_CvDiscardedType_UNKNOWN },
{ { (SYMS_U8*)"NOT_SELECTED", 12 }, (SYMS_U64)SYMS_CvDiscardedType_NOT_SELECTED },
{ { (SYMS_U8*)"NOT_REFERENCED", 14 }, (SYMS_U64)SYMS_CvDiscardedType_NOT_REFERENCED },
};
SYMS_SerialFlag _syms_serial_members_for_SYMS_CvDiscardedFlags[] = {
{ { (SYMS_U8*)"TYPE", 4 }, &_syms_serial_type_SYMS_CvDiscardedType, 0xff, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_CvDiscarded[] = {
{ {(SYMS_U8*)"flags", 5}, &_syms_serial_type_SYMS_CvDiscardedFlags, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"file_id", 7}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"file_ln", 7}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
};
SYMS_SerialValue _syms_serial_members_for_SYMS_CvLeaf[] = {
{ { (SYMS_U8*)"MODIFIER_16T", 12 }, (SYMS_U64)SYMS_CvLeaf_MODIFIER_16t },
{ { (SYMS_U8*)"POINTER_16T", 11 }, (SYMS_U64)SYMS_CvLeaf_POINTER_16t },
{ { (SYMS_U8*)"ARRAY_16T", 9 }, (SYMS_U64)SYMS_CvLeaf_ARRAY_16t },
{ { (SYMS_U8*)"CLASS_16T", 9 }, (SYMS_U64)SYMS_CvLeaf_CLASS_16t },
{ { (SYMS_U8*)"STRUCTURE_16T", 13 }, (SYMS_U64)SYMS_CvLeaf_STRUCTURE_16t },
{ { (SYMS_U8*)"UNION_16T", 9 }, (SYMS_U64)SYMS_CvLeaf_UNION_16t },
{ { (SYMS_U8*)"ENUM_16T", 8 }, (SYMS_U64)SYMS_CvLeaf_ENUM_16t },
{ { (SYMS_U8*)"PROCEDURE_16T", 13 }, (SYMS_U64)SYMS_CvLeaf_PROCEDURE_16t },
{ { (SYMS_U8*)"MFUNCTION_16T", 13 }, (SYMS_U64)SYMS_CvLeaf_MFUNCTION_16t },
{ { (SYMS_U8*)"VTSHAPE", 7 }, (SYMS_U64)SYMS_CvLeaf_VTSHAPE },
{ { (SYMS_U8*)"COBOL0_16T", 10 }, (SYMS_U64)SYMS_CvLeaf_COBOL0_16t },
{ { (SYMS_U8*)"COBOL1", 6 }, (SYMS_U64)SYMS_CvLeaf_COBOL1 },
{ { (SYMS_U8*)"BARRAY_16T", 10 }, (SYMS_U64)SYMS_CvLeaf_BARRAY_16t },
{ { (SYMS_U8*)"LABEL", 5 }, (SYMS_U64)SYMS_CvLeaf_LABEL },
{ { (SYMS_U8*)"NULL", 4 }, (SYMS_U64)SYMS_CvLeaf_NULL },
{ { (SYMS_U8*)"NOTTRAN", 7 }, (SYMS_U64)SYMS_CvLeaf_NOTTRAN },
{ { (SYMS_U8*)"DIMARRAY_16T", 12 }, (SYMS_U64)SYMS_CvLeaf_DIMARRAY_16t },
{ { (SYMS_U8*)"VFTPATH_16T", 11 }, (SYMS_U64)SYMS_CvLeaf_VFTPATH_16t },
{ { (SYMS_U8*)"PRECOMP_16T", 11 }, (SYMS_U64)SYMS_CvLeaf_PRECOMP_16t },
{ { (SYMS_U8*)"ENDPRECOMP", 10 }, (SYMS_U64)SYMS_CvLeaf_ENDPRECOMP },
{ { (SYMS_U8*)"OEM_16T", 7 }, (SYMS_U64)SYMS_CvLeaf_OEM_16t },
{ { (SYMS_U8*)"TYPESERVER_ST", 13 }, (SYMS_U64)SYMS_CvLeaf_TYPESERVER_ST },
{ { (SYMS_U8*)"SKIP_16T", 8 }, (SYMS_U64)SYMS_CvLeaf_SKIP_16t },
{ { (SYMS_U8*)"ARGLIST_16T", 11 }, (SYMS_U64)SYMS_CvLeaf_ARGLIST_16t },
{ { (SYMS_U8*)"DEFARG_16T", 10 }, (SYMS_U64)SYMS_CvLeaf_DEFARG_16t },
{ { (SYMS_U8*)"LIST", 4 }, (SYMS_U64)SYMS_CvLeaf_LIST },
{ { (SYMS_U8*)"FIELDLIST_16T", 13 }, (SYMS_U64)SYMS_CvLeaf_FIELDLIST_16t },
{ { (SYMS_U8*)"DERIVED_16T", 11 }, (SYMS_U64)SYMS_CvLeaf_DERIVED_16t },
{ { (SYMS_U8*)"BITFIELD_16T", 12 }, (SYMS_U64)SYMS_CvLeaf_BITFIELD_16t },
{ { (SYMS_U8*)"METHODLIST_16T", 14 }, (SYMS_U64)SYMS_CvLeaf_METHODLIST_16t },
{ { (SYMS_U8*)"DIMCONU_16T", 11 }, (SYMS_U64)SYMS_CvLeaf_DIMCONU_16t },
{ { (SYMS_U8*)"DIMCONLU_16T", 12 }, (SYMS_U64)SYMS_CvLeaf_DIMCONLU_16t },
{ { (SYMS_U8*)"DIMVARU_16T", 11 }, (SYMS_U64)SYMS_CvLeaf_DIMVARU_16t },
{ { (SYMS_U8*)"DIMVARLU_16T", 12 }, (SYMS_U64)SYMS_CvLeaf_DIMVARLU_16t },
{ { (SYMS_U8*)"REFSYM", 6 }, (SYMS_U64)SYMS_CvLeaf_REFSYM },
{ { (SYMS_U8*)"BCLASS_16T", 10 }, (SYMS_U64)SYMS_CvLeaf_BCLASS_16t },
{ { (SYMS_U8*)"VBCLASS_16T", 11 }, (SYMS_U64)SYMS_CvLeaf_VBCLASS_16t },
{ { (SYMS_U8*)"IVBCLASS_16T", 12 }, (SYMS_U64)SYMS_CvLeaf_IVBCLASS_16t },
{ { (SYMS_U8*)"ENUMERATE_ST", 12 }, (SYMS_U64)SYMS_CvLeaf_ENUMERATE_ST },
{ { (SYMS_U8*)"FRIENDFCN_16T", 13 }, (SYMS_U64)SYMS_CvLeaf_FRIENDFCN_16t },
{ { (SYMS_U8*)"INDEX_16T", 9 }, (SYMS_U64)SYMS_CvLeaf_INDEX_16t },
{ { (SYMS_U8*)"MEMBER_16T", 10 }, (SYMS_U64)SYMS_CvLeaf_MEMBER_16t },
{ { (SYMS_U8*)"STMEMBER_16T", 12 }, (SYMS_U64)SYMS_CvLeaf_STMEMBER_16t },
{ { (SYMS_U8*)"METHOD_16T", 10 }, (SYMS_U64)SYMS_CvLeaf_METHOD_16t },
{ { (SYMS_U8*)"NESTTYPE_16T", 12 }, (SYMS_U64)SYMS_CvLeaf_NESTTYPE_16t },
{ { (SYMS_U8*)"VFUNCTAB_16T", 12 }, (SYMS_U64)SYMS_CvLeaf_VFUNCTAB_16t },
{ { (SYMS_U8*)"FRIENDCLS_16T", 13 }, (SYMS_U64)SYMS_CvLeaf_FRIENDCLS_16t },
{ { (SYMS_U8*)"ONEMETHOD_16T", 13 }, (SYMS_U64)SYMS_CvLeaf_ONEMETHOD_16t },
{ { (SYMS_U8*)"VFUNCOFF_16T", 12 }, (SYMS_U64)SYMS_CvLeaf_VFUNCOFF_16t },
{ { (SYMS_U8*)"TI16_MAX", 8 }, (SYMS_U64)SYMS_CvLeaf_TI16_MAX },
{ { (SYMS_U8*)"MODIFIER", 8 }, (SYMS_U64)SYMS_CvLeaf_MODIFIER },
{ { (SYMS_U8*)"POINTER", 7 }, (SYMS_U64)SYMS_CvLeaf_POINTER },
{ { (SYMS_U8*)"ARRAY_ST", 8 }, (SYMS_U64)SYMS_CvLeaf_ARRAY_ST },
{ { (SYMS_U8*)"CLASS_ST", 8 }, (SYMS_U64)SYMS_CvLeaf_CLASS_ST },
{ { (SYMS_U8*)"STRUCTURE_ST", 12 }, (SYMS_U64)SYMS_CvLeaf_STRUCTURE_ST },
{ { (SYMS_U8*)"UNION_ST", 8 }, (SYMS_U64)SYMS_CvLeaf_UNION_ST },
{ { (SYMS_U8*)"ENUM_ST", 7 }, (SYMS_U64)SYMS_CvLeaf_ENUM_ST },
{ { (SYMS_U8*)"PROCEDURE", 9 }, (SYMS_U64)SYMS_CvLeaf_PROCEDURE },
{ { (SYMS_U8*)"MFUNCTION", 9 }, (SYMS_U64)SYMS_CvLeaf_MFUNCTION },
{ { (SYMS_U8*)"COBOL0", 6 }, (SYMS_U64)SYMS_CvLeaf_COBOL0 },
{ { (SYMS_U8*)"BARRAY", 6 }, (SYMS_U64)SYMS_CvLeaf_BARRAY },
{ { (SYMS_U8*)"DIMARRAY_ST", 11 }, (SYMS_U64)SYMS_CvLeaf_DIMARRAY_ST },
{ { (SYMS_U8*)"VFTPATH", 7 }, (SYMS_U64)SYMS_CvLeaf_VFTPATH },
{ { (SYMS_U8*)"PRECOMP_ST", 10 }, (SYMS_U64)SYMS_CvLeaf_PRECOMP_ST },
{ { (SYMS_U8*)"OEM", 3 }, (SYMS_U64)SYMS_CvLeaf_OEM },
{ { (SYMS_U8*)"ALIAS_ST", 8 }, (SYMS_U64)SYMS_CvLeaf_ALIAS_ST },
{ { (SYMS_U8*)"OEM2", 4 }, (SYMS_U64)SYMS_CvLeaf_OEM2 },
{ { (SYMS_U8*)"SKIP", 4 }, (SYMS_U64)SYMS_CvLeaf_SKIP },
{ { (SYMS_U8*)"ARGLIST", 7 }, (SYMS_U64)SYMS_CvLeaf_ARGLIST },
{ { (SYMS_U8*)"DEFARG_ST", 9 }, (SYMS_U64)SYMS_CvLeaf_DEFARG_ST },
{ { (SYMS_U8*)"FIELDLIST", 9 }, (SYMS_U64)SYMS_CvLeaf_FIELDLIST },
{ { (SYMS_U8*)"DERIVED", 7 }, (SYMS_U64)SYMS_CvLeaf_DERIVED },
{ { (SYMS_U8*)"BITFIELD", 8 }, (SYMS_U64)SYMS_CvLeaf_BITFIELD },
{ { (SYMS_U8*)"METHODLIST", 10 }, (SYMS_U64)SYMS_CvLeaf_METHODLIST },
{ { (SYMS_U8*)"DIMCONU", 7 }, (SYMS_U64)SYMS_CvLeaf_DIMCONU },
{ { (SYMS_U8*)"DIMCONLU", 8 }, (SYMS_U64)SYMS_CvLeaf_DIMCONLU },
{ { (SYMS_U8*)"DIMVARU", 7 }, (SYMS_U64)SYMS_CvLeaf_DIMVARU },
{ { (SYMS_U8*)"DIMVARLU", 8 }, (SYMS_U64)SYMS_CvLeaf_DIMVARLU },
{ { (SYMS_U8*)"BCLASS", 6 }, (SYMS_U64)SYMS_CvLeaf_BCLASS },
{ { (SYMS_U8*)"VBCLASS", 7 }, (SYMS_U64)SYMS_CvLeaf_VBCLASS },
{ { (SYMS_U8*)"IVBCLASS", 8 }, (SYMS_U64)SYMS_CvLeaf_IVBCLASS },
{ { (SYMS_U8*)"FRIENDFCN_ST", 12 }, (SYMS_U64)SYMS_CvLeaf_FRIENDFCN_ST },
{ { (SYMS_U8*)"INDEX", 5 }, (SYMS_U64)SYMS_CvLeaf_INDEX },
{ { (SYMS_U8*)"MEMBER_ST", 9 }, (SYMS_U64)SYMS_CvLeaf_MEMBER_ST },
{ { (SYMS_U8*)"STMEMBER_ST", 11 }, (SYMS_U64)SYMS_CvLeaf_STMEMBER_ST },
{ { (SYMS_U8*)"METHOD_ST", 9 }, (SYMS_U64)SYMS_CvLeaf_METHOD_ST },
{ { (SYMS_U8*)"NESTTYPE_ST", 11 }, (SYMS_U64)SYMS_CvLeaf_NESTTYPE_ST },
{ { (SYMS_U8*)"VFUNCTAB", 8 }, (SYMS_U64)SYMS_CvLeaf_VFUNCTAB },
{ { (SYMS_U8*)"FRIENDCLS", 9 }, (SYMS_U64)SYMS_CvLeaf_FRIENDCLS },
{ { (SYMS_U8*)"ONEMETHOD_ST", 12 }, (SYMS_U64)SYMS_CvLeaf_ONEMETHOD_ST },
{ { (SYMS_U8*)"VFUNCOFF", 8 }, (SYMS_U64)SYMS_CvLeaf_VFUNCOFF },
{ { (SYMS_U8*)"NESTTYPEEX_ST", 13 }, (SYMS_U64)SYMS_CvLeaf_NESTTYPEEX_ST },
{ { (SYMS_U8*)"MEMBERMODIFY_ST", 15 }, (SYMS_U64)SYMS_CvLeaf_MEMBERMODIFY_ST },
{ { (SYMS_U8*)"MANAGED_ST", 10 }, (SYMS_U64)SYMS_CvLeaf_MANAGED_ST },
{ { (SYMS_U8*)"ST_MAX", 6 }, (SYMS_U64)SYMS_CvLeaf_ST_MAX },
{ { (SYMS_U8*)"TYPESERVER", 10 }, (SYMS_U64)SYMS_CvLeaf_TYPESERVER },
{ { (SYMS_U8*)"ENUMERATE", 9 }, (SYMS_U64)SYMS_CvLeaf_ENUMERATE },
{ { (SYMS_U8*)"ARRAY", 5 }, (SYMS_U64)SYMS_CvLeaf_ARRAY },
{ { (SYMS_U8*)"CLASS", 5 }, (SYMS_U64)SYMS_CvLeaf_CLASS },
{ { (SYMS_U8*)"STRUCTURE", 9 }, (SYMS_U64)SYMS_CvLeaf_STRUCTURE },
{ { (SYMS_U8*)"UNION", 5 }, (SYMS_U64)SYMS_CvLeaf_UNION },
{ { (SYMS_U8*)"ENUM", 4 }, (SYMS_U64)SYMS_CvLeaf_ENUM },
{ { (SYMS_U8*)"DIMARRAY", 8 }, (SYMS_U64)SYMS_CvLeaf_DIMARRAY },
{ { (SYMS_U8*)"PRECOMP", 7 }, (SYMS_U64)SYMS_CvLeaf_PRECOMP },
{ { (SYMS_U8*)"ALIAS", 5 }, (SYMS_U64)SYMS_CvLeaf_ALIAS },
{ { (SYMS_U8*)"DEFARG", 6 }, (SYMS_U64)SYMS_CvLeaf_DEFARG },
{ { (SYMS_U8*)"FRIENDFCN", 9 }, (SYMS_U64)SYMS_CvLeaf_FRIENDFCN },
{ { (SYMS_U8*)"MEMBER", 6 }, (SYMS_U64)SYMS_CvLeaf_MEMBER },
{ { (SYMS_U8*)"STMEMBER", 8 }, (SYMS_U64)SYMS_CvLeaf_STMEMBER },
{ { (SYMS_U8*)"METHOD", 6 }, (SYMS_U64)SYMS_CvLeaf_METHOD },
{ { (SYMS_U8*)"NESTTYPE", 8 }, (SYMS_U64)SYMS_CvLeaf_NESTTYPE },
{ { (SYMS_U8*)"ONEMETHOD", 9 }, (SYMS_U64)SYMS_CvLeaf_ONEMETHOD },
{ { (SYMS_U8*)"NESTTYPEEX", 10 }, (SYMS_U64)SYMS_CvLeaf_NESTTYPEEX },
{ { (SYMS_U8*)"MEMBERMODIFY", 12 }, (SYMS_U64)SYMS_CvLeaf_MEMBERMODIFY },
{ { (SYMS_U8*)"MANAGED", 7 }, (SYMS_U64)SYMS_CvLeaf_MANAGED },
{ { (SYMS_U8*)"TYPESERVER2", 11 }, (SYMS_U64)SYMS_CvLeaf_TYPESERVER2 },
{ { (SYMS_U8*)"STRIDED_ARRAY", 13 }, (SYMS_U64)SYMS_CvLeaf_STRIDED_ARRAY },
{ { (SYMS_U8*)"HLSL", 4 }, (SYMS_U64)SYMS_CvLeaf_HLSL },
{ { (SYMS_U8*)"MODIFIER_EX", 11 }, (SYMS_U64)SYMS_CvLeaf_MODIFIER_EX },
{ { (SYMS_U8*)"INTERFACE", 9 }, (SYMS_U64)SYMS_CvLeaf_INTERFACE },
{ { (SYMS_U8*)"BINTERFACE", 10 }, (SYMS_U64)SYMS_CvLeaf_BINTERFACE },
{ { (SYMS_U8*)"VECTOR", 6 }, (SYMS_U64)SYMS_CvLeaf_VECTOR },
{ { (SYMS_U8*)"MATRIX", 6 }, (SYMS_U64)SYMS_CvLeaf_MATRIX },
{ { (SYMS_U8*)"VFTABLE", 7 }, (SYMS_U64)SYMS_CvLeaf_VFTABLE },
{ { (SYMS_U8*)"ENDOFLEAFRECORD", 15 }, (SYMS_U64)SYMS_CvLeaf_ENDOFLEAFRECORD },
{ { (SYMS_U8*)"TYPE_LAST", 9 }, (SYMS_U64)SYMS_CvLeaf_TYPE_LAST },
{ { (SYMS_U8*)"TYPE_MAX", 8 }, (SYMS_U64)SYMS_CvLeaf_TYPE_MAX },
{ { (SYMS_U8*)"FUNC_ID", 7 }, (SYMS_U64)SYMS_CvLeaf_FUNC_ID },
{ { (SYMS_U8*)"MFUNC_ID", 8 }, (SYMS_U64)SYMS_CvLeaf_MFUNC_ID },
{ { (SYMS_U8*)"BUILDINFO", 9 }, (SYMS_U64)SYMS_CvLeaf_BUILDINFO },
{ { (SYMS_U8*)"SUBSTR_LIST", 11 }, (SYMS_U64)SYMS_CvLeaf_SUBSTR_LIST },
{ { (SYMS_U8*)"STRING_ID", 9 }, (SYMS_U64)SYMS_CvLeaf_STRING_ID },
{ { (SYMS_U8*)"UDT_SRC_LINE", 12 }, (SYMS_U64)SYMS_CvLeaf_UDT_SRC_LINE },
{ { (SYMS_U8*)"UDT_MOD_SRC_LINE", 16 }, (SYMS_U64)SYMS_CvLeaf_UDT_MOD_SRC_LINE },
{ { (SYMS_U8*)"CLASSPTR", 8 }, (SYMS_U64)SYMS_CvLeaf_CLASSPTR },
{ { (SYMS_U8*)"CLASSPTR2", 9 }, (SYMS_U64)SYMS_CvLeaf_CLASSPTR2 },
{ { (SYMS_U8*)"ID_LAST", 7 }, (SYMS_U64)SYMS_CvLeaf_ID_LAST },
{ { (SYMS_U8*)"ID_MAX", 6 }, (SYMS_U64)SYMS_CvLeaf_ID_MAX },
{ { (SYMS_U8*)"NUMERIC", 7 }, (SYMS_U64)SYMS_CvLeaf_NUMERIC },
{ { (SYMS_U8*)"CHAR", 4 }, (SYMS_U64)SYMS_CvLeaf_CHAR },
{ { (SYMS_U8*)"SHORT", 5 }, (SYMS_U64)SYMS_CvLeaf_SHORT },
{ { (SYMS_U8*)"USHORT", 6 }, (SYMS_U64)SYMS_CvLeaf_USHORT },
{ { (SYMS_U8*)"LONG", 4 }, (SYMS_U64)SYMS_CvLeaf_LONG },
{ { (SYMS_U8*)"ULONG", 5 }, (SYMS_U64)SYMS_CvLeaf_ULONG },
{ { (SYMS_U8*)"FLOAT32", 7 }, (SYMS_U64)SYMS_CvLeaf_FLOAT32 },
{ { (SYMS_U8*)"FLOAT64", 7 }, (SYMS_U64)SYMS_CvLeaf_FLOAT64 },
{ { (SYMS_U8*)"FLOAT80", 7 }, (SYMS_U64)SYMS_CvLeaf_FLOAT80 },
{ { (SYMS_U8*)"FLOAT128", 8 }, (SYMS_U64)SYMS_CvLeaf_FLOAT128 },
{ { (SYMS_U8*)"QUADWORD", 8 }, (SYMS_U64)SYMS_CvLeaf_QUADWORD },
{ { (SYMS_U8*)"UQUADWORD", 9 }, (SYMS_U64)SYMS_CvLeaf_UQUADWORD },
{ { (SYMS_U8*)"FLOAT48", 7 }, (SYMS_U64)SYMS_CvLeaf_FLOAT48 },
{ { (SYMS_U8*)"COMPLEX32", 9 }, (SYMS_U64)SYMS_CvLeaf_COMPLEX32 },
{ { (SYMS_U8*)"COMPLEX64", 9 }, (SYMS_U64)SYMS_CvLeaf_COMPLEX64 },
{ { (SYMS_U8*)"COMPLEX80", 9 }, (SYMS_U64)SYMS_CvLeaf_COMPLEX80 },
{ { (SYMS_U8*)"COMPLEX128", 10 }, (SYMS_U64)SYMS_CvLeaf_COMPLEX128 },
{ { (SYMS_U8*)"VARSTRING", 9 }, (SYMS_U64)SYMS_CvLeaf_VARSTRING },
{ { (SYMS_U8*)"OCTWORD", 7 }, (SYMS_U64)SYMS_CvLeaf_OCTWORD },
{ { (SYMS_U8*)"UOCTWORD", 8 }, (SYMS_U64)SYMS_CvLeaf_UOCTWORD },
{ { (SYMS_U8*)"DECIMAL", 7 }, (SYMS_U64)SYMS_CvLeaf_DECIMAL },
{ { (SYMS_U8*)"DATE", 4 }, (SYMS_U64)SYMS_CvLeaf_DATE },
{ { (SYMS_U8*)"UTF8STRING", 10 }, (SYMS_U64)SYMS_CvLeaf_UTF8STRING },
{ { (SYMS_U8*)"FLOAT16", 7 }, (SYMS_U64)SYMS_CvLeaf_FLOAT16 },
{ { (SYMS_U8*)"PAD0", 4 }, (SYMS_U64)SYMS_CvLeaf_PAD0 },
{ { (SYMS_U8*)"PAD1", 4 }, (SYMS_U64)SYMS_CvLeaf_PAD1 },
{ { (SYMS_U8*)"PAD2", 4 }, (SYMS_U64)SYMS_CvLeaf_PAD2 },
{ { (SYMS_U8*)"PAD3", 4 }, (SYMS_U64)SYMS_CvLeaf_PAD3 },
{ { (SYMS_U8*)"PAD4", 4 }, (SYMS_U64)SYMS_CvLeaf_PAD4 },
{ { (SYMS_U8*)"PAD5", 4 }, (SYMS_U64)SYMS_CvLeaf_PAD5 },
{ { (SYMS_U8*)"PAD6", 4 }, (SYMS_U64)SYMS_CvLeaf_PAD6 },
{ { (SYMS_U8*)"PAD7", 4 }, (SYMS_U64)SYMS_CvLeaf_PAD7 },
{ { (SYMS_U8*)"PAD8", 4 }, (SYMS_U64)SYMS_CvLeaf_PAD8 },
{ { (SYMS_U8*)"PAD9", 4 }, (SYMS_U64)SYMS_CvLeaf_PAD9 },
{ { (SYMS_U8*)"PAD10", 5 }, (SYMS_U64)SYMS_CvLeaf_PAD10 },
{ { (SYMS_U8*)"PAD11", 5 }, (SYMS_U64)SYMS_CvLeaf_PAD11 },
{ { (SYMS_U8*)"PAD12", 5 }, (SYMS_U64)SYMS_CvLeaf_PAD12 },
{ { (SYMS_U8*)"PAD13", 5 }, (SYMS_U64)SYMS_CvLeaf_PAD13 },
{ { (SYMS_U8*)"PAD14", 5 }, (SYMS_U64)SYMS_CvLeaf_PAD14 },
{ { (SYMS_U8*)"PAD15", 5 }, (SYMS_U64)SYMS_CvLeaf_PAD15 },
};
SYMS_SerialFlag _syms_serial_members_for_SYMS_CvModifierFlags[] = {
{ { (SYMS_U8*)"CONST", 5 }, &_syms_serial_type_SYMS_U16, 0x1, 0 },
{ { (SYMS_U8*)"VOLATILE", 8 }, &_syms_serial_type_SYMS_U16, 0x1, 1 },
{ { (SYMS_U8*)"UNALIGNED", 9 }, &_syms_serial_type_SYMS_U16, 0x1, 2 },
};
SYMS_SerialValue _syms_serial_members_for_SYMS_CvHFAKind[] = {
{ { (SYMS_U8*)"NONE", 4 }, (SYMS_U64)SYMS_CvHFAKind_NONE },
{ { (SYMS_U8*)"FLOAT", 5 }, (SYMS_U64)SYMS_CvHFAKind_FLOAT },
{ { (SYMS_U8*)"DOUBLE", 6 }, (SYMS_U64)SYMS_CvHFAKind_DOUBLE },
{ { (SYMS_U8*)"OTHER", 5 }, (SYMS_U64)SYMS_CvHFAKind_OTHER },
};
SYMS_SerialValue _syms_serial_members_for_SYMS_CvMoComUDTKind[] = {
{ { (SYMS_U8*)"NONE", 4 }, (SYMS_U64)SYMS_CvMoComUDTKind_NONE },
{ { (SYMS_U8*)"REF", 3 }, (SYMS_U64)SYMS_CvMoComUDTKind_REF },
{ { (SYMS_U8*)"VALUE", 5 }, (SYMS_U64)SYMS_CvMoComUDTKind_VALUE },
{ { (SYMS_U8*)"INTERFACE", 9 }, (SYMS_U64)SYMS_CvMoComUDTKind_INTERFACE },
};
SYMS_SerialFlag _syms_serial_members_for_SYMS_CvTypeProps[] = {
{ { (SYMS_U8*)"PACKED", 6 }, &_syms_serial_type_SYMS_U16, 0x1, 0 },
{ { (SYMS_U8*)"CTOR", 4 }, &_syms_serial_type_SYMS_U16, 0x1, 1 },
{ { (SYMS_U8*)"OVLOPS", 6 }, &_syms_serial_type_SYMS_U16, 0x1, 2 },
{ { (SYMS_U8*)"ISNSESTED", 9 }, &_syms_serial_type_SYMS_U16, 0x1, 3 },
{ { (SYMS_U8*)"CNESTED", 7 }, &_syms_serial_type_SYMS_U16, 0x1, 4 },
{ { (SYMS_U8*)"OPASSIGN", 8 }, &_syms_serial_type_SYMS_U16, 0x1, 5 },
{ { (SYMS_U8*)"OPCAST", 6 }, &_syms_serial_type_SYMS_U16, 0x1, 6 },
{ { (SYMS_U8*)"FWDREF", 6 }, &_syms_serial_type_SYMS_U16, 0x1, 7 },
{ { (SYMS_U8*)"SCOPED", 6 }, &_syms_serial_type_SYMS_U16, 0x1, 8 },
{ { (SYMS_U8*)"HAS_UNIQUE_NAME", 15 }, &_syms_serial_type_SYMS_U16, 0x1, 9 },
{ { (SYMS_U8*)"SEALED", 6 }, &_syms_serial_type_SYMS_U16, 0x1, 10 },
{ { (SYMS_U8*)"HFA", 3 }, &_syms_serial_type_SYMS_CvHFAKind, 0x3, 11 },
{ { (SYMS_U8*)"INTRINSIC", 9 }, &_syms_serial_type_SYMS_U16, 0x1, 13 },
{ { (SYMS_U8*)"MOCOM", 5 }, &_syms_serial_type_SYMS_CvMoComUDTKind, 0x3, 14 },
};
SYMS_SerialValue _syms_serial_members_for_SYMS_CvPointerKind[] = {
{ { (SYMS_U8*)"NEAR", 4 }, (SYMS_U64)SYMS_CvPointerKind_NEAR },
{ { (SYMS_U8*)"FAR", 3 }, (SYMS_U64)SYMS_CvPointerKind_FAR },
{ { (SYMS_U8*)"HUGE", 4 }, (SYMS_U64)SYMS_CvPointerKind_HUGE },
{ { (SYMS_U8*)"BASE_SEG", 8 }, (SYMS_U64)SYMS_CvPointerKind_BASE_SEG },
{ { (SYMS_U8*)"BASE_VAL", 8 }, (SYMS_U64)SYMS_CvPointerKind_BASE_VAL },
{ { (SYMS_U8*)"BASE_SEGVAL", 11 }, (SYMS_U64)SYMS_CvPointerKind_BASE_SEGVAL },
{ { (SYMS_U8*)"BASE_ADDR", 9 }, (SYMS_U64)SYMS_CvPointerKind_BASE_ADDR },
{ { (SYMS_U8*)"BASE_SEGADDR", 12 }, (SYMS_U64)SYMS_CvPointerKind_BASE_SEGADDR },
{ { (SYMS_U8*)"BASE_TYPE", 9 }, (SYMS_U64)SYMS_CvPointerKind_BASE_TYPE },
{ { (SYMS_U8*)"BASE_SELF", 9 }, (SYMS_U64)SYMS_CvPointerKind_BASE_SELF },
{ { (SYMS_U8*)"NEAR32", 6 }, (SYMS_U64)SYMS_CvPointerKind_NEAR32 },
{ { (SYMS_U8*)"FAR32", 5 }, (SYMS_U64)SYMS_CvPointerKind_FAR32 },
{ { (SYMS_U8*)"64", 2 }, (SYMS_U64)SYMS_CvPointerKind_64 },
{ { (SYMS_U8*)"UNUSEDPTR", 9 }, (SYMS_U64)SYMS_CvPointerKind_UNUSEDPTR },
};
SYMS_SerialValue _syms_serial_members_for_SYMS_CvPointerMode[] = {
{ { (SYMS_U8*)"PTR", 3 }, (SYMS_U64)SYMS_CvPointerMode_PTR },
{ { (SYMS_U8*)"REF", 3 }, (SYMS_U64)SYMS_CvPointerMode_REF },
{ { (SYMS_U8*)"LVREF", 5 }, (SYMS_U64)SYMS_CvPointerMode_LVREF },
{ { (SYMS_U8*)"PMEM", 4 }, (SYMS_U64)SYMS_CvPointerMode_PMEM },
{ { (SYMS_U8*)"PMFUNC", 6 }, (SYMS_U64)SYMS_CvPointerMode_PMFUNC },
{ { (SYMS_U8*)"RVREF", 5 }, (SYMS_U64)SYMS_CvPointerMode_RVREF },
{ { (SYMS_U8*)"RESERVED", 8 }, (SYMS_U64)SYMS_CvPointerMode_RESERVED },
};
SYMS_SerialFlag _syms_serial_members_for_SYMS_CvPointerAttribs[] = {
{ { (SYMS_U8*)"KIND", 4 }, &_syms_serial_type_SYMS_CvPointerKind, 0x1f, 0 },
{ { (SYMS_U8*)"MODE", 4 }, &_syms_serial_type_SYMS_CvPointerMode, 0x7, 5 },
{ { (SYMS_U8*)"IS_FLAT", 7 }, &_syms_serial_type_SYMS_U32, 0x1, 8 },
{ { (SYMS_U8*)"IS_VOLATILE", 11 }, &_syms_serial_type_SYMS_U32, 0x1, 9 },
{ { (SYMS_U8*)"IS_CONST", 8 }, &_syms_serial_type_SYMS_U32, 0x1, 10 },
{ { (SYMS_U8*)"IS_UNALIGNED", 12 }, &_syms_serial_type_SYMS_U32, 0x1, 11 },
{ { (SYMS_U8*)"IS_RESTRICTED", 13 }, &_syms_serial_type_SYMS_U32, 0x1, 12 },
{ { (SYMS_U8*)"SIZE", 4 }, &_syms_serial_type_SYMS_U32, 0x3f, 13 },
{ { (SYMS_U8*)"IS_MOCOM", 8 }, &_syms_serial_type_SYMS_U32, 0x1, 19 },
{ { (SYMS_U8*)"IS_LREF", 7 }, &_syms_serial_type_SYMS_U32, 0x1, 20 },
{ { (SYMS_U8*)"IS_RREF", 7 }, &_syms_serial_type_SYMS_U32, 0x1, 21 },
};
SYMS_SerialValue _syms_serial_members_for_SYMS_CvMemberPointerKind[] = {
{ { (SYMS_U8*)"Undef", 5 }, (SYMS_U64)SYMS_CvMemberPointerKind_Undef },
{ { (SYMS_U8*)"D_Single", 8 }, (SYMS_U64)SYMS_CvMemberPointerKind_D_Single },
{ { (SYMS_U8*)"D_Multiple", 10 }, (SYMS_U64)SYMS_CvMemberPointerKind_D_Multiple },
{ { (SYMS_U8*)"D_Virtual", 9 }, (SYMS_U64)SYMS_CvMemberPointerKind_D_Virtual },
{ { (SYMS_U8*)"D_General", 9 }, (SYMS_U64)SYMS_CvMemberPointerKind_D_General },
{ { (SYMS_U8*)"F_Single", 8 }, (SYMS_U64)SYMS_CvMemberPointerKind_F_Single },
{ { (SYMS_U8*)"F_Multiple", 10 }, (SYMS_U64)SYMS_CvMemberPointerKind_F_Multiple },
{ { (SYMS_U8*)"F_Virtual", 9 }, (SYMS_U64)SYMS_CvMemberPointerKind_F_Virtual },
{ { (SYMS_U8*)"F_General", 9 }, (SYMS_U64)SYMS_CvMemberPointerKind_F_General },
};
SYMS_SerialValue _syms_serial_members_for_SYMS_CvVirtualTableShape[] = {
{ { (SYMS_U8*)"NEAR", 4 }, (SYMS_U64)SYMS_CvVirtualTableShape_NEAR },
{ { (SYMS_U8*)"FAR", 3 }, (SYMS_U64)SYMS_CvVirtualTableShape_FAR },
{ { (SYMS_U8*)"THIN", 4 }, (SYMS_U64)SYMS_CvVirtualTableShape_THIN },
{ { (SYMS_U8*)"OUTER", 5 }, (SYMS_U64)SYMS_CvVirtualTableShape_OUTER },
{ { (SYMS_U8*)"META", 4 }, (SYMS_U64)SYMS_CvVirtualTableShape_META },
{ { (SYMS_U8*)"NEAR32", 6 }, (SYMS_U64)SYMS_CvVirtualTableShape_NEAR32 },
{ { (SYMS_U8*)"FAR32", 5 }, (SYMS_U64)SYMS_CvVirtualTableShape_FAR32 },
{ { (SYMS_U8*)"UNUSED", 6 }, (SYMS_U64)SYMS_CvVirtualTableShape_UNUSED },
};
SYMS_SerialValue _syms_serial_members_for_SYMS_CvMethodProp[] = {
{ { (SYMS_U8*)"VANILLA", 7 }, (SYMS_U64)SYMS_CvMethodProp_VANILLA },
{ { (SYMS_U8*)"VIRTUAL", 7 }, (SYMS_U64)SYMS_CvMethodProp_VIRTUAL },
{ { (SYMS_U8*)"STATIC", 6 }, (SYMS_U64)SYMS_CvMethodProp_STATIC },
{ { (SYMS_U8*)"FRIEND", 6 }, (SYMS_U64)SYMS_CvMethodProp_FRIEND },
{ { (SYMS_U8*)"INTRO", 5 }, (SYMS_U64)SYMS_CvMethodProp_INTRO },
{ { (SYMS_U8*)"PUREVIRT", 8 }, (SYMS_U64)SYMS_CvMethodProp_PUREVIRT },
{ { (SYMS_U8*)"PUREINTRO", 9 }, (SYMS_U64)SYMS_CvMethodProp_PUREINTRO },
};
SYMS_SerialValue _syms_serial_members_for_SYMS_CvMemberAccess[] = {
{ { (SYMS_U8*)"NULL", 4 }, (SYMS_U64)SYMS_CvMemberAccess_NULL },
{ { (SYMS_U8*)"PRIVATE", 7 }, (SYMS_U64)SYMS_CvMemberAccess_PRIVATE },
{ { (SYMS_U8*)"PROTECTED", 9 }, (SYMS_U64)SYMS_CvMemberAccess_PROTECTED },
{ { (SYMS_U8*)"PUBLIC", 6 }, (SYMS_U64)SYMS_CvMemberAccess_PUBLIC },
};
SYMS_SerialFlag _syms_serial_members_for_SYMS_CvFieldAttribs[] = {
{ { (SYMS_U8*)"ACCESS", 6 }, &_syms_serial_type_SYMS_CvMemberAccess, 0x3, 0 },
{ { (SYMS_U8*)"MPROP", 5 }, &_syms_serial_type_SYMS_CvMethodProp, 0x7, 2 },
{ { (SYMS_U8*)"PSEUDO", 6 }, &_syms_serial_type_SYMS_U16, 0x1, 5 },
{ { (SYMS_U8*)"NOINHERIT", 9 }, &_syms_serial_type_SYMS_U16, 0x1, 6 },
{ { (SYMS_U8*)"NOCONSTRUCT", 11 }, &_syms_serial_type_SYMS_U16, 0x1, 7 },
{ { (SYMS_U8*)"COMPGENX", 8 }, &_syms_serial_type_SYMS_U16, 0x1, 8 },
{ { (SYMS_U8*)"SEALED", 6 }, &_syms_serial_type_SYMS_U16, 0x1, 9 },
};
SYMS_SerialValue _syms_serial_members_for_SYMS_CvLabelKind[] = {
{ { (SYMS_U8*)"NEAR", 4 }, (SYMS_U64)SYMS_CvLabelKind_NEAR },
{ { (SYMS_U8*)"FAR", 3 }, (SYMS_U64)SYMS_CvLabelKind_FAR },
};
SYMS_SerialFlag _syms_serial_members_for_SYMS_CvFunctionAttribs[] = {
{ { (SYMS_U8*)"CXXRETURNUDT", 12 }, &_syms_serial_type_SYMS_U8, 0x1, 0 },
{ { (SYMS_U8*)"CTOR", 4 }, &_syms_serial_type_SYMS_U8, 0x1, 1 },
{ { (SYMS_U8*)"CTORVBASE", 9 }, &_syms_serial_type_SYMS_U8, 0x1, 2 },
};
SYMS_SerialValue _syms_serial_members_for_SYMS_CvCallKind[] = {
{ { (SYMS_U8*)"Near C", 6 }, (SYMS_U64)SYMS_CvCallKind_NEAR_C },
{ { (SYMS_U8*)"Far C", 5 }, (SYMS_U64)SYMS_CvCallKind_FAR_C },
{ { (SYMS_U8*)"Near Pascal", 11 }, (SYMS_U64)SYMS_CvCallKind_NEAR_PASCAL },
{ { (SYMS_U8*)"Far Pascal", 10 }, (SYMS_U64)SYMS_CvCallKind_FAR_PASCAL },
{ { (SYMS_U8*)"Near Fast", 9 }, (SYMS_U64)SYMS_CvCallKind_NEAR_FAST },
{ { (SYMS_U8*)"Fat Fast", 8 }, (SYMS_U64)SYMS_CvCallKind_FAR_FAST },
{ { (SYMS_U8*)"SKIPPED", 7 }, (SYMS_U64)SYMS_CvCallKind_SKIPPED },
{ { (SYMS_U8*)"Near Standard", 13 }, (SYMS_U64)SYMS_CvCallKind_NEAR_STD },
{ { (SYMS_U8*)"Far Standard", 12 }, (SYMS_U64)SYMS_CvCallKind_FAR_STD },
{ { (SYMS_U8*)"Near System", 11 }, (SYMS_U64)SYMS_CvCallKind_NEAR_SYS },
{ { (SYMS_U8*)"Far System", 10 }, (SYMS_U64)SYMS_CvCallKind_FAR_SYS },
{ { (SYMS_U8*)"This", 4 }, (SYMS_U64)SYMS_CvCallKind_THISCALL },
{ { (SYMS_U8*)"MIPS", 4 }, (SYMS_U64)SYMS_CvCallKind_MIPSCALL },
{ { (SYMS_U8*)"Generic", 7 }, (SYMS_U64)SYMS_CvCallKind_GENERIC },
{ { (SYMS_U8*)"Alpha", 5 }, (SYMS_U64)SYMS_CvCallKind_ALPHACALL },
{ { (SYMS_U8*)"PowerPC", 7 }, (SYMS_U64)SYMS_CvCallKind_PPCCALL },
{ { (SYMS_U8*)"HitachiSuperH", 13 }, (SYMS_U64)SYMS_CvCallKind_SHCALL },
{ { (SYMS_U8*)"ARM", 3 }, (SYMS_U64)SYMS_CvCallKind_ARMCALL },
{ { (SYMS_U8*)"ARM33", 5 }, (SYMS_U64)SYMS_CvCallKind_AM33CALL },
{ { (SYMS_U8*)"TriCore", 7 }, (SYMS_U64)SYMS_CvCallKind_TRICALL },
{ { (SYMS_U8*)"HitachiSuperH-5", 15 }, (SYMS_U64)SYMS_CvCallKind_SH5CALL },
{ { (SYMS_U8*)"M32R", 4 }, (SYMS_U64)SYMS_CvCallKind_M32RCALL },
{ { (SYMS_U8*)"CLR", 3 }, (SYMS_U64)SYMS_CvCallKind_CLRCALL },
{ { (SYMS_U8*)"Inline", 6 }, (SYMS_U64)SYMS_CvCallKind_INLINE },
{ { (SYMS_U8*)"Near Vector", 11 }, (SYMS_U64)SYMS_CvCallKind_NEAR_VECTOR },
{ { (SYMS_U8*)"RESERVED", 8 }, (SYMS_U64)SYMS_CvCallKind_RESERVED },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_CvLeafPreComp[] = {
{ {(SYMS_U8*)"start_index", 11}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"count", 5}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"signature", 9}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"name", 4}, &_syms_serial_type_SYMS_U8, SYMS_SerialWidthKind_NullTerminated, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_CvLeafTypeServer[] = {
{ {(SYMS_U8*)"sig", 3}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"age", 3}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"name", 4}, &_syms_serial_type_SYMS_U8, SYMS_SerialWidthKind_NullTerminated, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_CvLeafTypeServer2[] = {
{ {(SYMS_U8*)"sig70", 5}, &_syms_serial_type_SYMS_CvGuid, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"age", 3}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"name", 4}, &_syms_serial_type_SYMS_U8, SYMS_SerialWidthKind_NullTerminated, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_CvLeafBuildInfo[] = {
{ {(SYMS_U8*)"count", 5}, &_syms_serial_type_SYMS_U16, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"arg", 3}, &_syms_serial_type_SYMS_CvItemId, SYMS_SerialWidthKind_Array, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_CvLeafSkip_16t[] = {
{ {(SYMS_U8*)"type", 4}, &_syms_serial_type_SYMS_U16, SYMS_SerialWidthKind_Null, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_CvLeafSkip[] = {
{ {(SYMS_U8*)"type", 4}, &_syms_serial_type_SYMS_CvTypeIndex, SYMS_SerialWidthKind_Null, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_CvLeafVTShape[] = {
{ {(SYMS_U8*)"count", 5}, &_syms_serial_type_SYMS_U16, SYMS_SerialWidthKind_Null, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_CvLeafLabel[] = {
{ {(SYMS_U8*)"mode", 4}, &_syms_serial_type_SYMS_CvLabelKind, SYMS_SerialWidthKind_Null, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_CvLeafModifier[] = {
{ {(SYMS_U8*)"itype", 5}, &_syms_serial_type_SYMS_CvTypeId, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"flags", 5}, &_syms_serial_type_SYMS_CvModifierFlags, SYMS_SerialWidthKind_Null, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_CvLeafPointer[] = {
{ {(SYMS_U8*)"itype", 5}, &_syms_serial_type_SYMS_CvTypeId, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"attr", 4}, &_syms_serial_type_SYMS_CvPointerAttribs, SYMS_SerialWidthKind_Null, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_CvLeafProcedure[] = {
{ {(SYMS_U8*)"ret_itype", 9}, &_syms_serial_type_SYMS_CvTypeId, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"call_kind", 9}, &_syms_serial_type_SYMS_CvCallKind, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"funcattr", 8}, &_syms_serial_type_SYMS_CvFunctionAttribs, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"arg_count", 9}, &_syms_serial_type_SYMS_U16, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"arg_itype", 9}, &_syms_serial_type_SYMS_CvTypeId, SYMS_SerialWidthKind_Null, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_CvLeafMFunction[] = {
{ {(SYMS_U8*)"ret_itype", 9}, &_syms_serial_type_SYMS_CvTypeId, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"class_itype", 11}, &_syms_serial_type_SYMS_CvTypeId, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"this_itype", 10}, &_syms_serial_type_SYMS_CvTypeId, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"call_kind", 9}, &_syms_serial_type_SYMS_CvCallKind, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"funcattr", 8}, &_syms_serial_type_SYMS_CvFunctionAttribs, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"arg_count", 9}, &_syms_serial_type_SYMS_U16, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"arg_itype", 9}, &_syms_serial_type_SYMS_CvTypeId, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"thisadjust", 10}, &_syms_serial_type_SYMS_S32, SYMS_SerialWidthKind_Null, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_CvLeafArgList[] = {
{ {(SYMS_U8*)"count", 5}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_CvLeafBitField[] = {
{ {(SYMS_U8*)"itype", 5}, &_syms_serial_type_SYMS_CvTypeId, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"len", 3}, &_syms_serial_type_SYMS_U8, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"pos", 3}, &_syms_serial_type_SYMS_U8, SYMS_SerialWidthKind_Null, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_CvLeafIndex[] = {
{ {(SYMS_U8*)"pad", 3}, &_syms_serial_type_SYMS_U16, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"itype", 5}, &_syms_serial_type_SYMS_CvTypeId, SYMS_SerialWidthKind_Null, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_CvLeafArray[] = {
{ {(SYMS_U8*)"entry_itype", 11}, &_syms_serial_type_SYMS_CvTypeId, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"index_itype", 11}, &_syms_serial_type_SYMS_CvTypeId, SYMS_SerialWidthKind_Null, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_CvLeafStruct[] = {
{ {(SYMS_U8*)"count", 5}, &_syms_serial_type_SYMS_U16, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"props", 5}, &_syms_serial_type_SYMS_CvTypeProps, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"field", 5}, &_syms_serial_type_SYMS_CvTypeId, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"derived", 7}, &_syms_serial_type_SYMS_CvTypeId, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"vshape", 6}, &_syms_serial_type_SYMS_CvTypeId, SYMS_SerialWidthKind_Null, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_CvLeafUnion[] = {
{ {(SYMS_U8*)"count", 5}, &_syms_serial_type_SYMS_U16, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"props", 5}, &_syms_serial_type_SYMS_CvTypeProps, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"field", 5}, &_syms_serial_type_SYMS_CvTypeId, SYMS_SerialWidthKind_Null, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_CvLeafEnum[] = {
{ {(SYMS_U8*)"count", 5}, &_syms_serial_type_SYMS_U16, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"props", 5}, &_syms_serial_type_SYMS_CvTypeProps, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"itype", 5}, &_syms_serial_type_SYMS_CvTypeId, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"field", 5}, &_syms_serial_type_SYMS_CvTypeId, SYMS_SerialWidthKind_Null, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_CvLeafAlias[] = {
{ {(SYMS_U8*)"itype", 5}, &_syms_serial_type_SYMS_CvTypeId, SYMS_SerialWidthKind_Null, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_CvLeafMember[] = {
{ {(SYMS_U8*)"attribs", 7}, &_syms_serial_type_SYMS_CvFieldAttribs, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"itype", 5}, &_syms_serial_type_SYMS_CvTypeId, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"offset", 6}, &_syms_serial_type_SYMS_PdbNumeric, SYMS_SerialWidthKind_PdbNumeric, 0 },
{ {(SYMS_U8*)"name", 4}, &_syms_serial_type_SYMS_U8, SYMS_SerialWidthKind_NullTerminated, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_CvLeafStMember[] = {
{ {(SYMS_U8*)"attribs", 7}, &_syms_serial_type_SYMS_CvFieldAttribs, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"itype", 5}, &_syms_serial_type_SYMS_CvTypeId, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"name", 4}, &_syms_serial_type_SYMS_U8, SYMS_SerialWidthKind_NullTerminated, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_CvLeafMethodListMember[] = {
{ {(SYMS_U8*)"attribs", 7}, &_syms_serial_type_SYMS_CvFieldAttribs, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"pad", 3}, &_syms_serial_type_SYMS_U16, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"index", 5}, &_syms_serial_type_SYMS_CvTypeId, SYMS_SerialWidthKind_Null, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_CvLeafMethod[] = {
{ {(SYMS_U8*)"count", 5}, &_syms_serial_type_SYMS_U16, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"itype_list", 10}, &_syms_serial_type_SYMS_CvTypeId, SYMS_SerialWidthKind_Null, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_CvLeafOneMethod[] = {
{ {(SYMS_U8*)"attribs", 7}, &_syms_serial_type_SYMS_CvFieldAttribs, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"itype", 5}, &_syms_serial_type_SYMS_CvTypeId, SYMS_SerialWidthKind_Null, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_CvLeafEnumerate[] = {
{ {(SYMS_U8*)"attribs", 7}, &_syms_serial_type_SYMS_CvFieldAttribs, SYMS_SerialWidthKind_Null, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_CvLeafNestType[] = {
{ {(SYMS_U8*)"pad", 3}, &_syms_serial_type_SYMS_U16, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"index", 5}, &_syms_serial_type_SYMS_CvTypeId, SYMS_SerialWidthKind_Null, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_CvLeafNestTypeEx[] = {
{ {(SYMS_U8*)"attribs", 7}, &_syms_serial_type_SYMS_CvFieldAttribs, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"itype", 5}, &_syms_serial_type_SYMS_CvTypeId, SYMS_SerialWidthKind_Null, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_CvLeafBClass[] = {
{ {(SYMS_U8*)"attribs", 7}, &_syms_serial_type_SYMS_CvFieldAttribs, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"itype", 5}, &_syms_serial_type_SYMS_CvTypeId, SYMS_SerialWidthKind_Null, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_CvLeafVBClass[] = {
{ {(SYMS_U8*)"attribs", 7}, &_syms_serial_type_SYMS_CvFieldAttribs, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"itype", 5}, &_syms_serial_type_SYMS_CvTypeId, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"vbptr_itype", 11}, &_syms_serial_type_SYMS_CvTypeId, SYMS_SerialWidthKind_Null, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_CvLeafVFuncTab[] = {
{ {(SYMS_U8*)"pad", 3}, &_syms_serial_type_SYMS_U16, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"itype", 5}, &_syms_serial_type_SYMS_CvTypeId, SYMS_SerialWidthKind_Null, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_CvLeafVFuncOff[] = {
{ {(SYMS_U8*)"pad", 3}, &_syms_serial_type_SYMS_U16, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"itype", 5}, &_syms_serial_type_SYMS_CvTypeId, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"off", 3}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_CvLeafVFTable[] = {
{ {(SYMS_U8*)"owner_itype", 11}, &_syms_serial_type_SYMS_CvTypeId, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"base_table_itype", 16}, &_syms_serial_type_SYMS_CvTypeId, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"offset_in_object_layout", 23}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"names_len", 9}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_CvLeafVFPath[] = {
{ {(SYMS_U8*)"count", 5}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_CvLeafFuncId[] = {
{ {(SYMS_U8*)"scope_id", 8}, &_syms_serial_type_SYMS_CvItemId, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"itype", 5}, &_syms_serial_type_SYMS_CvTypeId, SYMS_SerialWidthKind_Null, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_CvLeafMFuncId[] = {
{ {(SYMS_U8*)"parent_itype", 12}, &_syms_serial_type_SYMS_CvItemId, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"itype", 5}, &_syms_serial_type_SYMS_CvItemId, SYMS_SerialWidthKind_Null, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_CvLeafStringId[] = {
{ {(SYMS_U8*)"id", 2}, &_syms_serial_type_SYMS_CvItemId, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"name", 4}, &_syms_serial_type_SYMS_U8, SYMS_SerialWidthKind_NullTerminated, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_CvLeafUDTSrcLine[] = {
{ {(SYMS_U8*)"udt_itype", 9}, &_syms_serial_type_SYMS_CvTypeId, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"src", 3}, &_syms_serial_type_SYMS_CvItemId, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"ln", 2}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_CvLeafModSrcLine[] = {
{ {(SYMS_U8*)"udt_itype", 9}, &_syms_serial_type_SYMS_CvTypeId, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"src", 3}, &_syms_serial_type_SYMS_CvItemId, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"ln", 2}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"comp_unit", 9}, &_syms_serial_type_SYMS_U16, SYMS_SerialWidthKind_Null, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_CvLeafClassPtr[] = {
{ {(SYMS_U8*)"props", 5}, &_syms_serial_type_SYMS_CvTypeProps, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"padding", 7}, &_syms_serial_type_SYMS_U16, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"field", 5}, &_syms_serial_type_SYMS_CvTypeId, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"unknown2", 8}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"unknown3", 8}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"unknown4", 8}, &_syms_serial_type_SYMS_U16, SYMS_SerialWidthKind_Null, 0 },
};
SYMS_SerialValue _syms_serial_members_for_SYMS_CvSubSectionKind[] = {
{ { (SYMS_U8*)"SYMBOLS", 7 }, (SYMS_U64)SYMS_CvSubSectionKind_SYMBOLS },
{ { (SYMS_U8*)"LINES", 5 }, (SYMS_U64)SYMS_CvSubSectionKind_LINES },
{ { (SYMS_U8*)"STRINGTABLE", 11 }, (SYMS_U64)SYMS_CvSubSectionKind_STRINGTABLE },
{ { (SYMS_U8*)"FILECHKSMS", 10 }, (SYMS_U64)SYMS_CvSubSectionKind_FILECHKSMS },
{ { (SYMS_U8*)"FRAMEDATA", 9 }, (SYMS_U64)SYMS_CvSubSectionKind_FRAMEDATA },
{ { (SYMS_U8*)"INLINEELINES", 12 }, (SYMS_U64)SYMS_CvSubSectionKind_INLINEELINES },
{ { (SYMS_U8*)"CROSSSCOPEIMPORTS", 17 }, (SYMS_U64)SYMS_CvSubSectionKind_CROSSSCOPEIMPORTS },
{ { (SYMS_U8*)"CROSSSCOPEEXPORTS", 17 }, (SYMS_U64)SYMS_CvSubSectionKind_CROSSSCOPEEXPORTS },
{ { (SYMS_U8*)"IL_LINES", 8 }, (SYMS_U64)SYMS_CvSubSectionKind_IL_LINES },
{ { (SYMS_U8*)"FUNC_MDTOKEN_MAP", 16 }, (SYMS_U64)SYMS_CvSubSectionKind_FUNC_MDTOKEN_MAP },
{ { (SYMS_U8*)"TYPE_MDTOKEN_MAP", 16 }, (SYMS_U64)SYMS_CvSubSectionKind_TYPE_MDTOKEN_MAP },
{ { (SYMS_U8*)"MERGED_ASSEMBLY_INPUT", 21 }, (SYMS_U64)SYMS_CvSubSectionKind_MERGED_ASSEMBLY_INPUT },
{ { (SYMS_U8*)"COFF_SYMBOL_RVA", 15 }, (SYMS_U64)SYMS_CvSubSectionKind_COFF_SYMBOL_RVA },
{ { (SYMS_U8*)"XFG_HASH_TYPE", 13 }, (SYMS_U64)SYMS_CvSubSectionKind_XFG_HASH_TYPE },
{ { (SYMS_U8*)"XFG_HASH_VRITUAL", 16 }, (SYMS_U64)SYMS_CvSubSectionKind_XFG_HASH_VRITUAL },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_CvSubSectionHeader[] = {
{ {(SYMS_U8*)"kind", 4}, &_syms_serial_type_SYMS_CvSubSectionKind, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"size", 4}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
};
SYMS_SerialValue _syms_serial_members_for_SYMS_CvChecksumKind[] = {
{ { (SYMS_U8*)"NULL", 4 }, (SYMS_U64)SYMS_CvChecksumKind_NULL },
{ { (SYMS_U8*)"MD5", 3 }, (SYMS_U64)SYMS_CvChecksumKind_MD5 },
{ { (SYMS_U8*)"SHA1", 4 }, (SYMS_U64)SYMS_CvChecksumKind_SHA1 },
{ { (SYMS_U8*)"SHA256", 6 }, (SYMS_U64)SYMS_CvChecksumKind_SHA256 },
};
SYMS_SerialValue _syms_serial_members_for_SYMS_CvInlineeSourceLineSig[] = {
{ { (SYMS_U8*)"REGULAR", 7 }, (SYMS_U64)SYMS_CvInlineeSourceLineSig_REGULAR },
{ { (SYMS_U8*)"EXTENDED", 8 }, (SYMS_U64)SYMS_CvInlineeSourceLineSig_EXTENDED },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_CvInlineeSourceLine[] = {
{ {(SYMS_U8*)"inlinee", 7}, &_syms_serial_type_SYMS_CvItemId, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"file_id", 7}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"base_line_number", 16}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_CvInlineeSourceLineEx[] = {
{ {(SYMS_U8*)"inlinee", 7}, &_syms_serial_type_SYMS_CvItemId, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"file_id", 7}, &_syms_serial_type_SYMS_CvItemId, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"base_line_number", 16}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"extra_file_count", 16}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"extra_file_id", 13}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Array, 3 },
};

//~ generated from code at syms/metaprogram/syms_metaprogram_serial.c:1458
SYMS_SerialType _syms_serial_type_SYMS_CvGuid = {
{(SYMS_U8*)"CvGuid", 6}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvGuid), _syms_serial_members_for_SYMS_CvGuid, sizeof(SYMS_CvGuid), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvBasicPointerKind = {
{(SYMS_U8*)"SYMS_CvBasicPointerKind", 23}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvBasicPointerKind), _syms_serial_members_for_SYMS_CvBasicPointerKind, sizeof(SYMS_CvBasicPointerKind), syms_enum_index_from_value_identity
};
SYMS_SerialType _syms_serial_type_SYMS_CvArch = {
{(SYMS_U8*)"SYMS_CvArch", 11}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvArch), _syms_serial_members_for_SYMS_CvArch, sizeof(SYMS_CvArch), syms_enum_index_from_cvarch
};
SYMS_SerialType _syms_serial_type_SYMS_CvAllReg = {
{(SYMS_U8*)"SYMS_CvAllReg", 13}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvAllReg), _syms_serial_members_for_SYMS_CvAllReg, sizeof(SYMS_CvAllReg), syms_enum_index_from_cv_all_reg
};
SYMS_SerialType _syms_serial_type_SYMS_CvRegx86 = {
{(SYMS_U8*)"SYMS_CvRegx86", 13}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvRegx86), _syms_serial_members_for_SYMS_CvRegx86, sizeof(SYMS_CvRegx86), syms_enum_index_from_cvregx86
};
SYMS_SerialType _syms_serial_type_SYMS_CvRegx64 = {
{(SYMS_U8*)"SYMS_CvRegx64", 13}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvRegx64), _syms_serial_members_for_SYMS_CvRegx64, sizeof(SYMS_CvRegx64), syms_enum_index_from_cvregx64
};
SYMS_SerialType _syms_serial_type_SYMS_CvSignature = {
{(SYMS_U8*)"SYMS_CvSignature", 16}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvSignature), _syms_serial_members_for_SYMS_CvSignature, sizeof(SYMS_CvSignature), syms_enum_index_from_cvsignature
};
SYMS_SerialType _syms_serial_type_SYMS_CvSymKind = {
{(SYMS_U8*)"SYMS_CvSymKind", 14}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvSymKind), _syms_serial_members_for_SYMS_CvSymKind, sizeof(SYMS_CvSymKind), syms_enum_index_from_cvsymkind
};
SYMS_SerialType _syms_serial_type_SYMS_CvAnnotation = {
{(SYMS_U8*)"CvAnnotation", 12}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvAnnotation), _syms_serial_members_for_SYMS_CvAnnotation, sizeof(SYMS_CvAnnotation), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvRegister = {
{(SYMS_U8*)"CvRegister", 10}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvRegister), _syms_serial_members_for_SYMS_CvRegister, sizeof(SYMS_CvRegister), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvBPRelSym32 = {
{(SYMS_U8*)"CvBPRelSym32", 12}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvBPRelSym32), _syms_serial_members_for_SYMS_CvBPRelSym32, sizeof(SYMS_CvBPRelSym32), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvVPathSym32 = {
{(SYMS_U8*)"CvVPathSym32", 12}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvVPathSym32), _syms_serial_members_for_SYMS_CvVPathSym32, sizeof(SYMS_CvVPathSym32), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvOEM = {
{(SYMS_U8*)"CvOEM", 5}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvOEM), _syms_serial_members_for_SYMS_CvOEM, sizeof(SYMS_CvOEM), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvSLink32 = {
{(SYMS_U8*)"CvSLink32", 9}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvSLink32), _syms_serial_members_for_SYMS_CvSLink32, sizeof(SYMS_CvSLink32), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvGenericStyle = {
{(SYMS_U8*)"SYMS_CvGenericStyle", 19}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvGenericStyle), _syms_serial_members_for_SYMS_CvGenericStyle, sizeof(SYMS_CvGenericStyle), syms_enum_index_from_value_identity
};
SYMS_SerialType _syms_serial_type_SYMS_CvGenericFlags = {
{(SYMS_U8*)"SYMS_CvGenericFlags", 19}, SYMS_SerialTypeKind_Flags, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvGenericFlags), _syms_serial_members_for_SYMS_CvGenericFlags, sizeof(SYMS_CvGenericFlags), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvReturn = {
{(SYMS_U8*)"CvReturn", 8}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvReturn), _syms_serial_members_for_SYMS_CvReturn, sizeof(SYMS_CvReturn), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvStartSearch = {
{(SYMS_U8*)"CvStartSearch", 13}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvStartSearch), _syms_serial_members_for_SYMS_CvStartSearch, sizeof(SYMS_CvStartSearch), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvLanguage = {
{(SYMS_U8*)"SYMS_CvLanguage", 15}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvLanguage), _syms_serial_members_for_SYMS_CvLanguage, sizeof(SYMS_CvLanguage), syms_enum_index_from_value_identity
};
SYMS_SerialType _syms_serial_type_SYMS_CvThread32 = {
{(SYMS_U8*)"CvThread32", 10}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvThread32), _syms_serial_members_for_SYMS_CvThread32, sizeof(SYMS_CvThread32), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvCompileFlags = {
{(SYMS_U8*)"SYMS_CvCompileFlags", 19}, SYMS_SerialTypeKind_Flags, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvCompileFlags), _syms_serial_members_for_SYMS_CvCompileFlags, sizeof(SYMS_CvCompileFlags), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvCompile = {
{(SYMS_U8*)"CvCompile", 9}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvCompile), _syms_serial_members_for_SYMS_CvCompile, sizeof(SYMS_CvCompile), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvEnd = {
{(SYMS_U8*)"CvEnd", 5}, SYMS_SerialTypeKind_Struct, 0, 0, 0, 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvCompile2Flags = {
{(SYMS_U8*)"SYMS_CvCompile2Flags", 20}, SYMS_SerialTypeKind_Flags, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvCompile2Flags), _syms_serial_members_for_SYMS_CvCompile2Flags, sizeof(SYMS_CvCompile2Flags), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvCompile2 = {
{(SYMS_U8*)"CvCompile2", 10}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvCompile2), _syms_serial_members_for_SYMS_CvCompile2, sizeof(SYMS_CvCompile2), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvCompile3Flags = {
{(SYMS_U8*)"SYMS_CvCompile3Flags", 20}, SYMS_SerialTypeKind_Flags, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvCompile3Flags), _syms_serial_members_for_SYMS_CvCompile3Flags, sizeof(SYMS_CvCompile3Flags), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvCompile3 = {
{(SYMS_U8*)"CvCompile3", 10}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvCompile3), _syms_serial_members_for_SYMS_CvCompile3, sizeof(SYMS_CvCompile3), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvObjname = {
{(SYMS_U8*)"CvObjname", 9}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvObjname), _syms_serial_members_for_SYMS_CvObjname, sizeof(SYMS_CvObjname), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvUNamespace = {
{(SYMS_U8*)"CvUNamespace", 12}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvUNamespace), _syms_serial_members_for_SYMS_CvUNamespace, 0, 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvRef2 = {
{(SYMS_U8*)"CvRef2", 6}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvRef2), _syms_serial_members_for_SYMS_CvRef2, sizeof(SYMS_CvRef2), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvSepcodeFlags = {
{(SYMS_U8*)"SYMS_CvSepcodeFlags", 19}, SYMS_SerialTypeKind_Flags, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvSepcodeFlags), _syms_serial_members_for_SYMS_CvSepcodeFlags, sizeof(SYMS_CvSepcodeFlags), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvSepcode = {
{(SYMS_U8*)"CvSepcode", 9}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvSepcode), _syms_serial_members_for_SYMS_CvSepcode, sizeof(SYMS_CvSepcode), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvSlotsym32 = {
{(SYMS_U8*)"CvSlotsym32", 11}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvSlotsym32), _syms_serial_members_for_SYMS_CvSlotsym32, sizeof(SYMS_CvSlotsym32), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvPogoInfo = {
{(SYMS_U8*)"CvPogoInfo", 10}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvPogoInfo), _syms_serial_members_for_SYMS_CvPogoInfo, sizeof(SYMS_CvPogoInfo), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvManyreg = {
{(SYMS_U8*)"CvManyreg", 9}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvManyreg), _syms_serial_members_for_SYMS_CvManyreg, sizeof(SYMS_CvManyreg), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvManyreg2 = {
{(SYMS_U8*)"CvManyreg2", 10}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvManyreg2), _syms_serial_members_for_SYMS_CvManyreg2, sizeof(SYMS_CvManyreg2), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvEncodedFramePtrReg = {
{(SYMS_U8*)"SYMS_CvEncodedFramePtrReg", 25}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvEncodedFramePtrReg), _syms_serial_members_for_SYMS_CvEncodedFramePtrReg, sizeof(SYMS_CvEncodedFramePtrReg), syms_enum_index_from_value_identity
};
SYMS_SerialType _syms_serial_type_SYMS_CvFrameprocFlags = {
{(SYMS_U8*)"SYMS_CvFrameprocFlags", 21}, SYMS_SerialTypeKind_Flags, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvFrameprocFlags), _syms_serial_members_for_SYMS_CvFrameprocFlags, sizeof(SYMS_CvFrameprocFlags), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvFrameproc = {
{(SYMS_U8*)"CvFrameproc", 11}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvFrameproc), _syms_serial_members_for_SYMS_CvFrameproc, sizeof(SYMS_CvFrameproc), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvThunkOrdinal = {
{(SYMS_U8*)"SYMS_CvThunkOrdinal", 19}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvThunkOrdinal), _syms_serial_members_for_SYMS_CvThunkOrdinal, sizeof(SYMS_CvThunkOrdinal), syms_enum_index_from_value_identity
};
SYMS_SerialType _syms_serial_type_SYMS_CvThunk32 = {
{(SYMS_U8*)"CvThunk32", 9}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvThunk32), _syms_serial_members_for_SYMS_CvThunk32, sizeof(SYMS_CvThunk32), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvBlock32 = {
{(SYMS_U8*)"CvBlock32", 9}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvBlock32), _syms_serial_members_for_SYMS_CvBlock32, sizeof(SYMS_CvBlock32), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvProcFlags = {
{(SYMS_U8*)"SYMS_CvProcFlags", 16}, SYMS_SerialTypeKind_Flags, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvProcFlags), _syms_serial_members_for_SYMS_CvProcFlags, sizeof(SYMS_CvProcFlags), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvLabel32 = {
{(SYMS_U8*)"CvLabel32", 9}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvLabel32), _syms_serial_members_for_SYMS_CvLabel32, sizeof(SYMS_CvLabel32), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvConstant = {
{(SYMS_U8*)"CvConstant", 10}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvConstant), _syms_serial_members_for_SYMS_CvConstant, sizeof(SYMS_CvConstant), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvUDT = {
{(SYMS_U8*)"CvUDT", 5}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvUDT), _syms_serial_members_for_SYMS_CvUDT, sizeof(SYMS_CvUDT), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvTrampolineKind = {
{(SYMS_U8*)"SYMS_CvTrampolineKind", 21}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvTrampolineKind), _syms_serial_members_for_SYMS_CvTrampolineKind, sizeof(SYMS_CvTrampolineKind), syms_enum_index_from_value_identity
};
SYMS_SerialType _syms_serial_type_SYMS_CvTrampoline = {
{(SYMS_U8*)"CvTrampoline", 12}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvTrampoline), _syms_serial_members_for_SYMS_CvTrampoline, sizeof(SYMS_CvTrampoline), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvSection = {
{(SYMS_U8*)"CvSection", 9}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvSection), _syms_serial_members_for_SYMS_CvSection, sizeof(SYMS_CvSection), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvCoffGroup = {
{(SYMS_U8*)"CvCoffGroup", 11}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvCoffGroup), _syms_serial_members_for_SYMS_CvCoffGroup, sizeof(SYMS_CvCoffGroup), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvExportFlags = {
{(SYMS_U8*)"SYMS_CvExportFlags", 18}, SYMS_SerialTypeKind_Flags, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvExportFlags), _syms_serial_members_for_SYMS_CvExportFlags, sizeof(SYMS_CvExportFlags), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvExport = {
{(SYMS_U8*)"CvExport", 8}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvExport), _syms_serial_members_for_SYMS_CvExport, sizeof(SYMS_CvExport), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvCallSiteInfo = {
{(SYMS_U8*)"CvCallSiteInfo", 14}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvCallSiteInfo), _syms_serial_members_for_SYMS_CvCallSiteInfo, sizeof(SYMS_CvCallSiteInfo), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvFrameCookieKind = {
{(SYMS_U8*)"SYMS_CvFrameCookieKind", 22}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvFrameCookieKind), _syms_serial_members_for_SYMS_CvFrameCookieKind, sizeof(SYMS_CvFrameCookieKind), syms_enum_index_from_value_identity
};
SYMS_SerialType _syms_serial_type_SYMS_CvFrameCookie = {
{(SYMS_U8*)"CvFrameCookie", 13}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvFrameCookie), _syms_serial_members_for_SYMS_CvFrameCookie, sizeof(SYMS_CvFrameCookie), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvEnvblock = {
{(SYMS_U8*)"CvEnvblock", 10}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvEnvblock), _syms_serial_members_for_SYMS_CvEnvblock, sizeof(SYMS_CvEnvblock), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvLocalFlags = {
{(SYMS_U8*)"SYMS_CvLocalFlags", 17}, SYMS_SerialTypeKind_Flags, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvLocalFlags), _syms_serial_members_for_SYMS_CvLocalFlags, sizeof(SYMS_CvLocalFlags), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvLocal = {
{(SYMS_U8*)"CvLocal", 7}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvLocal), _syms_serial_members_for_SYMS_CvLocal, sizeof(SYMS_CvLocal), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvLvarAddrRange = {
{(SYMS_U8*)"CvLvarAddrRange", 15}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvLvarAddrRange), _syms_serial_members_for_SYMS_CvLvarAddrRange, sizeof(SYMS_CvLvarAddrRange), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvLvarAddrGap = {
{(SYMS_U8*)"CvLvarAddrGap", 13}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvLvarAddrGap), _syms_serial_members_for_SYMS_CvLvarAddrGap, sizeof(SYMS_CvLvarAddrGap), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvRangeAttribs = {
{(SYMS_U8*)"SYMS_CvRangeAttribs", 19}, SYMS_SerialTypeKind_Flags, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvRangeAttribs), _syms_serial_members_for_SYMS_CvRangeAttribs, sizeof(SYMS_CvRangeAttribs), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvDefrange = {
{(SYMS_U8*)"CvDefrange", 10}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvDefrange), _syms_serial_members_for_SYMS_CvDefrange, sizeof(SYMS_CvDefrange), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvDefrangeSubfield = {
{(SYMS_U8*)"CvDefrangeSubfield", 18}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvDefrangeSubfield), _syms_serial_members_for_SYMS_CvDefrangeSubfield, sizeof(SYMS_CvDefrangeSubfield), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvDefrangeRegister = {
{(SYMS_U8*)"CvDefrangeRegister", 18}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvDefrangeRegister), _syms_serial_members_for_SYMS_CvDefrangeRegister, sizeof(SYMS_CvDefrangeRegister), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvDefrangeFramepointerRel = {
{(SYMS_U8*)"CvDefrangeFramepointerRel", 25}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvDefrangeFramepointerRel), _syms_serial_members_for_SYMS_CvDefrangeFramepointerRel, sizeof(SYMS_CvDefrangeFramepointerRel), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvDefrangeSubfieldRegister = {
{(SYMS_U8*)"CvDefrangeSubfieldRegister", 26}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvDefrangeSubfieldRegister), _syms_serial_members_for_SYMS_CvDefrangeSubfieldRegister, sizeof(SYMS_CvDefrangeSubfieldRegister), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvDefrangeFramepointerRelFullScope = {
{(SYMS_U8*)"CvDefrangeFramepointerRelFullScope", 34}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvDefrangeFramepointerRelFullScope), _syms_serial_members_for_SYMS_CvDefrangeFramepointerRelFullScope, sizeof(SYMS_CvDefrangeFramepointerRelFullScope), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvDefrangeRegisterRelFlags = {
{(SYMS_U8*)"SYMS_CvDefrangeRegisterRelFlags", 31}, SYMS_SerialTypeKind_Flags, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvDefrangeRegisterRelFlags), _syms_serial_members_for_SYMS_CvDefrangeRegisterRelFlags, sizeof(SYMS_CvDefrangeRegisterRelFlags), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvDefrangeRegisterRel = {
{(SYMS_U8*)"CvDefrangeRegisterRel", 21}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvDefrangeRegisterRel), _syms_serial_members_for_SYMS_CvDefrangeRegisterRel, sizeof(SYMS_CvDefrangeRegisterRel), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvData32 = {
{(SYMS_U8*)"CvData32", 8}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvData32), _syms_serial_members_for_SYMS_CvData32, sizeof(SYMS_CvData32), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvPubsymFlags = {
{(SYMS_U8*)"SYMS_CvPubsymFlags", 18}, SYMS_SerialTypeKind_Flags, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvPubsymFlags), _syms_serial_members_for_SYMS_CvPubsymFlags, sizeof(SYMS_CvPubsymFlags), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvPubsym32 = {
{(SYMS_U8*)"CvPubsym32", 10}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvPubsym32), _syms_serial_members_for_SYMS_CvPubsym32, sizeof(SYMS_CvPubsym32), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvGProc16 = {
{(SYMS_U8*)"CvGProc16", 9}, SYMS_SerialTypeKind_Struct, 0, 0, 0, 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvGProc3216t = {
{(SYMS_U8*)"CvGProc3216t", 12}, SYMS_SerialTypeKind_Struct, 0, 0, 0, 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvProc32 = {
{(SYMS_U8*)"CvProc32", 8}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvProc32), _syms_serial_members_for_SYMS_CvProc32, sizeof(SYMS_CvProc32), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvRegrel32 = {
{(SYMS_U8*)"CvRegrel32", 10}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvRegrel32), _syms_serial_members_for_SYMS_CvRegrel32, sizeof(SYMS_CvRegrel32), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvBuildInfo = {
{(SYMS_U8*)"CvBuildInfo", 11}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvBuildInfo), _syms_serial_members_for_SYMS_CvBuildInfo, sizeof(SYMS_CvBuildInfo), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvFunctionList = {
{(SYMS_U8*)"CvFunctionList", 14}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvFunctionList), _syms_serial_members_for_SYMS_CvFunctionList, sizeof(SYMS_CvFunctionList), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvBAOpcode = {
{(SYMS_U8*)"SYMS_CvBAOpcode", 15}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvBAOpcode), _syms_serial_members_for_SYMS_CvBAOpcode, sizeof(SYMS_CvBAOpcode), syms_enum_index_from_value_identity
};
SYMS_SerialType _syms_serial_type_SYMS_CvInlineSite = {
{(SYMS_U8*)"CvInlineSite", 12}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvInlineSite), _syms_serial_members_for_SYMS_CvInlineSite, sizeof(SYMS_CvInlineSite), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvInlineSite2 = {
{(SYMS_U8*)"CvInlineSite2", 13}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvInlineSite2), _syms_serial_members_for_SYMS_CvInlineSite2, sizeof(SYMS_CvInlineSite2), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvInlineSiteEnd = {
{(SYMS_U8*)"CvInlineSiteEnd", 15}, SYMS_SerialTypeKind_Struct, 0, 0, 0, 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvInlinees = {
{(SYMS_U8*)"CvInlinees", 10}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvInlinees), _syms_serial_members_for_SYMS_CvInlinees, sizeof(SYMS_CvInlinees), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvFileStatic = {
{(SYMS_U8*)"CvFileStatic", 12}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvFileStatic), _syms_serial_members_for_SYMS_CvFileStatic, sizeof(SYMS_CvFileStatic), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvHeapAllocSite = {
{(SYMS_U8*)"CvHeapAllocSite", 15}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvHeapAllocSite), _syms_serial_members_for_SYMS_CvHeapAllocSite, sizeof(SYMS_CvHeapAllocSite), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvLocalVarAttr = {
{(SYMS_U8*)"CvLocalVarAttr", 14}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvLocalVarAttr), _syms_serial_members_for_SYMS_CvLocalVarAttr, sizeof(SYMS_CvLocalVarAttr), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvFramerel = {
{(SYMS_U8*)"CvFramerel", 10}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvFramerel), _syms_serial_members_for_SYMS_CvFramerel, sizeof(SYMS_CvFramerel), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvAttrReg = {
{(SYMS_U8*)"CvAttrReg", 9}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvAttrReg), _syms_serial_members_for_SYMS_CvAttrReg, sizeof(SYMS_CvAttrReg), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvAttrRegrel = {
{(SYMS_U8*)"CvAttrRegrel", 12}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvAttrRegrel), _syms_serial_members_for_SYMS_CvAttrRegrel, sizeof(SYMS_CvAttrRegrel), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvAttrManyreg = {
{(SYMS_U8*)"CvAttrManyreg", 13}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvAttrManyreg), _syms_serial_members_for_SYMS_CvAttrManyreg, sizeof(SYMS_CvAttrManyreg), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvFastLinkFlags = {
{(SYMS_U8*)"SYMS_CvFastLinkFlags", 20}, SYMS_SerialTypeKind_Flags, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvFastLinkFlags), _syms_serial_members_for_SYMS_CvFastLinkFlags, sizeof(SYMS_CvFastLinkFlags), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvFastLink = {
{(SYMS_U8*)"CvFastLink", 10}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvFastLink), _syms_serial_members_for_SYMS_CvFastLink, sizeof(SYMS_CvFastLink), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvArmSwitchType = {
{(SYMS_U8*)"SYMS_CvArmSwitchType", 20}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvArmSwitchType), _syms_serial_members_for_SYMS_CvArmSwitchType, sizeof(SYMS_CvArmSwitchType), syms_enum_index_from_value_identity
};
SYMS_SerialType _syms_serial_type_SYMS_CvArmSwitchTable = {
{(SYMS_U8*)"CvArmSwitchTable", 16}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvArmSwitchTable), _syms_serial_members_for_SYMS_CvArmSwitchTable, sizeof(SYMS_CvArmSwitchTable), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvRefMiniPdbFlags = {
{(SYMS_U8*)"SYMS_CvRefMiniPdbFlags", 22}, SYMS_SerialTypeKind_Flags, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvRefMiniPdbFlags), _syms_serial_members_for_SYMS_CvRefMiniPdbFlags, sizeof(SYMS_CvRefMiniPdbFlags), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvRefMiniPdb = {
{(SYMS_U8*)"CvRefMiniPdb", 12}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvRefMiniPdb), _syms_serial_members_for_SYMS_CvRefMiniPdb, sizeof(SYMS_CvRefMiniPdb), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvModTypeRefFlags = {
{(SYMS_U8*)"SYMS_CvModTypeRefFlags", 22}, SYMS_SerialTypeKind_Flags, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvModTypeRefFlags), _syms_serial_members_for_SYMS_CvModTypeRefFlags, sizeof(SYMS_CvModTypeRefFlags), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvModTypeRef = {
{(SYMS_U8*)"CvModTypeRef", 12}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvModTypeRef), _syms_serial_members_for_SYMS_CvModTypeRef, sizeof(SYMS_CvModTypeRef), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvDiscardedType = {
{(SYMS_U8*)"SYMS_CvDiscardedType", 20}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvDiscardedType), _syms_serial_members_for_SYMS_CvDiscardedType, sizeof(SYMS_CvDiscardedType), syms_enum_index_from_value_identity
};
SYMS_SerialType _syms_serial_type_SYMS_CvDiscardedFlags = {
{(SYMS_U8*)"SYMS_CvDiscardedFlags", 21}, SYMS_SerialTypeKind_Flags, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvDiscardedFlags), _syms_serial_members_for_SYMS_CvDiscardedFlags, sizeof(SYMS_CvDiscardedFlags), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvDiscarded = {
{(SYMS_U8*)"CvDiscarded", 11}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvDiscarded), _syms_serial_members_for_SYMS_CvDiscarded, sizeof(SYMS_CvDiscarded), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvLeaf = {
{(SYMS_U8*)"SYMS_CvLeaf", 11}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvLeaf), _syms_serial_members_for_SYMS_CvLeaf, sizeof(SYMS_CvLeaf), syms_enum_index_from_cvleaf
};
SYMS_SerialType _syms_serial_type_SYMS_CvModifierFlags = {
{(SYMS_U8*)"SYMS_CvModifierFlags", 20}, SYMS_SerialTypeKind_Flags, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvModifierFlags), _syms_serial_members_for_SYMS_CvModifierFlags, sizeof(SYMS_CvModifierFlags), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvHFAKind = {
{(SYMS_U8*)"SYMS_CvHFAKind", 14}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvHFAKind), _syms_serial_members_for_SYMS_CvHFAKind, sizeof(SYMS_CvHFAKind), syms_enum_index_from_value_identity
};
SYMS_SerialType _syms_serial_type_SYMS_CvMoComUDTKind = {
{(SYMS_U8*)"SYMS_CvMoComUDTKind", 19}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvMoComUDTKind), _syms_serial_members_for_SYMS_CvMoComUDTKind, sizeof(SYMS_CvMoComUDTKind), syms_enum_index_from_value_identity
};
SYMS_SerialType _syms_serial_type_SYMS_CvTypeProps = {
{(SYMS_U8*)"SYMS_CvTypeProps", 16}, SYMS_SerialTypeKind_Flags, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvTypeProps), _syms_serial_members_for_SYMS_CvTypeProps, sizeof(SYMS_CvTypeProps), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvPointerKind = {
{(SYMS_U8*)"SYMS_CvPointerKind", 18}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvPointerKind), _syms_serial_members_for_SYMS_CvPointerKind, sizeof(SYMS_CvPointerKind), syms_enum_index_from_value_identity
};
SYMS_SerialType _syms_serial_type_SYMS_CvPointerMode = {
{(SYMS_U8*)"SYMS_CvPointerMode", 18}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvPointerMode), _syms_serial_members_for_SYMS_CvPointerMode, sizeof(SYMS_CvPointerMode), syms_enum_index_from_cvpointermode
};
SYMS_SerialType _syms_serial_type_SYMS_CvPointerAttribs = {
{(SYMS_U8*)"SYMS_CvPointerAttribs", 21}, SYMS_SerialTypeKind_Flags, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvPointerAttribs), _syms_serial_members_for_SYMS_CvPointerAttribs, sizeof(SYMS_CvPointerAttribs), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvMemberPointerKind = {
{(SYMS_U8*)"SYMS_CvMemberPointerKind", 24}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvMemberPointerKind), _syms_serial_members_for_SYMS_CvMemberPointerKind, sizeof(SYMS_CvMemberPointerKind), syms_enum_index_from_value_identity
};
SYMS_SerialType _syms_serial_type_SYMS_CvVirtualTableShape = {
{(SYMS_U8*)"SYMS_CvVirtualTableShape", 24}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvVirtualTableShape), _syms_serial_members_for_SYMS_CvVirtualTableShape, sizeof(SYMS_CvVirtualTableShape), syms_enum_index_from_value_identity
};
SYMS_SerialType _syms_serial_type_SYMS_CvMethodProp = {
{(SYMS_U8*)"SYMS_CvMethodProp", 17}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvMethodProp), _syms_serial_members_for_SYMS_CvMethodProp, sizeof(SYMS_CvMethodProp), syms_enum_index_from_value_identity
};
SYMS_SerialType _syms_serial_type_SYMS_CvMemberAccess = {
{(SYMS_U8*)"SYMS_CvMemberAccess", 19}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvMemberAccess), _syms_serial_members_for_SYMS_CvMemberAccess, sizeof(SYMS_CvMemberAccess), syms_enum_index_from_value_identity
};
SYMS_SerialType _syms_serial_type_SYMS_CvFieldAttribs = {
{(SYMS_U8*)"SYMS_CvFieldAttribs", 19}, SYMS_SerialTypeKind_Flags, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvFieldAttribs), _syms_serial_members_for_SYMS_CvFieldAttribs, sizeof(SYMS_CvFieldAttribs), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvLabelKind = {
{(SYMS_U8*)"SYMS_CvLabelKind", 16}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvLabelKind), _syms_serial_members_for_SYMS_CvLabelKind, sizeof(SYMS_CvLabelKind), syms_enum_index_from_cvlabelkind
};
SYMS_SerialType _syms_serial_type_SYMS_CvFunctionAttribs = {
{(SYMS_U8*)"SYMS_CvFunctionAttribs", 22}, SYMS_SerialTypeKind_Flags, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvFunctionAttribs), _syms_serial_members_for_SYMS_CvFunctionAttribs, sizeof(SYMS_CvFunctionAttribs), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvCallKind = {
{(SYMS_U8*)"SYMS_CvCallKind", 15}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvCallKind), _syms_serial_members_for_SYMS_CvCallKind, sizeof(SYMS_CvCallKind), syms_enum_index_from_value_identity
};
SYMS_SerialType _syms_serial_type_SYMS_CvLeafPreComp = {
{(SYMS_U8*)"CvLeafPreComp", 13}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvLeafPreComp), _syms_serial_members_for_SYMS_CvLeafPreComp, sizeof(SYMS_CvLeafPreComp), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvLeafTypeServer = {
{(SYMS_U8*)"CvLeafTypeServer", 16}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvLeafTypeServer), _syms_serial_members_for_SYMS_CvLeafTypeServer, sizeof(SYMS_CvLeafTypeServer), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvLeafTypeServer2 = {
{(SYMS_U8*)"CvLeafTypeServer2", 17}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvLeafTypeServer2), _syms_serial_members_for_SYMS_CvLeafTypeServer2, sizeof(SYMS_CvLeafTypeServer2), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvLeafBuildInfo = {
{(SYMS_U8*)"CvLeafBuildInfo", 15}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvLeafBuildInfo), _syms_serial_members_for_SYMS_CvLeafBuildInfo, sizeof(SYMS_CvLeafBuildInfo), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvLeafSkip_16t = {
{(SYMS_U8*)"CvLeafSkip_16t", 14}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvLeafSkip_16t), _syms_serial_members_for_SYMS_CvLeafSkip_16t, sizeof(SYMS_CvLeafSkip_16t), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvLeafSkip = {
{(SYMS_U8*)"CvLeafSkip", 10}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvLeafSkip), _syms_serial_members_for_SYMS_CvLeafSkip, sizeof(SYMS_CvLeafSkip), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvLeafVTShape = {
{(SYMS_U8*)"CvLeafVTShape", 13}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvLeafVTShape), _syms_serial_members_for_SYMS_CvLeafVTShape, sizeof(SYMS_CvLeafVTShape), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvLeafLabel = {
{(SYMS_U8*)"CvLeafLabel", 11}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvLeafLabel), _syms_serial_members_for_SYMS_CvLeafLabel, sizeof(SYMS_CvLeafLabel), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvLeafModifier = {
{(SYMS_U8*)"CvLeafModifier", 14}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvLeafModifier), _syms_serial_members_for_SYMS_CvLeafModifier, sizeof(SYMS_CvLeafModifier), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvLeafPointer = {
{(SYMS_U8*)"CvLeafPointer", 13}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvLeafPointer), _syms_serial_members_for_SYMS_CvLeafPointer, sizeof(SYMS_CvLeafPointer), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvLeafProcedure = {
{(SYMS_U8*)"CvLeafProcedure", 15}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvLeafProcedure), _syms_serial_members_for_SYMS_CvLeafProcedure, sizeof(SYMS_CvLeafProcedure), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvLeafMFunction = {
{(SYMS_U8*)"CvLeafMFunction", 15}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvLeafMFunction), _syms_serial_members_for_SYMS_CvLeafMFunction, sizeof(SYMS_CvLeafMFunction), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvLeafArgList = {
{(SYMS_U8*)"CvLeafArgList", 13}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvLeafArgList), _syms_serial_members_for_SYMS_CvLeafArgList, sizeof(SYMS_CvLeafArgList), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvLeafBitField = {
{(SYMS_U8*)"CvLeafBitField", 14}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvLeafBitField), _syms_serial_members_for_SYMS_CvLeafBitField, sizeof(SYMS_CvLeafBitField), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvLeafIndex = {
{(SYMS_U8*)"CvLeafIndex", 11}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvLeafIndex), _syms_serial_members_for_SYMS_CvLeafIndex, sizeof(SYMS_CvLeafIndex), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvLeafArray = {
{(SYMS_U8*)"CvLeafArray", 11}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvLeafArray), _syms_serial_members_for_SYMS_CvLeafArray, sizeof(SYMS_CvLeafArray), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvLeafStruct = {
{(SYMS_U8*)"CvLeafStruct", 12}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvLeafStruct), _syms_serial_members_for_SYMS_CvLeafStruct, sizeof(SYMS_CvLeafStruct), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvLeafUnion = {
{(SYMS_U8*)"CvLeafUnion", 11}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvLeafUnion), _syms_serial_members_for_SYMS_CvLeafUnion, sizeof(SYMS_CvLeafUnion), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvLeafEnum = {
{(SYMS_U8*)"CvLeafEnum", 10}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvLeafEnum), _syms_serial_members_for_SYMS_CvLeafEnum, sizeof(SYMS_CvLeafEnum), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvLeafAlias = {
{(SYMS_U8*)"CvLeafAlias", 11}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvLeafAlias), _syms_serial_members_for_SYMS_CvLeafAlias, sizeof(SYMS_CvLeafAlias), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvLeafMember = {
{(SYMS_U8*)"CvLeafMember", 12}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvLeafMember), _syms_serial_members_for_SYMS_CvLeafMember, sizeof(SYMS_CvLeafMember), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvLeafStMember = {
{(SYMS_U8*)"CvLeafStMember", 14}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvLeafStMember), _syms_serial_members_for_SYMS_CvLeafStMember, sizeof(SYMS_CvLeafStMember), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvLeafMethodListMember = {
{(SYMS_U8*)"CvLeafMethodListMember", 22}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvLeafMethodListMember), _syms_serial_members_for_SYMS_CvLeafMethodListMember, sizeof(SYMS_CvLeafMethodListMember), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvLeafMethod = {
{(SYMS_U8*)"CvLeafMethod", 12}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvLeafMethod), _syms_serial_members_for_SYMS_CvLeafMethod, sizeof(SYMS_CvLeafMethod), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvLeafOneMethod = {
{(SYMS_U8*)"CvLeafOneMethod", 15}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvLeafOneMethod), _syms_serial_members_for_SYMS_CvLeafOneMethod, sizeof(SYMS_CvLeafOneMethod), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvLeafEnumerate = {
{(SYMS_U8*)"CvLeafEnumerate", 15}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvLeafEnumerate), _syms_serial_members_for_SYMS_CvLeafEnumerate, sizeof(SYMS_CvLeafEnumerate), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvLeafNestType = {
{(SYMS_U8*)"CvLeafNestType", 14}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvLeafNestType), _syms_serial_members_for_SYMS_CvLeafNestType, sizeof(SYMS_CvLeafNestType), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvLeafNestTypeEx = {
{(SYMS_U8*)"CvLeafNestTypeEx", 16}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvLeafNestTypeEx), _syms_serial_members_for_SYMS_CvLeafNestTypeEx, sizeof(SYMS_CvLeafNestTypeEx), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvLeafBClass = {
{(SYMS_U8*)"CvLeafBClass", 12}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvLeafBClass), _syms_serial_members_for_SYMS_CvLeafBClass, sizeof(SYMS_CvLeafBClass), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvLeafVBClass = {
{(SYMS_U8*)"CvLeafVBClass", 13}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvLeafVBClass), _syms_serial_members_for_SYMS_CvLeafVBClass, sizeof(SYMS_CvLeafVBClass), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvLeafVFuncTab = {
{(SYMS_U8*)"CvLeafVFuncTab", 14}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvLeafVFuncTab), _syms_serial_members_for_SYMS_CvLeafVFuncTab, sizeof(SYMS_CvLeafVFuncTab), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvLeafVFuncOff = {
{(SYMS_U8*)"CvLeafVFuncOff", 14}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvLeafVFuncOff), _syms_serial_members_for_SYMS_CvLeafVFuncOff, sizeof(SYMS_CvLeafVFuncOff), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvLeafVFTable = {
{(SYMS_U8*)"CvLeafVFTable", 13}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvLeafVFTable), _syms_serial_members_for_SYMS_CvLeafVFTable, sizeof(SYMS_CvLeafVFTable), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvLeafVFPath = {
{(SYMS_U8*)"CvLeafVFPath", 12}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvLeafVFPath), _syms_serial_members_for_SYMS_CvLeafVFPath, sizeof(SYMS_CvLeafVFPath), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvLeafFuncId = {
{(SYMS_U8*)"CvLeafFuncId", 12}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvLeafFuncId), _syms_serial_members_for_SYMS_CvLeafFuncId, sizeof(SYMS_CvLeafFuncId), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvLeafMFuncId = {
{(SYMS_U8*)"CvLeafMFuncId", 13}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvLeafMFuncId), _syms_serial_members_for_SYMS_CvLeafMFuncId, sizeof(SYMS_CvLeafMFuncId), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvLeafStringId = {
{(SYMS_U8*)"CvLeafStringId", 14}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvLeafStringId), _syms_serial_members_for_SYMS_CvLeafStringId, sizeof(SYMS_CvLeafStringId), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvLeafUDTSrcLine = {
{(SYMS_U8*)"CvLeafUDTSrcLine", 16}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvLeafUDTSrcLine), _syms_serial_members_for_SYMS_CvLeafUDTSrcLine, sizeof(SYMS_CvLeafUDTSrcLine), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvLeafModSrcLine = {
{(SYMS_U8*)"CvLeafModSrcLine", 16}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvLeafModSrcLine), _syms_serial_members_for_SYMS_CvLeafModSrcLine, sizeof(SYMS_CvLeafModSrcLine), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvLeafClassPtr = {
{(SYMS_U8*)"CvLeafClassPtr", 14}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvLeafClassPtr), _syms_serial_members_for_SYMS_CvLeafClassPtr, sizeof(SYMS_CvLeafClassPtr), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvSubSectionKind = {
{(SYMS_U8*)"SYMS_CvSubSectionKind", 21}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvSubSectionKind), _syms_serial_members_for_SYMS_CvSubSectionKind, sizeof(SYMS_CvSubSectionKind), syms_enum_index_from_cvsubsectionkind
};
SYMS_SerialType _syms_serial_type_SYMS_CvSubSectionHeader = {
{(SYMS_U8*)"CvSubSectionHeader", 18}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvSubSectionHeader), _syms_serial_members_for_SYMS_CvSubSectionHeader, sizeof(SYMS_CvSubSectionHeader), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvChecksumKind = {
{(SYMS_U8*)"SYMS_CvChecksumKind", 19}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvChecksumKind), _syms_serial_members_for_SYMS_CvChecksumKind, sizeof(SYMS_CvChecksumKind), syms_enum_index_from_value_identity
};
SYMS_SerialType _syms_serial_type_SYMS_CvInlineeSourceLineSig = {
{(SYMS_U8*)"SYMS_CvInlineeSourceLineSig", 27}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvInlineeSourceLineSig), _syms_serial_members_for_SYMS_CvInlineeSourceLineSig, sizeof(SYMS_CvInlineeSourceLineSig), syms_enum_index_from_value_identity
};
SYMS_SerialType _syms_serial_type_SYMS_CvInlineeSourceLine = {
{(SYMS_U8*)"CvInlineeSourceLine", 19}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvInlineeSourceLine), _syms_serial_members_for_SYMS_CvInlineeSourceLine, sizeof(SYMS_CvInlineeSourceLine), 0
};
SYMS_SerialType _syms_serial_type_SYMS_CvInlineeSourceLineEx = {
{(SYMS_U8*)"CvInlineeSourceLineEx", 21}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_CvInlineeSourceLineEx), _syms_serial_members_for_SYMS_CvInlineeSourceLineEx, sizeof(SYMS_CvInlineeSourceLineEx), 0
};

#endif // defined(SYMS_ENABLE_CV_SERIAL_INFO)


////////////////////////////////
#if defined(SYMS_ENABLE_DEBUG_INFO_SERIAL_INFO)

//~ generated from code at syms/metaprogram/syms_metaprogram_serial.c:1091
// syms_enum_index_from_symbol_kind - skipped identity mapping
// syms_enum_index_from_mem_visibility - skipped identity mapping

//~ generated from code at syms/metaprogram/syms_metaprogram_serial.c:1322
SYMS_SerialFlag _syms_serial_members_for_SYMS_UnitFeatures[] = {
{ { (SYMS_U8*)"CompilationUnit", 15 }, &_syms_serial_type_SYMS_U32, 0x1, 0 },
{ { (SYMS_U8*)"Types", 5 }, &_syms_serial_type_SYMS_U32, 0x1, 1 },
{ { (SYMS_U8*)"StaticVariables", 15 }, &_syms_serial_type_SYMS_U32, 0x1, 2 },
{ { (SYMS_U8*)"ExternVariables", 15 }, &_syms_serial_type_SYMS_U32, 0x1, 3 },
{ { (SYMS_U8*)"Functions", 9 }, &_syms_serial_type_SYMS_U32, 0x1, 4 },
{ { (SYMS_U8*)"FunctionStubs", 13 }, &_syms_serial_type_SYMS_U32, 0x1, 5 },
};
SYMS_SerialValue _syms_serial_members_for_SYMS_SymbolKind[] = {
{ { (SYMS_U8*)"Null", 4 }, (SYMS_U64)SYMS_SymbolKind_Null },
{ { (SYMS_U8*)"Type", 4 }, (SYMS_U64)SYMS_SymbolKind_Type },
{ { (SYMS_U8*)"Procedure", 9 }, (SYMS_U64)SYMS_SymbolKind_Procedure },
{ { (SYMS_U8*)"ImageRelativeVariable", 21 }, (SYMS_U64)SYMS_SymbolKind_ImageRelativeVariable },
{ { (SYMS_U8*)"LocalVariable", 13 }, (SYMS_U64)SYMS_SymbolKind_LocalVariable },
{ { (SYMS_U8*)"TLSVariable", 11 }, (SYMS_U64)SYMS_SymbolKind_TLSVariable },
{ { (SYMS_U8*)"Const", 5 }, (SYMS_U64)SYMS_SymbolKind_Const },
{ { (SYMS_U8*)"Scope", 5 }, (SYMS_U64)SYMS_SymbolKind_Scope },
{ { (SYMS_U8*)"Inline", 6 }, (SYMS_U64)SYMS_SymbolKind_Inline },
};
SYMS_SerialFlag _syms_serial_members_for_SYMS_TypeModifiers[] = {
{ { (SYMS_U8*)"Const", 5 }, &_syms_serial_type_SYMS_U32, 0x1, 0 },
{ { (SYMS_U8*)"Packed", 6 }, &_syms_serial_type_SYMS_U32, 0x1, 1 },
{ { (SYMS_U8*)"Restrict", 8 }, &_syms_serial_type_SYMS_U32, 0x1, 2 },
{ { (SYMS_U8*)"Shared", 6 }, &_syms_serial_type_SYMS_U32, 0x1, 3 },
{ { (SYMS_U8*)"Volatile", 8 }, &_syms_serial_type_SYMS_U32, 0x1, 4 },
{ { (SYMS_U8*)"Char", 4 }, &_syms_serial_type_SYMS_U32, 0x1, 5 },
{ { (SYMS_U8*)"Reference", 9 }, &_syms_serial_type_SYMS_U32, 0x1, 6 },
{ { (SYMS_U8*)"RValueReference", 15 }, &_syms_serial_type_SYMS_U32, 0x1, 7 },
};
SYMS_SerialValue _syms_serial_members_for_SYMS_MemVisibility[] = {
{ { (SYMS_U8*)"Null", 4 }, (SYMS_U64)SYMS_MemVisibility_Null },
{ { (SYMS_U8*)"Private", 7 }, (SYMS_U64)SYMS_MemVisibility_Private },
{ { (SYMS_U8*)"Public", 6 }, (SYMS_U64)SYMS_MemVisibility_Public },
{ { (SYMS_U8*)"Protected", 9 }, (SYMS_U64)SYMS_MemVisibility_Protected },
};

//~ generated from code at syms/metaprogram/syms_metaprogram_serial.c:1458
SYMS_SerialType _syms_serial_type_SYMS_UnitFeatures = {
{(SYMS_U8*)"SYMS_UnitFeatures", 17}, SYMS_SerialTypeKind_Flags, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_UnitFeatures), _syms_serial_members_for_SYMS_UnitFeatures, sizeof(SYMS_UnitFeatures), 0
};
SYMS_SerialType _syms_serial_type_SYMS_SymbolKind = {
{(SYMS_U8*)"SYMS_SymbolKind", 15}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_SymbolKind), _syms_serial_members_for_SYMS_SymbolKind, sizeof(SYMS_SymbolKind), syms_enum_index_from_value_identity
};
SYMS_SerialType _syms_serial_type_SYMS_TypeModifiers = {
{(SYMS_U8*)"SYMS_TypeModifiers", 18}, SYMS_SerialTypeKind_Flags, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_TypeModifiers), _syms_serial_members_for_SYMS_TypeModifiers, sizeof(SYMS_TypeModifiers), 0
};
SYMS_SerialType _syms_serial_type_SYMS_MemVisibility = {
{(SYMS_U8*)"SYMS_MemVisibility", 18}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_MemVisibility), _syms_serial_members_for_SYMS_MemVisibility, sizeof(SYMS_MemVisibility), syms_enum_index_from_value_identity
};

#endif // defined(SYMS_ENABLE_DEBUG_INFO_SERIAL_INFO)


////////////////////////////////
#if defined(SYMS_ENABLE_DWARF_SERIAL_INFO)

//~ generated from code at syms/metaprogram/syms_metaprogram_serial.c:1091
// syms_enum_index_from_dw_mode - skipped identity mapping
// syms_enum_index_from_dw_version - skipped identity mapping
// syms_enum_index_from_dw_section_kind - skipped identity mapping
SYMS_API SYMS_U64
syms_enum_index_from_dwlanguage(SYMS_U64 v){
SYMS_U64 result = 0;
switch ((SYMS_U64)v){
default: break;
case SYMS_DwLanguage_NULL: result = 0; break;
case SYMS_DwLanguage_C89: result = 1; break;
case SYMS_DwLanguage_C: result = 2; break;
case SYMS_DwLanguage_Ada83: result = 3; break;
case SYMS_DwLanguage_CPlusPlus: result = 4; break;
case SYMS_DwLanguage_Cobol74: result = 5; break;
case SYMS_DwLanguage_Cobol85: result = 6; break;
case SYMS_DwLanguage_Fortran77: result = 7; break;
case SYMS_DwLanguage_Fortran90: result = 8; break;
case SYMS_DwLanguage_Pascal83: result = 9; break;
case SYMS_DwLanguage_Modula2: result = 10; break;
case SYMS_DwLanguage_Java: result = 11; break;
case SYMS_DwLanguage_C99: result = 12; break;
case SYMS_DwLanguage_Ada95: result = 13; break;
case SYMS_DwLanguage_Fortran95: result = 14; break;
case SYMS_DwLanguage_PLI: result = 15; break;
case SYMS_DwLanguage_ObjectiveC: result = 16; break;
case SYMS_DwLanguage_ObjectiveCPlusPlus: result = 17; break;
case SYMS_DwLanguage_UPC: result = 18; break;
case SYMS_DwLanguage_D: result = 19; break;
case SYMS_DwLanguage_Python: result = 20; break;
case SYMS_DwLanguage_OpenCL: result = 21; break;
case SYMS_DwLanguage_Go: result = 22; break;
case SYMS_DwLanguage_Modula3: result = 23; break;
case SYMS_DwLanguage_Haskell: result = 24; break;
case SYMS_DwLanguage_CPlusPlus03: result = 25; break;
case SYMS_DwLanguage_CPlusPlus11: result = 26; break;
case SYMS_DwLanguage_OCaml: result = 27; break;
case SYMS_DwLanguage_Rust: result = 28; break;
case SYMS_DwLanguage_C11: result = 29; break;
case SYMS_DwLanguage_Swift: result = 30; break;
case SYMS_DwLanguage_Julia: result = 31; break;
case SYMS_DwLanguage_Dylan: result = 32; break;
case SYMS_DwLanguage_CPlusPlus14: result = 33; break;
case SYMS_DwLanguage_Fortran03: result = 34; break;
case SYMS_DwLanguage_Fortran08: result = 35; break;
case SYMS_DwLanguage_RenderScript: result = 36; break;
case SYMS_DwLanguage_BLISS: result = 37; break;
case SYMS_DwLanguage_MIPS_ASSEMBLER: result = 38; break;
case SYMS_DwLanguage_GOOGLE_RENDER_SCRIPT: result = 39; break;
case SYMS_DwLanguage_SUN_ASSEMBLER: result = 40; break;
case SYMS_DwLanguage_BORLAND_DELPHI: result = 41; break;
case SYMS_DwLanguage_LO_USER: result = 42; break;
case SYMS_DwLanguage_HI_USER: result = 43; break;
}
return(result);
}
// syms_enum_index_from_dwstdopcode - skipped identity mapping
SYMS_API SYMS_U64
syms_enum_index_from_dwextopcode(SYMS_U64 v){
SYMS_U64 result = 0;
switch ((SYMS_U64)v){
default: break;
case SYMS_DwExtOpcode_UNDEFINED: result = 0; break;
case SYMS_DwExtOpcode_END_SEQUENCE: result = 1; break;
case SYMS_DwExtOpcode_SET_ADDRESS: result = 2; break;
case SYMS_DwExtOpcode_DEFINE_FILE: result = 3; break;
case SYMS_DwExtOpcode_SET_DISCRIMINATOR: result = 4; break;
case SYMS_DwExtOpcode_LO_USER: result = 5; break;
case SYMS_DwExtOpcode_HI_USER: result = 6; break;
}
return(result);
}
// syms_enum_index_from_dw_name_case - skipped identity mapping
SYMS_API SYMS_U64
syms_enum_index_from_dwtagkind(SYMS_U64 v){
SYMS_U64 result = 0;
switch ((SYMS_U64)v){
default: break;
case SYMS_DwTagKind_NULL: result = 0; break;
case SYMS_DwTagKind_ARRAY_TYPE: result = 1; break;
case SYMS_DwTagKind_CLASS_TYPE: result = 2; break;
case SYMS_DwTagKind_ENTRY_POINT: result = 3; break;
case SYMS_DwTagKind_ENUMERATION_TYPE: result = 4; break;
case SYMS_DwTagKind_FORMAL_PARAMETER: result = 5; break;
case SYMS_DwTagKind_IMPORTED_DECLARATION: result = 6; break;
case SYMS_DwTagKind_LABEL: result = 7; break;
case SYMS_DwTagKind_LEXICAL_BLOCK: result = 8; break;
case SYMS_DwTagKind_MEMBER: result = 9; break;
case SYMS_DwTagKind_POINTER_TYPE: result = 10; break;
case SYMS_DwTagKind_REFERENCE_TYPE: result = 11; break;
case SYMS_DwTagKind_COMPILE_UNIT: result = 12; break;
case SYMS_DwTagKind_STRING_TYPE: result = 13; break;
case SYMS_DwTagKind_STRUCTURE_TYPE: result = 14; break;
case SYMS_DwTagKind_SUBROUTINE_TYPE: result = 15; break;
case SYMS_DwTagKind_TYPEDEF: result = 16; break;
case SYMS_DwTagKind_UNION_TYPE: result = 17; break;
case SYMS_DwTagKind_UNSPECIFIED_PARAMETERS: result = 18; break;
case SYMS_DwTagKind_VARIANT: result = 19; break;
case SYMS_DwTagKind_COMMON_BLOCK: result = 20; break;
case SYMS_DwTagKind_COMMON_INCLUSION: result = 21; break;
case SYMS_DwTagKind_INHERITANCE: result = 22; break;
case SYMS_DwTagKind_INLINED_SUBROUTINE: result = 23; break;
case SYMS_DwTagKind_MODULE: result = 24; break;
case SYMS_DwTagKind_PTR_TO_MEMBER_TYPE: result = 25; break;
case SYMS_DwTagKind_SET_TYPE: result = 26; break;
case SYMS_DwTagKind_SUBRANGE_TYPE: result = 27; break;
case SYMS_DwTagKind_WITH_STMT: result = 28; break;
case SYMS_DwTagKind_ACCESS_DECLARATION: result = 29; break;
case SYMS_DwTagKind_BASE_TYPE: result = 30; break;
case SYMS_DwTagKind_CATCH_BLOCK: result = 31; break;
case SYMS_DwTagKind_CONST_TYPE: result = 32; break;
case SYMS_DwTagKind_CONSTANT: result = 33; break;
case SYMS_DwTagKind_ENUMERATOR: result = 34; break;
case SYMS_DwTagKind_FILE_TYPE: result = 35; break;
case SYMS_DwTagKind_FRIEND: result = 36; break;
case SYMS_DwTagKind_NAMELIST: result = 37; break;
case SYMS_DwTagKind_NAMELIST_ITEM: result = 38; break;
case SYMS_DwTagKind_PACKED_TYPE: result = 39; break;
case SYMS_DwTagKind_SUBPROGRAM: result = 40; break;
case SYMS_DwTagKind_TEMPLATE_TYPE_PARAMETER: result = 41; break;
case SYMS_DwTagKind_TEMPLATE_VALUE_PARAMETER: result = 42; break;
case SYMS_DwTagKind_THROWN_TYPE: result = 43; break;
case SYMS_DwTagKind_TRY_BLOCK: result = 44; break;
case SYMS_DwTagKind_VARIANT_PART: result = 45; break;
case SYMS_DwTagKind_VARIABLE: result = 46; break;
case SYMS_DwTagKind_VOLATILE_TYPE: result = 47; break;
case SYMS_DwTagKind_DWARF_PROCEDURE: result = 48; break;
case SYMS_DwTagKind_RESTRICT_TYPE: result = 49; break;
case SYMS_DwTagKind_INTERFACE_TYPE: result = 50; break;
case SYMS_DwTagKind_NAMESPACE: result = 51; break;
case SYMS_DwTagKind_IMPORTED_MODULE: result = 52; break;
case SYMS_DwTagKind_UNSPECIFIED_TYPE: result = 53; break;
case SYMS_DwTagKind_PARTIAL_UNIT: result = 54; break;
case SYMS_DwTagKind_IMPORTED_UNIT: result = 55; break;
case SYMS_DwTagKind_CONDITION: result = 56; break;
case SYMS_DwTagKind_SHARED_TYPE: result = 57; break;
case SYMS_DwTagKind_TYPE_UNIT: result = 58; break;
case SYMS_DwTagKind_RVALUE_REFERENCE_TYPE: result = 59; break;
case SYMS_DwTagKind_TEMPLATE_ALIAS: result = 60; break;
case SYMS_DwTagKind_COARRAY_TYPE: result = 61; break;
case SYMS_DwTagKind_GENERIC_SUBRANGE: result = 62; break;
case SYMS_DwTagKind_DYNAMIC_TYPE: result = 63; break;
case SYMS_DwTagKind_ATOMIC_TYPE: result = 64; break;
case SYMS_DwTagKind_CALL_SITE: result = 65; break;
case SYMS_DwTagKind_CALL_SITE_PARAMETER: result = 66; break;
case SYMS_DwTagKind_SKELETON_UNIT: result = 67; break;
case SYMS_DwTagKind_IMMUTABLE_TYPE: result = 68; break;
case SYMS_DwTagKind_GNU_CALL_SITE: result = 69; break;
case SYMS_DwTagKind_GNU_CALL_SITE_PARAMETER: result = 70; break;
case SYMS_DwTagKind_LO_USER: result = 71; break;
case SYMS_DwTagKind_HI_USER: result = 72; break;
}
return(result);
}
SYMS_API SYMS_U64
syms_enum_index_from_dwformkind(SYMS_U64 v){
SYMS_U64 result = 0;
switch ((SYMS_U64)v){
default: break;
case SYMS_DwFormKind_ADDR: result = 0; break;
case SYMS_DwFormKind_BLOCK2: result = 1; break;
case SYMS_DwFormKind_BLOCK4: result = 2; break;
case SYMS_DwFormKind_DATA2: result = 3; break;
case SYMS_DwFormKind_DATA4: result = 4; break;
case SYMS_DwFormKind_DATA8: result = 5; break;
case SYMS_DwFormKind_STRING: result = 6; break;
case SYMS_DwFormKind_BLOCK: result = 7; break;
case SYMS_DwFormKind_BLOCK1: result = 8; break;
case SYMS_DwFormKind_DATA1: result = 9; break;
case SYMS_DwFormKind_FLAG: result = 10; break;
case SYMS_DwFormKind_SDATA: result = 11; break;
case SYMS_DwFormKind_STRP: result = 12; break;
case SYMS_DwFormKind_UDATA: result = 13; break;
case SYMS_DwFormKind_REF_ADDR: result = 14; break;
case SYMS_DwFormKind_REF1: result = 15; break;
case SYMS_DwFormKind_REF2: result = 16; break;
case SYMS_DwFormKind_REF4: result = 17; break;
case SYMS_DwFormKind_REF8: result = 18; break;
case SYMS_DwFormKind_REF_UDATA: result = 19; break;
case SYMS_DwFormKind_INDIRECT: result = 20; break;
case SYMS_DwFormKind_SEC_OFFSET: result = 21; break;
case SYMS_DwFormKind_EXPRLOC: result = 22; break;
case SYMS_DwFormKind_FLAG_PRESENT: result = 23; break;
case SYMS_DwFormKind_REF_SIG8: result = 24; break;
case SYMS_DwFormKind_STRX: result = 25; break;
case SYMS_DwFormKind_ADDRX: result = 26; break;
case SYMS_DwFormKind_REF_SUP4: result = 27; break;
case SYMS_DwFormKind_STRP_SUP: result = 28; break;
case SYMS_DwFormKind_DATA16: result = 29; break;
case SYMS_DwFormKind_LINE_STRP: result = 30; break;
case SYMS_DwFormKind_IMPLICIT_CONST: result = 31; break;
case SYMS_DwFormKind_LOCLISTX: result = 32; break;
case SYMS_DwFormKind_RNGLISTX: result = 33; break;
case SYMS_DwFormKind_REF_SUP8: result = 34; break;
case SYMS_DwFormKind_STRX1: result = 35; break;
case SYMS_DwFormKind_STRX2: result = 36; break;
case SYMS_DwFormKind_STRX3: result = 37; break;
case SYMS_DwFormKind_STRX4: result = 38; break;
case SYMS_DwFormKind_ADDRX1: result = 39; break;
case SYMS_DwFormKind_ADDRX2: result = 40; break;
case SYMS_DwFormKind_ADDRX3: result = 41; break;
case SYMS_DwFormKind_ADDRX4: result = 42; break;
case SYMS_DwFormKind_INVALID: result = 43; break;
}
return(result);
}
SYMS_API SYMS_U64
syms_enum_index_from_dwattribkind(SYMS_U64 v){
SYMS_U64 result = 0;
switch ((SYMS_U64)v){
default: break;
case SYMS_DwAttribKind_SIBLING: result = 0; break;
case SYMS_DwAttribKind_LOCATION: result = 1; break;
case SYMS_DwAttribKind_NAME: result = 2; break;
case SYMS_DwAttribKind_ORDERING: result = 3; break;
case SYMS_DwAttribKind_BYTE_SIZE: result = 4; break;
case SYMS_DwAttribKind_BIT_OFFSET: result = 5; break;
case SYMS_DwAttribKind_BIT_SIZE: result = 6; break;
case SYMS_DwAttribKind_STMT_LIST: result = 7; break;
case SYMS_DwAttribKind_LOW_PC: result = 8; break;
case SYMS_DwAttribKind_HIGH_PC: result = 9; break;
case SYMS_DwAttribKind_LANGUAGE: result = 10; break;
case SYMS_DwAttribKind_DISCR: result = 11; break;
case SYMS_DwAttribKind_DISCR_VALUE: result = 12; break;
case SYMS_DwAttribKind_VISIBILITY: result = 13; break;
case SYMS_DwAttribKind_IMPORT: result = 14; break;
case SYMS_DwAttribKind_STRING_LENGTH: result = 15; break;
case SYMS_DwAttribKind_COMMON_REFERENCE: result = 16; break;
case SYMS_DwAttribKind_COMP_DIR: result = 17; break;
case SYMS_DwAttribKind_CONST_VALUE: result = 18; break;
case SYMS_DwAttribKind_CONTAINING_TYPE: result = 19; break;
case SYMS_DwAttribKind_DEFAULT_VALUE: result = 20; break;
case SYMS_DwAttribKind_INLINE: result = 21; break;
case SYMS_DwAttribKind_IS_OPTIONAL: result = 22; break;
case SYMS_DwAttribKind_LOWER_BOUND: result = 23; break;
case SYMS_DwAttribKind_PRODUCER: result = 24; break;
case SYMS_DwAttribKind_PROTOTYPED: result = 25; break;
case SYMS_DwAttribKind_RETURN_ADDR: result = 26; break;
case SYMS_DwAttribKind_START_SCOPE: result = 27; break;
case SYMS_DwAttribKind_BIT_STRIDE: result = 28; break;
case SYMS_DwAttribKind_UPPER_BOUND: result = 29; break;
case SYMS_DwAttribKind_ABSTRACT_ORIGIN: result = 30; break;
case SYMS_DwAttribKind_ACCESSIBILITY: result = 31; break;
case SYMS_DwAttribKind_ADDRESS_CLASS: result = 32; break;
case SYMS_DwAttribKind_ARTIFICIAL: result = 33; break;
case SYMS_DwAttribKind_BASE_TYPES: result = 34; break;
case SYMS_DwAttribKind_CALLING_CONVENTION: result = 35; break;
case SYMS_DwAttribKind_ARR_COUNT: result = 36; break;
case SYMS_DwAttribKind_DATA_MEMBER_LOCATION: result = 37; break;
case SYMS_DwAttribKind_DECL_COLUMN: result = 38; break;
case SYMS_DwAttribKind_DECL_FILE: result = 39; break;
case SYMS_DwAttribKind_DECL_LINE: result = 40; break;
case SYMS_DwAttribKind_DECLARATION: result = 41; break;
case SYMS_DwAttribKind_DISCR_LIST: result = 42; break;
case SYMS_DwAttribKind_ENCODING: result = 43; break;
case SYMS_DwAttribKind_EXTERNAL: result = 44; break;
case SYMS_DwAttribKind_FRAME_BASE: result = 45; break;
case SYMS_DwAttribKind_FRIEND: result = 46; break;
case SYMS_DwAttribKind_IDENTIFIER_CASE: result = 47; break;
case SYMS_DwAttribKind_MACRO_INFO: result = 48; break;
case SYMS_DwAttribKind_NAMELIST_ITEM: result = 49; break;
case SYMS_DwAttribKind_PRIORITY: result = 50; break;
case SYMS_DwAttribKind_SEGMENT: result = 51; break;
case SYMS_DwAttribKind_SPECIFICATION: result = 52; break;
case SYMS_DwAttribKind_STATIC_LINK: result = 53; break;
case SYMS_DwAttribKind_TYPE: result = 54; break;
case SYMS_DwAttribKind_USE_LOCATION: result = 55; break;
case SYMS_DwAttribKind_VARIABLE_PARAMETER: result = 56; break;
case SYMS_DwAttribKind_VIRTUALITY: result = 57; break;
case SYMS_DwAttribKind_VTABLE_ELEM_LOCATION: result = 58; break;
case SYMS_DwAttribKind_ALLOCATED: result = 59; break;
case SYMS_DwAttribKind_ASSOCIATED: result = 60; break;
case SYMS_DwAttribKind_DATA_LOCATION: result = 61; break;
case SYMS_DwAttribKind_BYTE_STRIDE: result = 62; break;
case SYMS_DwAttribKind_ENTRY_PC: result = 63; break;
case SYMS_DwAttribKind_USE_UTF8: result = 64; break;
case SYMS_DwAttribKind_EXTENSION: result = 65; break;
case SYMS_DwAttribKind_RANGES: result = 66; break;
case SYMS_DwAttribKind_TRAMPOLINE: result = 67; break;
case SYMS_DwAttribKind_CALL_COLUMN: result = 68; break;
case SYMS_DwAttribKind_CALL_FILE: result = 69; break;
case SYMS_DwAttribKind_CALL_LINE: result = 70; break;
case SYMS_DwAttribKind_DESCRIPTION: result = 71; break;
case SYMS_DwAttribKind_BINARY_SCALE: result = 72; break;
case SYMS_DwAttribKind_DECIMAL_SCALE: result = 73; break;
case SYMS_DwAttribKind_SMALL: result = 74; break;
case SYMS_DwAttribKind_DECIMAL_SIGN: result = 75; break;
case SYMS_DwAttribKind_DIGIT_COUNT: result = 76; break;
case SYMS_DwAttribKind_PICTURE_STRING: result = 77; break;
case SYMS_DwAttribKind_MUTABLE: result = 78; break;
case SYMS_DwAttribKind_THREADS_SCALED: result = 79; break;
case SYMS_DwAttribKind_EXPLICIT: result = 80; break;
case SYMS_DwAttribKind_OBJECT_POINTER: result = 81; break;
case SYMS_DwAttribKind_ENDIANITY: result = 82; break;
case SYMS_DwAttribKind_ELEMENTAL: result = 83; break;
case SYMS_DwAttribKind_PURE: result = 84; break;
case SYMS_DwAttribKind_RECURSIVE: result = 85; break;
case SYMS_DwAttribKind_SIGNATURE: result = 86; break;
case SYMS_DwAttribKind_MAIN_SUBPROGRAM: result = 87; break;
case SYMS_DwAttribKind_DATA_BIT_OFFSET: result = 88; break;
case SYMS_DwAttribKind_CONST_EXPR: result = 89; break;
case SYMS_DwAttribKind_ENUM_CLASS: result = 90; break;
case SYMS_DwAttribKind_LINKAGE_NAME: result = 91; break;
case SYMS_DwAttribKind_STRING_LENGTH_BIT_SIZE: result = 92; break;
case SYMS_DwAttribKind_STRING_LENGTH_BYTE_SIZE: result = 93; break;
case SYMS_DwAttribKind_RANK: result = 94; break;
case SYMS_DwAttribKind_STR_OFFSETS_BASE: result = 95; break;
case SYMS_DwAttribKind_ADDR_BASE: result = 96; break;
case SYMS_DwAttribKind_RNGLISTS_BASE: result = 97; break;
case SYMS_DwAttribKind_DWO_NAME: result = 98; break;
case SYMS_DwAttribKind_REFERENCE: result = 99; break;
case SYMS_DwAttribKind_RVALUE_REFERENCE: result = 100; break;
case SYMS_DwAttribKind_MACROS: result = 101; break;
case SYMS_DwAttribKind_CALL_ALL_CALLS: result = 102; break;
case SYMS_DwAttribKind_CALL_ALL_SOURCE_CALLS: result = 103; break;
case SYMS_DwAttribKind_CALL_ALL_TAIL_CALLS: result = 104; break;
case SYMS_DwAttribKind_CALL_RETURN_PC: result = 105; break;
case SYMS_DwAttribKind_CALL_VALUE: result = 106; break;
case SYMS_DwAttribKind_CALL_ORIGIN: result = 107; break;
case SYMS_DwAttribKind_CALL_PARAMETER: result = 108; break;
case SYMS_DwAttribKind_CALL_PC: result = 109; break;
case SYMS_DwAttribKind_CALL_TAIL_CALL: result = 110; break;
case SYMS_DwAttribKind_CALL_TARGET: result = 111; break;
case SYMS_DwAttribKind_CALL_TARGET_CLOBBERED: result = 112; break;
case SYMS_DwAttribKind_CALL_DATA_LOCATION: result = 113; break;
case SYMS_DwAttribKind_CALL_DATA_VALUE: result = 114; break;
case SYMS_DwAttribKind_NORETURN: result = 115; break;
case SYMS_DwAttribKind_ALIGNMENT: result = 116; break;
case SYMS_DwAttribKind_EXPORT_SYMBOLS: result = 117; break;
case SYMS_DwAttribKind_DELETED: result = 118; break;
case SYMS_DwAttribKind_DEFAULTED: result = 119; break;
case SYMS_DwAttribKind_LOCLISTS_BASE: result = 120; break;
case SYMS_DwAttribKind_GNU_VECTOR: result = 121; break;
case SYMS_DwAttribKind_GNU_GUARDED_BY: result = 122; break;
case SYMS_DwAttribKind_GNU_PT_GUARDED_BY: result = 123; break;
case SYMS_DwAttribKind_GNU_GUARDED: result = 124; break;
case SYMS_DwAttribKind_GNU_PT_GUARDED: result = 125; break;
case SYMS_DwAttribKind_GNU_LOCKS_EXCLUDED: result = 126; break;
case SYMS_DwAttribKind_GNU_EXCLUSIVE_LOCKS_REQUIRED: result = 127; break;
case SYMS_DwAttribKind_GNU_SHARED_LOCKS_REQUIRED: result = 128; break;
case SYMS_DwAttribKind_GNU_ODR_SIGNATURE: result = 129; break;
case SYMS_DwAttribKind_GNU_TEMPLATE_NAME: result = 130; break;
case SYMS_DwAttribKind_GNU_CALL_SITE_VALUE: result = 131; break;
case SYMS_DwAttribKind_GNU_CALL_SITE_DATA_VALUE: result = 132; break;
case SYMS_DwAttribKind_GNU_CALL_SITE_TARGET: result = 133; break;
case SYMS_DwAttribKind_GNU_CALL_SITE_TARGET_CLOBBERED: result = 134; break;
case SYMS_DwAttribKind_GNU_TAIL_CALL: result = 135; break;
case SYMS_DwAttribKind_GNU_ALL_TAIL_CALL_SITES: result = 136; break;
case SYMS_DwAttribKind_GNU_ALL_CALL_SITES: result = 137; break;
case SYMS_DwAttribKind_GNU_ALL_SOURCE_CALL_SITES: result = 138; break;
case SYMS_DwAttribKind_GNU_MACROS: result = 139; break;
case SYMS_DwAttribKind_GNU_DELETED: result = 140; break;
case SYMS_DwAttribKind_GNU_DWO_NAME: result = 141; break;
case SYMS_DwAttribKind_GNU_DWO_ID: result = 142; break;
case SYMS_DwAttribKind_GNU_RANGES_BASE: result = 143; break;
case SYMS_DwAttribKind_GNU_ADDR_BASE: result = 144; break;
case SYMS_DwAttribKind_GNU_PUBNAMES: result = 145; break;
case SYMS_DwAttribKind_GNU_PUBTYPES: result = 146; break;
case SYMS_DwAttribKind_GNU_DISCRIMINATOR: result = 147; break;
case SYMS_DwAttribKind_GNU_LOCVIEWS: result = 148; break;
case SYMS_DwAttribKind_GNU_ENTRY_VIEW: result = 149; break;
case SYMS_DwAttribKind_VMS_RTNBEG_PD_ADDRESS: result = 150; break;
case SYMS_DwAttribKind_USE_GNAT_DESCRIPTIVE_TYPE: result = 151; break;
case SYMS_DwAttribKind_GNAT_DESCRIPTIVE_TYPE: result = 152; break;
case SYMS_DwAttribKind_GNU_NUMERATOR: result = 153; break;
case SYMS_DwAttribKind_GNU_DENOMINATOR: result = 154; break;
case SYMS_DwAttribKind_GNU_BIAS: result = 155; break;
case SYMS_DwAttribKind_UPC_THREADS_SCALED: result = 156; break;
case SYMS_DwAttribKind_PGI_LBASE: result = 157; break;
case SYMS_DwAttribKind_PGI_SOFFSET: result = 158; break;
case SYMS_DwAttribKind_PGI_LSTRIDE: result = 159; break;
case SYMS_DwAttribKind_LLVM_INCLUDE_PATH: result = 160; break;
case SYMS_DwAttribKind_LLVM_CONFIG_MACROS: result = 161; break;
case SYMS_DwAttribKind_LLVM_SYSROOT: result = 162; break;
case SYMS_DwAttribKind_LLVM_API_NOTES: result = 163; break;
case SYMS_DwAttribKind_LLVM_TAG_OFFSET: result = 164; break;
case SYMS_DwAttribKind_APPLE_OPTIMIZED: result = 165; break;
case SYMS_DwAttribKind_APPLE_FLAGS: result = 166; break;
case SYMS_DwAttribKind_APPLE_ISA: result = 167; break;
case SYMS_DwAttribKind_APPLE_BLOCK: result = 168; break;
case SYMS_DwAttribKind_APPLE_MAJOR_RUNTIME_VERS: result = 169; break;
case SYMS_DwAttribKind_APPLE_RUNTIME_CLASS: result = 170; break;
case SYMS_DwAttribKind_APPLE_OMIT_FRAME_PTR: result = 171; break;
case SYMS_DwAttribKind_APPLE_PROPERTY_NAME: result = 172; break;
case SYMS_DwAttribKind_APPLE_PROPERTY_GETTER: result = 173; break;
case SYMS_DwAttribKind_APPLE_PROPERTY_SETTER: result = 174; break;
case SYMS_DwAttribKind_APPLE_PROPERTY_ATTRIBUTE: result = 175; break;
case SYMS_DwAttribKind_APPLE_OBJC_COMPLETE_TYPE: result = 176; break;
case SYMS_DwAttribKind_APPLE_PROPERTY: result = 177; break;
case SYMS_DwAttribKind_APPLE_OBJ_DIRECT: result = 178; break;
case SYMS_DwAttribKind_APPLE_SDK: result = 179; break;
case SYMS_DwAttribKind_MIPS_FDE: result = 180; break;
case SYMS_DwAttribKind_MIPS_LOOP_BEGIN: result = 181; break;
case SYMS_DwAttribKind_MIPS_TAIL_LOOP_BEGIN: result = 182; break;
case SYMS_DwAttribKind_MIPS_EPILOG_BEGIN: result = 183; break;
case SYMS_DwAttribKind_MIPS_LOOP_UNROLL_FACTOR: result = 184; break;
case SYMS_DwAttribKind_MIPS_SOFTWARE_PIPELINE_DEPTH: result = 185; break;
case SYMS_DwAttribKind_MIPS_LINKAGE_NAME: result = 186; break;
case SYMS_DwAttribKind_MIPS_STRIDE: result = 187; break;
case SYMS_DwAttribKind_MIPS_ABSTRACT_NAME: result = 188; break;
case SYMS_DwAttribKind_MIPS_CLONE_ORIGIN: result = 189; break;
case SYMS_DwAttribKind_MIPS_HAS_INLINES: result = 190; break;
case SYMS_DwAttribKind_MIPS_STRIDE_BYTE: result = 191; break;
case SYMS_DwAttribKind_MIPS_STRIDE_ELEM: result = 192; break;
case SYMS_DwAttribKind_MIPS_PTR_DOPETYPE: result = 193; break;
case SYMS_DwAttribKind_MIPS_ALLOCATABLE_DOPETYPE: result = 194; break;
case SYMS_DwAttribKind_MIPS_ASSUMED_SHAPE_DOPETYPE: result = 195; break;
case SYMS_DwAttribKind_MIPS_ASSUMED_SIZE: result = 196; break;
case SYMS_DwAttribKind_LO_USER: result = 197; break;
case SYMS_DwAttribKind_HI_USER: result = 198; break;
}
return(result);
}
SYMS_API SYMS_U64
syms_enum_index_from_dw_attrib_type_encoding(SYMS_U64 v){
SYMS_U64 result = 0;
switch ((SYMS_U64)v){
default: break;
case SYMS_DwAttribTypeEncoding_Null: result = 0; break;
case SYMS_DwAttribTypeEncoding_ADDRESS: result = 1; break;
case SYMS_DwAttribTypeEncoding_BOOLEAN: result = 2; break;
case SYMS_DwAttribTypeEncoding_COMPLEX_FLOAT: result = 3; break;
case SYMS_DwAttribTypeEncoding_FLOAT: result = 4; break;
case SYMS_DwAttribTypeEncoding_SIGNED: result = 5; break;
case SYMS_DwAttribTypeEncoding_SIGNED_CHAR: result = 6; break;
case SYMS_DwAttribTypeEncoding_UNSIGNED: result = 7; break;
case SYMS_DwAttribTypeEncoding_UNSIGNED_CHAR: result = 8; break;
case SYMS_DwAttribTypeEncoding_IMAGINARY_FLOAT: result = 9; break;
case SYMS_DwAttribTypeEncoding_PACKED_DECIMAL: result = 10; break;
case SYMS_DwAttribTypeEncoding_NUMERIC_STRING: result = 11; break;
case SYMS_DwAttribTypeEncoding_EDITED: result = 12; break;
case SYMS_DwAttribTypeEncoding_SIGNED_FIXED: result = 13; break;
case SYMS_DwAttribTypeEncoding_UNSIGNED_FIXED: result = 14; break;
case SYMS_DwAttribTypeEncoding_DECIMAL_FLOAT: result = 15; break;
case SYMS_DwAttribTypeEncoding_UTF: result = 16; break;
case SYMS_DwAttribTypeEncoding_UCS: result = 17; break;
case SYMS_DwAttribTypeEncoding_ASCII: result = 18; break;
}
return(result);
}
// syms_enum_index_from_dw_calling_convention - skipped identity mapping
// syms_enum_index_from_dw_access - skipped identity mapping
// syms_enum_index_from_dw_virtuality - skipped identity mapping
// syms_enum_index_from_dw_rng_list_entry_kind - skipped identity mapping
// syms_enum_index_from_dw_loc_list_entry_kind - skipped identity mapping

//~ generated from code at syms/metaprogram/syms_metaprogram_serial.c:1322
SYMS_SerialValue _syms_serial_members_for_SYMS_DwMode[] = {
{ { (SYMS_U8*)"Null", 4 }, (SYMS_U64)SYMS_DwMode_Null },
{ { (SYMS_U8*)"32Bit", 5 }, (SYMS_U64)SYMS_DwMode_32Bit },
{ { (SYMS_U8*)"64Bit", 5 }, (SYMS_U64)SYMS_DwMode_64Bit },
};
SYMS_SerialValue _syms_serial_members_for_SYMS_DwVersion[] = {
{ { (SYMS_U8*)"Null", 4 }, (SYMS_U64)SYMS_DwVersion_Null },
{ { (SYMS_U8*)"V1", 2 }, (SYMS_U64)SYMS_DwVersion_V1 },
{ { (SYMS_U8*)"V2", 2 }, (SYMS_U64)SYMS_DwVersion_V2 },
{ { (SYMS_U8*)"V3", 2 }, (SYMS_U64)SYMS_DwVersion_V3 },
{ { (SYMS_U8*)"V4", 2 }, (SYMS_U64)SYMS_DwVersion_V4 },
{ { (SYMS_U8*)"V5", 2 }, (SYMS_U64)SYMS_DwVersion_V5 },
{ { (SYMS_U8*)"LastVersion", 11 }, (SYMS_U64)SYMS_DwVersion_LastVersion },
};
SYMS_SerialValue _syms_serial_members_for_SYMS_DwSectionKind[] = {
{ { (SYMS_U8*)"Null", 4 }, (SYMS_U64)SYMS_DwSectionKind_Null },
{ { (SYMS_U8*)"Abbrev", 6 }, (SYMS_U64)SYMS_DwSectionKind_Abbrev },
{ { (SYMS_U8*)"ARanges", 7 }, (SYMS_U64)SYMS_DwSectionKind_ARanges },
{ { (SYMS_U8*)"Frame", 5 }, (SYMS_U64)SYMS_DwSectionKind_Frame },
{ { (SYMS_U8*)"Info", 4 }, (SYMS_U64)SYMS_DwSectionKind_Info },
{ { (SYMS_U8*)"Line", 4 }, (SYMS_U64)SYMS_DwSectionKind_Line },
{ { (SYMS_U8*)"Loc", 3 }, (SYMS_U64)SYMS_DwSectionKind_Loc },
{ { (SYMS_U8*)"MacInfo", 7 }, (SYMS_U64)SYMS_DwSectionKind_MacInfo },
{ { (SYMS_U8*)"PubNames", 8 }, (SYMS_U64)SYMS_DwSectionKind_PubNames },
{ { (SYMS_U8*)"PubTypes", 8 }, (SYMS_U64)SYMS_DwSectionKind_PubTypes },
{ { (SYMS_U8*)"Ranges", 6 }, (SYMS_U64)SYMS_DwSectionKind_Ranges },
{ { (SYMS_U8*)"Str", 3 }, (SYMS_U64)SYMS_DwSectionKind_Str },
{ { (SYMS_U8*)"Addr", 4 }, (SYMS_U64)SYMS_DwSectionKind_Addr },
{ { (SYMS_U8*)"LocLists", 8 }, (SYMS_U64)SYMS_DwSectionKind_LocLists },
{ { (SYMS_U8*)"RngLists", 8 }, (SYMS_U64)SYMS_DwSectionKind_RngLists },
{ { (SYMS_U8*)"StrOffsets", 10 }, (SYMS_U64)SYMS_DwSectionKind_StrOffsets },
{ { (SYMS_U8*)"LineStr", 7 }, (SYMS_U64)SYMS_DwSectionKind_LineStr },
{ { (SYMS_U8*)"Names", 5 }, (SYMS_U64)SYMS_DwSectionKind_Names },
};
SYMS_SerialValue _syms_serial_members_for_SYMS_DwLanguage[] = {
{ { (SYMS_U8*)"NULL", 4 }, (SYMS_U64)SYMS_DwLanguage_NULL },
{ { (SYMS_U8*)"C89", 3 }, (SYMS_U64)SYMS_DwLanguage_C89 },
{ { (SYMS_U8*)"C", 1 }, (SYMS_U64)SYMS_DwLanguage_C },
{ { (SYMS_U8*)"ADA83", 5 }, (SYMS_U64)SYMS_DwLanguage_Ada83 },
{ { (SYMS_U8*)"C++", 3 }, (SYMS_U64)SYMS_DwLanguage_CPlusPlus },
{ { (SYMS_U8*)"COBOL74", 7 }, (SYMS_U64)SYMS_DwLanguage_Cobol74 },
{ { (SYMS_U8*)"COBOL85", 7 }, (SYMS_U64)SYMS_DwLanguage_Cobol85 },
{ { (SYMS_U8*)"FORTRAN77", 9 }, (SYMS_U64)SYMS_DwLanguage_Fortran77 },
{ { (SYMS_U8*)"FORTRAN90", 9 }, (SYMS_U64)SYMS_DwLanguage_Fortran90 },
{ { (SYMS_U8*)"PASCAL83", 8 }, (SYMS_U64)SYMS_DwLanguage_Pascal83 },
{ { (SYMS_U8*)"MODULA2", 7 }, (SYMS_U64)SYMS_DwLanguage_Modula2 },
{ { (SYMS_U8*)"JAVA", 4 }, (SYMS_U64)SYMS_DwLanguage_Java },
{ { (SYMS_U8*)"C99", 3 }, (SYMS_U64)SYMS_DwLanguage_C99 },
{ { (SYMS_U8*)"ADA95", 5 }, (SYMS_U64)SYMS_DwLanguage_Ada95 },
{ { (SYMS_U8*)"FORTRAN95", 9 }, (SYMS_U64)SYMS_DwLanguage_Fortran95 },
{ { (SYMS_U8*)"PLI", 3 }, (SYMS_U64)SYMS_DwLanguage_PLI },
{ { (SYMS_U8*)"Objective-C", 11 }, (SYMS_U64)SYMS_DwLanguage_ObjectiveC },
{ { (SYMS_U8*)"Objective-C++", 13 }, (SYMS_U64)SYMS_DwLanguage_ObjectiveCPlusPlus },
{ { (SYMS_U8*)"UPC", 3 }, (SYMS_U64)SYMS_DwLanguage_UPC },
{ { (SYMS_U8*)"D", 1 }, (SYMS_U64)SYMS_DwLanguage_D },
{ { (SYMS_U8*)"PYTHON", 6 }, (SYMS_U64)SYMS_DwLanguage_Python },
{ { (SYMS_U8*)"OPENCL", 6 }, (SYMS_U64)SYMS_DwLanguage_OpenCL },
{ { (SYMS_U8*)"GO", 2 }, (SYMS_U64)SYMS_DwLanguage_Go },
{ { (SYMS_U8*)"MODULA3", 7 }, (SYMS_U64)SYMS_DwLanguage_Modula3 },
{ { (SYMS_U8*)"HASKELL", 7 }, (SYMS_U64)SYMS_DwLanguage_Haskell },
{ { (SYMS_U8*)"C++03", 5 }, (SYMS_U64)SYMS_DwLanguage_CPlusPlus03 },
{ { (SYMS_U8*)"C++11", 5 }, (SYMS_U64)SYMS_DwLanguage_CPlusPlus11 },
{ { (SYMS_U8*)"OCAML", 5 }, (SYMS_U64)SYMS_DwLanguage_OCaml },
{ { (SYMS_U8*)"RUST", 4 }, (SYMS_U64)SYMS_DwLanguage_Rust },
{ { (SYMS_U8*)"C11", 3 }, (SYMS_U64)SYMS_DwLanguage_C11 },
{ { (SYMS_U8*)"SWIFT", 5 }, (SYMS_U64)SYMS_DwLanguage_Swift },
{ { (SYMS_U8*)"JULIA", 5 }, (SYMS_U64)SYMS_DwLanguage_Julia },
{ { (SYMS_U8*)"DYLAN", 5 }, (SYMS_U64)SYMS_DwLanguage_Dylan },
{ { (SYMS_U8*)"C++14", 5 }, (SYMS_U64)SYMS_DwLanguage_CPlusPlus14 },
{ { (SYMS_U8*)"FORTRAN03", 9 }, (SYMS_U64)SYMS_DwLanguage_Fortran03 },
{ { (SYMS_U8*)"FORTRAN08", 9 }, (SYMS_U64)SYMS_DwLanguage_Fortran08 },
{ { (SYMS_U8*)"RENDERSCRIPT", 12 }, (SYMS_U64)SYMS_DwLanguage_RenderScript },
{ { (SYMS_U8*)"BLISS", 5 }, (SYMS_U64)SYMS_DwLanguage_BLISS },
{ { (SYMS_U8*)"MIPS Assembler", 14 }, (SYMS_U64)SYMS_DwLanguage_MIPS_ASSEMBLER },
{ { (SYMS_U8*)"Google Render Script", 20 }, (SYMS_U64)SYMS_DwLanguage_GOOGLE_RENDER_SCRIPT },
{ { (SYMS_U8*)"Sun Assembler", 13 }, (SYMS_U64)SYMS_DwLanguage_SUN_ASSEMBLER },
{ { (SYMS_U8*)"Borland Delphi", 14 }, (SYMS_U64)SYMS_DwLanguage_BORLAND_DELPHI },
{ { (SYMS_U8*)"LO_USER", 7 }, (SYMS_U64)SYMS_DwLanguage_LO_USER },
{ { (SYMS_U8*)"HI_USER", 7 }, (SYMS_U64)SYMS_DwLanguage_HI_USER },
};
SYMS_SerialValue _syms_serial_members_for_SYMS_DwStdOpcode[] = {
{ { (SYMS_U8*)"EXTENDED_OPCODE", 15 }, (SYMS_U64)SYMS_DwStdOpcode_EXTENDED_OPCODE },
{ { (SYMS_U8*)"COPY", 4 }, (SYMS_U64)SYMS_DwStdOpcode_COPY },
{ { (SYMS_U8*)"ADVANCE_PC", 10 }, (SYMS_U64)SYMS_DwStdOpcode_ADVANCE_PC },
{ { (SYMS_U8*)"ADVANCE_LINE", 12 }, (SYMS_U64)SYMS_DwStdOpcode_ADVANCE_LINE },
{ { (SYMS_U8*)"SET_FILE", 8 }, (SYMS_U64)SYMS_DwStdOpcode_SET_FILE },
{ { (SYMS_U8*)"SET_COLUMN", 10 }, (SYMS_U64)SYMS_DwStdOpcode_SET_COLUMN },
{ { (SYMS_U8*)"NEGATE_STMT", 11 }, (SYMS_U64)SYMS_DwStdOpcode_NEGATE_STMT },
{ { (SYMS_U8*)"SET_BASIC_BLOCK", 15 }, (SYMS_U64)SYMS_DwStdOpcode_SET_BASIC_BLOCK },
{ { (SYMS_U8*)"CONST_ADD_PC", 12 }, (SYMS_U64)SYMS_DwStdOpcode_CONST_ADD_PC },
{ { (SYMS_U8*)"FIXED_ADVANCE_PC", 16 }, (SYMS_U64)SYMS_DwStdOpcode_FIXED_ADVANCE_PC },
{ { (SYMS_U8*)"SET_PROLOGUE_END", 16 }, (SYMS_U64)SYMS_DwStdOpcode_SET_PROLOGUE_END },
{ { (SYMS_U8*)"SET_EPILOGUE_BEGIN", 18 }, (SYMS_U64)SYMS_DwStdOpcode_SET_EPILOGUE_BEGIN },
{ { (SYMS_U8*)"SET_ISA", 7 }, (SYMS_U64)SYMS_DwStdOpcode_SET_ISA },
};
SYMS_SerialValue _syms_serial_members_for_SYMS_DwExtOpcode[] = {
{ { (SYMS_U8*)"UNDEFINED", 9 }, (SYMS_U64)SYMS_DwExtOpcode_UNDEFINED },
{ { (SYMS_U8*)"END_SEQUENCE", 12 }, (SYMS_U64)SYMS_DwExtOpcode_END_SEQUENCE },
{ { (SYMS_U8*)"SET_ADDRESS", 11 }, (SYMS_U64)SYMS_DwExtOpcode_SET_ADDRESS },
{ { (SYMS_U8*)"DEFINE_FILE", 11 }, (SYMS_U64)SYMS_DwExtOpcode_DEFINE_FILE },
{ { (SYMS_U8*)"SET_DISCRIMINATOR", 17 }, (SYMS_U64)SYMS_DwExtOpcode_SET_DISCRIMINATOR },
{ { (SYMS_U8*)"LO_USER", 7 }, (SYMS_U64)SYMS_DwExtOpcode_LO_USER },
{ { (SYMS_U8*)"HI_USER", 7 }, (SYMS_U64)SYMS_DwExtOpcode_HI_USER },
};
SYMS_SerialValue _syms_serial_members_for_SYMS_DwNameCase[] = {
{ { (SYMS_U8*)"Sensitive", 9 }, (SYMS_U64)SYMS_DwNameCase_Sensitive },
{ { (SYMS_U8*)"Upper", 5 }, (SYMS_U64)SYMS_DwNameCase_Upper },
{ { (SYMS_U8*)"Lower", 5 }, (SYMS_U64)SYMS_DwNameCase_Lower },
{ { (SYMS_U8*)"Insensitive", 11 }, (SYMS_U64)SYMS_DwNameCase_Insensitive },
};
SYMS_SerialValue _syms_serial_members_for_SYMS_DwTagKind[] = {
{ { (SYMS_U8*)"NULL", 4 }, (SYMS_U64)SYMS_DwTagKind_NULL },
{ { (SYMS_U8*)"ARRAY_TYPE", 10 }, (SYMS_U64)SYMS_DwTagKind_ARRAY_TYPE },
{ { (SYMS_U8*)"CLASS_TYPE", 10 }, (SYMS_U64)SYMS_DwTagKind_CLASS_TYPE },
{ { (SYMS_U8*)"ENTRY_POINT", 11 }, (SYMS_U64)SYMS_DwTagKind_ENTRY_POINT },
{ { (SYMS_U8*)"ENUMERATION_TYPE", 16 }, (SYMS_U64)SYMS_DwTagKind_ENUMERATION_TYPE },
{ { (SYMS_U8*)"FORMAL_PARAMETER", 16 }, (SYMS_U64)SYMS_DwTagKind_FORMAL_PARAMETER },
{ { (SYMS_U8*)"IMPORTED_DECLARATION", 20 }, (SYMS_U64)SYMS_DwTagKind_IMPORTED_DECLARATION },
{ { (SYMS_U8*)"LABEL", 5 }, (SYMS_U64)SYMS_DwTagKind_LABEL },
{ { (SYMS_U8*)"LEXICAL_BLOCK", 13 }, (SYMS_U64)SYMS_DwTagKind_LEXICAL_BLOCK },
{ { (SYMS_U8*)"MEMBER", 6 }, (SYMS_U64)SYMS_DwTagKind_MEMBER },
{ { (SYMS_U8*)"POINTER_TYPE", 12 }, (SYMS_U64)SYMS_DwTagKind_POINTER_TYPE },
{ { (SYMS_U8*)"REFERENCE_TYPE", 14 }, (SYMS_U64)SYMS_DwTagKind_REFERENCE_TYPE },
{ { (SYMS_U8*)"COMPILE_UNIT", 12 }, (SYMS_U64)SYMS_DwTagKind_COMPILE_UNIT },
{ { (SYMS_U8*)"STRING_TYPE", 11 }, (SYMS_U64)SYMS_DwTagKind_STRING_TYPE },
{ { (SYMS_U8*)"STRUCTURE_TYPE", 14 }, (SYMS_U64)SYMS_DwTagKind_STRUCTURE_TYPE },
{ { (SYMS_U8*)"SUBROUTINE_TYPE", 15 }, (SYMS_U64)SYMS_DwTagKind_SUBROUTINE_TYPE },
{ { (SYMS_U8*)"TYPEDEF", 7 }, (SYMS_U64)SYMS_DwTagKind_TYPEDEF },
{ { (SYMS_U8*)"UNION_TYPE", 10 }, (SYMS_U64)SYMS_DwTagKind_UNION_TYPE },
{ { (SYMS_U8*)"UNSPECIFIED_PARAMETERS", 22 }, (SYMS_U64)SYMS_DwTagKind_UNSPECIFIED_PARAMETERS },
{ { (SYMS_U8*)"VARIANT", 7 }, (SYMS_U64)SYMS_DwTagKind_VARIANT },
{ { (SYMS_U8*)"COMMON_BLOCK", 12 }, (SYMS_U64)SYMS_DwTagKind_COMMON_BLOCK },
{ { (SYMS_U8*)"COMMON_INCLUSION", 16 }, (SYMS_U64)SYMS_DwTagKind_COMMON_INCLUSION },
{ { (SYMS_U8*)"INHERITANCE", 11 }, (SYMS_U64)SYMS_DwTagKind_INHERITANCE },
{ { (SYMS_U8*)"INLINED_SUBROUTINE", 18 }, (SYMS_U64)SYMS_DwTagKind_INLINED_SUBROUTINE },
{ { (SYMS_U8*)"MODULE", 6 }, (SYMS_U64)SYMS_DwTagKind_MODULE },
{ { (SYMS_U8*)"PTR_TO_MEMBER_TYPE", 18 }, (SYMS_U64)SYMS_DwTagKind_PTR_TO_MEMBER_TYPE },
{ { (SYMS_U8*)"SET_TYPE", 8 }, (SYMS_U64)SYMS_DwTagKind_SET_TYPE },
{ { (SYMS_U8*)"SUBRANGE_TYPE", 13 }, (SYMS_U64)SYMS_DwTagKind_SUBRANGE_TYPE },
{ { (SYMS_U8*)"WITH_STMT", 9 }, (SYMS_U64)SYMS_DwTagKind_WITH_STMT },
{ { (SYMS_U8*)"ACCESS_DECLARATION", 18 }, (SYMS_U64)SYMS_DwTagKind_ACCESS_DECLARATION },
{ { (SYMS_U8*)"BASE_TYPE", 9 }, (SYMS_U64)SYMS_DwTagKind_BASE_TYPE },
{ { (SYMS_U8*)"CATCH_BLOCK", 11 }, (SYMS_U64)SYMS_DwTagKind_CATCH_BLOCK },
{ { (SYMS_U8*)"CONST_TYPE", 10 }, (SYMS_U64)SYMS_DwTagKind_CONST_TYPE },
{ { (SYMS_U8*)"CONSTANT", 8 }, (SYMS_U64)SYMS_DwTagKind_CONSTANT },
{ { (SYMS_U8*)"ENUMERATOR", 10 }, (SYMS_U64)SYMS_DwTagKind_ENUMERATOR },
{ { (SYMS_U8*)"FILE_TYPE", 9 }, (SYMS_U64)SYMS_DwTagKind_FILE_TYPE },
{ { (SYMS_U8*)"FRIEND", 6 }, (SYMS_U64)SYMS_DwTagKind_FRIEND },
{ { (SYMS_U8*)"NAMELIST", 8 }, (SYMS_U64)SYMS_DwTagKind_NAMELIST },
{ { (SYMS_U8*)"NAMELIST_ITEM", 13 }, (SYMS_U64)SYMS_DwTagKind_NAMELIST_ITEM },
{ { (SYMS_U8*)"PACKED_TYPE", 11 }, (SYMS_U64)SYMS_DwTagKind_PACKED_TYPE },
{ { (SYMS_U8*)"SUBPROGRAM", 10 }, (SYMS_U64)SYMS_DwTagKind_SUBPROGRAM },
{ { (SYMS_U8*)"TEMPLATE_TYPE_PARAMETER", 23 }, (SYMS_U64)SYMS_DwTagKind_TEMPLATE_TYPE_PARAMETER },
{ { (SYMS_U8*)"TEMPLATE_VALUE_PARAMETER", 24 }, (SYMS_U64)SYMS_DwTagKind_TEMPLATE_VALUE_PARAMETER },
{ { (SYMS_U8*)"THROWN_TYPE", 11 }, (SYMS_U64)SYMS_DwTagKind_THROWN_TYPE },
{ { (SYMS_U8*)"TRY_BLOCK", 9 }, (SYMS_U64)SYMS_DwTagKind_TRY_BLOCK },
{ { (SYMS_U8*)"VARIANT_PART", 12 }, (SYMS_U64)SYMS_DwTagKind_VARIANT_PART },
{ { (SYMS_U8*)"VARIABLE", 8 }, (SYMS_U64)SYMS_DwTagKind_VARIABLE },
{ { (SYMS_U8*)"VOLATILE_TYPE", 13 }, (SYMS_U64)SYMS_DwTagKind_VOLATILE_TYPE },
{ { (SYMS_U8*)"DWARF_PROCEDURE", 15 }, (SYMS_U64)SYMS_DwTagKind_DWARF_PROCEDURE },
{ { (SYMS_U8*)"RESTRICT_TYPE", 13 }, (SYMS_U64)SYMS_DwTagKind_RESTRICT_TYPE },
{ { (SYMS_U8*)"INTERFACE_TYPE", 14 }, (SYMS_U64)SYMS_DwTagKind_INTERFACE_TYPE },
{ { (SYMS_U8*)"NAMESPACE", 9 }, (SYMS_U64)SYMS_DwTagKind_NAMESPACE },
{ { (SYMS_U8*)"IMPORTED_MODULE", 15 }, (SYMS_U64)SYMS_DwTagKind_IMPORTED_MODULE },
{ { (SYMS_U8*)"UNSPECIFIED_TYPE", 16 }, (SYMS_U64)SYMS_DwTagKind_UNSPECIFIED_TYPE },
{ { (SYMS_U8*)"PARTIAL_UNIT", 12 }, (SYMS_U64)SYMS_DwTagKind_PARTIAL_UNIT },
{ { (SYMS_U8*)"IMPORTED_UNIT", 13 }, (SYMS_U64)SYMS_DwTagKind_IMPORTED_UNIT },
{ { (SYMS_U8*)"CONDITION", 9 }, (SYMS_U64)SYMS_DwTagKind_CONDITION },
{ { (SYMS_U8*)"SHARED_TYPE", 11 }, (SYMS_U64)SYMS_DwTagKind_SHARED_TYPE },
{ { (SYMS_U8*)"TYPE_UNIT", 9 }, (SYMS_U64)SYMS_DwTagKind_TYPE_UNIT },
{ { (SYMS_U8*)"RVALUE_REFERENCE_TYPE", 21 }, (SYMS_U64)SYMS_DwTagKind_RVALUE_REFERENCE_TYPE },
{ { (SYMS_U8*)"TEMPLATE_ALIAS", 14 }, (SYMS_U64)SYMS_DwTagKind_TEMPLATE_ALIAS },
{ { (SYMS_U8*)"COARRAY_TYPE", 12 }, (SYMS_U64)SYMS_DwTagKind_COARRAY_TYPE },
{ { (SYMS_U8*)"GENERIC_SUBRANGE", 16 }, (SYMS_U64)SYMS_DwTagKind_GENERIC_SUBRANGE },
{ { (SYMS_U8*)"DYNAMIC_TYPE", 12 }, (SYMS_U64)SYMS_DwTagKind_DYNAMIC_TYPE },
{ { (SYMS_U8*)"ATOMIC_TYPE", 11 }, (SYMS_U64)SYMS_DwTagKind_ATOMIC_TYPE },
{ { (SYMS_U8*)"CALL_SITE", 9 }, (SYMS_U64)SYMS_DwTagKind_CALL_SITE },
{ { (SYMS_U8*)"CALL_SITE_PARAMETER", 19 }, (SYMS_U64)SYMS_DwTagKind_CALL_SITE_PARAMETER },
{ { (SYMS_U8*)"SKELETON_UNIT", 13 }, (SYMS_U64)SYMS_DwTagKind_SKELETON_UNIT },
{ { (SYMS_U8*)"IMMUTABLE_TYPE", 14 }, (SYMS_U64)SYMS_DwTagKind_IMMUTABLE_TYPE },
{ { (SYMS_U8*)"GNU_CALL_SITE", 13 }, (SYMS_U64)SYMS_DwTagKind_GNU_CALL_SITE },
{ { (SYMS_U8*)"GNU_CALL_SITE_PARAMETER", 23 }, (SYMS_U64)SYMS_DwTagKind_GNU_CALL_SITE_PARAMETER },
{ { (SYMS_U8*)"LO_USER", 7 }, (SYMS_U64)SYMS_DwTagKind_LO_USER },
{ { (SYMS_U8*)"HI_USER", 7 }, (SYMS_U64)SYMS_DwTagKind_HI_USER },
};
SYMS_SerialFlag _syms_serial_members_for_SYMS_DwAttribClass[] = {
{ { (SYMS_U8*)"ADDRESS", 7 }, &_syms_serial_type_SYMS_U32, 0x1, 0 },
{ { (SYMS_U8*)"BLOCK", 5 }, &_syms_serial_type_SYMS_U32, 0x1, 1 },
{ { (SYMS_U8*)"CONST", 5 }, &_syms_serial_type_SYMS_U32, 0x1, 2 },
{ { (SYMS_U8*)"EXPRLOC", 7 }, &_syms_serial_type_SYMS_U32, 0x1, 3 },
{ { (SYMS_U8*)"FLAG", 4 }, &_syms_serial_type_SYMS_U32, 0x1, 4 },
{ { (SYMS_U8*)"LINEPTR", 7 }, &_syms_serial_type_SYMS_U32, 0x1, 5 },
{ { (SYMS_U8*)"LOCLIST", 7 }, &_syms_serial_type_SYMS_U32, 0x1, 6 },
{ { (SYMS_U8*)"LOCLISTPTR", 10 }, &_syms_serial_type_SYMS_U32, 0x1, 7 },
{ { (SYMS_U8*)"MACPTR", 6 }, &_syms_serial_type_SYMS_U32, 0x1, 8 },
{ { (SYMS_U8*)"RNGLISTPTR", 10 }, &_syms_serial_type_SYMS_U32, 0x1, 9 },
{ { (SYMS_U8*)"RNGLIST", 7 }, &_syms_serial_type_SYMS_U32, 0x1, 10 },
{ { (SYMS_U8*)"REFERENCE", 9 }, &_syms_serial_type_SYMS_U32, 0x1, 11 },
{ { (SYMS_U8*)"STRING", 6 }, &_syms_serial_type_SYMS_U32, 0x1, 12 },
{ { (SYMS_U8*)"STROFFSETSPTR", 13 }, &_syms_serial_type_SYMS_U32, 0x1, 13 },
{ { (SYMS_U8*)"ADDRPTR", 7 }, &_syms_serial_type_SYMS_U32, 0x1, 14 },
{ { (SYMS_U8*)"UNDEFINED", 9 }, &_syms_serial_type_SYMS_U32, 0x1, 15 },
};
SYMS_SerialValue _syms_serial_members_for_SYMS_DwFormKind[] = {
{ { (SYMS_U8*)"ADDR", 4 }, (SYMS_U64)SYMS_DwFormKind_ADDR },
{ { (SYMS_U8*)"BLOCK2", 6 }, (SYMS_U64)SYMS_DwFormKind_BLOCK2 },
{ { (SYMS_U8*)"BLOCK4", 6 }, (SYMS_U64)SYMS_DwFormKind_BLOCK4 },
{ { (SYMS_U8*)"DATA2", 5 }, (SYMS_U64)SYMS_DwFormKind_DATA2 },
{ { (SYMS_U8*)"DATA4", 5 }, (SYMS_U64)SYMS_DwFormKind_DATA4 },
{ { (SYMS_U8*)"DATA8", 5 }, (SYMS_U64)SYMS_DwFormKind_DATA8 },
{ { (SYMS_U8*)"STRING", 6 }, (SYMS_U64)SYMS_DwFormKind_STRING },
{ { (SYMS_U8*)"BLOCK", 5 }, (SYMS_U64)SYMS_DwFormKind_BLOCK },
{ { (SYMS_U8*)"BLOCK1", 6 }, (SYMS_U64)SYMS_DwFormKind_BLOCK1 },
{ { (SYMS_U8*)"DATA1", 5 }, (SYMS_U64)SYMS_DwFormKind_DATA1 },
{ { (SYMS_U8*)"FLAG", 4 }, (SYMS_U64)SYMS_DwFormKind_FLAG },
{ { (SYMS_U8*)"SDATA", 5 }, (SYMS_U64)SYMS_DwFormKind_SDATA },
{ { (SYMS_U8*)"STRP", 4 }, (SYMS_U64)SYMS_DwFormKind_STRP },
{ { (SYMS_U8*)"UDATA", 5 }, (SYMS_U64)SYMS_DwFormKind_UDATA },
{ { (SYMS_U8*)"REF_ADDR", 8 }, (SYMS_U64)SYMS_DwFormKind_REF_ADDR },
{ { (SYMS_U8*)"REF1", 4 }, (SYMS_U64)SYMS_DwFormKind_REF1 },
{ { (SYMS_U8*)"REF2", 4 }, (SYMS_U64)SYMS_DwFormKind_REF2 },
{ { (SYMS_U8*)"REF4", 4 }, (SYMS_U64)SYMS_DwFormKind_REF4 },
{ { (SYMS_U8*)"REF8", 4 }, (SYMS_U64)SYMS_DwFormKind_REF8 },
{ { (SYMS_U8*)"REF_UDATA", 9 }, (SYMS_U64)SYMS_DwFormKind_REF_UDATA },
{ { (SYMS_U8*)"INDIRECT", 8 }, (SYMS_U64)SYMS_DwFormKind_INDIRECT },
{ { (SYMS_U8*)"SEC_OFFSET", 10 }, (SYMS_U64)SYMS_DwFormKind_SEC_OFFSET },
{ { (SYMS_U8*)"EXPRLOC", 7 }, (SYMS_U64)SYMS_DwFormKind_EXPRLOC },
{ { (SYMS_U8*)"FLAG_PRESENT", 12 }, (SYMS_U64)SYMS_DwFormKind_FLAG_PRESENT },
{ { (SYMS_U8*)"REF_SIG8", 8 }, (SYMS_U64)SYMS_DwFormKind_REF_SIG8 },
{ { (SYMS_U8*)"STRX", 4 }, (SYMS_U64)SYMS_DwFormKind_STRX },
{ { (SYMS_U8*)"ADDRX", 5 }, (SYMS_U64)SYMS_DwFormKind_ADDRX },
{ { (SYMS_U8*)"REF_SUP4", 8 }, (SYMS_U64)SYMS_DwFormKind_REF_SUP4 },
{ { (SYMS_U8*)"STRP_SUP", 8 }, (SYMS_U64)SYMS_DwFormKind_STRP_SUP },
{ { (SYMS_U8*)"DATA16", 6 }, (SYMS_U64)SYMS_DwFormKind_DATA16 },
{ { (SYMS_U8*)"LINE_STRP", 9 }, (SYMS_U64)SYMS_DwFormKind_LINE_STRP },
{ { (SYMS_U8*)"IMPLICIT_CONST", 14 }, (SYMS_U64)SYMS_DwFormKind_IMPLICIT_CONST },
{ { (SYMS_U8*)"LOCLISTX", 8 }, (SYMS_U64)SYMS_DwFormKind_LOCLISTX },
{ { (SYMS_U8*)"RNGLISTX", 8 }, (SYMS_U64)SYMS_DwFormKind_RNGLISTX },
{ { (SYMS_U8*)"REF_SUP8", 8 }, (SYMS_U64)SYMS_DwFormKind_REF_SUP8 },
{ { (SYMS_U8*)"STRX1", 5 }, (SYMS_U64)SYMS_DwFormKind_STRX1 },
{ { (SYMS_U8*)"STRX2", 5 }, (SYMS_U64)SYMS_DwFormKind_STRX2 },
{ { (SYMS_U8*)"STRX3", 5 }, (SYMS_U64)SYMS_DwFormKind_STRX3 },
{ { (SYMS_U8*)"STRX4", 5 }, (SYMS_U64)SYMS_DwFormKind_STRX4 },
{ { (SYMS_U8*)"ADDRX1", 6 }, (SYMS_U64)SYMS_DwFormKind_ADDRX1 },
{ { (SYMS_U8*)"ADDRX2", 6 }, (SYMS_U64)SYMS_DwFormKind_ADDRX2 },
{ { (SYMS_U8*)"ADDRX3", 6 }, (SYMS_U64)SYMS_DwFormKind_ADDRX3 },
{ { (SYMS_U8*)"ADDRX4", 6 }, (SYMS_U64)SYMS_DwFormKind_ADDRX4 },
{ { (SYMS_U8*)"INVALID", 7 }, (SYMS_U64)SYMS_DwFormKind_INVALID },
};
SYMS_SerialValue _syms_serial_members_for_SYMS_DwAttribKind[] = {
{ { (SYMS_U8*)"SIBLING", 7 }, (SYMS_U64)SYMS_DwAttribKind_SIBLING },
{ { (SYMS_U8*)"LOCATION", 8 }, (SYMS_U64)SYMS_DwAttribKind_LOCATION },
{ { (SYMS_U8*)"NAME", 4 }, (SYMS_U64)SYMS_DwAttribKind_NAME },
{ { (SYMS_U8*)"ORDERING", 8 }, (SYMS_U64)SYMS_DwAttribKind_ORDERING },
{ { (SYMS_U8*)"BYTE_SIZE", 9 }, (SYMS_U64)SYMS_DwAttribKind_BYTE_SIZE },
{ { (SYMS_U8*)"BIT_OFFSET", 10 }, (SYMS_U64)SYMS_DwAttribKind_BIT_OFFSET },
{ { (SYMS_U8*)"BIT_SIZE", 8 }, (SYMS_U64)SYMS_DwAttribKind_BIT_SIZE },
{ { (SYMS_U8*)"STMT_LIST", 9 }, (SYMS_U64)SYMS_DwAttribKind_STMT_LIST },
{ { (SYMS_U8*)"LOW_PC", 6 }, (SYMS_U64)SYMS_DwAttribKind_LOW_PC },
{ { (SYMS_U8*)"HIGH_PC", 7 }, (SYMS_U64)SYMS_DwAttribKind_HIGH_PC },
{ { (SYMS_U8*)"LANGUAGE", 8 }, (SYMS_U64)SYMS_DwAttribKind_LANGUAGE },
{ { (SYMS_U8*)"DISCR", 5 }, (SYMS_U64)SYMS_DwAttribKind_DISCR },
{ { (SYMS_U8*)"DISCR_VALUE", 11 }, (SYMS_U64)SYMS_DwAttribKind_DISCR_VALUE },
{ { (SYMS_U8*)"VISIBILITY", 10 }, (SYMS_U64)SYMS_DwAttribKind_VISIBILITY },
{ { (SYMS_U8*)"IMPORT", 6 }, (SYMS_U64)SYMS_DwAttribKind_IMPORT },
{ { (SYMS_U8*)"STRING_LENGTH", 13 }, (SYMS_U64)SYMS_DwAttribKind_STRING_LENGTH },
{ { (SYMS_U8*)"COMMON_REFERENCE", 16 }, (SYMS_U64)SYMS_DwAttribKind_COMMON_REFERENCE },
{ { (SYMS_U8*)"COMP_DIR", 8 }, (SYMS_U64)SYMS_DwAttribKind_COMP_DIR },
{ { (SYMS_U8*)"CONST_VALUE", 11 }, (SYMS_U64)SYMS_DwAttribKind_CONST_VALUE },
{ { (SYMS_U8*)"CONTAINING_TYPE", 15 }, (SYMS_U64)SYMS_DwAttribKind_CONTAINING_TYPE },
{ { (SYMS_U8*)"DEFAULT_VALUE", 13 }, (SYMS_U64)SYMS_DwAttribKind_DEFAULT_VALUE },
{ { (SYMS_U8*)"INLINE", 6 }, (SYMS_U64)SYMS_DwAttribKind_INLINE },
{ { (SYMS_U8*)"IS_OPTIONAL", 11 }, (SYMS_U64)SYMS_DwAttribKind_IS_OPTIONAL },
{ { (SYMS_U8*)"LOWER_BOUND", 11 }, (SYMS_U64)SYMS_DwAttribKind_LOWER_BOUND },
{ { (SYMS_U8*)"PRODUCER", 8 }, (SYMS_U64)SYMS_DwAttribKind_PRODUCER },
{ { (SYMS_U8*)"PROTOTYPED", 10 }, (SYMS_U64)SYMS_DwAttribKind_PROTOTYPED },
{ { (SYMS_U8*)"RETURN_ADDR", 11 }, (SYMS_U64)SYMS_DwAttribKind_RETURN_ADDR },
{ { (SYMS_U8*)"START_SCOPE", 11 }, (SYMS_U64)SYMS_DwAttribKind_START_SCOPE },
{ { (SYMS_U8*)"BIT_STRIDE", 10 }, (SYMS_U64)SYMS_DwAttribKind_BIT_STRIDE },
{ { (SYMS_U8*)"UPPER_BOUND", 11 }, (SYMS_U64)SYMS_DwAttribKind_UPPER_BOUND },
{ { (SYMS_U8*)"ABSTRACT_ORIGIN", 15 }, (SYMS_U64)SYMS_DwAttribKind_ABSTRACT_ORIGIN },
{ { (SYMS_U8*)"ACCESSIBILITY", 13 }, (SYMS_U64)SYMS_DwAttribKind_ACCESSIBILITY },
{ { (SYMS_U8*)"ADDRESS_CLASS", 13 }, (SYMS_U64)SYMS_DwAttribKind_ADDRESS_CLASS },
{ { (SYMS_U8*)"ARTIFICIAL", 10 }, (SYMS_U64)SYMS_DwAttribKind_ARTIFICIAL },
{ { (SYMS_U8*)"BASE_TYPES", 10 }, (SYMS_U64)SYMS_DwAttribKind_BASE_TYPES },
{ { (SYMS_U8*)"CALLING_CONVENTION", 18 }, (SYMS_U64)SYMS_DwAttribKind_CALLING_CONVENTION },
{ { (SYMS_U8*)"ARR_COUNT", 9 }, (SYMS_U64)SYMS_DwAttribKind_ARR_COUNT },
{ { (SYMS_U8*)"DATA_MEMBER_LOCATION", 20 }, (SYMS_U64)SYMS_DwAttribKind_DATA_MEMBER_LOCATION },
{ { (SYMS_U8*)"DECL_COLUMN", 11 }, (SYMS_U64)SYMS_DwAttribKind_DECL_COLUMN },
{ { (SYMS_U8*)"DECL_FILE", 9 }, (SYMS_U64)SYMS_DwAttribKind_DECL_FILE },
{ { (SYMS_U8*)"DECL_LINE", 9 }, (SYMS_U64)SYMS_DwAttribKind_DECL_LINE },
{ { (SYMS_U8*)"DECLARATION", 11 }, (SYMS_U64)SYMS_DwAttribKind_DECLARATION },
{ { (SYMS_U8*)"DISCR_LIST", 10 }, (SYMS_U64)SYMS_DwAttribKind_DISCR_LIST },
{ { (SYMS_U8*)"ENCODING", 8 }, (SYMS_U64)SYMS_DwAttribKind_ENCODING },
{ { (SYMS_U8*)"EXTERNAL", 8 }, (SYMS_U64)SYMS_DwAttribKind_EXTERNAL },
{ { (SYMS_U8*)"FRAME_BASE", 10 }, (SYMS_U64)SYMS_DwAttribKind_FRAME_BASE },
{ { (SYMS_U8*)"FRIEND", 6 }, (SYMS_U64)SYMS_DwAttribKind_FRIEND },
{ { (SYMS_U8*)"IDENTIFIER_CASE", 15 }, (SYMS_U64)SYMS_DwAttribKind_IDENTIFIER_CASE },
{ { (SYMS_U8*)"MACRO_INFO", 10 }, (SYMS_U64)SYMS_DwAttribKind_MACRO_INFO },
{ { (SYMS_U8*)"NAMELIST_ITEM", 13 }, (SYMS_U64)SYMS_DwAttribKind_NAMELIST_ITEM },
{ { (SYMS_U8*)"PRIORITY", 8 }, (SYMS_U64)SYMS_DwAttribKind_PRIORITY },
{ { (SYMS_U8*)"SEGMENT", 7 }, (SYMS_U64)SYMS_DwAttribKind_SEGMENT },
{ { (SYMS_U8*)"SPECIFICATION", 13 }, (SYMS_U64)SYMS_DwAttribKind_SPECIFICATION },
{ { (SYMS_U8*)"STATIC_LINK", 11 }, (SYMS_U64)SYMS_DwAttribKind_STATIC_LINK },
{ { (SYMS_U8*)"TYPE", 4 }, (SYMS_U64)SYMS_DwAttribKind_TYPE },
{ { (SYMS_U8*)"USE_LOCATION", 12 }, (SYMS_U64)SYMS_DwAttribKind_USE_LOCATION },
{ { (SYMS_U8*)"VARIABLE_PARAMETER", 18 }, (SYMS_U64)SYMS_DwAttribKind_VARIABLE_PARAMETER },
{ { (SYMS_U8*)"VIRTUALITY", 10 }, (SYMS_U64)SYMS_DwAttribKind_VIRTUALITY },
{ { (SYMS_U8*)"VTABLE_ELEM_LOCATION", 20 }, (SYMS_U64)SYMS_DwAttribKind_VTABLE_ELEM_LOCATION },
{ { (SYMS_U8*)"ALLOCATED", 9 }, (SYMS_U64)SYMS_DwAttribKind_ALLOCATED },
{ { (SYMS_U8*)"ASSOCIATED", 10 }, (SYMS_U64)SYMS_DwAttribKind_ASSOCIATED },
{ { (SYMS_U8*)"DATA_LOCATION", 13 }, (SYMS_U64)SYMS_DwAttribKind_DATA_LOCATION },
{ { (SYMS_U8*)"BYTE_STRIDE", 11 }, (SYMS_U64)SYMS_DwAttribKind_BYTE_STRIDE },
{ { (SYMS_U8*)"ENTRY_PC", 8 }, (SYMS_U64)SYMS_DwAttribKind_ENTRY_PC },
{ { (SYMS_U8*)"USE_UTF8", 8 }, (SYMS_U64)SYMS_DwAttribKind_USE_UTF8 },
{ { (SYMS_U8*)"EXTENSION", 9 }, (SYMS_U64)SYMS_DwAttribKind_EXTENSION },
{ { (SYMS_U8*)"RANGES", 6 }, (SYMS_U64)SYMS_DwAttribKind_RANGES },
{ { (SYMS_U8*)"TRAMPOLINE", 10 }, (SYMS_U64)SYMS_DwAttribKind_TRAMPOLINE },
{ { (SYMS_U8*)"CALL_COLUMN", 11 }, (SYMS_U64)SYMS_DwAttribKind_CALL_COLUMN },
{ { (SYMS_U8*)"CALL_FILE", 9 }, (SYMS_U64)SYMS_DwAttribKind_CALL_FILE },
{ { (SYMS_U8*)"CALL_LINE", 9 }, (SYMS_U64)SYMS_DwAttribKind_CALL_LINE },
{ { (SYMS_U8*)"DESCRIPTION", 11 }, (SYMS_U64)SYMS_DwAttribKind_DESCRIPTION },
{ { (SYMS_U8*)"BINARY_SCALE", 12 }, (SYMS_U64)SYMS_DwAttribKind_BINARY_SCALE },
{ { (SYMS_U8*)"DECIMAL_SCALE", 13 }, (SYMS_U64)SYMS_DwAttribKind_DECIMAL_SCALE },
{ { (SYMS_U8*)"SMALL", 5 }, (SYMS_U64)SYMS_DwAttribKind_SMALL },
{ { (SYMS_U8*)"DECIMAL_SIGN", 12 }, (SYMS_U64)SYMS_DwAttribKind_DECIMAL_SIGN },
{ { (SYMS_U8*)"DIGIT_COUNT", 11 }, (SYMS_U64)SYMS_DwAttribKind_DIGIT_COUNT },
{ { (SYMS_U8*)"PICTURE_STRING", 14 }, (SYMS_U64)SYMS_DwAttribKind_PICTURE_STRING },
{ { (SYMS_U8*)"MUTABLE", 7 }, (SYMS_U64)SYMS_DwAttribKind_MUTABLE },
{ { (SYMS_U8*)"THREADS_SCALED", 14 }, (SYMS_U64)SYMS_DwAttribKind_THREADS_SCALED },
{ { (SYMS_U8*)"EXPLICIT", 8 }, (SYMS_U64)SYMS_DwAttribKind_EXPLICIT },
{ { (SYMS_U8*)"OBJECT_POINTER", 14 }, (SYMS_U64)SYMS_DwAttribKind_OBJECT_POINTER },
{ { (SYMS_U8*)"ENDIANITY", 9 }, (SYMS_U64)SYMS_DwAttribKind_ENDIANITY },
{ { (SYMS_U8*)"ELEMENTAL", 9 }, (SYMS_U64)SYMS_DwAttribKind_ELEMENTAL },
{ { (SYMS_U8*)"PURE", 4 }, (SYMS_U64)SYMS_DwAttribKind_PURE },
{ { (SYMS_U8*)"RECURSIVE", 9 }, (SYMS_U64)SYMS_DwAttribKind_RECURSIVE },
{ { (SYMS_U8*)"SIGNATURE", 9 }, (SYMS_U64)SYMS_DwAttribKind_SIGNATURE },
{ { (SYMS_U8*)"MAIN_SUBPROGRAM", 15 }, (SYMS_U64)SYMS_DwAttribKind_MAIN_SUBPROGRAM },
{ { (SYMS_U8*)"DATA_BIT_OFFSET", 15 }, (SYMS_U64)SYMS_DwAttribKind_DATA_BIT_OFFSET },
{ { (SYMS_U8*)"CONST_EXPR", 10 }, (SYMS_U64)SYMS_DwAttribKind_CONST_EXPR },
{ { (SYMS_U8*)"ENUM_CLASS", 10 }, (SYMS_U64)SYMS_DwAttribKind_ENUM_CLASS },
{ { (SYMS_U8*)"LINKAGE_NAME", 12 }, (SYMS_U64)SYMS_DwAttribKind_LINKAGE_NAME },
{ { (SYMS_U8*)"STRING_LENGTH_BIT_SIZE", 22 }, (SYMS_U64)SYMS_DwAttribKind_STRING_LENGTH_BIT_SIZE },
{ { (SYMS_U8*)"STRING_LENGTH_BYTE_SIZE", 23 }, (SYMS_U64)SYMS_DwAttribKind_STRING_LENGTH_BYTE_SIZE },
{ { (SYMS_U8*)"RANK", 4 }, (SYMS_U64)SYMS_DwAttribKind_RANK },
{ { (SYMS_U8*)"STR_OFFSETS_BASE", 16 }, (SYMS_U64)SYMS_DwAttribKind_STR_OFFSETS_BASE },
{ { (SYMS_U8*)"ADDR_BASE", 9 }, (SYMS_U64)SYMS_DwAttribKind_ADDR_BASE },
{ { (SYMS_U8*)"RNGLISTS_BASE", 13 }, (SYMS_U64)SYMS_DwAttribKind_RNGLISTS_BASE },
{ { (SYMS_U8*)"DWO_NAME", 8 }, (SYMS_U64)SYMS_DwAttribKind_DWO_NAME },
{ { (SYMS_U8*)"REFERENCE", 9 }, (SYMS_U64)SYMS_DwAttribKind_REFERENCE },
{ { (SYMS_U8*)"RVALUE_REFERENCE", 16 }, (SYMS_U64)SYMS_DwAttribKind_RVALUE_REFERENCE },
{ { (SYMS_U8*)"MACROS", 6 }, (SYMS_U64)SYMS_DwAttribKind_MACROS },
{ { (SYMS_U8*)"CALL_ALL_CALLS", 14 }, (SYMS_U64)SYMS_DwAttribKind_CALL_ALL_CALLS },
{ { (SYMS_U8*)"CALL_ALL_SOURCE_CALLS", 21 }, (SYMS_U64)SYMS_DwAttribKind_CALL_ALL_SOURCE_CALLS },
{ { (SYMS_U8*)"CALL_ALL_TAIL_CALLS", 19 }, (SYMS_U64)SYMS_DwAttribKind_CALL_ALL_TAIL_CALLS },
{ { (SYMS_U8*)"CALL_RETURN_PC", 14 }, (SYMS_U64)SYMS_DwAttribKind_CALL_RETURN_PC },
{ { (SYMS_U8*)"CALL_VALUE", 10 }, (SYMS_U64)SYMS_DwAttribKind_CALL_VALUE },
{ { (SYMS_U8*)"CALL_ORIGIN", 11 }, (SYMS_U64)SYMS_DwAttribKind_CALL_ORIGIN },
{ { (SYMS_U8*)"CALL_PARAMETER", 14 }, (SYMS_U64)SYMS_DwAttribKind_CALL_PARAMETER },
{ { (SYMS_U8*)"CALL_PC", 7 }, (SYMS_U64)SYMS_DwAttribKind_CALL_PC },
{ { (SYMS_U8*)"CALL_TAIL_CALL", 14 }, (SYMS_U64)SYMS_DwAttribKind_CALL_TAIL_CALL },
{ { (SYMS_U8*)"CALL_TARGET", 11 }, (SYMS_U64)SYMS_DwAttribKind_CALL_TARGET },
{ { (SYMS_U8*)"CALL_TARGET_CLOBBERED", 21 }, (SYMS_U64)SYMS_DwAttribKind_CALL_TARGET_CLOBBERED },
{ { (SYMS_U8*)"CALL_DATA_LOCATION", 18 }, (SYMS_U64)SYMS_DwAttribKind_CALL_DATA_LOCATION },
{ { (SYMS_U8*)"CALL_DATA_VALUE", 15 }, (SYMS_U64)SYMS_DwAttribKind_CALL_DATA_VALUE },
{ { (SYMS_U8*)"NORETURN", 8 }, (SYMS_U64)SYMS_DwAttribKind_NORETURN },
{ { (SYMS_U8*)"ALIGNMENT", 9 }, (SYMS_U64)SYMS_DwAttribKind_ALIGNMENT },
{ { (SYMS_U8*)"EXPORT_SYMBOLS", 14 }, (SYMS_U64)SYMS_DwAttribKind_EXPORT_SYMBOLS },
{ { (SYMS_U8*)"DELETED", 7 }, (SYMS_U64)SYMS_DwAttribKind_DELETED },
{ { (SYMS_U8*)"DEFAULTED", 9 }, (SYMS_U64)SYMS_DwAttribKind_DEFAULTED },
{ { (SYMS_U8*)"LOCLISTS_BASE", 13 }, (SYMS_U64)SYMS_DwAttribKind_LOCLISTS_BASE },
{ { (SYMS_U8*)"GNU_VECTOR", 10 }, (SYMS_U64)SYMS_DwAttribKind_GNU_VECTOR },
{ { (SYMS_U8*)"GNU_GUARDED_BY", 14 }, (SYMS_U64)SYMS_DwAttribKind_GNU_GUARDED_BY },
{ { (SYMS_U8*)"GNU_PT_GUARDED_BY", 17 }, (SYMS_U64)SYMS_DwAttribKind_GNU_PT_GUARDED_BY },
{ { (SYMS_U8*)"GNU_GUARDED", 11 }, (SYMS_U64)SYMS_DwAttribKind_GNU_GUARDED },
{ { (SYMS_U8*)"GNU_PT_GUARDED", 14 }, (SYMS_U64)SYMS_DwAttribKind_GNU_PT_GUARDED },
{ { (SYMS_U8*)"GNU_LOCKS_EXCLUDED", 18 }, (SYMS_U64)SYMS_DwAttribKind_GNU_LOCKS_EXCLUDED },
{ { (SYMS_U8*)"GNU_EXCLUSIVE_LOCKS_REQUIRED", 28 }, (SYMS_U64)SYMS_DwAttribKind_GNU_EXCLUSIVE_LOCKS_REQUIRED },
{ { (SYMS_U8*)"GNU_SHARED_LOCKS_REQUIRED", 25 }, (SYMS_U64)SYMS_DwAttribKind_GNU_SHARED_LOCKS_REQUIRED },
{ { (SYMS_U8*)"GNU_ODR_SIGNATURE", 17 }, (SYMS_U64)SYMS_DwAttribKind_GNU_ODR_SIGNATURE },
{ { (SYMS_U8*)"GNU_TEMPLATE_NAME", 17 }, (SYMS_U64)SYMS_DwAttribKind_GNU_TEMPLATE_NAME },
{ { (SYMS_U8*)"GNU_CALL_SITE_VALUE", 19 }, (SYMS_U64)SYMS_DwAttribKind_GNU_CALL_SITE_VALUE },
{ { (SYMS_U8*)"GNU_CALL_SITE_DATA_VALUE", 24 }, (SYMS_U64)SYMS_DwAttribKind_GNU_CALL_SITE_DATA_VALUE },
{ { (SYMS_U8*)"GNU_CALL_SITE_TARGET", 20 }, (SYMS_U64)SYMS_DwAttribKind_GNU_CALL_SITE_TARGET },
{ { (SYMS_U8*)"GNU_CALL_SITE_TARGET_CLOBBERED", 30 }, (SYMS_U64)SYMS_DwAttribKind_GNU_CALL_SITE_TARGET_CLOBBERED },
{ { (SYMS_U8*)"GNU_TAIL_CALL", 13 }, (SYMS_U64)SYMS_DwAttribKind_GNU_TAIL_CALL },
{ { (SYMS_U8*)"GNU_ALL_TAIL_CALL_SITES", 23 }, (SYMS_U64)SYMS_DwAttribKind_GNU_ALL_TAIL_CALL_SITES },
{ { (SYMS_U8*)"GNU_ALL_CALL_SITES", 18 }, (SYMS_U64)SYMS_DwAttribKind_GNU_ALL_CALL_SITES },
{ { (SYMS_U8*)"GNU_ALL_SOURCE_CALL_SITES", 25 }, (SYMS_U64)SYMS_DwAttribKind_GNU_ALL_SOURCE_CALL_SITES },
{ { (SYMS_U8*)"GNU_MACROS", 10 }, (SYMS_U64)SYMS_DwAttribKind_GNU_MACROS },
{ { (SYMS_U8*)"GNU_DELETED", 11 }, (SYMS_U64)SYMS_DwAttribKind_GNU_DELETED },
{ { (SYMS_U8*)"GNU_DWO_NAME", 12 }, (SYMS_U64)SYMS_DwAttribKind_GNU_DWO_NAME },
{ { (SYMS_U8*)"GNU_DWO_ID", 10 }, (SYMS_U64)SYMS_DwAttribKind_GNU_DWO_ID },
{ { (SYMS_U8*)"GNU_RANGES_BASE", 15 }, (SYMS_U64)SYMS_DwAttribKind_GNU_RANGES_BASE },
{ { (SYMS_U8*)"GNU_ADDR_BASE", 13 }, (SYMS_U64)SYMS_DwAttribKind_GNU_ADDR_BASE },
{ { (SYMS_U8*)"GNU_PUBNAMES", 12 }, (SYMS_U64)SYMS_DwAttribKind_GNU_PUBNAMES },
{ { (SYMS_U8*)"GNU_PUBTYPES", 12 }, (SYMS_U64)SYMS_DwAttribKind_GNU_PUBTYPES },
{ { (SYMS_U8*)"GNU_DISCRIMINATOR", 17 }, (SYMS_U64)SYMS_DwAttribKind_GNU_DISCRIMINATOR },
{ { (SYMS_U8*)"GNU_LOCVIEWS", 12 }, (SYMS_U64)SYMS_DwAttribKind_GNU_LOCVIEWS },
{ { (SYMS_U8*)"GNU_ENTRY_VIEW", 14 }, (SYMS_U64)SYMS_DwAttribKind_GNU_ENTRY_VIEW },
{ { (SYMS_U8*)"VMS_RTNBEG_PD_ADDRESS", 21 }, (SYMS_U64)SYMS_DwAttribKind_VMS_RTNBEG_PD_ADDRESS },
{ { (SYMS_U8*)"USE_GNAT_DESCRIPTIVE_TYPE", 25 }, (SYMS_U64)SYMS_DwAttribKind_USE_GNAT_DESCRIPTIVE_TYPE },
{ { (SYMS_U8*)"GNAT_DESCRIPTIVE_TYPE", 21 }, (SYMS_U64)SYMS_DwAttribKind_GNAT_DESCRIPTIVE_TYPE },
{ { (SYMS_U8*)"GNU_NUMERATOR", 13 }, (SYMS_U64)SYMS_DwAttribKind_GNU_NUMERATOR },
{ { (SYMS_U8*)"GNU_DENOMINATOR", 15 }, (SYMS_U64)SYMS_DwAttribKind_GNU_DENOMINATOR },
{ { (SYMS_U8*)"GNU_BIAS", 8 }, (SYMS_U64)SYMS_DwAttribKind_GNU_BIAS },
{ { (SYMS_U8*)"UPC_THREADS_SCALED", 18 }, (SYMS_U64)SYMS_DwAttribKind_UPC_THREADS_SCALED },
{ { (SYMS_U8*)"PGI_LBASE", 9 }, (SYMS_U64)SYMS_DwAttribKind_PGI_LBASE },
{ { (SYMS_U8*)"PGI_SOFFSET", 11 }, (SYMS_U64)SYMS_DwAttribKind_PGI_SOFFSET },
{ { (SYMS_U8*)"PGI_LSTRIDE", 11 }, (SYMS_U64)SYMS_DwAttribKind_PGI_LSTRIDE },
{ { (SYMS_U8*)"LLVM_INCLUDE_PATH", 17 }, (SYMS_U64)SYMS_DwAttribKind_LLVM_INCLUDE_PATH },
{ { (SYMS_U8*)"LLVM_CONFIG_MACROS", 18 }, (SYMS_U64)SYMS_DwAttribKind_LLVM_CONFIG_MACROS },
{ { (SYMS_U8*)"LLVM_SYSROOT", 12 }, (SYMS_U64)SYMS_DwAttribKind_LLVM_SYSROOT },
{ { (SYMS_U8*)"LLVM_API_NOTES", 14 }, (SYMS_U64)SYMS_DwAttribKind_LLVM_API_NOTES },
{ { (SYMS_U8*)"LLVM_TAG_OFFSET", 15 }, (SYMS_U64)SYMS_DwAttribKind_LLVM_TAG_OFFSET },
{ { (SYMS_U8*)"APPLE_OPTIMIZED", 15 }, (SYMS_U64)SYMS_DwAttribKind_APPLE_OPTIMIZED },
{ { (SYMS_U8*)"APPLE_FLAGS", 11 }, (SYMS_U64)SYMS_DwAttribKind_APPLE_FLAGS },
{ { (SYMS_U8*)"APPLE_ISA", 9 }, (SYMS_U64)SYMS_DwAttribKind_APPLE_ISA },
{ { (SYMS_U8*)"APPLE_BLOCK", 11 }, (SYMS_U64)SYMS_DwAttribKind_APPLE_BLOCK },
{ { (SYMS_U8*)"APPLE_MAJOR_RUNTIME_VERS", 24 }, (SYMS_U64)SYMS_DwAttribKind_APPLE_MAJOR_RUNTIME_VERS },
{ { (SYMS_U8*)"APPLE_RUNTIME_CLASS", 19 }, (SYMS_U64)SYMS_DwAttribKind_APPLE_RUNTIME_CLASS },
{ { (SYMS_U8*)"APPLE_OMIT_FRAME_PTR", 20 }, (SYMS_U64)SYMS_DwAttribKind_APPLE_OMIT_FRAME_PTR },
{ { (SYMS_U8*)"APPLE_PROPERTY_NAME", 19 }, (SYMS_U64)SYMS_DwAttribKind_APPLE_PROPERTY_NAME },
{ { (SYMS_U8*)"APPLE_PROPERTY_GETTER", 21 }, (SYMS_U64)SYMS_DwAttribKind_APPLE_PROPERTY_GETTER },
{ { (SYMS_U8*)"APPLE_PROPERTY_SETTER", 21 }, (SYMS_U64)SYMS_DwAttribKind_APPLE_PROPERTY_SETTER },
{ { (SYMS_U8*)"APPLE_PROPERTY_ATTRIBUTE", 24 }, (SYMS_U64)SYMS_DwAttribKind_APPLE_PROPERTY_ATTRIBUTE },
{ { (SYMS_U8*)"APPLE_OBJC_COMPLETE_TYPE", 24 }, (SYMS_U64)SYMS_DwAttribKind_APPLE_OBJC_COMPLETE_TYPE },
{ { (SYMS_U8*)"APPLE_PROPERTY", 14 }, (SYMS_U64)SYMS_DwAttribKind_APPLE_PROPERTY },
{ { (SYMS_U8*)"APPLE_OBJ_DIRECT", 16 }, (SYMS_U64)SYMS_DwAttribKind_APPLE_OBJ_DIRECT },
{ { (SYMS_U8*)"APPLE_SDK", 9 }, (SYMS_U64)SYMS_DwAttribKind_APPLE_SDK },
{ { (SYMS_U8*)"MIPS_FDE", 8 }, (SYMS_U64)SYMS_DwAttribKind_MIPS_FDE },
{ { (SYMS_U8*)"MIPS_LOOP_BEGIN", 15 }, (SYMS_U64)SYMS_DwAttribKind_MIPS_LOOP_BEGIN },
{ { (SYMS_U8*)"MIPS_TAIL_LOOP_BEGIN", 20 }, (SYMS_U64)SYMS_DwAttribKind_MIPS_TAIL_LOOP_BEGIN },
{ { (SYMS_U8*)"MIPS_EPILOG_BEGIN", 17 }, (SYMS_U64)SYMS_DwAttribKind_MIPS_EPILOG_BEGIN },
{ { (SYMS_U8*)"MIPS_LOOP_UNROLL_FACTOR", 23 }, (SYMS_U64)SYMS_DwAttribKind_MIPS_LOOP_UNROLL_FACTOR },
{ { (SYMS_U8*)"MIPS_SOFTWARE_PIPELINE_DEPTH", 28 }, (SYMS_U64)SYMS_DwAttribKind_MIPS_SOFTWARE_PIPELINE_DEPTH },
{ { (SYMS_U8*)"MIPS_LINKAGE_NAME", 17 }, (SYMS_U64)SYMS_DwAttribKind_MIPS_LINKAGE_NAME },
{ { (SYMS_U8*)"MIPS_STRIDE", 11 }, (SYMS_U64)SYMS_DwAttribKind_MIPS_STRIDE },
{ { (SYMS_U8*)"MIPS_ABSTRACT_NAME", 18 }, (SYMS_U64)SYMS_DwAttribKind_MIPS_ABSTRACT_NAME },
{ { (SYMS_U8*)"MIPS_CLONE_ORIGIN", 17 }, (SYMS_U64)SYMS_DwAttribKind_MIPS_CLONE_ORIGIN },
{ { (SYMS_U8*)"MIPS_HAS_INLINES", 16 }, (SYMS_U64)SYMS_DwAttribKind_MIPS_HAS_INLINES },
{ { (SYMS_U8*)"MIPS_STRIDE_BYTE", 16 }, (SYMS_U64)SYMS_DwAttribKind_MIPS_STRIDE_BYTE },
{ { (SYMS_U8*)"MIPS_STRIDE_ELEM", 16 }, (SYMS_U64)SYMS_DwAttribKind_MIPS_STRIDE_ELEM },
{ { (SYMS_U8*)"MIPS_PTR_DOPETYPE", 17 }, (SYMS_U64)SYMS_DwAttribKind_MIPS_PTR_DOPETYPE },
{ { (SYMS_U8*)"MIPS_ALLOCATABLE_DOPETYPE", 25 }, (SYMS_U64)SYMS_DwAttribKind_MIPS_ALLOCATABLE_DOPETYPE },
{ { (SYMS_U8*)"MIPS_ASSUMED_SHAPE_DOPETYPE", 27 }, (SYMS_U64)SYMS_DwAttribKind_MIPS_ASSUMED_SHAPE_DOPETYPE },
{ { (SYMS_U8*)"MIPS_ASSUMED_SIZE", 17 }, (SYMS_U64)SYMS_DwAttribKind_MIPS_ASSUMED_SIZE },
{ { (SYMS_U8*)"LO_USER", 7 }, (SYMS_U64)SYMS_DwAttribKind_LO_USER },
{ { (SYMS_U8*)"HI_USER", 7 }, (SYMS_U64)SYMS_DwAttribKind_HI_USER },
};
SYMS_SerialValue _syms_serial_members_for_SYMS_DwAttribTypeEncoding[] = {
{ { (SYMS_U8*)"Null", 4 }, (SYMS_U64)SYMS_DwAttribTypeEncoding_Null },
{ { (SYMS_U8*)"ADDRESS", 7 }, (SYMS_U64)SYMS_DwAttribTypeEncoding_ADDRESS },
{ { (SYMS_U8*)"BOOLEAN", 7 }, (SYMS_U64)SYMS_DwAttribTypeEncoding_BOOLEAN },
{ { (SYMS_U8*)"COMPLEX_FLOAT", 13 }, (SYMS_U64)SYMS_DwAttribTypeEncoding_COMPLEX_FLOAT },
{ { (SYMS_U8*)"FLOAT", 5 }, (SYMS_U64)SYMS_DwAttribTypeEncoding_FLOAT },
{ { (SYMS_U8*)"SIGNED", 6 }, (SYMS_U64)SYMS_DwAttribTypeEncoding_SIGNED },
{ { (SYMS_U8*)"SIGNED_CHAR", 11 }, (SYMS_U64)SYMS_DwAttribTypeEncoding_SIGNED_CHAR },
{ { (SYMS_U8*)"UNSIGNED", 8 }, (SYMS_U64)SYMS_DwAttribTypeEncoding_UNSIGNED },
{ { (SYMS_U8*)"UNSIGNED_CHAR", 13 }, (SYMS_U64)SYMS_DwAttribTypeEncoding_UNSIGNED_CHAR },
{ { (SYMS_U8*)"IMAGINARY_FLOAT", 15 }, (SYMS_U64)SYMS_DwAttribTypeEncoding_IMAGINARY_FLOAT },
{ { (SYMS_U8*)"PACKED_DECIMAL", 14 }, (SYMS_U64)SYMS_DwAttribTypeEncoding_PACKED_DECIMAL },
{ { (SYMS_U8*)"NUMERIC_STRING", 14 }, (SYMS_U64)SYMS_DwAttribTypeEncoding_NUMERIC_STRING },
{ { (SYMS_U8*)"EDITED", 6 }, (SYMS_U64)SYMS_DwAttribTypeEncoding_EDITED },
{ { (SYMS_U8*)"SIGNED_FIXED", 12 }, (SYMS_U64)SYMS_DwAttribTypeEncoding_SIGNED_FIXED },
{ { (SYMS_U8*)"UNSIGNED_FIXED", 14 }, (SYMS_U64)SYMS_DwAttribTypeEncoding_UNSIGNED_FIXED },
{ { (SYMS_U8*)"DECIMAL_FLOAT", 13 }, (SYMS_U64)SYMS_DwAttribTypeEncoding_DECIMAL_FLOAT },
{ { (SYMS_U8*)"UTF", 3 }, (SYMS_U64)SYMS_DwAttribTypeEncoding_UTF },
{ { (SYMS_U8*)"UCS", 3 }, (SYMS_U64)SYMS_DwAttribTypeEncoding_UCS },
{ { (SYMS_U8*)"ASCII", 5 }, (SYMS_U64)SYMS_DwAttribTypeEncoding_ASCII },
};
SYMS_SerialValue _syms_serial_members_for_SYMS_DwCallingConvention[] = {
{ { (SYMS_U8*)"Normal", 6 }, (SYMS_U64)SYMS_DwCallingConvention_Normal },
{ { (SYMS_U8*)"Program", 7 }, (SYMS_U64)SYMS_DwCallingConvention_Program },
{ { (SYMS_U8*)"NoCall", 6 }, (SYMS_U64)SYMS_DwCallingConvention_NoCall },
{ { (SYMS_U8*)"PassByValue", 11 }, (SYMS_U64)SYMS_DwCallingConvention_PassByValue },
{ { (SYMS_U8*)"PassByReference", 15 }, (SYMS_U64)SYMS_DwCallingConvention_PassByReference },
};
SYMS_SerialValue _syms_serial_members_for_SYMS_DwAccess[] = {
{ { (SYMS_U8*)"Public", 6 }, (SYMS_U64)SYMS_DwAccess_Public },
{ { (SYMS_U8*)"Private", 7 }, (SYMS_U64)SYMS_DwAccess_Private },
{ { (SYMS_U8*)"Protected", 9 }, (SYMS_U64)SYMS_DwAccess_Protected },
};
SYMS_SerialValue _syms_serial_members_for_SYMS_DwVirtuality[] = {
{ { (SYMS_U8*)"None", 4 }, (SYMS_U64)SYMS_DwVirtuality_None },
{ { (SYMS_U8*)"Virtual", 7 }, (SYMS_U64)SYMS_DwVirtuality_Virtual },
{ { (SYMS_U8*)"PureVirtual", 11 }, (SYMS_U64)SYMS_DwVirtuality_PureVirtual },
};
SYMS_SerialValue _syms_serial_members_for_SYMS_DwRngListEntryKind[] = {
{ { (SYMS_U8*)"EndOfList", 9 }, (SYMS_U64)SYMS_DwRngListEntryKind_EndOfList },
{ { (SYMS_U8*)"BaseAddressX", 12 }, (SYMS_U64)SYMS_DwRngListEntryKind_BaseAddressX },
{ { (SYMS_U8*)"StartxEndx", 10 }, (SYMS_U64)SYMS_DwRngListEntryKind_StartxEndx },
{ { (SYMS_U8*)"StartxLength", 12 }, (SYMS_U64)SYMS_DwRngListEntryKind_StartxLength },
{ { (SYMS_U8*)"OffsetPair", 10 }, (SYMS_U64)SYMS_DwRngListEntryKind_OffsetPair },
{ { (SYMS_U8*)"BaseAddress", 11 }, (SYMS_U64)SYMS_DwRngListEntryKind_BaseAddress },
{ { (SYMS_U8*)"StartEnd", 8 }, (SYMS_U64)SYMS_DwRngListEntryKind_StartEnd },
{ { (SYMS_U8*)"StartLength", 11 }, (SYMS_U64)SYMS_DwRngListEntryKind_StartLength },
};
SYMS_SerialValue _syms_serial_members_for_SYMS_DwLocListEntryKind[] = {
{ { (SYMS_U8*)"EndOfList", 9 }, (SYMS_U64)SYMS_DwLocListEntryKind_EndOfList },
{ { (SYMS_U8*)"BaseAddressX", 12 }, (SYMS_U64)SYMS_DwLocListEntryKind_BaseAddressX },
{ { (SYMS_U8*)"StartXEndX", 10 }, (SYMS_U64)SYMS_DwLocListEntryKind_StartXEndX },
{ { (SYMS_U8*)"StartXLength", 12 }, (SYMS_U64)SYMS_DwLocListEntryKind_StartXLength },
{ { (SYMS_U8*)"OffsetPair", 10 }, (SYMS_U64)SYMS_DwLocListEntryKind_OffsetPair },
{ { (SYMS_U8*)"DefaultLocation", 15 }, (SYMS_U64)SYMS_DwLocListEntryKind_DefaultLocation },
{ { (SYMS_U8*)"BaseAddress", 11 }, (SYMS_U64)SYMS_DwLocListEntryKind_BaseAddress },
{ { (SYMS_U8*)"StartEnd", 8 }, (SYMS_U64)SYMS_DwLocListEntryKind_StartEnd },
{ { (SYMS_U8*)"StartLength", 11 }, (SYMS_U64)SYMS_DwLocListEntryKind_StartLength },
};

//~ generated from code at syms/metaprogram/syms_metaprogram_serial.c:1458
SYMS_SerialType _syms_serial_type_SYMS_DwMode = {
{(SYMS_U8*)"SYMS_DwMode", 11}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_DwMode), _syms_serial_members_for_SYMS_DwMode, sizeof(SYMS_DwMode), syms_enum_index_from_value_identity
};
SYMS_SerialType _syms_serial_type_SYMS_DwVersion = {
{(SYMS_U8*)"SYMS_DwVersion", 14}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_DwVersion), _syms_serial_members_for_SYMS_DwVersion, sizeof(SYMS_DwVersion), syms_enum_index_from_value_identity
};
SYMS_SerialType _syms_serial_type_SYMS_DwSectionKind = {
{(SYMS_U8*)"SYMS_DwSectionKind", 18}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_DwSectionKind), _syms_serial_members_for_SYMS_DwSectionKind, sizeof(SYMS_DwSectionKind), syms_enum_index_from_value_identity
};
SYMS_SerialType _syms_serial_type_SYMS_DwLanguage = {
{(SYMS_U8*)"SYMS_DwLanguage", 15}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_DwLanguage), _syms_serial_members_for_SYMS_DwLanguage, sizeof(SYMS_DwLanguage), syms_enum_index_from_dwlanguage
};
SYMS_SerialType _syms_serial_type_SYMS_DwStdOpcode = {
{(SYMS_U8*)"SYMS_DwStdOpcode", 16}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_DwStdOpcode), _syms_serial_members_for_SYMS_DwStdOpcode, sizeof(SYMS_DwStdOpcode), syms_enum_index_from_value_identity
};
SYMS_SerialType _syms_serial_type_SYMS_DwExtOpcode = {
{(SYMS_U8*)"SYMS_DwExtOpcode", 16}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_DwExtOpcode), _syms_serial_members_for_SYMS_DwExtOpcode, sizeof(SYMS_DwExtOpcode), syms_enum_index_from_dwextopcode
};
SYMS_SerialType _syms_serial_type_SYMS_DwNameCase = {
{(SYMS_U8*)"SYMS_DwNameCase", 15}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_DwNameCase), _syms_serial_members_for_SYMS_DwNameCase, sizeof(SYMS_DwNameCase), syms_enum_index_from_value_identity
};
SYMS_SerialType _syms_serial_type_SYMS_DwTagKind = {
{(SYMS_U8*)"SYMS_DwTagKind", 14}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_DwTagKind), _syms_serial_members_for_SYMS_DwTagKind, sizeof(SYMS_DwTagKind), syms_enum_index_from_dwtagkind
};
SYMS_SerialType _syms_serial_type_SYMS_DwAttribClass = {
{(SYMS_U8*)"SYMS_DwAttribClass", 18}, SYMS_SerialTypeKind_Flags, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_DwAttribClass), _syms_serial_members_for_SYMS_DwAttribClass, sizeof(SYMS_DwAttribClass), 0
};
SYMS_SerialType _syms_serial_type_SYMS_DwFormKind = {
{(SYMS_U8*)"SYMS_DwFormKind", 15}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_DwFormKind), _syms_serial_members_for_SYMS_DwFormKind, sizeof(SYMS_DwFormKind), syms_enum_index_from_dwformkind
};
SYMS_SerialType _syms_serial_type_SYMS_DwAttribKind = {
{(SYMS_U8*)"SYMS_DwAttribKind", 17}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_DwAttribKind), _syms_serial_members_for_SYMS_DwAttribKind, sizeof(SYMS_DwAttribKind), syms_enum_index_from_dwattribkind
};
SYMS_SerialType _syms_serial_type_SYMS_DwAttribTypeEncoding = {
{(SYMS_U8*)"SYMS_DwAttribTypeEncoding", 25}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_DwAttribTypeEncoding), _syms_serial_members_for_SYMS_DwAttribTypeEncoding, sizeof(SYMS_DwAttribTypeEncoding), syms_enum_index_from_dw_attrib_type_encoding
};
SYMS_SerialType _syms_serial_type_SYMS_DwCallingConvention = {
{(SYMS_U8*)"SYMS_DwCallingConvention", 24}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_DwCallingConvention), _syms_serial_members_for_SYMS_DwCallingConvention, sizeof(SYMS_DwCallingConvention), syms_enum_index_from_value_identity
};
SYMS_SerialType _syms_serial_type_SYMS_DwAccess = {
{(SYMS_U8*)"SYMS_DwAccess", 13}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_DwAccess), _syms_serial_members_for_SYMS_DwAccess, sizeof(SYMS_DwAccess), syms_enum_index_from_value_identity
};
SYMS_SerialType _syms_serial_type_SYMS_DwVirtuality = {
{(SYMS_U8*)"SYMS_DwVirtuality", 17}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_DwVirtuality), _syms_serial_members_for_SYMS_DwVirtuality, sizeof(SYMS_DwVirtuality), syms_enum_index_from_value_identity
};
SYMS_SerialType _syms_serial_type_SYMS_DwRngListEntryKind = {
{(SYMS_U8*)"SYMS_DwRngListEntryKind", 23}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_DwRngListEntryKind), _syms_serial_members_for_SYMS_DwRngListEntryKind, sizeof(SYMS_DwRngListEntryKind), syms_enum_index_from_value_identity
};
SYMS_SerialType _syms_serial_type_SYMS_DwLocListEntryKind = {
{(SYMS_U8*)"SYMS_DwLocListEntryKind", 23}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_DwLocListEntryKind), _syms_serial_members_for_SYMS_DwLocListEntryKind, sizeof(SYMS_DwLocListEntryKind), syms_enum_index_from_value_identity
};

#endif // defined(SYMS_ENABLE_DWARF_SERIAL_INFO)


////////////////////////////////
#if defined(SYMS_ENABLE_DWARF_SERIAL_INFO)

//~ generated from code at syms/metaprogram/syms_metaprogram_serial.c:1091
SYMS_API SYMS_U64
syms_enum_index_from_dw_c_f_a_detail(SYMS_U64 v){
SYMS_U64 result = 0;
switch ((SYMS_U8)v){
default: break;
case SYMS_DwCFADetail_OPL_KIND1: result = 0; break;
case SYMS_DwCFADetail_OPL_KIND2: result = 1; break;
}
return(result);
}
SYMS_API SYMS_U64
syms_enum_index_from_dw_c_f_a_mask(SYMS_U64 v){
SYMS_U64 result = 0;
switch ((SYMS_U8)v){
default: break;
case SYMS_DwCFAMask_HI_OPCODE: result = 0; break;
case SYMS_DwCFAMask_OPERAND: result = 1; break;
}
return(result);
}

//~ generated from code at syms/metaprogram/syms_metaprogram_serial.c:1322
SYMS_SerialValue _syms_serial_members_for_SYMS_DwCFADetail[] = {
{ { (SYMS_U8*)"OPL_KIND1", 9 }, (SYMS_U64)SYMS_DwCFADetail_OPL_KIND1 },
{ { (SYMS_U8*)"OPL_KIND2", 9 }, (SYMS_U64)SYMS_DwCFADetail_OPL_KIND2 },
};
SYMS_SerialValue _syms_serial_members_for_SYMS_DwCFAMask[] = {
{ { (SYMS_U8*)"HI_OPCODE", 9 }, (SYMS_U64)SYMS_DwCFAMask_HI_OPCODE },
{ { (SYMS_U8*)"OPERAND", 7 }, (SYMS_U64)SYMS_DwCFAMask_OPERAND },
};

//~ generated from code at syms/metaprogram/syms_metaprogram_serial.c:1458
SYMS_SerialType _syms_serial_type_SYMS_DwCFADetail = {
{(SYMS_U8*)"SYMS_DwCFADetail", 16}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_DwCFADetail), _syms_serial_members_for_SYMS_DwCFADetail, sizeof(SYMS_DwCFADetail), syms_enum_index_from_dw_c_f_a_detail
};
SYMS_SerialType _syms_serial_type_SYMS_DwCFAMask = {
{(SYMS_U8*)"SYMS_DwCFAMask", 14}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_DwCFAMask), _syms_serial_members_for_SYMS_DwCFAMask, sizeof(SYMS_DwCFAMask), syms_enum_index_from_dw_c_f_a_mask
};

#endif // defined(SYMS_ENABLE_DWARF_SERIAL_INFO)


////////////////////////////////
#if defined(SYMS_ENABLE_DWARF_SERIAL_INFO)

//~ generated from code at syms/metaprogram/syms_metaprogram_serial.c:1091
SYMS_API SYMS_U64
syms_enum_index_from_dwregx86(SYMS_U64 v){
SYMS_U64 result = 0;
switch ((SYMS_U64)v){
default: break;
case SYMS_DwRegX86_EAX: result = 0; break;
case SYMS_DwRegX86_ECX: result = 1; break;
case SYMS_DwRegX86_EDX: result = 2; break;
case SYMS_DwRegX86_EBX: result = 3; break;
case SYMS_DwRegX86_ESP: result = 4; break;
case SYMS_DwRegX86_EBP: result = 5; break;
case SYMS_DwRegX86_ESI: result = 6; break;
case SYMS_DwRegX86_EDI: result = 7; break;
case SYMS_DwRegX86_EIP: result = 8; break;
case SYMS_DwRegX86_EFLAGS: result = 9; break;
case SYMS_DwRegX86_TRAPNO: result = 10; break;
case SYMS_DwRegX86_ST0: result = 11; break;
case SYMS_DwRegX86_ST1: result = 12; break;
case SYMS_DwRegX86_ST2: result = 13; break;
case SYMS_DwRegX86_ST3: result = 14; break;
case SYMS_DwRegX86_ST4: result = 15; break;
case SYMS_DwRegX86_ST5: result = 16; break;
case SYMS_DwRegX86_ST6: result = 17; break;
case SYMS_DwRegX86_ST7: result = 18; break;
case SYMS_DwRegX86_XMM0: result = 19; break;
case SYMS_DwRegX86_XMM1: result = 20; break;
case SYMS_DwRegX86_XMM2: result = 21; break;
case SYMS_DwRegX86_XMM3: result = 22; break;
case SYMS_DwRegX86_XMM4: result = 23; break;
case SYMS_DwRegX86_XMM5: result = 24; break;
case SYMS_DwRegX86_XMM6: result = 25; break;
case SYMS_DwRegX86_XMM7: result = 26; break;
case SYMS_DwRegX86_MM0: result = 27; break;
case SYMS_DwRegX86_MM1: result = 28; break;
case SYMS_DwRegX86_MM2: result = 29; break;
case SYMS_DwRegX86_MM3: result = 30; break;
case SYMS_DwRegX86_MM4: result = 31; break;
case SYMS_DwRegX86_MM5: result = 32; break;
case SYMS_DwRegX86_MM6: result = 33; break;
case SYMS_DwRegX86_MM7: result = 34; break;
case SYMS_DwRegX86_FCW: result = 35; break;
case SYMS_DwRegX86_FSW: result = 36; break;
case SYMS_DwRegX86_MXCSR: result = 37; break;
case SYMS_DwRegX86_ES: result = 38; break;
case SYMS_DwRegX86_CS: result = 39; break;
case SYMS_DwRegX86_SS: result = 40; break;
case SYMS_DwRegX86_DS: result = 41; break;
case SYMS_DwRegX86_FS: result = 42; break;
case SYMS_DwRegX86_GS: result = 43; break;
case SYMS_DwRegX86_TR: result = 44; break;
case SYMS_DwRegX86_LDTR: result = 45; break;
}
return(result);
}
SYMS_API SYMS_U64
syms_enum_index_from_dwregx64(SYMS_U64 v){
SYMS_U64 result = 0;
switch ((SYMS_U32)v){
default: break;
case SYMS_DwRegX64_RAX: result = 0; break;
case SYMS_DwRegX64_RDX: result = 1; break;
case SYMS_DwRegX64_RCX: result = 2; break;
case SYMS_DwRegX64_RBX: result = 3; break;
case SYMS_DwRegX64_RSI: result = 4; break;
case SYMS_DwRegX64_RDI: result = 5; break;
case SYMS_DwRegX64_RBP: result = 6; break;
case SYMS_DwRegX64_RSP: result = 7; break;
case SYMS_DwRegX64_R8: result = 8; break;
case SYMS_DwRegX64_R9: result = 9; break;
case SYMS_DwRegX64_R10: result = 10; break;
case SYMS_DwRegX64_R11: result = 11; break;
case SYMS_DwRegX64_R12: result = 12; break;
case SYMS_DwRegX64_R13: result = 13; break;
case SYMS_DwRegX64_R14: result = 14; break;
case SYMS_DwRegX64_R15: result = 15; break;
case SYMS_DwRegX64_RIP: result = 16; break;
case SYMS_DwRegX64_XMM0: result = 17; break;
case SYMS_DwRegX64_XMM1: result = 18; break;
case SYMS_DwRegX64_XMM2: result = 19; break;
case SYMS_DwRegX64_XMM3: result = 20; break;
case SYMS_DwRegX64_XMM4: result = 21; break;
case SYMS_DwRegX64_XMM5: result = 22; break;
case SYMS_DwRegX64_XMM6: result = 23; break;
case SYMS_DwRegX64_XMM7: result = 24; break;
case SYMS_DwRegX64_XMM8: result = 25; break;
case SYMS_DwRegX64_XMM9: result = 26; break;
case SYMS_DwRegX64_XMM10: result = 27; break;
case SYMS_DwRegX64_XMM11: result = 28; break;
case SYMS_DwRegX64_XMM12: result = 29; break;
case SYMS_DwRegX64_XMM13: result = 30; break;
case SYMS_DwRegX64_XMM14: result = 31; break;
case SYMS_DwRegX64_XMM15: result = 32; break;
case SYMS_DwRegX64_ST0: result = 33; break;
case SYMS_DwRegX64_ST1: result = 34; break;
case SYMS_DwRegX64_ST2: result = 35; break;
case SYMS_DwRegX64_ST3: result = 36; break;
case SYMS_DwRegX64_ST4: result = 37; break;
case SYMS_DwRegX64_ST5: result = 38; break;
case SYMS_DwRegX64_ST6: result = 39; break;
case SYMS_DwRegX64_ST7: result = 40; break;
case SYMS_DwRegX64_MM0: result = 41; break;
case SYMS_DwRegX64_MM1: result = 42; break;
case SYMS_DwRegX64_MM2: result = 43; break;
case SYMS_DwRegX64_MM3: result = 44; break;
case SYMS_DwRegX64_MM4: result = 45; break;
case SYMS_DwRegX64_MM5: result = 46; break;
case SYMS_DwRegX64_MM6: result = 47; break;
case SYMS_DwRegX64_MM7: result = 48; break;
case SYMS_DwRegX64_RFLAGS: result = 49; break;
case SYMS_DwRegX64_ES: result = 50; break;
case SYMS_DwRegX64_CS: result = 51; break;
case SYMS_DwRegX64_SS: result = 52; break;
case SYMS_DwRegX64_DS: result = 53; break;
case SYMS_DwRegX64_FS: result = 54; break;
case SYMS_DwRegX64_GS: result = 55; break;
case SYMS_DwRegX64_FS_BASE: result = 56; break;
case SYMS_DwRegX64_GS_BASE: result = 57; break;
case SYMS_DwRegX64_TR: result = 58; break;
case SYMS_DwRegX64_LDTR: result = 59; break;
}
return(result);
}
SYMS_API SYMS_U64
syms_enum_index_from_dw_op(SYMS_U64 v){
SYMS_U64 result = 0;
switch ((SYMS_U64)v){
default: break;
case SYMS_DwOp_NULL: result = 0; break;
case SYMS_DwOp_ADDR: result = 1; break;
case SYMS_DwOp_DEREF: result = 2; break;
case SYMS_DwOp_CONST1U: result = 3; break;
case SYMS_DwOp_CONST1S: result = 4; break;
case SYMS_DwOp_CONST2U: result = 5; break;
case SYMS_DwOp_CONST2S: result = 6; break;
case SYMS_DwOp_CONST4U: result = 7; break;
case SYMS_DwOp_CONST4S: result = 8; break;
case SYMS_DwOp_CONST8U: result = 9; break;
case SYMS_DwOp_CONST8S: result = 10; break;
case SYMS_DwOp_CONSTU: result = 11; break;
case SYMS_DwOp_CONSTS: result = 12; break;
case SYMS_DwOp_DUP: result = 13; break;
case SYMS_DwOp_DROP: result = 14; break;
case SYMS_DwOp_OVER: result = 15; break;
case SYMS_DwOp_PICK: result = 16; break;
case SYMS_DwOp_SWAP: result = 17; break;
case SYMS_DwOp_ROT: result = 18; break;
case SYMS_DwOp_XDEREF: result = 19; break;
case SYMS_DwOp_ABS: result = 20; break;
case SYMS_DwOp_AND: result = 21; break;
case SYMS_DwOp_DIV: result = 22; break;
case SYMS_DwOp_MINUS: result = 23; break;
case SYMS_DwOp_MOD: result = 24; break;
case SYMS_DwOp_MUL: result = 25; break;
case SYMS_DwOp_NEG: result = 26; break;
case SYMS_DwOp_NOT: result = 27; break;
case SYMS_DwOp_OR: result = 28; break;
case SYMS_DwOp_PLUS: result = 29; break;
case SYMS_DwOp_PLUS_UCONST: result = 30; break;
case SYMS_DwOp_SHL: result = 31; break;
case SYMS_DwOp_SHR: result = 32; break;
case SYMS_DwOp_SHRA: result = 33; break;
case SYMS_DwOp_XOR: result = 34; break;
case SYMS_DwOp_SKIP: result = 35; break;
case SYMS_DwOp_BRA: result = 36; break;
case SYMS_DwOp_EQ: result = 37; break;
case SYMS_DwOp_GE: result = 38; break;
case SYMS_DwOp_GT: result = 39; break;
case SYMS_DwOp_LE: result = 40; break;
case SYMS_DwOp_LT: result = 41; break;
case SYMS_DwOp_NE: result = 42; break;
case SYMS_DwOp_LIT0: result = 43; break;
case SYMS_DwOp_LIT1: result = 44; break;
case SYMS_DwOp_LIT2: result = 45; break;
case SYMS_DwOp_LIT3: result = 46; break;
case SYMS_DwOp_LIT4: result = 47; break;
case SYMS_DwOp_LIT5: result = 48; break;
case SYMS_DwOp_LIT6: result = 49; break;
case SYMS_DwOp_LIT7: result = 50; break;
case SYMS_DwOp_LIT8: result = 51; break;
case SYMS_DwOp_LIT9: result = 52; break;
case SYMS_DwOp_LIT10: result = 53; break;
case SYMS_DwOp_LIT11: result = 54; break;
case SYMS_DwOp_LIT12: result = 55; break;
case SYMS_DwOp_LIT13: result = 56; break;
case SYMS_DwOp_LIT14: result = 57; break;
case SYMS_DwOp_LIT15: result = 58; break;
case SYMS_DwOp_LIT16: result = 59; break;
case SYMS_DwOp_LIT17: result = 60; break;
case SYMS_DwOp_LIT18: result = 61; break;
case SYMS_DwOp_LIT19: result = 62; break;
case SYMS_DwOp_LIT20: result = 63; break;
case SYMS_DwOp_LIT21: result = 64; break;
case SYMS_DwOp_LIT22: result = 65; break;
case SYMS_DwOp_LIT23: result = 66; break;
case SYMS_DwOp_LIT24: result = 67; break;
case SYMS_DwOp_LIT25: result = 68; break;
case SYMS_DwOp_LIT26: result = 69; break;
case SYMS_DwOp_LIT27: result = 70; break;
case SYMS_DwOp_LIT28: result = 71; break;
case SYMS_DwOp_LIT29: result = 72; break;
case SYMS_DwOp_LIT30: result = 73; break;
case SYMS_DwOp_LIT31: result = 74; break;
case SYMS_DwOp_REG0: result = 75; break;
case SYMS_DwOp_REG1: result = 76; break;
case SYMS_DwOp_REG2: result = 77; break;
case SYMS_DwOp_REG3: result = 78; break;
case SYMS_DwOp_REG4: result = 79; break;
case SYMS_DwOp_REG5: result = 80; break;
case SYMS_DwOp_REG6: result = 81; break;
case SYMS_DwOp_REG7: result = 82; break;
case SYMS_DwOp_REG8: result = 83; break;
case SYMS_DwOp_REG9: result = 84; break;
case SYMS_DwOp_REG10: result = 85; break;
case SYMS_DwOp_REG11: result = 86; break;
case SYMS_DwOp_REG12: result = 87; break;
case SYMS_DwOp_REG13: result = 88; break;
case SYMS_DwOp_REG14: result = 89; break;
case SYMS_DwOp_REG15: result = 90; break;
case SYMS_DwOp_REG16: result = 91; break;
case SYMS_DwOp_REG17: result = 92; break;
case SYMS_DwOp_REG18: result = 93; break;
case SYMS_DwOp_REG19: result = 94; break;
case SYMS_DwOp_REG20: result = 95; break;
case SYMS_DwOp_REG21: result = 96; break;
case SYMS_DwOp_REG22: result = 97; break;
case SYMS_DwOp_REG23: result = 98; break;
case SYMS_DwOp_REG24: result = 99; break;
case SYMS_DwOp_REG25: result = 100; break;
case SYMS_DwOp_REG26: result = 101; break;
case SYMS_DwOp_REG27: result = 102; break;
case SYMS_DwOp_REG28: result = 103; break;
case SYMS_DwOp_REG29: result = 104; break;
case SYMS_DwOp_REG30: result = 105; break;
case SYMS_DwOp_REG31: result = 106; break;
case SYMS_DwOp_BREG0: result = 107; break;
case SYMS_DwOp_BREG1: result = 108; break;
case SYMS_DwOp_BREG2: result = 109; break;
case SYMS_DwOp_BREG3: result = 110; break;
case SYMS_DwOp_BREG4: result = 111; break;
case SYMS_DwOp_BREG5: result = 112; break;
case SYMS_DwOp_BREG6: result = 113; break;
case SYMS_DwOp_BREG7: result = 114; break;
case SYMS_DwOp_BREG8: result = 115; break;
case SYMS_DwOp_BREG9: result = 116; break;
case SYMS_DwOp_BREG10: result = 117; break;
case SYMS_DwOp_BREG11: result = 118; break;
case SYMS_DwOp_BREG12: result = 119; break;
case SYMS_DwOp_BREG13: result = 120; break;
case SYMS_DwOp_BREG14: result = 121; break;
case SYMS_DwOp_BREG15: result = 122; break;
case SYMS_DwOp_BREG16: result = 123; break;
case SYMS_DwOp_BREG17: result = 124; break;
case SYMS_DwOp_BREG18: result = 125; break;
case SYMS_DwOp_BREG19: result = 126; break;
case SYMS_DwOp_BREG20: result = 127; break;
case SYMS_DwOp_BREG21: result = 128; break;
case SYMS_DwOp_BREG22: result = 129; break;
case SYMS_DwOp_BREG23: result = 130; break;
case SYMS_DwOp_BREG24: result = 131; break;
case SYMS_DwOp_BREG25: result = 132; break;
case SYMS_DwOp_BREG26: result = 133; break;
case SYMS_DwOp_BREG27: result = 134; break;
case SYMS_DwOp_BREG28: result = 135; break;
case SYMS_DwOp_BREG29: result = 136; break;
case SYMS_DwOp_BREG30: result = 137; break;
case SYMS_DwOp_BREG31: result = 138; break;
case SYMS_DwOp_REGX: result = 139; break;
case SYMS_DwOp_FBREG: result = 140; break;
case SYMS_DwOp_BREGX: result = 141; break;
case SYMS_DwOp_PIECE: result = 142; break;
case SYMS_DwOp_DEREF_SIZE: result = 143; break;
case SYMS_DwOp_XDEREF_SIZE: result = 144; break;
case SYMS_DwOp_NOP: result = 145; break;
case SYMS_DwOp_PUSH_OBJECT_ADDRESS: result = 146; break;
case SYMS_DwOp_CALL2: result = 147; break;
case SYMS_DwOp_CALL4: result = 148; break;
case SYMS_DwOp_CALL_REF: result = 149; break;
case SYMS_DwOp_FORM_TLS_ADDRESS: result = 150; break;
case SYMS_DwOp_CALL_FRAME_CFA: result = 151; break;
case SYMS_DwOp_BIT_PIECE: result = 152; break;
case SYMS_DwOp_IMPLICIT_VALUE: result = 153; break;
case SYMS_DwOp_STACK_VALUE: result = 154; break;
case SYMS_DwOp_IMPLICIT_POINTER: result = 155; break;
case SYMS_DwOp_ADDRX: result = 156; break;
case SYMS_DwOp_CONSTX: result = 157; break;
case SYMS_DwOp_ENTRY_VALUE: result = 158; break;
case SYMS_DwOp_CONST_TYPE: result = 159; break;
case SYMS_DwOp_REGVAL_TYPE: result = 160; break;
case SYMS_DwOp_DEREF_TYPE: result = 161; break;
case SYMS_DwOp_XDEREF_TYPE: result = 162; break;
case SYMS_DwOp_CONVERT: result = 163; break;
case SYMS_DwOp_REINTERPRET: result = 164; break;
case SYMS_DwOp_GNU_PUSH_TLS_ADDRESS: result = 165; break;
case SYMS_DwOp_GNU_UNINIT: result = 166; break;
case SYMS_DwOp_HI_USER: result = 168; break;
}
return(result);
}

//~ generated from code at syms/metaprogram/syms_metaprogram_serial.c:1322
SYMS_SerialValue _syms_serial_members_for_SYMS_DwRegX86[] = {
{ { (SYMS_U8*)"EAX", 3 }, (SYMS_U64)SYMS_DwRegX86_EAX },
{ { (SYMS_U8*)"ECX", 3 }, (SYMS_U64)SYMS_DwRegX86_ECX },
{ { (SYMS_U8*)"EDX", 3 }, (SYMS_U64)SYMS_DwRegX86_EDX },
{ { (SYMS_U8*)"EBX", 3 }, (SYMS_U64)SYMS_DwRegX86_EBX },
{ { (SYMS_U8*)"ESP", 3 }, (SYMS_U64)SYMS_DwRegX86_ESP },
{ { (SYMS_U8*)"EBP", 3 }, (SYMS_U64)SYMS_DwRegX86_EBP },
{ { (SYMS_U8*)"ESI", 3 }, (SYMS_U64)SYMS_DwRegX86_ESI },
{ { (SYMS_U8*)"EDI", 3 }, (SYMS_U64)SYMS_DwRegX86_EDI },
{ { (SYMS_U8*)"EIP", 3 }, (SYMS_U64)SYMS_DwRegX86_EIP },
{ { (SYMS_U8*)"EFLAGS", 6 }, (SYMS_U64)SYMS_DwRegX86_EFLAGS },
{ { (SYMS_U8*)"TRAPNO", 6 }, (SYMS_U64)SYMS_DwRegX86_TRAPNO },
{ { (SYMS_U8*)"ST0", 3 }, (SYMS_U64)SYMS_DwRegX86_ST0 },
{ { (SYMS_U8*)"ST1", 3 }, (SYMS_U64)SYMS_DwRegX86_ST1 },
{ { (SYMS_U8*)"ST2", 3 }, (SYMS_U64)SYMS_DwRegX86_ST2 },
{ { (SYMS_U8*)"ST3", 3 }, (SYMS_U64)SYMS_DwRegX86_ST3 },
{ { (SYMS_U8*)"ST4", 3 }, (SYMS_U64)SYMS_DwRegX86_ST4 },
{ { (SYMS_U8*)"ST5", 3 }, (SYMS_U64)SYMS_DwRegX86_ST5 },
{ { (SYMS_U8*)"ST6", 3 }, (SYMS_U64)SYMS_DwRegX86_ST6 },
{ { (SYMS_U8*)"ST7", 3 }, (SYMS_U64)SYMS_DwRegX86_ST7 },
{ { (SYMS_U8*)"XMM0", 4 }, (SYMS_U64)SYMS_DwRegX86_XMM0 },
{ { (SYMS_U8*)"XMM1", 4 }, (SYMS_U64)SYMS_DwRegX86_XMM1 },
{ { (SYMS_U8*)"XMM2", 4 }, (SYMS_U64)SYMS_DwRegX86_XMM2 },
{ { (SYMS_U8*)"XMM3", 4 }, (SYMS_U64)SYMS_DwRegX86_XMM3 },
{ { (SYMS_U8*)"XMM4", 4 }, (SYMS_U64)SYMS_DwRegX86_XMM4 },
{ { (SYMS_U8*)"XMM5", 4 }, (SYMS_U64)SYMS_DwRegX86_XMM5 },
{ { (SYMS_U8*)"XMM6", 4 }, (SYMS_U64)SYMS_DwRegX86_XMM6 },
{ { (SYMS_U8*)"XMM7", 4 }, (SYMS_U64)SYMS_DwRegX86_XMM7 },
{ { (SYMS_U8*)"MM0", 3 }, (SYMS_U64)SYMS_DwRegX86_MM0 },
{ { (SYMS_U8*)"MM1", 3 }, (SYMS_U64)SYMS_DwRegX86_MM1 },
{ { (SYMS_U8*)"MM2", 3 }, (SYMS_U64)SYMS_DwRegX86_MM2 },
{ { (SYMS_U8*)"MM3", 3 }, (SYMS_U64)SYMS_DwRegX86_MM3 },
{ { (SYMS_U8*)"MM4", 3 }, (SYMS_U64)SYMS_DwRegX86_MM4 },
{ { (SYMS_U8*)"MM5", 3 }, (SYMS_U64)SYMS_DwRegX86_MM5 },
{ { (SYMS_U8*)"MM6", 3 }, (SYMS_U64)SYMS_DwRegX86_MM6 },
{ { (SYMS_U8*)"MM7", 3 }, (SYMS_U64)SYMS_DwRegX86_MM7 },
{ { (SYMS_U8*)"FCW", 3 }, (SYMS_U64)SYMS_DwRegX86_FCW },
{ { (SYMS_U8*)"FSW", 3 }, (SYMS_U64)SYMS_DwRegX86_FSW },
{ { (SYMS_U8*)"MXCSR", 5 }, (SYMS_U64)SYMS_DwRegX86_MXCSR },
{ { (SYMS_U8*)"ES", 2 }, (SYMS_U64)SYMS_DwRegX86_ES },
{ { (SYMS_U8*)"CS", 2 }, (SYMS_U64)SYMS_DwRegX86_CS },
{ { (SYMS_U8*)"SS", 2 }, (SYMS_U64)SYMS_DwRegX86_SS },
{ { (SYMS_U8*)"DS", 2 }, (SYMS_U64)SYMS_DwRegX86_DS },
{ { (SYMS_U8*)"FS", 2 }, (SYMS_U64)SYMS_DwRegX86_FS },
{ { (SYMS_U8*)"GS", 2 }, (SYMS_U64)SYMS_DwRegX86_GS },
{ { (SYMS_U8*)"TR", 2 }, (SYMS_U64)SYMS_DwRegX86_TR },
{ { (SYMS_U8*)"LDTR", 4 }, (SYMS_U64)SYMS_DwRegX86_LDTR },
};
SYMS_SerialValue _syms_serial_members_for_SYMS_DwRegX64[] = {
{ { (SYMS_U8*)"RAX", 3 }, (SYMS_U64)SYMS_DwRegX64_RAX },
{ { (SYMS_U8*)"RDX", 3 }, (SYMS_U64)SYMS_DwRegX64_RDX },
{ { (SYMS_U8*)"RCX", 3 }, (SYMS_U64)SYMS_DwRegX64_RCX },
{ { (SYMS_U8*)"RBX", 3 }, (SYMS_U64)SYMS_DwRegX64_RBX },
{ { (SYMS_U8*)"RSI", 3 }, (SYMS_U64)SYMS_DwRegX64_RSI },
{ { (SYMS_U8*)"RDI", 3 }, (SYMS_U64)SYMS_DwRegX64_RDI },
{ { (SYMS_U8*)"RBP", 3 }, (SYMS_U64)SYMS_DwRegX64_RBP },
{ { (SYMS_U8*)"RSP", 3 }, (SYMS_U64)SYMS_DwRegX64_RSP },
{ { (SYMS_U8*)"R8", 2 }, (SYMS_U64)SYMS_DwRegX64_R8 },
{ { (SYMS_U8*)"R9", 2 }, (SYMS_U64)SYMS_DwRegX64_R9 },
{ { (SYMS_U8*)"R10", 3 }, (SYMS_U64)SYMS_DwRegX64_R10 },
{ { (SYMS_U8*)"R11", 3 }, (SYMS_U64)SYMS_DwRegX64_R11 },
{ { (SYMS_U8*)"R12", 3 }, (SYMS_U64)SYMS_DwRegX64_R12 },
{ { (SYMS_U8*)"R13", 3 }, (SYMS_U64)SYMS_DwRegX64_R13 },
{ { (SYMS_U8*)"R14", 3 }, (SYMS_U64)SYMS_DwRegX64_R14 },
{ { (SYMS_U8*)"R15", 3 }, (SYMS_U64)SYMS_DwRegX64_R15 },
{ { (SYMS_U8*)"RIP", 3 }, (SYMS_U64)SYMS_DwRegX64_RIP },
{ { (SYMS_U8*)"XMM0", 4 }, (SYMS_U64)SYMS_DwRegX64_XMM0 },
{ { (SYMS_U8*)"XMM1", 4 }, (SYMS_U64)SYMS_DwRegX64_XMM1 },
{ { (SYMS_U8*)"XMM2", 4 }, (SYMS_U64)SYMS_DwRegX64_XMM2 },
{ { (SYMS_U8*)"XMM3", 4 }, (SYMS_U64)SYMS_DwRegX64_XMM3 },
{ { (SYMS_U8*)"XMM4", 4 }, (SYMS_U64)SYMS_DwRegX64_XMM4 },
{ { (SYMS_U8*)"XMM5", 4 }, (SYMS_U64)SYMS_DwRegX64_XMM5 },
{ { (SYMS_U8*)"XMM6", 4 }, (SYMS_U64)SYMS_DwRegX64_XMM6 },
{ { (SYMS_U8*)"XMM7", 4 }, (SYMS_U64)SYMS_DwRegX64_XMM7 },
{ { (SYMS_U8*)"XMM8", 4 }, (SYMS_U64)SYMS_DwRegX64_XMM8 },
{ { (SYMS_U8*)"XMM9", 4 }, (SYMS_U64)SYMS_DwRegX64_XMM9 },
{ { (SYMS_U8*)"XMM10", 5 }, (SYMS_U64)SYMS_DwRegX64_XMM10 },
{ { (SYMS_U8*)"XMM11", 5 }, (SYMS_U64)SYMS_DwRegX64_XMM11 },
{ { (SYMS_U8*)"XMM12", 5 }, (SYMS_U64)SYMS_DwRegX64_XMM12 },
{ { (SYMS_U8*)"XMM13", 5 }, (SYMS_U64)SYMS_DwRegX64_XMM13 },
{ { (SYMS_U8*)"XMM14", 5 }, (SYMS_U64)SYMS_DwRegX64_XMM14 },
{ { (SYMS_U8*)"XMM15", 5 }, (SYMS_U64)SYMS_DwRegX64_XMM15 },
{ { (SYMS_U8*)"ST0", 3 }, (SYMS_U64)SYMS_DwRegX64_ST0 },
{ { (SYMS_U8*)"ST1", 3 }, (SYMS_U64)SYMS_DwRegX64_ST1 },
{ { (SYMS_U8*)"ST2", 3 }, (SYMS_U64)SYMS_DwRegX64_ST2 },
{ { (SYMS_U8*)"ST3", 3 }, (SYMS_U64)SYMS_DwRegX64_ST3 },
{ { (SYMS_U8*)"ST4", 3 }, (SYMS_U64)SYMS_DwRegX64_ST4 },
{ { (SYMS_U8*)"ST5", 3 }, (SYMS_U64)SYMS_DwRegX64_ST5 },
{ { (SYMS_U8*)"ST6", 3 }, (SYMS_U64)SYMS_DwRegX64_ST6 },
{ { (SYMS_U8*)"ST7", 3 }, (SYMS_U64)SYMS_DwRegX64_ST7 },
{ { (SYMS_U8*)"MM0", 3 }, (SYMS_U64)SYMS_DwRegX64_MM0 },
{ { (SYMS_U8*)"MM1", 3 }, (SYMS_U64)SYMS_DwRegX64_MM1 },
{ { (SYMS_U8*)"MM2", 3 }, (SYMS_U64)SYMS_DwRegX64_MM2 },
{ { (SYMS_U8*)"MM3", 3 }, (SYMS_U64)SYMS_DwRegX64_MM3 },
{ { (SYMS_U8*)"MM4", 3 }, (SYMS_U64)SYMS_DwRegX64_MM4 },
{ { (SYMS_U8*)"MM5", 3 }, (SYMS_U64)SYMS_DwRegX64_MM5 },
{ { (SYMS_U8*)"MM6", 3 }, (SYMS_U64)SYMS_DwRegX64_MM6 },
{ { (SYMS_U8*)"MM7", 3 }, (SYMS_U64)SYMS_DwRegX64_MM7 },
{ { (SYMS_U8*)"RFLAGS", 6 }, (SYMS_U64)SYMS_DwRegX64_RFLAGS },
{ { (SYMS_U8*)"ES", 2 }, (SYMS_U64)SYMS_DwRegX64_ES },
{ { (SYMS_U8*)"CS", 2 }, (SYMS_U64)SYMS_DwRegX64_CS },
{ { (SYMS_U8*)"SS", 2 }, (SYMS_U64)SYMS_DwRegX64_SS },
{ { (SYMS_U8*)"DS", 2 }, (SYMS_U64)SYMS_DwRegX64_DS },
{ { (SYMS_U8*)"FS", 2 }, (SYMS_U64)SYMS_DwRegX64_FS },
{ { (SYMS_U8*)"GS", 2 }, (SYMS_U64)SYMS_DwRegX64_GS },
{ { (SYMS_U8*)"FS_BASE", 7 }, (SYMS_U64)SYMS_DwRegX64_FS_BASE },
{ { (SYMS_U8*)"GS_BASE", 7 }, (SYMS_U64)SYMS_DwRegX64_GS_BASE },
{ { (SYMS_U8*)"TR", 2 }, (SYMS_U64)SYMS_DwRegX64_TR },
{ { (SYMS_U8*)"LDTR", 4 }, (SYMS_U64)SYMS_DwRegX64_LDTR },
};
SYMS_SerialValue _syms_serial_members_for_SYMS_DwOp[] = {
{ { (SYMS_U8*)"NULL", 4 }, (SYMS_U64)SYMS_DwOp_NULL },
{ { (SYMS_U8*)"ADDR", 4 }, (SYMS_U64)SYMS_DwOp_ADDR },
{ { (SYMS_U8*)"DEREF", 5 }, (SYMS_U64)SYMS_DwOp_DEREF },
{ { (SYMS_U8*)"CONST1U", 7 }, (SYMS_U64)SYMS_DwOp_CONST1U },
{ { (SYMS_U8*)"CONST1S", 7 }, (SYMS_U64)SYMS_DwOp_CONST1S },
{ { (SYMS_U8*)"CONST2U", 7 }, (SYMS_U64)SYMS_DwOp_CONST2U },
{ { (SYMS_U8*)"CONST2S", 7 }, (SYMS_U64)SYMS_DwOp_CONST2S },
{ { (SYMS_U8*)"CONST4U", 7 }, (SYMS_U64)SYMS_DwOp_CONST4U },
{ { (SYMS_U8*)"CONST4S", 7 }, (SYMS_U64)SYMS_DwOp_CONST4S },
{ { (SYMS_U8*)"CONST8U", 7 }, (SYMS_U64)SYMS_DwOp_CONST8U },
{ { (SYMS_U8*)"CONST8S", 7 }, (SYMS_U64)SYMS_DwOp_CONST8S },
{ { (SYMS_U8*)"CONSTU", 6 }, (SYMS_U64)SYMS_DwOp_CONSTU },
{ { (SYMS_U8*)"CONSTS", 6 }, (SYMS_U64)SYMS_DwOp_CONSTS },
{ { (SYMS_U8*)"DUP", 3 }, (SYMS_U64)SYMS_DwOp_DUP },
{ { (SYMS_U8*)"DROP", 4 }, (SYMS_U64)SYMS_DwOp_DROP },
{ { (SYMS_U8*)"OVER", 4 }, (SYMS_U64)SYMS_DwOp_OVER },
{ { (SYMS_U8*)"PICK", 4 }, (SYMS_U64)SYMS_DwOp_PICK },
{ { (SYMS_U8*)"SWAP", 4 }, (SYMS_U64)SYMS_DwOp_SWAP },
{ { (SYMS_U8*)"ROT", 3 }, (SYMS_U64)SYMS_DwOp_ROT },
{ { (SYMS_U8*)"XDEREF", 6 }, (SYMS_U64)SYMS_DwOp_XDEREF },
{ { (SYMS_U8*)"ABS", 3 }, (SYMS_U64)SYMS_DwOp_ABS },
{ { (SYMS_U8*)"AND", 3 }, (SYMS_U64)SYMS_DwOp_AND },
{ { (SYMS_U8*)"DIV", 3 }, (SYMS_U64)SYMS_DwOp_DIV },
{ { (SYMS_U8*)"MINUS", 5 }, (SYMS_U64)SYMS_DwOp_MINUS },
{ { (SYMS_U8*)"MOD", 3 }, (SYMS_U64)SYMS_DwOp_MOD },
{ { (SYMS_U8*)"MUL", 3 }, (SYMS_U64)SYMS_DwOp_MUL },
{ { (SYMS_U8*)"NEG", 3 }, (SYMS_U64)SYMS_DwOp_NEG },
{ { (SYMS_U8*)"NOT", 3 }, (SYMS_U64)SYMS_DwOp_NOT },
{ { (SYMS_U8*)"OR", 2 }, (SYMS_U64)SYMS_DwOp_OR },
{ { (SYMS_U8*)"PLUS", 4 }, (SYMS_U64)SYMS_DwOp_PLUS },
{ { (SYMS_U8*)"PLUS_UCONST", 11 }, (SYMS_U64)SYMS_DwOp_PLUS_UCONST },
{ { (SYMS_U8*)"SHL", 3 }, (SYMS_U64)SYMS_DwOp_SHL },
{ { (SYMS_U8*)"SHR", 3 }, (SYMS_U64)SYMS_DwOp_SHR },
{ { (SYMS_U8*)"SHRA", 4 }, (SYMS_U64)SYMS_DwOp_SHRA },
{ { (SYMS_U8*)"XOR", 3 }, (SYMS_U64)SYMS_DwOp_XOR },
{ { (SYMS_U8*)"SKIP", 4 }, (SYMS_U64)SYMS_DwOp_SKIP },
{ { (SYMS_U8*)"BRA", 3 }, (SYMS_U64)SYMS_DwOp_BRA },
{ { (SYMS_U8*)"EQ", 2 }, (SYMS_U64)SYMS_DwOp_EQ },
{ { (SYMS_U8*)"GE", 2 }, (SYMS_U64)SYMS_DwOp_GE },
{ { (SYMS_U8*)"GT", 2 }, (SYMS_U64)SYMS_DwOp_GT },
{ { (SYMS_U8*)"LE", 2 }, (SYMS_U64)SYMS_DwOp_LE },
{ { (SYMS_U8*)"LT", 2 }, (SYMS_U64)SYMS_DwOp_LT },
{ { (SYMS_U8*)"NE", 2 }, (SYMS_U64)SYMS_DwOp_NE },
{ { (SYMS_U8*)"LIT0", 4 }, (SYMS_U64)SYMS_DwOp_LIT0 },
{ { (SYMS_U8*)"LIT1", 4 }, (SYMS_U64)SYMS_DwOp_LIT1 },
{ { (SYMS_U8*)"LIT2", 4 }, (SYMS_U64)SYMS_DwOp_LIT2 },
{ { (SYMS_U8*)"LIT3", 4 }, (SYMS_U64)SYMS_DwOp_LIT3 },
{ { (SYMS_U8*)"LIT4", 4 }, (SYMS_U64)SYMS_DwOp_LIT4 },
{ { (SYMS_U8*)"LIT5", 4 }, (SYMS_U64)SYMS_DwOp_LIT5 },
{ { (SYMS_U8*)"LIT6", 4 }, (SYMS_U64)SYMS_DwOp_LIT6 },
{ { (SYMS_U8*)"LIT7", 4 }, (SYMS_U64)SYMS_DwOp_LIT7 },
{ { (SYMS_U8*)"LIT8", 4 }, (SYMS_U64)SYMS_DwOp_LIT8 },
{ { (SYMS_U8*)"LIT9", 4 }, (SYMS_U64)SYMS_DwOp_LIT9 },
{ { (SYMS_U8*)"LIT10", 5 }, (SYMS_U64)SYMS_DwOp_LIT10 },
{ { (SYMS_U8*)"LIT11", 5 }, (SYMS_U64)SYMS_DwOp_LIT11 },
{ { (SYMS_U8*)"LIT12", 5 }, (SYMS_U64)SYMS_DwOp_LIT12 },
{ { (SYMS_U8*)"LIT13", 5 }, (SYMS_U64)SYMS_DwOp_LIT13 },
{ { (SYMS_U8*)"LIT14", 5 }, (SYMS_U64)SYMS_DwOp_LIT14 },
{ { (SYMS_U8*)"LIT15", 5 }, (SYMS_U64)SYMS_DwOp_LIT15 },
{ { (SYMS_U8*)"LIT16", 5 }, (SYMS_U64)SYMS_DwOp_LIT16 },
{ { (SYMS_U8*)"LIT17", 5 }, (SYMS_U64)SYMS_DwOp_LIT17 },
{ { (SYMS_U8*)"LIT18", 5 }, (SYMS_U64)SYMS_DwOp_LIT18 },
{ { (SYMS_U8*)"LIT19", 5 }, (SYMS_U64)SYMS_DwOp_LIT19 },
{ { (SYMS_U8*)"LIT20", 5 }, (SYMS_U64)SYMS_DwOp_LIT20 },
{ { (SYMS_U8*)"LIT21", 5 }, (SYMS_U64)SYMS_DwOp_LIT21 },
{ { (SYMS_U8*)"LIT22", 5 }, (SYMS_U64)SYMS_DwOp_LIT22 },
{ { (SYMS_U8*)"LIT23", 5 }, (SYMS_U64)SYMS_DwOp_LIT23 },
{ { (SYMS_U8*)"LIT24", 5 }, (SYMS_U64)SYMS_DwOp_LIT24 },
{ { (SYMS_U8*)"LIT25", 5 }, (SYMS_U64)SYMS_DwOp_LIT25 },
{ { (SYMS_U8*)"LIT26", 5 }, (SYMS_U64)SYMS_DwOp_LIT26 },
{ { (SYMS_U8*)"LIT27", 5 }, (SYMS_U64)SYMS_DwOp_LIT27 },
{ { (SYMS_U8*)"LIT28", 5 }, (SYMS_U64)SYMS_DwOp_LIT28 },
{ { (SYMS_U8*)"LIT29", 5 }, (SYMS_U64)SYMS_DwOp_LIT29 },
{ { (SYMS_U8*)"LIT30", 5 }, (SYMS_U64)SYMS_DwOp_LIT30 },
{ { (SYMS_U8*)"LIT31", 5 }, (SYMS_U64)SYMS_DwOp_LIT31 },
{ { (SYMS_U8*)"REG0", 4 }, (SYMS_U64)SYMS_DwOp_REG0 },
{ { (SYMS_U8*)"REG1", 4 }, (SYMS_U64)SYMS_DwOp_REG1 },
{ { (SYMS_U8*)"REG2", 4 }, (SYMS_U64)SYMS_DwOp_REG2 },
{ { (SYMS_U8*)"REG3", 4 }, (SYMS_U64)SYMS_DwOp_REG3 },
{ { (SYMS_U8*)"REG4", 4 }, (SYMS_U64)SYMS_DwOp_REG4 },
{ { (SYMS_U8*)"REG5", 4 }, (SYMS_U64)SYMS_DwOp_REG5 },
{ { (SYMS_U8*)"REG6", 4 }, (SYMS_U64)SYMS_DwOp_REG6 },
{ { (SYMS_U8*)"REG7", 4 }, (SYMS_U64)SYMS_DwOp_REG7 },
{ { (SYMS_U8*)"REG8", 4 }, (SYMS_U64)SYMS_DwOp_REG8 },
{ { (SYMS_U8*)"REG9", 4 }, (SYMS_U64)SYMS_DwOp_REG9 },
{ { (SYMS_U8*)"REG10", 5 }, (SYMS_U64)SYMS_DwOp_REG10 },
{ { (SYMS_U8*)"REG11", 5 }, (SYMS_U64)SYMS_DwOp_REG11 },
{ { (SYMS_U8*)"REG12", 5 }, (SYMS_U64)SYMS_DwOp_REG12 },
{ { (SYMS_U8*)"REG13", 5 }, (SYMS_U64)SYMS_DwOp_REG13 },
{ { (SYMS_U8*)"REG14", 5 }, (SYMS_U64)SYMS_DwOp_REG14 },
{ { (SYMS_U8*)"REG15", 5 }, (SYMS_U64)SYMS_DwOp_REG15 },
{ { (SYMS_U8*)"REG16", 5 }, (SYMS_U64)SYMS_DwOp_REG16 },
{ { (SYMS_U8*)"REG17", 5 }, (SYMS_U64)SYMS_DwOp_REG17 },
{ { (SYMS_U8*)"REG18", 5 }, (SYMS_U64)SYMS_DwOp_REG18 },
{ { (SYMS_U8*)"REG19", 5 }, (SYMS_U64)SYMS_DwOp_REG19 },
{ { (SYMS_U8*)"REG20", 5 }, (SYMS_U64)SYMS_DwOp_REG20 },
{ { (SYMS_U8*)"REG21", 5 }, (SYMS_U64)SYMS_DwOp_REG21 },
{ { (SYMS_U8*)"REG22", 5 }, (SYMS_U64)SYMS_DwOp_REG22 },
{ { (SYMS_U8*)"REG23", 5 }, (SYMS_U64)SYMS_DwOp_REG23 },
{ { (SYMS_U8*)"REG24", 5 }, (SYMS_U64)SYMS_DwOp_REG24 },
{ { (SYMS_U8*)"REG25", 5 }, (SYMS_U64)SYMS_DwOp_REG25 },
{ { (SYMS_U8*)"REG26", 5 }, (SYMS_U64)SYMS_DwOp_REG26 },
{ { (SYMS_U8*)"REG27", 5 }, (SYMS_U64)SYMS_DwOp_REG27 },
{ { (SYMS_U8*)"REG28", 5 }, (SYMS_U64)SYMS_DwOp_REG28 },
{ { (SYMS_U8*)"REG29", 5 }, (SYMS_U64)SYMS_DwOp_REG29 },
{ { (SYMS_U8*)"REG30", 5 }, (SYMS_U64)SYMS_DwOp_REG30 },
{ { (SYMS_U8*)"REG31", 5 }, (SYMS_U64)SYMS_DwOp_REG31 },
{ { (SYMS_U8*)"BREG0", 5 }, (SYMS_U64)SYMS_DwOp_BREG0 },
{ { (SYMS_U8*)"BREG1", 5 }, (SYMS_U64)SYMS_DwOp_BREG1 },
{ { (SYMS_U8*)"BREG2", 5 }, (SYMS_U64)SYMS_DwOp_BREG2 },
{ { (SYMS_U8*)"BREG3", 5 }, (SYMS_U64)SYMS_DwOp_BREG3 },
{ { (SYMS_U8*)"BREG4", 5 }, (SYMS_U64)SYMS_DwOp_BREG4 },
{ { (SYMS_U8*)"BREG5", 5 }, (SYMS_U64)SYMS_DwOp_BREG5 },
{ { (SYMS_U8*)"BREG6", 5 }, (SYMS_U64)SYMS_DwOp_BREG6 },
{ { (SYMS_U8*)"BREG7", 5 }, (SYMS_U64)SYMS_DwOp_BREG7 },
{ { (SYMS_U8*)"BREG8", 5 }, (SYMS_U64)SYMS_DwOp_BREG8 },
{ { (SYMS_U8*)"BREG9", 5 }, (SYMS_U64)SYMS_DwOp_BREG9 },
{ { (SYMS_U8*)"BREG10", 6 }, (SYMS_U64)SYMS_DwOp_BREG10 },
{ { (SYMS_U8*)"BREG11", 6 }, (SYMS_U64)SYMS_DwOp_BREG11 },
{ { (SYMS_U8*)"BREG12", 6 }, (SYMS_U64)SYMS_DwOp_BREG12 },
{ { (SYMS_U8*)"BREG13", 6 }, (SYMS_U64)SYMS_DwOp_BREG13 },
{ { (SYMS_U8*)"BREG14", 6 }, (SYMS_U64)SYMS_DwOp_BREG14 },
{ { (SYMS_U8*)"BREG15", 6 }, (SYMS_U64)SYMS_DwOp_BREG15 },
{ { (SYMS_U8*)"BREG16", 6 }, (SYMS_U64)SYMS_DwOp_BREG16 },
{ { (SYMS_U8*)"BREG17", 6 }, (SYMS_U64)SYMS_DwOp_BREG17 },
{ { (SYMS_U8*)"BREG18", 6 }, (SYMS_U64)SYMS_DwOp_BREG18 },
{ { (SYMS_U8*)"BREG19", 6 }, (SYMS_U64)SYMS_DwOp_BREG19 },
{ { (SYMS_U8*)"BREG20", 6 }, (SYMS_U64)SYMS_DwOp_BREG20 },
{ { (SYMS_U8*)"BREG21", 6 }, (SYMS_U64)SYMS_DwOp_BREG21 },
{ { (SYMS_U8*)"BREG22", 6 }, (SYMS_U64)SYMS_DwOp_BREG22 },
{ { (SYMS_U8*)"BREG23", 6 }, (SYMS_U64)SYMS_DwOp_BREG23 },
{ { (SYMS_U8*)"BREG24", 6 }, (SYMS_U64)SYMS_DwOp_BREG24 },
{ { (SYMS_U8*)"BREG25", 6 }, (SYMS_U64)SYMS_DwOp_BREG25 },
{ { (SYMS_U8*)"BREG26", 6 }, (SYMS_U64)SYMS_DwOp_BREG26 },
{ { (SYMS_U8*)"BREG27", 6 }, (SYMS_U64)SYMS_DwOp_BREG27 },
{ { (SYMS_U8*)"BREG28", 6 }, (SYMS_U64)SYMS_DwOp_BREG28 },
{ { (SYMS_U8*)"BREG29", 6 }, (SYMS_U64)SYMS_DwOp_BREG29 },
{ { (SYMS_U8*)"BREG30", 6 }, (SYMS_U64)SYMS_DwOp_BREG30 },
{ { (SYMS_U8*)"BREG31", 6 }, (SYMS_U64)SYMS_DwOp_BREG31 },
{ { (SYMS_U8*)"REGX", 4 }, (SYMS_U64)SYMS_DwOp_REGX },
{ { (SYMS_U8*)"FBREG", 5 }, (SYMS_U64)SYMS_DwOp_FBREG },
{ { (SYMS_U8*)"BREGX", 5 }, (SYMS_U64)SYMS_DwOp_BREGX },
{ { (SYMS_U8*)"PIECE", 5 }, (SYMS_U64)SYMS_DwOp_PIECE },
{ { (SYMS_U8*)"DEREF_SIZE", 10 }, (SYMS_U64)SYMS_DwOp_DEREF_SIZE },
{ { (SYMS_U8*)"XDEREF_SIZE", 11 }, (SYMS_U64)SYMS_DwOp_XDEREF_SIZE },
{ { (SYMS_U8*)"NOP", 3 }, (SYMS_U64)SYMS_DwOp_NOP },
{ { (SYMS_U8*)"PUSH_OBJECT_ADDRESS", 19 }, (SYMS_U64)SYMS_DwOp_PUSH_OBJECT_ADDRESS },
{ { (SYMS_U8*)"CALL2", 5 }, (SYMS_U64)SYMS_DwOp_CALL2 },
{ { (SYMS_U8*)"CALL4", 5 }, (SYMS_U64)SYMS_DwOp_CALL4 },
{ { (SYMS_U8*)"CALL_REF", 8 }, (SYMS_U64)SYMS_DwOp_CALL_REF },
{ { (SYMS_U8*)"FORM_TLS_ADDRESS", 16 }, (SYMS_U64)SYMS_DwOp_FORM_TLS_ADDRESS },
{ { (SYMS_U8*)"CALL_FRAME_CFA", 14 }, (SYMS_U64)SYMS_DwOp_CALL_FRAME_CFA },
{ { (SYMS_U8*)"BIT_PIECE", 9 }, (SYMS_U64)SYMS_DwOp_BIT_PIECE },
{ { (SYMS_U8*)"IMPLICIT_VALUE", 14 }, (SYMS_U64)SYMS_DwOp_IMPLICIT_VALUE },
{ { (SYMS_U8*)"STACK_VALUE", 11 }, (SYMS_U64)SYMS_DwOp_STACK_VALUE },
{ { (SYMS_U8*)"IMPLICIT_POINTER", 16 }, (SYMS_U64)SYMS_DwOp_IMPLICIT_POINTER },
{ { (SYMS_U8*)"ADDRX", 5 }, (SYMS_U64)SYMS_DwOp_ADDRX },
{ { (SYMS_U8*)"CONSTX", 6 }, (SYMS_U64)SYMS_DwOp_CONSTX },
{ { (SYMS_U8*)"ENTRY_VALUE", 11 }, (SYMS_U64)SYMS_DwOp_ENTRY_VALUE },
{ { (SYMS_U8*)"CONST_TYPE", 10 }, (SYMS_U64)SYMS_DwOp_CONST_TYPE },
{ { (SYMS_U8*)"REGVAL_TYPE", 11 }, (SYMS_U64)SYMS_DwOp_REGVAL_TYPE },
{ { (SYMS_U8*)"DEREF_TYPE", 10 }, (SYMS_U64)SYMS_DwOp_DEREF_TYPE },
{ { (SYMS_U8*)"XDEREF_TYPE", 11 }, (SYMS_U64)SYMS_DwOp_XDEREF_TYPE },
{ { (SYMS_U8*)"CONVERT", 7 }, (SYMS_U64)SYMS_DwOp_CONVERT },
{ { (SYMS_U8*)"REINTERPRET", 11 }, (SYMS_U64)SYMS_DwOp_REINTERPRET },
{ { (SYMS_U8*)"GNU_PUSH_TLS_ADDRESS", 20 }, (SYMS_U64)SYMS_DwOp_GNU_PUSH_TLS_ADDRESS },
{ { (SYMS_U8*)"GNU_UNINIT", 10 }, (SYMS_U64)SYMS_DwOp_GNU_UNINIT },
{ { (SYMS_U8*)"LO_USER", 7 }, (SYMS_U64)SYMS_DwOp_LO_USER },
{ { (SYMS_U8*)"HI_USER", 7 }, (SYMS_U64)SYMS_DwOp_HI_USER },
};

//~ generated from code at syms/metaprogram/syms_metaprogram_serial.c:1458
SYMS_SerialType _syms_serial_type_SYMS_DwRegX86 = {
{(SYMS_U8*)"SYMS_DwRegX86", 13}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_DwRegX86), _syms_serial_members_for_SYMS_DwRegX86, sizeof(SYMS_DwRegX86), syms_enum_index_from_dwregx86
};
SYMS_SerialType _syms_serial_type_SYMS_DwRegX64 = {
{(SYMS_U8*)"SYMS_DwRegX64", 13}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_DwRegX64), _syms_serial_members_for_SYMS_DwRegX64, sizeof(SYMS_DwRegX64), syms_enum_index_from_dwregx64
};
SYMS_SerialType _syms_serial_type_SYMS_DwOp = {
{(SYMS_U8*)"SYMS_DwOp", 9}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_DwOp), _syms_serial_members_for_SYMS_DwOp, sizeof(SYMS_DwOp), syms_enum_index_from_dw_op
};

#endif // defined(SYMS_ENABLE_DWARF_SERIAL_INFO)


////////////////////////////////
#if defined(SYMS_ENABLE_ELF_SERIAL_INFO)

//~ generated from code at syms/metaprogram/syms_metaprogram_serial.c:1091
// syms_enum_index_from_elf_class - skipped identity mapping
SYMS_API SYMS_U64
syms_enum_index_from_elf_os_abi(SYMS_U64 v){
SYMS_U64 result = 0;
switch ((SYMS_U8)v){
default: break;
case SYMS_ElfOsAbi_NONE: result = 0; break;
case SYMS_ElfOsAbi_SYSV: result = 1; break;
case SYMS_ElfOsAbi_HPUX: result = 2; break;
case SYMS_ElfOsAbi_NETBSD: result = 3; break;
case SYMS_ElfOsAbi_GNU: result = 4; break;
case SYMS_ElfOsAbi_LINUX: result = 5; break;
case SYMS_ElfOsAbi_SOLARIS: result = 6; break;
case SYMS_ElfOsAbi_IRIX: result = 7; break;
case SYMS_ElfOsAbi_FREEBSD: result = 8; break;
case SYMS_ElfOsAbi_TRU64: result = 9; break;
case SYMS_ElfOsAbi_ARM: result = 10; break;
case SYMS_ElfOsAbi_STANDALONE: result = 11; break;
}
return(result);
}
// syms_enum_index_from_elf_version - skipped identity mapping
SYMS_API SYMS_U64
syms_enum_index_from_elf_machine_kind(SYMS_U64 v){
SYMS_U64 result = 0;
switch ((SYMS_U16)v){
default: break;
case SYMS_ElfMachineKind_NONE: result = 0; break;
case SYMS_ElfMachineKind_M32: result = 1; break;
case SYMS_ElfMachineKind_SPARC: result = 2; break;
case SYMS_ElfMachineKind_386: result = 3; break;
case SYMS_ElfMachineKind_68K: result = 4; break;
case SYMS_ElfMachineKind_88K: result = 5; break;
case SYMS_ElfMachineKind_IAMCU: result = 6; break;
case SYMS_ElfMachineKind_860: result = 7; break;
case SYMS_ElfMachineKind_MIPS: result = 8; break;
case SYMS_ElfMachineKind_S370: result = 9; break;
case SYMS_ElfMachineKind_MIPS_RS3_LE: result = 10; break;
case SYMS_ElfMachineKind_PARISC: result = 11; break;
case SYMS_ElfMachineKind_VPP500: result = 12; break;
case SYMS_ElfMachineKind_SPARC32PLUS: result = 13; break;
case SYMS_ElfMachineKind_INTEL960: result = 14; break;
case SYMS_ElfMachineKind_PPC: result = 15; break;
case SYMS_ElfMachineKind_PPC64: result = 16; break;
case SYMS_ElfMachineKind_S390: result = 17; break;
case SYMS_ElfMachineKind_SPU: result = 18; break;
case SYMS_ElfMachineKind_V800: result = 19; break;
case SYMS_ElfMachineKind_FR20: result = 20; break;
case SYMS_ElfMachineKind_RH32: result = 21; break;
case SYMS_ElfMachineKind_MCORE: result = 22; break;
case SYMS_ElfMachineKind_ARM: result = 23; break;
case SYMS_ElfMachineKind_SH: result = 24; break;
case SYMS_ElfMachineKind_ALPHA: result = 25; break;
case SYMS_ElfMachineKind_SPARCV9: result = 26; break;
case SYMS_ElfMachineKind_TRICORE: result = 27; break;
case SYMS_ElfMachineKind_ARC: result = 28; break;
case SYMS_ElfMachineKind_H8_300: result = 29; break;
case SYMS_ElfMachineKind_H8_300H: result = 30; break;
case SYMS_ElfMachineKind_H8S: result = 31; break;
case SYMS_ElfMachineKind_H8_500: result = 32; break;
case SYMS_ElfMachineKind_IA_64: result = 33; break;
case SYMS_ElfMachineKind_MIPS_X: result = 34; break;
case SYMS_ElfMachineKind_COLDFILE: result = 35; break;
case SYMS_ElfMachineKind_68HC12: result = 36; break;
case SYMS_ElfMachineKind_MMA: result = 37; break;
case SYMS_ElfMachineKind_PCP: result = 38; break;
case SYMS_ElfMachineKind_NCPU: result = 39; break;
case SYMS_ElfMachineKind_NDR1: result = 40; break;
case SYMS_ElfMachineKind_STARCORE: result = 41; break;
case SYMS_ElfMachineKind_ME16: result = 42; break;
case SYMS_ElfMachineKind_ST100: result = 43; break;
case SYMS_ElfMachineKind_TINYJ: result = 44; break;
case SYMS_ElfMachineKind_X86_64: result = 45; break;
case SYMS_ElfMachineKind_AARCH64: result = 46; break;
case SYMS_ElfMachineKind_TI_C6000: result = 47; break;
case SYMS_ElfMachineKind_L1OM: result = 48; break;
case SYMS_ElfMachineKind_K1OM: result = 49; break;
case SYMS_ElfMachineKind_RISCV: result = 50; break;
case SYMS_ElfMachineKind_S390_OLD: result = 51; break;
}
return(result);
}
// syms_enum_index_from_elf_type - skipped identity mapping
// syms_enum_index_from_elf_data - skipped identity mapping
SYMS_API SYMS_U64
syms_enum_index_from_elf_p_kind(SYMS_U64 v){
SYMS_U64 result = 0;
switch ((SYMS_U32)v){
default: break;
case SYMS_ElfPKind_Null: result = 0; break;
case SYMS_ElfPKind_Load: result = 1; break;
case SYMS_ElfPKind_Dynamic: result = 2; break;
case SYMS_ElfPKind_Interp: result = 3; break;
case SYMS_ElfPKind_Note: result = 4; break;
case SYMS_ElfPKind_SHLib: result = 5; break;
case SYMS_ElfPKind_PHDR: result = 6; break;
case SYMS_ElfPKind_TLS: result = 7; break;
case SYMS_ElfPKind_LOOS: result = 8; break;
case SYMS_ElfPKind_HIOS: result = 9; break;
case SYMS_ElfPKind_LowProc: result = 10; break;
case SYMS_ElfPKind_HighProc: result = 11; break;
case SYMS_ElfPKind_LowSunW: result = 12; break;
case SYMS_ElfPKind_SunWBSS: result = 13; break;
case SYMS_ElfPKind_GnuEHFrame: result = 14; break;
case SYMS_ElfPKind_GnuStack: result = 15; break;
case SYMS_ElfPKind_GnuRelro: result = 16; break;
case SYMS_ElfPKind_GnuProperty: result = 17; break;
}
return(result);
}
SYMS_API SYMS_U64
syms_enum_index_from_elfsectioncode(SYMS_U64 v){
SYMS_U64 result = 0;
switch ((SYMS_U32)v){
default: break;
case SYMS_ElfSectionCode_NULL: result = 0; break;
case SYMS_ElfSectionCode_PROGBITS: result = 1; break;
case SYMS_ElfSectionCode_SYMTAB: result = 2; break;
case SYMS_ElfSectionCode_STRTAB: result = 3; break;
case SYMS_ElfSectionCode_RELA: result = 4; break;
case SYMS_ElfSectionCode_HASH: result = 5; break;
case SYMS_ElfSectionCode_DYNAMIC: result = 6; break;
case SYMS_ElfSectionCode_NOTE: result = 7; break;
case SYMS_ElfSectionCode_NOBITS: result = 8; break;
case SYMS_ElfSectionCode_REL: result = 9; break;
case SYMS_ElfSectionCode_SHLIB: result = 10; break;
case SYMS_ElfSectionCode_DYNSYM: result = 11; break;
case SYMS_ElfSectionCode_INIT_ARRAY: result = 12; break;
case SYMS_ElfSectionCode_FINI_ARRAY: result = 13; break;
case SYMS_ElfSectionCode_PREINIT_ARRAY: result = 14; break;
case SYMS_ElfSectionCode_GROUP: result = 15; break;
case SYMS_ElfSectionCode_SYMTAB_SHNDX: result = 16; break;
case SYMS_ElfSectionCode_GNU_INCREMENTAL_INPUTS: result = 17; break;
case SYMS_ElfSectionCode_GNU_ATTRIBUTES: result = 18; break;
case SYMS_ElfSectionCode_GNU_HASH: result = 19; break;
case SYMS_ElfSectionCode_GNU_LIBLIST: result = 20; break;
case SYMS_ElfSectionCode_SUNW_verdef: result = 21; break;
case SYMS_ElfSectionCode_SUNW_verneed: result = 22; break;
case SYMS_ElfSectionCode_SUNW_versym: result = 23; break;
case SYMS_ElfSectionCode_PROC: result = 27; break;
case SYMS_ElfSectionCode_USER: result = 28; break;
}
return(result);
}
SYMS_API SYMS_U64
syms_enum_index_from_elf_section_index(SYMS_U64 v){
SYMS_U64 result = 0;
switch ((SYMS_U32)v){
default: break;
case SYMS_ElfSectionIndex_UNDEF: result = 0; break;
case SYMS_ElfSectionIndex_ABS: result = 1; break;
case SYMS_ElfSectionIndex_COMMON: result = 2; break;
case SYMS_ElfSectionIndex_LO_RESERVE: result = 3; break;
case SYMS_ElfSectionIndex_HI_RESERVE: result = 4; break;
case SYMS_ElfSectionIndex_HI_PROC: result = 6; break;
case SYMS_ElfSectionIndex_LO_OS: result = 7; break;
case SYMS_ElfSectionIndex_HI_OS: result = 8; break;
case SYMS_ElfSectionIndex_X86_64_LCOMMON: result = 10; break;
case SYMS_ElfSectionIndex_MIPS_SCOMMON: result = 11; break;
case SYMS_ElfSectionIndex_MIPS_SUNDEFINED: result = 13; break;
}
return(result);
}
SYMS_API SYMS_U64
syms_enum_index_from_elfauxtype(SYMS_U64 v){
SYMS_U64 result = 0;
switch ((SYMS_U32)v){
default: break;
case SYMS_ElfAuxType_NULL: result = 0; break;
case SYMS_ElfAuxType_PHDR: result = 1; break;
case SYMS_ElfAuxType_PHENT: result = 2; break;
case SYMS_ElfAuxType_PHNUM: result = 3; break;
case SYMS_ElfAuxType_PAGESZ: result = 4; break;
case SYMS_ElfAuxType_BASE: result = 5; break;
case SYMS_ElfAuxType_FLAGS: result = 6; break;
case SYMS_ElfAuxType_ENTRY: result = 7; break;
case SYMS_ElfAuxType_UID: result = 8; break;
case SYMS_ElfAuxType_EUID: result = 9; break;
case SYMS_ElfAuxType_GID: result = 10; break;
case SYMS_ElfAuxType_EGID: result = 11; break;
case SYMS_ElfAuxType_PLATFORM: result = 12; break;
case SYMS_ElfAuxType_HWCAP: result = 13; break;
case SYMS_ElfAuxType_CLKTCK: result = 14; break;
case SYMS_ElfAuxType_DCACHEBSIZE: result = 15; break;
case SYMS_ElfAuxType_ICACHEBSIZE: result = 16; break;
case SYMS_ElfAuxType_UCACHEBSIZE: result = 17; break;
case SYMS_ElfAuxType_IGNOREPPC: result = 18; break;
case SYMS_ElfAuxType_SECURE: result = 19; break;
case SYMS_ElfAuxType_BASE_PLATFORM: result = 20; break;
case SYMS_ElfAuxType_RANDOM: result = 21; break;
case SYMS_ElfAuxType_HWCAP2: result = 22; break;
case SYMS_ElfAuxType_EXECFN: result = 23; break;
case SYMS_ElfAuxType_SYSINFO: result = 24; break;
case SYMS_ElfAuxType_SYSINFO_EHDR: result = 25; break;
case SYMS_ElfAuxType_L1I_CACHESIZE: result = 26; break;
case SYMS_ElfAuxType_L1I_CACHEGEOMETRY: result = 27; break;
case SYMS_ElfAuxType_L1D_CACHESIZE: result = 28; break;
case SYMS_ElfAuxType_L1D_CACHEGEOMETRY: result = 29; break;
case SYMS_ElfAuxType_L2_CACHESIZE: result = 30; break;
case SYMS_ElfAuxType_L2_CACHEGEOMETRY: result = 31; break;
case SYMS_ElfAuxType_L3_CACHESIZE: result = 32; break;
case SYMS_ElfAuxType_L3_CACHEGEOMETRY: result = 33; break;
}
return(result);
}
SYMS_API SYMS_U64
syms_enum_index_from_elfdyntag(SYMS_U64 v){
SYMS_U64 result = 0;
switch ((SYMS_U32)v){
default: break;
case SYMS_ElfDynTag_NULL: result = 0; break;
case SYMS_ElfDynTag_NEEDED: result = 1; break;
case SYMS_ElfDynTag_PLTRELSZ: result = 2; break;
case SYMS_ElfDynTag_PLTGOT: result = 3; break;
case SYMS_ElfDynTag_HASH: result = 4; break;
case SYMS_ElfDynTag_STRTAB: result = 5; break;
case SYMS_ElfDynTag_SYMTAB: result = 6; break;
case SYMS_ElfDynTag_RELA: result = 7; break;
case SYMS_ElfDynTag_RELASZ: result = 8; break;
case SYMS_ElfDynTag_RELAENT: result = 9; break;
case SYMS_ElfDynTag_STRSZ: result = 10; break;
case SYMS_ElfDynTag_SYMENT: result = 11; break;
case SYMS_ElfDynTag_INIT: result = 12; break;
case SYMS_ElfDynTag_FINI: result = 13; break;
case SYMS_ElfDynTag_SONAME: result = 14; break;
case SYMS_ElfDynTag_RPATH: result = 15; break;
case SYMS_ElfDynTag_SYMBOLIC: result = 16; break;
case SYMS_ElfDynTag_REL: result = 17; break;
case SYMS_ElfDynTag_RELSZ: result = 18; break;
case SYMS_ElfDynTag_RELENT: result = 19; break;
case SYMS_ElfDynTag_PLTREL: result = 20; break;
case SYMS_ElfDynTag_DEBUG: result = 21; break;
case SYMS_ElfDynTag_TEXTREL: result = 22; break;
case SYMS_ElfDynTag_JMPREL: result = 23; break;
case SYMS_ElfDynTag_BIND_NOW: result = 24; break;
case SYMS_ElfDynTag_INIT_ARRAY: result = 25; break;
case SYMS_ElfDynTag_FINI_ARRAY: result = 26; break;
case SYMS_ElfDynTag_INIT_ARRAYSZ: result = 27; break;
case SYMS_ElfDynTag_FINI_ARRAYSZ: result = 28; break;
case SYMS_ElfDynTag_RUNPATH: result = 29; break;
case SYMS_ElfDynTag_FLAGS: result = 30; break;
case SYMS_ElfDynTag_PREINIT_ARRAY: result = 31; break;
case SYMS_ElfDynTag_PREINIT_ARRAYSZ: result = 32; break;
case SYMS_ElfDynTag_SYMTAB_SHNDX: result = 33; break;
case SYMS_ElfDynTag_LOOS: result = 34; break;
case SYMS_ElfDynTag_HIOS: result = 35; break;
case SYMS_ElfDynTag_VALRNGLO: result = 36; break;
case SYMS_ElfDynTag_GNU_PRELINKED: result = 37; break;
case SYMS_ElfDynTag_GNU_CONFLICTSZ: result = 38; break;
case SYMS_ElfDynTag_GNU_LIBLISTSZ: result = 39; break;
case SYMS_ElfDynTag_CHECKSUM: result = 40; break;
case SYMS_ElfDynTag_PLTPADSZ: result = 41; break;
case SYMS_ElfDynTag_MOVEENT: result = 42; break;
case SYMS_ElfDynTag_MOVESZ: result = 43; break;
case SYMS_ElfDynTag_FEATURE: result = 44; break;
case SYMS_ElfDynTag_POSFLAG_1: result = 45; break;
case SYMS_ElfDynTag_SYMINSZ: result = 46; break;
case SYMS_ElfDynTag_SYMINENT: result = 47; break;
case SYMS_ElfDynTag_ADDRRNGLO: result = 49; break;
case SYMS_ElfDynTag_GNU_HASH: result = 50; break;
case SYMS_ElfDynTag_TLSDESC_PLT: result = 51; break;
case SYMS_ElfDynTag_TLSDESC_GOT: result = 52; break;
case SYMS_ElfDynTag_GNU_CONFLICT: result = 53; break;
case SYMS_ElfDynTag_GNU_LIBLIST: result = 54; break;
case SYMS_ElfDynTag_CONFIG: result = 55; break;
case SYMS_ElfDynTag_DEPAUDIT: result = 56; break;
case SYMS_ElfDynTag_AUDIT: result = 57; break;
case SYMS_ElfDynTag_PLTPAD: result = 58; break;
case SYMS_ElfDynTag_MOVETAB: result = 59; break;
case SYMS_ElfDynTag_SYMINFO: result = 60; break;
case SYMS_ElfDynTag_RELACOUNT: result = 62; break;
case SYMS_ElfDynTag_RELCOUNT: result = 63; break;
case SYMS_ElfDynTag_FLAGS_1: result = 64; break;
case SYMS_ElfDynTag_VERDEF: result = 65; break;
case SYMS_ElfDynTag_VERDEFNUM: result = 66; break;
case SYMS_ElfDynTag_VERNEED: result = 67; break;
case SYMS_ElfDynTag_VERNEEDNUM: result = 68; break;
case SYMS_ElfDynTag_VERSYM: result = 69; break;
case SYMS_ElfDynTag_LOPROC: result = 70; break;
case SYMS_ElfDynTag_AUXILIARY: result = 71; break;
case SYMS_ElfDynTag_USED: result = 72; break;
case SYMS_ElfDynTag_FILTER: result = 73; break;
}
return(result);
}
SYMS_API SYMS_U64
syms_enum_index_from_elf_sym_bind(SYMS_U64 v){
SYMS_U64 result = 0;
switch ((SYMS_U8)v){
default: break;
case SYMS_ElfSymBind_LOCAL: result = 0; break;
case SYMS_ElfSymBind_GLOBAL: result = 1; break;
case SYMS_ElfSymBind_WEAK: result = 2; break;
case SYMS_ElfSymBind_LOPROC: result = 3; break;
case SYMS_ElfSymBind_HIPROC: result = 4; break;
}
return(result);
}
SYMS_API SYMS_U64
syms_enum_index_from_elf_sym_type(SYMS_U64 v){
SYMS_U64 result = 0;
switch ((SYMS_U8)v){
default: break;
case SYMS_ElfSymType_NOTYPE: result = 0; break;
case SYMS_ElfSymType_OBJECT: result = 1; break;
case SYMS_ElfSymType_FUNC: result = 2; break;
case SYMS_ElfSymType_SECTION: result = 3; break;
case SYMS_ElfSymType_FILE: result = 4; break;
case SYMS_ElfSymType_COMMON: result = 5; break;
case SYMS_ElfSymType_TLS: result = 6; break;
case SYMS_ElfSymType_LOPROC: result = 7; break;
case SYMS_ElfSymType_HIPROC: result = 8; break;
}
return(result);
}
// syms_enum_index_from_elf_sym_visibility - skipped identity mapping
SYMS_API SYMS_U64
syms_enum_index_from_elf_reloc_i386(SYMS_U64 v){
SYMS_U64 result = 0;
switch ((SYMS_U32)v){
default: break;
case SYMS_ElfRelocI386_NONE: result = 0; break;
case SYMS_ElfRelocI386_32: result = 1; break;
case SYMS_ElfRelocI386_PC32: result = 2; break;
case SYMS_ElfRelocI386_GOT32: result = 3; break;
case SYMS_ElfRelocI386_PLT32: result = 4; break;
case SYMS_ElfRelocI386_COPY: result = 5; break;
case SYMS_ElfRelocI386_GLOB_DAT: result = 6; break;
case SYMS_ElfRelocI386_JUMP_SLOT: result = 7; break;
case SYMS_ElfRelocI386_RELATIVE: result = 8; break;
case SYMS_ElfRelocI386_GOTOFF: result = 9; break;
case SYMS_ElfRelocI386_GOTPC: result = 10; break;
case SYMS_ElfRelocI386_32PLT: result = 11; break;
case SYMS_ElfRelocI386_TLS_TPOFF: result = 12; break;
case SYMS_ElfRelocI386_TLS_IE: result = 13; break;
case SYMS_ElfRelocI386_TLS_GOTIE: result = 14; break;
case SYMS_ElfRelocI386_TLS_LE: result = 15; break;
case SYMS_ElfRelocI386_TLS_GD: result = 16; break;
case SYMS_ElfRelocI386_TLS_LDM: result = 17; break;
case SYMS_ElfRelocI386_16: result = 18; break;
case SYMS_ElfRelocI386_PC16: result = 19; break;
case SYMS_ElfRelocI386_8: result = 20; break;
case SYMS_ElfRelocI386_PC8: result = 21; break;
case SYMS_ElfRelocI386_TLS_GD_32: result = 22; break;
case SYMS_ElfRelocI386_TLS_GD_PUSH: result = 23; break;
case SYMS_ElfRelocI386_TLS_GD_CALL: result = 24; break;
case SYMS_ElfRelocI386_TLS_GD_POP: result = 25; break;
case SYMS_ElfRelocI386_TLS_LDM_32: result = 26; break;
case SYMS_ElfRelocI386_TLS_LDM_PUSH: result = 27; break;
case SYMS_ElfRelocI386_TLS_LDM_CALL: result = 28; break;
case SYMS_ElfRelocI386_TLS_LDM_POP: result = 29; break;
case SYMS_ElfRelocI386_TLS_LDO_32: result = 30; break;
case SYMS_ElfRelocI386_TLS_IE_32: result = 31; break;
case SYMS_ElfRelocI386_TLS_LE_32: result = 32; break;
case SYMS_ElfRelocI386_TLS_DTPMOD32: result = 33; break;
case SYMS_ElfRelocI386_TLS_DTPOFF32: result = 34; break;
case SYMS_ElfRelocI386_TLS_TPOFF32: result = 35; break;
case SYMS_ElfRelocI386_TLS_GOTDESC: result = 36; break;
case SYMS_ElfRelocI386_TLS_DESC_CALL: result = 37; break;
case SYMS_ElfRelocI386_TLS_DESC: result = 38; break;
case SYMS_ElfRelocI386_IRELATIVE: result = 39; break;
case SYMS_ElfRelocI386_GOTX32X: result = 40; break;
case SYMS_ElfRelocI386_USED_BY_INTEL_200: result = 41; break;
case SYMS_ElfRelocI386_GNU_VTINHERIT: result = 42; break;
case SYMS_ElfRelocI386_GNU_VTENTRY: result = 43; break;
}
return(result);
}
SYMS_API SYMS_U64
syms_enum_index_from_elf_reloc_x8664(SYMS_U64 v){
SYMS_U64 result = 0;
switch ((SYMS_U32)v){
default: break;
case SYMS_ElfRelocX8664_NONE: result = 0; break;
case SYMS_ElfRelocX8664_64: result = 1; break;
case SYMS_ElfRelocX8664_PC32: result = 2; break;
case SYMS_ElfRelocX8664_GOT32: result = 3; break;
case SYMS_ElfRelocX8664_PLT32: result = 4; break;
case SYMS_ElfRelocX8664_COPY: result = 5; break;
case SYMS_ElfRelocX8664_GLOB_DAT: result = 6; break;
case SYMS_ElfRelocX8664_JUMP_SLOT: result = 7; break;
case SYMS_ElfRelocX8664_RELATIVE: result = 8; break;
case SYMS_ElfRelocX8664_GOTPCREL: result = 9; break;
case SYMS_ElfRelocX8664_32: result = 10; break;
case SYMS_ElfRelocX8664_32S: result = 11; break;
case SYMS_ElfRelocX8664_16: result = 12; break;
case SYMS_ElfRelocX8664_PC16: result = 13; break;
case SYMS_ElfRelocX8664_8: result = 14; break;
case SYMS_ElfRelocX8664_PC8: result = 15; break;
case SYMS_ElfRelocX8664_DTPMOD64: result = 16; break;
case SYMS_ElfRelocX8664_DTPOFF64: result = 17; break;
case SYMS_ElfRelocX8664_TPOFF64: result = 18; break;
case SYMS_ElfRelocX8664_TLSGD: result = 19; break;
case SYMS_ElfRelocX8664_TLSLD: result = 20; break;
case SYMS_ElfRelocX8664_DTPOFF32: result = 21; break;
case SYMS_ElfRelocX8664_GOTTPOFF: result = 22; break;
case SYMS_ElfRelocX8664_TPOFF32: result = 23; break;
case SYMS_ElfRelocX8664_PC64: result = 24; break;
case SYMS_ElfRelocX8664_GOTOFF64: result = 25; break;
case SYMS_ElfRelocX8664_GOTPC32: result = 26; break;
case SYMS_ElfRelocX8664_GOT64: result = 27; break;
case SYMS_ElfRelocX8664_GOTPCREL64: result = 28; break;
case SYMS_ElfRelocX8664_GOTPC64: result = 29; break;
case SYMS_ElfRelocX8664_GOTPLT64: result = 30; break;
case SYMS_ElfRelocX8664_PLTOFF64: result = 31; break;
case SYMS_ElfRelocX8664_SIZE32: result = 32; break;
case SYMS_ElfRelocX8664_SIZE64: result = 33; break;
case SYMS_ElfRelocX8664_GOTPC32_TLSDESC: result = 34; break;
case SYMS_ElfRelocX8664_TLSDESC_CALL: result = 35; break;
case SYMS_ElfRelocX8664_TLSDESC: result = 36; break;
case SYMS_ElfRelocX8664_IRELATIVE: result = 37; break;
case SYMS_ElfRelocX8664_RELATIVE64: result = 38; break;
case SYMS_ElfRelocX8664_PC32_BND: result = 39; break;
case SYMS_ElfRelocX8664_PLT32_BND: result = 40; break;
case SYMS_ElfRelocX8664_GOTPCRELX: result = 41; break;
case SYMS_ElfRelocX8664_REX_GOTPCRELX: result = 42; break;
case SYMS_ElfRelocX8664_GNU_VTINHERIT: result = 43; break;
case SYMS_ElfRelocX8664_GNU_VTENTRY: result = 44; break;
}
return(result);
}
SYMS_API SYMS_U64
syms_enum_index_from_elf_note_type(SYMS_U64 v){
SYMS_U64 result = 0;
switch ((SYMS_U32)v){
default: break;
case SYMS_ElfNoteType_GNU_ABI: result = 0; break;
case SYMS_ElfNoteType_GNU_HWCAP: result = 1; break;
case SYMS_ElfNoteType_GNU_BUILD_ID: result = 2; break;
case SYMS_ElfNoteType_GNU_GOLD_VERSION: result = 3; break;
case SYMS_ElfNoteType_GNU_PROPERTY_TYPE_0: result = 4; break;
}
return(result);
}
// syms_enum_index_from_elf_gnu_a_b_i_tag - skipped identity mapping
SYMS_API SYMS_U64
syms_enum_index_from_elf_gnu_property(SYMS_U64 v){
SYMS_U64 result = 0;
switch ((SYMS_S32)v){
default: break;
case SYMS_ElfGnuProperty_LOPROC: result = 0; break;
case SYMS_ElfGnuProperty_HIPROC: result = 1; break;
case SYMS_ElfGnuProperty_LOUSER: result = 2; break;
case SYMS_ElfGnuProperty_HIUSER: result = 3; break;
case SYMS_ElfGnuProperty_STACK_SIZE: result = 4; break;
case SYMS_ElfGnuProperty_NO_COPY_ON_PROTECTED: result = 5; break;
}
return(result);
}
SYMS_API SYMS_U64
syms_enum_index_from_elf_gnu_property_x86(SYMS_U64 v){
SYMS_U64 result = 0;
switch ((SYMS_S32)v){
default: break;
case SYMS_ElfGnuPropertyX86_FEATURE_1_AND: result = 0; break;
case SYMS_ElfGnuPropertyX86_FEATURE_2_USED: result = 1; break;
case SYMS_ElfGnuPropertyX86_ISA_1_NEEDED: result = 2; break;
case SYMS_ElfGnuPropertyX86_ISA_2_NEEDED: result = 3; break;
case SYMS_ElfGnuPropertyX86_ISA_1_USED: result = 4; break;
case SYMS_ElfGnuPropertyX86_COMPAT_ISA_1_USED: result = 5; break;
case SYMS_ElfGnuPropertyX86_COMPAT_ISA_1_NEEDED: result = 6; break;
case SYMS_ElfGnuPropertyX86_UINT32_AND_HI: result = 8; break;
case SYMS_ElfGnuPropertyX86_UINT32_OR_LO: result = 9; break;
case SYMS_ElfGnuPropertyX86_UINT32_OR_HI: result = 10; break;
case SYMS_ElfGnuPropertyX86_UINT32_OR_AND_LO: result = 11; break;
case SYMS_ElfGnuPropertyX86_UINT32_OR_AND_HI: result = 12; break;
}
return(result);
}

//~ generated from code at syms/metaprogram/syms_metaprogram_serial.c:1322
SYMS_SerialValue _syms_serial_members_for_SYMS_ElfClass[] = {
{ { (SYMS_U8*)"None", 4 }, (SYMS_U64)SYMS_ElfClass_None },
{ { (SYMS_U8*)"ELF 32-bit", 10 }, (SYMS_U64)SYMS_ElfClass_32 },
{ { (SYMS_U8*)"ELF 64-bit", 10 }, (SYMS_U64)SYMS_ElfClass_64 },
};
SYMS_SerialValue _syms_serial_members_for_SYMS_ElfOsAbi[] = {
{ { (SYMS_U8*)"NONE", 4 }, (SYMS_U64)SYMS_ElfOsAbi_NONE },
{ { (SYMS_U8*)"UNIX System-V", 13 }, (SYMS_U64)SYMS_ElfOsAbi_SYSV },
{ { (SYMS_U8*)"HP-UX", 5 }, (SYMS_U64)SYMS_ElfOsAbi_HPUX },
{ { (SYMS_U8*)"NetBSD", 6 }, (SYMS_U64)SYMS_ElfOsAbi_NETBSD },
{ { (SYMS_U8*)"GNU", 3 }, (SYMS_U64)SYMS_ElfOsAbi_GNU },
{ { (SYMS_U8*)"Linux", 5 }, (SYMS_U64)SYMS_ElfOsAbi_LINUX },
{ { (SYMS_U8*)"Solaris", 7 }, (SYMS_U64)SYMS_ElfOsAbi_SOLARIS },
{ { (SYMS_U8*)"IRIX", 4 }, (SYMS_U64)SYMS_ElfOsAbi_IRIX },
{ { (SYMS_U8*)"FreeBSD", 7 }, (SYMS_U64)SYMS_ElfOsAbi_FREEBSD },
{ { (SYMS_U8*)"TRU64 UNIX", 10 }, (SYMS_U64)SYMS_ElfOsAbi_TRU64 },
{ { (SYMS_U8*)"ARM", 3 }, (SYMS_U64)SYMS_ElfOsAbi_ARM },
{ { (SYMS_U8*)"Standalone", 10 }, (SYMS_U64)SYMS_ElfOsAbi_STANDALONE },
};
SYMS_SerialValue _syms_serial_members_for_SYMS_ElfVersion[] = {
{ { (SYMS_U8*)"0 (Invalid)", 11 }, (SYMS_U64)SYMS_ElfVersion_NONE },
{ { (SYMS_U8*)"1 (Current)", 11 }, (SYMS_U64)SYMS_ElfVersion_CURRENT },
};
SYMS_SerialValue _syms_serial_members_for_SYMS_ElfMachineKind[] = {
{ { (SYMS_U8*)"None", 4 }, (SYMS_U64)SYMS_ElfMachineKind_NONE },
{ { (SYMS_U8*)"AT&T WE 32100", 13 }, (SYMS_U64)SYMS_ElfMachineKind_M32 },
{ { (SYMS_U8*)"Sun Microsystems SPARC", 22 }, (SYMS_U64)SYMS_ElfMachineKind_SPARC },
{ { (SYMS_U8*)"Intel 80386", 11 }, (SYMS_U64)SYMS_ElfMachineKind_386 },
{ { (SYMS_U8*)"Motorola 68000", 14 }, (SYMS_U64)SYMS_ElfMachineKind_68K },
{ { (SYMS_U8*)"Motorola 88000", 14 }, (SYMS_U64)SYMS_ElfMachineKind_88K },
{ { (SYMS_U8*)"Intel 80860", 11 }, (SYMS_U64)SYMS_ElfMachineKind_IAMCU },
{ { (SYMS_U8*)"Intel MCU", 9 }, (SYMS_U64)SYMS_ElfMachineKind_860 },
{ { (SYMS_U8*)"MIPS RS3000", 11 }, (SYMS_U64)SYMS_ElfMachineKind_MIPS },
{ { (SYMS_U8*)"IBM System/370", 14 }, (SYMS_U64)SYMS_ElfMachineKind_S370 },
{ { (SYMS_U8*)"MIPS RS3000 LE", 14 }, (SYMS_U64)SYMS_ElfMachineKind_MIPS_RS3_LE },
{ { (SYMS_U8*)"HPPA", 4 }, (SYMS_U64)SYMS_ElfMachineKind_PARISC },
{ { (SYMS_U8*)"Fujistsu VPP500", 15 }, (SYMS_U64)SYMS_ElfMachineKind_VPP500 },
{ { (SYMS_U8*)"SPARC-32 V8+", 12 }, (SYMS_U64)SYMS_ElfMachineKind_SPARC32PLUS },
{ { (SYMS_U8*)"Intel 80980", 11 }, (SYMS_U64)SYMS_ElfMachineKind_INTEL960 },
{ { (SYMS_U8*)"PowerPC", 7 }, (SYMS_U64)SYMS_ElfMachineKind_PPC },
{ { (SYMS_U8*)"PowerPC 64bit", 13 }, (SYMS_U64)SYMS_ElfMachineKind_PPC64 },
{ { (SYMS_U8*)"IBM S/390", 9 }, (SYMS_U64)SYMS_ElfMachineKind_S390 },
{ { (SYMS_U8*)"Sony/Toshiba/IBM SPU", 20 }, (SYMS_U64)SYMS_ElfMachineKind_SPU },
{ { (SYMS_U8*)"NEC V800 series", 15 }, (SYMS_U64)SYMS_ElfMachineKind_V800 },
{ { (SYMS_U8*)"Fujitsu FR20", 12 }, (SYMS_U64)SYMS_ElfMachineKind_FR20 },
{ { (SYMS_U8*)"TRW RH32", 8 }, (SYMS_U64)SYMS_ElfMachineKind_RH32 },
{ { (SYMS_U8*)"Motorola M*Core", 15 }, (SYMS_U64)SYMS_ElfMachineKind_MCORE },
{ { (SYMS_U8*)"ARM", 3 }, (SYMS_U64)SYMS_ElfMachineKind_ARM },
{ { (SYMS_U8*)"Renesas / SuperH SH", 19 }, (SYMS_U64)SYMS_ElfMachineKind_SH },
{ { (SYMS_U8*)"Alpha", 5 }, (SYMS_U64)SYMS_ElfMachineKind_ALPHA },
{ { (SYMS_U8*)"SPARC V9 64-bit", 15 }, (SYMS_U64)SYMS_ElfMachineKind_SPARCV9 },
{ { (SYMS_U8*)"Siemens Tricore", 15 }, (SYMS_U64)SYMS_ElfMachineKind_TRICORE },
{ { (SYMS_U8*)"ARC Cores", 9 }, (SYMS_U64)SYMS_ElfMachineKind_ARC },
{ { (SYMS_U8*)"H8/300", 6 }, (SYMS_U64)SYMS_ElfMachineKind_H8_300 },
{ { (SYMS_U8*)"H8/300H", 7 }, (SYMS_U64)SYMS_ElfMachineKind_H8_300H },
{ { (SYMS_U8*)"H8S", 3 }, (SYMS_U64)SYMS_ElfMachineKind_H8S },
{ { (SYMS_U8*)"H8/500", 6 }, (SYMS_U64)SYMS_ElfMachineKind_H8_500 },
{ { (SYMS_U8*)"Intel IA-64", 11 }, (SYMS_U64)SYMS_ElfMachineKind_IA_64 },
{ { (SYMS_U8*)"Stanford MIPS-X", 15 }, (SYMS_U64)SYMS_ElfMachineKind_MIPS_X },
{ { (SYMS_U8*)"Motorola Coldfire", 17 }, (SYMS_U64)SYMS_ElfMachineKind_COLDFILE },
{ { (SYMS_U8*)"Motorola M68HC12", 16 }, (SYMS_U64)SYMS_ElfMachineKind_68HC12 },
{ { (SYMS_U8*)"Fujistu Multimidea Accelerator", 30 }, (SYMS_U64)SYMS_ElfMachineKind_MMA },
{ { (SYMS_U8*)"Siemens PCP", 11 }, (SYMS_U64)SYMS_ElfMachineKind_PCP },
{ { (SYMS_U8*)"Sony nCPU Embedded RISC Processor", 33 }, (SYMS_U64)SYMS_ElfMachineKind_NCPU },
{ { (SYMS_U8*)"Denso NDR1", 10 }, (SYMS_U64)SYMS_ElfMachineKind_NDR1 },
{ { (SYMS_U8*)"Motorola Star*Core", 18 }, (SYMS_U64)SYMS_ElfMachineKind_STARCORE },
{ { (SYMS_U8*)"Toyota ME16", 11 }, (SYMS_U64)SYMS_ElfMachineKind_ME16 },
{ { (SYMS_U8*)"STMicroelectronics ST100", 24 }, (SYMS_U64)SYMS_ElfMachineKind_ST100 },
{ { (SYMS_U8*)"TinyJ", 5 }, (SYMS_U64)SYMS_ElfMachineKind_TINYJ },
{ { (SYMS_U8*)"X86-64", 6 }, (SYMS_U64)SYMS_ElfMachineKind_X86_64 },
{ { (SYMS_U8*)"ARM 64-bit", 10 }, (SYMS_U64)SYMS_ElfMachineKind_AARCH64 },
{ { (SYMS_U8*)"Texas Instruments TMS320C6000 DSP Family", 40 }, (SYMS_U64)SYMS_ElfMachineKind_TI_C6000 },
{ { (SYMS_U8*)"Intel L1OM", 10 }, (SYMS_U64)SYMS_ElfMachineKind_L1OM },
{ { (SYMS_U8*)"Intel K10M", 10 }, (SYMS_U64)SYMS_ElfMachineKind_K1OM },
{ { (SYMS_U8*)"RISC-V", 6 }, (SYMS_U64)SYMS_ElfMachineKind_RISCV },
{ { (SYMS_U8*)"S390-OLD", 8 }, (SYMS_U64)SYMS_ElfMachineKind_S390_OLD },
};
SYMS_SerialValue _syms_serial_members_for_SYMS_ElfType[] = {
{ { (SYMS_U8*)"None", 4 }, (SYMS_U64)SYMS_ElfType_NONE },
{ { (SYMS_U8*)"REL (Relocatable File)", 22 }, (SYMS_U64)SYMS_ElfType_REL },
{ { (SYMS_U8*)"EXEC (Executable File)", 22 }, (SYMS_U64)SYMS_ElfType_EXEC },
{ { (SYMS_U8*)"DYN (Shared Object)", 19 }, (SYMS_U64)SYMS_ElfType_DYN },
{ { (SYMS_U8*)"CORE (Core File)", 16 }, (SYMS_U64)SYMS_ElfType_CORE },
};
SYMS_SerialValue _syms_serial_members_for_SYMS_ElfData[] = {
{ { (SYMS_U8*)"invalid data encoding", 21 }, (SYMS_U64)SYMS_ElfData_None },
{ { (SYMS_U8*)"2's complement, little endian", 29 }, (SYMS_U64)SYMS_ElfData_2LSB },
{ { (SYMS_U8*)"2's complement, big endian", 26 }, (SYMS_U64)SYMS_ElfData_2MSB },
};
SYMS_SerialValue _syms_serial_members_for_SYMS_ElfPKind[] = {
{ { (SYMS_U8*)"NONE", 4 }, (SYMS_U64)SYMS_ElfPKind_Null },
{ { (SYMS_U8*)"LOAD", 4 }, (SYMS_U64)SYMS_ElfPKind_Load },
{ { (SYMS_U8*)"DYNAMIC", 7 }, (SYMS_U64)SYMS_ElfPKind_Dynamic },
{ { (SYMS_U8*)"INTERP", 6 }, (SYMS_U64)SYMS_ElfPKind_Interp },
{ { (SYMS_U8*)"NOTE", 4 }, (SYMS_U64)SYMS_ElfPKind_Note },
{ { (SYMS_U8*)"SHLIB", 5 }, (SYMS_U64)SYMS_ElfPKind_SHLib },
{ { (SYMS_U8*)"PHDR", 4 }, (SYMS_U64)SYMS_ElfPKind_PHDR },
{ { (SYMS_U8*)"TLS", 3 }, (SYMS_U64)SYMS_ElfPKind_TLS },
{ { (SYMS_U8*)"LOOS", 4 }, (SYMS_U64)SYMS_ElfPKind_LOOS },
{ { (SYMS_U8*)"HIOS", 4 }, (SYMS_U64)SYMS_ElfPKind_HIOS },
{ { (SYMS_U8*)"LowProc", 7 }, (SYMS_U64)SYMS_ElfPKind_LowProc },
{ { (SYMS_U8*)"HighProc", 8 }, (SYMS_U64)SYMS_ElfPKind_HighProc },
{ { (SYMS_U8*)"LowSunW", 7 }, (SYMS_U64)SYMS_ElfPKind_LowSunW },
{ { (SYMS_U8*)"SunWBSS", 7 }, (SYMS_U64)SYMS_ElfPKind_SunWBSS },
{ { (SYMS_U8*)"GNU_EH_FRAME", 12 }, (SYMS_U64)SYMS_ElfPKind_GnuEHFrame },
{ { (SYMS_U8*)"GNU_STACK", 9 }, (SYMS_U64)SYMS_ElfPKind_GnuStack },
{ { (SYMS_U8*)"GNU_RELRO", 9 }, (SYMS_U64)SYMS_ElfPKind_GnuRelro },
{ { (SYMS_U8*)"GNU_PROPERTY", 12 }, (SYMS_U64)SYMS_ElfPKind_GnuProperty },
{ { (SYMS_U8*)"SunEHFrame", 10 }, (SYMS_U64)SYMS_ElfPKind_SunEHFrame },
};
SYMS_SerialFlag _syms_serial_members_for_SYMS_ElfPFlag[] = {
{ { (SYMS_U8*)"Execute", 7 }, &_syms_serial_type_SYMS_U32, 0x1, 0 },
{ { (SYMS_U8*)"Write", 5 }, &_syms_serial_type_SYMS_U32, 0x1, 1 },
{ { (SYMS_U8*)"Read", 4 }, &_syms_serial_type_SYMS_U32, 0x1, 2 },
};
SYMS_SerialValue _syms_serial_members_for_SYMS_ElfSectionCode[] = {
{ { (SYMS_U8*)"NULL", 4 }, (SYMS_U64)SYMS_ElfSectionCode_NULL },
{ { (SYMS_U8*)"PROGBITS", 8 }, (SYMS_U64)SYMS_ElfSectionCode_PROGBITS },
{ { (SYMS_U8*)"SYMTAB", 6 }, (SYMS_U64)SYMS_ElfSectionCode_SYMTAB },
{ { (SYMS_U8*)"STRTAB", 6 }, (SYMS_U64)SYMS_ElfSectionCode_STRTAB },
{ { (SYMS_U8*)"RELA", 4 }, (SYMS_U64)SYMS_ElfSectionCode_RELA },
{ { (SYMS_U8*)"HASH", 4 }, (SYMS_U64)SYMS_ElfSectionCode_HASH },
{ { (SYMS_U8*)"DYNAMIC", 7 }, (SYMS_U64)SYMS_ElfSectionCode_DYNAMIC },
{ { (SYMS_U8*)"NOTE", 4 }, (SYMS_U64)SYMS_ElfSectionCode_NOTE },
{ { (SYMS_U8*)"NOBITS", 6 }, (SYMS_U64)SYMS_ElfSectionCode_NOBITS },
{ { (SYMS_U8*)"REL", 3 }, (SYMS_U64)SYMS_ElfSectionCode_REL },
{ { (SYMS_U8*)"SHLIB", 5 }, (SYMS_U64)SYMS_ElfSectionCode_SHLIB },
{ { (SYMS_U8*)"DYNSYM", 6 }, (SYMS_U64)SYMS_ElfSectionCode_DYNSYM },
{ { (SYMS_U8*)"INIT_ARRAY", 10 }, (SYMS_U64)SYMS_ElfSectionCode_INIT_ARRAY },
{ { (SYMS_U8*)"FINI_ARRAY", 10 }, (SYMS_U64)SYMS_ElfSectionCode_FINI_ARRAY },
{ { (SYMS_U8*)"PREINIT_ARRAY", 13 }, (SYMS_U64)SYMS_ElfSectionCode_PREINIT_ARRAY },
{ { (SYMS_U8*)"GROUP", 5 }, (SYMS_U64)SYMS_ElfSectionCode_GROUP },
{ { (SYMS_U8*)"SYMTAB_SHNDX", 12 }, (SYMS_U64)SYMS_ElfSectionCode_SYMTAB_SHNDX },
{ { (SYMS_U8*)"GNU_INCREMENTAL_INPUTS", 22 }, (SYMS_U64)SYMS_ElfSectionCode_GNU_INCREMENTAL_INPUTS },
{ { (SYMS_U8*)"GNU_ATTRIBUTES", 14 }, (SYMS_U64)SYMS_ElfSectionCode_GNU_ATTRIBUTES },
{ { (SYMS_U8*)"GNU_HASH", 8 }, (SYMS_U64)SYMS_ElfSectionCode_GNU_HASH },
{ { (SYMS_U8*)"GNU_LIBLIST", 11 }, (SYMS_U64)SYMS_ElfSectionCode_GNU_LIBLIST },
{ { (SYMS_U8*)"VERDEF", 6 }, (SYMS_U64)SYMS_ElfSectionCode_SUNW_verdef },
{ { (SYMS_U8*)"VERNEED", 7 }, (SYMS_U64)SYMS_ElfSectionCode_SUNW_verneed },
{ { (SYMS_U8*)"VERSYM", 6 }, (SYMS_U64)SYMS_ElfSectionCode_SUNW_versym },
{ { (SYMS_U8*)"GNU_VERDEF", 10 }, (SYMS_U64)SYMS_ElfSectionCode_GNU_verdef },
{ { (SYMS_U8*)"GNU_VERNEED", 11 }, (SYMS_U64)SYMS_ElfSectionCode_GNU_verneed },
{ { (SYMS_U8*)"GNU_VERSYM", 10 }, (SYMS_U64)SYMS_ElfSectionCode_GNU_versym },
{ { (SYMS_U8*)"PROC", 4 }, (SYMS_U64)SYMS_ElfSectionCode_PROC },
{ { (SYMS_U8*)"USER", 4 }, (SYMS_U64)SYMS_ElfSectionCode_USER },
};
SYMS_SerialValue _syms_serial_members_for_SYMS_ElfSectionIndex[] = {
{ { (SYMS_U8*)"UND", 3 }, (SYMS_U64)SYMS_ElfSectionIndex_UNDEF },
{ { (SYMS_U8*)"ABS", 3 }, (SYMS_U64)SYMS_ElfSectionIndex_ABS },
{ { (SYMS_U8*)"COM", 3 }, (SYMS_U64)SYMS_ElfSectionIndex_COMMON },
{ { (SYMS_U8*)"LO_RESERVE", 10 }, (SYMS_U64)SYMS_ElfSectionIndex_LO_RESERVE },
{ { (SYMS_U8*)"HI_RESERVE", 10 }, (SYMS_U64)SYMS_ElfSectionIndex_HI_RESERVE },
{ { (SYMS_U8*)"LO_PROC", 7 }, (SYMS_U64)SYMS_ElfSectionIndex_LO_PROC },
{ { (SYMS_U8*)"HI_PROC", 7 }, (SYMS_U64)SYMS_ElfSectionIndex_HI_PROC },
{ { (SYMS_U8*)"LO_OS", 5 }, (SYMS_U64)SYMS_ElfSectionIndex_LO_OS },
{ { (SYMS_U8*)"HI_OS", 5 }, (SYMS_U64)SYMS_ElfSectionIndex_HI_OS },
{ { (SYMS_U8*)"ANSI_COM", 8 }, (SYMS_U64)SYMS_ElfSectionIndex_IA_64_ASNI_COMMON },
{ { (SYMS_U8*)"LARGE_COM", 9 }, (SYMS_U64)SYMS_ElfSectionIndex_X86_64_LCOMMON },
{ { (SYMS_U8*)"SCOM", 4 }, (SYMS_U64)SYMS_ElfSectionIndex_MIPS_SCOMMON },
{ { (SYMS_U8*)"SCOM", 4 }, (SYMS_U64)SYMS_ElfSectionIndex_TIC6X_COMMON },
{ { (SYMS_U8*)"SUND", 4 }, (SYMS_U64)SYMS_ElfSectionIndex_MIPS_SUNDEFINED },
};
SYMS_SerialFlag _syms_serial_members_for_SYMS_ElfSectionFlag[] = {
{ { (SYMS_U8*)"Write", 5 }, &_syms_serial_type_SYMS_U32, 0x1, 0 },
{ { (SYMS_U8*)"Alloc", 5 }, &_syms_serial_type_SYMS_U32, 0x1, 1 },
{ { (SYMS_U8*)"Exec", 4 }, &_syms_serial_type_SYMS_U32, 0x1, 2 },
{ { (SYMS_U8*)"Merge", 5 }, &_syms_serial_type_SYMS_U32, 0x1, 4 },
{ { (SYMS_U8*)"Strings", 7 }, &_syms_serial_type_SYMS_U32, 0x1, 5 },
{ { (SYMS_U8*)"Link Info", 9 }, &_syms_serial_type_SYMS_U32, 0x1, 6 },
{ { (SYMS_U8*)"Lnik Order", 10 }, &_syms_serial_type_SYMS_U32, 0x1, 7 },
{ { (SYMS_U8*)"OS nonconforming", 16 }, &_syms_serial_type_SYMS_U32, 0x1, 8 },
{ { (SYMS_U8*)"Group", 5 }, &_syms_serial_type_SYMS_U32, 0x1, 9 },
{ { (SYMS_U8*)"TLS", 3 }, &_syms_serial_type_SYMS_U32, 0x1, 10 },
{ { (SYMS_U8*)"MASKOS", 6 }, &_syms_serial_type_SYMS_U8, 0xff, 16 },
{ { (SYMS_U8*)"AMD64 Large", 11 }, &_syms_serial_type_SYMS_U32, 0x1, 28 },
{ { (SYMS_U8*)"Ordered", 7 }, &_syms_serial_type_SYMS_U32, 0x1, 30 },
{ { (SYMS_U8*)"Exclude", 7 }, &_syms_serial_type_SYMS_U32, 0x1, 31 },
{ { (SYMS_U8*)"MASKPROC", 8 }, &_syms_serial_type_SYMS_U8, 0xf, 28 },
};
SYMS_SerialValue _syms_serial_members_for_SYMS_ElfAuxType[] = {
{ { (SYMS_U8*)"NULL", 4 }, (SYMS_U64)SYMS_ElfAuxType_NULL },
{ { (SYMS_U8*)"PHDR", 4 }, (SYMS_U64)SYMS_ElfAuxType_PHDR },
{ { (SYMS_U8*)"PHENT", 5 }, (SYMS_U64)SYMS_ElfAuxType_PHENT },
{ { (SYMS_U8*)"PHNUM", 5 }, (SYMS_U64)SYMS_ElfAuxType_PHNUM },
{ { (SYMS_U8*)"PAGESZ", 6 }, (SYMS_U64)SYMS_ElfAuxType_PAGESZ },
{ { (SYMS_U8*)"BASE", 4 }, (SYMS_U64)SYMS_ElfAuxType_BASE },
{ { (SYMS_U8*)"FLAGS", 5 }, (SYMS_U64)SYMS_ElfAuxType_FLAGS },
{ { (SYMS_U8*)"ENTRY", 5 }, (SYMS_U64)SYMS_ElfAuxType_ENTRY },
{ { (SYMS_U8*)"UID", 3 }, (SYMS_U64)SYMS_ElfAuxType_UID },
{ { (SYMS_U8*)"EUID", 4 }, (SYMS_U64)SYMS_ElfAuxType_EUID },
{ { (SYMS_U8*)"GID", 3 }, (SYMS_U64)SYMS_ElfAuxType_GID },
{ { (SYMS_U8*)"EGID", 4 }, (SYMS_U64)SYMS_ElfAuxType_EGID },
{ { (SYMS_U8*)"PLATFORM", 8 }, (SYMS_U64)SYMS_ElfAuxType_PLATFORM },
{ { (SYMS_U8*)"HWCAP", 5 }, (SYMS_U64)SYMS_ElfAuxType_HWCAP },
{ { (SYMS_U8*)"CLKTCK", 6 }, (SYMS_U64)SYMS_ElfAuxType_CLKTCK },
{ { (SYMS_U8*)"DCACHEBSIZE", 11 }, (SYMS_U64)SYMS_ElfAuxType_DCACHEBSIZE },
{ { (SYMS_U8*)"ICACHEBSIZE", 11 }, (SYMS_U64)SYMS_ElfAuxType_ICACHEBSIZE },
{ { (SYMS_U8*)"UCACHEBSIZE", 11 }, (SYMS_U64)SYMS_ElfAuxType_UCACHEBSIZE },
{ { (SYMS_U8*)"IGNOREPPC", 9 }, (SYMS_U64)SYMS_ElfAuxType_IGNOREPPC },
{ { (SYMS_U8*)"SECURE", 6 }, (SYMS_U64)SYMS_ElfAuxType_SECURE },
{ { (SYMS_U8*)"BASE_PLATFORM", 13 }, (SYMS_U64)SYMS_ElfAuxType_BASE_PLATFORM },
{ { (SYMS_U8*)"RANDOM", 6 }, (SYMS_U64)SYMS_ElfAuxType_RANDOM },
{ { (SYMS_U8*)"HWCAP2", 6 }, (SYMS_U64)SYMS_ElfAuxType_HWCAP2 },
{ { (SYMS_U8*)"EXECFN", 6 }, (SYMS_U64)SYMS_ElfAuxType_EXECFN },
{ { (SYMS_U8*)"SYSINFO", 7 }, (SYMS_U64)SYMS_ElfAuxType_SYSINFO },
{ { (SYMS_U8*)"SYSINFO_EHDR", 12 }, (SYMS_U64)SYMS_ElfAuxType_SYSINFO_EHDR },
{ { (SYMS_U8*)"L1I_CACHESIZE", 13 }, (SYMS_U64)SYMS_ElfAuxType_L1I_CACHESIZE },
{ { (SYMS_U8*)"L1I_CACHEGEOMETRY", 17 }, (SYMS_U64)SYMS_ElfAuxType_L1I_CACHEGEOMETRY },
{ { (SYMS_U8*)"L1D_CACHESIZE", 13 }, (SYMS_U64)SYMS_ElfAuxType_L1D_CACHESIZE },
{ { (SYMS_U8*)"L1D_CACHEGEOMETRY", 17 }, (SYMS_U64)SYMS_ElfAuxType_L1D_CACHEGEOMETRY },
{ { (SYMS_U8*)"L2_CACHESIZE", 12 }, (SYMS_U64)SYMS_ElfAuxType_L2_CACHESIZE },
{ { (SYMS_U8*)"L2_CACHEGEOMETRY", 16 }, (SYMS_U64)SYMS_ElfAuxType_L2_CACHEGEOMETRY },
{ { (SYMS_U8*)"L3_CACHESIZE", 12 }, (SYMS_U64)SYMS_ElfAuxType_L3_CACHESIZE },
{ { (SYMS_U8*)"L3_CACHEGEOMETRY", 16 }, (SYMS_U64)SYMS_ElfAuxType_L3_CACHEGEOMETRY },
};
SYMS_SerialValue _syms_serial_members_for_SYMS_ElfDynTag[] = {
{ { (SYMS_U8*)"NULL", 4 }, (SYMS_U64)SYMS_ElfDynTag_NULL },
{ { (SYMS_U8*)"NEEDED", 6 }, (SYMS_U64)SYMS_ElfDynTag_NEEDED },
{ { (SYMS_U8*)"PLTRELSZ", 8 }, (SYMS_U64)SYMS_ElfDynTag_PLTRELSZ },
{ { (SYMS_U8*)"PLTGOT", 6 }, (SYMS_U64)SYMS_ElfDynTag_PLTGOT },
{ { (SYMS_U8*)"HASH", 4 }, (SYMS_U64)SYMS_ElfDynTag_HASH },
{ { (SYMS_U8*)"STRTAB", 6 }, (SYMS_U64)SYMS_ElfDynTag_STRTAB },
{ { (SYMS_U8*)"SYMTAB", 6 }, (SYMS_U64)SYMS_ElfDynTag_SYMTAB },
{ { (SYMS_U8*)"RELA", 4 }, (SYMS_U64)SYMS_ElfDynTag_RELA },
{ { (SYMS_U8*)"RELASZ", 6 }, (SYMS_U64)SYMS_ElfDynTag_RELASZ },
{ { (SYMS_U8*)"RELAENT", 7 }, (SYMS_U64)SYMS_ElfDynTag_RELAENT },
{ { (SYMS_U8*)"STRSZ", 5 }, (SYMS_U64)SYMS_ElfDynTag_STRSZ },
{ { (SYMS_U8*)"SYMENT", 6 }, (SYMS_U64)SYMS_ElfDynTag_SYMENT },
{ { (SYMS_U8*)"INIT", 4 }, (SYMS_U64)SYMS_ElfDynTag_INIT },
{ { (SYMS_U8*)"FINI", 4 }, (SYMS_U64)SYMS_ElfDynTag_FINI },
{ { (SYMS_U8*)"SONAME", 6 }, (SYMS_U64)SYMS_ElfDynTag_SONAME },
{ { (SYMS_U8*)"RPATH", 5 }, (SYMS_U64)SYMS_ElfDynTag_RPATH },
{ { (SYMS_U8*)"SYMBOLIC", 8 }, (SYMS_U64)SYMS_ElfDynTag_SYMBOLIC },
{ { (SYMS_U8*)"REL", 3 }, (SYMS_U64)SYMS_ElfDynTag_REL },
{ { (SYMS_U8*)"RELSZ", 5 }, (SYMS_U64)SYMS_ElfDynTag_RELSZ },
{ { (SYMS_U8*)"RELENT", 6 }, (SYMS_U64)SYMS_ElfDynTag_RELENT },
{ { (SYMS_U8*)"PLTREL", 6 }, (SYMS_U64)SYMS_ElfDynTag_PLTREL },
{ { (SYMS_U8*)"DEBUG", 5 }, (SYMS_U64)SYMS_ElfDynTag_DEBUG },
{ { (SYMS_U8*)"TEXTREL", 7 }, (SYMS_U64)SYMS_ElfDynTag_TEXTREL },
{ { (SYMS_U8*)"JMPREL", 6 }, (SYMS_U64)SYMS_ElfDynTag_JMPREL },
{ { (SYMS_U8*)"BIND_NOW", 8 }, (SYMS_U64)SYMS_ElfDynTag_BIND_NOW },
{ { (SYMS_U8*)"INIT_ARRAY", 10 }, (SYMS_U64)SYMS_ElfDynTag_INIT_ARRAY },
{ { (SYMS_U8*)"FINI_ARRAY", 10 }, (SYMS_U64)SYMS_ElfDynTag_FINI_ARRAY },
{ { (SYMS_U8*)"INIT_ARRAYSZ", 12 }, (SYMS_U64)SYMS_ElfDynTag_INIT_ARRAYSZ },
{ { (SYMS_U8*)"FINI_ARRAYSZ", 12 }, (SYMS_U64)SYMS_ElfDynTag_FINI_ARRAYSZ },
{ { (SYMS_U8*)"RUNPATH", 7 }, (SYMS_U64)SYMS_ElfDynTag_RUNPATH },
{ { (SYMS_U8*)"FLAGS", 5 }, (SYMS_U64)SYMS_ElfDynTag_FLAGS },
{ { (SYMS_U8*)"PREINIT_ARRAY", 13 }, (SYMS_U64)SYMS_ElfDynTag_PREINIT_ARRAY },
{ { (SYMS_U8*)"PREINIT_ARRAYSZ", 15 }, (SYMS_U64)SYMS_ElfDynTag_PREINIT_ARRAYSZ },
{ { (SYMS_U8*)"SYMTAB_SHNDX", 12 }, (SYMS_U64)SYMS_ElfDynTag_SYMTAB_SHNDX },
{ { (SYMS_U8*)"LOOS", 4 }, (SYMS_U64)SYMS_ElfDynTag_LOOS },
{ { (SYMS_U8*)"HIOS", 4 }, (SYMS_U64)SYMS_ElfDynTag_HIOS },
{ { (SYMS_U8*)"VALRNGLO", 8 }, (SYMS_U64)SYMS_ElfDynTag_VALRNGLO },
{ { (SYMS_U8*)"GNU_PRELINKED", 13 }, (SYMS_U64)SYMS_ElfDynTag_GNU_PRELINKED },
{ { (SYMS_U8*)"GNU_CONFLICTSZ", 14 }, (SYMS_U64)SYMS_ElfDynTag_GNU_CONFLICTSZ },
{ { (SYMS_U8*)"GNU_LIBLISTSZ", 13 }, (SYMS_U64)SYMS_ElfDynTag_GNU_LIBLISTSZ },
{ { (SYMS_U8*)"CHECKSUM", 8 }, (SYMS_U64)SYMS_ElfDynTag_CHECKSUM },
{ { (SYMS_U8*)"PLTPADSZ", 8 }, (SYMS_U64)SYMS_ElfDynTag_PLTPADSZ },
{ { (SYMS_U8*)"MOVEENT", 7 }, (SYMS_U64)SYMS_ElfDynTag_MOVEENT },
{ { (SYMS_U8*)"MOVESZ", 6 }, (SYMS_U64)SYMS_ElfDynTag_MOVESZ },
{ { (SYMS_U8*)"FEATURE", 7 }, (SYMS_U64)SYMS_ElfDynTag_FEATURE },
{ { (SYMS_U8*)"POSFLAG_1", 9 }, (SYMS_U64)SYMS_ElfDynTag_POSFLAG_1 },
{ { (SYMS_U8*)"SYMINSZ", 7 }, (SYMS_U64)SYMS_ElfDynTag_SYMINSZ },
{ { (SYMS_U8*)"SYMINENT", 8 }, (SYMS_U64)SYMS_ElfDynTag_SYMINENT },
{ { (SYMS_U8*)"VALRNGHI", 8 }, (SYMS_U64)SYMS_ElfDynTag_VALRNGHI },
{ { (SYMS_U8*)"ADDRRNGLO", 9 }, (SYMS_U64)SYMS_ElfDynTag_ADDRRNGLO },
{ { (SYMS_U8*)"GNU_HASH", 8 }, (SYMS_U64)SYMS_ElfDynTag_GNU_HASH },
{ { (SYMS_U8*)"TLSDESC_PLT", 11 }, (SYMS_U64)SYMS_ElfDynTag_TLSDESC_PLT },
{ { (SYMS_U8*)"TLSDESC_GOT", 11 }, (SYMS_U64)SYMS_ElfDynTag_TLSDESC_GOT },
{ { (SYMS_U8*)"GNU_CONFLICT", 12 }, (SYMS_U64)SYMS_ElfDynTag_GNU_CONFLICT },
{ { (SYMS_U8*)"GNU_LIBLIST", 11 }, (SYMS_U64)SYMS_ElfDynTag_GNU_LIBLIST },
{ { (SYMS_U8*)"CONFIG", 6 }, (SYMS_U64)SYMS_ElfDynTag_CONFIG },
{ { (SYMS_U8*)"DEPAUDIT", 8 }, (SYMS_U64)SYMS_ElfDynTag_DEPAUDIT },
{ { (SYMS_U8*)"AUDIT", 5 }, (SYMS_U64)SYMS_ElfDynTag_AUDIT },
{ { (SYMS_U8*)"PLTPAD", 6 }, (SYMS_U64)SYMS_ElfDynTag_PLTPAD },
{ { (SYMS_U8*)"MOVETAB", 7 }, (SYMS_U64)SYMS_ElfDynTag_MOVETAB },
{ { (SYMS_U8*)"SYMINFO", 7 }, (SYMS_U64)SYMS_ElfDynTag_SYMINFO },
{ { (SYMS_U8*)"ADDRRNGHI", 9 }, (SYMS_U64)SYMS_ElfDynTag_ADDRRNGHI },
{ { (SYMS_U8*)"RELACOUNT", 9 }, (SYMS_U64)SYMS_ElfDynTag_RELACOUNT },
{ { (SYMS_U8*)"RELCOUNT", 8 }, (SYMS_U64)SYMS_ElfDynTag_RELCOUNT },
{ { (SYMS_U8*)"FLAGS_1", 7 }, (SYMS_U64)SYMS_ElfDynTag_FLAGS_1 },
{ { (SYMS_U8*)"VERDEF", 6 }, (SYMS_U64)SYMS_ElfDynTag_VERDEF },
{ { (SYMS_U8*)"VERDEFNUM", 9 }, (SYMS_U64)SYMS_ElfDynTag_VERDEFNUM },
{ { (SYMS_U8*)"VERNEED", 7 }, (SYMS_U64)SYMS_ElfDynTag_VERNEED },
{ { (SYMS_U8*)"VERNEEDNUM", 10 }, (SYMS_U64)SYMS_ElfDynTag_VERNEEDNUM },
{ { (SYMS_U8*)"VERSYM", 6 }, (SYMS_U64)SYMS_ElfDynTag_VERSYM },
{ { (SYMS_U8*)"LOPROC", 6 }, (SYMS_U64)SYMS_ElfDynTag_LOPROC },
{ { (SYMS_U8*)"AUXILIARY", 9 }, (SYMS_U64)SYMS_ElfDynTag_AUXILIARY },
{ { (SYMS_U8*)"USED", 4 }, (SYMS_U64)SYMS_ElfDynTag_USED },
{ { (SYMS_U8*)"FILTER", 6 }, (SYMS_U64)SYMS_ElfDynTag_FILTER },
{ { (SYMS_U8*)"HIPROC", 6 }, (SYMS_U64)SYMS_ElfDynTag_HIPROC },
};
SYMS_SerialFlag _syms_serial_members_for_SYMS_ElfDynFlag[] = {
{ { (SYMS_U8*)"ORIGIN", 6 }, &_syms_serial_type_SYMS_U32, 0x1, 0 },
{ { (SYMS_U8*)"SYMBOLIC", 8 }, &_syms_serial_type_SYMS_U32, 0x1, 1 },
{ { (SYMS_U8*)"TEXTREL", 7 }, &_syms_serial_type_SYMS_U32, 0x1, 2 },
{ { (SYMS_U8*)"BIND_NOW", 8 }, &_syms_serial_type_SYMS_U32, 0x1, 3 },
{ { (SYMS_U8*)"STATIC_TLS", 10 }, &_syms_serial_type_SYMS_U32, 0x1, 4 },
};
SYMS_SerialFlag _syms_serial_members_for_SYMS_ElfDynFeatureFlag[] = {
{ { (SYMS_U8*)"PARINIT", 7 }, &_syms_serial_type_SYMS_U32, 0x1, 0 },
{ { (SYMS_U8*)"CONFEXP", 7 }, &_syms_serial_type_SYMS_U32, 0x1, 1 },
};
SYMS_SerialValue _syms_serial_members_for_SYMS_ElfSymBind[] = {
{ { (SYMS_U8*)"LOCAL", 5 }, (SYMS_U64)SYMS_ElfSymBind_LOCAL },
{ { (SYMS_U8*)"GLOBAL", 6 }, (SYMS_U64)SYMS_ElfSymBind_GLOBAL },
{ { (SYMS_U8*)"WEAK", 4 }, (SYMS_U64)SYMS_ElfSymBind_WEAK },
{ { (SYMS_U8*)"LOPROC", 6 }, (SYMS_U64)SYMS_ElfSymBind_LOPROC },
{ { (SYMS_U8*)"HIPROC", 6 }, (SYMS_U64)SYMS_ElfSymBind_HIPROC },
};
SYMS_SerialValue _syms_serial_members_for_SYMS_ElfSymType[] = {
{ { (SYMS_U8*)"NOTYPE", 6 }, (SYMS_U64)SYMS_ElfSymType_NOTYPE },
{ { (SYMS_U8*)"OBJECT", 6 }, (SYMS_U64)SYMS_ElfSymType_OBJECT },
{ { (SYMS_U8*)"FUNC", 4 }, (SYMS_U64)SYMS_ElfSymType_FUNC },
{ { (SYMS_U8*)"SECTION", 7 }, (SYMS_U64)SYMS_ElfSymType_SECTION },
{ { (SYMS_U8*)"FILE", 4 }, (SYMS_U64)SYMS_ElfSymType_FILE },
{ { (SYMS_U8*)"COMMON", 6 }, (SYMS_U64)SYMS_ElfSymType_COMMON },
{ { (SYMS_U8*)"TLS", 3 }, (SYMS_U64)SYMS_ElfSymType_TLS },
{ { (SYMS_U8*)"LOPROC", 6 }, (SYMS_U64)SYMS_ElfSymType_LOPROC },
{ { (SYMS_U8*)"HIPROC", 6 }, (SYMS_U64)SYMS_ElfSymType_HIPROC },
};
SYMS_SerialValue _syms_serial_members_for_SYMS_ElfSymVisibility[] = {
{ { (SYMS_U8*)"DEFAULT", 7 }, (SYMS_U64)SYMS_ElfSymVisibility_DEFAULT },
{ { (SYMS_U8*)"INTERNAL", 8 }, (SYMS_U64)SYMS_ElfSymVisibility_INTERNAL },
{ { (SYMS_U8*)"HIDDEN", 6 }, (SYMS_U64)SYMS_ElfSymVisibility_HIDDEN },
{ { (SYMS_U8*)"PROTECTED", 9 }, (SYMS_U64)SYMS_ElfSymVisibility_PROTECTED },
};
SYMS_SerialValue _syms_serial_members_for_SYMS_ElfRelocI386[] = {
{ { (SYMS_U8*)"NONE", 4 }, (SYMS_U64)SYMS_ElfRelocI386_NONE },
{ { (SYMS_U8*)"32", 2 }, (SYMS_U64)SYMS_ElfRelocI386_32 },
{ { (SYMS_U8*)"PC32", 4 }, (SYMS_U64)SYMS_ElfRelocI386_PC32 },
{ { (SYMS_U8*)"GOT32", 5 }, (SYMS_U64)SYMS_ElfRelocI386_GOT32 },
{ { (SYMS_U8*)"PLT32", 5 }, (SYMS_U64)SYMS_ElfRelocI386_PLT32 },
{ { (SYMS_U8*)"COPY", 4 }, (SYMS_U64)SYMS_ElfRelocI386_COPY },
{ { (SYMS_U8*)"GLOB_DAT", 8 }, (SYMS_U64)SYMS_ElfRelocI386_GLOB_DAT },
{ { (SYMS_U8*)"JUMP_SLOT", 9 }, (SYMS_U64)SYMS_ElfRelocI386_JUMP_SLOT },
{ { (SYMS_U8*)"RELATIVE", 8 }, (SYMS_U64)SYMS_ElfRelocI386_RELATIVE },
{ { (SYMS_U8*)"GOTOFF", 6 }, (SYMS_U64)SYMS_ElfRelocI386_GOTOFF },
{ { (SYMS_U8*)"GOTPC", 5 }, (SYMS_U64)SYMS_ElfRelocI386_GOTPC },
{ { (SYMS_U8*)"32PLT", 5 }, (SYMS_U64)SYMS_ElfRelocI386_32PLT },
{ { (SYMS_U8*)"TLS_TPOFF", 9 }, (SYMS_U64)SYMS_ElfRelocI386_TLS_TPOFF },
{ { (SYMS_U8*)"TLS_IE", 6 }, (SYMS_U64)SYMS_ElfRelocI386_TLS_IE },
{ { (SYMS_U8*)"TLS_GOTIE", 9 }, (SYMS_U64)SYMS_ElfRelocI386_TLS_GOTIE },
{ { (SYMS_U8*)"TLS_LE", 6 }, (SYMS_U64)SYMS_ElfRelocI386_TLS_LE },
{ { (SYMS_U8*)"TLS_GD", 6 }, (SYMS_U64)SYMS_ElfRelocI386_TLS_GD },
{ { (SYMS_U8*)"TLS_LDM", 7 }, (SYMS_U64)SYMS_ElfRelocI386_TLS_LDM },
{ { (SYMS_U8*)"16", 2 }, (SYMS_U64)SYMS_ElfRelocI386_16 },
{ { (SYMS_U8*)"PC16", 4 }, (SYMS_U64)SYMS_ElfRelocI386_PC16 },
{ { (SYMS_U8*)"8", 1 }, (SYMS_U64)SYMS_ElfRelocI386_8 },
{ { (SYMS_U8*)"PC8", 3 }, (SYMS_U64)SYMS_ElfRelocI386_PC8 },
{ { (SYMS_U8*)"TLS_GD_32", 9 }, (SYMS_U64)SYMS_ElfRelocI386_TLS_GD_32 },
{ { (SYMS_U8*)"TLS_GD_PUSH", 11 }, (SYMS_U64)SYMS_ElfRelocI386_TLS_GD_PUSH },
{ { (SYMS_U8*)"TLS_GD_CALL", 11 }, (SYMS_U64)SYMS_ElfRelocI386_TLS_GD_CALL },
{ { (SYMS_U8*)"TLS_GD_POP", 10 }, (SYMS_U64)SYMS_ElfRelocI386_TLS_GD_POP },
{ { (SYMS_U8*)"TLS_LDM_32", 10 }, (SYMS_U64)SYMS_ElfRelocI386_TLS_LDM_32 },
{ { (SYMS_U8*)"TLS_LDM_PUSH", 12 }, (SYMS_U64)SYMS_ElfRelocI386_TLS_LDM_PUSH },
{ { (SYMS_U8*)"TLS_LDM_CALL", 12 }, (SYMS_U64)SYMS_ElfRelocI386_TLS_LDM_CALL },
{ { (SYMS_U8*)"TLS_LDM_POP", 11 }, (SYMS_U64)SYMS_ElfRelocI386_TLS_LDM_POP },
{ { (SYMS_U8*)"TLS_LDO_32", 10 }, (SYMS_U64)SYMS_ElfRelocI386_TLS_LDO_32 },
{ { (SYMS_U8*)"TLS_IE_32", 9 }, (SYMS_U64)SYMS_ElfRelocI386_TLS_IE_32 },
{ { (SYMS_U8*)"TLS_LE_32", 9 }, (SYMS_U64)SYMS_ElfRelocI386_TLS_LE_32 },
{ { (SYMS_U8*)"TLS_DTPMOD32", 12 }, (SYMS_U64)SYMS_ElfRelocI386_TLS_DTPMOD32 },
{ { (SYMS_U8*)"TLS_DTPOFF32", 12 }, (SYMS_U64)SYMS_ElfRelocI386_TLS_DTPOFF32 },
{ { (SYMS_U8*)"TLS_TPOFF32", 11 }, (SYMS_U64)SYMS_ElfRelocI386_TLS_TPOFF32 },
{ { (SYMS_U8*)"TLS_GOTDESC", 11 }, (SYMS_U64)SYMS_ElfRelocI386_TLS_GOTDESC },
{ { (SYMS_U8*)"TLS_DESC_CALL", 13 }, (SYMS_U64)SYMS_ElfRelocI386_TLS_DESC_CALL },
{ { (SYMS_U8*)"TLS_DESC", 8 }, (SYMS_U64)SYMS_ElfRelocI386_TLS_DESC },
{ { (SYMS_U8*)"IRELATIVE", 9 }, (SYMS_U64)SYMS_ElfRelocI386_IRELATIVE },
{ { (SYMS_U8*)"GOTX32X", 7 }, (SYMS_U64)SYMS_ElfRelocI386_GOTX32X },
{ { (SYMS_U8*)"USED_BY_INTEL_200", 17 }, (SYMS_U64)SYMS_ElfRelocI386_USED_BY_INTEL_200 },
{ { (SYMS_U8*)"GNU_VTINHERIT", 13 }, (SYMS_U64)SYMS_ElfRelocI386_GNU_VTINHERIT },
{ { (SYMS_U8*)"GNU_VTENTRY", 11 }, (SYMS_U64)SYMS_ElfRelocI386_GNU_VTENTRY },
};
SYMS_SerialValue _syms_serial_members_for_SYMS_ElfRelocX8664[] = {
{ { (SYMS_U8*)"NONE", 4 }, (SYMS_U64)SYMS_ElfRelocX8664_NONE },
{ { (SYMS_U8*)"64", 2 }, (SYMS_U64)SYMS_ElfRelocX8664_64 },
{ { (SYMS_U8*)"PC32", 4 }, (SYMS_U64)SYMS_ElfRelocX8664_PC32 },
{ { (SYMS_U8*)"GOT32", 5 }, (SYMS_U64)SYMS_ElfRelocX8664_GOT32 },
{ { (SYMS_U8*)"PLT32", 5 }, (SYMS_U64)SYMS_ElfRelocX8664_PLT32 },
{ { (SYMS_U8*)"COPY", 4 }, (SYMS_U64)SYMS_ElfRelocX8664_COPY },
{ { (SYMS_U8*)"GLOB_DAT", 8 }, (SYMS_U64)SYMS_ElfRelocX8664_GLOB_DAT },
{ { (SYMS_U8*)"JUMP_SLOT", 9 }, (SYMS_U64)SYMS_ElfRelocX8664_JUMP_SLOT },
{ { (SYMS_U8*)"RELATIVE", 8 }, (SYMS_U64)SYMS_ElfRelocX8664_RELATIVE },
{ { (SYMS_U8*)"GOTPCREL", 8 }, (SYMS_U64)SYMS_ElfRelocX8664_GOTPCREL },
{ { (SYMS_U8*)"32", 2 }, (SYMS_U64)SYMS_ElfRelocX8664_32 },
{ { (SYMS_U8*)"32S", 3 }, (SYMS_U64)SYMS_ElfRelocX8664_32S },
{ { (SYMS_U8*)"16", 2 }, (SYMS_U64)SYMS_ElfRelocX8664_16 },
{ { (SYMS_U8*)"PC16", 4 }, (SYMS_U64)SYMS_ElfRelocX8664_PC16 },
{ { (SYMS_U8*)"8", 1 }, (SYMS_U64)SYMS_ElfRelocX8664_8 },
{ { (SYMS_U8*)"PC8", 3 }, (SYMS_U64)SYMS_ElfRelocX8664_PC8 },
{ { (SYMS_U8*)"DTPMOD64", 8 }, (SYMS_U64)SYMS_ElfRelocX8664_DTPMOD64 },
{ { (SYMS_U8*)"DTPOFF64", 8 }, (SYMS_U64)SYMS_ElfRelocX8664_DTPOFF64 },
{ { (SYMS_U8*)"TPOFF64", 7 }, (SYMS_U64)SYMS_ElfRelocX8664_TPOFF64 },
{ { (SYMS_U8*)"TLSGD", 5 }, (SYMS_U64)SYMS_ElfRelocX8664_TLSGD },
{ { (SYMS_U8*)"TLSLD", 5 }, (SYMS_U64)SYMS_ElfRelocX8664_TLSLD },
{ { (SYMS_U8*)"DTPOFF32", 8 }, (SYMS_U64)SYMS_ElfRelocX8664_DTPOFF32 },
{ { (SYMS_U8*)"GOTTPOFF", 8 }, (SYMS_U64)SYMS_ElfRelocX8664_GOTTPOFF },
{ { (SYMS_U8*)"TPOFF32", 7 }, (SYMS_U64)SYMS_ElfRelocX8664_TPOFF32 },
{ { (SYMS_U8*)"PC64", 4 }, (SYMS_U64)SYMS_ElfRelocX8664_PC64 },
{ { (SYMS_U8*)"GOTOFF64", 8 }, (SYMS_U64)SYMS_ElfRelocX8664_GOTOFF64 },
{ { (SYMS_U8*)"GOTPC32", 7 }, (SYMS_U64)SYMS_ElfRelocX8664_GOTPC32 },
{ { (SYMS_U8*)"GOT64", 5 }, (SYMS_U64)SYMS_ElfRelocX8664_GOT64 },
{ { (SYMS_U8*)"GOTPCREL64", 10 }, (SYMS_U64)SYMS_ElfRelocX8664_GOTPCREL64 },
{ { (SYMS_U8*)"GOTPC64", 7 }, (SYMS_U64)SYMS_ElfRelocX8664_GOTPC64 },
{ { (SYMS_U8*)"GOTPLT64", 8 }, (SYMS_U64)SYMS_ElfRelocX8664_GOTPLT64 },
{ { (SYMS_U8*)"PLTOFF64", 8 }, (SYMS_U64)SYMS_ElfRelocX8664_PLTOFF64 },
{ { (SYMS_U8*)"SIZE32", 6 }, (SYMS_U64)SYMS_ElfRelocX8664_SIZE32 },
{ { (SYMS_U8*)"SIZE64", 6 }, (SYMS_U64)SYMS_ElfRelocX8664_SIZE64 },
{ { (SYMS_U8*)"GOTPC32_TLSDESC", 15 }, (SYMS_U64)SYMS_ElfRelocX8664_GOTPC32_TLSDESC },
{ { (SYMS_U8*)"TLSDESC_CALL", 12 }, (SYMS_U64)SYMS_ElfRelocX8664_TLSDESC_CALL },
{ { (SYMS_U8*)"TLSDESC", 7 }, (SYMS_U64)SYMS_ElfRelocX8664_TLSDESC },
{ { (SYMS_U8*)"IRELATIVE", 9 }, (SYMS_U64)SYMS_ElfRelocX8664_IRELATIVE },
{ { (SYMS_U8*)"RELATIVE64", 10 }, (SYMS_U64)SYMS_ElfRelocX8664_RELATIVE64 },
{ { (SYMS_U8*)"PC32_BND", 8 }, (SYMS_U64)SYMS_ElfRelocX8664_PC32_BND },
{ { (SYMS_U8*)"PLT32_BND", 9 }, (SYMS_U64)SYMS_ElfRelocX8664_PLT32_BND },
{ { (SYMS_U8*)"GOTPCRELX", 9 }, (SYMS_U64)SYMS_ElfRelocX8664_GOTPCRELX },
{ { (SYMS_U8*)"REX_GOTPCRELX", 13 }, (SYMS_U64)SYMS_ElfRelocX8664_REX_GOTPCRELX },
{ { (SYMS_U8*)"GNU_VTINHERIT", 13 }, (SYMS_U64)SYMS_ElfRelocX8664_GNU_VTINHERIT },
{ { (SYMS_U8*)"GNU_VTENTRY", 11 }, (SYMS_U64)SYMS_ElfRelocX8664_GNU_VTENTRY },
};
SYMS_SerialFlag _syms_serial_members_for_SYMS_ElfExternalVerFlag[] = {
{ { (SYMS_U8*)"BASE", 4 }, &_syms_serial_type_SYMS_U32, 0x1, 0 },
{ { (SYMS_U8*)"WEAK", 4 }, &_syms_serial_type_SYMS_U32, 0x1, 1 },
{ { (SYMS_U8*)"INFO", 4 }, &_syms_serial_type_SYMS_U32, 0x1, 2 },
};
SYMS_SerialValue _syms_serial_members_for_SYMS_ElfNoteType[] = {
{ { (SYMS_U8*)"GNU_ABI", 7 }, (SYMS_U64)SYMS_ElfNoteType_GNU_ABI },
{ { (SYMS_U8*)"GNU_HWCAP", 9 }, (SYMS_U64)SYMS_ElfNoteType_GNU_HWCAP },
{ { (SYMS_U8*)"GNU_BUILD_ID", 12 }, (SYMS_U64)SYMS_ElfNoteType_GNU_BUILD_ID },
{ { (SYMS_U8*)"GNU_GOLD_VERSION", 16 }, (SYMS_U64)SYMS_ElfNoteType_GNU_GOLD_VERSION },
{ { (SYMS_U8*)"GNU_PROPERTY_TYPE_0", 19 }, (SYMS_U64)SYMS_ElfNoteType_GNU_PROPERTY_TYPE_0 },
};
SYMS_SerialValue _syms_serial_members_for_SYMS_ElfGnuABITag[] = {
{ { (SYMS_U8*)"Linux", 5 }, (SYMS_U64)SYMS_ElfGnuABITag_LINUX },
{ { (SYMS_U8*)"Hurd", 4 }, (SYMS_U64)SYMS_ElfGnuABITag_HURD },
{ { (SYMS_U8*)"Solaris", 7 }, (SYMS_U64)SYMS_ElfGnuABITag_SOLARIS },
{ { (SYMS_U8*)"FreeBSD", 7 }, (SYMS_U64)SYMS_ElfGnuABITag_FREEBSD },
{ { (SYMS_U8*)"NetBSD", 6 }, (SYMS_U64)SYMS_ElfGnuABITag_NETBSD },
{ { (SYMS_U8*)"Syllable", 8 }, (SYMS_U64)SYMS_ElfGnuABITag_SYLLABLE },
{ { (SYMS_U8*)"NaCL", 4 }, (SYMS_U64)SYMS_ElfGnuABITag_NACL },
};
SYMS_SerialValue _syms_serial_members_for_SYMS_ElfGnuProperty[] = {
{ { (SYMS_U8*)"LOPROC", 6 }, (SYMS_U64)SYMS_ElfGnuProperty_LOPROC },
{ { (SYMS_U8*)"HIPROC", 6 }, (SYMS_U64)SYMS_ElfGnuProperty_HIPROC },
{ { (SYMS_U8*)"LOUSER", 6 }, (SYMS_U64)SYMS_ElfGnuProperty_LOUSER },
{ { (SYMS_U8*)"HIUSER", 6 }, (SYMS_U64)SYMS_ElfGnuProperty_HIUSER },
{ { (SYMS_U8*)"STACK_SIZE", 10 }, (SYMS_U64)SYMS_ElfGnuProperty_STACK_SIZE },
{ { (SYMS_U8*)"NO_COPY_ON_PROTECTED", 20 }, (SYMS_U64)SYMS_ElfGnuProperty_NO_COPY_ON_PROTECTED },
};
SYMS_SerialFlag _syms_serial_members_for_SYMS_ElfGnuPropertyX86Isa1[] = {
{ { (SYMS_U8*)"BASE_LINE", 9 }, &_syms_serial_type_SYMS_U32, 0x1, 0 },
{ { (SYMS_U8*)"V2", 2 }, &_syms_serial_type_SYMS_U32, 0x1, 1 },
{ { (SYMS_U8*)"V3", 2 }, &_syms_serial_type_SYMS_U32, 0x1, 2 },
{ { (SYMS_U8*)"V4", 2 }, &_syms_serial_type_SYMS_U32, 0x1, 3 },
};
SYMS_SerialFlag _syms_serial_members_for_SYMS_ElfGnuPropertyX86Compat1Isa1[] = {
{ { (SYMS_U8*)"486", 3 }, &_syms_serial_type_SYMS_U32, 0x1, 0 },
{ { (SYMS_U8*)"586", 3 }, &_syms_serial_type_SYMS_U32, 0x1, 1 },
{ { (SYMS_U8*)"686", 3 }, &_syms_serial_type_SYMS_U32, 0x1, 2 },
{ { (SYMS_U8*)"SSE", 3 }, &_syms_serial_type_SYMS_U32, 0x1, 3 },
{ { (SYMS_U8*)"SSE2", 4 }, &_syms_serial_type_SYMS_U32, 0x1, 4 },
{ { (SYMS_U8*)"SSE3", 4 }, &_syms_serial_type_SYMS_U32, 0x1, 5 },
{ { (SYMS_U8*)"SSSE3", 5 }, &_syms_serial_type_SYMS_U32, 0x1, 6 },
{ { (SYMS_U8*)"SSE4_1", 6 }, &_syms_serial_type_SYMS_U32, 0x1, 7 },
{ { (SYMS_U8*)"SSE4_2", 6 }, &_syms_serial_type_SYMS_U32, 0x1, 8 },
{ { (SYMS_U8*)"AVX", 3 }, &_syms_serial_type_SYMS_U32, 0x1, 9 },
{ { (SYMS_U8*)"AVX2", 4 }, &_syms_serial_type_SYMS_U32, 0x1, 10 },
{ { (SYMS_U8*)"AVX512F", 7 }, &_syms_serial_type_SYMS_U32, 0x1, 11 },
{ { (SYMS_U8*)"AVX512ER", 8 }, &_syms_serial_type_SYMS_U32, 0x1, 12 },
{ { (SYMS_U8*)"AVX512PF", 8 }, &_syms_serial_type_SYMS_U32, 0x1, 13 },
{ { (SYMS_U8*)"AVX512VL", 8 }, &_syms_serial_type_SYMS_U32, 0x1, 14 },
{ { (SYMS_U8*)"AVX512DQ", 8 }, &_syms_serial_type_SYMS_U32, 0x1, 15 },
{ { (SYMS_U8*)"AVX512BW", 8 }, &_syms_serial_type_SYMS_U32, 0x1, 16 },
};
SYMS_SerialFlag _syms_serial_members_for_SYMS_ElfGnuPropertyX86Compat2Isa1[] = {
{ { (SYMS_U8*)"CMOVE", 5 }, &_syms_serial_type_SYMS_U32, 0x1, 0 },
{ { (SYMS_U8*)"SSE", 3 }, &_syms_serial_type_SYMS_U32, 0x1, 1 },
{ { (SYMS_U8*)"SSE2", 4 }, &_syms_serial_type_SYMS_U32, 0x1, 2 },
{ { (SYMS_U8*)"SSE3", 4 }, &_syms_serial_type_SYMS_U32, 0x1, 3 },
{ { (SYMS_U8*)"SSE4_1", 6 }, &_syms_serial_type_SYMS_U32, 0x1, 4 },
{ { (SYMS_U8*)"SSE4_2", 6 }, &_syms_serial_type_SYMS_U32, 0x1, 5 },
{ { (SYMS_U8*)"AVX", 3 }, &_syms_serial_type_SYMS_U32, 0x1, 6 },
{ { (SYMS_U8*)"AVX2", 4 }, &_syms_serial_type_SYMS_U32, 0x1, 7 },
{ { (SYMS_U8*)"FMA", 3 }, &_syms_serial_type_SYMS_U32, 0x1, 8 },
{ { (SYMS_U8*)"AVX512F", 7 }, &_syms_serial_type_SYMS_U32, 0x1, 9 },
{ { (SYMS_U8*)"AVX512CD", 8 }, &_syms_serial_type_SYMS_U32, 0x1, 10 },
{ { (SYMS_U8*)"AVX512ER", 8 }, &_syms_serial_type_SYMS_U32, 0x1, 11 },
{ { (SYMS_U8*)"AVX512PF", 8 }, &_syms_serial_type_SYMS_U32, 0x1, 12 },
{ { (SYMS_U8*)"AVX512VL", 8 }, &_syms_serial_type_SYMS_U32, 0x1, 13 },
{ { (SYMS_U8*)"AVX512DQ", 8 }, &_syms_serial_type_SYMS_U32, 0x1, 14 },
{ { (SYMS_U8*)"AVX512BW", 8 }, &_syms_serial_type_SYMS_U32, 0x1, 15 },
{ { (SYMS_U8*)"AVX512_4FMAPS", 13 }, &_syms_serial_type_SYMS_U32, 0x1, 16 },
{ { (SYMS_U8*)"AVX512_4VNNIW", 13 }, &_syms_serial_type_SYMS_U32, 0x1, 17 },
{ { (SYMS_U8*)"AVX512_BITALG", 13 }, &_syms_serial_type_SYMS_U32, 0x1, 18 },
{ { (SYMS_U8*)"AVX512_IFMA", 11 }, &_syms_serial_type_SYMS_U32, 0x1, 19 },
{ { (SYMS_U8*)"AVX512_VBMI", 11 }, &_syms_serial_type_SYMS_U32, 0x1, 20 },
{ { (SYMS_U8*)"AVX512_VBMI2", 12 }, &_syms_serial_type_SYMS_U32, 0x1, 21 },
{ { (SYMS_U8*)"AVX512_VNNI", 11 }, &_syms_serial_type_SYMS_U32, 0x1, 22 },
{ { (SYMS_U8*)"AVX512_BF16", 11 }, &_syms_serial_type_SYMS_U32, 0x1, 23 },
};
SYMS_SerialValue _syms_serial_members_for_SYMS_ElfGnuPropertyX86[] = {
{ { (SYMS_U8*)"x86 feature1", 12 }, (SYMS_U64)SYMS_ElfGnuPropertyX86_FEATURE_1_AND },
{ { (SYMS_U8*)"x86 feature2", 12 }, (SYMS_U64)SYMS_ElfGnuPropertyX86_FEATURE_2_USED },
{ { (SYMS_U8*)"x86 isa1 needed", 15 }, (SYMS_U64)SYMS_ElfGnuPropertyX86_ISA_1_NEEDED },
{ { (SYMS_U8*)"x86 isa2 needed", 15 }, (SYMS_U64)SYMS_ElfGnuPropertyX86_ISA_2_NEEDED },
{ { (SYMS_U8*)"x86 isa1 used", 13 }, (SYMS_U64)SYMS_ElfGnuPropertyX86_ISA_1_USED },
{ { (SYMS_U8*)"x86 compat isa1 used", 20 }, (SYMS_U64)SYMS_ElfGnuPropertyX86_COMPAT_ISA_1_USED },
{ { (SYMS_U8*)"x86 compat isa1 needed", 22 }, (SYMS_U64)SYMS_ElfGnuPropertyX86_COMPAT_ISA_1_NEEDED },
{ { (SYMS_U8*)"UINT32_AND_LO", 13 }, (SYMS_U64)SYMS_ElfGnuPropertyX86_UINT32_AND_LO },
{ { (SYMS_U8*)"UINT32_AND_HI", 13 }, (SYMS_U64)SYMS_ElfGnuPropertyX86_UINT32_AND_HI },
{ { (SYMS_U8*)"UINT32_OR_LO", 12 }, (SYMS_U64)SYMS_ElfGnuPropertyX86_UINT32_OR_LO },
{ { (SYMS_U8*)"UINT32_OR_HI", 12 }, (SYMS_U64)SYMS_ElfGnuPropertyX86_UINT32_OR_HI },
{ { (SYMS_U8*)"UINT32_OR_AND_LO", 16 }, (SYMS_U64)SYMS_ElfGnuPropertyX86_UINT32_OR_AND_LO },
{ { (SYMS_U8*)"UINT32_OR_AND_HI", 16 }, (SYMS_U64)SYMS_ElfGnuPropertyX86_UINT32_OR_AND_HI },
};
SYMS_SerialFlag _syms_serial_members_for_SYMS_ElfGnuPropertyX86Feature1[] = {
{ { (SYMS_U8*)"IBT", 3 }, &_syms_serial_type_SYMS_U32, 0x1, 0 },
{ { (SYMS_U8*)"SHSTK", 5 }, &_syms_serial_type_SYMS_U32, 0x1, 1 },
{ { (SYMS_U8*)"LAM_U48", 7 }, &_syms_serial_type_SYMS_U32, 0x1, 2 },
{ { (SYMS_U8*)"LAM_U57", 7 }, &_syms_serial_type_SYMS_U32, 0x1, 3 },
};
SYMS_SerialFlag _syms_serial_members_for_SYMS_ElfGnuPropertyX86Feature2[] = {
{ { (SYMS_U8*)"X86", 3 }, &_syms_serial_type_SYMS_U32, 0x1, 0 },
{ { (SYMS_U8*)"X87", 3 }, &_syms_serial_type_SYMS_U32, 0x1, 1 },
{ { (SYMS_U8*)"MMX", 3 }, &_syms_serial_type_SYMS_U32, 0x1, 2 },
{ { (SYMS_U8*)"XMM", 3 }, &_syms_serial_type_SYMS_U32, 0x1, 3 },
{ { (SYMS_U8*)"YMM", 3 }, &_syms_serial_type_SYMS_U32, 0x1, 4 },
{ { (SYMS_U8*)"ZMM", 3 }, &_syms_serial_type_SYMS_U32, 0x1, 5 },
{ { (SYMS_U8*)"FXSR", 4 }, &_syms_serial_type_SYMS_U32, 0x1, 6 },
{ { (SYMS_U8*)"XSAVE", 5 }, &_syms_serial_type_SYMS_U32, 0x1, 7 },
{ { (SYMS_U8*)"XSAVEOPT", 8 }, &_syms_serial_type_SYMS_U32, 0x1, 8 },
{ { (SYMS_U8*)"XSAVEC", 6 }, &_syms_serial_type_SYMS_U32, 0x1, 9 },
{ { (SYMS_U8*)"TMM", 3 }, &_syms_serial_type_SYMS_U32, 0x1, 10 },
{ { (SYMS_U8*)"MASK", 4 }, &_syms_serial_type_SYMS_U32, 0x1, 11 },
};

//~ generated from code at syms/metaprogram/syms_metaprogram_serial.c:1458
SYMS_SerialType _syms_serial_type_SYMS_ElfClass = {
{(SYMS_U8*)"SYMS_ElfClass", 13}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_ElfClass), _syms_serial_members_for_SYMS_ElfClass, sizeof(SYMS_ElfClass), syms_enum_index_from_value_identity
};
SYMS_SerialType _syms_serial_type_SYMS_ElfOsAbi = {
{(SYMS_U8*)"SYMS_ElfOsAbi", 13}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_ElfOsAbi), _syms_serial_members_for_SYMS_ElfOsAbi, sizeof(SYMS_ElfOsAbi), syms_enum_index_from_elf_os_abi
};
SYMS_SerialType _syms_serial_type_SYMS_ElfVersion = {
{(SYMS_U8*)"SYMS_ElfVersion", 15}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_ElfVersion), _syms_serial_members_for_SYMS_ElfVersion, sizeof(SYMS_ElfVersion), syms_enum_index_from_value_identity
};
SYMS_SerialType _syms_serial_type_SYMS_ElfMachineKind = {
{(SYMS_U8*)"SYMS_ElfMachineKind", 19}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_ElfMachineKind), _syms_serial_members_for_SYMS_ElfMachineKind, sizeof(SYMS_ElfMachineKind), syms_enum_index_from_elf_machine_kind
};
SYMS_SerialType _syms_serial_type_SYMS_ElfType = {
{(SYMS_U8*)"SYMS_ElfType", 12}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_ElfType), _syms_serial_members_for_SYMS_ElfType, sizeof(SYMS_ElfType), syms_enum_index_from_value_identity
};
SYMS_SerialType _syms_serial_type_SYMS_ElfData = {
{(SYMS_U8*)"SYMS_ElfData", 12}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_ElfData), _syms_serial_members_for_SYMS_ElfData, sizeof(SYMS_ElfData), syms_enum_index_from_value_identity
};
SYMS_SerialType _syms_serial_type_SYMS_ElfPKind = {
{(SYMS_U8*)"SYMS_ElfPKind", 13}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_ElfPKind), _syms_serial_members_for_SYMS_ElfPKind, sizeof(SYMS_ElfPKind), syms_enum_index_from_elf_p_kind
};
SYMS_SerialType _syms_serial_type_SYMS_ElfPFlag = {
{(SYMS_U8*)"SYMS_ElfPFlag", 13}, SYMS_SerialTypeKind_Flags, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_ElfPFlag), _syms_serial_members_for_SYMS_ElfPFlag, sizeof(SYMS_ElfPFlag), 0
};
SYMS_SerialType _syms_serial_type_SYMS_ElfSectionCode = {
{(SYMS_U8*)"SYMS_ElfSectionCode", 19}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_ElfSectionCode), _syms_serial_members_for_SYMS_ElfSectionCode, sizeof(SYMS_ElfSectionCode), syms_enum_index_from_elfsectioncode
};
SYMS_SerialType _syms_serial_type_SYMS_ElfSectionIndex = {
{(SYMS_U8*)"SYMS_ElfSectionIndex", 20}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_ElfSectionIndex), _syms_serial_members_for_SYMS_ElfSectionIndex, sizeof(SYMS_ElfSectionIndex), syms_enum_index_from_elf_section_index
};
SYMS_SerialType _syms_serial_type_SYMS_ElfSectionFlag = {
{(SYMS_U8*)"SYMS_ElfSectionFlag", 19}, SYMS_SerialTypeKind_Flags, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_ElfSectionFlag), _syms_serial_members_for_SYMS_ElfSectionFlag, sizeof(SYMS_ElfSectionFlag), 0
};
SYMS_SerialType _syms_serial_type_SYMS_ElfAuxType = {
{(SYMS_U8*)"SYMS_ElfAuxType", 15}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_ElfAuxType), _syms_serial_members_for_SYMS_ElfAuxType, sizeof(SYMS_ElfAuxType), syms_enum_index_from_elfauxtype
};
SYMS_SerialType _syms_serial_type_SYMS_ElfDynTag = {
{(SYMS_U8*)"SYMS_ElfDynTag", 14}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_ElfDynTag), _syms_serial_members_for_SYMS_ElfDynTag, sizeof(SYMS_ElfDynTag), syms_enum_index_from_elfdyntag
};
SYMS_SerialType _syms_serial_type_SYMS_ElfDynFlag = {
{(SYMS_U8*)"SYMS_ElfDynFlag", 15}, SYMS_SerialTypeKind_Flags, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_ElfDynFlag), _syms_serial_members_for_SYMS_ElfDynFlag, sizeof(SYMS_ElfDynFlag), 0
};
SYMS_SerialType _syms_serial_type_SYMS_ElfDynFeatureFlag = {
{(SYMS_U8*)"SYMS_ElfDynFeatureFlag", 22}, SYMS_SerialTypeKind_Flags, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_ElfDynFeatureFlag), _syms_serial_members_for_SYMS_ElfDynFeatureFlag, sizeof(SYMS_ElfDynFeatureFlag), 0
};
SYMS_SerialType _syms_serial_type_SYMS_ElfSymBind = {
{(SYMS_U8*)"SYMS_ElfSymBind", 15}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_ElfSymBind), _syms_serial_members_for_SYMS_ElfSymBind, sizeof(SYMS_ElfSymBind), syms_enum_index_from_elf_sym_bind
};
SYMS_SerialType _syms_serial_type_SYMS_ElfSymType = {
{(SYMS_U8*)"SYMS_ElfSymType", 15}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_ElfSymType), _syms_serial_members_for_SYMS_ElfSymType, sizeof(SYMS_ElfSymType), syms_enum_index_from_elf_sym_type
};
SYMS_SerialType _syms_serial_type_SYMS_ElfSymVisibility = {
{(SYMS_U8*)"SYMS_ElfSymVisibility", 21}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_ElfSymVisibility), _syms_serial_members_for_SYMS_ElfSymVisibility, sizeof(SYMS_ElfSymVisibility), syms_enum_index_from_value_identity
};
SYMS_SerialType _syms_serial_type_SYMS_ElfRelocI386 = {
{(SYMS_U8*)"SYMS_ElfRelocI386", 17}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_ElfRelocI386), _syms_serial_members_for_SYMS_ElfRelocI386, sizeof(SYMS_ElfRelocI386), syms_enum_index_from_elf_reloc_i386
};
SYMS_SerialType _syms_serial_type_SYMS_ElfRelocX8664 = {
{(SYMS_U8*)"SYMS_ElfRelocX8664", 18}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_ElfRelocX8664), _syms_serial_members_for_SYMS_ElfRelocX8664, sizeof(SYMS_ElfRelocX8664), syms_enum_index_from_elf_reloc_x8664
};
SYMS_SerialType _syms_serial_type_SYMS_ElfExternalVerFlag = {
{(SYMS_U8*)"SYMS_ElfExternalVerFlag", 23}, SYMS_SerialTypeKind_Flags, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_ElfExternalVerFlag), _syms_serial_members_for_SYMS_ElfExternalVerFlag, sizeof(SYMS_ElfExternalVerFlag), 0
};
SYMS_SerialType _syms_serial_type_SYMS_ElfNoteType = {
{(SYMS_U8*)"SYMS_ElfNoteType", 16}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_ElfNoteType), _syms_serial_members_for_SYMS_ElfNoteType, sizeof(SYMS_ElfNoteType), syms_enum_index_from_elf_note_type
};
SYMS_SerialType _syms_serial_type_SYMS_ElfGnuABITag = {
{(SYMS_U8*)"SYMS_ElfGnuABITag", 17}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_ElfGnuABITag), _syms_serial_members_for_SYMS_ElfGnuABITag, sizeof(SYMS_ElfGnuABITag), syms_enum_index_from_value_identity
};
SYMS_SerialType _syms_serial_type_SYMS_ElfGnuProperty = {
{(SYMS_U8*)"SYMS_ElfGnuProperty", 19}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_ElfGnuProperty), _syms_serial_members_for_SYMS_ElfGnuProperty, sizeof(SYMS_ElfGnuProperty), syms_enum_index_from_elf_gnu_property
};
SYMS_SerialType _syms_serial_type_SYMS_ElfGnuPropertyX86Isa1 = {
{(SYMS_U8*)"SYMS_ElfGnuPropertyX86Isa1", 26}, SYMS_SerialTypeKind_Flags, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_ElfGnuPropertyX86Isa1), _syms_serial_members_for_SYMS_ElfGnuPropertyX86Isa1, sizeof(SYMS_ElfGnuPropertyX86Isa1), 0
};
SYMS_SerialType _syms_serial_type_SYMS_ElfGnuPropertyX86Compat1Isa1 = {
{(SYMS_U8*)"SYMS_ElfGnuPropertyX86Compat1Isa1", 33}, SYMS_SerialTypeKind_Flags, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_ElfGnuPropertyX86Compat1Isa1), _syms_serial_members_for_SYMS_ElfGnuPropertyX86Compat1Isa1, sizeof(SYMS_ElfGnuPropertyX86Compat1Isa1), 0
};
SYMS_SerialType _syms_serial_type_SYMS_ElfGnuPropertyX86Compat2Isa1 = {
{(SYMS_U8*)"SYMS_ElfGnuPropertyX86Compat2Isa1", 33}, SYMS_SerialTypeKind_Flags, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_ElfGnuPropertyX86Compat2Isa1), _syms_serial_members_for_SYMS_ElfGnuPropertyX86Compat2Isa1, sizeof(SYMS_ElfGnuPropertyX86Compat2Isa1), 0
};
SYMS_SerialType _syms_serial_type_SYMS_ElfGnuPropertyX86 = {
{(SYMS_U8*)"SYMS_ElfGnuPropertyX86", 22}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_ElfGnuPropertyX86), _syms_serial_members_for_SYMS_ElfGnuPropertyX86, sizeof(SYMS_ElfGnuPropertyX86), syms_enum_index_from_elf_gnu_property_x86
};
SYMS_SerialType _syms_serial_type_SYMS_ElfGnuPropertyX86Feature1 = {
{(SYMS_U8*)"SYMS_ElfGnuPropertyX86Feature1", 30}, SYMS_SerialTypeKind_Flags, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_ElfGnuPropertyX86Feature1), _syms_serial_members_for_SYMS_ElfGnuPropertyX86Feature1, sizeof(SYMS_ElfGnuPropertyX86Feature1), 0
};
SYMS_SerialType _syms_serial_type_SYMS_ElfGnuPropertyX86Feature2 = {
{(SYMS_U8*)"SYMS_ElfGnuPropertyX86Feature2", 30}, SYMS_SerialTypeKind_Flags, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_ElfGnuPropertyX86Feature2), _syms_serial_members_for_SYMS_ElfGnuPropertyX86Feature2, sizeof(SYMS_ElfGnuPropertyX86Feature2), 0
};

#endif // defined(SYMS_ENABLE_ELF_SERIAL_INFO)


////////////////////////////////
#if defined(SYMS_ENABLE_MACH_SERIAL_INFO)

//~ generated from code at syms/metaprogram/syms_metaprogram_serial.c:1091
SYMS_API SYMS_U64
syms_enum_index_from_mach_cpu_type(SYMS_U64 v){
SYMS_U64 result = 0;
switch ((SYMS_S32)v){
default: break;
case SYMS_MachCpuType_ANY: result = 0; break;
case SYMS_MachCpuType_VAX: result = 1; break;
case SYMS_MachCpuType_RESERVED2: result = 2; break;
case SYMS_MachCpuType_RESERVED3: result = 3; break;
case SYMS_MachCpuType_RESERVED4: result = 4; break;
case SYMS_MachCpuType_RESERVED5: result = 5; break;
case SYMS_MachCpuType_MC680x0: result = 6; break;
case SYMS_MachCpuType_X86: result = 7; break;
case SYMS_MachCpuType_X86_64: result = 9; break;
case SYMS_MachCpuType_RESERVED8: result = 10; break;
case SYMS_MachCpuType_RESERVED9: result = 11; break;
case SYMS_MachCpuType_MC98000: result = 12; break;
case SYMS_MachCpuType_HPPA: result = 13; break;
case SYMS_MachCpuType_ARM: result = 14; break;
case SYMS_MachCpuType_ARM64: result = 15; break;
case SYMS_MachCpuType_MC88000: result = 16; break;
case SYMS_MachCpuType_SPARC: result = 17; break;
case SYMS_MachCpuType_I860: result = 18; break;
case SYMS_MachCpuType_ALPHA: result = 19; break;
case SYMS_MachCpuType_RESERVED17: result = 20; break;
case SYMS_MachCpuType_POWERPC: result = 21; break;
case SYMS_MachCpuType_POWERPC64: result = 22; break;
}
return(result);
}
SYMS_API SYMS_U64
syms_enum_index_from_mach_cpu_family(SYMS_U64 v){
SYMS_U64 result = 0;
switch ((SYMS_S32)v){
default: break;
case SYMS_MachCpuFamily_UNKNOWN: result = 0; break;
case SYMS_MachCpuFamily_POWERPC_G3: result = 1; break;
case SYMS_MachCpuFamily_POWERPC_G4: result = 2; break;
case SYMS_MachCpuFamily_POWERPC_G5: result = 3; break;
case SYMS_MachCpuFamily_INTEL_6_13: result = 4; break;
case SYMS_MachCpuFamily_INTEL_PENRYN: result = 5; break;
case SYMS_MachCpuFamily_INTEL_NEHALEM: result = 6; break;
case SYMS_MachCpuFamily_INTEL_WESTMERE: result = 7; break;
case SYMS_MachCpuFamily_INTEL_SANDYBRIDGE: result = 8; break;
case SYMS_MachCpuFamily_INTEL_IVYBRIDGE: result = 9; break;
case SYMS_MachCpuFamily_INTEL_HASWELL: result = 10; break;
case SYMS_MachCpuFamily_INTEL_BROADWELL: result = 11; break;
case SYMS_MachCpuFamily_INTEL_SKYLAKE: result = 12; break;
case SYMS_MachCpuFamily_INTEL_KABYLAKE: result = 13; break;
case SYMS_MachCpuFamily_ARM_9: result = 14; break;
case SYMS_MachCpuFamily_ARM_11: result = 15; break;
case SYMS_MachCpuFamily_ARM_XSCALE: result = 16; break;
case SYMS_MachCpuFamily_ARM_12: result = 17; break;
case SYMS_MachCpuFamily_ARM_13: result = 18; break;
case SYMS_MachCpuFamily_ARM_14: result = 19; break;
case SYMS_MachCpuFamily_ARM_15: result = 20; break;
case SYMS_MachCpuFamily_ARM_SWIFT: result = 21; break;
case SYMS_MachCpuFamily_ARM_CYCLONE: result = 22; break;
case SYMS_MachCpuFamily_ARM_TYPHOON: result = 23; break;
case SYMS_MachCpuFamily_ARM_TWISTER: result = 24; break;
case SYMS_MachCpuFamily_ARM_HURRICANE: result = 25; break;
}
return(result);
}
// syms_enum_index_from_mach_cpu_subtype - skipped identity mapping
// syms_enum_index_from_mach_cpu_subtype_v_ax - skipped identity mapping
SYMS_API SYMS_U64
syms_enum_index_from_mach_cpu_subtype_x86(SYMS_U64 v){
SYMS_U64 result = 0;
switch ((SYMS_U32)v){
default: break;
case SYMS_MachCpuSubtypeX86_X86_ALL: result = 0; break;
case SYMS_MachCpuSubtypeX86_X86_ARCH1: result = 2; break;
case SYMS_MachCpuSubtypeX86_X86_64_HASWELL: result = 3; break;
}
return(result);
}
SYMS_API SYMS_U64
syms_enum_index_from_mach_cpu_subtype_intel(SYMS_U64 v){
SYMS_U64 result = 0;
switch ((SYMS_U32)v){
default: break;
case SYMS_MachCpuSubtypeIntel_I386_ALL: result = 0; break;
case SYMS_MachCpuSubtypeIntel_I486: result = 2; break;
case SYMS_MachCpuSubtypeIntel_I486SX: result = 3; break;
case SYMS_MachCpuSubtypeIntel_I586: result = 4; break;
case SYMS_MachCpuSubtypeIntel_PENTPRO: result = 6; break;
case SYMS_MachCpuSubtypeIntel_PENTII_M3: result = 7; break;
case SYMS_MachCpuSubtypeIntel_PENTII_M5: result = 8; break;
case SYMS_MachCpuSubtypeIntel_CELERON: result = 9; break;
case SYMS_MachCpuSubtypeIntel_CELERON_MOBILE: result = 10; break;
case SYMS_MachCpuSubtypeIntel_PENTIUM_3: result = 11; break;
case SYMS_MachCpuSubtypeIntel_PENTIUM_3_M: result = 12; break;
case SYMS_MachCpuSubtypeIntel_PENTIUM_3_XENON: result = 13; break;
case SYMS_MachCpuSubtypeIntel_PENTIUM_M: result = 14; break;
case SYMS_MachCpuSubtypeIntel_PENTIUM_4: result = 15; break;
case SYMS_MachCpuSubtypeIntel_PENTIUM_4_M: result = 16; break;
case SYMS_MachCpuSubtypeIntel_PENTIUM_ITANIUM: result = 17; break;
case SYMS_MachCpuSubtypeIntel_PENTIUM_ITANIUM_2: result = 18; break;
case SYMS_MachCpuSubtypeIntel_XEON: result = 19; break;
case SYMS_MachCpuSubtypeIntel_XEON_MP: result = 20; break;
}
return(result);
}
SYMS_API SYMS_U64
syms_enum_index_from_mach_cpu_subtype_a_rm(SYMS_U64 v){
SYMS_U64 result = 0;
switch ((SYMS_U32)v){
default: break;
case SYMS_MachCpuSubtypeARM_ALL: result = 0; break;
case SYMS_MachCpuSubtypeARM_V4T: result = 1; break;
case SYMS_MachCpuSubtypeARM_V6: result = 2; break;
case SYMS_MachCpuSubtypeARM_V5TEJ: result = 3; break;
case SYMS_MachCpuSubtypeARM_XSCALE: result = 4; break;
case SYMS_MachCpuSubtypeARM_V7: result = 5; break;
case SYMS_MachCpuSubtypeARM_V7F: result = 6; break;
case SYMS_MachCpuSubtypeARM_V7S: result = 7; break;
case SYMS_MachCpuSubtypeARM_V7K: result = 8; break;
case SYMS_MachCpuSubtypeARM_V6M: result = 9; break;
case SYMS_MachCpuSubtypeARM_V7M: result = 10; break;
case SYMS_MachCpuSubtypeARM_V7EM: result = 11; break;
case SYMS_MachCpuSubtypeARM_V8: result = 12; break;
}
return(result);
}
// syms_enum_index_from_mach_cpu_subtype_a_r_m64 - skipped identity mapping
SYMS_API SYMS_U64
syms_enum_index_from_mach_filetype(SYMS_U64 v){
SYMS_U64 result = 0;
switch ((SYMS_U32)v){
default: break;
case SYMS_MachFiletype_OBJECT: result = 0; break;
case SYMS_MachFiletype_EXECUTE: result = 1; break;
case SYMS_MachFiletype_FVMLIB: result = 2; break;
case SYMS_MachFiletype_CORE: result = 3; break;
case SYMS_MachFiletype_PRELOAD: result = 4; break;
case SYMS_MachFiletype_DYLIB: result = 5; break;
case SYMS_MachFiletype_DYLINKER: result = 6; break;
case SYMS_MachFiletype_BUNDLE: result = 7; break;
case SYMS_MachFiletype_DYLIB_STUB: result = 8; break;
case SYMS_MachFiletype_DSYM: result = 9; break;
case SYMS_MachFiletype_KEXT_BUNDLE: result = 10; break;
}
return(result);
}
SYMS_API SYMS_U64
syms_enum_index_from_mach_flags(SYMS_U64 v){
SYMS_U64 result = 0;
switch ((SYMS_U32)v){
default: break;
case SYMS_MachFlags_NOUNDEFS: result = 0; break;
case SYMS_MachFlags_INCRLINK: result = 1; break;
case SYMS_MachFlags_DYLDLINK: result = 2; break;
case SYMS_MachFlags_BINDATLOAD: result = 3; break;
case SYMS_MachFlags_PREBOUND: result = 4; break;
case SYMS_MachFlags_SPLIT_SEGS: result = 5; break;
case SYMS_MachFlags_LAZY_INIT: result = 6; break;
case SYMS_MachFlags_TWOLEVEL: result = 7; break;
case SYMS_MachFlags_FORCE_FLAT: result = 8; break;
case SYMS_MachFlags_NOMULTIDEFS: result = 9; break;
case SYMS_MachFlags_NOFIXPREBOUNDING: result = 10; break;
case SYMS_MachFlags_PREBINDABLE: result = 11; break;
case SYMS_MachFlags_ALLMODSBOUND: result = 12; break;
case SYMS_MachFlags_SUBSECTIONS_VIA_SYMBOLS: result = 13; break;
case SYMS_MachFlags_CANONICAL: result = 14; break;
case SYMS_MachFlags_WEAK_DEFINES: result = 15; break;
case SYMS_MachFlags_BINDS_TO_WEAK: result = 16; break;
case SYMS_MachFlags_ALLOW_STACK_EXECUTION: result = 17; break;
case SYMS_MachFlags_ROOT_SAFE: result = 18; break;
case SYMS_MachFlags_SETUID_SAFE: result = 19; break;
case SYMS_MachFlags_NO_REEXPORTED_DYLIBS: result = 20; break;
case SYMS_MachFlags_PIE: result = 21; break;
case SYMS_MachFlags_STRIPPABLE_DYLIB: result = 22; break;
case SYMS_MachFlags_HAS_TLV_DESRIPTORS: result = 23; break;
case SYMS_MachFlags_NO_HEAP_EXECUTION: result = 24; break;
}
return(result);
}
SYMS_API SYMS_U64
syms_enum_index_from_mach_load_command_type(SYMS_U64 v){
SYMS_U64 result = 0;
switch ((SYMS_S32)v){
default: break;
case SYMS_MachLoadCommandType_SEGMENT: result = 0; break;
case SYMS_MachLoadCommandType_SYMTAB: result = 1; break;
case SYMS_MachLoadCommandType_SYMSEG: result = 2; break;
case SYMS_MachLoadCommandType_THREAD: result = 3; break;
case SYMS_MachLoadCommandType_UNIXTHREAD: result = 4; break;
case SYMS_MachLoadCommandType_LOADFVMLIB: result = 5; break;
case SYMS_MachLoadCommandType_IDFVMLIB: result = 6; break;
case SYMS_MachLoadCommandType_IDENT: result = 7; break;
case SYMS_MachLoadCommandType_FVMFILE: result = 8; break;
case SYMS_MachLoadCommandType_PREPAGE: result = 9; break;
case SYMS_MachLoadCommandType_DYSYMTAB: result = 10; break;
case SYMS_MachLoadCommandType_LOAD_DYLIB: result = 11; break;
case SYMS_MachLoadCommandType_ID_DYLIB: result = 12; break;
case SYMS_MachLoadCommandType_LOAD_DYLINKER: result = 13; break;
case SYMS_MachLoadCommandType_ID_DYLINKER: result = 14; break;
case SYMS_MachLoadCommandType_PREBOUND_DYLIB: result = 15; break;
case SYMS_MachLoadCommandType_ROUTINES: result = 16; break;
case SYMS_MachLoadCommandType_SUB_FRAMEWORK: result = 17; break;
case SYMS_MachLoadCommandType_SUB_UMBRELLA: result = 18; break;
case SYMS_MachLoadCommandType_SUB_CLIENT: result = 19; break;
case SYMS_MachLoadCommandType_SUB_LIBRARY: result = 20; break;
case SYMS_MachLoadCommandType_TWOLEVEL_HINTS: result = 21; break;
case SYMS_MachLoadCommandType_PREBIND_CHKSUM: result = 22; break;
case SYMS_MachLoadCommandType_LOAD_WEAK_DYLIB: result = 23; break;
case SYMS_MachLoadCommandType_SEGMENT_64: result = 24; break;
case SYMS_MachLoadCommandType_ROUTINES_64: result = 25; break;
case SYMS_MachLoadCommandType_UUID: result = 26; break;
case SYMS_MachLoadCommandType_RPATH: result = 27; break;
case SYMS_MachLoadCommandType_CODE_SIGNATURE: result = 28; break;
case SYMS_MachLoadCommandType_SEGMENT_SPLIT_INFO: result = 29; break;
case SYMS_MachLoadCommandType_REEXPORT_DYLIB: result = 30; break;
case SYMS_MachLoadCommandType_LAZY_LOAD_DYLIB: result = 31; break;
case SYMS_MachLoadCommandType_ENCRYPTION_INFO: result = 32; break;
case SYMS_MachLoadCommandType_DYLD_INFO: result = 33; break;
case SYMS_MachLoadCommandType_DYLD_INFO_ONLY: result = 34; break;
case SYMS_MachLoadCommandType_LOAD_UPWARD_DYLIB: result = 35; break;
case SYMS_MachLoadCommandType_VERSION_MIN_MACOSX: result = 36; break;
case SYMS_MachLoadCommandType_VERSION_MIN_IPHONES: result = 37; break;
case SYMS_MachLoadCommandType_FUNCTION_STARTS: result = 38; break;
case SYMS_MachLoadCommandType_DYLD_ENVIORNMENT: result = 39; break;
case SYMS_MachLoadCommandType_MAIN: result = 40; break;
case SYMS_MachLoadCommandType_DATA_IN_CODE: result = 41; break;
case SYMS_MachLoadCommandType_SOURCE_VERSION: result = 42; break;
case SYMS_MachLoadCommandType_DYLIB_CODE_SIGN_DRS: result = 43; break;
case SYMS_MachLoadCommandType_ENCRYPTION_INFO_64: result = 44; break;
case SYMS_MachLoadCommandType_LINKER_OPTION: result = 45; break;
case SYMS_MachLoadCommandType_LINKER_OPTIMIZATION_HINT: result = 46; break;
case SYMS_MachLoadCommandType_VERSION_MIN_TVOS: result = 47; break;
case SYMS_MachLoadCommandType_VERSION_MIN_WATCHOS: result = 48; break;
case SYMS_MachLoadCommandType_NOTE: result = 49; break;
case SYMS_MachLoadCommandType_BUILD_VERSION: result = 50; break;
}
return(result);
}
// syms_enum_index_from_mach_section_type - skipped identity mapping
SYMS_API SYMS_U64
syms_enum_index_from_mach_section_attr(SYMS_U64 v){
SYMS_U64 result = 0;
switch ((SYMS_S32)v){
default: break;
case SYMS_MachSectionAttr_USR: result = 0; break;
case SYMS_MachSectionAttr_SYSTEM: result = 1; break;
case SYMS_MachSectionAttr_PURE_INSTRUCTIONS: result = 2; break;
case SYMS_MachSectionAttr_NO_TOC: result = 3; break;
case SYMS_MachSectionAttr_STRIP_STATIC_SYMS: result = 4; break;
case SYMS_MachSectionAttr_NO_DEAD_STRIP: result = 5; break;
case SYMS_MachSectionAttr_LIVE_SUPPORT: result = 6; break;
case SYMS_MachSectionAttr_SELF_MODIFYING_CODE: result = 7; break;
case SYMS_MachSectionAttr_DEBUG: result = 8; break;
case SYMS_MachSectionAttr_SOME_INSTRUCTIONS: result = 9; break;
case SYMS_MachSectionAttr_SECTION_RELOC: result = 10; break;
case SYMS_MachSectionAttr_LOC_RELOC: result = 11; break;
}
return(result);
}
SYMS_API SYMS_U64
syms_enum_index_from_mach_platform_type(SYMS_U64 v){
SYMS_U64 result = 0;
switch ((SYMS_U32)v){
default: break;
case SYMS_MachPlatformType_MACOS: result = 0; break;
case SYMS_MachPlatformType_IOS: result = 1; break;
case SYMS_MachPlatformType_TVOS: result = 2; break;
case SYMS_MachPlatformType_WATCHOS: result = 3; break;
}
return(result);
}
SYMS_API SYMS_U64
syms_enum_index_from_mach_tool_type(SYMS_U64 v){
SYMS_U64 result = 0;
switch ((SYMS_U32)v){
default: break;
case SYMS_MachToolType_CLANG: result = 0; break;
case SYMS_MachToolType_SWITFT: result = 1; break;
case SYMS_MachToolType_LD: result = 2; break;
}
return(result);
}
SYMS_API SYMS_U64
syms_enum_index_from_mach_bind_type(SYMS_U64 v){
SYMS_U64 result = 0;
switch ((SYMS_U8)v){
default: break;
case SYMS_MachBindType_POINTER: result = 0; break;
case SYMS_MachBindType_TEXT_ABSOLUTE32: result = 1; break;
case SYMS_MachBindType_PCREL32: result = 2; break;
}
return(result);
}
SYMS_API SYMS_U64
syms_enum_index_from_mach_bind_opcode(SYMS_U64 v){
SYMS_U64 result = 0;
switch ((SYMS_U8)v){
default: break;
case SYMS_MachBindOpcode_DONE: result = 0; break;
case SYMS_MachBindOpcode_SET_DYLIB_ORDINAL_IMM: result = 1; break;
case SYMS_MachBindOpcode_SET_DYLIB_ORDINAL_ULEB: result = 2; break;
case SYMS_MachBindOpcode_SET_DYLIB_SPECIAL_IMM: result = 3; break;
case SYMS_MachBindOpcode_SET_SYMBOL_TRAILING_FLAGS_IMM: result = 4; break;
case SYMS_MachBindOpcode_SET_TYPE_IMM: result = 5; break;
case SYMS_MachBindOpcode_SET_ADDEND_SLEB: result = 6; break;
case SYMS_MachBindOpcode_SET_SEGMENT_AND_OFFSET_ULEB: result = 7; break;
case SYMS_MachBindOpcode_ADD_ADDR_ULEB: result = 8; break;
case SYMS_MachBindOpcode_DO_BIND: result = 9; break;
case SYMS_MachBindOpcode_DO_BIND_ADD_ADDR_ULEB: result = 10; break;
case SYMS_MachBindOpcode_DO_BIND_ADD_ADDR_IMM_SCALED: result = 11; break;
case SYMS_MachBindOpcode_DO_BIND_ULEB_TIMES_SKIPPING_ULEB: result = 12; break;
case SYMS_MachBindOpcode_MASK: result = 13; break;
case SYMS_MachBindOpcode_IMM_MASK: result = 14; break;
}
return(result);
}
SYMS_API SYMS_U64
syms_enum_index_from_mach_bind_special_dylib(SYMS_U64 v){
SYMS_U64 result = 0;
switch ((SYMS_S8)v){
default: break;
case SYMS_MachBindSpecialDylib_SELF: result = 0; break;
case SYMS_MachBindSpecialDylib_MAIN_EXECUTABLE: result = 1; break;
case SYMS_MachBindSpecialDylib_FLAT_LOOKUP: result = 2; break;
}
return(result);
}
SYMS_API SYMS_U64
syms_enum_index_from_mach_n_list_type(SYMS_U64 v){
SYMS_U64 result = 0;
switch ((SYMS_U32)v){
default: break;
case SYMS_MachNListType_UNDF: result = 0; break;
case SYMS_MachNListType_ABS: result = 1; break;
case SYMS_MachNListType_SECT: result = 2; break;
case SYMS_MachNListType_PBUD: result = 3; break;
case SYMS_MachNListType_INDR: result = 4; break;
}
return(result);
}
SYMS_API SYMS_U64
syms_enum_index_from_mach_stab_type(SYMS_U64 v){
SYMS_U64 result = 0;
switch ((SYMS_U32)v){
default: break;
case SYMS_MachStabType_GSYM: result = 0; break;
case SYMS_MachStabType_FNAME: result = 1; break;
case SYMS_MachStabType_FUN: result = 2; break;
case SYMS_MachStabType_STSYM: result = 3; break;
case SYMS_MachStabType_LCSYM: result = 4; break;
case SYMS_MachStabType_BNSYM: result = 5; break;
case SYMS_MachStabType_AST: result = 6; break;
case SYMS_MachStabType_OPT: result = 7; break;
case SYMS_MachStabType_RSYM: result = 8; break;
case SYMS_MachStabType_SLINE: result = 9; break;
case SYMS_MachStabType_ENSYM: result = 10; break;
case SYMS_MachStabType_SSYM: result = 11; break;
case SYMS_MachStabType_SO: result = 12; break;
case SYMS_MachStabType_OSO: result = 13; break;
case SYMS_MachStabType_LSYM: result = 14; break;
case SYMS_MachStabType_BINCL: result = 15; break;
case SYMS_MachStabType_SOL: result = 16; break;
case SYMS_MachStabType_PARAMS: result = 17; break;
case SYMS_MachStabType_VERSION: result = 18; break;
case SYMS_MachStabType_OLEVEL: result = 19; break;
case SYMS_MachStabType_PSYM: result = 20; break;
case SYMS_MachStabType_EINCL: result = 21; break;
case SYMS_MachStabType_ENTRY: result = 22; break;
case SYMS_MachStabType_LBRAC: result = 23; break;
case SYMS_MachStabType_EXCL: result = 24; break;
case SYMS_MachStabType_RBRAC: result = 25; break;
case SYMS_MachStabType_BCOMM: result = 26; break;
case SYMS_MachStabType_ECOMM: result = 27; break;
case SYMS_MachStabType_ECOML: result = 28; break;
case SYMS_MachStabType_LENG: result = 29; break;
}
return(result);
}
// syms_enum_index_from_mach_export_symbol_kind - skipped identity mapping
SYMS_API SYMS_U64
syms_enum_index_from_mach_unwind_enc_mode_x86(SYMS_U64 v){
SYMS_U64 result = 0;
switch ((SYMS_U32)v){
default: break;
case SYMS_MachUnwindEncModeX86_EBP_FRAME: result = 0; break;
case SYMS_MachUnwindEncModeX86_STACK_IMMD: result = 1; break;
case SYMS_MachUnwindEncModeX86_STACK_IND: result = 2; break;
case SYMS_MachUnwindEncModeX86_DWARF: result = 3; break;
}
return(result);
}
// syms_enum_index_from_mach_unwind_register_x86 - skipped identity mapping
SYMS_API SYMS_U64
syms_enum_index_from_mach_unwind_enc_mode_x64(SYMS_U64 v){
SYMS_U64 result = 0;
switch ((SYMS_U32)v){
default: break;
case SYMS_MachUnwindEncModeX64_RBP_FRAME: result = 0; break;
case SYMS_MachUnwindEncModeX64_STACK_IMMD: result = 1; break;
case SYMS_MachUnwindEncModeX64_STACK_IND: result = 2; break;
case SYMS_MachUnwindEncModeX64_DWARF: result = 3; break;
}
return(result);
}
// syms_enum_index_from_mach_unwind_register_x64 - skipped identity mapping

//~ generated from code at syms/metaprogram/syms_metaprogram_serial.c:1322
SYMS_SerialValue _syms_serial_members_for_SYMS_MachCpuType[] = {
{ { (SYMS_U8*)"ANY", 3 }, (SYMS_U64)SYMS_MachCpuType_ANY },
{ { (SYMS_U8*)"VAX", 3 }, (SYMS_U64)SYMS_MachCpuType_VAX },
{ { (SYMS_U8*)"RESERVED2", 9 }, (SYMS_U64)SYMS_MachCpuType_RESERVED2 },
{ { (SYMS_U8*)"RESERVED3", 9 }, (SYMS_U64)SYMS_MachCpuType_RESERVED3 },
{ { (SYMS_U8*)"RESERVED4", 9 }, (SYMS_U64)SYMS_MachCpuType_RESERVED4 },
{ { (SYMS_U8*)"RESERVED5", 9 }, (SYMS_U64)SYMS_MachCpuType_RESERVED5 },
{ { (SYMS_U8*)"MC680x0", 7 }, (SYMS_U64)SYMS_MachCpuType_MC680x0 },
{ { (SYMS_U8*)"X86", 3 }, (SYMS_U64)SYMS_MachCpuType_X86 },
{ { (SYMS_U8*)"I386", 4 }, (SYMS_U64)SYMS_MachCpuType_I386 },
{ { (SYMS_U8*)"X86_64", 6 }, (SYMS_U64)SYMS_MachCpuType_X86_64 },
{ { (SYMS_U8*)"RESERVED8", 9 }, (SYMS_U64)SYMS_MachCpuType_RESERVED8 },
{ { (SYMS_U8*)"RESERVED9", 9 }, (SYMS_U64)SYMS_MachCpuType_RESERVED9 },
{ { (SYMS_U8*)"MC98000", 7 }, (SYMS_U64)SYMS_MachCpuType_MC98000 },
{ { (SYMS_U8*)"HPPA", 4 }, (SYMS_U64)SYMS_MachCpuType_HPPA },
{ { (SYMS_U8*)"ARM", 3 }, (SYMS_U64)SYMS_MachCpuType_ARM },
{ { (SYMS_U8*)"ARM64", 5 }, (SYMS_U64)SYMS_MachCpuType_ARM64 },
{ { (SYMS_U8*)"MC88000", 7 }, (SYMS_U64)SYMS_MachCpuType_MC88000 },
{ { (SYMS_U8*)"SPARC", 5 }, (SYMS_U64)SYMS_MachCpuType_SPARC },
{ { (SYMS_U8*)"I860", 4 }, (SYMS_U64)SYMS_MachCpuType_I860 },
{ { (SYMS_U8*)"ALPHA", 5 }, (SYMS_U64)SYMS_MachCpuType_ALPHA },
{ { (SYMS_U8*)"RESERVED17", 10 }, (SYMS_U64)SYMS_MachCpuType_RESERVED17 },
{ { (SYMS_U8*)"POWERPC", 7 }, (SYMS_U64)SYMS_MachCpuType_POWERPC },
{ { (SYMS_U8*)"POWERPC64", 9 }, (SYMS_U64)SYMS_MachCpuType_POWERPC64 },
};
SYMS_SerialValue _syms_serial_members_for_SYMS_MachCpuFamily[] = {
{ { (SYMS_U8*)"UNKNOWN", 7 }, (SYMS_U64)SYMS_MachCpuFamily_UNKNOWN },
{ { (SYMS_U8*)"POWERPC_G3", 10 }, (SYMS_U64)SYMS_MachCpuFamily_POWERPC_G3 },
{ { (SYMS_U8*)"POWERPC_G4", 10 }, (SYMS_U64)SYMS_MachCpuFamily_POWERPC_G4 },
{ { (SYMS_U8*)"POWERPC_G5", 10 }, (SYMS_U64)SYMS_MachCpuFamily_POWERPC_G5 },
{ { (SYMS_U8*)"INTEL_6_13", 10 }, (SYMS_U64)SYMS_MachCpuFamily_INTEL_6_13 },
{ { (SYMS_U8*)"INTEL_PENRYN", 12 }, (SYMS_U64)SYMS_MachCpuFamily_INTEL_PENRYN },
{ { (SYMS_U8*)"INTEL_NEHALEM", 13 }, (SYMS_U64)SYMS_MachCpuFamily_INTEL_NEHALEM },
{ { (SYMS_U8*)"INTEL_WESTMERE", 14 }, (SYMS_U64)SYMS_MachCpuFamily_INTEL_WESTMERE },
{ { (SYMS_U8*)"INTEL_SANDYBRIDGE", 17 }, (SYMS_U64)SYMS_MachCpuFamily_INTEL_SANDYBRIDGE },
{ { (SYMS_U8*)"INTEL_IVYBRIDGE", 15 }, (SYMS_U64)SYMS_MachCpuFamily_INTEL_IVYBRIDGE },
{ { (SYMS_U8*)"INTEL_HASWELL", 13 }, (SYMS_U64)SYMS_MachCpuFamily_INTEL_HASWELL },
{ { (SYMS_U8*)"INTEL_BROADWELL", 15 }, (SYMS_U64)SYMS_MachCpuFamily_INTEL_BROADWELL },
{ { (SYMS_U8*)"INTEL_SKYLAKE", 13 }, (SYMS_U64)SYMS_MachCpuFamily_INTEL_SKYLAKE },
{ { (SYMS_U8*)"INTEL_KABYLAKE", 14 }, (SYMS_U64)SYMS_MachCpuFamily_INTEL_KABYLAKE },
{ { (SYMS_U8*)"ARM_9", 5 }, (SYMS_U64)SYMS_MachCpuFamily_ARM_9 },
{ { (SYMS_U8*)"ARM_11", 6 }, (SYMS_U64)SYMS_MachCpuFamily_ARM_11 },
{ { (SYMS_U8*)"ARM_XSCALE", 10 }, (SYMS_U64)SYMS_MachCpuFamily_ARM_XSCALE },
{ { (SYMS_U8*)"ARM_12", 6 }, (SYMS_U64)SYMS_MachCpuFamily_ARM_12 },
{ { (SYMS_U8*)"ARM_13", 6 }, (SYMS_U64)SYMS_MachCpuFamily_ARM_13 },
{ { (SYMS_U8*)"ARM_14", 6 }, (SYMS_U64)SYMS_MachCpuFamily_ARM_14 },
{ { (SYMS_U8*)"ARM_15", 6 }, (SYMS_U64)SYMS_MachCpuFamily_ARM_15 },
{ { (SYMS_U8*)"ARM_SWIFT", 9 }, (SYMS_U64)SYMS_MachCpuFamily_ARM_SWIFT },
{ { (SYMS_U8*)"ARM_CYCLONE", 11 }, (SYMS_U64)SYMS_MachCpuFamily_ARM_CYCLONE },
{ { (SYMS_U8*)"ARM_TYPHOON", 11 }, (SYMS_U64)SYMS_MachCpuFamily_ARM_TYPHOON },
{ { (SYMS_U8*)"ARM_TWISTER", 11 }, (SYMS_U64)SYMS_MachCpuFamily_ARM_TWISTER },
{ { (SYMS_U8*)"ARM_HURRICANE", 13 }, (SYMS_U64)SYMS_MachCpuFamily_ARM_HURRICANE },
{ { (SYMS_U8*)"INTEL_6_23", 10 }, (SYMS_U64)SYMS_MachCpuFamily_INTEL_6_23 },
{ { (SYMS_U8*)"INTEL_6_26", 10 }, (SYMS_U64)SYMS_MachCpuFamily_INTEL_6_26 },
};
SYMS_SerialValue _syms_serial_members_for_SYMS_MachCpuSubtype[] = {
{ { (SYMS_U8*)"ALL", 3 }, (SYMS_U64)SYMS_MachCpuSubtype_ALL },
};
SYMS_SerialValue _syms_serial_members_for_SYMS_MachCpuSubtypeVAX[] = {
{ { (SYMS_U8*)"VAX_ALL", 7 }, (SYMS_U64)SYMS_MachCpuSubtypeVAX_VAX_ALL },
{ { (SYMS_U8*)"VAX780", 6 }, (SYMS_U64)SYMS_MachCpuSubtypeVAX_VAX780 },
{ { (SYMS_U8*)"VAX785", 6 }, (SYMS_U64)SYMS_MachCpuSubtypeVAX_VAX785 },
{ { (SYMS_U8*)"VAX750", 6 }, (SYMS_U64)SYMS_MachCpuSubtypeVAX_VAX750 },
{ { (SYMS_U8*)"VAX730", 6 }, (SYMS_U64)SYMS_MachCpuSubtypeVAX_VAX730 },
{ { (SYMS_U8*)"UVAXI", 5 }, (SYMS_U64)SYMS_MachCpuSubtypeVAX_UVAXI },
{ { (SYMS_U8*)"UVAXII", 6 }, (SYMS_U64)SYMS_MachCpuSubtypeVAX_UVAXII },
{ { (SYMS_U8*)"VAX8200", 7 }, (SYMS_U64)SYMS_MachCpuSubtypeVAX_VAX8200 },
{ { (SYMS_U8*)"VAX8500", 7 }, (SYMS_U64)SYMS_MachCpuSubtypeVAX_VAX8500 },
{ { (SYMS_U8*)"VAX8600", 7 }, (SYMS_U64)SYMS_MachCpuSubtypeVAX_VAX8600 },
{ { (SYMS_U8*)"VAX8650", 7 }, (SYMS_U64)SYMS_MachCpuSubtypeVAX_VAX8650 },
{ { (SYMS_U8*)"VAX8800", 7 }, (SYMS_U64)SYMS_MachCpuSubtypeVAX_VAX8800 },
{ { (SYMS_U8*)"UVAXIII", 7 }, (SYMS_U64)SYMS_MachCpuSubtypeVAX_UVAXIII },
};
SYMS_SerialValue _syms_serial_members_for_SYMS_MachCpuSubtypeX86[] = {
{ { (SYMS_U8*)"X86_ALL", 7 }, (SYMS_U64)SYMS_MachCpuSubtypeX86_X86_ALL },
{ { (SYMS_U8*)"X86_64_ALL", 10 }, (SYMS_U64)SYMS_MachCpuSubtypeX86_X86_64_ALL },
{ { (SYMS_U8*)"X86_ARCH1", 9 }, (SYMS_U64)SYMS_MachCpuSubtypeX86_X86_ARCH1 },
{ { (SYMS_U8*)"X86_64_HASWELL", 14 }, (SYMS_U64)SYMS_MachCpuSubtypeX86_X86_64_HASWELL },
};
SYMS_SerialValue _syms_serial_members_for_SYMS_MachCpuSubtypeIntel[] = {
{ { (SYMS_U8*)"I386_ALL", 8 }, (SYMS_U64)SYMS_MachCpuSubtypeIntel_I386_ALL },
{ { (SYMS_U8*)"I386", 4 }, (SYMS_U64)SYMS_MachCpuSubtypeIntel_I386 },
{ { (SYMS_U8*)"I486", 4 }, (SYMS_U64)SYMS_MachCpuSubtypeIntel_I486 },
{ { (SYMS_U8*)"I486SX", 6 }, (SYMS_U64)SYMS_MachCpuSubtypeIntel_I486SX },
{ { (SYMS_U8*)"I586", 4 }, (SYMS_U64)SYMS_MachCpuSubtypeIntel_I586 },
{ { (SYMS_U8*)"PENT", 4 }, (SYMS_U64)SYMS_MachCpuSubtypeIntel_PENT },
{ { (SYMS_U8*)"PENTPRO", 7 }, (SYMS_U64)SYMS_MachCpuSubtypeIntel_PENTPRO },
{ { (SYMS_U8*)"PENTII_M3", 9 }, (SYMS_U64)SYMS_MachCpuSubtypeIntel_PENTII_M3 },
{ { (SYMS_U8*)"PENTII_M5", 9 }, (SYMS_U64)SYMS_MachCpuSubtypeIntel_PENTII_M5 },
{ { (SYMS_U8*)"CELERON", 7 }, (SYMS_U64)SYMS_MachCpuSubtypeIntel_CELERON },
{ { (SYMS_U8*)"CELERON_MOBILE", 14 }, (SYMS_U64)SYMS_MachCpuSubtypeIntel_CELERON_MOBILE },
{ { (SYMS_U8*)"PENTIUM_3", 9 }, (SYMS_U64)SYMS_MachCpuSubtypeIntel_PENTIUM_3 },
{ { (SYMS_U8*)"PENTIUM_3_M", 11 }, (SYMS_U64)SYMS_MachCpuSubtypeIntel_PENTIUM_3_M },
{ { (SYMS_U8*)"PENTIUM_3_XENON", 15 }, (SYMS_U64)SYMS_MachCpuSubtypeIntel_PENTIUM_3_XENON },
{ { (SYMS_U8*)"PENTIUM_M", 9 }, (SYMS_U64)SYMS_MachCpuSubtypeIntel_PENTIUM_M },
{ { (SYMS_U8*)"PENTIUM_4", 9 }, (SYMS_U64)SYMS_MachCpuSubtypeIntel_PENTIUM_4 },
{ { (SYMS_U8*)"PENTIUM_4_M", 11 }, (SYMS_U64)SYMS_MachCpuSubtypeIntel_PENTIUM_4_M },
{ { (SYMS_U8*)"PENTIUM_ITANIUM", 15 }, (SYMS_U64)SYMS_MachCpuSubtypeIntel_PENTIUM_ITANIUM },
{ { (SYMS_U8*)"PENTIUM_ITANIUM_2", 17 }, (SYMS_U64)SYMS_MachCpuSubtypeIntel_PENTIUM_ITANIUM_2 },
{ { (SYMS_U8*)"XEON", 4 }, (SYMS_U64)SYMS_MachCpuSubtypeIntel_XEON },
{ { (SYMS_U8*)"XEON_MP", 7 }, (SYMS_U64)SYMS_MachCpuSubtypeIntel_XEON_MP },
};
SYMS_SerialValue _syms_serial_members_for_SYMS_MachCpuSubtypeARM[] = {
{ { (SYMS_U8*)"ALL", 3 }, (SYMS_U64)SYMS_MachCpuSubtypeARM_ALL },
{ { (SYMS_U8*)"V4T", 3 }, (SYMS_U64)SYMS_MachCpuSubtypeARM_V4T },
{ { (SYMS_U8*)"V6", 2 }, (SYMS_U64)SYMS_MachCpuSubtypeARM_V6 },
{ { (SYMS_U8*)"V5TEJ", 5 }, (SYMS_U64)SYMS_MachCpuSubtypeARM_V5TEJ },
{ { (SYMS_U8*)"XSCALE", 6 }, (SYMS_U64)SYMS_MachCpuSubtypeARM_XSCALE },
{ { (SYMS_U8*)"V7", 2 }, (SYMS_U64)SYMS_MachCpuSubtypeARM_V7 },
{ { (SYMS_U8*)"V7F", 3 }, (SYMS_U64)SYMS_MachCpuSubtypeARM_V7F },
{ { (SYMS_U8*)"V7S", 3 }, (SYMS_U64)SYMS_MachCpuSubtypeARM_V7S },
{ { (SYMS_U8*)"V7K", 3 }, (SYMS_U64)SYMS_MachCpuSubtypeARM_V7K },
{ { (SYMS_U8*)"V6M", 3 }, (SYMS_U64)SYMS_MachCpuSubtypeARM_V6M },
{ { (SYMS_U8*)"V7M", 3 }, (SYMS_U64)SYMS_MachCpuSubtypeARM_V7M },
{ { (SYMS_U8*)"V7EM", 4 }, (SYMS_U64)SYMS_MachCpuSubtypeARM_V7EM },
{ { (SYMS_U8*)"V8", 2 }, (SYMS_U64)SYMS_MachCpuSubtypeARM_V8 },
};
SYMS_SerialValue _syms_serial_members_for_SYMS_MachCpuSubtypeARM64[] = {
{ { (SYMS_U8*)"ALL", 3 }, (SYMS_U64)SYMS_MachCpuSubtypeARM64_ALL },
{ { (SYMS_U8*)"V8", 2 }, (SYMS_U64)SYMS_MachCpuSubtypeARM64_V8 },
};
SYMS_SerialValue _syms_serial_members_for_SYMS_MachFiletype[] = {
{ { (SYMS_U8*)"OBJECT", 6 }, (SYMS_U64)SYMS_MachFiletype_OBJECT },
{ { (SYMS_U8*)"EXECUTE", 7 }, (SYMS_U64)SYMS_MachFiletype_EXECUTE },
{ { (SYMS_U8*)"FVMLIB", 6 }, (SYMS_U64)SYMS_MachFiletype_FVMLIB },
{ { (SYMS_U8*)"CORE", 4 }, (SYMS_U64)SYMS_MachFiletype_CORE },
{ { (SYMS_U8*)"PRELOAD", 7 }, (SYMS_U64)SYMS_MachFiletype_PRELOAD },
{ { (SYMS_U8*)"DYLIB", 5 }, (SYMS_U64)SYMS_MachFiletype_DYLIB },
{ { (SYMS_U8*)"DYLINKER", 8 }, (SYMS_U64)SYMS_MachFiletype_DYLINKER },
{ { (SYMS_U8*)"BUNDLE", 6 }, (SYMS_U64)SYMS_MachFiletype_BUNDLE },
{ { (SYMS_U8*)"DYLIB_STUB", 10 }, (SYMS_U64)SYMS_MachFiletype_DYLIB_STUB },
{ { (SYMS_U8*)"DSYM", 4 }, (SYMS_U64)SYMS_MachFiletype_DSYM },
{ { (SYMS_U8*)"KEXT_BUNDLE", 11 }, (SYMS_U64)SYMS_MachFiletype_KEXT_BUNDLE },
};
SYMS_SerialValue _syms_serial_members_for_SYMS_MachFlags[] = {
{ { (SYMS_U8*)"NOUNDEFS", 8 }, (SYMS_U64)SYMS_MachFlags_NOUNDEFS },
{ { (SYMS_U8*)"INCRLINK", 8 }, (SYMS_U64)SYMS_MachFlags_INCRLINK },
{ { (SYMS_U8*)"DYLDLINK", 8 }, (SYMS_U64)SYMS_MachFlags_DYLDLINK },
{ { (SYMS_U8*)"BINDATLOAD", 10 }, (SYMS_U64)SYMS_MachFlags_BINDATLOAD },
{ { (SYMS_U8*)"PREBOUND", 8 }, (SYMS_U64)SYMS_MachFlags_PREBOUND },
{ { (SYMS_U8*)"SPLIT_SEGS", 10 }, (SYMS_U64)SYMS_MachFlags_SPLIT_SEGS },
{ { (SYMS_U8*)"LAZY_INIT", 9 }, (SYMS_U64)SYMS_MachFlags_LAZY_INIT },
{ { (SYMS_U8*)"TWOLEVEL", 8 }, (SYMS_U64)SYMS_MachFlags_TWOLEVEL },
{ { (SYMS_U8*)"FORCE_FLAT", 10 }, (SYMS_U64)SYMS_MachFlags_FORCE_FLAT },
{ { (SYMS_U8*)"NOMULTIDEFS", 11 }, (SYMS_U64)SYMS_MachFlags_NOMULTIDEFS },
{ { (SYMS_U8*)"NOFIXPREBOUNDING", 16 }, (SYMS_U64)SYMS_MachFlags_NOFIXPREBOUNDING },
{ { (SYMS_U8*)"PREBINDABLE", 11 }, (SYMS_U64)SYMS_MachFlags_PREBINDABLE },
{ { (SYMS_U8*)"ALLMODSBOUND", 12 }, (SYMS_U64)SYMS_MachFlags_ALLMODSBOUND },
{ { (SYMS_U8*)"SUBSECTIONS_VIA_SYMBOLS", 23 }, (SYMS_U64)SYMS_MachFlags_SUBSECTIONS_VIA_SYMBOLS },
{ { (SYMS_U8*)"CANONICAL", 9 }, (SYMS_U64)SYMS_MachFlags_CANONICAL },
{ { (SYMS_U8*)"WEAK_DEFINES", 12 }, (SYMS_U64)SYMS_MachFlags_WEAK_DEFINES },
{ { (SYMS_U8*)"BINDS_TO_WEAK", 13 }, (SYMS_U64)SYMS_MachFlags_BINDS_TO_WEAK },
{ { (SYMS_U8*)"ALLOW_STACK_EXECUTION", 21 }, (SYMS_U64)SYMS_MachFlags_ALLOW_STACK_EXECUTION },
{ { (SYMS_U8*)"ROOT_SAFE", 9 }, (SYMS_U64)SYMS_MachFlags_ROOT_SAFE },
{ { (SYMS_U8*)"SETUID_SAFE", 11 }, (SYMS_U64)SYMS_MachFlags_SETUID_SAFE },
{ { (SYMS_U8*)"NO_REEXPORTED_DYLIBS", 20 }, (SYMS_U64)SYMS_MachFlags_NO_REEXPORTED_DYLIBS },
{ { (SYMS_U8*)"PIE", 3 }, (SYMS_U64)SYMS_MachFlags_PIE },
{ { (SYMS_U8*)"STRIPPABLE_DYLIB", 16 }, (SYMS_U64)SYMS_MachFlags_STRIPPABLE_DYLIB },
{ { (SYMS_U8*)"HAS_TLV_DESRIPTORS", 18 }, (SYMS_U64)SYMS_MachFlags_HAS_TLV_DESRIPTORS },
{ { (SYMS_U8*)"NO_HEAP_EXECUTION", 17 }, (SYMS_U64)SYMS_MachFlags_NO_HEAP_EXECUTION },
};
SYMS_SerialValue _syms_serial_members_for_SYMS_MachLoadCommandType[] = {
{ { (SYMS_U8*)"SEGMENT", 7 }, (SYMS_U64)SYMS_MachLoadCommandType_SEGMENT },
{ { (SYMS_U8*)"SYMTAB", 6 }, (SYMS_U64)SYMS_MachLoadCommandType_SYMTAB },
{ { (SYMS_U8*)"SYMSEG", 6 }, (SYMS_U64)SYMS_MachLoadCommandType_SYMSEG },
{ { (SYMS_U8*)"THREAD", 6 }, (SYMS_U64)SYMS_MachLoadCommandType_THREAD },
{ { (SYMS_U8*)"UNIXTHREAD", 10 }, (SYMS_U64)SYMS_MachLoadCommandType_UNIXTHREAD },
{ { (SYMS_U8*)"LOADFVMLIB", 10 }, (SYMS_U64)SYMS_MachLoadCommandType_LOADFVMLIB },
{ { (SYMS_U8*)"IDFVMLIB", 8 }, (SYMS_U64)SYMS_MachLoadCommandType_IDFVMLIB },
{ { (SYMS_U8*)"IDENT", 5 }, (SYMS_U64)SYMS_MachLoadCommandType_IDENT },
{ { (SYMS_U8*)"FVMFILE", 7 }, (SYMS_U64)SYMS_MachLoadCommandType_FVMFILE },
{ { (SYMS_U8*)"PREPAGE", 7 }, (SYMS_U64)SYMS_MachLoadCommandType_PREPAGE },
{ { (SYMS_U8*)"DYSYMTAB", 8 }, (SYMS_U64)SYMS_MachLoadCommandType_DYSYMTAB },
{ { (SYMS_U8*)"LOAD_DYLIB", 10 }, (SYMS_U64)SYMS_MachLoadCommandType_LOAD_DYLIB },
{ { (SYMS_U8*)"ID_DYLIB", 8 }, (SYMS_U64)SYMS_MachLoadCommandType_ID_DYLIB },
{ { (SYMS_U8*)"LOAD_DYLINKER", 13 }, (SYMS_U64)SYMS_MachLoadCommandType_LOAD_DYLINKER },
{ { (SYMS_U8*)"ID_DYLINKER", 11 }, (SYMS_U64)SYMS_MachLoadCommandType_ID_DYLINKER },
{ { (SYMS_U8*)"PREBOUND_DYLIB", 14 }, (SYMS_U64)SYMS_MachLoadCommandType_PREBOUND_DYLIB },
{ { (SYMS_U8*)"ROUTINES", 8 }, (SYMS_U64)SYMS_MachLoadCommandType_ROUTINES },
{ { (SYMS_U8*)"SUB_FRAMEWORK", 13 }, (SYMS_U64)SYMS_MachLoadCommandType_SUB_FRAMEWORK },
{ { (SYMS_U8*)"SUB_UMBRELLA", 12 }, (SYMS_U64)SYMS_MachLoadCommandType_SUB_UMBRELLA },
{ { (SYMS_U8*)"SUB_CLIENT", 10 }, (SYMS_U64)SYMS_MachLoadCommandType_SUB_CLIENT },
{ { (SYMS_U8*)"SUB_LIBRARY", 11 }, (SYMS_U64)SYMS_MachLoadCommandType_SUB_LIBRARY },
{ { (SYMS_U8*)"TWOLEVEL_HINTS", 14 }, (SYMS_U64)SYMS_MachLoadCommandType_TWOLEVEL_HINTS },
{ { (SYMS_U8*)"PREBIND_CHKSUM", 14 }, (SYMS_U64)SYMS_MachLoadCommandType_PREBIND_CHKSUM },
{ { (SYMS_U8*)"LOAD_WEAK_DYLIB", 15 }, (SYMS_U64)SYMS_MachLoadCommandType_LOAD_WEAK_DYLIB },
{ { (SYMS_U8*)"SEGMENT_64", 10 }, (SYMS_U64)SYMS_MachLoadCommandType_SEGMENT_64 },
{ { (SYMS_U8*)"ROUTINES_64", 11 }, (SYMS_U64)SYMS_MachLoadCommandType_ROUTINES_64 },
{ { (SYMS_U8*)"UUID", 4 }, (SYMS_U64)SYMS_MachLoadCommandType_UUID },
{ { (SYMS_U8*)"RPATH", 5 }, (SYMS_U64)SYMS_MachLoadCommandType_RPATH },
{ { (SYMS_U8*)"CODE_SIGNATURE", 14 }, (SYMS_U64)SYMS_MachLoadCommandType_CODE_SIGNATURE },
{ { (SYMS_U8*)"SEGMENT_SPLIT_INFO", 18 }, (SYMS_U64)SYMS_MachLoadCommandType_SEGMENT_SPLIT_INFO },
{ { (SYMS_U8*)"REEXPORT_DYLIB", 14 }, (SYMS_U64)SYMS_MachLoadCommandType_REEXPORT_DYLIB },
{ { (SYMS_U8*)"LAZY_LOAD_DYLIB", 15 }, (SYMS_U64)SYMS_MachLoadCommandType_LAZY_LOAD_DYLIB },
{ { (SYMS_U8*)"ENCRYPTION_INFO", 15 }, (SYMS_U64)SYMS_MachLoadCommandType_ENCRYPTION_INFO },
{ { (SYMS_U8*)"DYLD_INFO", 9 }, (SYMS_U64)SYMS_MachLoadCommandType_DYLD_INFO },
{ { (SYMS_U8*)"DYLD_INFO_ONLY", 14 }, (SYMS_U64)SYMS_MachLoadCommandType_DYLD_INFO_ONLY },
{ { (SYMS_U8*)"LOAD_UPWARD_DYLIB", 17 }, (SYMS_U64)SYMS_MachLoadCommandType_LOAD_UPWARD_DYLIB },
{ { (SYMS_U8*)"VERSION_MIN_MACOSX", 18 }, (SYMS_U64)SYMS_MachLoadCommandType_VERSION_MIN_MACOSX },
{ { (SYMS_U8*)"VERSION_MIN_IPHONES", 19 }, (SYMS_U64)SYMS_MachLoadCommandType_VERSION_MIN_IPHONES },
{ { (SYMS_U8*)"FUNCTION_STARTS", 15 }, (SYMS_U64)SYMS_MachLoadCommandType_FUNCTION_STARTS },
{ { (SYMS_U8*)"DYLD_ENVIORNMENT", 16 }, (SYMS_U64)SYMS_MachLoadCommandType_DYLD_ENVIORNMENT },
{ { (SYMS_U8*)"MAIN", 4 }, (SYMS_U64)SYMS_MachLoadCommandType_MAIN },
{ { (SYMS_U8*)"DATA_IN_CODE", 12 }, (SYMS_U64)SYMS_MachLoadCommandType_DATA_IN_CODE },
{ { (SYMS_U8*)"SOURCE_VERSION", 14 }, (SYMS_U64)SYMS_MachLoadCommandType_SOURCE_VERSION },
{ { (SYMS_U8*)"DYLIB_CODE_SIGN_DRS", 19 }, (SYMS_U64)SYMS_MachLoadCommandType_DYLIB_CODE_SIGN_DRS },
{ { (SYMS_U8*)"ENCRYPTION_INFO_64", 18 }, (SYMS_U64)SYMS_MachLoadCommandType_ENCRYPTION_INFO_64 },
{ { (SYMS_U8*)"LINKER_OPTION", 13 }, (SYMS_U64)SYMS_MachLoadCommandType_LINKER_OPTION },
{ { (SYMS_U8*)"LINKER_OPTIMIZATION_HINT", 24 }, (SYMS_U64)SYMS_MachLoadCommandType_LINKER_OPTIMIZATION_HINT },
{ { (SYMS_U8*)"VERSION_MIN_TVOS", 16 }, (SYMS_U64)SYMS_MachLoadCommandType_VERSION_MIN_TVOS },
{ { (SYMS_U8*)"VERSION_MIN_WATCHOS", 19 }, (SYMS_U64)SYMS_MachLoadCommandType_VERSION_MIN_WATCHOS },
{ { (SYMS_U8*)"NOTE", 4 }, (SYMS_U64)SYMS_MachLoadCommandType_NOTE },
{ { (SYMS_U8*)"BUILD_VERSION", 13 }, (SYMS_U64)SYMS_MachLoadCommandType_BUILD_VERSION },
};
SYMS_SerialValue _syms_serial_members_for_SYMS_MachSectionType[] = {
{ { (SYMS_U8*)"REGULAR", 7 }, (SYMS_U64)SYMS_MachSectionType_REGULAR },
{ { (SYMS_U8*)"ZEROFILL", 8 }, (SYMS_U64)SYMS_MachSectionType_ZEROFILL },
{ { (SYMS_U8*)"CSTRING_LITERAL", 15 }, (SYMS_U64)SYMS_MachSectionType_CSTRING_LITERAL },
{ { (SYMS_U8*)"FOUR_BYTE_LITERALS", 18 }, (SYMS_U64)SYMS_MachSectionType_FOUR_BYTE_LITERALS },
{ { (SYMS_U8*)"EIGHT_BYTE_LITERALS", 19 }, (SYMS_U64)SYMS_MachSectionType_EIGHT_BYTE_LITERALS },
{ { (SYMS_U8*)"LITERAL_POINTERS", 16 }, (SYMS_U64)SYMS_MachSectionType_LITERAL_POINTERS },
{ { (SYMS_U8*)"NON_LAZY_SYMBOL_POINTERS", 24 }, (SYMS_U64)SYMS_MachSectionType_NON_LAZY_SYMBOL_POINTERS },
{ { (SYMS_U8*)"LAZY_SYMBOL_POINTERS", 20 }, (SYMS_U64)SYMS_MachSectionType_LAZY_SYMBOL_POINTERS },
{ { (SYMS_U8*)"SYMBOL_STUBS", 12 }, (SYMS_U64)SYMS_MachSectionType_SYMBOL_STUBS },
{ { (SYMS_U8*)"MOD_INIT_FUNC_POINTERS", 22 }, (SYMS_U64)SYMS_MachSectionType_MOD_INIT_FUNC_POINTERS },
{ { (SYMS_U8*)"MOD_TERM_FUNC_POINTERS", 22 }, (SYMS_U64)SYMS_MachSectionType_MOD_TERM_FUNC_POINTERS },
{ { (SYMS_U8*)"COALESCED", 9 }, (SYMS_U64)SYMS_MachSectionType_COALESCED },
{ { (SYMS_U8*)"GB_ZERO_FILL", 12 }, (SYMS_U64)SYMS_MachSectionType_GB_ZERO_FILL },
{ { (SYMS_U8*)"INTERPOSING", 11 }, (SYMS_U64)SYMS_MachSectionType_INTERPOSING },
{ { (SYMS_U8*)"SIXTEENBYTE_LITERALS", 20 }, (SYMS_U64)SYMS_MachSectionType_SIXTEENBYTE_LITERALS },
{ { (SYMS_U8*)"DTRACE_DOF", 10 }, (SYMS_U64)SYMS_MachSectionType_DTRACE_DOF },
{ { (SYMS_U8*)"LAZY_DLIB_SYMBOL_POINTERS", 25 }, (SYMS_U64)SYMS_MachSectionType_LAZY_DLIB_SYMBOL_POINTERS },
{ { (SYMS_U8*)"THREAD_LOCAL_REGULAR", 20 }, (SYMS_U64)SYMS_MachSectionType_THREAD_LOCAL_REGULAR },
{ { (SYMS_U8*)"THREAD_LOCAL_ZEROFILL", 21 }, (SYMS_U64)SYMS_MachSectionType_THREAD_LOCAL_ZEROFILL },
{ { (SYMS_U8*)"THREAD_LOCAL_VARIABLES", 22 }, (SYMS_U64)SYMS_MachSectionType_THREAD_LOCAL_VARIABLES },
{ { (SYMS_U8*)"THREAD_LOCAL_VARIABLES_POINTERS", 31 }, (SYMS_U64)SYMS_MachSectionType_THREAD_LOCAL_VARIABLES_POINTERS },
{ { (SYMS_U8*)"LOCAL_INIT_FUNCTION_POINTERS", 28 }, (SYMS_U64)SYMS_MachSectionType_LOCAL_INIT_FUNCTION_POINTERS },
};
SYMS_SerialValue _syms_serial_members_for_SYMS_MachSectionAttr[] = {
{ { (SYMS_U8*)"USR", 3 }, (SYMS_U64)SYMS_MachSectionAttr_USR },
{ { (SYMS_U8*)"SYSTEM", 6 }, (SYMS_U64)SYMS_MachSectionAttr_SYSTEM },
{ { (SYMS_U8*)"PURE_INSTRUCTIONS", 17 }, (SYMS_U64)SYMS_MachSectionAttr_PURE_INSTRUCTIONS },
{ { (SYMS_U8*)"NO_TOC", 6 }, (SYMS_U64)SYMS_MachSectionAttr_NO_TOC },
{ { (SYMS_U8*)"STRIP_STATIC_SYMS", 17 }, (SYMS_U64)SYMS_MachSectionAttr_STRIP_STATIC_SYMS },
{ { (SYMS_U8*)"NO_DEAD_STRIP", 13 }, (SYMS_U64)SYMS_MachSectionAttr_NO_DEAD_STRIP },
{ { (SYMS_U8*)"LIVE_SUPPORT", 12 }, (SYMS_U64)SYMS_MachSectionAttr_LIVE_SUPPORT },
{ { (SYMS_U8*)"SELF_MODIFYING_CODE", 19 }, (SYMS_U64)SYMS_MachSectionAttr_SELF_MODIFYING_CODE },
{ { (SYMS_U8*)"DEBUG", 5 }, (SYMS_U64)SYMS_MachSectionAttr_DEBUG },
{ { (SYMS_U8*)"SOME_INSTRUCTIONS", 17 }, (SYMS_U64)SYMS_MachSectionAttr_SOME_INSTRUCTIONS },
{ { (SYMS_U8*)"SECTION_RELOC", 13 }, (SYMS_U64)SYMS_MachSectionAttr_SECTION_RELOC },
{ { (SYMS_U8*)"LOC_RELOC", 9 }, (SYMS_U64)SYMS_MachSectionAttr_LOC_RELOC },
};
SYMS_SerialValue _syms_serial_members_for_SYMS_MachPlatformType[] = {
{ { (SYMS_U8*)"MACOS", 5 }, (SYMS_U64)SYMS_MachPlatformType_MACOS },
{ { (SYMS_U8*)"IOS", 3 }, (SYMS_U64)SYMS_MachPlatformType_IOS },
{ { (SYMS_U8*)"TVOS", 4 }, (SYMS_U64)SYMS_MachPlatformType_TVOS },
{ { (SYMS_U8*)"WATCHOS", 7 }, (SYMS_U64)SYMS_MachPlatformType_WATCHOS },
};
SYMS_SerialValue _syms_serial_members_for_SYMS_MachToolType[] = {
{ { (SYMS_U8*)"CLANG", 5 }, (SYMS_U64)SYMS_MachToolType_CLANG },
{ { (SYMS_U8*)"SWITFT", 6 }, (SYMS_U64)SYMS_MachToolType_SWITFT },
{ { (SYMS_U8*)"LD", 2 }, (SYMS_U64)SYMS_MachToolType_LD },
};
SYMS_SerialValue _syms_serial_members_for_SYMS_MachBindType[] = {
{ { (SYMS_U8*)"POINTER", 7 }, (SYMS_U64)SYMS_MachBindType_POINTER },
{ { (SYMS_U8*)"TEXT_ABSOLUTE32", 15 }, (SYMS_U64)SYMS_MachBindType_TEXT_ABSOLUTE32 },
{ { (SYMS_U8*)"PCREL32", 7 }, (SYMS_U64)SYMS_MachBindType_PCREL32 },
};
SYMS_SerialValue _syms_serial_members_for_SYMS_MachBindOpcode[] = {
{ { (SYMS_U8*)"DONE", 4 }, (SYMS_U64)SYMS_MachBindOpcode_DONE },
{ { (SYMS_U8*)"SET_DYLIB_ORDINAL_IMM", 21 }, (SYMS_U64)SYMS_MachBindOpcode_SET_DYLIB_ORDINAL_IMM },
{ { (SYMS_U8*)"SET_DYLIB_ORDINAL_ULEB", 22 }, (SYMS_U64)SYMS_MachBindOpcode_SET_DYLIB_ORDINAL_ULEB },
{ { (SYMS_U8*)"SET_DYLIB_SPECIAL_IMM", 21 }, (SYMS_U64)SYMS_MachBindOpcode_SET_DYLIB_SPECIAL_IMM },
{ { (SYMS_U8*)"SET_SYMBOL_TRAILING_FLAGS_IMM", 29 }, (SYMS_U64)SYMS_MachBindOpcode_SET_SYMBOL_TRAILING_FLAGS_IMM },
{ { (SYMS_U8*)"SET_TYPE_IMM", 12 }, (SYMS_U64)SYMS_MachBindOpcode_SET_TYPE_IMM },
{ { (SYMS_U8*)"SET_ADDEND_SLEB", 15 }, (SYMS_U64)SYMS_MachBindOpcode_SET_ADDEND_SLEB },
{ { (SYMS_U8*)"SET_SEGMENT_AND_OFFSET_ULEB", 27 }, (SYMS_U64)SYMS_MachBindOpcode_SET_SEGMENT_AND_OFFSET_ULEB },
{ { (SYMS_U8*)"ADD_ADDR_ULEB", 13 }, (SYMS_U64)SYMS_MachBindOpcode_ADD_ADDR_ULEB },
{ { (SYMS_U8*)"DO_BIND", 7 }, (SYMS_U64)SYMS_MachBindOpcode_DO_BIND },
{ { (SYMS_U8*)"DO_BIND_ADD_ADDR_ULEB", 21 }, (SYMS_U64)SYMS_MachBindOpcode_DO_BIND_ADD_ADDR_ULEB },
{ { (SYMS_U8*)"DO_BIND_ADD_ADDR_IMM_SCALED", 27 }, (SYMS_U64)SYMS_MachBindOpcode_DO_BIND_ADD_ADDR_IMM_SCALED },
{ { (SYMS_U8*)"DO_BIND_ULEB_TIMES_SKIPPING_ULEB", 32 }, (SYMS_U64)SYMS_MachBindOpcode_DO_BIND_ULEB_TIMES_SKIPPING_ULEB },
{ { (SYMS_U8*)"MASK", 4 }, (SYMS_U64)SYMS_MachBindOpcode_MASK },
{ { (SYMS_U8*)"IMM_MASK", 8 }, (SYMS_U64)SYMS_MachBindOpcode_IMM_MASK },
};
SYMS_SerialFlag _syms_serial_members_for_SYMS_MachBindSymbolFlags[] = {
{ { (SYMS_U8*)"WEAK_IMPORT", 11 }, &_syms_serial_type_SYMS_U32, 0x1, 0 },
{ { (SYMS_U8*)"NON_WEAK_DEFINITION", 19 }, &_syms_serial_type_SYMS_U32, 0x1, 3 },
};
SYMS_SerialValue _syms_serial_members_for_SYMS_MachBindSpecialDylib[] = {
{ { (SYMS_U8*)"SELF", 4 }, (SYMS_U64)SYMS_MachBindSpecialDylib_SELF },
{ { (SYMS_U8*)"MAIN_EXECUTABLE", 15 }, (SYMS_U64)SYMS_MachBindSpecialDylib_MAIN_EXECUTABLE },
{ { (SYMS_U8*)"FLAT_LOOKUP", 11 }, (SYMS_U64)SYMS_MachBindSpecialDylib_FLAT_LOOKUP },
};
SYMS_SerialValue _syms_serial_members_for_SYMS_MachNListType[] = {
{ { (SYMS_U8*)"UNDF", 4 }, (SYMS_U64)SYMS_MachNListType_UNDF },
{ { (SYMS_U8*)"ABS", 3 }, (SYMS_U64)SYMS_MachNListType_ABS },
{ { (SYMS_U8*)"SECT", 4 }, (SYMS_U64)SYMS_MachNListType_SECT },
{ { (SYMS_U8*)"PBUD", 4 }, (SYMS_U64)SYMS_MachNListType_PBUD },
{ { (SYMS_U8*)"INDR", 4 }, (SYMS_U64)SYMS_MachNListType_INDR },
};
SYMS_SerialValue _syms_serial_members_for_SYMS_MachStabType[] = {
{ { (SYMS_U8*)"GSYM", 4 }, (SYMS_U64)SYMS_MachStabType_GSYM },
{ { (SYMS_U8*)"FNAME", 5 }, (SYMS_U64)SYMS_MachStabType_FNAME },
{ { (SYMS_U8*)"FUN", 3 }, (SYMS_U64)SYMS_MachStabType_FUN },
{ { (SYMS_U8*)"STSYM", 5 }, (SYMS_U64)SYMS_MachStabType_STSYM },
{ { (SYMS_U8*)"LCSYM", 5 }, (SYMS_U64)SYMS_MachStabType_LCSYM },
{ { (SYMS_U8*)"BNSYM", 5 }, (SYMS_U64)SYMS_MachStabType_BNSYM },
{ { (SYMS_U8*)"AST", 3 }, (SYMS_U64)SYMS_MachStabType_AST },
{ { (SYMS_U8*)"OPT", 3 }, (SYMS_U64)SYMS_MachStabType_OPT },
{ { (SYMS_U8*)"RSYM", 4 }, (SYMS_U64)SYMS_MachStabType_RSYM },
{ { (SYMS_U8*)"SLINE", 5 }, (SYMS_U64)SYMS_MachStabType_SLINE },
{ { (SYMS_U8*)"ENSYM", 5 }, (SYMS_U64)SYMS_MachStabType_ENSYM },
{ { (SYMS_U8*)"SSYM", 4 }, (SYMS_U64)SYMS_MachStabType_SSYM },
{ { (SYMS_U8*)"SO", 2 }, (SYMS_U64)SYMS_MachStabType_SO },
{ { (SYMS_U8*)"OSO", 3 }, (SYMS_U64)SYMS_MachStabType_OSO },
{ { (SYMS_U8*)"LSYM", 4 }, (SYMS_U64)SYMS_MachStabType_LSYM },
{ { (SYMS_U8*)"BINCL", 5 }, (SYMS_U64)SYMS_MachStabType_BINCL },
{ { (SYMS_U8*)"SOL", 3 }, (SYMS_U64)SYMS_MachStabType_SOL },
{ { (SYMS_U8*)"PARAMS", 6 }, (SYMS_U64)SYMS_MachStabType_PARAMS },
{ { (SYMS_U8*)"VERSION", 7 }, (SYMS_U64)SYMS_MachStabType_VERSION },
{ { (SYMS_U8*)"OLEVEL", 6 }, (SYMS_U64)SYMS_MachStabType_OLEVEL },
{ { (SYMS_U8*)"PSYM", 4 }, (SYMS_U64)SYMS_MachStabType_PSYM },
{ { (SYMS_U8*)"EINCL", 5 }, (SYMS_U64)SYMS_MachStabType_EINCL },
{ { (SYMS_U8*)"ENTRY", 5 }, (SYMS_U64)SYMS_MachStabType_ENTRY },
{ { (SYMS_U8*)"LBRAC", 5 }, (SYMS_U64)SYMS_MachStabType_LBRAC },
{ { (SYMS_U8*)"EXCL", 4 }, (SYMS_U64)SYMS_MachStabType_EXCL },
{ { (SYMS_U8*)"RBRAC", 5 }, (SYMS_U64)SYMS_MachStabType_RBRAC },
{ { (SYMS_U8*)"BCOMM", 5 }, (SYMS_U64)SYMS_MachStabType_BCOMM },
{ { (SYMS_U8*)"ECOMM", 5 }, (SYMS_U64)SYMS_MachStabType_ECOMM },
{ { (SYMS_U8*)"ECOML", 5 }, (SYMS_U64)SYMS_MachStabType_ECOML },
{ { (SYMS_U8*)"LENG", 4 }, (SYMS_U64)SYMS_MachStabType_LENG },
};
SYMS_SerialValue _syms_serial_members_for_SYMS_MachExportSymbolKind[] = {
{ { (SYMS_U8*)"REGULAR", 7 }, (SYMS_U64)SYMS_MachExportSymbolKind_REGULAR },
{ { (SYMS_U8*)"THREAD_LOCAL", 12 }, (SYMS_U64)SYMS_MachExportSymbolKind_THREAD_LOCAL },
{ { (SYMS_U8*)"ABSOLUTE", 8 }, (SYMS_U64)SYMS_MachExportSymbolKind_ABSOLUTE },
};
SYMS_SerialFlag _syms_serial_members_for_SYMS_MachExportSymbolFlags[] = {
{ { (SYMS_U8*)"KIND_MASK", 9 }, &_syms_serial_type_SYMS_MachExportSymbolKind, 0x1, 2 },
{ { (SYMS_U8*)"WEAK_DEFINITION", 15 }, &_syms_serial_type_SYMS_U64, 0x1, 2 },
{ { (SYMS_U8*)"REEXPORT", 8 }, &_syms_serial_type_SYMS_U64, 0x1, 3 },
{ { (SYMS_U8*)"STUB_AND_RESOLVED", 17 }, &_syms_serial_type_SYMS_U64, 0x1, 4 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_MachLCStr[] = {
{ {(SYMS_U8*)"offset", 6}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_MachUUID[] = {
{ {(SYMS_U8*)"cmd", 3}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"cmdsize", 7}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"uuid", 4}, &_syms_serial_type_SYMS_U8, SYMS_SerialWidthKind_Fixed, 16 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_MachDylib[] = {
{ {(SYMS_U8*)"name", 4}, &_syms_serial_type_SYMS_MachLCStr, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"timestamp", 9}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"current_version", 15}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"compatability_version", 21}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_MachDylibCommand[] = {
{ {(SYMS_U8*)"cmd", 3}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"cmdsize", 7}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"dylib", 5}, &_syms_serial_type_SYMS_MachDylib, SYMS_SerialWidthKind_Null, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_MachDyldInfoCommand[] = {
{ {(SYMS_U8*)"cmd", 3}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"cmdsize", 7}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"rebase_off", 10}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"rebase_size", 11}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"bind_off", 8}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"bind_size", 9}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"weak_bind_off", 13}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"weak_bind_size", 14}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"lazy_bind_off", 13}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"lazy_bind_size", 14}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"export_off", 10}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"export_size", 11}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_MachLoadCommand[] = {
{ {(SYMS_U8*)"type", 4}, &_syms_serial_type_SYMS_MachLoadCommandType, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"size", 4}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_MachFatHeader[] = {
{ {(SYMS_U8*)"magic", 5}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"nfat_arch", 9}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_MachFatArch[] = {
{ {(SYMS_U8*)"cputype", 7}, &_syms_serial_type_SYMS_MachCpuType, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"cpusubtype", 10}, &_syms_serial_type_SYMS_MachCpuSubtype, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"offset", 6}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"size", 4}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"align", 5}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_MachHeader32[] = {
{ {(SYMS_U8*)"magic", 5}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"cputype", 7}, &_syms_serial_type_SYMS_MachCpuType, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"cpusubtype", 10}, &_syms_serial_type_SYMS_MachCpuSubtype, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"filetype", 8}, &_syms_serial_type_SYMS_MachFiletype, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"ncmds", 5}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"sizeofcmds", 10}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"flags", 5}, &_syms_serial_type_SYMS_MachFlags, SYMS_SerialWidthKind_Null, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_MachHeader64[] = {
{ {(SYMS_U8*)"magic", 5}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"cputype", 7}, &_syms_serial_type_SYMS_MachCpuType, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"cpusubtype", 10}, &_syms_serial_type_SYMS_MachCpuSubtype, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"filetype", 8}, &_syms_serial_type_SYMS_MachFiletype, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"ncmds", 5}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"sizeofcmds", 10}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"flags", 5}, &_syms_serial_type_SYMS_MachFlags, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"reserved", 8}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_MachSegmentCommand32[] = {
{ {(SYMS_U8*)"cmd", 3}, &_syms_serial_type_SYMS_MachLoadCommand, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"segname", 7}, &_syms_serial_type_SYMS_U8, SYMS_SerialWidthKind_Fixed, 16 },
{ {(SYMS_U8*)"vmaddr", 6}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"vmsize", 6}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"fileoff", 7}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"filesize", 8}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"maxprot", 7}, &_syms_serial_type_SYMS_MachVMProt, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"initprot", 8}, &_syms_serial_type_SYMS_MachVMProt, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"nsects", 6}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"flags", 5}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_MachSegmentCommand64[] = {
{ {(SYMS_U8*)"cmd", 3}, &_syms_serial_type_SYMS_MachLoadCommand, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"segname", 7}, &_syms_serial_type_SYMS_U8, SYMS_SerialWidthKind_Fixed, 16 },
{ {(SYMS_U8*)"vmaddr", 6}, &_syms_serial_type_SYMS_U64, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"vmsize", 6}, &_syms_serial_type_SYMS_U64, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"fileoff", 7}, &_syms_serial_type_SYMS_U64, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"filesize", 8}, &_syms_serial_type_SYMS_U64, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"maxprot", 7}, &_syms_serial_type_SYMS_MachVMProt, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"initprot", 8}, &_syms_serial_type_SYMS_MachVMProt, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"nsects", 6}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"flags", 5}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_MachSection32[] = {
{ {(SYMS_U8*)"sectname", 8}, &_syms_serial_type_SYMS_U8, SYMS_SerialWidthKind_Fixed, 16 },
{ {(SYMS_U8*)"segname", 7}, &_syms_serial_type_SYMS_U8, SYMS_SerialWidthKind_Fixed, 16 },
{ {(SYMS_U8*)"addr", 4}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"size", 4}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"offset", 6}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"align", 5}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"relocoff", 8}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"nreloc", 6}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"flags", 5}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"reserved1", 9}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"reserved2", 9}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_MachSection64[] = {
{ {(SYMS_U8*)"sectname", 8}, &_syms_serial_type_SYMS_U8, SYMS_SerialWidthKind_Fixed, 16 },
{ {(SYMS_U8*)"segname", 7}, &_syms_serial_type_SYMS_U8, SYMS_SerialWidthKind_Fixed, 16 },
{ {(SYMS_U8*)"addr", 4}, &_syms_serial_type_SYMS_U64, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"size", 4}, &_syms_serial_type_SYMS_U64, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"offset", 6}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"align", 5}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"relocoff", 8}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"nreloc", 6}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"flags", 5}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"reserved1", 9}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"reserved2", 9}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"pad", 3}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_MachSymtabCommand[] = {
{ {(SYMS_U8*)"cmd", 3}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"cmdsize", 7}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"symoff", 6}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"nsyms", 5}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"stroff", 6}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"strsize", 7}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_MachDySymtabCommand[] = {
{ {(SYMS_U8*)"cmd", 3}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"cmdsize", 7}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"ilocalsym", 9}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"nlocalsym", 9}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"iextdefsym", 10}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"nextdefsym", 10}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"iundefsym", 9}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"nundefsym", 9}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"tocoff", 6}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"ntoc", 4}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"modtaboff", 9}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"nmodtab", 7}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"extrefsymoff", 12}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"nextrefsyms", 11}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"indirectsymoff", 14}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"nindirectsyms", 13}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"extreloff", 9}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"nextrel", 7}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"locreloff", 9}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"nlocrel", 7}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_MachNList32[] = {
{ {(SYMS_U8*)"n_strx", 6}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"n_type", 6}, &_syms_serial_type_SYMS_U8, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"n_sect", 6}, &_syms_serial_type_SYMS_U8, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"n_desc", 6}, &_syms_serial_type_SYMS_U16, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"n_value", 7}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_MachNList64[] = {
{ {(SYMS_U8*)"n_strx", 6}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"n_type", 6}, &_syms_serial_type_SYMS_U8, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"n_sect", 6}, &_syms_serial_type_SYMS_U8, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"n_desc", 6}, &_syms_serial_type_SYMS_U16, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"n_value", 7}, &_syms_serial_type_SYMS_U64, SYMS_SerialWidthKind_Null, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_MachBuildVersionCommand[] = {
{ {(SYMS_U8*)"cmd", 3}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"cmdsize", 7}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"platform", 8}, &_syms_serial_type_SYMS_MachPlatformType, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"minos", 5}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"sdk", 3}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"ntools", 6}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_MachBuildToolVersion[] = {
{ {(SYMS_U8*)"tool", 4}, &_syms_serial_type_SYMS_MachToolType, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"version", 7}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_MachVersionMin[] = {
{ {(SYMS_U8*)"cmd", 3}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"cmdsize", 7}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"version", 7}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"sdk", 3}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_MachDylinker[] = {
{ {(SYMS_U8*)"cmd", 3}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"cmdsize", 7}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"name", 4}, &_syms_serial_type_SYMS_MachLCStr, SYMS_SerialWidthKind_Null, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_MachPreboundDylibCommand[] = {
{ {(SYMS_U8*)"cmd", 3}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"cmdsize", 7}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"name", 4}, &_syms_serial_type_SYMS_MachLCStr, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"nmodules", 8}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_MachRoutinesCommand[] = {
{ {(SYMS_U8*)"cmd", 3}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"cmdsize", 7}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"init_address", 12}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"init_module", 11}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"reserved1", 9}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"reserved2", 9}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"reserved3", 9}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"reserved4", 9}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"reserved5", 9}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"reserved6", 9}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_MachRoutines64Command[] = {
{ {(SYMS_U8*)"cmd", 3}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"cmdsize", 7}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"init_address", 12}, &_syms_serial_type_SYMS_U64, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"init_module", 11}, &_syms_serial_type_SYMS_U64, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"reserved1", 9}, &_syms_serial_type_SYMS_U64, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"reserved2", 9}, &_syms_serial_type_SYMS_U64, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"reserved3", 9}, &_syms_serial_type_SYMS_U64, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"reserved4", 9}, &_syms_serial_type_SYMS_U64, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"reserved5", 9}, &_syms_serial_type_SYMS_U64, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"reserved6", 9}, &_syms_serial_type_SYMS_U64, SYMS_SerialWidthKind_Null, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_MachSubFrameworkCommand[] = {
{ {(SYMS_U8*)"cmd", 3}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"cmdsize", 7}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"umbrella", 8}, &_syms_serial_type_SYMS_MachLCStr, SYMS_SerialWidthKind_Null, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_MachSubUmbrellaCommand[] = {
{ {(SYMS_U8*)"cmd", 3}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"cmdsize", 7}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"sub_umbrella", 12}, &_syms_serial_type_SYMS_MachLCStr, SYMS_SerialWidthKind_Null, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_MachSubClientCommand[] = {
{ {(SYMS_U8*)"cmd", 3}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"cmdsize", 7}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"client", 6}, &_syms_serial_type_SYMS_MachLCStr, SYMS_SerialWidthKind_Null, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_MachSubLibraryCommand[] = {
{ {(SYMS_U8*)"cmd", 3}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"cmdsize", 7}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"sub_library", 11}, &_syms_serial_type_SYMS_MachLCStr, SYMS_SerialWidthKind_Null, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_MachTwoLevelHintsCommand[] = {
{ {(SYMS_U8*)"cmd", 3}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"cmdsize", 7}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"offset", 6}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"nhints", 6}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
};
SYMS_SerialFlag _syms_serial_members_for_SYMS_MachTwoLevelHint[] = {
{ { (SYMS_U8*)"isub_image", 10 }, &_syms_serial_type_SYMS_U32, 0xff, 0 },
{ { (SYMS_U8*)"itoc", 4 }, &_syms_serial_type_SYMS_U32, 0xffffff, 8 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_MachPrebindChecksumCommand[] = {
{ {(SYMS_U8*)"cmd", 3}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"cmdsize", 7}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"chksum", 6}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_MachRPathCommand[] = {
{ {(SYMS_U8*)"cmd", 3}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"cmdsize", 7}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"path", 4}, &_syms_serial_type_SYMS_MachLCStr, SYMS_SerialWidthKind_Null, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_MachLinkeditDataCommand[] = {
{ {(SYMS_U8*)"cmd", 3}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"cmdsize", 7}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"dataoff", 7}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"datasize", 8}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_MachEncryptionInfoCommand[] = {
{ {(SYMS_U8*)"cmd", 3}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"cmdsize", 7}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"cryptoff", 8}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"cryptsize", 9}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"cryptid", 7}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_MachEncryptionInfo64Command[] = {
{ {(SYMS_U8*)"cmd", 3}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"cmdsize", 7}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"cryptoff", 8}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"cryptsize", 9}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"cryptid", 7}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"pad", 3}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_MachEntryPointCommand[] = {
{ {(SYMS_U8*)"cmd", 3}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"cmdsize", 7}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"entryoff", 8}, &_syms_serial_type_SYMS_U64, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"stacksize", 9}, &_syms_serial_type_SYMS_U64, SYMS_SerialWidthKind_Null, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_MachSourceVersionCommand[] = {
{ {(SYMS_U8*)"cmd", 3}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"cmdsize", 7}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"version", 7}, &_syms_serial_type_SYMS_U64, SYMS_SerialWidthKind_Null, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_MachLinkerOptionCommand[] = {
{ {(SYMS_U8*)"cmd", 3}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"cmdsize", 7}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"count", 5}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_MachNoteCommand[] = {
{ {(SYMS_U8*)"cmd", 3}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"cmdsize", 7}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"data_owner", 10}, &_syms_serial_type_SYMS_U8, SYMS_SerialWidthKind_Fixed, 16 },
{ {(SYMS_U8*)"offset", 6}, &_syms_serial_type_SYMS_U64, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"size", 4}, &_syms_serial_type_SYMS_U64, SYMS_SerialWidthKind_Null, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_MachSymSegCommand[] = {
{ {(SYMS_U8*)"cmd", 3}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"cmdsize", 7}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"offset", 6}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"size", 4}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_MachFvmlib[] = {
{ {(SYMS_U8*)"name", 4}, &_syms_serial_type_SYMS_MachLCStr, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"minor_version", 13}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"header_addr", 11}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_MachFvmlibCommand[] = {
{ {(SYMS_U8*)"cmd", 3}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"cmdsize", 7}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"fvmlib", 6}, &_syms_serial_type_SYMS_MachFvmlib, SYMS_SerialWidthKind_Null, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_MachThreadCommand[] = {
{ {(SYMS_U8*)"cmd", 3}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"cmdsize", 7}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
};
SYMS_SerialValue _syms_serial_members_for_SYMS_MachUnwindEncModeX86[] = {
{ { (SYMS_U8*)"EBP_FRAME", 9 }, (SYMS_U64)SYMS_MachUnwindEncModeX86_EBP_FRAME },
{ { (SYMS_U8*)"STACK_IMMD", 10 }, (SYMS_U64)SYMS_MachUnwindEncModeX86_STACK_IMMD },
{ { (SYMS_U8*)"STACK_IND", 9 }, (SYMS_U64)SYMS_MachUnwindEncModeX86_STACK_IND },
{ { (SYMS_U8*)"DWARF", 5 }, (SYMS_U64)SYMS_MachUnwindEncModeX86_DWARF },
};
SYMS_SerialValue _syms_serial_members_for_SYMS_MachUnwindRegisterX86[] = {
{ { (SYMS_U8*)"NONE", 4 }, (SYMS_U64)SYMS_MachUnwindRegisterX86_NONE },
{ { (SYMS_U8*)"EBX", 3 }, (SYMS_U64)SYMS_MachUnwindRegisterX86_EBX },
{ { (SYMS_U8*)"ECX", 3 }, (SYMS_U64)SYMS_MachUnwindRegisterX86_ECX },
{ { (SYMS_U8*)"EDX", 3 }, (SYMS_U64)SYMS_MachUnwindRegisterX86_EDX },
{ { (SYMS_U8*)"EDI", 3 }, (SYMS_U64)SYMS_MachUnwindRegisterX86_EDI },
{ { (SYMS_U8*)"ESI", 3 }, (SYMS_U64)SYMS_MachUnwindRegisterX86_ESI },
{ { (SYMS_U8*)"EBP", 3 }, (SYMS_U64)SYMS_MachUnwindRegisterX86_EBP },
};
SYMS_SerialFlag _syms_serial_members_for_SYMS_MachUnwindEncX86[] = {
{ { (SYMS_U8*)"MODE_MASK", 9 }, &_syms_serial_type_SYMS_MachUnwindEncModeX86, 0xffffff, 4 },
{ { (SYMS_U8*)"EBP_FRAME_REGISTER", 18 }, &_syms_serial_type_SYMS_U32, 0x1, 15 },
{ { (SYMS_U8*)"EBP_FRAME_OFFSET", 16 }, &_syms_serial_type_SYMS_U32, 0x7fff, 8 },
{ { (SYMS_U8*)"FRAMELESS_STACK_SIZE", 20 }, &_syms_serial_type_SYMS_U32, 0x7fff, 8 },
{ { (SYMS_U8*)"FRAMELESS_STACK_ADJUST", 22 }, &_syms_serial_type_SYMS_U32, 0xfff, 3 },
{ { (SYMS_U8*)"FRAMELESS_REG_COUNT", 19 }, &_syms_serial_type_SYMS_U32, 0x1ff, 3 },
{ { (SYMS_U8*)"FRAMELESS_REG_PERMUTATION", 25 }, &_syms_serial_type_SYMS_U32, 0x1, 10 },
{ { (SYMS_U8*)"DWARF_SECTION_OFFSET", 20 }, &_syms_serial_type_SYMS_U32, 0x1, 24 },
};
SYMS_SerialValue _syms_serial_members_for_SYMS_MachUnwindEncModeX64[] = {
{ { (SYMS_U8*)"RBP_FRAME", 9 }, (SYMS_U64)SYMS_MachUnwindEncModeX64_RBP_FRAME },
{ { (SYMS_U8*)"STACK_IMMD", 10 }, (SYMS_U64)SYMS_MachUnwindEncModeX64_STACK_IMMD },
{ { (SYMS_U8*)"STACK_IND", 9 }, (SYMS_U64)SYMS_MachUnwindEncModeX64_STACK_IND },
{ { (SYMS_U8*)"DWARF", 5 }, (SYMS_U64)SYMS_MachUnwindEncModeX64_DWARF },
};
SYMS_SerialValue _syms_serial_members_for_SYMS_MachUnwindRegisterX64[] = {
{ { (SYMS_U8*)"NONE", 4 }, (SYMS_U64)SYMS_MachUnwindRegisterX64_NONE },
{ { (SYMS_U8*)"RBX", 3 }, (SYMS_U64)SYMS_MachUnwindRegisterX64_RBX },
{ { (SYMS_U8*)"R12", 3 }, (SYMS_U64)SYMS_MachUnwindRegisterX64_R12 },
{ { (SYMS_U8*)"R13", 3 }, (SYMS_U64)SYMS_MachUnwindRegisterX64_R13 },
{ { (SYMS_U8*)"R14", 3 }, (SYMS_U64)SYMS_MachUnwindRegisterX64_R14 },
{ { (SYMS_U8*)"R15", 3 }, (SYMS_U64)SYMS_MachUnwindRegisterX64_R15 },
{ { (SYMS_U8*)"RBP", 3 }, (SYMS_U64)SYMS_MachUnwindRegisterX64_RBP },
};

//~ generated from code at syms/metaprogram/syms_metaprogram_serial.c:1458
SYMS_SerialType _syms_serial_type_SYMS_MachCpuType = {
{(SYMS_U8*)"SYMS_MachCpuType", 16}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_MachCpuType), _syms_serial_members_for_SYMS_MachCpuType, sizeof(SYMS_MachCpuType), syms_enum_index_from_mach_cpu_type
};
SYMS_SerialType _syms_serial_type_SYMS_MachCpuFamily = {
{(SYMS_U8*)"SYMS_MachCpuFamily", 18}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_MachCpuFamily), _syms_serial_members_for_SYMS_MachCpuFamily, sizeof(SYMS_MachCpuFamily), syms_enum_index_from_mach_cpu_family
};
SYMS_SerialType _syms_serial_type_SYMS_MachCpuSubtype = {
{(SYMS_U8*)"SYMS_MachCpuSubtype", 19}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_MachCpuSubtype), _syms_serial_members_for_SYMS_MachCpuSubtype, sizeof(SYMS_MachCpuSubtype), syms_enum_index_from_value_identity
};
SYMS_SerialType _syms_serial_type_SYMS_MachCpuSubtypeVAX = {
{(SYMS_U8*)"SYMS_MachCpuSubtypeVAX", 22}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_MachCpuSubtypeVAX), _syms_serial_members_for_SYMS_MachCpuSubtypeVAX, sizeof(SYMS_MachCpuSubtypeVAX), syms_enum_index_from_value_identity
};
SYMS_SerialType _syms_serial_type_SYMS_MachCpuSubtypeX86 = {
{(SYMS_U8*)"SYMS_MachCpuSubtypeX86", 22}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_MachCpuSubtypeX86), _syms_serial_members_for_SYMS_MachCpuSubtypeX86, sizeof(SYMS_MachCpuSubtypeX86), syms_enum_index_from_mach_cpu_subtype_x86
};
SYMS_SerialType _syms_serial_type_SYMS_MachCpuSubtypeIntel = {
{(SYMS_U8*)"SYMS_MachCpuSubtypeIntel", 24}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_MachCpuSubtypeIntel), _syms_serial_members_for_SYMS_MachCpuSubtypeIntel, sizeof(SYMS_MachCpuSubtypeIntel), syms_enum_index_from_mach_cpu_subtype_intel
};
SYMS_SerialType _syms_serial_type_SYMS_MachCpuSubtypeARM = {
{(SYMS_U8*)"SYMS_MachCpuSubtypeARM", 22}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_MachCpuSubtypeARM), _syms_serial_members_for_SYMS_MachCpuSubtypeARM, sizeof(SYMS_MachCpuSubtypeARM), syms_enum_index_from_mach_cpu_subtype_a_rm
};
SYMS_SerialType _syms_serial_type_SYMS_MachCpuSubtypeARM64 = {
{(SYMS_U8*)"SYMS_MachCpuSubtypeARM64", 24}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_MachCpuSubtypeARM64), _syms_serial_members_for_SYMS_MachCpuSubtypeARM64, sizeof(SYMS_MachCpuSubtypeARM64), syms_enum_index_from_value_identity
};
SYMS_SerialType _syms_serial_type_SYMS_MachVMProt = {
{(SYMS_U8*)"SYMS_MachVMProt", 15}, SYMS_SerialTypeKind_Integer, 0, 0, 4, 0
};
SYMS_SerialType _syms_serial_type_SYMS_MachFiletype = {
{(SYMS_U8*)"SYMS_MachFiletype", 17}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_MachFiletype), _syms_serial_members_for_SYMS_MachFiletype, sizeof(SYMS_MachFiletype), syms_enum_index_from_mach_filetype
};
SYMS_SerialType _syms_serial_type_SYMS_MachFlags = {
{(SYMS_U8*)"SYMS_MachFlags", 14}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_MachFlags), _syms_serial_members_for_SYMS_MachFlags, sizeof(SYMS_MachFlags), syms_enum_index_from_mach_flags
};
SYMS_SerialType _syms_serial_type_SYMS_MachLoadCommandType = {
{(SYMS_U8*)"SYMS_MachLoadCommandType", 24}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_MachLoadCommandType), _syms_serial_members_for_SYMS_MachLoadCommandType, sizeof(SYMS_MachLoadCommandType), syms_enum_index_from_mach_load_command_type
};
SYMS_SerialType _syms_serial_type_SYMS_MachSectionType = {
{(SYMS_U8*)"SYMS_MachSectionType", 20}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_MachSectionType), _syms_serial_members_for_SYMS_MachSectionType, sizeof(SYMS_MachSectionType), syms_enum_index_from_value_identity
};
SYMS_SerialType _syms_serial_type_SYMS_MachSectionAttr = {
{(SYMS_U8*)"SYMS_MachSectionAttr", 20}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_MachSectionAttr), _syms_serial_members_for_SYMS_MachSectionAttr, sizeof(SYMS_MachSectionAttr), syms_enum_index_from_mach_section_attr
};
SYMS_SerialType _syms_serial_type_SYMS_MachPlatformType = {
{(SYMS_U8*)"SYMS_MachPlatformType", 21}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_MachPlatformType), _syms_serial_members_for_SYMS_MachPlatformType, sizeof(SYMS_MachPlatformType), syms_enum_index_from_mach_platform_type
};
SYMS_SerialType _syms_serial_type_SYMS_MachToolType = {
{(SYMS_U8*)"SYMS_MachToolType", 17}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_MachToolType), _syms_serial_members_for_SYMS_MachToolType, sizeof(SYMS_MachToolType), syms_enum_index_from_mach_tool_type
};
SYMS_SerialType _syms_serial_type_SYMS_MachBindType = {
{(SYMS_U8*)"SYMS_MachBindType", 17}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_MachBindType), _syms_serial_members_for_SYMS_MachBindType, sizeof(SYMS_MachBindType), syms_enum_index_from_mach_bind_type
};
SYMS_SerialType _syms_serial_type_SYMS_MachBindOpcode = {
{(SYMS_U8*)"SYMS_MachBindOpcode", 19}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_MachBindOpcode), _syms_serial_members_for_SYMS_MachBindOpcode, sizeof(SYMS_MachBindOpcode), syms_enum_index_from_mach_bind_opcode
};
SYMS_SerialType _syms_serial_type_SYMS_MachBindSymbolFlags = {
{(SYMS_U8*)"SYMS_MachBindSymbolFlags", 24}, SYMS_SerialTypeKind_Flags, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_MachBindSymbolFlags), _syms_serial_members_for_SYMS_MachBindSymbolFlags, sizeof(SYMS_MachBindSymbolFlags), 0
};
SYMS_SerialType _syms_serial_type_SYMS_MachBindSpecialDylib = {
{(SYMS_U8*)"SYMS_MachBindSpecialDylib", 25}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_MachBindSpecialDylib), _syms_serial_members_for_SYMS_MachBindSpecialDylib, sizeof(SYMS_MachBindSpecialDylib), syms_enum_index_from_mach_bind_special_dylib
};
SYMS_SerialType _syms_serial_type_SYMS_MachNListType = {
{(SYMS_U8*)"SYMS_MachNListType", 18}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_MachNListType), _syms_serial_members_for_SYMS_MachNListType, sizeof(SYMS_MachNListType), syms_enum_index_from_mach_n_list_type
};
SYMS_SerialType _syms_serial_type_SYMS_MachStabType = {
{(SYMS_U8*)"SYMS_MachStabType", 17}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_MachStabType), _syms_serial_members_for_SYMS_MachStabType, sizeof(SYMS_MachStabType), syms_enum_index_from_mach_stab_type
};
SYMS_SerialType _syms_serial_type_SYMS_MachExportSymbolKind = {
{(SYMS_U8*)"SYMS_MachExportSymbolKind", 25}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_MachExportSymbolKind), _syms_serial_members_for_SYMS_MachExportSymbolKind, sizeof(SYMS_MachExportSymbolKind), syms_enum_index_from_value_identity
};
SYMS_SerialType _syms_serial_type_SYMS_MachExportSymbolFlags = {
{(SYMS_U8*)"SYMS_MachExportSymbolFlags", 26}, SYMS_SerialTypeKind_Flags, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_MachExportSymbolFlags), _syms_serial_members_for_SYMS_MachExportSymbolFlags, sizeof(SYMS_MachExportSymbolFlags), 0
};
SYMS_SerialType _syms_serial_type_SYMS_MachLCStr = {
{(SYMS_U8*)"MachLCStr", 9}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_MachLCStr), _syms_serial_members_for_SYMS_MachLCStr, sizeof(SYMS_MachLCStr), 0
};
SYMS_SerialType _syms_serial_type_SYMS_MachUUID = {
{(SYMS_U8*)"MachUUID", 8}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_MachUUID), _syms_serial_members_for_SYMS_MachUUID, sizeof(SYMS_MachUUID), 0
};
SYMS_SerialType _syms_serial_type_SYMS_MachDylib = {
{(SYMS_U8*)"MachDylib", 9}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_MachDylib), _syms_serial_members_for_SYMS_MachDylib, sizeof(SYMS_MachDylib), 0
};
SYMS_SerialType _syms_serial_type_SYMS_MachDylibCommand = {
{(SYMS_U8*)"MachDylibCommand", 16}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_MachDylibCommand), _syms_serial_members_for_SYMS_MachDylibCommand, sizeof(SYMS_MachDylibCommand), 0
};
SYMS_SerialType _syms_serial_type_SYMS_MachDyldInfoCommand = {
{(SYMS_U8*)"MachDyldInfoCommand", 19}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_MachDyldInfoCommand), _syms_serial_members_for_SYMS_MachDyldInfoCommand, sizeof(SYMS_MachDyldInfoCommand), 0
};
SYMS_SerialType _syms_serial_type_SYMS_MachLoadCommand = {
{(SYMS_U8*)"MachLoadCommand", 15}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_MachLoadCommand), _syms_serial_members_for_SYMS_MachLoadCommand, sizeof(SYMS_MachLoadCommand), 0
};
SYMS_SerialType _syms_serial_type_SYMS_MachFatHeader = {
{(SYMS_U8*)"MachFatHeader", 13}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_MachFatHeader), _syms_serial_members_for_SYMS_MachFatHeader, sizeof(SYMS_MachFatHeader), 0
};
SYMS_SerialType _syms_serial_type_SYMS_MachFatArch = {
{(SYMS_U8*)"MachFatArch", 11}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_MachFatArch), _syms_serial_members_for_SYMS_MachFatArch, sizeof(SYMS_MachFatArch), 0
};
SYMS_SerialType _syms_serial_type_SYMS_MachHeader32 = {
{(SYMS_U8*)"MachHeader32", 12}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_MachHeader32), _syms_serial_members_for_SYMS_MachHeader32, sizeof(SYMS_MachHeader32), 0
};
SYMS_SerialType _syms_serial_type_SYMS_MachHeader64 = {
{(SYMS_U8*)"MachHeader64", 12}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_MachHeader64), _syms_serial_members_for_SYMS_MachHeader64, sizeof(SYMS_MachHeader64), 0
};
SYMS_SerialType _syms_serial_type_SYMS_MachSegmentCommand32 = {
{(SYMS_U8*)"MachSegmentCommand32", 20}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_MachSegmentCommand32), _syms_serial_members_for_SYMS_MachSegmentCommand32, sizeof(SYMS_MachSegmentCommand32), 0
};
SYMS_SerialType _syms_serial_type_SYMS_MachSegmentCommand64 = {
{(SYMS_U8*)"MachSegmentCommand64", 20}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_MachSegmentCommand64), _syms_serial_members_for_SYMS_MachSegmentCommand64, sizeof(SYMS_MachSegmentCommand64), 0
};
SYMS_SerialType _syms_serial_type_SYMS_MachSection32 = {
{(SYMS_U8*)"MachSection32", 13}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_MachSection32), _syms_serial_members_for_SYMS_MachSection32, sizeof(SYMS_MachSection32), 0
};
SYMS_SerialType _syms_serial_type_SYMS_MachSection64 = {
{(SYMS_U8*)"MachSection64", 13}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_MachSection64), _syms_serial_members_for_SYMS_MachSection64, sizeof(SYMS_MachSection64), 0
};
SYMS_SerialType _syms_serial_type_SYMS_MachSymtabCommand = {
{(SYMS_U8*)"MachSymtabCommand", 17}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_MachSymtabCommand), _syms_serial_members_for_SYMS_MachSymtabCommand, sizeof(SYMS_MachSymtabCommand), 0
};
SYMS_SerialType _syms_serial_type_SYMS_MachDySymtabCommand = {
{(SYMS_U8*)"MachDySymtabCommand", 19}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_MachDySymtabCommand), _syms_serial_members_for_SYMS_MachDySymtabCommand, sizeof(SYMS_MachDySymtabCommand), 0
};
SYMS_SerialType _syms_serial_type_SYMS_MachNList32 = {
{(SYMS_U8*)"MachNList32", 11}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_MachNList32), _syms_serial_members_for_SYMS_MachNList32, sizeof(SYMS_MachNList32), 0
};
SYMS_SerialType _syms_serial_type_SYMS_MachNList64 = {
{(SYMS_U8*)"MachNList64", 11}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_MachNList64), _syms_serial_members_for_SYMS_MachNList64, sizeof(SYMS_MachNList64), 0
};
SYMS_SerialType _syms_serial_type_SYMS_MachBuildVersionCommand = {
{(SYMS_U8*)"MachBuildVersionCommand", 23}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_MachBuildVersionCommand), _syms_serial_members_for_SYMS_MachBuildVersionCommand, sizeof(SYMS_MachBuildVersionCommand), 0
};
SYMS_SerialType _syms_serial_type_SYMS_MachBuildToolVersion = {
{(SYMS_U8*)"MachBuildToolVersion", 20}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_MachBuildToolVersion), _syms_serial_members_for_SYMS_MachBuildToolVersion, sizeof(SYMS_MachBuildToolVersion), 0
};
SYMS_SerialType _syms_serial_type_SYMS_MachVersionMin = {
{(SYMS_U8*)"MachVersionMin", 14}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_MachVersionMin), _syms_serial_members_for_SYMS_MachVersionMin, sizeof(SYMS_MachVersionMin), 0
};
SYMS_SerialType _syms_serial_type_SYMS_MachDylinker = {
{(SYMS_U8*)"MachDylinker", 12}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_MachDylinker), _syms_serial_members_for_SYMS_MachDylinker, sizeof(SYMS_MachDylinker), 0
};
SYMS_SerialType _syms_serial_type_SYMS_MachPreboundDylibCommand = {
{(SYMS_U8*)"MachPreboundDylibCommand", 24}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_MachPreboundDylibCommand), _syms_serial_members_for_SYMS_MachPreboundDylibCommand, sizeof(SYMS_MachPreboundDylibCommand), 0
};
SYMS_SerialType _syms_serial_type_SYMS_MachRoutinesCommand = {
{(SYMS_U8*)"MachRoutinesCommand", 19}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_MachRoutinesCommand), _syms_serial_members_for_SYMS_MachRoutinesCommand, sizeof(SYMS_MachRoutinesCommand), 0
};
SYMS_SerialType _syms_serial_type_SYMS_MachRoutines64Command = {
{(SYMS_U8*)"MachRoutines64Command", 21}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_MachRoutines64Command), _syms_serial_members_for_SYMS_MachRoutines64Command, sizeof(SYMS_MachRoutines64Command), 0
};
SYMS_SerialType _syms_serial_type_SYMS_MachSubFrameworkCommand = {
{(SYMS_U8*)"MachSubFrameworkCommand", 23}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_MachSubFrameworkCommand), _syms_serial_members_for_SYMS_MachSubFrameworkCommand, sizeof(SYMS_MachSubFrameworkCommand), 0
};
SYMS_SerialType _syms_serial_type_SYMS_MachSubUmbrellaCommand = {
{(SYMS_U8*)"MachSubUmbrellaCommand", 22}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_MachSubUmbrellaCommand), _syms_serial_members_for_SYMS_MachSubUmbrellaCommand, sizeof(SYMS_MachSubUmbrellaCommand), 0
};
SYMS_SerialType _syms_serial_type_SYMS_MachSubClientCommand = {
{(SYMS_U8*)"MachSubClientCommand", 20}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_MachSubClientCommand), _syms_serial_members_for_SYMS_MachSubClientCommand, sizeof(SYMS_MachSubClientCommand), 0
};
SYMS_SerialType _syms_serial_type_SYMS_MachSubLibraryCommand = {
{(SYMS_U8*)"MachSubLibraryCommand", 21}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_MachSubLibraryCommand), _syms_serial_members_for_SYMS_MachSubLibraryCommand, sizeof(SYMS_MachSubLibraryCommand), 0
};
SYMS_SerialType _syms_serial_type_SYMS_MachTwoLevelHintsCommand = {
{(SYMS_U8*)"MachTwoLevelHintsCommand", 24}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_MachTwoLevelHintsCommand), _syms_serial_members_for_SYMS_MachTwoLevelHintsCommand, sizeof(SYMS_MachTwoLevelHintsCommand), 0
};
SYMS_SerialType _syms_serial_type_SYMS_MachTwoLevelHint = {
{(SYMS_U8*)"SYMS_MachTwoLevelHint", 21}, SYMS_SerialTypeKind_Flags, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_MachTwoLevelHint), _syms_serial_members_for_SYMS_MachTwoLevelHint, sizeof(SYMS_MachTwoLevelHint), 0
};
SYMS_SerialType _syms_serial_type_SYMS_MachPrebindChecksumCommand = {
{(SYMS_U8*)"MachPrebindChecksumCommand", 26}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_MachPrebindChecksumCommand), _syms_serial_members_for_SYMS_MachPrebindChecksumCommand, sizeof(SYMS_MachPrebindChecksumCommand), 0
};
SYMS_SerialType _syms_serial_type_SYMS_MachRPathCommand = {
{(SYMS_U8*)"MachRPathCommand", 16}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_MachRPathCommand), _syms_serial_members_for_SYMS_MachRPathCommand, sizeof(SYMS_MachRPathCommand), 0
};
SYMS_SerialType _syms_serial_type_SYMS_MachLinkeditDataCommand = {
{(SYMS_U8*)"MachLinkeditDataCommand", 23}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_MachLinkeditDataCommand), _syms_serial_members_for_SYMS_MachLinkeditDataCommand, sizeof(SYMS_MachLinkeditDataCommand), 0
};
SYMS_SerialType _syms_serial_type_SYMS_MachEncryptionInfoCommand = {
{(SYMS_U8*)"MachEncryptionInfoCommand", 25}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_MachEncryptionInfoCommand), _syms_serial_members_for_SYMS_MachEncryptionInfoCommand, sizeof(SYMS_MachEncryptionInfoCommand), 0
};
SYMS_SerialType _syms_serial_type_SYMS_MachEncryptionInfo64Command = {
{(SYMS_U8*)"MachEncryptionInfo64Command", 27}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_MachEncryptionInfo64Command), _syms_serial_members_for_SYMS_MachEncryptionInfo64Command, sizeof(SYMS_MachEncryptionInfo64Command), 0
};
SYMS_SerialType _syms_serial_type_SYMS_MachEntryPointCommand = {
{(SYMS_U8*)"MachEntryPointCommand", 21}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_MachEntryPointCommand), _syms_serial_members_for_SYMS_MachEntryPointCommand, sizeof(SYMS_MachEntryPointCommand), 0
};
SYMS_SerialType _syms_serial_type_SYMS_MachSourceVersionCommand = {
{(SYMS_U8*)"MachSourceVersionCommand", 24}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_MachSourceVersionCommand), _syms_serial_members_for_SYMS_MachSourceVersionCommand, sizeof(SYMS_MachSourceVersionCommand), 0
};
SYMS_SerialType _syms_serial_type_SYMS_MachLinkerOptionCommand = {
{(SYMS_U8*)"MachLinkerOptionCommand", 23}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_MachLinkerOptionCommand), _syms_serial_members_for_SYMS_MachLinkerOptionCommand, sizeof(SYMS_MachLinkerOptionCommand), 0
};
SYMS_SerialType _syms_serial_type_SYMS_MachNoteCommand = {
{(SYMS_U8*)"MachNoteCommand", 15}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_MachNoteCommand), _syms_serial_members_for_SYMS_MachNoteCommand, sizeof(SYMS_MachNoteCommand), 0
};
SYMS_SerialType _syms_serial_type_SYMS_MachSymSegCommand = {
{(SYMS_U8*)"MachSymSegCommand", 17}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_MachSymSegCommand), _syms_serial_members_for_SYMS_MachSymSegCommand, sizeof(SYMS_MachSymSegCommand), 0
};
SYMS_SerialType _syms_serial_type_SYMS_MachFvmlib = {
{(SYMS_U8*)"MachFvmlib", 10}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_MachFvmlib), _syms_serial_members_for_SYMS_MachFvmlib, sizeof(SYMS_MachFvmlib), 0
};
SYMS_SerialType _syms_serial_type_SYMS_MachFvmlibCommand = {
{(SYMS_U8*)"MachFvmlibCommand", 17}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_MachFvmlibCommand), _syms_serial_members_for_SYMS_MachFvmlibCommand, sizeof(SYMS_MachFvmlibCommand), 0
};
SYMS_SerialType _syms_serial_type_SYMS_MachThreadCommand = {
{(SYMS_U8*)"MachThreadCommand", 17}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_MachThreadCommand), _syms_serial_members_for_SYMS_MachThreadCommand, sizeof(SYMS_MachThreadCommand), 0
};
SYMS_SerialType _syms_serial_type_SYMS_MachUnwindEncModeX86 = {
{(SYMS_U8*)"SYMS_MachUnwindEncModeX86", 25}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_MachUnwindEncModeX86), _syms_serial_members_for_SYMS_MachUnwindEncModeX86, sizeof(SYMS_MachUnwindEncModeX86), syms_enum_index_from_mach_unwind_enc_mode_x86
};
SYMS_SerialType _syms_serial_type_SYMS_MachUnwindRegisterX86 = {
{(SYMS_U8*)"SYMS_MachUnwindRegisterX86", 26}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_MachUnwindRegisterX86), _syms_serial_members_for_SYMS_MachUnwindRegisterX86, sizeof(SYMS_MachUnwindRegisterX86), syms_enum_index_from_value_identity
};
SYMS_SerialType _syms_serial_type_SYMS_MachUnwindEncX86 = {
{(SYMS_U8*)"SYMS_MachUnwindEncX86", 21}, SYMS_SerialTypeKind_Flags, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_MachUnwindEncX86), _syms_serial_members_for_SYMS_MachUnwindEncX86, sizeof(SYMS_MachUnwindEncX86), 0
};
SYMS_SerialType _syms_serial_type_SYMS_MachUnwindEncModeX64 = {
{(SYMS_U8*)"SYMS_MachUnwindEncModeX64", 25}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_MachUnwindEncModeX64), _syms_serial_members_for_SYMS_MachUnwindEncModeX64, sizeof(SYMS_MachUnwindEncModeX64), syms_enum_index_from_mach_unwind_enc_mode_x64
};
SYMS_SerialType _syms_serial_type_SYMS_MachUnwindRegisterX64 = {
{(SYMS_U8*)"SYMS_MachUnwindRegisterX64", 26}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_MachUnwindRegisterX64), _syms_serial_members_for_SYMS_MachUnwindRegisterX64, sizeof(SYMS_MachUnwindRegisterX64), syms_enum_index_from_value_identity
};

#endif // defined(SYMS_ENABLE_MACH_SERIAL_INFO)


////////////////////////////////
#if defined(SYMS_ENABLE_PE_SERIAL_INFO)

//~ generated from code at syms/metaprogram/syms_metaprogram_serial.c:1091
SYMS_API SYMS_U64
syms_enum_index_from_pe_windows_subsystem(SYMS_U64 v){
SYMS_U64 result = 0;
switch ((SYMS_U16)v){
default: break;
case SYMS_PeWindowsSubsystem_UNKNOWN: result = 0; break;
case SYMS_PeWindowsSubsystem_NATIVE: result = 1; break;
case SYMS_PeWindowsSubsystem_WINDOWS_GUI: result = 2; break;
case SYMS_PeWindowsSubsystem_WINDOWS_CUI: result = 3; break;
case SYMS_PeWindowsSubsystem_OS2_CUI: result = 4; break;
case SYMS_PeWindowsSubsystem_POSIX_CUI: result = 5; break;
case SYMS_PeWindowsSubsystem_NATIVE_WINDOWS: result = 6; break;
case SYMS_PeWindowsSubsystem_WINDOWS_CE_GUI: result = 7; break;
case SYMS_PeWindowsSubsystem_EFI_APPLICATION: result = 8; break;
case SYMS_PeWindowsSubsystem_EFI_BOOT_SERVICE_DRIVER: result = 9; break;
case SYMS_PeWindowsSubsystem_EFI_RUNTIME_DRIVER: result = 10; break;
case SYMS_PeWindowsSubsystem_EFI_ROM: result = 11; break;
case SYMS_PeWindowsSubsystem_XBOX: result = 12; break;
case SYMS_PeWindowsSubsystem_WINDOWS_BOOT_APPLICATION: result = 13; break;
}
return(result);
}
// syms_enum_index_from_pe_data_directory_index - skipped identity mapping
SYMS_API SYMS_U64
syms_enum_index_from_pe_debug_directory_type(SYMS_U64 v){
SYMS_U64 result = 0;
switch ((SYMS_U32)v){
default: break;
case SYMS_PeDebugDirectoryType_UNKNOWN: result = 0; break;
case SYMS_PeDebugDirectoryType_COFF: result = 1; break;
case SYMS_PeDebugDirectoryType_CODEVIEW: result = 2; break;
case SYMS_PeDebugDirectoryType_FPO: result = 3; break;
case SYMS_PeDebugDirectoryType_MISC: result = 4; break;
case SYMS_PeDebugDirectoryType_EXCEPTION: result = 5; break;
case SYMS_PeDebugDirectoryType_FIXUP: result = 6; break;
case SYMS_PeDebugDirectoryType_OMAP_TO_SRC: result = 7; break;
case SYMS_PeDebugDirectoryType_OMAP_FROM_SRC: result = 8; break;
case SYMS_PeDebugDirectoryType_BORLAND: result = 9; break;
case SYMS_PeDebugDirectoryType_RESERVED10: result = 10; break;
case SYMS_PeDebugDirectoryType_CLSID: result = 11; break;
case SYMS_PeDebugDirectoryType_VC_FEATURE: result = 12; break;
case SYMS_PeDebugDirectoryType_POGO: result = 13; break;
case SYMS_PeDebugDirectoryType_ILTCG: result = 14; break;
case SYMS_PeDebugDirectoryType_MPX: result = 15; break;
case SYMS_PeDebugDirectoryType_REPRO: result = 16; break;
case SYMS_PeDebugDirectoryType_EX_DLLCHARACTERISTICS: result = 17; break;
}
return(result);
}
SYMS_API SYMS_U64
syms_enum_index_from_pe_f_p_o_flags(SYMS_U64 v){
SYMS_U64 result = 0;
switch ((SYMS_U8)v){
default: break;
case SYMS_PeFPOFlags_HAS_SEH: result = 0; break;
case SYMS_PeFPOFlags_USE_BP_REG: result = 1; break;
case SYMS_PeFPOFlags_RESERVED: result = 2; break;
}
return(result);
}
// syms_enum_index_from_pe_f_p_o_type - skipped identity mapping
// syms_enum_index_from_pe_debug_misc_type - skipped identity mapping

//~ generated from code at syms/metaprogram/syms_metaprogram_serial.c:1322
static SYMS_SerialField _syms_serial_members_for_SYMS_DosHeader[] = {
{ {(SYMS_U8*)"magic", 5}, &_syms_serial_type_SYMS_U16, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"last_page_size", 14}, &_syms_serial_type_SYMS_U16, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"page_count", 10}, &_syms_serial_type_SYMS_U16, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"reloc_count", 11}, &_syms_serial_type_SYMS_U16, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"paragraph_header_size", 21}, &_syms_serial_type_SYMS_U16, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"min_paragraph", 13}, &_syms_serial_type_SYMS_U16, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"max_paragraph", 13}, &_syms_serial_type_SYMS_U16, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"init_ss", 7}, &_syms_serial_type_SYMS_U16, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"init_sp", 7}, &_syms_serial_type_SYMS_U16, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"checksum", 8}, &_syms_serial_type_SYMS_U16, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"init_ip", 7}, &_syms_serial_type_SYMS_U16, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"init_cs", 7}, &_syms_serial_type_SYMS_U16, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"reloc_table_file_off", 20}, &_syms_serial_type_SYMS_U16, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"overlay_number", 14}, &_syms_serial_type_SYMS_U16, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"reserved", 8}, &_syms_serial_type_SYMS_U16, SYMS_SerialWidthKind_Fixed, 4 },
{ {(SYMS_U8*)"oem_id", 6}, &_syms_serial_type_SYMS_U16, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"oem_info", 8}, &_syms_serial_type_SYMS_U16, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"reserved2", 9}, &_syms_serial_type_SYMS_U16, SYMS_SerialWidthKind_Fixed, 10 },
{ {(SYMS_U8*)"coff_file_offset", 16}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
};
SYMS_SerialValue _syms_serial_members_for_SYMS_PeWindowsSubsystem[] = {
{ { (SYMS_U8*)"Unknown", 7 }, (SYMS_U64)SYMS_PeWindowsSubsystem_UNKNOWN },
{ { (SYMS_U8*)"Native", 6 }, (SYMS_U64)SYMS_PeWindowsSubsystem_NATIVE },
{ { (SYMS_U8*)"Windows GUI", 11 }, (SYMS_U64)SYMS_PeWindowsSubsystem_WINDOWS_GUI },
{ { (SYMS_U8*)"Windows CUI", 11 }, (SYMS_U64)SYMS_PeWindowsSubsystem_WINDOWS_CUI },
{ { (SYMS_U8*)"OS/2 CUI", 8 }, (SYMS_U64)SYMS_PeWindowsSubsystem_OS2_CUI },
{ { (SYMS_U8*)"Posix CUI", 9 }, (SYMS_U64)SYMS_PeWindowsSubsystem_POSIX_CUI },
{ { (SYMS_U8*)"Native Win9x driver", 19 }, (SYMS_U64)SYMS_PeWindowsSubsystem_NATIVE_WINDOWS },
{ { (SYMS_U8*)"Windows CE", 10 }, (SYMS_U64)SYMS_PeWindowsSubsystem_WINDOWS_CE_GUI },
{ { (SYMS_U8*)"EFI Application", 15 }, (SYMS_U64)SYMS_PeWindowsSubsystem_EFI_APPLICATION },
{ { (SYMS_U8*)"EFI Boot Service Driver", 23 }, (SYMS_U64)SYMS_PeWindowsSubsystem_EFI_BOOT_SERVICE_DRIVER },
{ { (SYMS_U8*)"EFI Runtime Driver", 18 }, (SYMS_U64)SYMS_PeWindowsSubsystem_EFI_RUNTIME_DRIVER },
{ { (SYMS_U8*)"EFI Rom Image", 13 }, (SYMS_U64)SYMS_PeWindowsSubsystem_EFI_ROM },
{ { (SYMS_U8*)"XBOX", 4 }, (SYMS_U64)SYMS_PeWindowsSubsystem_XBOX },
{ { (SYMS_U8*)"Windows Boot Application", 24 }, (SYMS_U64)SYMS_PeWindowsSubsystem_WINDOWS_BOOT_APPLICATION },
};
SYMS_SerialFlag _syms_serial_members_for_SYMS_ImageFileCharacteristics[] = {
{ { (SYMS_U8*)"Stripped", 8 }, &_syms_serial_type_SYMS_U16, 0x1, 0 },
{ { (SYMS_U8*)"Executable", 10 }, &_syms_serial_type_SYMS_U16, 0x1, 1 },
{ { (SYMS_U8*)"Line Numbers Stripped", 21 }, &_syms_serial_type_SYMS_U16, 0x1, 2 },
{ { (SYMS_U8*)"Symbols Stripped", 16 }, &_syms_serial_type_SYMS_U16, 0x1, 3 },
{ { (SYMS_U8*)"Aggressive Trim", 15 }, &_syms_serial_type_SYMS_U16, 0x1, 4 },
{ { (SYMS_U8*)"Large Address Aware", 19 }, &_syms_serial_type_SYMS_U16, 0x1, 5 },
{ { (SYMS_U8*)"UNUSED1", 7 }, &_syms_serial_type_SYMS_U16, 0x1, 6 },
{ { (SYMS_U8*)"BYTES_RESERVED_LO", 17 }, &_syms_serial_type_SYMS_U16, 0x1, 7 },
{ { (SYMS_U8*)"32-Bit Machine", 14 }, &_syms_serial_type_SYMS_U16, 0x1, 8 },
{ { (SYMS_U8*)"Debug Info Stripped", 19 }, &_syms_serial_type_SYMS_U16, 0x1, 9 },
{ { (SYMS_U8*)"Run From Swap", 13 }, &_syms_serial_type_SYMS_U16, 0x1, 10 },
{ { (SYMS_U8*)"Net Run From Swap", 17 }, &_syms_serial_type_SYMS_U16, 0x1, 11 },
{ { (SYMS_U8*)"System File", 11 }, &_syms_serial_type_SYMS_U16, 0x1, 12 },
{ { (SYMS_U8*)"DLL", 3 }, &_syms_serial_type_SYMS_U16, 0x1, 13 },
{ { (SYMS_U8*)"File Up System Only", 19 }, &_syms_serial_type_SYMS_U16, 0x1, 14 },
{ { (SYMS_U8*)"BYTES_RESERVED_HI", 17 }, &_syms_serial_type_SYMS_U16, 0x1, 15 },
};
SYMS_SerialFlag _syms_serial_members_for_SYMS_DllCharacteristics[] = {
{ { (SYMS_U8*)"High Entropy VA", 15 }, &_syms_serial_type_SYMS_U16, 0x1, 5 },
{ { (SYMS_U8*)"Dynamic Base", 12 }, &_syms_serial_type_SYMS_U16, 0x1, 6 },
{ { (SYMS_U8*)"Force Integrity", 15 }, &_syms_serial_type_SYMS_U16, 0x1, 7 },
{ { (SYMS_U8*)"NX Compatible", 13 }, &_syms_serial_type_SYMS_U16, 0x1, 8 },
{ { (SYMS_U8*)"No Isolation", 12 }, &_syms_serial_type_SYMS_U16, 0x1, 9 },
{ { (SYMS_U8*)"No SEH", 6 }, &_syms_serial_type_SYMS_U16, 0x1, 10 },
{ { (SYMS_U8*)"No Bind", 7 }, &_syms_serial_type_SYMS_U16, 0x1, 11 },
{ { (SYMS_U8*)"App Container", 13 }, &_syms_serial_type_SYMS_U16, 0x1, 12 },
{ { (SYMS_U8*)"WDM Driver", 10 }, &_syms_serial_type_SYMS_U16, 0x1, 13 },
{ { (SYMS_U8*)"Guard CF", 8 }, &_syms_serial_type_SYMS_U16, 0x1, 14 },
{ { (SYMS_U8*)"Terminal Server Aware", 21 }, &_syms_serial_type_SYMS_U16, 0x1, 15 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_PeOptionalHeader32[] = {
{ {(SYMS_U8*)"magic", 5}, &_syms_serial_type_SYMS_U16, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"major_linker_version", 20}, &_syms_serial_type_SYMS_U8, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"minor_linker_version", 20}, &_syms_serial_type_SYMS_U8, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"sizeof_code", 11}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"sizeof_inited_data", 18}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"sizeof_uninited_data", 20}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"entry_point_va", 14}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"code_base", 9}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"data_base", 9}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"image_base", 10}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"section_alignment", 17}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"file_alignment", 14}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"major_os_ver", 12}, &_syms_serial_type_SYMS_U16, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"minor_os_ver", 12}, &_syms_serial_type_SYMS_U16, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"major_img_ver", 13}, &_syms_serial_type_SYMS_U16, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"minor_img_ver", 13}, &_syms_serial_type_SYMS_U16, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"major_subsystem_ver", 19}, &_syms_serial_type_SYMS_U16, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"minor_subsystem_ver", 19}, &_syms_serial_type_SYMS_U16, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"win32_version_value", 19}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"sizeof_image", 12}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"sizeof_headers", 14}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"check_sum", 9}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"subsystem", 9}, &_syms_serial_type_SYMS_PeWindowsSubsystem, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"dll_characteristics", 19}, &_syms_serial_type_SYMS_DllCharacteristics, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"sizeof_stack_reserve", 20}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"sizeof_stack_commit", 19}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"sizeof_heap_reserve", 19}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"sizeof_heap_commit", 18}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"loader_flags", 12}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"data_dir_count", 14}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_PeOptionalHeader32Plus[] = {
{ {(SYMS_U8*)"magic", 5}, &_syms_serial_type_SYMS_U16, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"major_linker_version", 20}, &_syms_serial_type_SYMS_U8, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"minor_linker_version", 20}, &_syms_serial_type_SYMS_U8, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"sizeof_code", 11}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"sizeof_inited_data", 18}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"sizeof_uninited_data", 20}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"entry_point_va", 14}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"code_base", 9}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"image_base", 10}, &_syms_serial_type_SYMS_U64, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"section_alignment", 17}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"file_alignment", 14}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"major_os_ver", 12}, &_syms_serial_type_SYMS_U16, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"minor_os_ver", 12}, &_syms_serial_type_SYMS_U16, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"major_img_ver", 13}, &_syms_serial_type_SYMS_U16, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"minor_img_ver", 13}, &_syms_serial_type_SYMS_U16, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"major_subsystem_ver", 19}, &_syms_serial_type_SYMS_U16, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"minor_subsystem_ver", 19}, &_syms_serial_type_SYMS_U16, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"win32_version_value", 19}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"sizeof_image", 12}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"sizeof_headers", 14}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"check_sum", 9}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"subsystem", 9}, &_syms_serial_type_SYMS_PeWindowsSubsystem, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"dll_characteristics", 19}, &_syms_serial_type_SYMS_DllCharacteristics, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"sizeof_stack_reserve", 20}, &_syms_serial_type_SYMS_U64, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"sizeof_stack_commit", 19}, &_syms_serial_type_SYMS_U64, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"sizeof_heap_reserve", 19}, &_syms_serial_type_SYMS_U64, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"sizeof_heap_commit", 18}, &_syms_serial_type_SYMS_U64, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"loader_flags", 12}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"data_dir_count", 14}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
};
SYMS_SerialValue _syms_serial_members_for_SYMS_PeDataDirectoryIndex[] = {
{ { (SYMS_U8*)"Export", 6 }, (SYMS_U64)SYMS_PeDataDirectoryIndex_EXPORT },
{ { (SYMS_U8*)"Import", 6 }, (SYMS_U64)SYMS_PeDataDirectoryIndex_IMPORT },
{ { (SYMS_U8*)"Resources", 9 }, (SYMS_U64)SYMS_PeDataDirectoryIndex_RESOURCES },
{ { (SYMS_U8*)"Exceptions", 10 }, (SYMS_U64)SYMS_PeDataDirectoryIndex_EXCEPTIONS },
{ { (SYMS_U8*)"Certificate", 11 }, (SYMS_U64)SYMS_PeDataDirectoryIndex_CERT },
{ { (SYMS_U8*)"Base Relocs", 11 }, (SYMS_U64)SYMS_PeDataDirectoryIndex_BASE_RELOC },
{ { (SYMS_U8*)"Debug", 5 }, (SYMS_U64)SYMS_PeDataDirectoryIndex_DEBUG },
{ { (SYMS_U8*)"Arch", 4 }, (SYMS_U64)SYMS_PeDataDirectoryIndex_ARCH },
{ { (SYMS_U8*)"Global PTR", 10 }, (SYMS_U64)SYMS_PeDataDirectoryIndex_GLOBAL_PTR },
{ { (SYMS_U8*)"TLS", 3 }, (SYMS_U64)SYMS_PeDataDirectoryIndex_TLS },
{ { (SYMS_U8*)"Load Config", 11 }, (SYMS_U64)SYMS_PeDataDirectoryIndex_LOAD_CONFIG },
{ { (SYMS_U8*)"Bound Imports", 13 }, (SYMS_U64)SYMS_PeDataDirectoryIndex_BOUND_IMPORT },
{ { (SYMS_U8*)"IAT", 3 }, (SYMS_U64)SYMS_PeDataDirectoryIndex_IMPORT_ADDR },
{ { (SYMS_U8*)"Delay Import", 12 }, (SYMS_U64)SYMS_PeDataDirectoryIndex_DELAY_IMPORT },
{ { (SYMS_U8*)"COM Descriptor", 14 }, (SYMS_U64)SYMS_PeDataDirectoryIndex_COM_DESCRIPTOR },
{ { (SYMS_U8*)"RESERVED", 8 }, (SYMS_U64)SYMS_PeDataDirectoryIndex_RESERVED },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_PeDataDirectory[] = {
{ {(SYMS_U8*)"virt_off", 8}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"virt_size", 9}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
};
SYMS_SerialValue _syms_serial_members_for_SYMS_PeDebugDirectoryType[] = {
{ { (SYMS_U8*)"UNKNOWN", 7 }, (SYMS_U64)SYMS_PeDebugDirectoryType_UNKNOWN },
{ { (SYMS_U8*)"COFF", 4 }, (SYMS_U64)SYMS_PeDebugDirectoryType_COFF },
{ { (SYMS_U8*)"CODEVIEW", 8 }, (SYMS_U64)SYMS_PeDebugDirectoryType_CODEVIEW },
{ { (SYMS_U8*)"FPO", 3 }, (SYMS_U64)SYMS_PeDebugDirectoryType_FPO },
{ { (SYMS_U8*)"MISC", 4 }, (SYMS_U64)SYMS_PeDebugDirectoryType_MISC },
{ { (SYMS_U8*)"EXCEPTION", 9 }, (SYMS_U64)SYMS_PeDebugDirectoryType_EXCEPTION },
{ { (SYMS_U8*)"FIXUP", 5 }, (SYMS_U64)SYMS_PeDebugDirectoryType_FIXUP },
{ { (SYMS_U8*)"OMAP_TO_SRC", 11 }, (SYMS_U64)SYMS_PeDebugDirectoryType_OMAP_TO_SRC },
{ { (SYMS_U8*)"OMAP_FROM_SRC", 13 }, (SYMS_U64)SYMS_PeDebugDirectoryType_OMAP_FROM_SRC },
{ { (SYMS_U8*)"BORLAND", 7 }, (SYMS_U64)SYMS_PeDebugDirectoryType_BORLAND },
{ { (SYMS_U8*)"RESERVED10", 10 }, (SYMS_U64)SYMS_PeDebugDirectoryType_RESERVED10 },
{ { (SYMS_U8*)"CLSID", 5 }, (SYMS_U64)SYMS_PeDebugDirectoryType_CLSID },
{ { (SYMS_U8*)"VC_FEATURE", 10 }, (SYMS_U64)SYMS_PeDebugDirectoryType_VC_FEATURE },
{ { (SYMS_U8*)"POGO", 4 }, (SYMS_U64)SYMS_PeDebugDirectoryType_POGO },
{ { (SYMS_U8*)"ILTCG", 5 }, (SYMS_U64)SYMS_PeDebugDirectoryType_ILTCG },
{ { (SYMS_U8*)"MPX", 3 }, (SYMS_U64)SYMS_PeDebugDirectoryType_MPX },
{ { (SYMS_U8*)"REPRO", 5 }, (SYMS_U64)SYMS_PeDebugDirectoryType_REPRO },
{ { (SYMS_U8*)"EX_DLLCHARACTERISTICS", 21 }, (SYMS_U64)SYMS_PeDebugDirectoryType_EX_DLLCHARACTERISTICS },
};
SYMS_SerialValue _syms_serial_members_for_SYMS_PeFPOFlags[] = {
{ { (SYMS_U8*)"HAS_SEH", 7 }, (SYMS_U64)SYMS_PeFPOFlags_HAS_SEH },
{ { (SYMS_U8*)"USE_BP_REG", 10 }, (SYMS_U64)SYMS_PeFPOFlags_USE_BP_REG },
{ { (SYMS_U8*)"RESERVED", 8 }, (SYMS_U64)SYMS_PeFPOFlags_RESERVED },
};
SYMS_SerialFlag _syms_serial_members_for_SYMS_PeFPOEncoded[] = {
{ { (SYMS_U8*)"PROLOG_SIZE", 11 }, &_syms_serial_type_SYMS_U8, 0xff, 0 },
{ { (SYMS_U8*)"SAVED_REGS_SIZE", 15 }, &_syms_serial_type_SYMS_U8, 0x7, 8 },
{ { (SYMS_U8*)"FLAGS", 5 }, &_syms_serial_type_SYMS_U8, 0x7, 11 },
{ { (SYMS_U8*)"FRAME_TYPE", 10 }, &_syms_serial_type_SYMS_U8, 0x3, 14 },
};
SYMS_SerialValue _syms_serial_members_for_SYMS_PeFPOType[] = {
{ { (SYMS_U8*)"FPO", 3 }, (SYMS_U64)SYMS_PeFPOType_FPO },
{ { (SYMS_U8*)"TRAP", 4 }, (SYMS_U64)SYMS_PeFPOType_TRAP },
{ { (SYMS_U8*)"TSS", 3 }, (SYMS_U64)SYMS_PeFPOType_TSS },
{ { (SYMS_U8*)"NOFPO", 5 }, (SYMS_U64)SYMS_PeFPOType_NOFPO },
};
SYMS_SerialValue _syms_serial_members_for_SYMS_PeDebugMiscType[] = {
{ { (SYMS_U8*)"NULL", 4 }, (SYMS_U64)SYMS_PeDebugMiscType_NULL },
{ { (SYMS_U8*)"EXE_NAME", 8 }, (SYMS_U64)SYMS_PeDebugMiscType_EXE_NAME },
};
static SYMS_SerialField _syms_serial_members_for_SYMS_PeDebugDirectory[] = {
{ {(SYMS_U8*)"characteristics", 15}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"time_stamp", 10}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"major_ver", 9}, &_syms_serial_type_SYMS_U16, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"minor_ver", 9}, &_syms_serial_type_SYMS_U16, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"type", 4}, &_syms_serial_type_SYMS_PeDebugDirectoryType, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"size", 4}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"virtual_offset", 14}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
{ {(SYMS_U8*)"file_offset", 11}, &_syms_serial_type_SYMS_U32, SYMS_SerialWidthKind_Null, 0 },
};
SYMS_SerialFlag _syms_serial_members_for_SYMS_PeGlobalFlags[] = {
{ { (SYMS_U8*)"STOP_ON_EXCEPTION", 17 }, &_syms_serial_type_SYMS_U32, 0x1, 0 },
{ { (SYMS_U8*)"SHOW_LDR_SNAPS", 14 }, &_syms_serial_type_SYMS_U32, 0x1, 1 },
{ { (SYMS_U8*)"DEBUG_INITIAL_COMMAND", 21 }, &_syms_serial_type_SYMS_U32, 0x1, 2 },
{ { (SYMS_U8*)"STOP_ON_HUNG_GUI", 16 }, &_syms_serial_type_SYMS_U32, 0x1, 3 },
{ { (SYMS_U8*)"HEAP_ENABLE_TAIL_CHECK", 22 }, &_syms_serial_type_SYMS_U32, 0x1, 4 },
{ { (SYMS_U8*)"HEAP_ENABLE_FREE_CHECK", 22 }, &_syms_serial_type_SYMS_U32, 0x1, 5 },
{ { (SYMS_U8*)"HEAP_VALIDATE_PARAMETERS", 24 }, &_syms_serial_type_SYMS_U32, 0x1, 6 },
{ { (SYMS_U8*)"HEAP_VALIDATE_ALL", 17 }, &_syms_serial_type_SYMS_U32, 0x1, 7 },
{ { (SYMS_U8*)"APPLICATION_VERIFIER", 20 }, &_syms_serial_type_SYMS_U32, 0x1, 8 },
{ { (SYMS_U8*)"POOL_ENABLE_TAGGING", 19 }, &_syms_serial_type_SYMS_U32, 0x1, 10 },
{ { (SYMS_U8*)"HEAP_ENABLE_TAGGING", 19 }, &_syms_serial_type_SYMS_U32, 0x1, 11 },
{ { (SYMS_U8*)"STACK_TRACE_DB", 14 }, &_syms_serial_type_SYMS_U32, 0x1, 12 },
{ { (SYMS_U8*)"KERNEL_STACK_TRACE_DB", 21 }, &_syms_serial_type_SYMS_U32, 0x1, 13 },
{ { (SYMS_U8*)"MAINTAIN_OBJECT_TYPELIST", 24 }, &_syms_serial_type_SYMS_U32, 0x1, 14 },
{ { (SYMS_U8*)"HEAP_ENABLE_TAG_BY_DLL", 22 }, &_syms_serial_type_SYMS_U32, 0x1, 15 },
{ { (SYMS_U8*)"DISABLE_STACK_EXTENSION", 23 }, &_syms_serial_type_SYMS_U32, 0x1, 16 },
{ { (SYMS_U8*)"ENABLE_CSRDEBUG", 15 }, &_syms_serial_type_SYMS_U32, 0x1, 17 },
{ { (SYMS_U8*)"ENABLE_KDEBUG_SYMBOL_LOAD", 25 }, &_syms_serial_type_SYMS_U32, 0x1, 18 },
{ { (SYMS_U8*)"DISABLE_PAGE_KERNEL_STACKS", 26 }, &_syms_serial_type_SYMS_U32, 0x1, 19 },
{ { (SYMS_U8*)"ENABLE_SYSTEM_CRIT_BREAKS", 25 }, &_syms_serial_type_SYMS_U32, 0x1, 20 },
{ { (SYMS_U8*)"HEAP_DISABLE_COALESCING", 23 }, &_syms_serial_type_SYMS_U32, 0x1, 21 },
{ { (SYMS_U8*)"ENABLE_CLOSE_EXCEPTIONS", 23 }, &_syms_serial_type_SYMS_U32, 0x1, 22 },
{ { (SYMS_U8*)"ENABLE_EXCEPTION_LOGGING", 24 }, &_syms_serial_type_SYMS_U32, 0x1, 23 },
{ { (SYMS_U8*)"ENABLE_HANDLE_TYPE_TAGGING", 26 }, &_syms_serial_type_SYMS_U32, 0x1, 24 },
{ { (SYMS_U8*)"HEAP_PAGE_ALLOCS", 16 }, &_syms_serial_type_SYMS_U32, 0x1, 25 },
{ { (SYMS_U8*)"DEBUG_INITIAL_COMMAND_EX", 24 }, &_syms_serial_type_SYMS_U32, 0x1, 26 },
{ { (SYMS_U8*)"DISABLE_DBGPRINT", 16 }, &_syms_serial_type_SYMS_U32, 0x1, 27 },
{ { (SYMS_U8*)"CRITSEC_EVENT_CREATION", 22 }, &_syms_serial_type_SYMS_U32, 0x1, 28 },
{ { (SYMS_U8*)"LDR_TOP_DOWN", 12 }, &_syms_serial_type_SYMS_U32, 0x1, 29 },
{ { (SYMS_U8*)"ENABLE_HANDLE_EXCEPTIONS", 24 }, &_syms_serial_type_SYMS_U32, 0x1, 30 },
{ { (SYMS_U8*)"DISABLE_PROTDLLS", 16 }, &_syms_serial_type_SYMS_U32, 0x1, 31 },
};
SYMS_SerialFlag _syms_serial_members_for_SYMS_PeLoadConfigGuardFlags[] = {
{ { (SYMS_U8*)"CF_INSTRUMENTED", 15 }, &_syms_serial_type_SYMS_U32, 0x1, 8 },
{ { (SYMS_U8*)"CFW_INSTRUMENTED", 16 }, &_syms_serial_type_SYMS_U32, 0x1, 9 },
{ { (SYMS_U8*)"CF_FUNCTION_TABLE_PRESENT", 25 }, &_syms_serial_type_SYMS_U32, 0x1, 10 },
{ { (SYMS_U8*)"SECURITY_COOKIE_UNUSED", 22 }, &_syms_serial_type_SYMS_U32, 0x1, 11 },
{ { (SYMS_U8*)"PROTECT_DELAYLOAD_IAT", 21 }, &_syms_serial_type_SYMS_U32, 0x1, 12 },
{ { (SYMS_U8*)"DELAYLOAD_IAT_IN_ITS_OWN_SECTION", 32 }, &_syms_serial_type_SYMS_U32, 0x1, 13 },
{ { (SYMS_U8*)"CF_EXPORT_SUPPRESSION_INFO_PRESENT", 34 }, &_syms_serial_type_SYMS_U32, 0x1, 14 },
{ { (SYMS_U8*)"CF_ENABLE_EXPORT_SUPPRESSION", 28 }, &_syms_serial_type_SYMS_U32, 0x1, 15 },
{ { (SYMS_U8*)"CF_LONGJUMP_TABLE_PRESENT", 25 }, &_syms_serial_type_SYMS_U32, 0x1, 16 },
{ { (SYMS_U8*)"EH_CONTINUATION_TABLE_PRESENT", 29 }, &_syms_serial_type_SYMS_U32, 0x1, 22 },
{ { (SYMS_U8*)"CF_FUNCTION_TABLE_SIZE", 22 }, &_syms_serial_type_SYMS_U32, 0xf, 28 },
};

//~ generated from code at syms/metaprogram/syms_metaprogram_serial.c:1458
SYMS_SerialType _syms_serial_type_SYMS_DosHeader = {
{(SYMS_U8*)"DosHeader", 9}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_DosHeader), _syms_serial_members_for_SYMS_DosHeader, sizeof(SYMS_DosHeader), 0
};
SYMS_SerialType _syms_serial_type_SYMS_PeWindowsSubsystem = {
{(SYMS_U8*)"SYMS_PeWindowsSubsystem", 23}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_PeWindowsSubsystem), _syms_serial_members_for_SYMS_PeWindowsSubsystem, sizeof(SYMS_PeWindowsSubsystem), syms_enum_index_from_pe_windows_subsystem
};
SYMS_SerialType _syms_serial_type_SYMS_ImageFileCharacteristics = {
{(SYMS_U8*)"SYMS_ImageFileCharacteristics", 29}, SYMS_SerialTypeKind_Flags, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_ImageFileCharacteristics), _syms_serial_members_for_SYMS_ImageFileCharacteristics, sizeof(SYMS_ImageFileCharacteristics), 0
};
SYMS_SerialType _syms_serial_type_SYMS_DllCharacteristics = {
{(SYMS_U8*)"SYMS_DllCharacteristics", 23}, SYMS_SerialTypeKind_Flags, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_DllCharacteristics), _syms_serial_members_for_SYMS_DllCharacteristics, sizeof(SYMS_DllCharacteristics), 0
};
SYMS_SerialType _syms_serial_type_SYMS_PeOptionalHeader32 = {
{(SYMS_U8*)"PeOptionalHeader32", 18}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_PeOptionalHeader32), _syms_serial_members_for_SYMS_PeOptionalHeader32, sizeof(SYMS_PeOptionalHeader32), 0
};
SYMS_SerialType _syms_serial_type_SYMS_PeOptionalHeader32Plus = {
{(SYMS_U8*)"PeOptionalHeader32Plus", 22}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_PeOptionalHeader32Plus), _syms_serial_members_for_SYMS_PeOptionalHeader32Plus, sizeof(SYMS_PeOptionalHeader32Plus), 0
};
SYMS_SerialType _syms_serial_type_SYMS_PeDataDirectoryIndex = {
{(SYMS_U8*)"SYMS_PeDataDirectoryIndex", 25}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_PeDataDirectoryIndex), _syms_serial_members_for_SYMS_PeDataDirectoryIndex, sizeof(SYMS_PeDataDirectoryIndex), syms_enum_index_from_value_identity
};
SYMS_SerialType _syms_serial_type_SYMS_PeDataDirectory = {
{(SYMS_U8*)"PeDataDirectory", 15}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_PeDataDirectory), _syms_serial_members_for_SYMS_PeDataDirectory, sizeof(SYMS_PeDataDirectory), 0
};
SYMS_SerialType _syms_serial_type_SYMS_PeDebugDirectoryType = {
{(SYMS_U8*)"SYMS_PeDebugDirectoryType", 25}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_PeDebugDirectoryType), _syms_serial_members_for_SYMS_PeDebugDirectoryType, sizeof(SYMS_PeDebugDirectoryType), syms_enum_index_from_pe_debug_directory_type
};
SYMS_SerialType _syms_serial_type_SYMS_PeFPOFlags = {
{(SYMS_U8*)"SYMS_PeFPOFlags", 15}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_PeFPOFlags), _syms_serial_members_for_SYMS_PeFPOFlags, sizeof(SYMS_PeFPOFlags), syms_enum_index_from_pe_f_p_o_flags
};
SYMS_SerialType _syms_serial_type_SYMS_PeFPOEncoded = {
{(SYMS_U8*)"SYMS_PeFPOEncoded", 17}, SYMS_SerialTypeKind_Flags, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_PeFPOEncoded), _syms_serial_members_for_SYMS_PeFPOEncoded, sizeof(SYMS_PeFPOEncoded), 0
};
SYMS_SerialType _syms_serial_type_SYMS_PeFPOType = {
{(SYMS_U8*)"SYMS_PeFPOType", 14}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_PeFPOType), _syms_serial_members_for_SYMS_PeFPOType, sizeof(SYMS_PeFPOType), syms_enum_index_from_value_identity
};
SYMS_SerialType _syms_serial_type_SYMS_PeDebugMiscType = {
{(SYMS_U8*)"SYMS_PeDebugMiscType", 20}, SYMS_SerialTypeKind_Enum, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_PeDebugMiscType), _syms_serial_members_for_SYMS_PeDebugMiscType, sizeof(SYMS_PeDebugMiscType), syms_enum_index_from_value_identity
};
SYMS_SerialType _syms_serial_type_SYMS_PeDebugDirectory = {
{(SYMS_U8*)"PeDebugDirectory", 16}, SYMS_SerialTypeKind_Struct, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_PeDebugDirectory), _syms_serial_members_for_SYMS_PeDebugDirectory, sizeof(SYMS_PeDebugDirectory), 0
};
SYMS_SerialType _syms_serial_type_SYMS_PeGlobalFlags = {
{(SYMS_U8*)"SYMS_PeGlobalFlags", 18}, SYMS_SerialTypeKind_Flags, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_PeGlobalFlags), _syms_serial_members_for_SYMS_PeGlobalFlags, sizeof(SYMS_PeGlobalFlags), 0
};
SYMS_SerialType _syms_serial_type_SYMS_PeLoadConfigGuardFlags = {
{(SYMS_U8*)"SYMS_PeLoadConfigGuardFlags", 27}, SYMS_SerialTypeKind_Flags, SYMS_ARRAY_SIZE(_syms_serial_members_for_SYMS_PeLoadConfigGuardFlags), _syms_serial_members_for_SYMS_PeLoadConfigGuardFlags, sizeof(SYMS_PeLoadConfigGuardFlags), 0
};

#endif // defined(SYMS_ENABLE_PE_SERIAL_INFO)

#endif
