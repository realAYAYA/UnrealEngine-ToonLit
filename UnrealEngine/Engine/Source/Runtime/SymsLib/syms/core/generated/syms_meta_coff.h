// Copyright Epic Games, Inc. All Rights Reserved.
// generated
#ifndef _SYMS_META_COFF_H
#define _SYMS_META_COFF_H
//~ generated from code at syms/metaprogram/syms_metaprogram_serial.c:885
typedef SYMS_U16 SYMS_CoffFlags;
enum{
SYMS_CoffFlag_RELOC_STRIPPED = (1 << 0),
SYMS_CoffFlag_EXECUTABLE_IMAGE = (1 << 1),
SYMS_CoffFlag_LINE_NUMS_STRIPPED = (1 << 2),
SYMS_CoffFlag_SYM_STRIPPED = (1 << 3),
SYMS_CoffFlag_RESERVED_0 = (1 << 4),
SYMS_CoffFlag_LARGE_ADDRESS_AWARE = (1 << 5),
SYMS_CoffFlag_RESERVED_1 = (1 << 6),
SYMS_CoffFlag_RESERVED_2 = (1 << 7),
SYMS_CoffFlag_32BIT_MACHINE = (1 << 8),
SYMS_CoffFlag_DEBUG_STRIPPED = (1 << 9),
SYMS_CoffFlag_REMOVABLE_RUN_FROM_SWAP = (1 << 10),
SYMS_CoffFlag_NET_RUN_FROM_SWAP = (1 << 11),
SYMS_CoffFlag_SYSTEM = (1 << 12),
SYMS_CoffFlag_DLL = (1 << 13),
SYMS_CoffFlag_UP_SYSTEM_ONLY = (1 << 14),
SYMS_CoffFlag_BYTES_RESERVED_HI = (1 << 15),
};
typedef SYMS_U16 SYMS_CoffMachineType;
enum{
SYMS_CoffMachineType_UNKNOWN = 0x0,
SYMS_CoffMachineType_X86 = 0x14c,
SYMS_CoffMachineType_X64 = 0x8664,
SYMS_CoffMachineType_ARM33 = 0x1d3,
SYMS_CoffMachineType_ARM = 0x1c0,
SYMS_CoffMachineType_ARM64 = 0xaa64,
SYMS_CoffMachineType_ARMNT = 0x1c4,
SYMS_CoffMachineType_EBC = 0xebc,
SYMS_CoffMachineType_IA64 = 0x200,
SYMS_CoffMachineType_M32R = 0x9041,
SYMS_CoffMachineType_MIPS16 = 0x266,
SYMS_CoffMachineType_MIPSFPU = 0x366,
SYMS_CoffMachineType_MIPSFPU16 = 0x466,
SYMS_CoffMachineType_POWERPC = 0x1f0,
SYMS_CoffMachineType_POWERPCFP = 0x1f1,
SYMS_CoffMachineType_R4000 = 0x166,
SYMS_CoffMachineType_RISCV32 = 0x5032,
SYMS_CoffMachineType_RISCV64 = 0x5064,
SYMS_CoffMachineType_RISCV128 = 0x5128,
SYMS_CoffMachineType_SH3 = 0x1a2,
SYMS_CoffMachineType_SH3DSP = 0x1a3,
SYMS_CoffMachineType_SH4 = 0x1a6,
SYMS_CoffMachineType_SH5 = 0x1a8,
SYMS_CoffMachineType_THUMB = 0x1c2,
SYMS_CoffMachineType_WCEMIPSV2 = 0x169,
SYMS_CoffMachineType_COUNT = 25
};
#pragma pack(push,1)
typedef struct SYMS_CoffHeader{
SYMS_CoffMachineType machine;
SYMS_U16 section_count;
SYMS_U32 time_date_stamp;
//  TODO: rename to "unix_timestamp"
SYMS_U32 pointer_to_symbol_table;
SYMS_U32 number_of_symbols;
//  TODO: rename to "symbol_count"
SYMS_U16 size_of_optional_header;
//  TODO: rename to "optional_header_size"
SYMS_CoffFlags flags;
} SYMS_CoffHeader;
typedef SYMS_U32 SYMS_CoffSectionAlign;
enum{
SYMS_CoffSectionAlign_1BYTES = 0x1,
SYMS_CoffSectionAlign_2BYTES = 0x2,
SYMS_CoffSectionAlign_4BYTES = 0x3,
SYMS_CoffSectionAlign_8BYTES = 0x4,
SYMS_CoffSectionAlign_16BYTES = 0x5,
SYMS_CoffSectionAlign_32BYTES = 0x6,
SYMS_CoffSectionAlign_64BYTES = 0x7,
SYMS_CoffSectionAlign_128BYTES = 0x8,
SYMS_CoffSectionAlign_256BYTES = 0x9,
SYMS_CoffSectionAlign_512BYTES = 0xA,
SYMS_CoffSectionAlign_1024BYTES = 0xB,
SYMS_CoffSectionAlign_2048BYTES = 0xC,
SYMS_CoffSectionAlign_4096BYTES = 0xD,
SYMS_CoffSectionAlign_8192BYTES = 0xE,
SYMS_CoffSectionAlign_COUNT = 14
};
typedef SYMS_U32 SYMS_CoffSectionFlags;
enum{
SYMS_CoffSectionFlag_TYPE_NO_PAD = (1 << 3),
SYMS_CoffSectionFlag_CNT_CODE = (1 << 5),
SYMS_CoffSectionFlag_CNT_INITIALIZED_DATA = (1 << 6),
SYMS_CoffSectionFlag_CNT_UNINITIALIZED_DATA = (1 << 7),
SYMS_CoffSectionFlag_LNK_OTHER = (1 << 8),
SYMS_CoffSectionFlag_LNK_INFO = (1 << 9),
SYMS_CoffSectionFlag_LNK_REMOVE = (1 << 11),
SYMS_CoffSectionFlag_LNK_COMDAT = (1 << 12),
SYMS_CoffSectionFlag_GPREL = (1 << 15),
SYMS_CoffSectionFlag_MEM_16BIT = (1 << 17),
SYMS_CoffSectionFlag_MEM_LOCKED = (1 << 18),
SYMS_CoffSectionFlag_MEM_PRELOAD = (1 << 19),
SYMS_CoffSectionFlag_ALIGN_SHIFT = 20, SYMS_CoffSectionFlag_ALIGN_MASK = 0xf,
SYMS_CoffSectionFlag_LNK_NRELOC_OVFL = (1 << 24),
SYMS_CoffSectionFlag_MEM_DISCARDABLE = (1 << 25),
SYMS_CoffSectionFlag_MEM_NOT_CACHED = (1 << 26),
SYMS_CoffSectionFlag_MEM_NOT_PAGED = (1 << 27),
SYMS_CoffSectionFlag_MEM_SHARED = (1 << 28),
SYMS_CoffSectionFlag_MEM_EXECUTE = (1 << 29),
SYMS_CoffSectionFlag_MEM_READ = (1 << 30),
SYMS_CoffSectionFlag_MEM_WRITE = (1 << 31),
};
#define SYMS_CoffSectionFlags_Extract_ALIGN(f) (SYMS_CoffSectionAlign)(((f) >> SYMS_CoffSectionFlag_ALIGN_SHIFT) & SYMS_CoffSectionFlag_ALIGN_MASK)
typedef struct SYMS_CoffSectionHeader{
SYMS_U8 name[8];
SYMS_U32 virt_size;
SYMS_U32 virt_off;
SYMS_U32 file_size;
SYMS_U32 file_off;
SYMS_U32 relocs_file_off;
SYMS_U32 lines_file_off;
SYMS_U16 reloc_count;
SYMS_U16 line_count;
SYMS_CoffSectionFlags flags;
} SYMS_CoffSectionHeader;
typedef SYMS_U16 SYMS_CoffRelocTypeX64;
enum{
SYMS_CoffRelocTypeX64_ABS = 0x0,
SYMS_CoffRelocTypeX64_ADDR64 = 0x1,
SYMS_CoffRelocTypeX64_ADDR32 = 0x2,
SYMS_CoffRelocTypeX64_ADDR32NB = 0x3,
//  NB => No Base
SYMS_CoffRelocTypeX64_REL32 = 0x4,
SYMS_CoffRelocTypeX64_REL32_1 = 0x5,
SYMS_CoffRelocTypeX64_REL32_2 = 0x6,
SYMS_CoffRelocTypeX64_REL32_3 = 0x7,
SYMS_CoffRelocTypeX64_REL32_4 = 0x8,
SYMS_CoffRelocTypeX64_REL32_5 = 0x9,
SYMS_CoffRelocTypeX64_SECTION = 0xA,
SYMS_CoffRelocTypeX64_SECREL = 0xB,
SYMS_CoffRelocTypeX64_SECREL7 = 0xC,
//  TODO(nick): MSDN doesn't specify size for CLR token
SYMS_CoffRelocTypeX64_TOKEN = 0xD,
SYMS_CoffRelocTypeX64_SREL32 = 0xE,
//  TODO(nick): MSDN doesn't specify size for PAIR
SYMS_CoffRelocTypeX64_PAIR = 0xF,
SYMS_CoffRelocTypeX64_SSPAN32 = 0x10,
SYMS_CoffRelocTypeX64_COUNT = 17
};
typedef SYMS_U16 SYMS_CoffRelocTypeX86;
enum{
SYMS_CoffRelocTypeX86_ABS = 0x0,
//  relocation is ignored
SYMS_CoffRelocTypeX86_DIR16 = 0x1,
//  no support
SYMS_CoffRelocTypeX86_REL16 = 0x2,
//  no support
SYMS_CoffRelocTypeX86_UNKNOWN0 = 0x3,
SYMS_CoffRelocTypeX86_UNKNOWN2 = 0x4,
SYMS_CoffRelocTypeX86_UNKNOWN3 = 0x5,
SYMS_CoffRelocTypeX86_DIR32 = 0x6,
//  32-bit virtual address
SYMS_CoffRelocTypeX86_DIR32NB = 0x7,
//  32-bit virtual offset
SYMS_CoffRelocTypeX86_SEG12 = 0x9,
//  no support
SYMS_CoffRelocTypeX86_SECTION = 0xA,
//  16-bit section index, used for debug info purposes
SYMS_CoffRelocTypeX86_SECREL = 0xB,
//  32-bit offset from start of a section
SYMS_CoffRelocTypeX86_TOKEN = 0xC,
//  CLR token? (for managed languages)
SYMS_CoffRelocTypeX86_SECREL7 = 0xD,
//  7-bit offset from the base of the section that contains the target.
SYMS_CoffRelocTypeX86_UNKNOWN4 = 0xE,
SYMS_CoffRelocTypeX86_UNKNOWN5 = 0xF,
SYMS_CoffRelocTypeX86_UNKNOWN6 = 0x10,
SYMS_CoffRelocTypeX86_UNKNOWN7 = 0x11,
SYMS_CoffRelocTypeX86_UNKNOWN8 = 0x12,
SYMS_CoffRelocTypeX86_UNKNOWN9 = 0x13,
SYMS_CoffRelocTypeX86_REL32 = 0x14,
SYMS_CoffRelocTypeX86_COUNT = 20
};
typedef SYMS_U16 SYMS_CoffRelocTypeARM;
enum{
SYMS_CoffRelocTypeARM_ABS = 0x0,
SYMS_CoffRelocTypeARM_ADDR32 = 0x1,
SYMS_CoffRelocTypeARM_ADDR32NB = 0x2,
SYMS_CoffRelocTypeARM_BRANCH24 = 0x3,
SYMS_CoffRelocTypeARM_BRANCH11 = 0x4,
SYMS_CoffRelocTypeARM_UNKNOWN1 = 0x5,
SYMS_CoffRelocTypeARM_UNKNOWN2 = 0x6,
SYMS_CoffRelocTypeARM_UNKNOWN3 = 0x7,
SYMS_CoffRelocTypeARM_UNKNOWN4 = 0x8,
SYMS_CoffRelocTypeARM_UNKNOWN5 = 0x9,
SYMS_CoffRelocTypeARM_REL32 = 0xA,
SYMS_CoffRelocTypeARM_SECTION = 0xE,
SYMS_CoffRelocTypeARM_SECREL = 0xF,
SYMS_CoffRelocTypeARM_MOV32 = 0x10,
SYMS_CoffRelocTypeARM_THUMB_MOV32 = 0x11,
SYMS_CoffRelocTypeARM_THUMB_BRANCH20 = 0x12,
SYMS_CoffRelocTypeARM_UNUSED = 0x13,
SYMS_CoffRelocTypeARM_THUMB_BRANCH24 = 0x14,
SYMS_CoffRelocTypeARM_THUMB_BLX23 = 0x15,
SYMS_CoffRelocTypeARM_PAIR = 0x16,
SYMS_CoffRelocTypeARM_COUNT = 20
};
typedef SYMS_U16 SYMS_CoffRelocTypeARM64;
enum{
SYMS_CoffRelocTypeARM64_ABS = 0x0,
SYMS_CoffRelocTypeARM64_ADDR32 = 0x1,
SYMS_CoffRelocTypeARM64_ADDR32NB = 0x2,
SYMS_CoffRelocTypeARM64_BRANCH26 = 0x3,
SYMS_CoffRelocTypeARM64_PAGEBASE_REL21 = 0x4,
SYMS_CoffRelocTypeARM64_REL21 = 0x5,
SYMS_CoffRelocTypeARM64_PAGEOFFSET_12A = 0x6,
SYMS_CoffRelocTypeARM64_SECREL = 0x8,
SYMS_CoffRelocTypeARM64_SECREL_LOW12A = 0x9,
SYMS_CoffRelocTypeARM64_SECREL_HIGH12A = 0xA,
SYMS_CoffRelocTypeARM64_SECREL_LOW12L = 0xB,
SYMS_CoffRelocTypeARM64_TOKEN = 0xC,
SYMS_CoffRelocTypeARM64_SECTION = 0xD,
SYMS_CoffRelocTypeARM64_ADDR64 = 0xE,
SYMS_CoffRelocTypeARM64_BRANCH19 = 0xF,
SYMS_CoffRelocTypeARM64_BRANCH14 = 0x10,
SYMS_CoffRelocTypeARM64_REL32 = 0x11,
SYMS_CoffRelocTypeARM64_COUNT = 17
};
typedef SYMS_U8 SYMS_CoffSymType;
enum{
SYMS_CoffSymType_NULL,
SYMS_CoffSymType_VOID,
SYMS_CoffSymType_CHAR,
SYMS_CoffSymType_SHORT,
SYMS_CoffSymType_INT,
SYMS_CoffSymType_LONG,
SYMS_CoffSymType_FLOAT,
SYMS_CoffSymType_DOUBLE,
SYMS_CoffSymType_STRUCT,
SYMS_CoffSymType_UNION,
SYMS_CoffSymType_ENUM,
SYMS_CoffSymType_MOE,
//  member of enumeration
SYMS_CoffSymType_BYTE,
SYMS_CoffSymType_WORD,
SYMS_CoffSymType_UINT,
SYMS_CoffSymType_DWORD,
SYMS_CoffSymType_COUNT = 16
};
typedef SYMS_U8 SYMS_CoffSymStorageClass;
enum{
SYMS_CoffSymStorageClass_END_OF_FUNCTION = 0xff,
SYMS_CoffSymStorageClass_NULL = 0,
SYMS_CoffSymStorageClass_AUTOMATIC = 1,
SYMS_CoffSymStorageClass_EXTERNAL = 2,
SYMS_CoffSymStorageClass_STATIC = 3,
SYMS_CoffSymStorageClass_REGISTER = 4,
SYMS_CoffSymStorageClass_EXTERNAL_DEF = 5,
SYMS_CoffSymStorageClass_LABEL = 6,
SYMS_CoffSymStorageClass_UNDEFINED_LABEL = 7,
SYMS_CoffSymStorageClass_MEMBER_OF_STRUCT = 8,
SYMS_CoffSymStorageClass_ARGUMENT = 9,
SYMS_CoffSymStorageClass_STRUCT_TAG = 10,
SYMS_CoffSymStorageClass_MEMBER_OF_UNION = 11,
SYMS_CoffSymStorageClass_UNION_TAG = 12,
SYMS_CoffSymStorageClass_TYPE_DEFINITION = 13,
SYMS_CoffSymStorageClass_UNDEFINED_STATIC = 14,
SYMS_CoffSymStorageClass_ENUM_TAG = 15,
SYMS_CoffSymStorageClass_MEMBER_OF_ENUM = 16,
SYMS_CoffSymStorageClass_REGISTER_PARAM = 17,
SYMS_CoffSymStorageClass_BIT_FIELD = 18,
SYMS_CoffSymStorageClass_BLOCK = 100,
SYMS_CoffSymStorageClass_FUNCTION = 101,
SYMS_CoffSymStorageClass_END_OF_STRUCT = 102,
SYMS_CoffSymStorageClass_FILE = 103,
SYMS_CoffSymStorageClass_SECTION = 104,
SYMS_CoffSymStorageClass_WEAK_EXTERNAL = 105,
SYMS_CoffSymStorageClass_CLR_TOKEN = 107,
SYMS_CoffSymStorageClass_COUNT = 27
};
typedef SYMS_U16 SYMS_CoffSymSecNumber;
enum{
SYMS_CoffSymSecNumber_NUMBER_UNDEFINED = 0,
SYMS_CoffSymSecNumber_ABSOLUTE = 0xffff,
SYMS_CoffSymSecNumber_DEBUG = 0xfffe,
SYMS_CoffSymSecNumber_COUNT = 3
};
typedef SYMS_U8 SYMS_CoffSymDType;
enum{
SYMS_CoffSymDType_NULL = 0,
SYMS_CoffSymDType_PTR = 16,
SYMS_CoffSymDType_FUNC = 32,
SYMS_CoffSymDType_ARRAY = 48,
SYMS_CoffSymDType_COUNT = 4
};
typedef SYMS_U32 SYMS_CoffWeakExtType;
enum{
SYMS_CoffWeakExtType_NOLIBRARY = 1,
SYMS_CoffWeakExtType_SEARCH_LIBRARY = 2,
SYMS_CoffWeakExtType_SEARCH_ALIAS = 3,
SYMS_CoffWeakExtType_COUNT = 3
};
typedef SYMS_U32 SYMS_CoffImportHeaderType;
enum{
SYMS_CoffImportHeaderType_CODE = 0,
SYMS_CoffImportHeaderType_DATA = 1,
SYMS_CoffImportHeaderType_CONST = 2,
SYMS_CoffImportHeaderType_COUNT = 3
};
typedef SYMS_U32 SYMS_CoffImportHeaderNameType;
enum{
SYMS_CoffImportHeaderNameType_ORDINAL = 0,
SYMS_CoffImportHeaderNameType_NAME = 1,
SYMS_CoffImportHeaderNameType_NAME_NOPREFIX = 2,
SYMS_CoffImportHeaderNameType_UNDECORATE = 3,
SYMS_CoffImportHeaderNameType_COUNT = 4
};
typedef SYMS_U8 SYMS_CoffComdatSelectType;
enum{
SYMS_CoffComdatSelectType_NULL = 0,
//  Only one symbol is allowed to be in global symbol table, otherwise multiply defintion error is thrown.
SYMS_CoffComdatSelectType_NODUPLICATES = 1,
//  Select any symbol, even if there are multiple definitions. (we default to first declaration)
SYMS_CoffComdatSelectType_ANY = 2,
//  Sections that symbols reference must match in size, otherwise multiply definition error is thrown.
SYMS_CoffComdatSelectType_SAME_SIZE = 3,
//  Sections that symbols reference must have identical checksums, otherwise multiply defintion error is thrown.
SYMS_CoffComdatSelectType_EXACT_MATCH = 4,
//  Symbols with associative type form a chain of sections are related to each other. (next link is indicated in SYMS_CoffSecDef in 'number')
SYMS_CoffComdatSelectType_ASSOCIATIVE = 5,
//  Linker selects section with largest size.
SYMS_CoffComdatSelectType_LARGEST = 6,
SYMS_CoffComdatSelectType_COUNT = 7
};
#pragma pack(pop)

//~ generated from code at syms/metaprogram/syms_metaprogram_serial.c:1133
SYMS_C_LINKAGE_BEGIN
SYMS_API SYMS_Arch syms_arch_from_coff_machine_type(SYMS_CoffMachineType v);
SYMS_API SYMS_U32 syms_coff_reloc_size_for_x64(SYMS_CoffRelocTypeX64 v);
SYMS_API SYMS_U32 syms_coff_reloc_size_for_x86(SYMS_CoffRelocTypeX86 v);
SYMS_C_LINKAGE_END

//~ generated from code at syms/metaprogram/syms_metaprogram_serial.c:1588
SYMS_C_LINKAGE_BEGIN
SYMS_C_LINKAGE_END

//~ generated from code at syms/metaprogram/syms_metaprogram_serial.c:1694
#endif
