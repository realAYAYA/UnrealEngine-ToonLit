// Copyright Epic Games, Inc. All Rights Reserved.

#ifndef SYMS_PE_PARSER_C
#define SYMS_PE_PARSER_C

////////////////////////////////
// NOTE(allen): Functions

SYMS_API SYMS_PeFileAccel*
syms_pe_file_accel_from_data(SYMS_Arena *arena, SYMS_String8 data){
  // based range setup
  void *base = data.str;
  SYMS_U64Range range = {0, data.size};
  
  // read dos header
  SYMS_DosHeader dos_header = {0};
  syms_based_range_read_struct(base, range, 0, &dos_header);
  
  // fill result
  SYMS_PeFileAccel *result = (SYMS_PeFileAccel*)&syms_format_nil;
  if (dos_header.magic == SYMS_DOS_MAGIC){
    result = syms_push_array(arena, SYMS_PeFileAccel, 1);
    result->format = SYMS_FileFormat_PE;
    result->coff_off = dos_header.coff_file_offset;
  }
  
  return(result);
}

SYMS_API SYMS_PeBinAccel*
syms_pe_bin_accel_from_file(SYMS_Arena *arena, SYMS_String8 data, SYMS_PeFileAccel *accel){
  // based range setup
  void *base = data.str;
  SYMS_U64Range range = {0, data.size};
  
  // read pe magic
  SYMS_U32 coff_off = accel->coff_off;
  SYMS_U32 pe_magic = 0;
  syms_based_range_read_struct(base, range, coff_off, &pe_magic);
  
  // check pe magic
  SYMS_PeBinAccel *result = (SYMS_PeBinAccel*)&syms_format_nil;
  if (pe_magic == SYMS_PE_MAGIC){
    
    // read coff header
    SYMS_U32 coff_header_off = coff_off + sizeof(pe_magic);
    SYMS_CoffHeader coff_header = {0};
    syms_based_range_read_struct(base, range, coff_header_off, &coff_header);
    
    // range of optional extension header ("optional" for short)
    SYMS_U32 optional_size = coff_header.size_of_optional_header;
    SYMS_U64 after_coff_header_off = coff_header_off + sizeof(coff_header);
    SYMS_U64 after_optional_header_off = after_coff_header_off + optional_size;
    SYMS_U64Range optional_range = {0};
    optional_range.min = SYMS_ClampTop(after_coff_header_off, data.size);
    optional_range.max = SYMS_ClampTop(after_optional_header_off, data.size);
    
    // get sections
    SYMS_U64 sec_array_off = optional_range.max;
    SYMS_U64 sec_array_raw_opl = sec_array_off + coff_header.section_count*sizeof(SYMS_CoffSection);
    SYMS_U64 sec_array_opl = SYMS_ClampTop(sec_array_raw_opl, data.size);
    SYMS_U64 clamped_sec_count = (sec_array_opl - sec_array_off)/sizeof(SYMS_CoffSection);
    SYMS_CoffSection *sections = (SYMS_CoffSection*)((SYMS_U8*)base + sec_array_off);
    
    // read ptional header
    SYMS_U64 image_base = 0;
    SYMS_U32 data_dir_count = 0;
    SYMS_U64Range *data_dirs_file = 0;
    SYMS_U64Range *data_dirs_virt = 0;
    if (optional_size > 0){
      // read magic number
      SYMS_U16 optional_magic = 0;
      syms_based_range_read_struct(base, optional_range, 0, &optional_magic);
      
      // read optional
      SYMS_U32 reported_data_dir_offset = 0;
      SYMS_U32 reported_data_dir_count = 0;
      switch (optional_magic){
        case SYMS_PE32_MAGIC:
        {
          SYMS_PeOptionalPe32 pe_optional = {0};
          syms_based_range_read_struct(base, optional_range, 0, &pe_optional);
          
          image_base = pe_optional.image_base;
          
          reported_data_dir_offset = sizeof(pe_optional);
          reported_data_dir_count = pe_optional.data_dir_count;
        }break;
        case SYMS_PE32PLUS_MAGIC:
        {
          SYMS_PeOptionalPe32Plus pe_optional = {0};
          syms_based_range_read_struct(base, optional_range, 0, &pe_optional);
          
          image_base = pe_optional.image_base;
          
          reported_data_dir_offset = sizeof(pe_optional);
          reported_data_dir_count = pe_optional.data_dir_count;
        }break;
      }
      
      SYMS_U32 data_dir_max = (optional_size - reported_data_dir_offset)/sizeof(SYMS_PeDataDirectory);
      data_dir_count = SYMS_ClampTop(reported_data_dir_count, data_dir_max);
      // convert PE directories to ranges
      data_dirs_virt = syms_push_array(arena, SYMS_U64Range, data_dir_count);
      data_dirs_file = syms_push_array(arena, SYMS_U64Range, data_dir_count);
      for (SYMS_U32 dir_idx = 0; dir_idx < data_dir_count; dir_idx += 1){
        SYMS_U64 dir_offset = optional_range.min + reported_data_dir_offset + sizeof(SYMS_PeDataDirectory)*dir_idx;
        SYMS_PeDataDirectory dir; syms_memzero_struct(&dir);
        syms_based_range_read_struct(base, range, dir_offset, &dir);
        SYMS_U64 file_off = syms_pe_virt_off_to_file_off(sections, clamped_sec_count, dir.virt_off);
        data_dirs_virt[dir_idx] = syms_make_u64_range(dir.virt_off, dir.virt_off + dir.virt_size);
        data_dirs_file[dir_idx] = syms_make_u64_inrange(range, file_off, dir.virt_size);
      }
    }
    
    // read info about debug file
    SYMS_U32 dbg_time = 0;
    SYMS_U32 dbg_age = 0;
    SYMS_PeGuid dbg_guid = {0};
    SYMS_U64 dbg_path_off = 0;
    SYMS_U64 dbg_path_size = 0;
    
    if (SYMS_PeDataDirectoryIndex_DEBUG < data_dir_count){
      // read debug directory
      SYMS_PeDebugDirectory dbg_data = {0};
      syms_based_range_read_struct(base, data_dirs_file[SYMS_PeDataDirectoryIndex_DEBUG], 0, &dbg_data);
      // extract external file info from codeview header
      if (dbg_data.type == SYMS_PeDebugDirectoryType_CODEVIEW){
        SYMS_U64 cv_offset = dbg_data.file_offset;
        SYMS_U32 cv_magic = 0;
        syms_based_range_read_struct(base, range, cv_offset, &cv_magic);
        switch (cv_magic){
          default:break;
          case SYMS_CODEVIEW_PDB20_MAGIC:
          {
            SYMS_PeCvHeaderPDB20 cv = {0};
            syms_based_range_read_struct(base, range, cv_offset, &cv);
            // TODO(allen): can we extend the ext match key system to use this in some way?
            dbg_time = cv.time;
            dbg_age = cv.age;
            dbg_path_off = cv_offset + sizeof(cv);
          }break;
          case SYMS_CODEVIEW_PDB70_MAGIC:
          {
            SYMS_PeCvHeaderPDB70 cv = {0};
            syms_based_range_read_struct(base, range, cv_offset, &cv);
            dbg_guid = cv.guid;
            dbg_age = cv.age;
            dbg_path_off = cv_offset + sizeof(cv);
          }break;
        }
        if (dbg_path_off > 0){
          SYMS_String8 dbg_path = syms_based_range_read_string(base, range, dbg_path_off);
          dbg_path_size = dbg_path.size;
        }
      }
    }
    
    // fill result
    result = syms_push_array(arena, SYMS_PeBinAccel, 1);
    result->format = SYMS_FileFormat_PE;
    result->image_base = image_base;
    result->section_array_off = sec_array_off;
    result->section_count = clamped_sec_count;
    result->dbg_path_off = dbg_path_off;
    result->dbg_path_size = dbg_path_size;
    result->dbg_guid = dbg_guid;
    result->dbg_age = dbg_age;
    result->arch = syms_arch_from_coff_machine_type(coff_header.machine);
    // store virtual and file ranges for data directories
    result->data_dir_count = data_dir_count;
    result->data_dirs_virt = data_dirs_virt;
    result->data_dirs_file = data_dirs_file;
  }
  
  return(result);
}

SYMS_API SYMS_CoffFileAccel*
syms_coff_file_accel_from_data(SYMS_Arena *arena, SYMS_String8 data){
  SYMS_CoffFileAccel *result = (SYMS_CoffFileAccel *)&syms_format_nil;
  // TODO(allen): 
  return(result);
}

SYMS_API SYMS_CoffBinAccel*
syms_coff_bin_accel_from_file(SYMS_Arena *arena, SYMS_String8 data, SYMS_CoffFileAccel *file){
  SYMS_CoffBinAccel *result = (SYMS_CoffBinAccel *)&syms_format_nil;
  // TODO(allen): 
  return(result);
}

SYMS_API SYMS_Arch
syms_pe_arch_from_bin(SYMS_PeBinAccel *bin){
  SYMS_Arch result = bin->arch;
  return(result);
}

SYMS_API SYMS_Arch
syms_coff_arch_from_bin(SYMS_CoffBinAccel *bin){
  SYMS_Arch result = bin->arch;
  return(result);
}

SYMS_API SYMS_ExtFileList
syms_pe_ext_file_list_from_bin(SYMS_Arena *arena, SYMS_String8 data, SYMS_PeBinAccel *bin){
  SYMS_ExtFileList result = {0};
  if (bin->dbg_path_size > 0 && bin->dbg_path_off + bin->dbg_path_size <= data.size){
    SYMS_String8 name = {0};
    name.str = data.str + bin->dbg_path_off;
    name.size = bin->dbg_path_size;
    SYMS_ExtFileNode *node = syms_push_array_zero(arena, SYMS_ExtFileNode, 1);
    SYMS_QueuePush(result.first, result.last, node);
    result.node_count += 1;
    node->ext_file.file_name = name;
    syms_memmove(&node->ext_file.match_key, &bin->dbg_guid, sizeof(bin->dbg_guid));
  }
  return(result);
}

SYMS_API SYMS_CoffSection
syms_pe_coff_section(SYMS_String8 data, SYMS_PeBinAccel *bin, SYMS_U64 n){
  SYMS_CoffSection result = {0};
  if (1 <= n && n <= bin->section_count){
    SYMS_U64 off = bin->section_array_off + (n - 1)*sizeof(result);
    syms_memmove(&result, data.str + off, sizeof(result));
  }
  return(result);
}

SYMS_API SYMS_SecInfoArray
syms_pe_coff_sec_info_array_from_data(SYMS_Arena *arena, SYMS_String8 data,
                                      SYMS_U64 sec_array_off, SYMS_U64 sec_count){
  SYMS_SecInfoArray result = {0};
  // TODO(allen): 
  return(result);
}

SYMS_API SYMS_SecInfoArray
syms_pe_sec_info_array_from_bin(SYMS_Arena *arena, SYMS_String8 data, SYMS_PeBinAccel *bin){
  SYMS_SecInfoArray result = {0};
  result.count = bin->section_count;
  result.sec_info = syms_push_array_zero(arena, SYMS_SecInfo, result.count);
  
  SYMS_SecInfo *sec_info = result.sec_info;
  SYMS_CoffSection *coff_sec = (SYMS_CoffSection*)(data.str + bin->section_array_off);
  
  for (SYMS_U64 i = 0; i < result.count; i += 1, sec_info += 1, coff_sec += 1){
    // extract name
    SYMS_U8 *name = coff_sec->name;
    SYMS_U8 *name_ptr = name;
    SYMS_U8 *name_opl = name + 8;
    for (;name_ptr < name_opl && *name_ptr != 0; name_ptr += 1);
    
    // fill sec info
    sec_info->name = syms_push_string_copy(arena, syms_str8_range(name, name_ptr));
    sec_info->vrange.min = coff_sec->virt_off;
    sec_info->vrange.max = coff_sec->virt_off + coff_sec->virt_size;
    sec_info->frange.min = coff_sec->file_off;
    sec_info->frange.max = coff_sec->file_off + coff_sec->file_size;
  }
  
  return(result);
}

SYMS_API SYMS_SecInfoArray
syms_coff_sec_info_array_from_bin(SYMS_Arena *arena, SYMS_String8 data,
                                  SYMS_CoffBinAccel *bin){
  SYMS_SecInfoArray result = {0};
  // TODO(allen): 
  return(result);
}

SYMS_API SYMS_ImportArray
syms_pe_imports_from_bin(SYMS_Arena *arena, SYMS_String8 data, SYMS_PeBinAccel *bin){
  SYMS_ArenaTemp scratch = syms_get_scratch(&arena, 1);
  
  //- rjf: grab prerequisites
  void *base = (void*)data.str;
  SYMS_Arch arch = syms_pe_arch_from_bin(bin);
  (void)arch;
  SYMS_U64 arch_bit_size = syms_address_size_from_arch(bin->arch);
  SYMS_U64 arch_byte_size = arch_bit_size / 8;
  
  //- rjf: grab ranges
  SYMS_U64Range dir_range = bin->data_dirs_file[SYMS_PeDataDirectoryIndex_IMPORT];
  SYMS_U64Range data_range = syms_make_u64_range(0, data.size);
  
  //- rjf: parse import list
  SYMS_PeImportDllNode *first_dll = 0;
  SYMS_PeImportDllNode *last_dll = 0;
  SYMS_U32 dll_count = 0;
  {
    for(SYMS_U64 read_offset = 0;;)
    {
      // rjf: read entry
      SYMS_PeImportDirectoryEntry import_entry = {0};
      read_offset += syms_based_range_read_struct(base, dir_range, read_offset, &import_entry);
      
      // rjf: break if this is the last entry
      SYMS_B32 is_last_import_directory = (import_entry.lookup_table_virt_off == 0 &&
                                           import_entry.timestamp == 0 &&
                                           import_entry.forwarder_chain == 0 &&
                                           import_entry.name_virt_off == 0 &&
                                           import_entry.import_addr_table_virt_off == 0);
      if(is_last_import_directory)
      {
        break;
      }
      
      // rjf: grab offsets
      SYMS_U64 roff__dll_name_file = syms_pe_bin_virt_off_to_file_off(data, bin, import_entry.name_virt_off);
      SYMS_U64 roff__lookup_table = syms_pe_bin_virt_off_to_file_off(data, bin, import_entry.lookup_table_virt_off);
      
      // rjf: build import DLL node
      SYMS_PeImportDllNode *dll_node = syms_push_array(scratch.arena, SYMS_PeImportDllNode, 1);
      dll_node->next = 0;
      dll_node->import_entry = import_entry;
      dll_node->name = syms_based_range_read_string(base, data_range, roff__dll_name_file);
      dll_node->first_import = 0;
      dll_node->last_import = 0;
      dll_node->import_count = 0;
      dll_count += 1;
      SYMS_QueuePush(first_dll, last_dll, dll_node);
      
      // rjf: build list of all imports for this DLL
      for(;;)
      {
        // rjf: parse entry
        SYMS_U64 lookup_entry = 0;
        roff__lookup_table += syms_based_range_read(base, data_range, roff__lookup_table, arch_byte_size, &lookup_entry);
        
        // rjf: break if we're done
        if(lookup_entry == 0)
        {
          break;
        }
        
        // rjf: build node
        SYMS_PeImportNode *import_node = syms_push_array_zero(scratch.arena, SYMS_PeImportNode, 1);
        SYMS_QueuePush(dll_node->first_import, dll_node->last_import, import_node);
        dll_node->import_count += 1;
        
        // rjf: grab name/ordinal
        SYMS_B32 is_ordinal = !!((arch_bit_size == 64) ? lookup_entry & 0x8000000000000000 : lookup_entry & 0x80000000);
        if(is_ordinal)
        {
          import_node->ordinal = lookup_entry & 0xFFFF;
        }
        else
        {
          SYMS_U64 hn_file_off = syms_pe_bin_virt_off_to_file_off(data, bin, lookup_entry);
          syms_based_range_read_struct(base, data_range, hn_file_off, &import_node->hint);
          import_node->name = syms_based_range_read_string(base, data_range, hn_file_off + sizeof(import_node->hint));
        }
      }
      
    }
  }
  
  //- rjf: sum imports
  SYMS_U64 total_import_count = 0;
  for(SYMS_PeImportDllNode *n = first_dll; n != 0; n = n->next)
  {
    total_import_count += n->import_count;
  }
  
  //- rjf: build+fill result
  SYMS_ImportArray import_array = {0};
  {
    SYMS_U64 import_write_idx = 0;
    import_array.count = total_import_count;
    import_array.imports = syms_push_array_zero(arena, SYMS_Import, import_array.count);
    for(SYMS_PeImportDllNode *dll_node = first_dll;
        dll_node != 0;
        dll_node = dll_node->next)
    {
      for(SYMS_PeImportNode *import_node = dll_node->first_import;
          import_node != 0;
          import_node = import_node->next)
      {
        SYMS_Import *imp = &import_array.imports[import_write_idx];
        imp->library_name = syms_push_string_copy(arena, dll_node->name);
        imp->name = syms_push_string_copy(arena, import_node->name);
        imp->ordinal = import_node->ordinal;
        import_write_idx += 1;
      }
    }
  }
  
  //- rjf: return
  syms_release_scratch(scratch);
  return import_array;
}

SYMS_API SYMS_ExportArray
syms_pe_exports_from_bin(SYMS_Arena *arena, SYMS_String8 data, SYMS_PeBinAccel *bin){
  SYMS_U64Range dir_range = bin->data_dirs_file[SYMS_PeDataDirectoryIndex_EXPORT];
  SYMS_U64Range data_range = syms_make_u64_range(0, data.size);
  void *base = (void*)data.str;
  
  SYMS_PeExportTable header; syms_memzero_struct(&header);
  syms_based_range_read_struct(base, dir_range, 0, &header);
  
  SYMS_U32 export_address_table_file_off = syms_pe_bin_virt_off_to_file_off(data, bin, header.export_address_table_virt_off);
  SYMS_U32 name_pointer_table_file_off   = syms_pe_bin_virt_off_to_file_off(data, bin, header.name_pointer_table_virt_off);
  SYMS_U32 ordinal_table_file_off        = syms_pe_bin_virt_off_to_file_off(data, bin, header.ordinal_table_virt_off);
  
  SYMS_U64Range export_table_range       = syms_make_u64_inrange(data_range, export_address_table_file_off, sizeof(SYMS_U32) * header.export_address_table_count);
  SYMS_U64Range name_pointer_table_range = syms_make_u64_inrange(data_range, name_pointer_table_file_off,   sizeof(SYMS_U32) * header.name_pointer_table_count);
  SYMS_U64Range ordinal_table_range      = syms_make_u64_inrange(data_range, ordinal_table_file_off,        sizeof(SYMS_U16) * header.name_pointer_table_count);
  
  // NOTE(nick): There is a DLL name string in the export header. This is
  // intersting because if we read image from memory we can figure out its
  // name by reading this string.
#if 0
  SYMS_U32 dll_name_file_off = syms_pe_bin_virt_off_to_file_off(data, bin, header.name_virt_off);
  SYMS_String8 dll_name = syms_based_range_read_string(base, data_range, dll_name_file_off);
#endif
  
  SYMS_ExportArray export_array;
  export_array.count = header.name_pointer_table_count;
  export_array.exports = syms_push_array_zero(arena, SYMS_Export, export_array.count);
  
  for (SYMS_U32 i = 0; i < header.name_pointer_table_count; i += 1) {
    SYMS_U32 name_virt_off = 0;
    syms_based_range_read_struct(base, name_pointer_table_range, i * sizeof(name_virt_off), &name_virt_off);
    SYMS_U32 name_file_off = syms_pe_bin_virt_off_to_file_off(data, bin, name_virt_off);
    SYMS_String8 name = syms_based_range_read_string(base, data_range, name_file_off);
    
    SYMS_U16 ordinal = 0;
    syms_based_range_read_struct(base, ordinal_table_range, i * sizeof(ordinal), &ordinal);
    SYMS_U32 biased_ordinal = ordinal + header.ordinal_base;
    
    SYMS_U32 export_virt_off = 0;
    syms_based_range_read_struct(base, export_table_range, ordinal * sizeof(export_virt_off), &export_virt_off);
    SYMS_U32 export_file_off = syms_pe_bin_virt_off_to_file_off(data, bin, export_virt_off);
    
    SYMS_Export *exp = &export_array.exports[i];
    SYMS_B32 is_virt_off_forwarder = bin->data_dirs_virt[SYMS_PeDataDirectoryIndex_EXPORT].min <= export_virt_off && export_virt_off < bin->data_dirs_virt[SYMS_PeDataDirectoryIndex_EXPORT].max;
    if (is_virt_off_forwarder) {
      SYMS_String8 library_name = syms_str8_lit("@MALFORMED_LIBRARY_NAME@");
      SYMS_String8 import_name = syms_str8_lit("@MALFORMED_IMPORT_NAME@");
      // parse forwarder command string that is formatted like "<DLL>.<IMPORT>"
      SYMS_String8 forwarder = syms_based_range_read_string(base, data_range, export_file_off);
      SYMS_ArenaTemp temp = syms_arena_temp_begin(arena);
      SYMS_String8List list = syms_string_split(temp.arena, forwarder, '.');
      if (list.node_count == 2) {
        library_name = list.first->string;
        import_name  = list.last->string;
      }
      syms_arena_temp_end(temp);
      // fill out export
      exp->name = syms_push_string_copy(arena, name);
      exp->address = 0;
      exp->ordinal = biased_ordinal;
      exp->forwarder_library_name = syms_push_string_copy(arena, library_name);
      exp->forwarder_import_name = syms_push_string_copy(arena, import_name);
    } else {
      // fill out export
      exp->name = syms_push_string_copy(arena, name);
      exp->address = export_virt_off;
      exp->ordinal = biased_ordinal;
    }
  }
  
  return export_array;
}


////////////////////////////////
// NOTE(allen): PE Specific Helpers

SYMS_API SYMS_U64
syms_pe_binary_search_intel_pdata(SYMS_String8 data, SYMS_PeBinAccel *bin, SYMS_U64 voff){
  SYMS_ASSERT(bin->arch == SYMS_Arch_X86 || bin->arch == SYMS_Arch_X64);
  SYMS_U64 pdata_off = bin->data_dirs_file[SYMS_PeDataDirectoryIndex_EXCEPTIONS].min;
  SYMS_U64 pdata_count = syms_u64_range_size(bin->data_dirs_file[SYMS_PeDataDirectoryIndex_EXCEPTIONS]) / sizeof(SYMS_PeIntelPdata);
  
  SYMS_U64 result = 0;
  // check if this bin includes a pdata array
  if (pdata_count > 0){
    SYMS_PeIntelPdata *pdata_array = (SYMS_PeIntelPdata*)(data.str + pdata_off);
    if (voff >= pdata_array[0].voff_first){
      
      // binary search:
      //  find max index s.t. pdata_array[index].voff_first <= voff
      //  we assume (i < j) -> (pdata_array[i].voff_first < pdata_array[j].voff_first)
      SYMS_U64 index = pdata_count;
      SYMS_U64 min = 0;
      SYMS_U64 opl = pdata_count;
      for (;;){
        SYMS_U64 mid = (min + opl)/2;
        SYMS_PeIntelPdata *pdata = pdata_array + mid;
        if (voff < pdata->voff_first){
          opl = mid;
        }
        else if (pdata->voff_first < voff){
          min = mid;
        }
        else{
          index = mid;
          break;
        }
        if (min + 1 >= opl){
          index = min;
          break;
        }
      }
      
      // if we are in range fill result
      {
        SYMS_PeIntelPdata *pdata = pdata_array + index;
        if (pdata->voff_first <= voff && voff < pdata->voff_one_past_last){
          result = pdata_off + index*sizeof(SYMS_PeIntelPdata);
        }
      }
    }
  }
  
  return(result);
}

SYMS_API SYMS_U64
syms_pe_sec_number_from_voff(SYMS_String8 data, SYMS_PeBinAccel *bin, SYMS_U64 voff){
  SYMS_U64 sec_count = bin->section_count;
  SYMS_CoffSection *sec_array = (SYMS_CoffSection*)((SYMS_U8*)data.str + bin->section_array_off);
  SYMS_CoffSection *sec_ptr = sec_array;
  SYMS_U64 result = 0;
  for (SYMS_U64 i = 1; i <= sec_count; i += 1, sec_ptr += 1){
    if (sec_ptr->virt_off <= voff && voff < sec_ptr->virt_off + sec_ptr->virt_size){
      result = i;
      break;
    }
  }
  return(result);
}

SYMS_API void*
syms_pe_ptr_from_sec_number(SYMS_String8 data, SYMS_PeBinAccel *bin, SYMS_U64 n){
  void *result = 0;
  SYMS_U64 sec_count = bin->section_count;
  if (1 <= n && n <= sec_count){
    SYMS_CoffSection *sec_array = (SYMS_CoffSection*)((SYMS_U8*)data.str + bin->section_array_off);
    SYMS_CoffSection *sec = sec_array + n - 1;
    if (sec->file_size > 0){
      result = data.str + sec->file_off;
    }
  }
  return(result);
}

SYMS_API void*
syms_pe_ptr_from_foff(SYMS_String8 data, SYMS_PeBinAccel *bin, SYMS_U64 foff){
  void *result = 0;
  if (foff < data.size){
    result = data.str + foff;
  }
  return(result);
}

SYMS_API void*
syms_pe_ptr_from_voff(SYMS_String8 data, SYMS_PeBinAccel *bin, SYMS_U64 voff){
  //- get the section for this voff
  SYMS_U64 sec_count = bin->section_count;
  SYMS_CoffSection *sec_array = (SYMS_CoffSection*)((SYMS_U8*)data.str + bin->section_array_off);
  SYMS_CoffSection *sec_ptr = sec_array;
  SYMS_CoffSection *sec = 0;
  for (SYMS_U64 i = 1; i <= sec_count; i += 1, sec_ptr += 1){
    if (sec_ptr->virt_off <= voff && voff < sec_ptr->virt_off + sec_ptr->virt_size){
      sec = sec_ptr;
      break;
    }
  }
  
  //- adjust to file pointer
  void *result = 0;
  if (sec != 0 & sec_ptr->file_size > 0){
    result = data.str + voff - sec->virt_off + sec->file_off;
  }
  
  return(result);
}

SYMS_API SYMS_U64
syms_pe_virt_off_to_file_off(SYMS_CoffSection *sections, SYMS_U32 section_count, SYMS_U64 virt_off){
  SYMS_U64 file_off = 0;
  for (SYMS_U32 sect_idx = 0; sect_idx < section_count; sect_idx += 1){
    SYMS_CoffSection *sect = &sections[sect_idx];
    if (sect->virt_off <= virt_off && virt_off < sect->virt_off + sect->virt_size){
      file_off = sect->file_off + (virt_off - sect->virt_off);
    }
  }
  return file_off;
}

SYMS_API SYMS_U64
syms_pe_bin_virt_off_to_file_off(SYMS_String8 data, SYMS_PeBinAccel *bin, SYMS_U64 virt_off){
  return syms_pe_virt_off_to_file_off((SYMS_CoffSection*)((SYMS_U8*)data.str + bin->section_array_off), bin->section_count, virt_off);
}

#endif //SYMS_PE_PARSER_C
