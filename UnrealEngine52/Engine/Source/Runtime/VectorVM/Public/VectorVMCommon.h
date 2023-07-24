// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include <atomic>
#include "UObject/ObjectMacros.h"

#include "VectorVMCommon.generated.h"

//TODO: move to a per platform header and have VM scale vectorization according to vector width.
#define VECTOR_WIDTH (128)
#define VECTOR_WIDTH_BYTES (16)
#define VECTOR_WIDTH_FLOATS (4)

UENUM()
enum class EVectorVMBaseTypes : uint8
{
	Float,
	Int,
	Bool,
	Num UMETA(Hidden),
};

UENUM()
enum class EVectorVMOperandLocation : uint8
{
	Register,
	Constant,
	Num
};

//              OpCode                       Category   num params	int/float flags
#define VVM_OP_XM_LIST																																									                                   \
	VVM_OP_XM( done                         , Other     , 0			, VVM_INS_PARAM_FFFFFF              ) /* 0  */                                                                                                         \
	VVM_OP_XM( add                          , Op        , 3			, VVM_INS_PARAM_FFFFFF              ) /* 1  */                                                                                                         \
	VVM_OP_XM( sub                          , Op        , 3			, VVM_INS_PARAM_FFFFFF              ) /* 2  */                                                                                                         \
	VVM_OP_XM( mul                          , Op        , 3			, VVM_INS_PARAM_FFFFFF              ) /* 3  */                                                                                                         \
	VVM_OP_XM( div                          , Op        , 3			, VVM_INS_PARAM_FFFFFF              ) /* 4  */                                                                                                         \
	VVM_OP_XM( mad                          , Op        , 4			, VVM_INS_PARAM_FFFFFF              ) /* 5  */                                                                                                         \
	VVM_OP_XM( lerp                         , Op        , 4			, VVM_INS_PARAM_FFFFFF              ) /* 6  */                                                                                                         \
	VVM_OP_XM( rcp                          , Op        , 2			, VVM_INS_PARAM_FFFFFF              ) /* 7  */                                                                                                         \
	VVM_OP_XM( rsq                          , Op        , 2			, VVM_INS_PARAM_FFFFFF              ) /* 8  */                                                                                                         \
	VVM_OP_XM( sqrt                         , Op        , 2			, VVM_INS_PARAM_FFFFFF              ) /* 9  */                                                                                                         \
	VVM_OP_XM( neg                          , Op        , 2			, VVM_INS_PARAM_FFFFFF              ) /* 10 */                                                                                                         \
	VVM_OP_XM( abs                          , Op        , 2			, VVM_INS_PARAM_FFFFFF              ) /* 11 */                                                                                                         \
	VVM_OP_XM( exp                          , Op        , 2			, VVM_INS_PARAM_FFFFFF              ) /* 12 */                                                                                                         \
	VVM_OP_XM( exp2                         , Op        , 2			, VVM_INS_PARAM_FFFFFF              ) /* 13 */                                                                                                         \
	VVM_OP_XM( log                          , Op        , 2			, VVM_INS_PARAM_FFFFFF              ) /* 14 */                                                                                                         \
	VVM_OP_XM( log2                         , Op        , 2			, VVM_INS_PARAM_FFFFFF              ) /* 15 */                                                                                                         \
	VVM_OP_XM( sin                          , Op        , 2			, VVM_INS_PARAM_FFFFFF              ) /* 16 */                                                                                                         \
	VVM_OP_XM( cos                          , Op        , 2			, VVM_INS_PARAM_FFFFFF              ) /* 17 */                                                                                                         \
	VVM_OP_XM( tan                          , Op        , 2			, VVM_INS_PARAM_FFFFFF              ) /* 18 */                                                                                                         \
	VVM_OP_XM( asin                         , Op        , 2			, VVM_INS_PARAM_FFFFFF              ) /* 19 */                                                                                                         \
	VVM_OP_XM( acos                         , Op        , 2			, VVM_INS_PARAM_FFFFFF              ) /* 20 */                                                                                                         \
	VVM_OP_XM( atan                         , Op        , 2			, VVM_INS_PARAM_FFFFFF              ) /* 21 */                                                                                                         \
	VVM_OP_XM( atan2                        , Op        , 3			, VVM_INS_PARAM_FFFFFF              ) /* 22 */                                                                                                         \
	VVM_OP_XM( ceil                         , Op        , 2			, VVM_INS_PARAM_FFFFFF              ) /* 23 */                                                                                                         \
	VVM_OP_XM( floor                        , Op        , 2			, VVM_INS_PARAM_FFFFFF              ) /* 24 */                                                                                                         \
	VVM_OP_XM( fmod                         , Op        , 3			, VVM_INS_PARAM_FFFFFF              ) /* 25 */                                                                                                         \
	VVM_OP_XM( frac                         , Op        , 2			, VVM_INS_PARAM_FFFFFF              ) /* 26 */                                                                                                         \
	VVM_OP_XM( trunc                        , Op        , 2			, VVM_INS_PARAM_FFFFFF              ) /* 27 */                                                                                                         \
	VVM_OP_XM( clamp                        , Op        , 4			, VVM_INS_PARAM_FFFFFF              ) /* 28 */                                                                                                         \
	VVM_OP_XM( min                          , Op        , 3			, VVM_INS_PARAM_FFFFFF              ) /* 29 */                                                                                                         \
	VVM_OP_XM( max                          , Op        , 3			, VVM_INS_PARAM_FFFFFF              ) /* 30 */                                                                                                         \
	VVM_OP_XM( pow                          , Op        , 3			, VVM_INS_PARAM_FFFFFF              ) /* 31 */                                                                                                         \
	VVM_OP_XM( round                        , Op        , 2			, VVM_INS_PARAM_FFFFFF              ) /* 32 */                                                                                                         \
	VVM_OP_XM( sign                         , Op        , 2			, VVM_INS_PARAM_FFFFFF              ) /* 33 */                                                                                                         \
	VVM_OP_XM( step                         , Op        , 3			, VVM_INS_PARAM_FFFFFF              ) /* 34 */                                                                                                         \
	VVM_OP_XM( random                       , Op        , 2			, VVM_INS_PARAM_FFFFFF              ) /* 35 */                                                                                                         \
	VVM_OP_XM( noise                        , Op        , 0			, VVM_INS_PARAM_FFFFFF              ) /* 36 */                                                                                                         \
	VVM_OP_XM( cmplt                        , Op        , 3			, VVM_INS_PARAM_FFFIFF              ) /* 37 */                                                                                                         \
	VVM_OP_XM( cmple                        , Op        , 3			, VVM_INS_PARAM_FFFIFF              ) /* 38 */                                                                                                         \
	VVM_OP_XM( cmpgt                        , Op        , 3			, VVM_INS_PARAM_FFFIFF              ) /* 39 */                                                                                                         \
	VVM_OP_XM( cmpge                        , Op        , 3			, VVM_INS_PARAM_FFFIFF              ) /* 40 */                                                                                                         \
	VVM_OP_XM( cmpeq                        , Op        , 3			, VVM_INS_PARAM_FFFIFF              ) /* 41 */                                                                                                         \
	VVM_OP_XM( cmpneq                       , Op        , 3			, VVM_INS_PARAM_FFFIFF              ) /* 42 */                                                                                                         \
	VVM_OP_XM( select                       , Op        , 4			, VVM_INS_PARAM_FFIFFF              ) /* 43 */                                                                                                         \
	VVM_OP_XM( addi                         , Op        , 3			, VVM_INS_PARAM_FFIIII              ) /* 44 */                                                                                                         \
	VVM_OP_XM( subi                         , Op        , 3			, VVM_INS_PARAM_FFIIII              ) /* 45 */                                                                                                         \
	VVM_OP_XM( muli                         , Op        , 3			, VVM_INS_PARAM_FFIIII              ) /* 46 */                                                                                                         \
	VVM_OP_XM( divi                         , Op        , 3			, VVM_INS_PARAM_FFIIII              ) /* 47 //SSE Integer division is not implemented as an intrinsic. Will have to do some manual implementation. */  \
	VVM_OP_XM( clampi                       , Op        , 4			, VVM_INS_PARAM_FFIIII              ) /* 48 */                                                                                                         \
	VVM_OP_XM( mini                         , Op        , 3			, VVM_INS_PARAM_FFIIII              ) /* 49 */                                                                                                         \
	VVM_OP_XM( maxi                         , Op        , 3			, VVM_INS_PARAM_FFIIII              ) /* 50 */                                                                                                         \
	VVM_OP_XM( absi                         , Op        , 2			, VVM_INS_PARAM_FFIIII              ) /* 51 */                                                                                                         \
	VVM_OP_XM( negi                         , Op        , 2			, VVM_INS_PARAM_FFIIII              ) /* 52 */                                                                                                         \
	VVM_OP_XM( signi                        , Op        , 2			, VVM_INS_PARAM_FFIIII              ) /* 53 */                                                                                                         \
	VVM_OP_XM( randomi                      , Op        , 2			, VVM_INS_PARAM_FFIIII              ) /* 54 */                                                                                                         \
	VVM_OP_XM( cmplti                       , Op        , 3			, VVM_INS_PARAM_FFIIII              ) /* 55 */                                                                                                         \
	VVM_OP_XM( cmplei                       , Op        , 3			, VVM_INS_PARAM_FFIIII              ) /* 56 */                                                                                                         \
	VVM_OP_XM( cmpgti                       , Op        , 3			, VVM_INS_PARAM_FFIIII              ) /* 57 */                                                                                                         \
	VVM_OP_XM( cmpgei                       , Op        , 3			, VVM_INS_PARAM_FFIIII              ) /* 58 */                                                                                                         \
	VVM_OP_XM( cmpeqi                       , Op        , 3			, VVM_INS_PARAM_FFIIII              ) /* 59 */                                                                                                         \
	VVM_OP_XM( cmpneqi                      , Op        , 3			, VVM_INS_PARAM_FFIIII              ) /* 60 */                                                                                                         \
	VVM_OP_XM( bit_and                      , Op        , 3			, VVM_INS_PARAM_FFIIII              ) /* 61 */                                                                                                         \
	VVM_OP_XM( bit_or                       , Op        , 3			, VVM_INS_PARAM_FFIIII              ) /* 62 */                                                                                                         \
	VVM_OP_XM( bit_xor                      , Op        , 3			, VVM_INS_PARAM_FFIIII              ) /* 63 */                                                                                                         \
	VVM_OP_XM( bit_not                      , Op        , 3			, VVM_INS_PARAM_FFIIII              ) /* 64 */                                                                                                         \
	VVM_OP_XM( bit_lshift                   , Op        , 3			, VVM_INS_PARAM_FFIIII              ) /* 65 */                                                                                                         \
	VVM_OP_XM( bit_rshift                   , Op        , 3			, VVM_INS_PARAM_FFIIII              ) /* 66 */                                                                                                         \
	VVM_OP_XM( logic_and                    , Op        , 3			, VVM_INS_PARAM_FFIIII              ) /* 67 */                                                                                                         \
	VVM_OP_XM( logic_or                     , Op        , 3			, VVM_INS_PARAM_FFIIII              ) /* 68 */                                                                                                         \
	VVM_OP_XM( logic_xor                    , Op        , 3			, VVM_INS_PARAM_FFIIII              ) /* 69 */                                                                                                         \
	VVM_OP_XM( logic_not                    , Op        , 2			, VVM_INS_PARAM_FFIIII              ) /* 70 */                                                                                                         \
	VVM_OP_XM( f2i                          , Op        , 2			, VVM_INS_PARAM_FFFFFI              ) /* 71 */                                                                                                         \
	VVM_OP_XM( i2f                          , Op        , 2			, VVM_INS_PARAM_FFFFIF              ) /* 72 */                                                                                                         \
	VVM_OP_XM( f2b                          , Op        , 2			, VVM_INS_PARAM_FFFFFI              ) /* 73 */                                                                                                         \
	VVM_OP_XM( b2f                          , Op        , 2			, VVM_INS_PARAM_FFFFIF              ) /* 74 */                                                                                                         \
	VVM_OP_XM( i2b                          , Op        , 2			, VVM_INS_PARAM_FFFFII              ) /* 75 */                                                                                                         \
	VVM_OP_XM( b2i                          , Op        , 2			, VVM_INS_PARAM_FFFFII              ) /* 76 */                                                                                                         \
	VVM_OP_XM( inputdata_float              , Input     , 3			, VVM_INS_PARAM_FFFFFF              ) /* 77 */                                                                                                         \
	VVM_OP_XM( inputdata_int32              , Input     , 3			, VVM_INS_PARAM_FFFFFF              ) /* 78 */                                                                                                         \
	VVM_OP_XM( inputdata_half               , Input     , 3			, VVM_INS_PARAM_FFFFFF              ) /* 79 */                                                                                                         \
	VVM_OP_XM( inputdata_noadvance_float    , Input     , 3			, VVM_INS_PARAM_FFFFFF              ) /* 80 */                                                                                                         \
	VVM_OP_XM( inputdata_noadvance_int32    , Input     , 3			, VVM_INS_PARAM_FFFFFF              ) /* 81 */                                                                                                         \
	VVM_OP_XM( inputdata_noadvance_half     , Input     , 3			, VVM_INS_PARAM_FFFFFF              ) /* 82 */                                                                                                         \
	VVM_OP_XM( outputdata_float             , Output    , 3			, VVM_INS_PARAM_FFFFFI              ) /* 83 */                                                                                                         \
	VVM_OP_XM( outputdata_int32             , Output    , 3			, VVM_INS_PARAM_FFFIII              ) /* 84 */                                                                                                         \
	VVM_OP_XM( outputdata_half              , Output    , 3			, VVM_INS_PARAM_FFFFFI              ) /* 85 */                                                                                                         \
	VVM_OP_XM( acquireindex                 , IndexGen  , 2			, VVM_INS_PARAM_FFFFII              ) /* 86 */                                                                                                         \
	VVM_OP_XM( external_func_call           , ExtFnCall , 0			, VVM_INS_PARAM_FFFFFF              ) /* 87 */                                                                                                         \
	VVM_OP_XM( exec_index                   , Op        , 1         , VVM_INS_PARAM_FFFFFI              ) /* 88 */                                                                                                         \
	VVM_OP_XM( noise2D                      , Other     , 0			, VVM_INS_PARAM_FFFFFF              ) /* 89 */                                                                                                         \
	VVM_OP_XM( noise3D                      , Other     , 0			, VVM_INS_PARAM_FFFFFF              ) /* 90 */                                                                                                         \
	VVM_OP_XM( enter_stat_scope             , Stat      , 1			, VVM_INS_PARAM_FFFFFF              ) /* 91 */                                                                                                         \
	VVM_OP_XM( exit_stat_scope              , Stat      , 0			, VVM_INS_PARAM_FFFFFF              ) /* 92 */                                                                                                         \
	VVM_OP_XM( update_id                    , RWBuffer  , 3			, VVM_INS_PARAM_FFFFFF              ) /* 93 */                                                                                                         \
	VVM_OP_XM( acquire_id                   , RWBuffer  , 3			, VVM_INS_PARAM_FFFFFF              ) /* 94 */                                                                                                         \
	VVM_OP_XM( half_to_float                , Fused     , 2			, VVM_INS_PARAM_FFFFFF              ) /* 95 */                                                                                                         \
	/* Fused ops -- combined ops that show up frequently together in Fornite.  There are three types: */                                                                                                                   \
	/* 1. exec_index that get immediately fed into an add or i2f */                                                                                                                                                        \
	/* 2. ops with identical inputs */                                                                                                                                                                                     \
	/* 3. ops where the output of one chain to the next.. ie a mul that feeds directly into a sub */                                                                                                                       \
	VVM_OP_XM( exec_indexf                  , Op        , 2         , VVM_INS_PARAM_FFIIIF              ) /* 96 */                                                                                                         \
	VVM_OP_XM( exec_index_addi              , Op        , 2         , VVM_INS_PARAM_FFIIII              ) /* 97 */                                                                                                         \
	\
	VVM_OP_XM( cmplt_select                 , Op        , 5         , VVM_INS_PARAM_FFFFFF              ) /* 98 */                                                                                                         \
	VVM_OP_XM( cmple_select                 , Op        , 5         , VVM_INS_PARAM_FFFFFF              ) /* 99 */                                                                                                        \
	VVM_OP_XM( cmpeq_select                 , Op        , 5         , VVM_INS_PARAM_FFFFFF              ) /* 100 */                                                                                                        \
	VVM_OP_XM( cmplti_select                , Op        , 5         , VVM_INS_PARAM_FIIFFF              ) /* 101 */                                                                                                        \
	VVM_OP_XM( cmplei_select                , Op        , 5         , VVM_INS_PARAM_FIIFFF              ) /* 102 */                                                                                                        \
	VVM_OP_XM( cmpeqi_select                , Op        , 5         , VVM_INS_PARAM_FIIFFF              ) /* 103 */                                                                                                        \
	\
	VVM_OP_XM( cmplt_logic_and              , Op        , 4         , VVM_INS_PARAM_FFFFII              ) /* 104 */                                                                                                        \
	VVM_OP_XM( cmple_logic_and              , Op        , 4         , VVM_INS_PARAM_FFFFII              ) /* 105 */                                                                                                        \
	VVM_OP_XM( cmpgt_logic_and              , Op        , 4         , VVM_INS_PARAM_FFFFII              ) /* 106 */                                                                                                        \
	VVM_OP_XM( cmpge_logic_and              , Op        , 4         , VVM_INS_PARAM_FFFFII              ) /* 107 */                                                                                                        \
	VVM_OP_XM( cmpeq_logic_and              , Op        , 4         , VVM_INS_PARAM_FFFFII              ) /* 108 */                                                                                                        \
	VVM_OP_XM( cmpne_logic_and              , Op        , 4         , VVM_INS_PARAM_FFFFII              ) /* 109 */                                                                                                        \
	VVM_OP_XM( cmplti_logic_and             , Op        , 4         , VVM_INS_PARAM_FIIFII              ) /* 110 */                                                                                                        \
	VVM_OP_XM( cmplei_logic_and             , Op        , 4         , VVM_INS_PARAM_FIIFII              ) /* 111 */                                                                                                        \
	VVM_OP_XM( cmpgti_logic_and             , Op        , 4         , VVM_INS_PARAM_FIIFII              ) /* 112 */                                                                                                        \
	VVM_OP_XM( cmpgei_logic_and             , Op        , 4         , VVM_INS_PARAM_FIIFIF              ) /* 113 */                                                                                                        \
	VVM_OP_XM( cmpeqi_logic_and             , Op        , 4         , VVM_INS_PARAM_FIIFIF              ) /* 114 */                                                                                                        \
	VVM_OP_XM( cmpnei_logic_and             , Op        , 4         , VVM_INS_PARAM_FIIFIF              ) /* 115 */                                                                                                        \
	\
	VVM_OP_XM( cmplt_logic_or               , Op        , 4         , VVM_INS_PARAM_FFFFFF              ) /* 116 */                                                                                                        \
	VVM_OP_XM( cmple_logic_or               , Op        , 4         , VVM_INS_PARAM_FFFFFF              ) /* 117 */                                                                                                        \
	VVM_OP_XM( cmpgt_logic_or               , Op        , 4         , VVM_INS_PARAM_FFFFFF              ) /* 118 */                                                                                                        \
	VVM_OP_XM( cmpge_logic_or               , Op        , 4         , VVM_INS_PARAM_FFFFFF              ) /* 119 */                                                                                                        \
	VVM_OP_XM( cmpeq_logic_or               , Op        , 4         , VVM_INS_PARAM_FFFFFF              ) /* 120 */                                                                                                        \
	VVM_OP_XM( cmpne_logic_or               , Op        , 4         , VVM_INS_PARAM_FFFFFF              ) /* 121 */                                                                                                        \
	VVM_OP_XM( cmplti_logic_or              , Op        , 4         , VVM_INS_PARAM_FIIFFF              ) /* 122 */                                                                                                        \
	VVM_OP_XM( cmplei_logic_or              , Op        , 4         , VVM_INS_PARAM_FIIFFF              ) /* 123 */                                                                                                        \
	VVM_OP_XM( cmpgti_logic_or              , Op        , 4         , VVM_INS_PARAM_FIIFFF              ) /* 124 */                                                                                                        \
	VVM_OP_XM( cmpgei_logic_or              , Op        , 4         , VVM_INS_PARAM_FIIFFF              ) /* 125 */                                                                                                        \
	VVM_OP_XM( cmpeqi_logic_or              , Op        , 4         , VVM_INS_PARAM_FIIFFF              ) /* 126 */                                                                                                        \
	VVM_OP_XM( cmpnei_logic_or              , Op        , 4         , VVM_INS_PARAM_FIIFFF              ) /* 127 */                                                                                                        \
	\
	VVM_OP_XM( mad_add                      , Op        , 5         , VVM_INS_PARAM_FFFFFF              ) /* 128 */                                                                                                        \
	VVM_OP_XM( mad_sub0                     , Op        , 5         , VVM_INS_PARAM_FFFFFF              ) /* 129 */                                                                                                        \
	VVM_OP_XM( mad_sub1                     , Op        , 5         , VVM_INS_PARAM_FFFFFF              ) /* 130 */                                                                                                        \
	VVM_OP_XM( mad_mul                      , Op        , 5         , VVM_INS_PARAM_FFFFFF              ) /* 131 */                                                                                                        \
	VVM_OP_XM( mad_sqrt                     , Op        , 4         , VVM_INS_PARAM_FFFFFF              ) /* 132 */                                                                                                        \
	VVM_OP_XM( mad_mad0                     , Op        , 6         , VVM_INS_PARAM_FFFFFF              ) /* 133 */                                                                                                        \
	VVM_OP_XM( mad_mad1                     , Op        , 6         , VVM_INS_PARAM_FFFFFF              ) /* 134 */                                                                                                        \
	\
	VVM_OP_XM( mul_mad0                     , Op        , 5         , VVM_INS_PARAM_FFFFFF              ) /* 135 */                                                                                                        \
	VVM_OP_XM( mul_mad1                     , Op        , 5         , VVM_INS_PARAM_FFFFFF              ) /* 136 */                                                                                                        \
	VVM_OP_XM( mul_add                      , Op        , 4         , VVM_INS_PARAM_FFFFFF              ) /* 137 */                                                                                                        \
	VVM_OP_XM( mul_sub0                     , Op        , 4         , VVM_INS_PARAM_FFFFFF              ) /* 138 */                                                                                                        \
	VVM_OP_XM( mul_sub1                     , Op        , 4         , VVM_INS_PARAM_FFFFFF              ) /* 139 */                                                                                                        \
	VVM_OP_XM( mul_mul                      , Op        , 4         , VVM_INS_PARAM_FFFFFF              ) /* 140 */                                                                                                        \
	VVM_OP_XM( mul_max                      , Op        , 4         , VVM_INS_PARAM_FFFFFF              ) /* 141 */                                                                                                        \
	VVM_OP_XM( mul_2x                       , Op        , 4         , VVM_INS_PARAM_FFFFFF              ) /* 142 */                                                                                                        \
	\
	VVM_OP_XM( add_mad1                     , Op        , 5         , VVM_INS_PARAM_FFFFFF              ) /* 143 */                                                                                                        \
	VVM_OP_XM( add_add                      , Op        , 4         , VVM_INS_PARAM_FFFFFF              ) /* 144 */                                                                                                        \
	\
	VVM_OP_XM( sub_cmplt1                   , Op        , 4         , VVM_INS_PARAM_FFFFFI              ) /* 145 */                                                                                                        \
	VVM_OP_XM( sub_neg                      , Op        , 3         , VVM_INS_PARAM_FFFFFF              ) /* 146 */                                                                                                        \
	VVM_OP_XM( sub_mul                      , Op        , 4         , VVM_INS_PARAM_FFFFFF              ) /* 147 */                                                                                                        \
	\
	VVM_OP_XM( div_mad0                     , Op        , 5         , VVM_INS_PARAM_FFFFFF              ) /* 148 */                                                                                                        \
	VVM_OP_XM( div_f2i                      , Op        , 3         , VVM_INS_PARAM_FFFFFI              ) /* 149 */                                                                                                        \
	VVM_OP_XM( div_mul                      , Op        , 4         , VVM_INS_PARAM_FFFFFF              ) /* 150 */                                                                                                        \
	\
	VVM_OP_XM( muli_addi                    , Op        , 4         , VVM_INS_PARAM_IIIIII              ) /* 151 */                                                                                                        \
	VVM_OP_XM( addi_bit_rshift				, Op        , 4         , VVM_INS_PARAM_IIIIII              ) /* 152 */                                                                                                        \
	VVM_OP_XM( addi_muli                    , Op        , 4         , VVM_INS_PARAM_IIIIII              ) /* 153 */                                                                                                        \
	\
	VVM_OP_XM( b2i_2x                       , Op        , 3         , VVM_INS_PARAM_IIIIII              ) /* 154 */                                                                                                        \
	\
	VVM_OP_XM( i2f_div0                     , Op        , 3         , VVM_INS_PARAM_FFFFFF              ) /* 155 */                                                                                                        \
	VVM_OP_XM( i2f_div1                     , Op        , 3         , VVM_INS_PARAM_FFFFFF              ) /* 156 */                                                                                                        \
	VVM_OP_XM( i2f_mul                      , Op        , 3         , VVM_INS_PARAM_FFFFFF              ) /* 157 */                                                                                                        \
	VVM_OP_XM( i2f_mad0                     , Op        , 4         , VVM_INS_PARAM_FFFFFF              ) /* 158 */                                                                                                        \
	VVM_OP_XM( i2f_mad1                     , Op        , 4         , VVM_INS_PARAM_FFFFFF              ) /* 159 */                                                                                                        \
	\
	VVM_OP_XM( f2i_select1                  , Op        , 4         , VVM_INS_PARAM_IIFIII              ) /* 160 */                                                                                                        \
	VVM_OP_XM( f2i_maxi                     , Op        , 3         , VVM_INS_PARAM_IIIFII              ) /* 161 */                                                                                                        \
	VVM_OP_XM( f2i_addi                     , Op        , 3         , VVM_INS_PARAM_IIIFII              ) /* 162 */                                                                                                        \
	\
	VVM_OP_XM( fmod_add                     , Op        , 3         , VVM_INS_PARAM_FFFFFF              ) /* 163 */                                                                                                        \
	VVM_OP_XM( bit_and_i2f                  , Op        , 3         , VVM_INS_PARAM_FFFFFF              ) /* 164 */                                                                                                        \
	VVM_OP_XM( bit_rshift_bit_and           , Op        , 3         , VVM_INS_PARAM_FFFFFF              ) /* 165 */                                                                                                        \
	VVM_OP_XM( neg_cmplt                    , Op        , 3         , VVM_INS_PARAM_FFFFFF              ) /* 166 */                                                                                                        \
	VVM_OP_XM( bit_or_muli                  , Op        , 3         , VVM_INS_PARAM_FFFFFF              ) /* 167 */                                                                                                        \
	VVM_OP_XM( bit_lshift_bit_or            , Op        , 3         , VVM_INS_PARAM_FFFFFF              ) /* 168 */                                                                                                        \
	VVM_OP_XM( random_add                   , Op        , 3         , VVM_INS_PARAM_FFFFFF              ) /* 169 */                                                                                                        \
	VVM_OP_XM( random_2x                    , Op        , 3         , VVM_INS_PARAM_FFFFFF              ) /* 170 */                                                                                                        \
	VVM_OP_XM( max_f2i                      , Op        , 3         , VVM_INS_PARAM_FFFFFF              ) /* 171 */                                                                                                        \
	VVM_OP_XM( select_mul                   , Op        , 5         , VVM_INS_PARAM_FFFFFF              ) /* 172 */                                                                                                        \
	VVM_OP_XM( select_add                   , Op        , 5         , VVM_INS_PARAM_FFFFFF              ) /* 173 */                                                                                                        \
	VVM_OP_XM( sin_cos                      , Op        , 3         , VVM_INS_PARAM_FFFFFF              ) /* 174 */

#define VVM_OP_XM(n, ...) n,
enum class EVectorVMOp : uint8 {
	VVM_OP_XM_LIST
	NumOpcodes
};
#undef VVM_OP_XM

#if STATS
struct FVMCycleCounter
{
	int32 ScopeIndex;
	uint64 ScopeEnterCycles;
};

struct FStatScopeData
{
	TStatId StatId;
	std::atomic<uint64> ExecutionCycleCount;

	FStatScopeData(TStatId InStatId) : StatId(InStatId)
	{
		ExecutionCycleCount.store(0);
	}
	
	FStatScopeData(const FStatScopeData& InObj)
	{
		StatId = InObj.StatId;
		ExecutionCycleCount.store(InObj.ExecutionCycleCount.load());
	}
};

struct FStatStackEntry
{
	FCycleCounter CycleCounter;
	FVMCycleCounter VmCycleCounter;
};
#endif

//TODO: 
//All of this stuff can be handled by the VM compiler rather than dirtying the VM code.
//Some require RWBuffer like support.
struct FDataSetMeta
{
	TArrayView<uint8 const* RESTRICT const> InputRegisters;
	TArrayView<uint8 const* RESTRICT const> OutputRegisters;

	uint32 InputRegisterTypeOffsets[3];
	uint32 OutputRegisterTypeOffsets[3];

	int32 DataSetAccessIndex;	// index for individual elements of this set

	int32 InstanceOffset;		// offset of the first instance processed 
	
	TArray<int32>*RESTRICT IDTable;
	TArray<int32>*RESTRICT FreeIDTable;
	TArray<int32>*RESTRICT SpawnedIDsTable;

	/** Number of free IDs in the FreeIDTable */
	int32* NumFreeIDs;

	/** MaxID used in this execution. */
	int32* MaxUsedID;

	int32 *NumSpawnedIDs;

	int32 IDAcquireTag;

	//Temporary lock we're using for thread safety when writing to the FreeIDTable.
	//TODO: A lock free algorithm is possible here. We can create a specialized lock free list and reuse the IDTable slots for FreeIndices as Next pointers for our LFL.
	//This would also work well on the GPU. 
	//UE-65856 for tracking this work.

	//@NOTE (smcgrath): the lock/unlock functions can go with the ne VM, we don't need them
#if VECTORVM_SUPPORTS_LEGACY
	FCriticalSection FreeTableLock;
	FORCEINLINE void LockFreeTable();
	FORCEINLINE void UnlockFreeTable();
#endif

	FDataSetMeta()
		: InputRegisterTypeOffsets{}
		, OutputRegisterTypeOffsets{}
		, DataSetAccessIndex(INDEX_NONE)
		, InstanceOffset(INDEX_NONE)
		, IDTable(nullptr)
		, FreeIDTable(nullptr)
		, SpawnedIDsTable(nullptr)
		, NumFreeIDs(nullptr)
		, MaxUsedID(nullptr)
		, IDAcquireTag(INDEX_NONE)
	{
	}	

	FORCEINLINE void Reset()
	{
		InputRegisters = TArrayView<uint8 const* RESTRICT const>();
		OutputRegisters = TArrayView<uint8 const* RESTRICT const>();
		DataSetAccessIndex = INDEX_NONE;
		InstanceOffset = INDEX_NONE;
		IDTable = nullptr;
		FreeIDTable = nullptr;
		SpawnedIDsTable = nullptr;
		NumFreeIDs = nullptr;
		MaxUsedID = nullptr;
		IDAcquireTag = INDEX_NONE;
	}

	FORCEINLINE void Init(const TArrayView<uint8 const* RESTRICT const>& InInputRegisters, const TArrayView<uint8 const* RESTRICT const>& InOutputRegisters, int32 InInstanceOffset, TArray<int32>* InIDTable, TArray<int32>* InFreeIDTable, int32* InNumFreeIDs, int32 *InNumSpawnedIDs, int32* InMaxUsedID, int32 InIDAcquireTag, TArray<int32>* InSpawnedIDsTable)
	{
		InputRegisters = InInputRegisters;
		OutputRegisters = InOutputRegisters;

		DataSetAccessIndex = INDEX_NONE;
		InstanceOffset = InInstanceOffset;
		IDTable = InIDTable;
		FreeIDTable = InFreeIDTable;
		NumFreeIDs = InNumFreeIDs;
		NumSpawnedIDs = InNumSpawnedIDs;
		MaxUsedID = InMaxUsedID;
		IDAcquireTag = InIDAcquireTag;
		SpawnedIDsTable = InSpawnedIDsTable;
	}

private:
	// Non-copyable and non-movable
	FDataSetMeta(FDataSetMeta&&) = delete;
	FDataSetMeta(const FDataSetMeta&) = delete;
	FDataSetMeta& operator=(FDataSetMeta&&) = delete;
	FDataSetMeta& operator=(const FDataSetMeta&) = delete;
};

#if VECTORVM_SUPPORTS_EXPERIMENTAL && VECTORVM_SUPPORTS_LEGACY

class FVectorVMExternalFunctionContextProxy;
DECLARE_DELEGATE_OneParam(FVMExternalFunction, FVectorVMExternalFunctionContextProxy& /*Context*/);

#elif VECTORVM_SUPPORTS_EXPERIMENTAL

class FVectorVMExternalFunctionContextExperimental;
DECLARE_DELEGATE_OneParam(FVMExternalFunction, FVectorVMExternalFunctionContextExperimental& /*Context*/);

#elif VECTORVM_SUPPORTS_LEGACY

class FVectorVMExternalFunctionContextLegacy;
DECLARE_DELEGATE_OneParam(FVMExternalFunction, FVectorVMExternalFunctionContextLegacy& /*Context*/);

#else
#error "At least one of VECTORVM_SUPPORTS_EXPERIMENTAL | VECTORVM_SUPPORTS_LEGACY must be defined"
#endif

