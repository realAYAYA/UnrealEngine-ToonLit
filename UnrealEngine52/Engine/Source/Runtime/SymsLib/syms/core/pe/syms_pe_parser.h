// Copyright Epic Games, Inc. All Rights Reserved.
/* date = June 7th 2021 1:06 pm */

#ifndef SYMS_PE_PARSER_H
#define SYMS_PE_PARSER_H

// TODO(allen): rename? split PE and COFF accelerators?

////////////////////////////////
// NOTE(allen): PE Parser Types

typedef struct SYMS_PeFileAccel{
  SYMS_FileFormat format;
  SYMS_U32 coff_off;
} SYMS_PeFileAccel;

typedef struct SYMS_CoffFileAccel{
  SYMS_FileFormat format;
  // TODO(allen): 
} SYMS_CoffFileAccel;

typedef struct SYMS_PeBinAccel{
  SYMS_FileFormat format;
  SYMS_U64 image_base;
  SYMS_U64 section_array_off;
  SYMS_U64 section_count;
  SYMS_U64 dbg_path_off;
  SYMS_U64 dbg_path_size;
  SYMS_PeGuid dbg_guid;
  SYMS_U32 dbg_age;
  SYMS_Arch arch;
  SYMS_U32 data_dir_count;
  SYMS_U64Range *data_dirs_virt;
  SYMS_U64Range *data_dirs_file;
} SYMS_PeBinAccel;

typedef struct SYMS_CoffBinAccel{
  SYMS_FileFormat format;
  SYMS_U64 image_base;
  SYMS_U64 section_array_off;
  SYMS_U64 section_count;
  SYMS_Arch arch;
} SYMS_CoffBinAccel;

enum{
  SYMS_PeLookupBy_Ordinal,
  SYMS_PeLookupBy_NameHint
};
typedef SYMS_U32 SYMS_PeImportLookupBy;

typedef struct SYMS_PeImportNode
{
  struct SYMS_PeImportNode *next;
  SYMS_PeImportLookupBy lookup_by;
  SYMS_U16 ordinal;
  SYMS_String8 name;
  SYMS_U16 hint;
} SYMS_PeImportNode;

typedef struct SYMS_PeImportDllNode
{
  struct SYMS_PeImportDllNode *next;
  SYMS_String8 name;
  SYMS_PeImportDirectoryEntry import_entry;
  SYMS_PeImportNode *first_import;
  SYMS_PeImportNode *last_import;
  SYMS_U32 import_count;
} SYMS_PeImportDllNode;

////////////////////////////////
// NOTE(allen): PE Parser Functions

// accelerators
SYMS_API SYMS_PeFileAccel* syms_pe_file_accel_from_data(SYMS_Arena *arena, SYMS_String8 data);
SYMS_API SYMS_PeBinAccel*  syms_pe_bin_accel_from_file(SYMS_Arena *arena, SYMS_String8 data,
                                                       SYMS_PeFileAccel *file);

SYMS_API SYMS_CoffFileAccel* syms_coff_file_accel_from_data(SYMS_Arena *arena, SYMS_String8 data);
SYMS_API SYMS_CoffBinAccel*  syms_coff_bin_accel_from_file(SYMS_Arena *arena, SYMS_String8 data,
                                                           SYMS_CoffFileAccel *file);


// arch
SYMS_API SYMS_Arch syms_pe_arch_from_bin(SYMS_PeBinAccel *bin);
SYMS_API SYMS_Arch syms_coff_arch_from_bin(SYMS_CoffBinAccel *bin);

// external info
SYMS_API SYMS_ExtFileList syms_pe_ext_file_list_from_bin(SYMS_Arena *arena, SYMS_String8 data,
                                                         SYMS_PeBinAccel *bin);

// binary secs
//  pe specific
SYMS_API SYMS_CoffSection syms_pe_coff_section(SYMS_String8 data, SYMS_PeBinAccel *bin, SYMS_U64 n);

SYMS_API SYMS_SecInfoArray syms_pe_coff_sec_info_array_from_data(SYMS_Arena *arena, SYMS_String8 data,
                                                                 SYMS_U64 sec_array_off, SYMS_U64 sec_count);


//  main api
SYMS_API SYMS_SecInfoArray syms_pe_sec_info_array_from_bin(SYMS_Arena *arena, SYMS_String8 data,
                                                           SYMS_PeBinAccel *bin);
SYMS_API SYMS_SecInfoArray syms_coff_sec_info_array_from_bin(SYMS_Arena *arena, SYMS_String8 data,
                                                             SYMS_CoffBinAccel *bin);

// imports & exports
//  main api
SYMS_API SYMS_ImportArray syms_pe_imports_from_bin(SYMS_Arena *arena, SYMS_String8 data, SYMS_PeBinAccel *bin);
SYMS_API SYMS_ExportArray syms_pe_exports_from_bin(SYMS_Arena *arena, SYMS_String8 data, SYMS_PeBinAccel *bin);

////////////////////////////////
// NOTE(allen): PE Specific Helpers

SYMS_API SYMS_U64 syms_pe_binary_search_intel_pdata(SYMS_String8 data, SYMS_PeBinAccel *bin, SYMS_U64 voff);

SYMS_API SYMS_U64 syms_pe_sec_number_from_voff(SYMS_String8 data, SYMS_PeBinAccel *bin, SYMS_U64 voff);

SYMS_API void*    syms_pe_ptr_from_sec_number(SYMS_String8 data, SYMS_PeBinAccel *bin, SYMS_U64 n);
SYMS_API void*    syms_pe_ptr_from_foff(SYMS_String8 data, SYMS_PeBinAccel *bin, SYMS_U64 foff);
SYMS_API void*    syms_pe_ptr_from_voff(SYMS_String8 data, SYMS_PeBinAccel *bin, SYMS_U64 voff);

SYMS_API SYMS_U64 syms_pe_virt_off_to_file_off(SYMS_CoffSection *sections, SYMS_U32 section_count, SYMS_U64 virt_off);
SYMS_API SYMS_U64 syms_pe_bin_virt_off_to_file_off(SYMS_String8 data, SYMS_PeBinAccel *bin, SYMS_U64 virt_off);

#endif //SYMS_PE_PARSER_H
