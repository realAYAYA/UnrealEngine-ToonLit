// Copyright Epic Games, Inc. All Rights Reserved.
// generated
#ifndef _SYMS_META_EVAL_H
#define _SYMS_META_EVAL_H
//~ generated from code at syms/metaprogram/syms_metaprogram_eval.c:339
typedef enum{
SYMS_EvalOp_Stop,
SYMS_EvalOp_Cond,
SYMS_EvalOp_Skip,
SYMS_EvalOp_MemRead,
SYMS_EvalOp_RegRead,
SYMS_EvalOp_RegReadDyn,
SYMS_EvalOp_FrameOff,
SYMS_EvalOp_ModuleOff,
SYMS_EvalOp_TLSOff,
SYMS_EvalOp_ObjectOff,
SYMS_EvalOp_CFA,
SYMS_EvalOp_ConstU8,
SYMS_EvalOp_ConstU16,
SYMS_EvalOp_ConstU32,
SYMS_EvalOp_ConstU64,
SYMS_EvalOp_Abs,
SYMS_EvalOp_Neg,
SYMS_EvalOp_Add,
SYMS_EvalOp_Sub,
SYMS_EvalOp_Mul,
SYMS_EvalOp_Div,
SYMS_EvalOp_Mod,
SYMS_EvalOp_LShift,
SYMS_EvalOp_RShift,
SYMS_EvalOp_BitAnd,
SYMS_EvalOp_BitOr,
SYMS_EvalOp_BitXor,
SYMS_EvalOp_BitNot,
SYMS_EvalOp_LogAnd,
SYMS_EvalOp_LogOr,
SYMS_EvalOp_LogNot,
SYMS_EvalOp_EqEq,
SYMS_EvalOp_NtEq,
SYMS_EvalOp_LsEq,
SYMS_EvalOp_GrEq,
SYMS_EvalOp_Less,
SYMS_EvalOp_Grtr,
SYMS_EvalOp_Trunc,
SYMS_EvalOp_TruncSigned,
SYMS_EvalOp_Convert,
SYMS_EvalOp_Pick,
SYMS_EvalOp_Pop,
SYMS_EvalOp_Insert,
SYMS_EvalOp_COUNT,
} SYMS_EvalOp;
//~ generated from code at syms/metaprogram/syms_metaprogram_eval.c:353
enum{
SYMS_EvalOpCtrlBits_DecodeMask = 0xf,
SYMS_EvalOpCtrlBits_DecodeShft = 4,
SYMS_EvalOpCtrlBits_PopMask = 0x3,
SYMS_EvalOpCtrlBits_PopShft = 2,
SYMS_EvalOpCtrlBits_PushMask = 0x3,
SYMS_EvalOpCtrlBits_PushShft = 0,
};
//~ generated from code at syms/metaprogram/syms_metaprogram_eval.c:400
typedef enum{
SYMS_EvalTypeGroup_Other,
SYMS_EvalTypeGroup_U,
SYMS_EvalTypeGroup_S,
SYMS_EvalTypeGroup_F32,
SYMS_EvalTypeGroup_F64,
SYMS_EvalTypeGroup_COUNT,
} SYMS_EvalTypeGroup;
//~ generated from code at syms/metaprogram/syms_metaprogram_eval.c:428
typedef enum{
SYMS_EvalConversionKind_Noop,
SYMS_EvalConversionKind_Legal,
SYMS_EvalConversionKind_OtherToOther,
SYMS_EvalConversionKind_ToOther,
SYMS_EvalConversionKind_FromOther,
SYMS_EvalConversionKind_COUNT,
} SYMS_EvalConversionKind;
#endif
