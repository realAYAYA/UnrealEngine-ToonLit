// Copyright Epic Games, Inc. All Rights Reserved.
/* date = January 13th 2021 4:55 pm */

#ifndef SYMS_PE_H
#define SYMS_PE_H

/* 
** NOTE(allen): Overview of PE format
** 
** At the offset 0: DOS Magic Number, DOS Header
**  DOS Header contains a "pointer" to the COFF Header.
**
** Starting at the COFF Header the following are packed in order:
** 1. PE Magic Number
** 2. COFF Header (fixed size)
** 3. COFF Optional Header (size determined by 1)
** 4. Section Table (size determined by 1)
**
*/

////////////////////////////////
// NOTE(allen): Generated

#include "syms/core/generated/syms_meta_coff.h"
#include "syms/core/generated/syms_meta_pe.h"

////////////////////////////////
// NOTE(allen): DOS Header

// this is the "MZ" as a 16-bit short
#define SYMS_DOS_MAGIC 0x5a4d

////////////////////////////////

#define SYMS_COFF_MIN_BIG_OBJ_VERSION 2
#define SYMS_COFF_BIG_OBJ_MAGIC_LO 0xC7A1BAD1EEBAA94Bull
#define SYMS_COFF_BIG_OBJ_MAGIC_HI 0xAF20FAF66AA4DCB8ull
typedef struct SYMS_CoffHeaderBigObj{
  SYMS_U16 sig1; // SYMS_CoffMachineType_UNKNOWN
  SYMS_U16 sig2; // SYMS_U16_MAX
  SYMS_U16 version;
  SYMS_U16 machine;
  SYMS_U32 time_stamp;
  SYMS_U8 magic[16];
  SYMS_U32 unused[4];
  SYMS_U32 section_count;
  SYMS_U32 pointer_to_symbol_table;
  SYMS_U32 number_of_symbols;
} SYMS_CoffHeaderBigObj;

#pragma pack(push, 1)

typedef union SYMS_CoffSymbolName{
  SYMS_U8 short_name[8];
  struct{
    // if this field is filled with zeroes we have a long name,
    // which means name is stored in the string table 
    // and we need to use the offset to look it up...
    SYMS_U32 zeroes;
    SYMS_U32 string_table_offset;
  } long_name;
} SYMS_CoffSymbolName;

typedef struct SYMS_CoffSymbol16{
  SYMS_CoffSymbolName name;
  SYMS_U32 value;
  SYMS_U16 section_number;
  union{
    struct{
      SYMS_CoffSymDType msb;
      SYMS_CoffSymType  lsb;
    } u;
    SYMS_U16 v;
  } type;
  SYMS_CoffSymStorageClass storage_class;
  SYMS_U8 number_of_aux_symbols;
} SYMS_CoffSymbol16;

typedef struct SYMS_CoffSymbol32{
  SYMS_CoffSymbolName name;
  SYMS_U32 value;
  SYMS_U32 section_number;
  union{
    struct{
      SYMS_CoffSymDType msb;
      SYMS_CoffSymType  lsb;
    } u;
    SYMS_U16 v;
  } type;
  SYMS_CoffSymStorageClass storage_class;
  SYMS_U8 number_of_aux_symbols;
} SYMS_CoffSymbol32;

#pragma pack(pop)

////////////////////////////////
// NOTE(allen): PE Magic Numbers

#define SYMS_PE_MAGIC   0x00004550u
#define SYMS_PE32_MAGIC     0x010bu
#define SYMS_PE32PLUS_MAGIC 0x020bu

////////////////////////////////
// NOTE(allen): .pdata

// TODO(allen): renaming pass here.
typedef struct SYMS_PeMipsPdata{
  SYMS_U32 voff_first;
  SYMS_U32 voff_one_past_last;
  SYMS_U32 voff_exception_handler;
  SYMS_U32 voff_exception_handler_data;
  SYMS_U32 voff_one_past_prolog;
} SYMS_PeMipsPdata;

typedef struct SYMS_PeArmPdata{
  SYMS_U32 voff_first;
  // NOTE(allen): 
  // bits    | meaning
  // [0:7]   | prolog length
  // [8:29]  | function length
  // [30:30] | instructions_are_32bits (otherwise they are 16 bits)
  // [31:31] | has_exception_handler
  SYMS_U32 combined;
} SYMS_PeArmPdata;

typedef struct SYMS_PeIntelPdata{
  SYMS_U32 voff_first;
  SYMS_U32 voff_one_past_last;
  SYMS_U32 voff_unwind_info;
} SYMS_PeIntelPdata;

////////////////////////////////
// NOTE(allen): Codeview Info

typedef struct SYMS_PeGuid{
  SYMS_U32 data1;
  SYMS_U16 data2;
  SYMS_U16 data3;
  SYMS_U8  data4[8];
} SYMS_PeGuid;
SYMS_STATIC_ASSERT(sizeof(SYMS_PeGuid) == 16);

#define SYMS_CODEVIEW_PDB20_MAGIC 0x3031424e
#define SYMS_CODEVIEW_PDB70_MAGIC 0x53445352

typedef struct SYMS_PeCvHeaderPDB20{
  SYMS_U32 magic;
  SYMS_U32 offset;
  SYMS_U32 time;
  SYMS_U32 age;
  // file name packed after struct
} SYMS_PeCvHeaderPDB20;

#pragma pack(push)
typedef struct SYMS_PeCvHeaderPDB70{
  SYMS_U32 magic;
  SYMS_PeGuid guid;
  SYMS_U32 age;
  // file name packed after struct
} SYMS_PeCvHeaderPDB70;
#pragma pack(pop)

////////////////////////////////
// Import

typedef struct SYMS_PeImportDirectoryEntry{
  SYMS_U32 lookup_table_virt_off;
  SYMS_U32 timestamp;
  SYMS_U32 forwarder_chain;
  SYMS_U32 name_virt_off;
  SYMS_U32 import_addr_table_virt_off;
} SYMS_PeImportDirectoryEntry;

////////////////////////////////
// Export

typedef struct SYMS_PeExportTable{
  SYMS_U32 flags;                       // must be zero
  SYMS_U32 timestamp;                   // time and date when export table was created
  SYMS_U16 major_ver;                   // table version, user can change major and minor version
  SYMS_U16 minor_ver; 
  SYMS_U32 name_virt_off;               // ASCII name of the dll
  SYMS_U32 ordinal_base;                // Starting oridnal number
  SYMS_U32 export_address_table_count;
  SYMS_U32 name_pointer_table_count;
  SYMS_U32 export_address_table_virt_off;
  SYMS_U32 name_pointer_table_virt_off;
  SYMS_U32 ordinal_table_virt_off;
} SYMS_PeExportTable;

#endif //SYMS_PE_H
