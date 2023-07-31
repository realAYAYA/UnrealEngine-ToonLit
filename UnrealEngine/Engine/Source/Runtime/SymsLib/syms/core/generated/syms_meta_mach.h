// Copyright Epic Games, Inc. All Rights Reserved.
// generated
#ifndef _SYMS_META_MACH_H
#define _SYMS_META_MACH_H
//~ generated from code at syms/metaprogram/syms_metaprogram_serial.c:885
typedef SYMS_S32 SYMS_MachCpuType;
enum{
SYMS_MachCpuType_ANY = 0xFFFFFFFF,
SYMS_MachCpuType_VAX = 0x1,
SYMS_MachCpuType_RESERVED2 = 0x2,
SYMS_MachCpuType_RESERVED3 = 0x3,
SYMS_MachCpuType_RESERVED4 = 0x4,
SYMS_MachCpuType_RESERVED5 = 0x5,
SYMS_MachCpuType_MC680x0 = 0x6,
SYMS_MachCpuType_X86 = 0x7,
SYMS_MachCpuType_I386 = SYMS_MachCpuType_X86,
SYMS_MachCpuType_X86_64 = 0x01000007,
SYMS_MachCpuType_RESERVED8 = 0x8,
SYMS_MachCpuType_RESERVED9 = 0x9,
SYMS_MachCpuType_MC98000 = 0xA,
SYMS_MachCpuType_HPPA = 0xB,
SYMS_MachCpuType_ARM = 0xC,
SYMS_MachCpuType_ARM64 = 0x0100000C,
SYMS_MachCpuType_MC88000 = 0xD,
SYMS_MachCpuType_SPARC = 0xE,
SYMS_MachCpuType_I860 = 0xF,
SYMS_MachCpuType_ALPHA = 0x10,
SYMS_MachCpuType_RESERVED17 = 0x11,
SYMS_MachCpuType_POWERPC = 0x12,
SYMS_MachCpuType_POWERPC64 = 0x01000012,
SYMS_MachCpuType_COUNT = 23
};
typedef SYMS_S32 SYMS_MachCpuFamily;
enum{
SYMS_MachCpuFamily_UNKNOWN = 0x0,
SYMS_MachCpuFamily_POWERPC_G3 = 0xCEE41549,
SYMS_MachCpuFamily_POWERPC_G4 = 0x77C184AE,
SYMS_MachCpuFamily_POWERPC_G5 = 0xED76D8AA,
SYMS_MachCpuFamily_INTEL_6_13 = 0xAA33392B,
SYMS_MachCpuFamily_INTEL_PENRYN = 0x78EA4FBC,
SYMS_MachCpuFamily_INTEL_NEHALEM = 0x6B5a4CD2,
SYMS_MachCpuFamily_INTEL_WESTMERE = 0x573B4EEC,
SYMS_MachCpuFamily_INTEL_SANDYBRIDGE = 0x5490B78C,
SYMS_MachCpuFamily_INTEL_IVYBRIDGE = 0x1F65E835,
SYMS_MachCpuFamily_INTEL_HASWELL = 0x10B282DC,
SYMS_MachCpuFamily_INTEL_BROADWELL = 0x582ED09C,
SYMS_MachCpuFamily_INTEL_SKYLAKE = 0x37FC219F,
SYMS_MachCpuFamily_INTEL_KABYLAKE = 0x0F817246,
SYMS_MachCpuFamily_ARM_9 = 0xE73283AE,
SYMS_MachCpuFamily_ARM_11 = 0x8FF620D8,
SYMS_MachCpuFamily_ARM_XSCALE = 0x53B005F5,
SYMS_MachCpuFamily_ARM_12 = 0xBD1B0AE9,
SYMS_MachCpuFamily_ARM_13 = 0x0CC90E64,
SYMS_MachCpuFamily_ARM_14 = 0x96077EF1,
SYMS_MachCpuFamily_ARM_15 = 0xA8511BCA,
SYMS_MachCpuFamily_ARM_SWIFT = 0x1E2D6381,
SYMS_MachCpuFamily_ARM_CYCLONE = 0x37A09642,
SYMS_MachCpuFamily_ARM_TYPHOON = 0x2C91A47E,
SYMS_MachCpuFamily_ARM_TWISTER = 0x92FB37C8,
SYMS_MachCpuFamily_ARM_HURRICANE = 0x67CEEE93,
//  deprecated
SYMS_MachCpuFamily_INTEL_6_23 = SYMS_MachCpuFamily_INTEL_PENRYN,
SYMS_MachCpuFamily_INTEL_6_26 = SYMS_MachCpuFamily_INTEL_NEHALEM,
SYMS_MachCpuFamily_COUNT = 28
};
typedef SYMS_U32 SYMS_MachCpuSubtype;
enum{
SYMS_MachCpuSubtype_ALL = 0,
SYMS_MachCpuSubtype_COUNT = 1
};
typedef SYMS_U32 SYMS_MachCpuSubtypeVAX;
enum{
SYMS_MachCpuSubtypeVAX_VAX_ALL = 0x0,
SYMS_MachCpuSubtypeVAX_VAX780 = 0x1,
SYMS_MachCpuSubtypeVAX_VAX785 = 0x2,
SYMS_MachCpuSubtypeVAX_VAX750 = 0x3,
SYMS_MachCpuSubtypeVAX_VAX730 = 0x4,
SYMS_MachCpuSubtypeVAX_UVAXI = 0x5,
SYMS_MachCpuSubtypeVAX_UVAXII = 0x6,
SYMS_MachCpuSubtypeVAX_VAX8200 = 0x7,
SYMS_MachCpuSubtypeVAX_VAX8500 = 0x8,
SYMS_MachCpuSubtypeVAX_VAX8600 = 0x9,
SYMS_MachCpuSubtypeVAX_VAX8650 = 0xA,
SYMS_MachCpuSubtypeVAX_VAX8800 = 0xB,
SYMS_MachCpuSubtypeVAX_UVAXIII = 0xC,
SYMS_MachCpuSubtypeVAX_COUNT = 13
};
typedef SYMS_U32 SYMS_MachCpuSubtypeX86;
enum{
SYMS_MachCpuSubtypeX86_X86_ALL = 0x3,
SYMS_MachCpuSubtypeX86_X86_64_ALL = SYMS_MachCpuSubtypeX86_X86_ALL,
SYMS_MachCpuSubtypeX86_X86_ARCH1 = 0x4,
SYMS_MachCpuSubtypeX86_X86_64_HASWELL = 0x8,
SYMS_MachCpuSubtypeX86_COUNT = 4
};
typedef SYMS_U32 SYMS_MachCpuSubtypeIntel;
enum{
SYMS_MachCpuSubtypeIntel_I386_ALL = 0x03,
SYMS_MachCpuSubtypeIntel_I386 = SYMS_MachCpuSubtypeIntel_I386_ALL,
SYMS_MachCpuSubtypeIntel_I486 = 0x04,
SYMS_MachCpuSubtypeIntel_I486SX = 0x84,
SYMS_MachCpuSubtypeIntel_I586 = 0x05,
SYMS_MachCpuSubtypeIntel_PENT = SYMS_MachCpuSubtypeIntel_I586,
SYMS_MachCpuSubtypeIntel_PENTPRO = 0x16,
SYMS_MachCpuSubtypeIntel_PENTII_M3 = 0x36,
SYMS_MachCpuSubtypeIntel_PENTII_M5 = 0x56,
SYMS_MachCpuSubtypeIntel_CELERON = 0x67,
SYMS_MachCpuSubtypeIntel_CELERON_MOBILE = 0x77,
SYMS_MachCpuSubtypeIntel_PENTIUM_3 = 0x08,
SYMS_MachCpuSubtypeIntel_PENTIUM_3_M = 0x18,
SYMS_MachCpuSubtypeIntel_PENTIUM_3_XENON = 0x28,
SYMS_MachCpuSubtypeIntel_PENTIUM_M = 0x09,
SYMS_MachCpuSubtypeIntel_PENTIUM_4 = 0x0A,
SYMS_MachCpuSubtypeIntel_PENTIUM_4_M = 0x1A,
SYMS_MachCpuSubtypeIntel_PENTIUM_ITANIUM = 0x0B,
SYMS_MachCpuSubtypeIntel_PENTIUM_ITANIUM_2 = 0x1B,
SYMS_MachCpuSubtypeIntel_XEON = 0x0C,
SYMS_MachCpuSubtypeIntel_XEON_MP = 0x1C,
SYMS_MachCpuSubtypeIntel_COUNT = 21
};
typedef SYMS_U32 SYMS_MachCpuSubtypeARM;
enum{
SYMS_MachCpuSubtypeARM_ALL = 0x0,
SYMS_MachCpuSubtypeARM_V4T = 0x5,
SYMS_MachCpuSubtypeARM_V6 = 0x6,
SYMS_MachCpuSubtypeARM_V5TEJ = 0x7,
SYMS_MachCpuSubtypeARM_XSCALE = 0x8,
SYMS_MachCpuSubtypeARM_V7 = 0x9,
SYMS_MachCpuSubtypeARM_V7F = 0xA,
SYMS_MachCpuSubtypeARM_V7S = 0xB,
SYMS_MachCpuSubtypeARM_V7K = 0xC,
SYMS_MachCpuSubtypeARM_V6M = 0xE,
SYMS_MachCpuSubtypeARM_V7M = 0xF,
SYMS_MachCpuSubtypeARM_V7EM = 0x10,
SYMS_MachCpuSubtypeARM_V8 = 0xD,
SYMS_MachCpuSubtypeARM_COUNT = 13
};
typedef SYMS_U32 SYMS_MachCpuSubtypeARM64;
enum{
SYMS_MachCpuSubtypeARM64_ALL = 0,
SYMS_MachCpuSubtypeARM64_V8 = 1,
SYMS_MachCpuSubtypeARM64_COUNT = 2
};
typedef SYMS_U32 SYMS_MachFiletype;
enum{
SYMS_MachFiletype_OBJECT = 0x1,
//  object file
SYMS_MachFiletype_EXECUTE = 0x2,
//  executable file
SYMS_MachFiletype_FVMLIB = 0x3,
//  fixed vm shared library file
SYMS_MachFiletype_CORE = 0x4,
SYMS_MachFiletype_PRELOAD = 0x5,
//  preloaded executable
SYMS_MachFiletype_DYLIB = 0x6,
//  dynamic library
SYMS_MachFiletype_DYLINKER = 0x7,
//  dynamic link editor
SYMS_MachFiletype_BUNDLE = 0x8,
//  executable file that program can load at runtime through dynamic linker
SYMS_MachFiletype_DYLIB_STUB = 0x9,
//  stub for linking with dynamic lib, contains info for linking only, no section data
SYMS_MachFiletype_DSYM = 0xA,
//  External debug symbols
SYMS_MachFiletype_KEXT_BUNDLE = 0xB,
SYMS_MachFiletype_COUNT = 11
};
typedef SYMS_U32 SYMS_MachFlags;
enum{
SYMS_MachFlags_NOUNDEFS = 0x1,
SYMS_MachFlags_INCRLINK = 0x2,
//  object was processed by linker and cannot be linked again
SYMS_MachFlags_DYLDLINK = 0x4,
//  object is input for dynamic linker and cannot be statically linked
SYMS_MachFlags_BINDATLOAD = 0x8,
//  undefined references are resolved at load
SYMS_MachFlags_PREBOUND = 0x10,
SYMS_MachFlags_SPLIT_SEGS = 0x20,
//  read-only and write-only segment is split
SYMS_MachFlags_LAZY_INIT = 0x40,
SYMS_MachFlags_TWOLEVEL = 0x80,
SYMS_MachFlags_FORCE_FLAT = 0x100,
SYMS_MachFlags_NOMULTIDEFS = 0x200,
SYMS_MachFlags_NOFIXPREBOUNDING = 0x400,
SYMS_MachFlags_PREBINDABLE = 0x800,
SYMS_MachFlags_ALLMODSBOUND = 0x1000,
SYMS_MachFlags_SUBSECTIONS_VIA_SYMBOLS = 0x2000,
SYMS_MachFlags_CANONICAL = 0x4000,
SYMS_MachFlags_WEAK_DEFINES = 0x8000,
SYMS_MachFlags_BINDS_TO_WEAK = 0x10000,
SYMS_MachFlags_ALLOW_STACK_EXECUTION = 0x20000,
SYMS_MachFlags_ROOT_SAFE = 0x40000,
SYMS_MachFlags_SETUID_SAFE = 0x80000,
SYMS_MachFlags_NO_REEXPORTED_DYLIBS = 0x100000,
SYMS_MachFlags_PIE = 0x200000,
SYMS_MachFlags_STRIPPABLE_DYLIB = 0x400000,
SYMS_MachFlags_HAS_TLV_DESRIPTORS = 0x800000,
SYMS_MachFlags_NO_HEAP_EXECUTION = 0x1000000,
SYMS_MachFlags_COUNT = 25
};
typedef SYMS_S32 SYMS_MachLoadCommandType;
enum{
SYMS_MachLoadCommandType_SEGMENT = 0x1,
SYMS_MachLoadCommandType_SYMTAB = 0x2,
SYMS_MachLoadCommandType_SYMSEG = 0x3,
//  gdb symbol table (obsolete)
SYMS_MachLoadCommandType_THREAD = 0x4,
SYMS_MachLoadCommandType_UNIXTHREAD = 0x5,
SYMS_MachLoadCommandType_LOADFVMLIB = 0x6,
SYMS_MachLoadCommandType_IDFVMLIB = 0x7,
SYMS_MachLoadCommandType_IDENT = 0x8,
//  object identification info (obsolete)
SYMS_MachLoadCommandType_FVMFILE = 0x9,
SYMS_MachLoadCommandType_PREPAGE = 0xA,
SYMS_MachLoadCommandType_DYSYMTAB = 0xB,
SYMS_MachLoadCommandType_LOAD_DYLIB = 0xC,
SYMS_MachLoadCommandType_ID_DYLIB = 0xD,
SYMS_MachLoadCommandType_LOAD_DYLINKER = 0xE,
SYMS_MachLoadCommandType_ID_DYLINKER = 0xF,
SYMS_MachLoadCommandType_PREBOUND_DYLIB = 0x10,
SYMS_MachLoadCommandType_ROUTINES = 0x11,
SYMS_MachLoadCommandType_SUB_FRAMEWORK = 0x12,
SYMS_MachLoadCommandType_SUB_UMBRELLA = 0x13,
SYMS_MachLoadCommandType_SUB_CLIENT = 0x14,
SYMS_MachLoadCommandType_SUB_LIBRARY = 0x15,
SYMS_MachLoadCommandType_TWOLEVEL_HINTS = 0x16,
SYMS_MachLoadCommandType_PREBIND_CHKSUM = 0x17,
SYMS_MachLoadCommandType_LOAD_WEAK_DYLIB = 0x80000018,
//  dylib is allowed to be missing
SYMS_MachLoadCommandType_SEGMENT_64 = 0x19,
SYMS_MachLoadCommandType_ROUTINES_64 = 0x1A,
SYMS_MachLoadCommandType_UUID = 0x1B,
SYMS_MachLoadCommandType_RPATH = 0x8000001C,
SYMS_MachLoadCommandType_CODE_SIGNATURE = 0x1D,
SYMS_MachLoadCommandType_SEGMENT_SPLIT_INFO = 0x1E,
SYMS_MachLoadCommandType_REEXPORT_DYLIB = 0x8000001F,
SYMS_MachLoadCommandType_LAZY_LOAD_DYLIB = 0x20,
SYMS_MachLoadCommandType_ENCRYPTION_INFO = 0x21,
SYMS_MachLoadCommandType_DYLD_INFO = 0x22,
SYMS_MachLoadCommandType_DYLD_INFO_ONLY = 0x80000022,
SYMS_MachLoadCommandType_LOAD_UPWARD_DYLIB = 0x80000023,
SYMS_MachLoadCommandType_VERSION_MIN_MACOSX = 0x24,
SYMS_MachLoadCommandType_VERSION_MIN_IPHONES = 0x25,
SYMS_MachLoadCommandType_FUNCTION_STARTS = 0x26,
SYMS_MachLoadCommandType_DYLD_ENVIORNMENT = 0x27,
SYMS_MachLoadCommandType_MAIN = 0x80000028,
SYMS_MachLoadCommandType_DATA_IN_CODE = 0x29,
SYMS_MachLoadCommandType_SOURCE_VERSION = 0x2A,
SYMS_MachLoadCommandType_DYLIB_CODE_SIGN_DRS = 0x2B,
SYMS_MachLoadCommandType_ENCRYPTION_INFO_64 = 0x2C,
SYMS_MachLoadCommandType_LINKER_OPTION = 0x2D,
SYMS_MachLoadCommandType_LINKER_OPTIMIZATION_HINT = 0x2E,
SYMS_MachLoadCommandType_VERSION_MIN_TVOS = 0x2F,
SYMS_MachLoadCommandType_VERSION_MIN_WATCHOS = 0x30,
SYMS_MachLoadCommandType_NOTE = 0x31,
SYMS_MachLoadCommandType_BUILD_VERSION = 0x32,
SYMS_MachLoadCommandType_COUNT = 51
};
typedef SYMS_U32 SYMS_MachSectionType;
enum{
SYMS_MachSectionType_REGULAR = 0x0,
SYMS_MachSectionType_ZEROFILL = 0x1,
SYMS_MachSectionType_CSTRING_LITERAL = 0x2,
SYMS_MachSectionType_FOUR_BYTE_LITERALS = 0x3,
//  TODO: number prefix
SYMS_MachSectionType_EIGHT_BYTE_LITERALS = 0x4,
//  TODO: number prefix
SYMS_MachSectionType_LITERAL_POINTERS = 0x5,
SYMS_MachSectionType_NON_LAZY_SYMBOL_POINTERS = 0x6,
SYMS_MachSectionType_LAZY_SYMBOL_POINTERS = 0x7,
SYMS_MachSectionType_SYMBOL_STUBS = 0x8,
SYMS_MachSectionType_MOD_INIT_FUNC_POINTERS = 0x9,
SYMS_MachSectionType_MOD_TERM_FUNC_POINTERS = 0xA,
SYMS_MachSectionType_COALESCED = 0xB,
SYMS_MachSectionType_GB_ZERO_FILL = 0xC,
SYMS_MachSectionType_INTERPOSING = 0xD,
SYMS_MachSectionType_SIXTEENBYTE_LITERALS = 0xE,
//  TODO: number prefix 
SYMS_MachSectionType_DTRACE_DOF = 0xF,
SYMS_MachSectionType_LAZY_DLIB_SYMBOL_POINTERS = 0x10,
SYMS_MachSectionType_THREAD_LOCAL_REGULAR = 0x11,
SYMS_MachSectionType_THREAD_LOCAL_ZEROFILL = 0x12,
SYMS_MachSectionType_THREAD_LOCAL_VARIABLES = 0x13,
SYMS_MachSectionType_THREAD_LOCAL_VARIABLES_POINTERS = 0x14,
SYMS_MachSectionType_LOCAL_INIT_FUNCTION_POINTERS = 0x15,
SYMS_MachSectionType_COUNT = 22
};
typedef SYMS_S32 SYMS_MachSectionAttr;
enum{
SYMS_MachSectionAttr_USR = 0xff000000,
SYMS_MachSectionAttr_SYSTEM = 0x00ffff00,
SYMS_MachSectionAttr_PURE_INSTRUCTIONS = 0x80000000,
SYMS_MachSectionAttr_NO_TOC = 0x40000000,
SYMS_MachSectionAttr_STRIP_STATIC_SYMS = 0x20000000,
SYMS_MachSectionAttr_NO_DEAD_STRIP = 0x10000000,
SYMS_MachSectionAttr_LIVE_SUPPORT = 0x08000000,
SYMS_MachSectionAttr_SELF_MODIFYING_CODE = 0x04000000,
SYMS_MachSectionAttr_DEBUG = 0x02000000,
SYMS_MachSectionAttr_SOME_INSTRUCTIONS = 0x00000400,
SYMS_MachSectionAttr_SECTION_RELOC = 0x00000200,
SYMS_MachSectionAttr_LOC_RELOC = 0x00000100,
SYMS_MachSectionAttr_COUNT = 12
};
typedef SYMS_U32 SYMS_MachPlatformType;
enum{
SYMS_MachPlatformType_MACOS = 0x1,
SYMS_MachPlatformType_IOS = 0x2,
SYMS_MachPlatformType_TVOS = 0x3,
SYMS_MachPlatformType_WATCHOS = 0x4,
SYMS_MachPlatformType_COUNT = 4
};
typedef SYMS_U32 SYMS_MachToolType;
enum{
SYMS_MachToolType_CLANG = 0x1,
SYMS_MachToolType_SWITFT = 0x2,
SYMS_MachToolType_LD = 0x3,
SYMS_MachToolType_COUNT = 3
};
typedef SYMS_U8 SYMS_MachBindType;
enum{
SYMS_MachBindType_POINTER = 0x1,
SYMS_MachBindType_TEXT_ABSOLUTE32 = 0x2,
SYMS_MachBindType_PCREL32 = 0x3,
SYMS_MachBindType_COUNT = 3
};
typedef SYMS_U8 SYMS_MachBindOpcode;
enum{
SYMS_MachBindOpcode_DONE = 0x00,
SYMS_MachBindOpcode_SET_DYLIB_ORDINAL_IMM = 0x10,
SYMS_MachBindOpcode_SET_DYLIB_ORDINAL_ULEB = 0x20,
SYMS_MachBindOpcode_SET_DYLIB_SPECIAL_IMM = 0x30,
SYMS_MachBindOpcode_SET_SYMBOL_TRAILING_FLAGS_IMM = 0x40,
SYMS_MachBindOpcode_SET_TYPE_IMM = 0x50,
SYMS_MachBindOpcode_SET_ADDEND_SLEB = 0x60,
SYMS_MachBindOpcode_SET_SEGMENT_AND_OFFSET_ULEB = 0x70,
SYMS_MachBindOpcode_ADD_ADDR_ULEB = 0x80,
SYMS_MachBindOpcode_DO_BIND = 0x90,
SYMS_MachBindOpcode_DO_BIND_ADD_ADDR_ULEB = 0xA0,
SYMS_MachBindOpcode_DO_BIND_ADD_ADDR_IMM_SCALED = 0xB0,
SYMS_MachBindOpcode_DO_BIND_ULEB_TIMES_SKIPPING_ULEB = 0xC0,
SYMS_MachBindOpcode_MASK = 0xF0,
SYMS_MachBindOpcode_IMM_MASK = 0xF,
SYMS_MachBindOpcode_COUNT = 15
};
typedef SYMS_U32 SYMS_MachBindSymbolFlags;
enum{
SYMS_MachBindSymbolFlags_WEAK_IMPORT = (1 << 0),
SYMS_MachBindSymbolFlags_NON_WEAK_DEFINITION = (1 << 3),
};
typedef SYMS_S8 SYMS_MachBindSpecialDylib;
enum{
SYMS_MachBindSpecialDylib_SELF = 0,
SYMS_MachBindSpecialDylib_MAIN_EXECUTABLE = 0xff,
SYMS_MachBindSpecialDylib_FLAT_LOOKUP = 0xfe,
SYMS_MachBindSpecialDylib_COUNT = 3
};
typedef SYMS_U32 SYMS_MachNListType;
enum{
SYMS_MachNListType_UNDF = 0x0,
SYMS_MachNListType_ABS = 0x2,
SYMS_MachNListType_SECT = 0xE,
SYMS_MachNListType_PBUD = 0xC,
SYMS_MachNListType_INDR = 0xA,
SYMS_MachNListType_COUNT = 5
};
typedef SYMS_U32 SYMS_MachStabType;
enum{
SYMS_MachStabType_GSYM = 0x20,
SYMS_MachStabType_FNAME = 0x22,
SYMS_MachStabType_FUN = 0x24,
SYMS_MachStabType_STSYM = 0x26,
SYMS_MachStabType_LCSYM = 0x28,
SYMS_MachStabType_BNSYM = 0x2E,
SYMS_MachStabType_AST = 0x32,
SYMS_MachStabType_OPT = 0x3C,
SYMS_MachStabType_RSYM = 0x40,
SYMS_MachStabType_SLINE = 0x44,
SYMS_MachStabType_ENSYM = 0x4E,
SYMS_MachStabType_SSYM = 0x60,
SYMS_MachStabType_SO = 0x64,
SYMS_MachStabType_OSO = 0x66,
SYMS_MachStabType_LSYM = 0x80,
SYMS_MachStabType_BINCL = 0x82,
SYMS_MachStabType_SOL = 0x84,
SYMS_MachStabType_PARAMS = 0x86,
SYMS_MachStabType_VERSION = 0x88,
SYMS_MachStabType_OLEVEL = 0x8A,
SYMS_MachStabType_PSYM = 0xA0,
SYMS_MachStabType_EINCL = 0xA2,
SYMS_MachStabType_ENTRY = 0xA4,
SYMS_MachStabType_LBRAC = 0xC0,
SYMS_MachStabType_EXCL = 0xC2,
SYMS_MachStabType_RBRAC = 0xE0,
SYMS_MachStabType_BCOMM = 0xE2,
SYMS_MachStabType_ECOMM = 0xE4,
SYMS_MachStabType_ECOML = 0xE8,
SYMS_MachStabType_LENG = 0xFE,
SYMS_MachStabType_COUNT = 30
};
typedef SYMS_U64 SYMS_MachExportSymbolKind;
enum{
SYMS_MachExportSymbolKind_REGULAR = 0,
SYMS_MachExportSymbolKind_THREAD_LOCAL = 1,
SYMS_MachExportSymbolKind_ABSOLUTE = 2,
SYMS_MachExportSymbolKind_COUNT = 3
};
typedef SYMS_U64 SYMS_MachExportSymbolFlags;
enum{
SYMS_MachExportSymbolFlags_KIND_MASK = (1 << 2),
SYMS_MachExportSymbolFlags_WEAK_DEFINITION = (1 << 2),
SYMS_MachExportSymbolFlags_REEXPORT = (1 << 3),
SYMS_MachExportSymbolFlags_STUB_AND_RESOLVED = (1 << 4),
};
#pragma pack(push,1)
typedef struct SYMS_MachLCStr{
SYMS_U32 offset;
} SYMS_MachLCStr;
typedef struct SYMS_MachUUID{
SYMS_U32 cmd;
SYMS_U32 cmdsize;
SYMS_U8 uuid[16];
} SYMS_MachUUID;
typedef struct SYMS_MachDylib{
SYMS_MachLCStr name;
SYMS_U32 timestamp;
SYMS_U32 current_version;
SYMS_U32 compatability_version;
} SYMS_MachDylib;
typedef struct SYMS_MachDylibCommand{
SYMS_U32 cmd;
SYMS_U32 cmdsize;
SYMS_MachDylib dylib;
} SYMS_MachDylibCommand;
typedef struct SYMS_MachDyldInfoCommand{
SYMS_U32 cmd;
SYMS_U32 cmdsize;
SYMS_U32 rebase_off;
SYMS_U32 rebase_size;
SYMS_U32 bind_off;
SYMS_U32 bind_size;
SYMS_U32 weak_bind_off;
SYMS_U32 weak_bind_size;
SYMS_U32 lazy_bind_off;
SYMS_U32 lazy_bind_size;
SYMS_U32 export_off;
SYMS_U32 export_size;
} SYMS_MachDyldInfoCommand;
typedef struct SYMS_MachLoadCommand{
SYMS_MachLoadCommandType type;
SYMS_U32 size;
} SYMS_MachLoadCommand;
typedef struct SYMS_MachFatHeader{
SYMS_U32 magic;
SYMS_U32 nfat_arch;
} SYMS_MachFatHeader;
typedef struct SYMS_MachFatArch{
SYMS_MachCpuType cputype;
SYMS_MachCpuSubtype cpusubtype;
SYMS_U32 offset;
SYMS_U32 size;
SYMS_U32 align;
} SYMS_MachFatArch;
typedef struct SYMS_MachHeader32{
SYMS_U32 magic;
SYMS_MachCpuType cputype;
SYMS_MachCpuSubtype cpusubtype;
SYMS_MachFiletype filetype;
SYMS_U32 ncmds;
SYMS_U32 sizeofcmds;
SYMS_MachFlags flags;
} SYMS_MachHeader32;
typedef struct SYMS_MachHeader64{
SYMS_U32 magic;
SYMS_MachCpuType cputype;
SYMS_MachCpuSubtype cpusubtype;
SYMS_MachFiletype filetype;
SYMS_U32 ncmds;
SYMS_U32 sizeofcmds;
SYMS_MachFlags flags;
SYMS_U32 reserved;
} SYMS_MachHeader64;
typedef struct SYMS_MachSegmentCommand32{
SYMS_MachLoadCommand cmd;
SYMS_U8 segname[16];
SYMS_U32 vmaddr;
SYMS_U32 vmsize;
SYMS_U32 fileoff;
SYMS_U32 filesize;
SYMS_MachVMProt maxprot;
SYMS_MachVMProt initprot;
SYMS_U32 nsects;
SYMS_U32 flags;
} SYMS_MachSegmentCommand32;
typedef struct SYMS_MachSegmentCommand64{
SYMS_MachLoadCommand cmd;
SYMS_U8 segname[16];
SYMS_U64 vmaddr;
SYMS_U64 vmsize;
SYMS_U64 fileoff;
SYMS_U64 filesize;
SYMS_MachVMProt maxprot;
SYMS_MachVMProt initprot;
SYMS_U32 nsects;
SYMS_U32 flags;
} SYMS_MachSegmentCommand64;
typedef struct SYMS_MachSection32{
SYMS_U8 sectname[16];
SYMS_U8 segname[16];
SYMS_U32 addr;
SYMS_U32 size;
SYMS_U32 offset;
SYMS_U32 align;
SYMS_U32 relocoff;
SYMS_U32 nreloc;
SYMS_U32 flags;
SYMS_U32 reserved1;
SYMS_U32 reserved2;
} SYMS_MachSection32;
typedef struct SYMS_MachSection64{
SYMS_U8 sectname[16];
SYMS_U8 segname[16];
SYMS_U64 addr;
SYMS_U64 size;
SYMS_U32 offset;
SYMS_U32 align;
SYMS_U32 relocoff;
SYMS_U32 nreloc;
SYMS_U32 flags;
SYMS_U32 reserved1;
SYMS_U32 reserved2;
SYMS_U32 pad;
} SYMS_MachSection64;
typedef struct SYMS_MachSymtabCommand{
SYMS_U32 cmd;
SYMS_U32 cmdsize;
SYMS_U32 symoff;
//  offset from the image base to array of SYMS_MachNList32
SYMS_U32 nsyms;
//  symbol count
SYMS_U32 stroff;
//  offset from the image base to string table
SYMS_U32 strsize;
} SYMS_MachSymtabCommand;
typedef struct SYMS_MachDySymtabCommand{
SYMS_U32 cmd;
SYMS_U32 cmdsize;
SYMS_U32 ilocalsym;
SYMS_U32 nlocalsym;
SYMS_U32 iextdefsym;
SYMS_U32 nextdefsym;
SYMS_U32 iundefsym;
SYMS_U32 nundefsym;
SYMS_U32 tocoff;
SYMS_U32 ntoc;
SYMS_U32 modtaboff;
SYMS_U32 nmodtab;
SYMS_U32 extrefsymoff;
SYMS_U32 nextrefsyms;
SYMS_U32 indirectsymoff;
SYMS_U32 nindirectsyms;
SYMS_U32 extreloff;
SYMS_U32 nextrel;
SYMS_U32 locreloff;
SYMS_U32 nlocrel;
} SYMS_MachDySymtabCommand;
typedef struct SYMS_MachNList32{
SYMS_U32 n_strx;
SYMS_U8 n_type;
SYMS_U8 n_sect;
SYMS_U16 n_desc;
SYMS_U32 n_value;
} SYMS_MachNList32;
typedef struct SYMS_MachNList64{
SYMS_U32 n_strx;
SYMS_U8 n_type;
SYMS_U8 n_sect;
SYMS_U16 n_desc;
SYMS_U64 n_value;
} SYMS_MachNList64;
typedef struct SYMS_MachBuildVersionCommand{
SYMS_U32 cmd;
SYMS_U32 cmdsize;
SYMS_MachPlatformType platform;
SYMS_U32 minos;
SYMS_U32 sdk;
SYMS_U32 ntools;
} SYMS_MachBuildVersionCommand;
typedef struct SYMS_MachBuildToolVersion{
SYMS_MachToolType tool;
SYMS_U32 version;
} SYMS_MachBuildToolVersion;
typedef struct SYMS_MachVersionMin{
SYMS_U32 cmd;
SYMS_U32 cmdsize;
SYMS_U32 version;
SYMS_U32 sdk;
} SYMS_MachVersionMin;
typedef struct SYMS_MachDylinker{
SYMS_U32 cmd;
SYMS_U32 cmdsize;
SYMS_MachLCStr name;
} SYMS_MachDylinker;
typedef struct SYMS_MachPreboundDylibCommand{
SYMS_U32 cmd;
SYMS_U32 cmdsize;
SYMS_MachLCStr name;
SYMS_U32 nmodules;
} SYMS_MachPreboundDylibCommand;
typedef struct SYMS_MachRoutinesCommand{
SYMS_U32 cmd;
SYMS_U32 cmdsize;
SYMS_U32 init_address;
SYMS_U32 init_module;
SYMS_U32 reserved1;
SYMS_U32 reserved2;
SYMS_U32 reserved3;
SYMS_U32 reserved4;
SYMS_U32 reserved5;
SYMS_U32 reserved6;
} SYMS_MachRoutinesCommand;
typedef struct SYMS_MachRoutines64Command{
SYMS_U32 cmd;
SYMS_U32 cmdsize;
SYMS_U64 init_address;
SYMS_U64 init_module;
SYMS_U64 reserved1;
SYMS_U64 reserved2;
SYMS_U64 reserved3;
SYMS_U64 reserved4;
SYMS_U64 reserved5;
SYMS_U64 reserved6;
} SYMS_MachRoutines64Command;
typedef struct SYMS_MachSubFrameworkCommand{
SYMS_U32 cmd;
SYMS_U32 cmdsize;
SYMS_MachLCStr umbrella;
} SYMS_MachSubFrameworkCommand;
typedef struct SYMS_MachSubUmbrellaCommand{
SYMS_U32 cmd;
SYMS_U32 cmdsize;
SYMS_MachLCStr sub_umbrella;
} SYMS_MachSubUmbrellaCommand;
typedef struct SYMS_MachSubClientCommand{
SYMS_U32 cmd;
SYMS_U32 cmdsize;
SYMS_MachLCStr client;
} SYMS_MachSubClientCommand;
typedef struct SYMS_MachSubLibraryCommand{
SYMS_U32 cmd;
SYMS_U32 cmdsize;
SYMS_MachLCStr sub_library;
} SYMS_MachSubLibraryCommand;
typedef struct SYMS_MachTwoLevelHintsCommand{
SYMS_U32 cmd;
SYMS_U32 cmdsize;
SYMS_U32 offset;
SYMS_U32 nhints;
} SYMS_MachTwoLevelHintsCommand;
typedef SYMS_U32 SYMS_MachTwoLevelHint;
enum{
SYMS_MachTwoLevelHint_isub_image_SHIFT = 0, SYMS_MachTwoLevelHint_isub_image_MASK = 0xff,
SYMS_MachTwoLevelHint_itoc_SHIFT = 8, SYMS_MachTwoLevelHint_itoc_MASK = 0xffffff,
};
#define SYMS_MachTwoLevelHint_Extract_isub_image(f) (SYMS_U32)(((f) >> SYMS_MachTwoLevelHint_isub_image_SHIFT) & SYMS_MachTwoLevelHint_isub_image_MASK)
#define SYMS_MachTwoLevelHint_Extract_itoc(f) (SYMS_U32)(((f) >> SYMS_MachTwoLevelHint_itoc_SHIFT) & SYMS_MachTwoLevelHint_itoc_MASK)
typedef struct SYMS_MachPrebindChecksumCommand{
SYMS_U32 cmd;
SYMS_U32 cmdsize;
SYMS_U32 chksum;
} SYMS_MachPrebindChecksumCommand;
typedef struct SYMS_MachRPathCommand{
SYMS_U32 cmd;
SYMS_U32 cmdsize;
SYMS_MachLCStr path;
} SYMS_MachRPathCommand;
typedef struct SYMS_MachLinkeditDataCommand{
SYMS_U32 cmd;
SYMS_U32 cmdsize;
SYMS_U32 dataoff;
SYMS_U32 datasize;
} SYMS_MachLinkeditDataCommand;
typedef struct SYMS_MachEncryptionInfoCommand{
SYMS_U32 cmd;
SYMS_U32 cmdsize;
SYMS_U32 cryptoff;
SYMS_U32 cryptsize;
SYMS_U32 cryptid;
} SYMS_MachEncryptionInfoCommand;
typedef struct SYMS_MachEncryptionInfo64Command{
SYMS_U32 cmd;
SYMS_U32 cmdsize;
SYMS_U32 cryptoff;
SYMS_U32 cryptsize;
SYMS_U32 cryptid;
SYMS_U32 pad;
} SYMS_MachEncryptionInfo64Command;
typedef struct SYMS_MachEntryPointCommand{
SYMS_U32 cmd;
SYMS_U32 cmdsize;
SYMS_U64 entryoff;
SYMS_U64 stacksize;
} SYMS_MachEntryPointCommand;
typedef struct SYMS_MachSourceVersionCommand{
SYMS_U32 cmd;
SYMS_U32 cmdsize;
SYMS_U64 version;
} SYMS_MachSourceVersionCommand;
typedef struct SYMS_MachLinkerOptionCommand{
SYMS_U32 cmd;
SYMS_U32 cmdsize;
SYMS_U32 count;
} SYMS_MachLinkerOptionCommand;
typedef struct SYMS_MachNoteCommand{
SYMS_U32 cmd;
SYMS_U32 cmdsize;
SYMS_U8 data_owner[16];
SYMS_U64 offset;
SYMS_U64 size;
} SYMS_MachNoteCommand;
typedef struct SYMS_MachSymSegCommand{
SYMS_U32 cmd;
SYMS_U32 cmdsize;
SYMS_U32 offset;
SYMS_U32 size;
} SYMS_MachSymSegCommand;
typedef struct SYMS_MachFvmlib{
SYMS_MachLCStr name;
SYMS_U32 minor_version;
SYMS_U32 header_addr;
} SYMS_MachFvmlib;
typedef struct SYMS_MachFvmlibCommand{
SYMS_U32 cmd;
SYMS_U32 cmdsize;
SYMS_MachFvmlib fvmlib;
} SYMS_MachFvmlibCommand;
typedef struct SYMS_MachThreadCommand{
SYMS_U32 cmd;
SYMS_U32 cmdsize;
} SYMS_MachThreadCommand;
typedef SYMS_U32 SYMS_MachUnwindEncModeX86;
enum{
SYMS_MachUnwindEncModeX86_EBP_FRAME = 1,
SYMS_MachUnwindEncModeX86_STACK_IMMD = 2,
SYMS_MachUnwindEncModeX86_STACK_IND = 3,
SYMS_MachUnwindEncModeX86_DWARF = 4,
SYMS_MachUnwindEncModeX86_COUNT = 4
};
typedef SYMS_U32 SYMS_MachUnwindRegisterX86;
enum{
SYMS_MachUnwindRegisterX86_NONE = 0,
SYMS_MachUnwindRegisterX86_EBX = 1,
SYMS_MachUnwindRegisterX86_ECX = 2,
SYMS_MachUnwindRegisterX86_EDX = 3,
SYMS_MachUnwindRegisterX86_EDI = 4,
SYMS_MachUnwindRegisterX86_ESI = 5,
SYMS_MachUnwindRegisterX86_EBP = 6,
SYMS_MachUnwindRegisterX86_COUNT = 7
};
typedef SYMS_U32 SYMS_MachUnwindEncX86;
enum{
SYMS_MachUnwindEncX86_MODE_MASK_SHIFT = 4, SYMS_MachUnwindEncX86_MODE_MASK_MASK = 0xffffff,
SYMS_MachUnwindEncX86_EBP_FRAME_REGISTER = (1 << 15),
SYMS_MachUnwindEncX86_EBP_FRAME_OFFSET_SHIFT = 8, SYMS_MachUnwindEncX86_EBP_FRAME_OFFSET_MASK = 0x7fff,
SYMS_MachUnwindEncX86_FRAMELESS_STACK_SIZE_SHIFT = 8, SYMS_MachUnwindEncX86_FRAMELESS_STACK_SIZE_MASK = 0x7fff,
SYMS_MachUnwindEncX86_FRAMELESS_STACK_ADJUST_SHIFT = 3, SYMS_MachUnwindEncX86_FRAMELESS_STACK_ADJUST_MASK = 0xfff,
SYMS_MachUnwindEncX86_FRAMELESS_REG_COUNT_SHIFT = 3, SYMS_MachUnwindEncX86_FRAMELESS_REG_COUNT_MASK = 0x1ff,
SYMS_MachUnwindEncX86_FRAMELESS_REG_PERMUTATION = (1 << 10),
SYMS_MachUnwindEncX86_DWARF_SECTION_OFFSET = (1 << 24),
};
#define SYMS_MachUnwindEncX86_Extract_MODE_MASK(f) (SYMS_MachUnwindEncModeX86)(((f) >> SYMS_MachUnwindEncX86_MODE_MASK_SHIFT) & SYMS_MachUnwindEncX86_MODE_MASK_MASK)
#define SYMS_MachUnwindEncX86_Extract_EBP_FRAME_OFFSET(f) (SYMS_U32)(((f) >> SYMS_MachUnwindEncX86_EBP_FRAME_OFFSET_SHIFT) & SYMS_MachUnwindEncX86_EBP_FRAME_OFFSET_MASK)
#define SYMS_MachUnwindEncX86_Extract_FRAMELESS_STACK_SIZE(f) (SYMS_U32)(((f) >> SYMS_MachUnwindEncX86_FRAMELESS_STACK_SIZE_SHIFT) & SYMS_MachUnwindEncX86_FRAMELESS_STACK_SIZE_MASK)
#define SYMS_MachUnwindEncX86_Extract_FRAMELESS_STACK_ADJUST(f) (SYMS_U32)(((f) >> SYMS_MachUnwindEncX86_FRAMELESS_STACK_ADJUST_SHIFT) & SYMS_MachUnwindEncX86_FRAMELESS_STACK_ADJUST_MASK)
#define SYMS_MachUnwindEncX86_Extract_FRAMELESS_REG_COUNT(f) (SYMS_U32)(((f) >> SYMS_MachUnwindEncX86_FRAMELESS_REG_COUNT_SHIFT) & SYMS_MachUnwindEncX86_FRAMELESS_REG_COUNT_MASK)
typedef SYMS_U32 SYMS_MachUnwindEncModeX64;
enum{
SYMS_MachUnwindEncModeX64_RBP_FRAME = 1,
SYMS_MachUnwindEncModeX64_STACK_IMMD = 2,
SYMS_MachUnwindEncModeX64_STACK_IND = 3,
SYMS_MachUnwindEncModeX64_DWARF = 4,
SYMS_MachUnwindEncModeX64_COUNT = 4
};
typedef SYMS_U32 SYMS_MachUnwindRegisterX64;
enum{
SYMS_MachUnwindRegisterX64_NONE = 0,
SYMS_MachUnwindRegisterX64_RBX = 1,
SYMS_MachUnwindRegisterX64_R12 = 2,
SYMS_MachUnwindRegisterX64_R13 = 3,
SYMS_MachUnwindRegisterX64_R14 = 4,
SYMS_MachUnwindRegisterX64_R15 = 5,
SYMS_MachUnwindRegisterX64_RBP = 6,
SYMS_MachUnwindRegisterX64_COUNT = 7
};
#pragma pack(pop)

//~ generated from code at syms/metaprogram/syms_metaprogram_serial.c:1133
SYMS_C_LINKAGE_BEGIN
SYMS_API SYMS_Arch syms_mach_arch_from_cputype(SYMS_MachCpuType v);
SYMS_C_LINKAGE_END

//~ generated from code at syms/metaprogram/syms_metaprogram_serial.c:1588
SYMS_C_LINKAGE_BEGIN
SYMS_API void syms_bswap_in_place__SYMS_MachLCStr(SYMS_MachLCStr *v);
SYMS_API void syms_bswap_in_place__SYMS_MachUUID(SYMS_MachUUID *v);
SYMS_API void syms_bswap_in_place__SYMS_MachDylib(SYMS_MachDylib *v);
SYMS_API void syms_bswap_in_place__SYMS_MachDylibCommand(SYMS_MachDylibCommand *v);
SYMS_API void syms_bswap_in_place__SYMS_MachDyldInfoCommand(SYMS_MachDyldInfoCommand *v);
SYMS_API void syms_bswap_in_place__SYMS_MachLoadCommand(SYMS_MachLoadCommand *v);
SYMS_API void syms_bswap_in_place__SYMS_MachFatHeader(SYMS_MachFatHeader *v);
SYMS_API void syms_bswap_in_place__SYMS_MachFatArch(SYMS_MachFatArch *v);
SYMS_API void syms_bswap_in_place__SYMS_MachHeader32(SYMS_MachHeader32 *v);
SYMS_API void syms_bswap_in_place__SYMS_MachHeader64(SYMS_MachHeader64 *v);
SYMS_API void syms_bswap_in_place__SYMS_MachSegmentCommand32(SYMS_MachSegmentCommand32 *v);
SYMS_API void syms_bswap_in_place__SYMS_MachSegmentCommand64(SYMS_MachSegmentCommand64 *v);
SYMS_API void syms_bswap_in_place__SYMS_MachSection32(SYMS_MachSection32 *v);
SYMS_API void syms_bswap_in_place__SYMS_MachSection64(SYMS_MachSection64 *v);
SYMS_API void syms_bswap_in_place__SYMS_MachSymtabCommand(SYMS_MachSymtabCommand *v);
SYMS_API void syms_bswap_in_place__SYMS_MachDySymtabCommand(SYMS_MachDySymtabCommand *v);
SYMS_API void syms_bswap_in_place__SYMS_MachNList32(SYMS_MachNList32 *v);
SYMS_API void syms_bswap_in_place__SYMS_MachNList64(SYMS_MachNList64 *v);
SYMS_API void syms_bswap_in_place__SYMS_MachBuildVersionCommand(SYMS_MachBuildVersionCommand *v);
SYMS_API void syms_bswap_in_place__SYMS_MachBuildToolVersion(SYMS_MachBuildToolVersion *v);
SYMS_API void syms_bswap_in_place__SYMS_MachVersionMin(SYMS_MachVersionMin *v);
SYMS_API void syms_bswap_in_place__SYMS_MachDylinker(SYMS_MachDylinker *v);
SYMS_API void syms_bswap_in_place__SYMS_MachPreboundDylibCommand(SYMS_MachPreboundDylibCommand *v);
SYMS_API void syms_bswap_in_place__SYMS_MachRoutinesCommand(SYMS_MachRoutinesCommand *v);
SYMS_API void syms_bswap_in_place__SYMS_MachRoutines64Command(SYMS_MachRoutines64Command *v);
SYMS_API void syms_bswap_in_place__SYMS_MachSubFrameworkCommand(SYMS_MachSubFrameworkCommand *v);
SYMS_API void syms_bswap_in_place__SYMS_MachSubUmbrellaCommand(SYMS_MachSubUmbrellaCommand *v);
SYMS_API void syms_bswap_in_place__SYMS_MachSubClientCommand(SYMS_MachSubClientCommand *v);
SYMS_API void syms_bswap_in_place__SYMS_MachSubLibraryCommand(SYMS_MachSubLibraryCommand *v);
SYMS_API void syms_bswap_in_place__SYMS_MachTwoLevelHintsCommand(SYMS_MachTwoLevelHintsCommand *v);
SYMS_API void syms_bswap_in_place__SYMS_MachPrebindChecksumCommand(SYMS_MachPrebindChecksumCommand *v);
SYMS_API void syms_bswap_in_place__SYMS_MachRPathCommand(SYMS_MachRPathCommand *v);
SYMS_API void syms_bswap_in_place__SYMS_MachLinkeditDataCommand(SYMS_MachLinkeditDataCommand *v);
SYMS_API void syms_bswap_in_place__SYMS_MachEncryptionInfoCommand(SYMS_MachEncryptionInfoCommand *v);
SYMS_API void syms_bswap_in_place__SYMS_MachEncryptionInfo64Command(SYMS_MachEncryptionInfo64Command *v);
SYMS_API void syms_bswap_in_place__SYMS_MachEntryPointCommand(SYMS_MachEntryPointCommand *v);
SYMS_API void syms_bswap_in_place__SYMS_MachSourceVersionCommand(SYMS_MachSourceVersionCommand *v);
SYMS_API void syms_bswap_in_place__SYMS_MachLinkerOptionCommand(SYMS_MachLinkerOptionCommand *v);
SYMS_API void syms_bswap_in_place__SYMS_MachNoteCommand(SYMS_MachNoteCommand *v);
SYMS_API void syms_bswap_in_place__SYMS_MachSymSegCommand(SYMS_MachSymSegCommand *v);
SYMS_API void syms_bswap_in_place__SYMS_MachFvmlib(SYMS_MachFvmlib *v);
SYMS_API void syms_bswap_in_place__SYMS_MachFvmlibCommand(SYMS_MachFvmlibCommand *v);
SYMS_API void syms_bswap_in_place__SYMS_MachThreadCommand(SYMS_MachThreadCommand *v);
SYMS_C_LINKAGE_END

//~ generated from code at syms/metaprogram/syms_metaprogram_serial.c:1694
#endif
