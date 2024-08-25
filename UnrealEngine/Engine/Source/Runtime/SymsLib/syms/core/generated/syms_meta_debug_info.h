// Copyright Epic Games, Inc. All Rights Reserved.
// generated
#ifndef _SYMS_META_DEBUG_INFO_H
#define _SYMS_META_DEBUG_INFO_H
//~ generated from code at syms/metaprogram/syms_metaprogram_serial.c:885
typedef SYMS_U32 SYMS_UnitFeatures;
enum{
SYMS_UnitFeature_CompilationUnit = (1 << 0),
SYMS_UnitFeature_Types = (1 << 1),
SYMS_UnitFeature_StaticVariables = (1 << 2),
SYMS_UnitFeature_ExternVariables = (1 << 3),
SYMS_UnitFeature_Functions = (1 << 4),
SYMS_UnitFeature_FunctionStubs = (1 << 5),
};
typedef enum SYMS_SymbolKind{
SYMS_SymbolKind_Null,
SYMS_SymbolKind_Type,
SYMS_SymbolKind_Procedure,
SYMS_SymbolKind_ImageRelativeVariable,
SYMS_SymbolKind_LocalVariable,
SYMS_SymbolKind_TLSVariable,
SYMS_SymbolKind_Const,
SYMS_SymbolKind_Scope,
SYMS_SymbolKind_Inline,
SYMS_SymbolKind_COUNT = 9
} SYMS_SymbolKind;
typedef enum SYMS_TypeKind{
SYMS_TypeKind_Null,
SYMS_TypeKind_Stub,
//  @maintenance(allen) sync with 'syms_type_kind_is_basic'
SYMS_TypeKind_Int8,
SYMS_TypeKind_Int16,
SYMS_TypeKind_Int32,
SYMS_TypeKind_Int64,
SYMS_TypeKind_Int128,
SYMS_TypeKind_Int256,
SYMS_TypeKind_Int512,
SYMS_TypeKind_UInt8,
SYMS_TypeKind_UInt16,
SYMS_TypeKind_UInt32,
SYMS_TypeKind_UInt64,
SYMS_TypeKind_UInt128,
SYMS_TypeKind_UInt256,
SYMS_TypeKind_UInt512,
SYMS_TypeKind_Bool,
SYMS_TypeKind_Float16,
SYMS_TypeKind_Float32,
SYMS_TypeKind_Float32PP,
SYMS_TypeKind_Float48,
SYMS_TypeKind_Float64,
SYMS_TypeKind_Float80,
SYMS_TypeKind_Float128,
SYMS_TypeKind_Complex32,
SYMS_TypeKind_Complex64,
SYMS_TypeKind_Complex80,
SYMS_TypeKind_Complex128,
SYMS_TypeKind_Void,
//                                'syms_type_kind_is_record'
SYMS_TypeKind_Struct,
SYMS_TypeKind_Class,
SYMS_TypeKind_Union,
SYMS_TypeKind_Enum,
SYMS_TypeKind_Typedef,
//  @maintenance(allen) sync with 'syms_type_kind_is_forward'
SYMS_TypeKind_ForwardStruct,
SYMS_TypeKind_ForwardClass,
SYMS_TypeKind_ForwardUnion,
SYMS_TypeKind_ForwardEnum,
SYMS_TypeKind_Modifier,
SYMS_TypeKind_Ptr,
SYMS_TypeKind_LValueReference,
SYMS_TypeKind_RValueReference,
SYMS_TypeKind_MemberPtr,
SYMS_TypeKind_Array,
SYMS_TypeKind_Proc,
SYMS_TypeKind_Bitfield,
SYMS_TypeKind_Variadic,
SYMS_TypeKind_Label,
SYMS_TypeKind_COUNT = 48
} SYMS_TypeKind;
typedef SYMS_U32 SYMS_TypeModifiers;
enum{
SYMS_TypeModifier_Const = (1 << 0),
SYMS_TypeModifier_Packed = (1 << 1),
SYMS_TypeModifier_Restrict = (1 << 2),
SYMS_TypeModifier_Shared = (1 << 3),
SYMS_TypeModifier_Volatile = (1 << 4),
SYMS_TypeModifier_Char = (1 << 5),
SYMS_TypeModifier_Reference = (1 << 6),
SYMS_TypeModifier_RValueReference = (1 << 7),
};
typedef enum SYMS_MemVisibility{
SYMS_MemVisibility_Null,
SYMS_MemVisibility_Private,
SYMS_MemVisibility_Public,
SYMS_MemVisibility_Protected,
SYMS_MemVisibility_COUNT = 4
} SYMS_MemVisibility;

//~ generated from code at syms/metaprogram/syms_metaprogram_serial.c:1133
SYMS_C_LINKAGE_BEGIN
SYMS_API SYMS_U32 syms_bit_size_from_type_kind(SYMS_TypeKind v);
SYMS_C_LINKAGE_END

//~ generated from code at syms/metaprogram/syms_metaprogram_serial.c:1588
SYMS_C_LINKAGE_BEGIN
SYMS_C_LINKAGE_END

//~ generated from code at syms/metaprogram/syms_metaprogram_serial.c:1694
#endif
