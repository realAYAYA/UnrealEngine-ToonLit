// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "VectorVMSerialization.h"
#include "VectorVMCommon.h"
#include "Math/RandomStream.h"

#if VECTORVM_SUPPORTS_EXPERIMENTAL

#define VECTORVM_SUPPORTS_COMPUTED_GOTO 0

struct FDataSetMeta;

#define VVM_INS_PARAM_FFFFFF	0b000000
#define VVM_INS_PARAM_FFFFFI	0b000001
#define VVM_INS_PARAM_FFFFIF	0b000010
#define VVM_INS_PARAM_FFFFII	0b000011
#define VVM_INS_PARAM_FFFIFF	0b000100
#define VVM_INS_PARAM_FFFIFI	0b000101
#define VVM_INS_PARAM_FFFIIF	0b000110
#define VVM_INS_PARAM_FFFIII	0b000111
#define VVM_INS_PARAM_FFIFFF	0b001000
#define VVM_INS_PARAM_FFIFFI	0b001001
#define VVM_INS_PARAM_FFIFIF	0b001010
#define VVM_INS_PARAM_FFIFII	0b001011
#define VVM_INS_PARAM_FFIIFF	0b001100
#define VVM_INS_PARAM_FFIIFI	0b001101
#define VVM_INS_PARAM_FFIIIF	0b001110
#define VVM_INS_PARAM_FFIIII	0b001111
#define VVM_INS_PARAM_FIFFFF	0b010000
#define VVM_INS_PARAM_FIFFFI	0b010001
#define VVM_INS_PARAM_FIFFIF	0b010010
#define VVM_INS_PARAM_FIFFII	0b010011
#define VVM_INS_PARAM_FIFIFF	0b010100
#define VVM_INS_PARAM_FIFIFI	0b010101
#define VVM_INS_PARAM_FIFIIF	0b010110
#define VVM_INS_PARAM_FIFIII	0b010111
#define VVM_INS_PARAM_FIIFFF	0b011000
#define VVM_INS_PARAM_FIIFFI	0b011001
#define VVM_INS_PARAM_FIIFIF	0b011010
#define VVM_INS_PARAM_FIIFII	0b011011
#define VVM_INS_PARAM_FIIIFF	0b011100
#define VVM_INS_PARAM_FIIIFI	0b011101
#define VVM_INS_PARAM_FIIIIF	0b011110
#define VVM_INS_PARAM_FIIIII	0b011111
#define VVM_INS_PARAM_IFFFFF	0b100000
#define VVM_INS_PARAM_IFFFFI	0b100001
#define VVM_INS_PARAM_IFFFIF	0b100010
#define VVM_INS_PARAM_IFFFII	0b100011
#define VVM_INS_PARAM_IFFIFF	0b100100
#define VVM_INS_PARAM_IFFIFI	0b100101
#define VVM_INS_PARAM_IFFIIF	0b100110
#define VVM_INS_PARAM_IFFIII	0b100111
#define VVM_INS_PARAM_IFIFFF	0b101000
#define VVM_INS_PARAM_IFIFFI	0b101001
#define VVM_INS_PARAM_IFIFIF	0b101010
#define VVM_INS_PARAM_IFIFII	0b101011
#define VVM_INS_PARAM_IFIIFF	0b101100
#define VVM_INS_PARAM_IFIIFI	0b101101
#define VVM_INS_PARAM_IFIIIF	0b101110
#define VVM_INS_PARAM_IFIIII	0b101111
#define VVM_INS_PARAM_IIFFFF	0b110000
#define VVM_INS_PARAM_IIFFFI	0b110001
#define VVM_INS_PARAM_IIFFIF	0b110010
#define VVM_INS_PARAM_IIFFII	0b110011
#define VVM_INS_PARAM_IIFIFF	0b110100
#define VVM_INS_PARAM_IIFIFI	0b110101
#define VVM_INS_PARAM_IIFIIF	0b110110
#define VVM_INS_PARAM_IIFIII	0b110111
#define VVM_INS_PARAM_IIIFFF	0b111000
#define VVM_INS_PARAM_IIIFFI	0b111001
#define VVM_INS_PARAM_IIIFIF	0b111010
#define VVM_INS_PARAM_IIIFII	0b111011
#define VVM_INS_PARAM_IIIIFF	0b111100
#define VVM_INS_PARAM_IIIIFI	0b111101
#define VVM_INS_PARAM_IIIIIF	0b111110
#define VVM_INS_PARAM_IIIIII	0b111111

#define VVM_OP_CAT_XM_LIST    \
	VVM_OP_CAT_XM(Input)      \
	VVM_OP_CAT_XM(Output)     \
	VVM_OP_CAT_XM(Op)         \
	VVM_OP_CAT_XM(ExtFnCall)  \
	VVM_OP_CAT_XM(IndexGen)   \
	VVM_OP_CAT_XM(RWBuffer)   \
	VVM_OP_CAT_XM(Stat)       \
	VVM_OP_CAT_XM(Other)

#define VVM_OP_CAT_XM(Category, ...) Category,
enum class EVectorVMOpCategory : uint8 {
	VVM_OP_CAT_XM_LIST
	MAX
};
#undef VVM_OP_CAT_XM

static const EVectorVMOpCategory VVM_OP_CATEGORIES[] = {
#define VVM_OP_XM(op, cat, ...) EVectorVMOpCategory::cat,
	VVM_OP_XM_LIST
#undef VVM_OP_XM
};

#define VVM_MIN(a, b)               ((a) < (b) ? (a) : (b))
#define VVM_MAX(a, b)               ((a) > (b) ? (a) : (b))
#define VVM_CLAMP(v, min, max)      ((v) < (min) ? (min) : ((v) < (max) ? (v) : (max)))
#define VVM_ALIGN(num, alignment)   (((size_t)(num) + (alignment) - 1) & ~((alignment) - 1))
#define VVM_ALIGN_4(num)            (((size_t)(num) + 3) & ~3)
#define VVM_ALIGN_8(num)            (((size_t)(num) + 7) & ~7)
#define VVM_ALIGN_16(num)           (((size_t)(num) + 15) & ~15)
#define VVM_ALIGN_32(num)           (((size_t)(num) + 31) & ~31)
#define VVM_ALIGN_64(num)           (((size_t)(num) + 63) & ~63)

#define VVM_PTR_ALIGN    VVM_ALIGN_16
#define VVM_REG_SIZE     sizeof(FVecReg)

enum {
	VVM_RT_TEMPREG,
	VVM_RT_CONST,
	VVM_RT_INPUT,
	VVM_RT_OUTPUT,
	VVM_RT_INVALID
};

enum EVectorVMFlags
{
	VVMFlag_OptSaveIntermediateState = 1 << 0,
	VVMFlag_OptOmitStats             = 1 << 1,
	VVMFlag_LargeScript              = 1 << 2,   //if true register indices are 16 bit, otherwise they're 8 bit
	VVMFlag_HasRandInstruction       = 1 << 3,
	VVMFlag_DataMapCacheSetup        = 1 << 4,
};

//prototypes for serialization that are required whether or not serialization is enabled
typedef uint32 (VectorVMOptimizeErrorCallback)     (struct FVectorVMOptimizeContext *OptimizeContext, uint32 ErrorFlags);	//return new error flags

union FVecReg {
	VectorRegister4f v;
	VectorRegister4i i;
};

struct FVectorVMExtFunctionData
{
	const FVMExternalFunction *Function;
	int32                      NumInputs;
	int32                      NumOutputs;
};

//Optimization
struct FVectorVMOptimizeInstruction
{
	EVectorVMOp         OpCode;
	EVectorVMOpCategory OpCat;
	uint32              PtrOffsetInOrigBytecode;
	uint32              PtrOffsetInOptimizedBytecode;
	int                 Index; //initial index.  Instructions are moved around and removed and dependency chains are created based on index, so we need to store this.
	int					InsMergedIdx; //if not -1, then the instruction index that this is merged with.  Instructions with a set InsMergedIdx are not written to the final bytecode
	int                 OutputMergeIdx[2]; //if not -1 then this instruction writes directly to an output, not a temp register
	uint16              RegPtrOffset;
	int                 NumInputRegisters;
	int                 NumOutputRegisters;
	union
	{
		struct
		{
			uint16 DataSetIdx;
			uint16 InputIdx;
		} Input;
		struct
		{
			uint16 DataSetIdx;
			uint16 DstRegIdx;
			int    MergeIdx; //if not -1 then this instruction index is merged with an output or an op, if it's -2 then it's already been taken care of
			uint16 SerialIdx;
		} Output;
		struct
		{

		} Op;
		struct
		{
			uint16 DataSetIdx;
		} IndexGen;
		struct
		{
			uint16 ExtFnIdx;
			uint16 NumInputs;
			uint16 NumOutputs;
		} ExtFnCall;
		struct
		{
			uint16 DataSetIdx;
		} RWBuffer;
		struct
		{
			uint16 ID;
		} Stat;
		struct
		{
		} Other;
	};
};

enum EVectorVMOptimizeError
{
	VVMOptErr_OutOfMemory           = 1 << 0,
	VVMOptErr_Overflow			    = 1 << 1,
	VVMOptErr_Bytecode              = 1 << 2,
	VVMOptErr_RegisterUsage         = 1 << 3,
	VVMOptErr_ConstRemap            = 1 << 4,
	VVMOptErr_Instructions          = 1 << 5,
	VVMOptErr_InputMergeBuffer      = 1 << 6,
	VVMOptErr_InstructionReOrder    = 1 << 7,
	VVMOptErr_SSARemap              = 1 << 8,
	VVMOptErr_OptimizedBytecode     = 1 << 9,
	VVMOptErr_ExternalFunction      = 1 << 10,
	VVMOptErr_RedundantInstruction  = 1 << 11,

	VVMOptErr_Fatal                 = 1 << 31
};

struct FVectorVMOptimizeContext
{
	uint8 *                               OutputBytecode;
	uint16 *                              ConstRemap[2];
	uint16 *                              InputRemapTable;
	uint16 *                              InputDataSetOffsets;
	uint8 *                               OutputRemapDataSetIdx;
	uint16 *                              OutputRemapDataType;
	uint16 *                              OutputRemapDst;

	FVectorVMExtFunctionData *            ExtFnTable;

	uint32                                NumBytecodeBytes;
	uint32                                MaxOutputDataSet;
	uint16                                NumConstsAlloced;    //upper bound to alloc
	uint32                                NumTempRegisters;
	uint16                                NumConstsRemapped;
	uint16                                NumInputsRemapped;
	uint16                                NumNoAdvanceInputs;
	uint16                                NumInputDataSets;
	uint16                                NumOutputsRemapped;
	uint16                                NumOutputInstructions;
	uint32                                NumExtFns;
	uint32                                MaxExtFnRegisters;
	uint32                                NumDummyRegsReq;     //External function "null" registers
	int32                                 MaxExtFnUsed;
	uint32                                Flags;
	uint64                                HashId;

#if WITH_EDITORONLY_DATA
	struct
	{
		VectorVMReallocFn *               ReallocFn;
		VectorVMFreeFn *                  FreeFn;
		const char *                      ScriptName;
	} Init;                                                    //Set this stuff when calling Optimize()

	struct
	{
		uint32                            Flags;               //zero is good
		uint32                            Line;
		VectorVMOptimizeErrorCallback *   CallbackFn;          //set this to get a callback whenever there's an error
	} Error;

	struct
	{
		FVectorVMOptimizeInstruction *    Instructions;
		uint8 *                           RegisterUsageType;
		uint16 *                          RegisterUsageBuffer;
		uint16 *                          SSARegisterUsageBuffer;
		uint16 *                          ParentInstructionIdx;
		uint32                            NumInstructions;
		uint32                            NumInstructionsAlloced;
		uint32                            NumRegistersUsed;
	} Intermediate;                                           //these are freed and NULL after optimize() unless SaveIntermediateData is true when calling OptimizeVectorVMScript
#endif
};

enum EVectorVMStateError
{
	VVMErr_InitOutOfMemory     = 1 << 0,
	VVMErr_InitMemMismatch     = 1 << 1,
	VVMErr_BatchMemory         = 1 << 2,
	VVMErr_AssignInstances     = 1 << 3,

	VVMErr_Fatal               = 1 << 31
};

struct FVectorVMBatchState
{
	MS_ALIGN(16) FVecReg *    RegisterData GCC_ALIGN(16);
	struct {
		uint32 *              StartingOutputIdxPerDataSet;
		uint32 *              NumOutputPerDataSet;
		uint8 **              OutputMaskIdx; //these point to the BatchState's OutputMaskIdx
		struct {
			uint32 **         RegData;
			uint8 *           RegInc;
			FVecReg *         DummyRegs;
		} ExtFnDecodedReg;
		int32 *               RandCounters; //used for external functions only.

		int                   ChunkIdx;
		int                   StartInstanceThisChunk;
		int                   NumInstancesThisChunk;
	} ChunkLocalData;
	
	uint8 **                  RegPtrTable;    //not aligned because input pointers could be offest from the DataSetInfo.InstanceOffset, so we must assume every op is unaligned
	uint8 *                   RegIncTable;    //0 for const, 1 for temp reg
	uint8 *                   OutputMaskIdx;

	union {
		struct {
			VectorRegister4i          State[5]; //xorwor state for random/randomi instructions.  DIs use RandomStream.
			VectorRegister4i          Counters;
		};
	} RandState;
	FRandomStream             RandStream;
};

static_assert((sizeof(FVectorVMBatchState) & 0xF) == 0, "FVectorVMBatchState must be 16 byte aligned");

struct FVectorVMState
{
	uint8 *                             Bytecode;
	uint32                              NumBytecodeBytes;

	FVecReg *                           ConstantBuffers;        //the last OptimizeCtx->NumNoAdvanceInputs are no advance inputs that are copied in the table setup
	FVectorVMExtFunctionData *          ExtFunctionTable;
	int32 *                             NumOutputPerDataSet;

	uint16 *                            ConstRemapTable;
	uint16 *                            InputRemapTable;
	uint16 *                            InputDataSetOffsets;
	uint8 *                             OutputRemapDataSetIdx;
	uint16 *                            OutputRemapDataType;
	uint16 *                            OutputRemapDst;

	uint8 *                             ConstMapCacheIdx;       //these don't get filled out until Exec() is called because they can't be filled out until the state
	uint16 *                            ConstMapCacheSrc;       //of const and input buffers from Niagara is unknown until exec() is called.
	uint8 *                             InputMapCacheIdx;
	uint16 *                            InputMapCacheSrc;
	int32                               NumInstancesExecCached;

	uint32                              Flags;
	
	uint32                              NumExtFunctions;
	uint32                              MaxExtFnRegisters;
	
	uint32                              NumTempRegisters;
	uint32                              NumConstBuffers;
	uint32                              NumInputBuffers;
	uint32                              NumInputDataSets;
	uint32                              NumOutputsRemapped;
	uint32                              NumOutputBuffers;
	uint32                              MaxOutputDataSet;
	uint32                              NumDummyRegsRequired;

	//batch stuff
	uint32                              BatchOverheadSize;
	uint32                              ChunkLocalDataOutputIdxNumBytes;
	uint32                              ChunkLocalNumOutputNumBytes;
	uint32                              ChunkLocalOutputMaskIdxNumBytes;

	uint64                              OptimizerHashId;
	uint32                              TotalNumBytes;

	struct {
		uint32          NumBytesRequiredPerBatch;
		uint32          PerBatchRegisterDataBytesRequired;
		uint32          MaxChunksPerBatch;
		uint32          MaxInstancesPerChunk;
	} ExecCtxCache;
};

struct FVectorVMExecContext
{
	struct
	{
		uint32          NumBytesRequiredPerBatch;
		uint32          PerBatchRegisterDataBytesRequired;
		uint32          MaxChunksPerBatch;
		uint32          MaxInstancesPerChunk;
	} Internal;

	FVectorVMState *                        VVMState;               //created with AllocVectorVMState()
	TArrayView<FDataSetMeta>                DataSets;					
	TArrayView<const FVMExternalFunction *> ExtFunctionTable;
	TArrayView<void *>                      UserPtrTable;
	int32                                   NumInstances;
	const uint8 * const *                   ConstantTableData;      //constant tables consist of an array of pointers
	const int *                             ConstantTableNumBytes;  //an array of sizes in bytes
	int32                                   ConstantTableCount;     //how many constant tables.  These tables must match the ones used with OptimizeVectorVMScript()
};

class FVectorVMExternalFunctionContextExperimental
{
public:
	MS_ALIGN(32) uint32**    RegisterData GCC_ALIGN(32);
	uint8 *                  RegInc;

	int                      RegReadCount;
	int                      NumRegisters;

	int                      StartInstance;
	int                      NumInstances;
	int                      NumLoops;
	int                      PerInstanceFnInstanceIdx;

	void **                  UserPtrTable;
	int                      NumUserPtrs;

	FRandomStream*           RandStream;
	int32 **                 RandCounters;
	TArrayView<FDataSetMeta> DataSets;

	FORCEINLINE int32                                  GetStartInstance() const          { return StartInstance; }
	FORCEINLINE int32                                  GetNumInstances() const           { return NumInstances; }
	FORCEINLINE int32 *                                GetRandCounters()                 { 
		                                                                                     if (!*RandCounters) {
																								 *RandCounters = (int *)FMemory::MallocZeroed(sizeof(int32) * NumInstances);
	                                                                                         }
		                                                                                     return *RandCounters; 
	                                                                                     }
	FORCEINLINE FRandomStream &                        GetRandStream()                   { return *RandStream; }
	FORCEINLINE void *                                 GetUserPtrTable(int32 UserPtrIdx) { check(UserPtrIdx < NumUserPtrs);  return UserPtrTable[UserPtrIdx]; }
	template<uint32 InstancesPerOp> FORCEINLINE int32  GetNumLoops() const               { static_assert(InstancesPerOp == 4); return NumLoops; };

	FORCEINLINE float* GetNextRegister(int32* OutAdvanceOffset)
	{
		check(RegReadCount < NumRegisters);
		*OutAdvanceOffset = RegInc[RegReadCount];
		return (float*)RegisterData[RegReadCount++];
	}
};

//API FUNCTIONS

//normal functions
VECTORVM_API FVectorVMState * AllocVectorVMState                     (FVectorVMOptimizeContext *OptimizeCtx);
VECTORVM_API void             FreeVectorVMState                      (FVectorVMState *VectorVMState);
VECTORVM_API void             ExecVectorVMState                      (FVectorVMExecContext *ExecCtx, FVectorVMSerializeState *SerializeState, FVectorVMSerializeState *CmpSerializeState);

#if WITH_EDITORONLY_DATA
//optimize functions
VECTORVM_API uint32           OptimizeVectorVMScript                 (const uint8 *Bytecode, int BytecodeLen, FVectorVMExtFunctionData *ExtFnIOData, int NumExtFns, FVectorVMOptimizeContext *OptContext, uint64 HashId, uint32 Flags); //OutContext must be zeroed except the Init struct
VECTORVM_API void             FreeVectorVMOptimizeContext            (FVectorVMOptimizeContext *Context);
VECTORVM_API void             FreezeVectorVMOptimizeContext          (const FVectorVMOptimizeContext& Context, TArray<uint8>& ContextData);
VECTORVM_API void             GenerateHumanReadableVectorVMScript    (const FVectorVMOptimizeContext& Context, FString& VMScript);
#endif
VECTORVM_API void             ReinterpretVectorVMOptimizeContextData (TConstArrayView<uint8> ContextData, FVectorVMOptimizeContext& Context);

//serialize functions
VECTORVM_API uint32           SerializeVectorVMInputDataSets         (FVectorVMSerializeState *SerializeState, FVectorVMExecContext *ExecContext);
VECTORVM_API uint32           SerializeVectorVMOutputDataSets        (FVectorVMSerializeState *SerializeState, FVectorVMExecContext *ExecContext);
VECTORVM_API void             SerializeVectorVMWriteToFile           (FVectorVMSerializeState *SerializeState, uint8 WhichStateWritten, const wchar_t *Filename);
VECTORVM_API void             FreeVectorVMSerializeState             (FVectorVMSerializeState *SerializeState);

//debug helper functions
VECTORVM_API uint16           VVMDbgGetRemappedInputIdx              (FVectorVMState *VectorVMState, int InputRegisterIdx, int DataSetIdx, bool IntReg, uint16 *OptOutIdxInRegTable);
VECTORVM_API uint16           VVMDbgGetRemappedOutputIdx             (FVectorVMState *VectorVMState, int OutputRegisterIdx, int DataSetIdx, bool IntReg, uint16 *OptOutIdxInRegTable);
VECTORVM_API uint16           VVMDbgGetRemappedConstIdx              (FVectorVMState *VectorVMState, int ConstTableIdx, int ConstBuffIdx, uint16 *OptOutIdxInRegTable);

#else // VECTORVM_SUPPORTS_EXPERIMENTAL

struct FVectorVMState {

};

VECTORVM_API void    SerializeVectorVMWriteToFile    (FVectorVMSerializeState *SerializeState, uint8 WhichStateWritten, const wchar_t *Filename);
VECTORVM_API void    FreeVectorVMSerializeState      (FVectorVMSerializeState *SerializeState);

#endif // VECTORVM_SUPPORTS_EXPERIMENTAL

//things needed for when both experimental and legacy are built
#if VECTORVM_SUPPORTS_LEGACY
VECTORVM_API uint32  SerializeVectorVMInputDataSets  (FVectorVMSerializeState *SerializeState, TArrayView<FDataSetMeta>, const uint8 * const *ConstantTableData, const int *ConstantTableSizes, int32 ConstantTableCount); //only use when not calling AllocVectorVMState()
VECTORVM_API uint32  SerializeVectorVMOutputDataSets (FVectorVMSerializeState *SerializeState, TArrayView<FDataSetMeta>, const uint8 * const *ConstantTableData, const int *ConstantTableSizes, int32 ConstantTableCount);
#endif