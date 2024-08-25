// Copyright Epic Games, Inc. All Rights Reserved.
// generated
#ifndef _SYMS_META_PE_H
#define _SYMS_META_PE_H
//~ generated from code at syms/metaprogram/syms_metaprogram_serial.c:885
#pragma pack(push,1)
typedef struct SYMS_DosHeader{
SYMS_U16 magic;
SYMS_U16 last_page_size;
SYMS_U16 page_count;
SYMS_U16 reloc_count;
SYMS_U16 paragraph_header_size;
SYMS_U16 min_paragraph;
SYMS_U16 max_paragraph;
SYMS_U16 init_ss;
SYMS_U16 init_sp;
SYMS_U16 checksum;
SYMS_U16 init_ip;
SYMS_U16 init_cs;
SYMS_U16 reloc_table_file_off;
SYMS_U16 overlay_number;
SYMS_U16 reserved[4];
SYMS_U16 oem_id;
SYMS_U16 oem_info;
SYMS_U16 reserved2[10];
SYMS_U32 coff_file_offset;
} SYMS_DosHeader;
typedef SYMS_U16 SYMS_PeWindowsSubsystem;
enum{
SYMS_PeWindowsSubsystem_UNKNOWN = 0,
SYMS_PeWindowsSubsystem_NATIVE = 1,
SYMS_PeWindowsSubsystem_WINDOWS_GUI = 2,
SYMS_PeWindowsSubsystem_WINDOWS_CUI = 3,
SYMS_PeWindowsSubsystem_OS2_CUI = 5,
SYMS_PeWindowsSubsystem_POSIX_CUI = 7,
SYMS_PeWindowsSubsystem_NATIVE_WINDOWS = 8,
SYMS_PeWindowsSubsystem_WINDOWS_CE_GUI = 9,
SYMS_PeWindowsSubsystem_EFI_APPLICATION = 10,
SYMS_PeWindowsSubsystem_EFI_BOOT_SERVICE_DRIVER = 11,
SYMS_PeWindowsSubsystem_EFI_RUNTIME_DRIVER = 12,
SYMS_PeWindowsSubsystem_EFI_ROM = 13,
SYMS_PeWindowsSubsystem_XBOX = 14,
SYMS_PeWindowsSubsystem_WINDOWS_BOOT_APPLICATION = 16,
SYMS_PeWindowsSubsystem_COUNT = 14
};
typedef SYMS_U16 SYMS_ImageFileCharacteristics;
enum{
SYMS_ImageFileCharacteristic_STRIPPED = (1 << 0),
SYMS_ImageFileCharacteristic_EXE = (1 << 1),
SYMS_ImageFileCharacteristic_NUMS_STRIPPED = (1 << 2),
SYMS_ImageFileCharacteristic_SYMS_STRIPPED = (1 << 3),
SYMS_ImageFileCharacteristic_AGGRESIVE_WS_TRIM = (1 << 4),
SYMS_ImageFileCharacteristic_LARGE_ADDRESS_AWARE = (1 << 5),
SYMS_ImageFileCharacteristic_UNUSED1 = (1 << 6),
SYMS_ImageFileCharacteristic_BYTES_RESERVED_LO = (1 << 7),
SYMS_ImageFileCharacteristic_32BIT_MACHINE = (1 << 8),
SYMS_ImageFileCharacteristic_DEBUG_STRIPPED = (1 << 9),
SYMS_ImageFileCharacteristic_FILE_REMOVABLE_RUN_FROM_SWAP = (1 << 10),
SYMS_ImageFileCharacteristic_NET_RUN_FROM_SWAP = (1 << 11),
SYMS_ImageFileCharacteristic_FILE_SYSTEM = (1 << 12),
SYMS_ImageFileCharacteristic_FILE_DLL = (1 << 13),
SYMS_ImageFileCharacteristic_FILE_UP_SYSTEM_ONLY = (1 << 14),
SYMS_ImageFileCharacteristic_BYTES_RESERVED_HI = (1 << 15),
};
typedef SYMS_U16 SYMS_DllCharacteristics;
enum{
SYMS_DllCharacteristic_HIGH_ENTROPY_VA = (1 << 5),
SYMS_DllCharacteristic_DYNAMIC_BASE = (1 << 6),
SYMS_DllCharacteristic_FORCE_INTEGRITY = (1 << 7),
SYMS_DllCharacteristic_NX_COMPAT = (1 << 8),
SYMS_DllCharacteristic_NO_ISOLATION = (1 << 9),
SYMS_DllCharacteristic_NO_SEH = (1 << 10),
SYMS_DllCharacteristic_NO_BIND = (1 << 11),
SYMS_DllCharacteristic_APPCONTAINER = (1 << 12),
SYMS_DllCharacteristic_WDM_DRIVER = (1 << 13),
SYMS_DllCharacteristic_GUARD_CF = (1 << 14),
SYMS_DllCharacteristic_TERMINAL_SERVER_AWARE = (1 << 15),
};
typedef struct SYMS_PeOptionalHeader32{
SYMS_U16 magic;
SYMS_U8 major_linker_version;
SYMS_U8 minor_linker_version;
SYMS_U32 sizeof_code;
SYMS_U32 sizeof_inited_data;
SYMS_U32 sizeof_uninited_data;
SYMS_U32 entry_point_va;
SYMS_U32 code_base;
SYMS_U32 data_base;
SYMS_U32 image_base;
SYMS_U32 section_alignment;
SYMS_U32 file_alignment;
SYMS_U16 major_os_ver;
SYMS_U16 minor_os_ver;
SYMS_U16 major_img_ver;
SYMS_U16 minor_img_ver;
SYMS_U16 major_subsystem_ver;
SYMS_U16 minor_subsystem_ver;
SYMS_U32 win32_version_value;
SYMS_U32 sizeof_image;
SYMS_U32 sizeof_headers;
SYMS_U32 check_sum;
SYMS_PeWindowsSubsystem subsystem;
SYMS_DllCharacteristics dll_characteristics;
SYMS_U32 sizeof_stack_reserve;
SYMS_U32 sizeof_stack_commit;
SYMS_U32 sizeof_heap_reserve;
SYMS_U32 sizeof_heap_commit;
SYMS_U32 loader_flags;
SYMS_U32 data_dir_count;
} SYMS_PeOptionalHeader32;
typedef struct SYMS_PeOptionalHeader32Plus{
SYMS_U16 magic;
SYMS_U8 major_linker_version;
SYMS_U8 minor_linker_version;
SYMS_U32 sizeof_code;
SYMS_U32 sizeof_inited_data;
SYMS_U32 sizeof_uninited_data;
SYMS_U32 entry_point_va;
SYMS_U32 code_base;
SYMS_U64 image_base;
SYMS_U32 section_alignment;
SYMS_U32 file_alignment;
SYMS_U16 major_os_ver;
SYMS_U16 minor_os_ver;
SYMS_U16 major_img_ver;
SYMS_U16 minor_img_ver;
SYMS_U16 major_subsystem_ver;
SYMS_U16 minor_subsystem_ver;
SYMS_U32 win32_version_value;
SYMS_U32 sizeof_image;
SYMS_U32 sizeof_headers;
SYMS_U32 check_sum;
SYMS_PeWindowsSubsystem subsystem;
SYMS_DllCharacteristics dll_characteristics;
SYMS_U64 sizeof_stack_reserve;
SYMS_U64 sizeof_stack_commit;
SYMS_U64 sizeof_heap_reserve;
SYMS_U64 sizeof_heap_commit;
SYMS_U32 loader_flags;
SYMS_U32 data_dir_count;
} SYMS_PeOptionalHeader32Plus;
typedef enum SYMS_PeDataDirectoryIndex{
SYMS_PeDataDirectoryIndex_EXPORT,
SYMS_PeDataDirectoryIndex_IMPORT,
SYMS_PeDataDirectoryIndex_RESOURCES,
SYMS_PeDataDirectoryIndex_EXCEPTIONS,
SYMS_PeDataDirectoryIndex_CERT,
SYMS_PeDataDirectoryIndex_BASE_RELOC,
SYMS_PeDataDirectoryIndex_DEBUG,
SYMS_PeDataDirectoryIndex_ARCH,
SYMS_PeDataDirectoryIndex_GLOBAL_PTR,
SYMS_PeDataDirectoryIndex_TLS,
SYMS_PeDataDirectoryIndex_LOAD_CONFIG,
SYMS_PeDataDirectoryIndex_BOUND_IMPORT,
SYMS_PeDataDirectoryIndex_IMPORT_ADDR,
SYMS_PeDataDirectoryIndex_DELAY_IMPORT,
SYMS_PeDataDirectoryIndex_COM_DESCRIPTOR,
SYMS_PeDataDirectoryIndex_RESERVED,
SYMS_PeDataDirectoryIndex_COUNT = 16
} SYMS_PeDataDirectoryIndex;
typedef struct SYMS_PeDataDirectory{
SYMS_U32 virt_off;
SYMS_U32 virt_size;
} SYMS_PeDataDirectory;
typedef SYMS_U32 SYMS_PeDebugDirectoryType;
enum{
SYMS_PeDebugDirectoryType_UNKNOWN = 0,
SYMS_PeDebugDirectoryType_COFF = 1,
SYMS_PeDebugDirectoryType_CODEVIEW = 2,
SYMS_PeDebugDirectoryType_FPO = 3,
SYMS_PeDebugDirectoryType_MISC = 4,
SYMS_PeDebugDirectoryType_EXCEPTION = 5,
SYMS_PeDebugDirectoryType_FIXUP = 6,
SYMS_PeDebugDirectoryType_OMAP_TO_SRC = 7,
SYMS_PeDebugDirectoryType_OMAP_FROM_SRC = 8,
SYMS_PeDebugDirectoryType_BORLAND = 9,
SYMS_PeDebugDirectoryType_RESERVED10 = 10,
SYMS_PeDebugDirectoryType_CLSID = 11,
SYMS_PeDebugDirectoryType_VC_FEATURE = 12,
SYMS_PeDebugDirectoryType_POGO = 13,
SYMS_PeDebugDirectoryType_ILTCG = 14,
SYMS_PeDebugDirectoryType_MPX = 15,
SYMS_PeDebugDirectoryType_REPRO = 16,
SYMS_PeDebugDirectoryType_EX_DLLCHARACTERISTICS = 20,
SYMS_PeDebugDirectoryType_COUNT = 18
};
typedef SYMS_U8 SYMS_PeFPOFlags;
enum{
SYMS_PeFPOFlags_HAS_SEH = 0x800,
SYMS_PeFPOFlags_USE_BP_REG = 0x1000,
SYMS_PeFPOFlags_RESERVED = 0x2000,
SYMS_PeFPOFlags_COUNT = 3
};
typedef SYMS_U16 SYMS_PeFPOEncoded;
enum{
SYMS_PeFPOEncoded_PROLOG_SIZE_SHIFT = 0, SYMS_PeFPOEncoded_PROLOG_SIZE_MASK = 0xff,
SYMS_PeFPOEncoded_SAVED_REGS_SIZE_SHIFT = 8, SYMS_PeFPOEncoded_SAVED_REGS_SIZE_MASK = 0x7,
SYMS_PeFPOEncoded_FLAGS_SHIFT = 11, SYMS_PeFPOEncoded_FLAGS_MASK = 0x7,
SYMS_PeFPOEncoded_FRAME_TYPE_SHIFT = 14, SYMS_PeFPOEncoded_FRAME_TYPE_MASK = 0x3,
};
#define SYMS_PeFPOEncoded_Extract_PROLOG_SIZE(f) (SYMS_U8)(((f) >> SYMS_PeFPOEncoded_PROLOG_SIZE_SHIFT) & SYMS_PeFPOEncoded_PROLOG_SIZE_MASK)
#define SYMS_PeFPOEncoded_Extract_SAVED_REGS_SIZE(f) (SYMS_U8)(((f) >> SYMS_PeFPOEncoded_SAVED_REGS_SIZE_SHIFT) & SYMS_PeFPOEncoded_SAVED_REGS_SIZE_MASK)
#define SYMS_PeFPOEncoded_Extract_FLAGS(f) (SYMS_U8)(((f) >> SYMS_PeFPOEncoded_FLAGS_SHIFT) & SYMS_PeFPOEncoded_FLAGS_MASK)
#define SYMS_PeFPOEncoded_Extract_FRAME_TYPE(f) (SYMS_U8)(((f) >> SYMS_PeFPOEncoded_FRAME_TYPE_SHIFT) & SYMS_PeFPOEncoded_FRAME_TYPE_MASK)
typedef SYMS_U8 SYMS_PeFPOType;
enum{
SYMS_PeFPOType_FPO = 0,
SYMS_PeFPOType_TRAP = 1,
SYMS_PeFPOType_TSS = 2,
SYMS_PeFPOType_NOFPO = 3,
SYMS_PeFPOType_COUNT = 4
};
typedef SYMS_U32 SYMS_PeDebugMiscType;
enum{
SYMS_PeDebugMiscType_NULL,
SYMS_PeDebugMiscType_EXE_NAME,
SYMS_PeDebugMiscType_COUNT = 2
};
typedef struct SYMS_PeDebugDirectory{
SYMS_U32 characteristics;
SYMS_U32 time_stamp;
SYMS_U16 major_ver;
SYMS_U16 minor_ver;
SYMS_PeDebugDirectoryType type;
SYMS_U32 size;
SYMS_U32 virtual_offset;
SYMS_U32 file_offset;
} SYMS_PeDebugDirectory;
typedef SYMS_U32 SYMS_PeGlobalFlags;
enum{
SYMS_PeGlobalFlags_STOP_ON_EXCEPTION = (1 << 0),
SYMS_PeGlobalFlags_SHOW_LDR_SNAPS = (1 << 1),
SYMS_PeGlobalFlags_DEBUG_INITIAL_COMMAND = (1 << 2),
SYMS_PeGlobalFlags_STOP_ON_HUNG_GUI = (1 << 3),
SYMS_PeGlobalFlags_HEAP_ENABLE_TAIL_CHECK = (1 << 4),
SYMS_PeGlobalFlags_HEAP_ENABLE_FREE_CHECK = (1 << 5),
SYMS_PeGlobalFlags_HEAP_VALIDATE_PARAMETERS = (1 << 6),
SYMS_PeGlobalFlags_HEAP_VALIDATE_ALL = (1 << 7),
SYMS_PeGlobalFlags_APPLICATION_VERIFIER = (1 << 8),
SYMS_PeGlobalFlags_POOL_ENABLE_TAGGING = (1 << 10),
SYMS_PeGlobalFlags_HEAP_ENABLE_TAGGING = (1 << 11),
SYMS_PeGlobalFlags_STACK_TRACE_DB = (1 << 12),
SYMS_PeGlobalFlags_KERNEL_STACK_TRACE_DB = (1 << 13),
SYMS_PeGlobalFlags_MAINTAIN_OBJECT_TYPELIST = (1 << 14),
SYMS_PeGlobalFlags_HEAP_ENABLE_TAG_BY_DLL = (1 << 15),
SYMS_PeGlobalFlags_DISABLE_STACK_EXTENSION = (1 << 16),
SYMS_PeGlobalFlags_ENABLE_CSRDEBUG = (1 << 17),
SYMS_PeGlobalFlags_ENABLE_KDEBUG_SYMBOL_LOAD = (1 << 18),
SYMS_PeGlobalFlags_DISABLE_PAGE_KERNEL_STACKS = (1 << 19),
SYMS_PeGlobalFlags_ENABLE_SYSTEM_CRIT_BREAKS = (1 << 20),
SYMS_PeGlobalFlags_HEAP_DISABLE_COALESCING = (1 << 21),
SYMS_PeGlobalFlags_ENABLE_CLOSE_EXCEPTIONS = (1 << 22),
SYMS_PeGlobalFlags_ENABLE_EXCEPTION_LOGGING = (1 << 23),
SYMS_PeGlobalFlags_ENABLE_HANDLE_TYPE_TAGGING = (1 << 24),
SYMS_PeGlobalFlags_HEAP_PAGE_ALLOCS = (1 << 25),
SYMS_PeGlobalFlags_DEBUG_INITIAL_COMMAND_EX = (1 << 26),
SYMS_PeGlobalFlags_DISABLE_DBGPRINT = (1 << 27),
SYMS_PeGlobalFlags_CRITSEC_EVENT_CREATION = (1 << 28),
SYMS_PeGlobalFlags_LDR_TOP_DOWN = (1 << 29),
SYMS_PeGlobalFlags_ENABLE_HANDLE_EXCEPTIONS = (1 << 30),
SYMS_PeGlobalFlags_DISABLE_PROTDLLS = (1 << 31),
};
typedef SYMS_U32 SYMS_PeLoadConfigGuardFlags;
enum{
SYMS_PeLoadConfigGuardFlags_CF_INSTRUMENTED = (1 << 8),
SYMS_PeLoadConfigGuardFlags_CFW_INSTRUMENTED = (1 << 9),
SYMS_PeLoadConfigGuardFlags_CF_FUNCTION_TABLE_PRESENT = (1 << 10),
SYMS_PeLoadConfigGuardFlags_SECURITY_COOKIE_UNUSED = (1 << 11),
SYMS_PeLoadConfigGuardFlags_PROTECT_DELAYLOAD_IAT = (1 << 12),
SYMS_PeLoadConfigGuardFlags_DELAYLOAD_IAT_IN_ITS_OWN_SECTION = (1 << 13),
SYMS_PeLoadConfigGuardFlags_CF_EXPORT_SUPPRESSION_INFO_PRESENT = (1 << 14),
SYMS_PeLoadConfigGuardFlags_CF_ENABLE_EXPORT_SUPPRESSION = (1 << 15),
SYMS_PeLoadConfigGuardFlags_CF_LONGJUMP_TABLE_PRESENT = (1 << 16),
SYMS_PeLoadConfigGuardFlags_EH_CONTINUATION_TABLE_PRESENT = (1 << 22),
SYMS_PeLoadConfigGuardFlags_CF_FUNCTION_TABLE_SIZE_SHIFT = 28, SYMS_PeLoadConfigGuardFlags_CF_FUNCTION_TABLE_SIZE_MASK = 0xf,
};
#define SYMS_PeLoadConfigGuardFlags_Extract_CF_FUNCTION_TABLE_SIZE(f) (SYMS_U32)(((f) >> SYMS_PeLoadConfigGuardFlags_CF_FUNCTION_TABLE_SIZE_SHIFT) & SYMS_PeLoadConfigGuardFlags_CF_FUNCTION_TABLE_SIZE_MASK)
#pragma pack(pop)

//~ generated from code at syms/metaprogram/syms_metaprogram_serial.c:1588
SYMS_C_LINKAGE_BEGIN
SYMS_C_LINKAGE_END

//~ generated from code at syms/metaprogram/syms_metaprogram_serial.c:1694
#endif
