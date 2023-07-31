// Copyright Epic Games, Inc. All Rights Reserved.
// generated
#ifndef _SYMS_META_ELF_H
#define _SYMS_META_ELF_H
//~ generated from code at syms/metaprogram/syms_metaprogram_serial.c:885
typedef SYMS_U8 SYMS_ElfClass;
enum{
SYMS_ElfClass_None = 0,
SYMS_ElfClass_32 = 1,
SYMS_ElfClass_64 = 2,
SYMS_ElfClass_COUNT = 3
};
typedef SYMS_U8 SYMS_ElfOsAbi;
enum{
SYMS_ElfOsAbi_NONE,
SYMS_ElfOsAbi_SYSV,
SYMS_ElfOsAbi_HPUX,
SYMS_ElfOsAbi_NETBSD,
SYMS_ElfOsAbi_GNU,
SYMS_ElfOsAbi_LINUX,
SYMS_ElfOsAbi_SOLARIS,
SYMS_ElfOsAbi_IRIX,
SYMS_ElfOsAbi_FREEBSD,
SYMS_ElfOsAbi_TRU64,
SYMS_ElfOsAbi_ARM = 97,
SYMS_ElfOsAbi_STANDALONE = 255,
SYMS_ElfOsAbi_COUNT = 12
};
typedef SYMS_U8 SYMS_ElfVersion;
enum{
SYMS_ElfVersion_NONE,
SYMS_ElfVersion_CURRENT,
SYMS_ElfVersion_COUNT = 2
};
typedef SYMS_U16 SYMS_ElfMachineKind;
enum{
SYMS_ElfMachineKind_NONE = 0,
SYMS_ElfMachineKind_M32 = 1,
SYMS_ElfMachineKind_SPARC = 2,
SYMS_ElfMachineKind_386 = 3,
SYMS_ElfMachineKind_68K = 4,
SYMS_ElfMachineKind_88K = 5,
SYMS_ElfMachineKind_IAMCU = 6,
SYMS_ElfMachineKind_860 = 7,
SYMS_ElfMachineKind_MIPS = 8,
SYMS_ElfMachineKind_S370 = 9,
SYMS_ElfMachineKind_MIPS_RS3_LE = 10,
//  11-14 reserved
SYMS_ElfMachineKind_PARISC = 15,
//  16 reserved
SYMS_ElfMachineKind_VPP500 = 17,
SYMS_ElfMachineKind_SPARC32PLUS = 18,
//  nick: Sun's "v8plus"
SYMS_ElfMachineKind_INTEL960 = 19,
SYMS_ElfMachineKind_PPC = 20,
SYMS_ElfMachineKind_PPC64 = 21,
SYMS_ElfMachineKind_S390 = 22,
SYMS_ElfMachineKind_SPU = 23,
//  24-35 reserved
SYMS_ElfMachineKind_V800 = 36,
SYMS_ElfMachineKind_FR20 = 37,
SYMS_ElfMachineKind_RH32 = 38,
SYMS_ElfMachineKind_MCORE = 39,
SYMS_ElfMachineKind_ARM = 40,
SYMS_ElfMachineKind_SH = 42,
SYMS_ElfMachineKind_ALPHA = 41,
SYMS_ElfMachineKind_SPARCV9 = 43,
SYMS_ElfMachineKind_TRICORE = 44,
SYMS_ElfMachineKind_ARC = 45,
SYMS_ElfMachineKind_H8_300 = 46,
SYMS_ElfMachineKind_H8_300H = 47,
SYMS_ElfMachineKind_H8S = 48,
SYMS_ElfMachineKind_H8_500 = 49,
SYMS_ElfMachineKind_IA_64 = 50,
SYMS_ElfMachineKind_MIPS_X = 51,
SYMS_ElfMachineKind_COLDFILE = 52,
SYMS_ElfMachineKind_68HC12 = 53,
SYMS_ElfMachineKind_MMA = 54,
SYMS_ElfMachineKind_PCP = 55,
SYMS_ElfMachineKind_NCPU = 56,
SYMS_ElfMachineKind_NDR1 = 57,
SYMS_ElfMachineKind_STARCORE = 58,
SYMS_ElfMachineKind_ME16 = 59,
SYMS_ElfMachineKind_ST100 = 60,
SYMS_ElfMachineKind_TINYJ = 61,
SYMS_ElfMachineKind_X86_64 = 62,
SYMS_ElfMachineKind_AARCH64 = 183,
SYMS_ElfMachineKind_TI_C6000 = 140,
SYMS_ElfMachineKind_L1OM = 180,
SYMS_ElfMachineKind_K1OM = 181,
SYMS_ElfMachineKind_RISCV = 243,
SYMS_ElfMachineKind_S390_OLD = 0xA390,
SYMS_ElfMachineKind_COUNT = 52
};
typedef SYMS_U16 SYMS_ElfType;
enum{
SYMS_ElfType_NONE,
SYMS_ElfType_REL,
SYMS_ElfType_EXEC,
SYMS_ElfType_DYN,
SYMS_ElfType_CORE,
SYMS_ElfType_COUNT = 5
};
typedef SYMS_U8 SYMS_ElfData;
enum{
SYMS_ElfData_None = 0,
SYMS_ElfData_2LSB = 1,
SYMS_ElfData_2MSB = 2,
SYMS_ElfData_COUNT = 3
};
typedef SYMS_U32 SYMS_ElfPKind;
enum{
//  TODO(nick): This is actually PType
SYMS_ElfPKind_Null = 0,
SYMS_ElfPKind_Load = 1,
SYMS_ElfPKind_Dynamic = 2,
SYMS_ElfPKind_Interp = 3,
SYMS_ElfPKind_Note = 4,
SYMS_ElfPKind_SHLib = 5,
SYMS_ElfPKind_PHDR = 6,
SYMS_ElfPKind_TLS = 7,
SYMS_ElfPKind_LOOS = 0x60000000,
SYMS_ElfPKind_HIOS = 0x6fffffff,
SYMS_ElfPKind_LowProc = 0x70000000,
SYMS_ElfPKind_HighProc = 0x7fffffff,
// - specific to Sun
SYMS_ElfPKind_LowSunW = 0x6ffffffa,
SYMS_ElfPKind_SunWBSS = 0x6ffffffb,
SYMS_ElfPKind_GnuEHFrame = 0x6474E550,
//  SYMS_ElfPKind_LOOS + 0x474e550, // frame unwind information
SYMS_ElfPKind_GnuStack = 0x6474E551,
//  SYMS_ElfPKind_LOOS + 0x474e551, // stack flags
SYMS_ElfPKind_GnuRelro = 0x6474E552,
//  SYMS_ElfPKind_LOOS + 0x474e552, // read-only after relocations
SYMS_ElfPKind_GnuProperty = 0x6474E553,
SYMS_ElfPKind_SunEHFrame = SYMS_ElfPKind_GnuEHFrame,
SYMS_ElfPKind_COUNT = 19
};
typedef SYMS_U32 SYMS_ElfPFlag;
enum{
SYMS_ElfPFlag_Exec = (1 << 0),
SYMS_ElfPFlag_Write = (1 << 1),
SYMS_ElfPFlag_Read = (1 << 2),
};
typedef SYMS_U32 SYMS_ElfSectionCode;
enum{
SYMS_ElfSectionCode_NULL = 0,
SYMS_ElfSectionCode_PROGBITS = 1,
SYMS_ElfSectionCode_SYMTAB = 2,
SYMS_ElfSectionCode_STRTAB = 3,
SYMS_ElfSectionCode_RELA = 4,
SYMS_ElfSectionCode_HASH = 5,
SYMS_ElfSectionCode_DYNAMIC = 6,
SYMS_ElfSectionCode_NOTE = 7,
SYMS_ElfSectionCode_NOBITS = 8,
SYMS_ElfSectionCode_REL = 9,
SYMS_ElfSectionCode_SHLIB = 10,
SYMS_ElfSectionCode_DYNSYM = 11,
SYMS_ElfSectionCode_INIT_ARRAY = 14,
//  nick: Array of ptrs to init functions
SYMS_ElfSectionCode_FINI_ARRAY = 15,
//  nick: Array of ptrs to finish functions
SYMS_ElfSectionCode_PREINIT_ARRAY = 16,
//  nick: Array of ptrs to pre-init funcs
SYMS_ElfSectionCode_GROUP = 17,
//  nick: Section contains a section group
SYMS_ElfSectionCode_SYMTAB_SHNDX = 18,
//  nick: Indices for SHN_XINDEX entries
SYMS_ElfSectionCode_GNU_INCREMENTAL_INPUTS = 0x6fff4700,
//  nick: incremental build data
SYMS_ElfSectionCode_GNU_ATTRIBUTES = 0x6ffffff5,
//  nick: Object attributes
SYMS_ElfSectionCode_GNU_HASH = 0x6ffffff6,
//  nick: GNU style symbol hash table
SYMS_ElfSectionCode_GNU_LIBLIST = 0x6ffffff7,
//  versions.
SYMS_ElfSectionCode_SUNW_verdef = 0x6ffffffd,
//  nick: Versions defined by file
SYMS_ElfSectionCode_SUNW_verneed = 0x6ffffffe,
//  nick: Versions needed by file
SYMS_ElfSectionCode_SUNW_versym = 0x6fffffff,
//  nick: Symbol versions
SYMS_ElfSectionCode_GNU_verdef = SYMS_ElfSectionCode_SUNW_verdef,
SYMS_ElfSectionCode_GNU_verneed = SYMS_ElfSectionCode_SUNW_verneed,
SYMS_ElfSectionCode_GNU_versym = SYMS_ElfSectionCode_SUNW_versym,
SYMS_ElfSectionCode_PROC,
SYMS_ElfSectionCode_USER,
SYMS_ElfSectionCode_COUNT = 29
};
typedef SYMS_U32 SYMS_ElfSectionIndex;
enum{
//  Symbol with section index is undefined and must be resolved by the link editor.
SYMS_ElfSectionIndex_UNDEF = 0,
//  Symbol has absolute value and wont change after relocations.
SYMS_ElfSectionIndex_ABS = 0xfff1,
//  This symbol indicates to linker to allocate the storage at address multiple of st_value.
SYMS_ElfSectionIndex_COMMON = 0xfff2,
SYMS_ElfSectionIndex_LO_RESERVE = 0xff00,
SYMS_ElfSectionIndex_HI_RESERVE = 0xffff,
//  Processor specific
SYMS_ElfSectionIndex_LO_PROC = SYMS_ElfSectionIndex_LO_RESERVE,
SYMS_ElfSectionIndex_HI_PROC = 0xff1f,
//  Reserved for OS
SYMS_ElfSectionIndex_LO_OS = 0xff20,
SYMS_ElfSectionIndex_HI_OS = 0xff3f,
SYMS_ElfSectionIndex_IA_64_ASNI_COMMON = SYMS_ElfSectionIndex_LO_PROC,
SYMS_ElfSectionIndex_X86_64_LCOMMON = 0xff02,
SYMS_ElfSectionIndex_MIPS_SCOMMON = 0xff03,
SYMS_ElfSectionIndex_TIC6X_COMMON = SYMS_ElfSectionIndex_LO_RESERVE,
SYMS_ElfSectionIndex_MIPS_SUNDEFINED = 0xff04,
SYMS_ElfSectionIndex_COUNT = 14
};
typedef SYMS_U32 SYMS_ElfSectionFlag;
enum{
SYMS_ElfSectionFlag_WRITE = (1 << 0),
SYMS_ElfSectionFlag_ALLOC = (1 << 1),
SYMS_ElfSectionFlag_EXECINSTR = (1 << 2),
SYMS_ElfSectionFlag_MERGE = (1 << 4),
SYMS_ElfSectionFlag_STRINGS = (1 << 5),
SYMS_ElfSectionFlag_INFO_LINK = (1 << 6),
SYMS_ElfSectionFlag_LINK_ORDER = (1 << 7),
SYMS_ElfSectionFlag_OS_NONCONFORMING = (1 << 8),
SYMS_ElfSectionFlag_GROUP = (1 << 9),
SYMS_ElfSectionFlag_TLS = (1 << 10),
SYMS_ElfSectionFlag_MASKOS_SHIFT = 16, SYMS_ElfSectionFlag_MASKOS_MASK = 0xff,
SYMS_ElfSectionFlag_AMD64_LARGE = (1 << 28),
SYMS_ElfSectionFlag_ORDERED = (1 << 30),
SYMS_ElfSectionFlag_EXCLUDE = (1 << 31),
SYMS_ElfSectionFlag_MASKPROC_SHIFT = 28, SYMS_ElfSectionFlag_MASKPROC_MASK = 0xf,
};
#define SYMS_ElfSectionFlag_Extract_MASKOS(f) (SYMS_U8)(((f) >> SYMS_ElfSectionFlag_MASKOS_SHIFT) & SYMS_ElfSectionFlag_MASKOS_MASK)
#define SYMS_ElfSectionFlag_Extract_MASKPROC(f) (SYMS_U8)(((f) >> SYMS_ElfSectionFlag_MASKPROC_SHIFT) & SYMS_ElfSectionFlag_MASKPROC_MASK)
typedef SYMS_U32 SYMS_ElfAuxType;
enum{
SYMS_ElfAuxType_NULL = 0,
SYMS_ElfAuxType_PHDR = 3,
//  program headers
SYMS_ElfAuxType_PHENT = 4,
//  size of a program header
SYMS_ElfAuxType_PHNUM = 5,
//  number of program headers
SYMS_ElfAuxType_PAGESZ = 6,
//  system page size
SYMS_ElfAuxType_BASE = 7,
//  interpreter base address
SYMS_ElfAuxType_FLAGS = 8,
SYMS_ElfAuxType_ENTRY = 9,
//  program entry point
SYMS_ElfAuxType_UID = 11,
SYMS_ElfAuxType_EUID = 12,
SYMS_ElfAuxType_GID = 13,
SYMS_ElfAuxType_EGID = 14,
SYMS_ElfAuxType_PLATFORM = 15,
//  'platform' as a string (TODO(allen): study)
SYMS_ElfAuxType_HWCAP = 16,
SYMS_ElfAuxType_CLKTCK = 17,
SYMS_ElfAuxType_DCACHEBSIZE = 19,
SYMS_ElfAuxType_ICACHEBSIZE = 20,
SYMS_ElfAuxType_UCACHEBSIZE = 21,
SYMS_ElfAuxType_IGNOREPPC = 22,
SYMS_ElfAuxType_SECURE = 23,
SYMS_ElfAuxType_BASE_PLATFORM = 24,
//  'platform' as a string (different) (TODO(allen): study)
SYMS_ElfAuxType_RANDOM = 25,
//  addres to 16 random bytes
SYMS_ElfAuxType_HWCAP2 = 26,
SYMS_ElfAuxType_EXECFN = 31,
//  file name of executable
SYMS_ElfAuxType_SYSINFO = 32,
SYMS_ElfAuxType_SYSINFO_EHDR = 33,
//  cool info about caches? (TODO(allen): study)
SYMS_ElfAuxType_L1I_CACHESIZE = 40,
SYMS_ElfAuxType_L1I_CACHEGEOMETRY = 41,
SYMS_ElfAuxType_L1D_CACHESIZE = 42,
SYMS_ElfAuxType_L1D_CACHEGEOMETRY = 43,
SYMS_ElfAuxType_L2_CACHESIZE = 44,
SYMS_ElfAuxType_L2_CACHEGEOMETRY = 45,
SYMS_ElfAuxType_L3_CACHESIZE = 46,
SYMS_ElfAuxType_L3_CACHEGEOMETRY = 47,
SYMS_ElfAuxType_COUNT = 34
};
typedef SYMS_U32 SYMS_ElfDynTag;
enum{
SYMS_ElfDynTag_NULL = 0,
SYMS_ElfDynTag_NEEDED = 1,
SYMS_ElfDynTag_PLTRELSZ = 2,
SYMS_ElfDynTag_PLTGOT = 3,
SYMS_ElfDynTag_HASH = 4,
SYMS_ElfDynTag_STRTAB = 5,
SYMS_ElfDynTag_SYMTAB = 6,
SYMS_ElfDynTag_RELA = 7,
SYMS_ElfDynTag_RELASZ = 8,
SYMS_ElfDynTag_RELAENT = 9,
SYMS_ElfDynTag_STRSZ = 10,
SYMS_ElfDynTag_SYMENT = 11,
SYMS_ElfDynTag_INIT = 12,
SYMS_ElfDynTag_FINI = 13,
SYMS_ElfDynTag_SONAME = 14,
SYMS_ElfDynTag_RPATH = 15,
SYMS_ElfDynTag_SYMBOLIC = 16,
SYMS_ElfDynTag_REL = 17,
SYMS_ElfDynTag_RELSZ = 18,
SYMS_ElfDynTag_RELENT = 19,
SYMS_ElfDynTag_PLTREL = 20,
SYMS_ElfDynTag_DEBUG = 21,
SYMS_ElfDynTag_TEXTREL = 22,
SYMS_ElfDynTag_JMPREL = 23,
SYMS_ElfDynTag_BIND_NOW = 24,
SYMS_ElfDynTag_INIT_ARRAY = 25,
SYMS_ElfDynTag_FINI_ARRAY = 26,
SYMS_ElfDynTag_INIT_ARRAYSZ = 27,
SYMS_ElfDynTag_FINI_ARRAYSZ = 28,
SYMS_ElfDynTag_RUNPATH = 29,
SYMS_ElfDynTag_FLAGS = 30,
SYMS_ElfDynTag_PREINIT_ARRAY = 32,
SYMS_ElfDynTag_PREINIT_ARRAYSZ = 33,
SYMS_ElfDynTag_SYMTAB_SHNDX = 34,
SYMS_ElfDynTag_LOOS = 0x6000000D,
SYMS_ElfDynTag_HIOS = 0x6ffff000,
SYMS_ElfDynTag_VALRNGLO = 0x6ffffd00,
SYMS_ElfDynTag_GNU_PRELINKED = 0x6ffffdf5,
SYMS_ElfDynTag_GNU_CONFLICTSZ = 0x6ffffdf6,
SYMS_ElfDynTag_GNU_LIBLISTSZ = 0x6ffffdf7,
SYMS_ElfDynTag_CHECKSUM = 0x6ffffdf8,
SYMS_ElfDynTag_PLTPADSZ = 0x6ffffdf9,
SYMS_ElfDynTag_MOVEENT = 0x6ffffdfa,
SYMS_ElfDynTag_MOVESZ = 0x6ffffdfb,
SYMS_ElfDynTag_FEATURE = 0x6ffffdfc,
SYMS_ElfDynTag_POSFLAG_1 = 0x6ffffdfd,
SYMS_ElfDynTag_SYMINSZ = 0x6ffffdfe,
SYMS_ElfDynTag_SYMINENT = 0x6ffffdff,
SYMS_ElfDynTag_VALRNGHI = SYMS_ElfDynTag_SYMINENT,
SYMS_ElfDynTag_ADDRRNGLO = 0x6ffffe00,
SYMS_ElfDynTag_GNU_HASH = 0x6ffffef5,
SYMS_ElfDynTag_TLSDESC_PLT = 0x6ffffef6,
SYMS_ElfDynTag_TLSDESC_GOT = 0x6ffffef7,
SYMS_ElfDynTag_GNU_CONFLICT = 0x6ffffef8,
SYMS_ElfDynTag_GNU_LIBLIST = 0x6ffffef9,
SYMS_ElfDynTag_CONFIG = 0x6ffffefa,
SYMS_ElfDynTag_DEPAUDIT = 0x6ffffefb,
SYMS_ElfDynTag_AUDIT = 0x6ffffefc,
SYMS_ElfDynTag_PLTPAD = 0x6ffffefd,
SYMS_ElfDynTag_MOVETAB = 0x6ffffefe,
SYMS_ElfDynTag_SYMINFO = 0x6ffffeff,
SYMS_ElfDynTag_ADDRRNGHI = SYMS_ElfDynTag_SYMINFO,
SYMS_ElfDynTag_RELACOUNT = 0x6ffffff9,
SYMS_ElfDynTag_RELCOUNT = 0x6ffffffa,
SYMS_ElfDynTag_FLAGS_1 = 0x6ffffffb,
SYMS_ElfDynTag_VERDEF = 0x6ffffffc,
SYMS_ElfDynTag_VERDEFNUM = 0x6ffffffd,
SYMS_ElfDynTag_VERNEED = 0x6ffffffe,
SYMS_ElfDynTag_VERNEEDNUM = 0x6fffffff,
SYMS_ElfDynTag_VERSYM = 0x6ffffff0,
//  gdb: These section tags are used on Solaris.  We support them everywhere, and hope they do not conflict.
SYMS_ElfDynTag_LOPROC = 0x70000000,
SYMS_ElfDynTag_AUXILIARY = 0x7ffffffd,
SYMS_ElfDynTag_USED = 0x7ffffffe,
SYMS_ElfDynTag_FILTER = 0x7fffffff,
SYMS_ElfDynTag_HIPROC = SYMS_ElfDynTag_FILTER,
SYMS_ElfDynTag_COUNT = 75
};
typedef SYMS_U32 SYMS_ElfDynFlag;
enum{
SYMS_ElfDynFlag_ORIGIN = (1 << 0),
SYMS_ElfDynFlag_SYMBOLIC = (1 << 1),
SYMS_ElfDynFlag_TEXTREL = (1 << 2),
SYMS_ElfDynFlag_BIND_NOW = (1 << 3),
SYMS_ElfDynFlag_STATIC_TLS = (1 << 4),
};
typedef SYMS_U32 SYMS_ElfDynFeatureFlag;
enum{
SYMS_ElfDynFeatureFlag_PARINIT = (1 << 0),
SYMS_ElfDynFeatureFlag_CONFEXP = (1 << 1),
};
typedef SYMS_U8 SYMS_ElfSymBind;
enum{
//  the same name may exists in multiple files without interfering with each other. 
SYMS_ElfSymBind_LOCAL = 0,
//  Visible to all objects that are linked together. 
SYMS_ElfSymBind_GLOBAL = 1,
//  If there is a global symbol with identical name linker doesn't issue an error.
SYMS_ElfSymBind_WEAK = 2,
SYMS_ElfSymBind_LOPROC = 13,
SYMS_ElfSymBind_HIPROC = 15,
SYMS_ElfSymBind_COUNT = 5
};
typedef SYMS_U8 SYMS_ElfSymType;
enum{
SYMS_ElfSymType_NOTYPE = 0,
//  Type is not specified.
SYMS_ElfSymType_OBJECT = 1,
//  Symbol is associated with data object, such as a variable, an array, etc.
SYMS_ElfSymType_FUNC = 2,
//  Symbol is associated with a function.
SYMS_ElfSymType_SECTION = 3,
//  Symbol is used to relocate sections and normally have LOCAL binding.
SYMS_ElfSymType_FILE = 4,
//  Gives name of the source file associated with object.
SYMS_ElfSymType_COMMON = 5,
SYMS_ElfSymType_TLS = 6,
SYMS_ElfSymType_LOPROC = 13,
SYMS_ElfSymType_HIPROC = 15,
SYMS_ElfSymType_COUNT = 9
};
typedef SYMS_U8 SYMS_ElfSymVisibility;
enum{
SYMS_ElfSymVisibility_DEFAULT = 0,
SYMS_ElfSymVisibility_INTERNAL = 1,
SYMS_ElfSymVisibility_HIDDEN = 2,
SYMS_ElfSymVisibility_PROTECTED = 3,
SYMS_ElfSymVisibility_COUNT = 4
};
typedef SYMS_U32 SYMS_ElfRelocI386;
enum{
SYMS_ElfRelocI386_NONE = 0,
SYMS_ElfRelocI386_32 = 1,
SYMS_ElfRelocI386_PC32 = 2,
SYMS_ElfRelocI386_GOT32 = 3,
SYMS_ElfRelocI386_PLT32 = 4,
SYMS_ElfRelocI386_COPY = 5,
SYMS_ElfRelocI386_GLOB_DAT = 6,
SYMS_ElfRelocI386_JUMP_SLOT = 7,
SYMS_ElfRelocI386_RELATIVE = 8,
SYMS_ElfRelocI386_GOTOFF = 9,
SYMS_ElfRelocI386_GOTPC = 10,
SYMS_ElfRelocI386_32PLT = 11,
SYMS_ElfRelocI386_TLS_TPOFF = 14,
SYMS_ElfRelocI386_TLS_IE = 15,
SYMS_ElfRelocI386_TLS_GOTIE = 16,
SYMS_ElfRelocI386_TLS_LE = 17,
SYMS_ElfRelocI386_TLS_GD = 18,
SYMS_ElfRelocI386_TLS_LDM = 19,
SYMS_ElfRelocI386_16 = 20,
SYMS_ElfRelocI386_PC16 = 21,
SYMS_ElfRelocI386_8 = 22,
SYMS_ElfRelocI386_PC8 = 23,
SYMS_ElfRelocI386_TLS_GD_32 = 24,
SYMS_ElfRelocI386_TLS_GD_PUSH = 25,
SYMS_ElfRelocI386_TLS_GD_CALL = 26,
SYMS_ElfRelocI386_TLS_GD_POP = 27,
SYMS_ElfRelocI386_TLS_LDM_32 = 28,
SYMS_ElfRelocI386_TLS_LDM_PUSH = 29,
SYMS_ElfRelocI386_TLS_LDM_CALL = 30,
SYMS_ElfRelocI386_TLS_LDM_POP = 31,
SYMS_ElfRelocI386_TLS_LDO_32 = 32,
SYMS_ElfRelocI386_TLS_IE_32 = 33,
SYMS_ElfRelocI386_TLS_LE_32 = 34,
SYMS_ElfRelocI386_TLS_DTPMOD32 = 35,
SYMS_ElfRelocI386_TLS_DTPOFF32 = 36,
SYMS_ElfRelocI386_TLS_TPOFF32 = 37,
//  38 is not taken
SYMS_ElfRelocI386_TLS_GOTDESC = 39,
SYMS_ElfRelocI386_TLS_DESC_CALL = 40,
SYMS_ElfRelocI386_TLS_DESC = 41,
SYMS_ElfRelocI386_IRELATIVE = 42,
SYMS_ElfRelocI386_GOTX32X = 43,
SYMS_ElfRelocI386_USED_BY_INTEL_200 = 200,
SYMS_ElfRelocI386_GNU_VTINHERIT = 250,
SYMS_ElfRelocI386_GNU_VTENTRY = 251,
SYMS_ElfRelocI386_COUNT = 44
};
typedef SYMS_U32 SYMS_ElfRelocX8664;
enum{
SYMS_ElfRelocX8664_NONE = 0,
SYMS_ElfRelocX8664_64 = 1,
SYMS_ElfRelocX8664_PC32 = 2,
SYMS_ElfRelocX8664_GOT32 = 3,
SYMS_ElfRelocX8664_PLT32 = 4,
SYMS_ElfRelocX8664_COPY = 5,
SYMS_ElfRelocX8664_GLOB_DAT = 6,
SYMS_ElfRelocX8664_JUMP_SLOT = 7,
SYMS_ElfRelocX8664_RELATIVE = 8,
SYMS_ElfRelocX8664_GOTPCREL = 9,
SYMS_ElfRelocX8664_32 = 10,
SYMS_ElfRelocX8664_32S = 11,
SYMS_ElfRelocX8664_16 = 12,
SYMS_ElfRelocX8664_PC16 = 13,
SYMS_ElfRelocX8664_8 = 14,
SYMS_ElfRelocX8664_PC8 = 15,
SYMS_ElfRelocX8664_DTPMOD64 = 16,
SYMS_ElfRelocX8664_DTPOFF64 = 17,
SYMS_ElfRelocX8664_TPOFF64 = 18,
SYMS_ElfRelocX8664_TLSGD = 19,
SYMS_ElfRelocX8664_TLSLD = 20,
SYMS_ElfRelocX8664_DTPOFF32 = 21,
SYMS_ElfRelocX8664_GOTTPOFF = 22,
SYMS_ElfRelocX8664_TPOFF32 = 23,
SYMS_ElfRelocX8664_PC64 = 24,
SYMS_ElfRelocX8664_GOTOFF64 = 25,
SYMS_ElfRelocX8664_GOTPC32 = 26,
SYMS_ElfRelocX8664_GOT64 = 27,
SYMS_ElfRelocX8664_GOTPCREL64 = 28,
SYMS_ElfRelocX8664_GOTPC64 = 29,
SYMS_ElfRelocX8664_GOTPLT64 = 30,
SYMS_ElfRelocX8664_PLTOFF64 = 31,
SYMS_ElfRelocX8664_SIZE32 = 32,
SYMS_ElfRelocX8664_SIZE64 = 33,
SYMS_ElfRelocX8664_GOTPC32_TLSDESC = 34,
SYMS_ElfRelocX8664_TLSDESC_CALL = 35,
SYMS_ElfRelocX8664_TLSDESC = 36,
SYMS_ElfRelocX8664_IRELATIVE = 37,
SYMS_ElfRelocX8664_RELATIVE64 = 38,
SYMS_ElfRelocX8664_PC32_BND = 39,
SYMS_ElfRelocX8664_PLT32_BND = 40,
SYMS_ElfRelocX8664_GOTPCRELX = 41,
SYMS_ElfRelocX8664_REX_GOTPCRELX = 42,
SYMS_ElfRelocX8664_GNU_VTINHERIT = 250,
SYMS_ElfRelocX8664_GNU_VTENTRY = 251,
SYMS_ElfRelocX8664_COUNT = 45
};
typedef SYMS_U32 SYMS_ElfExternalVerFlag;
enum{
SYMS_ElfExternalVerFlag_BASE = (1 << 0),
SYMS_ElfExternalVerFlag_WEAK = (1 << 1),
SYMS_ElfExternalVerFlag_INFO = (1 << 2),
};
typedef SYMS_U32 SYMS_ElfNoteType;
enum{
SYMS_ElfNoteType_GNU_ABI = 1,
SYMS_ElfNoteType_GNU_HWCAP = 2,
SYMS_ElfNoteType_GNU_BUILD_ID = 3,
SYMS_ElfNoteType_GNU_GOLD_VERSION = 4,
SYMS_ElfNoteType_GNU_PROPERTY_TYPE_0 = 5,
SYMS_ElfNoteType_COUNT = 5
};
typedef SYMS_U32 SYMS_ElfGnuABITag;
enum{
SYMS_ElfGnuABITag_LINUX = 0,
SYMS_ElfGnuABITag_HURD = 1,
SYMS_ElfGnuABITag_SOLARIS = 2,
SYMS_ElfGnuABITag_FREEBSD = 3,
SYMS_ElfGnuABITag_NETBSD = 4,
SYMS_ElfGnuABITag_SYLLABLE = 5,
SYMS_ElfGnuABITag_NACL = 6,
SYMS_ElfGnuABITag_COUNT = 7
};
typedef SYMS_S32 SYMS_ElfGnuProperty;
enum{
SYMS_ElfGnuProperty_LOPROC = 0xc0000000,
//  processor-specific range
SYMS_ElfGnuProperty_HIPROC = 0xdfffffff,
SYMS_ElfGnuProperty_LOUSER = 0xe0000000,
//  application-specific range
SYMS_ElfGnuProperty_HIUSER = 0xffffffff,
SYMS_ElfGnuProperty_STACK_SIZE = 1,
SYMS_ElfGnuProperty_NO_COPY_ON_PROTECTED = 2,
SYMS_ElfGnuProperty_COUNT = 6
};
typedef SYMS_U32 SYMS_ElfGnuPropertyX86Isa1;
enum{
SYMS_ElfGnuPropertyX86Isa1_BASE_LINE = (1 << 0),
SYMS_ElfGnuPropertyX86Isa1_V2 = (1 << 1),
SYMS_ElfGnuPropertyX86Isa1_V3 = (1 << 2),
SYMS_ElfGnuPropertyX86Isa1_V4 = (1 << 3),
};
typedef SYMS_U32 SYMS_ElfGnuPropertyX86Compat1Isa1;
enum{
SYMS_ElfGnuPropertyX86Compat1Isa1_486 = (1 << 0),
SYMS_ElfGnuPropertyX86Compat1Isa1_586 = (1 << 1),
SYMS_ElfGnuPropertyX86Compat1Isa1_686 = (1 << 2),
SYMS_ElfGnuPropertyX86Compat1Isa1_SSE = (1 << 3),
SYMS_ElfGnuPropertyX86Compat1Isa1_SSE2 = (1 << 4),
SYMS_ElfGnuPropertyX86Compat1Isa1_SSE3 = (1 << 5),
SYMS_ElfGnuPropertyX86Compat1Isa1_SSSE3 = (1 << 6),
SYMS_ElfGnuPropertyX86Compat1Isa1_SSE4_1 = (1 << 7),
SYMS_ElfGnuPropertyX86Compat1Isa1_SSE4_2 = (1 << 8),
SYMS_ElfGnuPropertyX86Compat1Isa1_AVX = (1 << 9),
SYMS_ElfGnuPropertyX86Compat1Isa1_AVX2 = (1 << 10),
SYMS_ElfGnuPropertyX86Compat1Isa1_AVX512F = (1 << 11),
SYMS_ElfGnuPropertyX86Compat1Isa1_AVX512ER = (1 << 12),
SYMS_ElfGnuPropertyX86Compat1Isa1_AVX512PF = (1 << 13),
SYMS_ElfGnuPropertyX86Compat1Isa1_AVX512VL = (1 << 14),
SYMS_ElfGnuPropertyX86Compat1Isa1_AVX512DQ = (1 << 15),
SYMS_ElfGnuPropertyX86Compat1Isa1_AVX512BW = (1 << 16),
};
typedef SYMS_U32 SYMS_ElfGnuPropertyX86Compat2Isa1;
enum{
SYMS_ElfGnuPropertyX86Compat2Isa1_CMOVE = (1 << 0),
SYMS_ElfGnuPropertyX86Compat2Isa1_SSE = (1 << 1),
SYMS_ElfGnuPropertyX86Compat2Isa1_SSE2 = (1 << 2),
SYMS_ElfGnuPropertyX86Compat2Isa1_SSE3 = (1 << 3),
SYMS_ElfGnuPropertyX86Compat2Isa1_SSE4_1 = (1 << 4),
SYMS_ElfGnuPropertyX86Compat2Isa1_SSE4_2 = (1 << 5),
SYMS_ElfGnuPropertyX86Compat2Isa1_AVX = (1 << 6),
SYMS_ElfGnuPropertyX86Compat2Isa1_AVX2 = (1 << 7),
SYMS_ElfGnuPropertyX86Compat2Isa1_FMA = (1 << 8),
SYMS_ElfGnuPropertyX86Compat2Isa1_AVX512F = (1 << 9),
SYMS_ElfGnuPropertyX86Compat2Isa1_AVX512CD = (1 << 10),
SYMS_ElfGnuPropertyX86Compat2Isa1_AVX512ER = (1 << 11),
SYMS_ElfGnuPropertyX86Compat2Isa1_AVX512PF = (1 << 12),
SYMS_ElfGnuPropertyX86Compat2Isa1_AVX512VL = (1 << 13),
SYMS_ElfGnuPropertyX86Compat2Isa1_AVX512DQ = (1 << 14),
SYMS_ElfGnuPropertyX86Compat2Isa1_AVX512BW = (1 << 15),
SYMS_ElfGnuPropertyX86Compat2Isa1_AVX512_4FMAPS = (1 << 16),
SYMS_ElfGnuPropertyX86Compat2Isa1_AVX512_4VNNIW = (1 << 17),
SYMS_ElfGnuPropertyX86Compat2Isa1_AVX512_BITALG = (1 << 18),
SYMS_ElfGnuPropertyX86Compat2Isa1_AVX512_IFMA = (1 << 19),
SYMS_ElfGnuPropertyX86Compat2Isa1_AVX512_VBMI = (1 << 20),
SYMS_ElfGnuPropertyX86Compat2Isa1_AVX512_VBMI2 = (1 << 21),
SYMS_ElfGnuPropertyX86Compat2Isa1_AVX512_VNNI = (1 << 22),
SYMS_ElfGnuPropertyX86Compat2Isa1_AVX512_BF16 = (1 << 23),
};
typedef SYMS_S32 SYMS_ElfGnuPropertyX86;
enum{
SYMS_ElfGnuPropertyX86_FEATURE_1_AND = 0xc0000002,
SYMS_ElfGnuPropertyX86_FEATURE_2_USED = 0xc0010001,
SYMS_ElfGnuPropertyX86_ISA_1_NEEDED = 0xc0008002,
SYMS_ElfGnuPropertyX86_ISA_2_NEEDED = 0xc0008001,
SYMS_ElfGnuPropertyX86_ISA_1_USED = 0xc0010002,
SYMS_ElfGnuPropertyX86_COMPAT_ISA_1_USED = 0xc0000000,
SYMS_ElfGnuPropertyX86_COMPAT_ISA_1_NEEDED = 0xc0000001,
SYMS_ElfGnuPropertyX86_UINT32_AND_LO = SYMS_ElfGnuPropertyX86_FEATURE_1_AND,
SYMS_ElfGnuPropertyX86_UINT32_AND_HI = 0xc0007fff,
SYMS_ElfGnuPropertyX86_UINT32_OR_LO = 0xc0008000,
SYMS_ElfGnuPropertyX86_UINT32_OR_HI = 0xc000ffff,
SYMS_ElfGnuPropertyX86_UINT32_OR_AND_LO = 0xc0010000,
SYMS_ElfGnuPropertyX86_UINT32_OR_AND_HI = 0xc0017fff,
SYMS_ElfGnuPropertyX86_COUNT = 13
};
typedef SYMS_U32 SYMS_ElfGnuPropertyX86Feature1;
enum{
SYMS_ElfGnuPropertyX86Feature1_IBT = (1 << 0),
SYMS_ElfGnuPropertyX86Feature1_SHSTK = (1 << 1),
SYMS_ElfGnuPropertyX86Feature1_LAM_U48 = (1 << 2),
SYMS_ElfGnuPropertyX86Feature1_LAM_U57 = (1 << 3),
};
typedef SYMS_U32 SYMS_ElfGnuPropertyX86Feature2;
enum{
SYMS_ElfGnuPropertyX86Feature2_X86 = (1 << 0),
SYMS_ElfGnuPropertyX86Feature2_X87 = (1 << 1),
SYMS_ElfGnuPropertyX86Feature2_MMX = (1 << 2),
SYMS_ElfGnuPropertyX86Feature2_XMM = (1 << 3),
SYMS_ElfGnuPropertyX86Feature2_YMM = (1 << 4),
SYMS_ElfGnuPropertyX86Feature2_ZMM = (1 << 5),
SYMS_ElfGnuPropertyX86Feature2_FXSR = (1 << 6),
SYMS_ElfGnuPropertyX86Feature2_XSAVE = (1 << 7),
SYMS_ElfGnuPropertyX86Feature2_XSAVEOPT = (1 << 8),
SYMS_ElfGnuPropertyX86Feature2_XSAVEC = (1 << 9),
SYMS_ElfGnuPropertyX86Feature2_TMM = (1 << 10),
SYMS_ElfGnuPropertyX86Feature2_MASK = (1 << 11),
};

//~ generated from code at syms/metaprogram/syms_metaprogram_serial.c:1133
SYMS_C_LINKAGE_BEGIN
SYMS_API SYMS_Arch syms_arch_from_elf_machine_type(SYMS_ElfMachineKind v);
SYMS_C_LINKAGE_END

//~ generated from code at syms/metaprogram/syms_metaprogram_serial.c:1588
SYMS_C_LINKAGE_BEGIN
SYMS_C_LINKAGE_END

//~ generated from code at syms/metaprogram/syms_metaprogram_serial.c:1694
#endif
