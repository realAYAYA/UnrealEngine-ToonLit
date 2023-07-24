// Copyright Epic Games, Inc. All Rights Reserved.
// generated
#ifndef _SYMS_META_DEBUG_INFO_C
#define _SYMS_META_DEBUG_INFO_C
//~ generated from code at syms/metaprogram/syms_metaprogram_serial.c:1150
SYMS_API SYMS_U32
syms_bit_size_from_type_kind(SYMS_TypeKind v){
SYMS_U32 result = 0;
switch (v){
default: break;
case SYMS_TypeKind_Int8: result = 8; break;
case SYMS_TypeKind_Int16: result = 16; break;
case SYMS_TypeKind_Int32: result = 32; break;
case SYMS_TypeKind_Int64: result = 64; break;
case SYMS_TypeKind_Int128: result = 128; break;
case SYMS_TypeKind_Int256: result = 256; break;
case SYMS_TypeKind_Int512: result = 512; break;
case SYMS_TypeKind_UInt8: result = 8; break;
case SYMS_TypeKind_UInt16: result = 16; break;
case SYMS_TypeKind_UInt32: result = 32; break;
case SYMS_TypeKind_UInt64: result = 64; break;
case SYMS_TypeKind_UInt128: result = 128; break;
case SYMS_TypeKind_UInt256: result = 256; break;
case SYMS_TypeKind_UInt512: result = 512; break;
case SYMS_TypeKind_Bool: result = 8; break;
case SYMS_TypeKind_Float16: result = 16; break;
case SYMS_TypeKind_Float32: result = 32; break;
case SYMS_TypeKind_Float32PP: result = 32; break;
case SYMS_TypeKind_Float48: result = 48; break;
case SYMS_TypeKind_Float64: result = 64; break;
case SYMS_TypeKind_Float80: result = 80; break;
case SYMS_TypeKind_Float128: result = 128; break;
case SYMS_TypeKind_Complex32: result = 64; break;
case SYMS_TypeKind_Complex64: result = 128; break;
case SYMS_TypeKind_Complex80: result = 160; break;
case SYMS_TypeKind_Complex128: result = 256; break;
case SYMS_TypeKind_Void: result = 0; break;
}
return(result);
}

//~ generated from code at syms/metaprogram/syms_metaprogram_serial.c:1607
#endif
