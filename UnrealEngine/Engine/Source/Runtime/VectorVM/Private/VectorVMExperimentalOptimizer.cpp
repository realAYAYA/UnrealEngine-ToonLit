// Copyright Epic Games, Inc. All Rights Reserved.
#include "CoreTypes.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "VectorVM.h"
#include "VectorVMExperimental.h"


#if VECTORVM_SUPPORTS_EXPERIMENTAL

namespace VectorVMSerializationHelper
{

struct FRuntimeContextData
{
	FRuntimeContextData(const FVectorVMOptimizeContext& Context)
	: NumBytecodeBytes(Context.NumBytecodeBytes)
	, MaxOutputDataSet(Context.MaxOutputDataSet)
	, NumConstsAlloced(Context.NumConstsAlloced)
	, NumTempRegisters(Context.NumTempRegisters)
	, NumConstsRemapped(Context.NumConstsRemapped)
	, NumInputsRemapped(Context.NumInputsRemapped)
	, NumNoAdvanceInputs(Context.NumNoAdvanceInputs)
	, NumInputDataSets(Context.NumInputDataSets)
	, NumOutputsRemapped(Context.NumOutputsRemapped)
	, NumOutputInstructions(Context.NumOutputInstructions)
	, NumExtFns(Context.NumExtFns)
	, MaxExtFnRegisters(Context.MaxExtFnRegisters)
	, NumDummyRegsReq(Context.NumDummyRegsReq)
	, MaxExtFnUsed(Context.MaxExtFnUsed)
	, Flags(Context.Flags)
	, HashId(Context.HashId)
	{}

	FRuntimeContextData(TConstArrayView<uint8> ContextData)
	{
		FMemoryReaderView Ar(ContextData);
		Ar << *this;
	}

	void CopyToContext(FVectorVMOptimizeContext& Context) const
	{
		Context.NumBytecodeBytes = NumBytecodeBytes;
		Context.MaxOutputDataSet = MaxOutputDataSet;
		Context.NumConstsAlloced = NumConstsAlloced;
		Context.NumTempRegisters = NumTempRegisters;
		Context.NumConstsRemapped = NumConstsRemapped;
		Context.NumInputsRemapped = NumInputsRemapped;
		Context.NumNoAdvanceInputs = NumNoAdvanceInputs;
		Context.NumInputDataSets = NumInputDataSets;
		Context.NumOutputsRemapped = NumOutputsRemapped;
		Context.NumOutputInstructions = NumOutputInstructions;
		Context.NumExtFns = NumExtFns;
		Context.MaxExtFnRegisters = MaxExtFnRegisters;
		Context.NumDummyRegsReq = NumDummyRegsReq;
		Context.MaxExtFnUsed = MaxExtFnUsed;
		Context.Flags = Flags;
		Context.HashId = HashId;
	}

	friend FArchive& operator<<(FArchive& Ar, FRuntimeContextData& ContextInfo);

	uint32 NumBytecodeBytes;
	uint32 MaxOutputDataSet;
	uint16 NumConstsAlloced;
	uint32 NumTempRegisters;
	uint16 NumConstsRemapped;
	uint16 NumInputsRemapped;
	uint16 NumNoAdvanceInputs;
	uint16 NumInputDataSets;
	uint16 NumOutputsRemapped;
	uint16 NumOutputInstructions;
	uint32 NumExtFns;
	uint32 MaxExtFnRegisters;
	uint32 NumDummyRegsReq;
	int32 MaxExtFnUsed;
	uint32 Flags;
	uint64 HashId;
};

FArchive& operator<<(FArchive& Ar, FRuntimeContextData& ContextInfo)
{
	Ar << ContextInfo.NumBytecodeBytes;
	Ar << ContextInfo.MaxOutputDataSet;
	Ar << ContextInfo.NumConstsAlloced;
	Ar << ContextInfo.NumTempRegisters;
	Ar << ContextInfo.NumConstsRemapped;
	Ar << ContextInfo.NumInputsRemapped;
	Ar << ContextInfo.NumNoAdvanceInputs;
	Ar << ContextInfo.NumInputDataSets;
	Ar << ContextInfo.NumOutputsRemapped;
	Ar << ContextInfo.NumOutputInstructions;
	Ar << ContextInfo.NumExtFns;
	Ar << ContextInfo.MaxExtFnRegisters;
	Ar << ContextInfo.NumDummyRegsReq;
	Ar << ContextInfo.MaxExtFnUsed;
	Ar << ContextInfo.Flags;
	Ar << ContextInfo.HashId;

	return Ar;
}

struct FContextInfoLayout
{
	FContextInfoLayout(const FRuntimeContextData& Context)
		: PropertySize(sizeof(Context))
		, BytecodeSize(Context.NumBytecodeBytes)
		, ConstRemapSize(Context.NumConstsRemapped * sizeof(uint16))
		, InputRemapSize(Context.NumInputsRemapped * sizeof(uint16))
		, InputDataSetOffsetsSize(Context.NumInputDataSets * 8 * sizeof(uint16))
		, OutputRemapDataSetIdxSize(Context.NumOutputsRemapped * sizeof(uint8))
		, OutputRemapDataTypeSize(Context.NumOutputsRemapped * sizeof(uint16))
		, OutputRemapDstSize(Context.NumOutputsRemapped * sizeof(uint16))
		, ExtFnSize(Context.NumExtFns * sizeof(FVectorVMExtFunctionData))
		, PropertyOffset(0)
		, BytecodeOffset(Align(PropertyOffset + PropertySize, 16))
		, ConstRemapOffset(Align(BytecodeOffset + BytecodeSize, 16))
		, InputRemapOffset(Align(ConstRemapOffset + ConstRemapSize, 16))
		, InputDataSetOffsetsOffset(Align(InputRemapOffset + InputRemapSize, 16))
		, OutputRemapDataSetIdxOffset(Align(InputDataSetOffsetsOffset + InputDataSetOffsetsSize, 16))
		, OutputRemapDataTypeOffset(Align(OutputRemapDataSetIdxOffset + OutputRemapDataSetIdxSize, 16))
		, OutputRemapDstOffset(Align(OutputRemapDataTypeOffset + OutputRemapDataTypeSize, 16))
		, ExtFnOffset(Align(OutputRemapDstOffset + OutputRemapDstSize, 16))
		, TotalSize(Align(ExtFnOffset + ExtFnSize, 16))
	{
	}

	const uint32 PropertySize;
	const uint32 BytecodeSize;
	const uint32 ConstRemapSize;
	const uint32 InputRemapSize;
	const uint32 InputDataSetOffsetsSize;
	const uint32 OutputRemapDataSetIdxSize;
	const uint32 OutputRemapDataTypeSize;
	const uint32 OutputRemapDstSize;
	const uint32 ExtFnSize;

	const size_t PropertyOffset;
	const size_t BytecodeOffset;
	const size_t ConstRemapOffset;
	const size_t InputRemapOffset;
	const size_t InputDataSetOffsetsOffset;
	const size_t OutputRemapDataSetIdxOffset;
	const size_t OutputRemapDataTypeOffset;
	const size_t OutputRemapDstOffset;
	const size_t ExtFnOffset;
	const size_t TotalSize;
};

#if WITH_EDITORONLY_DATA
void FreezeContext(const FVectorVMOptimizeContext& Context, TArray<uint8>& ContextData)
{
	FRuntimeContextData RuntimeData(Context);
	FContextInfoLayout Layout(RuntimeData);

	ContextData.SetNumZeroed(Layout.TotalSize);
	uint8* BufferData = ContextData.GetData();

	FMemoryWriter Ar(ContextData);
	Ar << RuntimeData;

	FMemory::Memcpy(BufferData + Layout.BytecodeOffset, Context.OutputBytecode, Layout.BytecodeSize);
	FMemory::Memcpy(BufferData + Layout.ConstRemapOffset, Context.ConstRemap[1], Layout.ConstRemapSize);
	FMemory::Memcpy(BufferData + Layout.InputRemapOffset, Context.InputRemapTable, Layout.InputRemapSize);
	FMemory::Memcpy(BufferData + Layout.InputDataSetOffsetsOffset, Context.InputDataSetOffsets, Layout.InputDataSetOffsetsSize);
	FMemory::Memcpy(BufferData + Layout.OutputRemapDataSetIdxOffset, Context.OutputRemapDataSetIdx, Layout.OutputRemapDataSetIdxSize);
	FMemory::Memcpy(BufferData + Layout.OutputRemapDataTypeOffset, Context.OutputRemapDataType, Layout.OutputRemapDataTypeSize);
	FMemory::Memcpy(BufferData + Layout.OutputRemapDstOffset, Context.OutputRemapDst, Layout.OutputRemapDstSize);
	FMemory::Memcpy(BufferData + Layout.ExtFnOffset, Context.ExtFnTable, Layout.ExtFnSize);

	// external function pointers
	FVectorVMExtFunctionData* ExtFunctionTable = reinterpret_cast<FVectorVMExtFunctionData*>(BufferData + Layout.ExtFnOffset);
	for (uint32 ExtFunctionIt = 0; ExtFunctionIt < Context.NumExtFns; ++ExtFunctionIt)
	{
		ExtFunctionTable[ExtFunctionIt].Function = nullptr;
	}

}
#endif // WITH_EDITORONLY_DATA

void ThawContext(TConstArrayView<uint8> ContextData, FVectorVMOptimizeContext& Context)
{
	FMemory::Memzero(Context);

	FRuntimeContextData RuntimeData(ContextData);

	VectorVMSerializationHelper::FContextInfoLayout Layout(RuntimeData);

	RuntimeData.CopyToContext(Context);
	const uint8* BufferData = ContextData.GetData();
	Context.OutputBytecode = const_cast<uint8*>(reinterpret_cast<const uint8*>(BufferData + Layout.BytecodeOffset));
	Context.ConstRemap[1] = const_cast<uint16*>(reinterpret_cast<const uint16*>(BufferData + Layout.ConstRemapOffset));
	Context.InputRemapTable = const_cast<uint16*>(reinterpret_cast<const uint16*>(BufferData + Layout.InputRemapOffset));
	Context.InputDataSetOffsets = const_cast<uint16*>(reinterpret_cast<const uint16*>(BufferData + Layout.InputDataSetOffsetsOffset));
	Context.OutputRemapDataSetIdx = const_cast<uint8*>(reinterpret_cast<const uint8*>(BufferData + Layout.OutputRemapDataSetIdxOffset));
	Context.OutputRemapDataType = const_cast<uint16*>(reinterpret_cast<const uint16*>(BufferData + Layout.OutputRemapDataTypeOffset));
	Context.OutputRemapDst = const_cast<uint16*>(reinterpret_cast<const uint16*>(BufferData + Layout.OutputRemapDstOffset));
	Context.ExtFnTable = const_cast<FVectorVMExtFunctionData*>(reinterpret_cast<const FVectorVMExtFunctionData*>(BufferData + Layout.ExtFnOffset));
}

};

#if WITH_EDITORONLY_DATA

void *VVMDefaultRealloc(void *Ptr, size_t NumBytes, const char *Filename, int LineNumber);
void VVMDefaultFree(void *Ptr, const char *Filename, int LineNumber);


struct VVMIOReg {
	union
	{
		FVectorVMOptimizeInstruction *InputIns;    //this is safe to keep as a pointer because no re-ordering happens after this
		FVectorVMOptimizeInstruction *OutputIns;   //this is safe to keep as a pointer because no re-ordering happens after this
	};
	static int SortOutputReg_Fn(const void *a, const void *b) {
		VVMIOReg *R0 = (VVMIOReg *)a;
		VVMIOReg *R1 = (VVMIOReg *)b;
				
		
		uint32 OutputType0 = (uint32)R0->OutputIns->OpCode - (uint32)EVectorVMOp::outputdata_float;
		uint32 OutputType1 = (uint32)R1->OutputIns->OpCode - (uint32)EVectorVMOp::outputdata_float;

		check(R0->OutputIns->Output.DataSetIdx < 255);
		check(R1->OutputIns->Output.DataSetIdx < 255);
		check(OutputType0 < 16);
		check(OutputType1 < 16);

		uint32 V0 = ((uint32)R0->OutputIns->Output.DataSetIdx << 24) | (OutputType0 << 20) | ((uint32)R0->OutputIns->Output.SerialIdx);
		uint32 V1 = ((uint32)R1->OutputIns->Output.DataSetIdx << 24) | (OutputType1 << 20) | ((uint32)R1->OutputIns->Output.SerialIdx);
		check(V0 < (1 << 30)); //There shouldn't be that many DataSets, so we can do a signed compare
		check(V1 < (1 << 30));
		return (int)V0 - (int)V1;
	}
	static int SortInputReg_Fn(const void *a, const void *b) {
		VVMIOReg *R0 = (VVMIOReg *)a;
		VVMIOReg *R1 = (VVMIOReg *)b;
				
		uint32 InputType0 = (uint32)R0->InputIns->OpCode - (uint32)EVectorVMOp::inputdata_float;
		uint32 InputType1 = (uint32)R1->InputIns->OpCode - (uint32)EVectorVMOp::inputdata_float;

		check(R0->InputIns->Input.DataSetIdx < 255);
		check(R1->InputIns->Input.DataSetIdx < 255);
		check(InputType0 < 16);
		check(InputType1 < 16);

		uint32 V0 = ((uint32)R0->InputIns->Input.DataSetIdx << 24) | (InputType0 << 20) | ((uint32)R0->InputIns->Input.InputIdx);
		uint32 V1 = ((uint32)R1->InputIns->Input.DataSetIdx << 24) | (InputType1 << 20) | ((uint32)R1->InputIns->Input.InputIdx);
		check(V0 < (1 << 30)); //There shouldn't be that many DataSets, so we can do a signed compare
		check(V1 < (1 << 30));
		return (int)V0 - (int)V1;
	}
};


static void VectorVMFreeOptimizerIntermediateData(FVectorVMOptimizeContext *OptContext)
{
	if (OptContext->Init.FreeFn)
	{
		OptContext->Init.FreeFn(OptContext->Intermediate.Instructions           , __FILE__, __LINE__);
		OptContext->Init.FreeFn(OptContext->Intermediate.RegisterUsageType      , __FILE__, __LINE__);
		OptContext->Init.FreeFn(OptContext->Intermediate.RegisterUsageBuffer    , __FILE__, __LINE__);
		OptContext->Init.FreeFn(OptContext->Intermediate.SSARegisterUsageBuffer , __FILE__, __LINE__);
		FMemory::Memset(&OptContext->Intermediate, 0, sizeof(OptContext->Intermediate));
	}
	else 
	{
		check(OptContext->Intermediate.Instructions            == nullptr);
		check(OptContext->Intermediate.RegisterUsageType       == nullptr);
		check(OptContext->Intermediate.RegisterUsageBuffer     == nullptr);
		check(OptContext->Intermediate.SSARegisterUsageBuffer  == nullptr);
	}
}

namespace VectorVMScriptStringHelper
{

static void GenerateConstantTableString(const FVectorVMOptimizeContext& Context, FString& OpsConstantTable)
{
	// todo - implement a string of the form:
	// 0 | Engine_WorldDeltaTime
	// 1 | Engine_DeltaTime
	// 2 | Engine_InverseDeltaTime
	// to put into the human readable script version
}

// todo - note that this is just an initial implementation that needs to be revisited to confirm that it's actually
// closely matching the byte that will be run.  In particular the register representations when aliasing output
// buffers seems confusing.
static bool GenerateInstructionString(const FVectorVMOptimizeContext& Context, uint32 InstrIndex, FString& InstructionString)
{
	const FVectorVMOptimizeInstruction& Instruction = Context.Intermediate.Instructions[InstrIndex];

	TStringBuilder<128> OpName;
	TStringBuilder<256> InstString;

#if WITH_EDITOR
	OpName << VectorVM::GetOpName(Instruction.OpCode);
#else
	OpName << TEXT("__OP__") << (int32)Instruction.OpCode;
#endif

	auto WriteRegisterList = [&](int32 RegOffset, int32 Count, FStringBuilderBase& StringBuilder) -> void
	{
		for (int32 RegIt = 0; RegIt < Count; ++RegIt)
		{
			if (RegIt)
			{
				StringBuilder << TEXT(", ");
			}
			switch (Context.Intermediate.RegisterUsageType[RegOffset + RegIt])
			{
				case VVM_RT_TEMPREG: StringBuilder << TEXT("R"); break;
				case VVM_RT_CONST: StringBuilder << TEXT("C"); break;
				case VVM_RT_INPUT: StringBuilder << TEXT("I"); break;
				case VVM_RT_OUTPUT: StringBuilder << TEXT("O"); break;
			}

			StringBuilder << TEXT("[");
			StringBuilder << Context.Intermediate.RegisterUsageBuffer[RegOffset + RegIt];
			StringBuilder << TEXT("]");
		}
	};
	
	switch (Instruction.OpCat)
	{
		case EVectorVMOpCategory::Input:
		{
			check(Instruction.NumOutputRegisters == 1);
			WriteRegisterList(Instruction.RegPtrOffset + Instruction.NumInputRegisters, 1, InstString);
			InstString << TEXT(" = ");
			InstString << OpName;
			InstString << TEXT("(");
			InstString << TEXT(")");
		} break;

		case EVectorVMOpCategory::Output:
		{
			InstString << OpName;
			InstString << TEXT("(");
			InstString << Instruction.Output.DataSetIdx;
			InstString << TEXT(", ");
			InstString << Instruction.Output.DstRegIdx;
			InstString << TEXT(", ");
			WriteRegisterList(Instruction.RegPtrOffset, Instruction.NumInputRegisters, InstString);
			InstString << TEXT(")");
		} break;

		case EVectorVMOpCategory::Op:
		{
			check(Instruction.NumOutputRegisters == 1);
			WriteRegisterList(Instruction.RegPtrOffset + Instruction.NumInputRegisters, 1, InstString);
			InstString << TEXT(" = ");
#if WITH_EDITOR
			InstString << VectorVM::GetOpName(Instruction.OpCode);
#else
			InstString << TEXT("__OP__") << (int32) Instruction.OpCode;
#endif
			InstString << TEXT("(");
			WriteRegisterList(Instruction.RegPtrOffset, Instruction.NumInputRegisters, InstString);
			InstString << TEXT(")");
		} break;

		case EVectorVMOpCategory::ExtFnCall:
		{
			InstString << OpName;
			InstString << TEXT("(");
			WriteRegisterList(Instruction.RegPtrOffset, Instruction.NumInputRegisters, InstString);
			WriteRegisterList(Instruction.RegPtrOffset + Instruction.NumInputRegisters, Instruction.NumOutputRegisters, InstString);
			InstString << TEXT(")");
		} break;

		case EVectorVMOpCategory::IndexGen:
		{
			check(Instruction.NumOutputRegisters == 1);
			WriteRegisterList(Instruction.RegPtrOffset + Instruction.NumInputRegisters, 1, InstString);
			InstString << TEXT(" = ");
			InstString << OpName;
			InstString << TEXT("(");
			WriteRegisterList(Instruction.RegPtrOffset, Instruction.NumInputRegisters, InstString);
			InstString << TEXT(")");
		} break;

		case EVectorVMOpCategory::RWBuffer:
		case EVectorVMOpCategory::Stat:
		case EVectorVMOpCategory::Other:
		{
			InstString << OpName;
			InstString << TEXT("(");
			WriteRegisterList(Instruction.RegPtrOffset, Instruction.NumInputRegisters, InstString);
			InstString << TEXT(")");
		} break;
	}

	InstructionString = InstString.ToString();

	return Instruction.InsMergedIdx == INDEX_NONE;
}

}; // VectorVMScriptStringHelper

void GenerateHumanReadableVectorVMScript(const FVectorVMOptimizeContext& Context, FString& VMScript)
{
	using namespace VectorVMScriptStringHelper;

	FString OpsConstantTable;
	GenerateConstantTableString(Context, OpsConstantTable);

	VMScript += TEXT("\n-------------------------------\n");
	VMScript += TEXT("Summary\n");
	VMScript += TEXT("-------------------------------\n");
	VMScript += FString::Printf(TEXT("Num Byte Code Ops: %d\n"), Context.Intermediate.NumInstructions);
	VMScript += FString::Printf(TEXT("Num Constants: %d\n"), Context.NumConstsRemapped);

	//Dump the constant table
	VMScript += TEXT("\n-------------------------------\n");
	VMScript += TEXT("Constant Table\n");
	VMScript += TEXT("-------------------------------\n");
	VMScript += OpsConstantTable;

	VMScript += TEXT("-------------------------------\n");
	VMScript += FString::Printf(TEXT("Byte Code (%d Ops)\n"), Context.Intermediate.NumInstructions);
	VMScript += TEXT("-------------------------------\n");

	//Dump the bytecode		
	for (uint32 op_idx = 0; op_idx < Context.Intermediate.NumInstructions; ++op_idx)
	{
		FString InstructionString;
		if (GenerateInstructionString(Context, op_idx, InstructionString))
		{
			VMScript += FString::Printf(TEXT("%d\t| "), op_idx) + InstructionString;
			VMScript += TEXT(";\n");
		}
	}

	VMScript += TEXT("-------------------------------\n");
}

void FreeVectorVMOptimizeContext(FVectorVMOptimizeContext *OptContext)
{
	//save init data
	VectorVMReallocFn *ReallocFn  = OptContext->Init.ReallocFn;
	VectorVMFreeFn    *FreeFn     = OptContext->Init.FreeFn;
	const char *       ScriptName = OptContext->Init.ScriptName;
	//save error data
	uint32 ErrorFlags                              = OptContext->Error.Flags;
	uint32 ErrorLine                               = OptContext->Error.Line;
	VectorVMOptimizeErrorCallback *ErrorCallbackFn = OptContext->Error.CallbackFn;
	//save Hash Data
	uint64 HashId = OptContext->HashId;
	//free and zero everything
	if (FreeFn)
	{
		FreeFn(OptContext->OutputBytecode        , __FILE__, __LINE__);
		FreeFn(OptContext->ConstRemap[0]         , __FILE__, __LINE__);
		FreeFn(OptContext->ConstRemap[1]         , __FILE__, __LINE__);
		FreeFn(OptContext->InputRemapTable       , __FILE__, __LINE__);
		FreeFn(OptContext->InputDataSetOffsets   , __FILE__, __LINE__);
		FreeFn(OptContext->OutputRemapDataSetIdx , __FILE__, __LINE__);
		FreeFn(OptContext->OutputRemapDataType   , __FILE__, __LINE__);
		FreeFn(OptContext->OutputRemapDst        , __FILE__, __LINE__);
		FreeFn(OptContext->ExtFnTable            , __FILE__, __LINE__);
	}
	else
	{
		check(OptContext->OutputBytecode        == nullptr);
		check(OptContext->ConstRemap[0]         == nullptr);
		check(OptContext->ConstRemap[1]         == nullptr);
		check(OptContext->InputRemapTable       == nullptr);
		check(OptContext->InputDataSetOffsets   == nullptr);
		check(OptContext->OutputRemapDataSetIdx == nullptr);
		check(OptContext->OutputRemapDataType   == nullptr);
		check(OptContext->OutputRemapDst        == nullptr);
		check(OptContext->ExtFnTable            == nullptr);
	}
	VectorVMFreeOptimizerIntermediateData(OptContext);
	FMemory::Memset(OptContext, 0, sizeof(FVectorVMOptimizeContext));
	//restore init data
	OptContext->Init.ReallocFn   = ReallocFn;
	OptContext->Init.FreeFn      = FreeFn;
	OptContext->Init.ScriptName  = ScriptName;
	//restore error data
	OptContext->Error.Flags      = ErrorFlags;
	OptContext->Error.Line       = ErrorLine;
	OptContext->Error.CallbackFn = ErrorCallbackFn;
	//restore hash data
	OptContext->HashId           = HashId;
}

static uint32 VectorVMOptimizerSetError_(FVectorVMOptimizeContext *OptContext, uint32 Flags, uint32 LineNum)
{
	OptContext->Error.Line = LineNum;
	if (OptContext->Error.CallbackFn)
	{
		OptContext->Error.Flags = OptContext->Error.CallbackFn(OptContext, OptContext->Error.Flags | Flags);
	}
	else
	{
		OptContext->Error.Flags |= Flags;
	}
	if (OptContext->Error.Flags & VVMOptErr_Fatal)
	{
		check(false); //hit the debugger
		FreeVectorVMOptimizeContext(OptContext);
	}
	return OptContext->Error.Flags;
}

#define VectorVMOptimizerSetError(Context, Flags)   VectorVMOptimizerSetError_(Context, Flags, __LINE__)

static uint16 VectorVMOptimizeRemapConst(FVectorVMOptimizeContext *OptContext, uint16 ConstIdx)
{
	check((ConstIdx & 3) == 0);
	ConstIdx >>= 2; //original VM uses byte indices, we use 32 bit indices
	if (ConstIdx >= OptContext->NumConstsAlloced)
	{
		uint16 NumConstsToAlloc = (ConstIdx + 1 + 511) & ~511;
		uint16 *ConstRemap0 = (uint16 *)OptContext->Init.ReallocFn(OptContext->ConstRemap[0], sizeof(uint16) * NumConstsToAlloc, __FILE__, __LINE__);
		if (ConstRemap0 == nullptr)
		{
			VectorVMOptimizerSetError(OptContext, VVMOptErr_OutOfMemory | VVMOptErr_ConstRemap | VVMOptErr_Fatal);
			return 0;
		}
		OptContext->ConstRemap[0] = ConstRemap0;
		uint16 *ConstRemap1 = (uint16 *)OptContext->Init.ReallocFn(OptContext->ConstRemap[1], sizeof(uint16) * NumConstsToAlloc, __FILE__, __LINE__);
		if (ConstRemap1 == nullptr)
		{
			VectorVMOptimizerSetError(OptContext, VVMOptErr_OutOfMemory | VVMOptErr_ConstRemap | VVMOptErr_Fatal);
			return 0;
		}
		OptContext->ConstRemap[1] = ConstRemap1;
		if (NumConstsToAlloc > OptContext->NumConstsAlloced)
		{
			FMemory::Memset(OptContext->ConstRemap[0] + OptContext->NumConstsAlloced, 0xFF, sizeof(uint16) * (NumConstsToAlloc - OptContext->NumConstsAlloced));
			FMemory::Memset(OptContext->ConstRemap[1] + OptContext->NumConstsAlloced, 0xFF, sizeof(uint16) * (NumConstsToAlloc - OptContext->NumConstsAlloced));
		}
		OptContext->NumConstsAlloced = NumConstsToAlloc;
	}
	if (OptContext->ConstRemap[0][ConstIdx] == 0xFFFF)
	{
		OptContext->ConstRemap[0][ConstIdx] = OptContext->NumConstsRemapped;
		OptContext->ConstRemap[1][OptContext->NumConstsRemapped] = ConstIdx;
		OptContext->NumConstsRemapped++;
		check(OptContext->NumConstsRemapped <= OptContext->NumConstsAlloced);
	}
	else
	{
		check(OptContext->ConstRemap[1][OptContext->ConstRemap[0][ConstIdx]] == ConstIdx);
	}
	return OptContext->ConstRemap[0][ConstIdx];
}


inline uint64 VVMOutputInsGetSortKey(uint16 *SSARegisters, FVectorVMOptimizeInstruction *OutputIns)
{
	check(OutputIns->OpCat == EVectorVMOpCategory::Output);
	check(OutputIns->Output.DataSetIdx < (1 << 14));                                                // max 14 bits for DataSet Index (In reality this number is < 5... ie 3 bits)
	check((int)OutputIns->OpCode >= (int)EVectorVMOp::outputdata_float);
	uint64 key = (((uint64)OutputIns->OpCode - (uint64)EVectorVMOp::outputdata_float) << 62ULL) +
		         ((uint64)OutputIns->Output.DataSetIdx                                << 48ULL) +
		         ((uint64)SSARegisters[OutputIns->RegPtrOffset]                       << 16ULL) +
		         ((uint64)OutputIns->Output.DstRegIdx)                                          ;
	return key;
}

static void VVMEnsureInstructionAlloced(FVectorVMOptimizeContext *OptContext, uint32 NumInstructions) {
	uint32 NewNumInstructionsAlloced = OptContext->Intermediate.NumInstructionsAlloced;
	while (OptContext->Intermediate.NumRegistersUsed + NumInstructions >= NewNumInstructionsAlloced)
	{
		if (NewNumInstructionsAlloced == 0)
		{
			NewNumInstructionsAlloced = 16384;
		}
		else
		{
			NewNumInstructionsAlloced <<= 1;
		}
	}
	if (NewNumInstructionsAlloced != OptContext->Intermediate.NumInstructionsAlloced)
	{
		FVectorVMOptimizeInstruction *NewInstructions = (FVectorVMOptimizeInstruction *)OptContext->Init.ReallocFn(OptContext->Intermediate.Instructions, sizeof(FVectorVMOptimizeInstruction) * NewNumInstructionsAlloced, __FILE__, __LINE__);
		if (NewInstructions == nullptr)
		{
			if (OptContext->Intermediate.Instructions)
			{
				FMemory::Free(OptContext->Intermediate.Instructions);
				OptContext->Intermediate.Instructions = nullptr;
			}
			return;
		}
		OptContext->Intermediate.Instructions           = NewInstructions;
		OptContext->Intermediate.NumInstructionsAlloced = NewNumInstructionsAlloced;
	}
}

static FVectorVMOptimizeInstruction *VVMPushNewInstruction(FVectorVMOptimizeContext *OptContext, EVectorVMOp OpCode)
{
	VVMEnsureInstructionAlloced(OptContext, 32);
	FVectorVMOptimizeInstruction *Ins = OptContext->Intermediate.Instructions + OptContext->Intermediate.NumInstructions;
	Ins->Index              = OptContext->Intermediate.NumInstructions++;
	Ins->OpCode             = OpCode;
	Ins->OpCat              = VVM_OP_CATEGORIES[(int)OpCode];
	Ins->InsMergedIdx       = -1;
	Ins->OutputMergeIdx[0]  = -1;
	Ins->OutputMergeIdx[1]  = -1;
	Ins->RegPtrOffset       = OptContext->Intermediate.NumRegistersUsed;
	Ins->NumInputRegisters  = 0;
	Ins->NumOutputRegisters = 0;
	if (Ins->OpCat == EVectorVMOpCategory::Output) {
		Ins->Output.MergeIdx = -1;
	}
	return Ins;
}

static bool RegDupeCheck(FVectorVMOptimizeContext *OptContext, FVectorVMOptimizeInstruction *Ins0, int Ins0OutputRegIdx, FVectorVMOptimizeInstruction *Ins1, int FirstRegIdx, int LastRegIdx) {
	uint16 Ins0OutputSSA = OptContext->Intermediate.SSARegisterUsageBuffer[Ins0->RegPtrOffset + Ins0OutputRegIdx];

	uint16 *SSA = OptContext->Intermediate.SSARegisterUsageBuffer + Ins1->RegPtrOffset;
	uint8 *Type = OptContext->Intermediate.RegisterUsageType + Ins1->RegPtrOffset;

	for (int i = FirstRegIdx; i < LastRegIdx; ++i)
	{
		if (SSA[i] == Ins0OutputSSA && Type[i] == VVM_RT_TEMPREG)
		{
			for (int j = i + 1; j <= LastRegIdx; ++j)
			{
				if (SSA[i] == SSA[j] && Type[i] == Type[j])
				{
					return true;
				}
			}
		}
	}
	return false;
}

static void VVMEnsureRegAlloced(FVectorVMOptimizeContext *OptContext, bool AllocSSA, uint32 *NumRegisterUsageAlloced, uint32 NumRegisters)
{
	uint32 NewNumRegisterUsageAlloced = *NumRegisterUsageAlloced;
	while (OptContext->Intermediate.NumRegistersUsed + NumRegisters >= NewNumRegisterUsageAlloced)
	{
		if (NewNumRegisterUsageAlloced == 0)
		{
			NewNumRegisterUsageAlloced = 16384;
		}
		else
		{
			NewNumRegisterUsageAlloced <<= 1;
		}
	}
	if (NewNumRegisterUsageAlloced != *NumRegisterUsageAlloced)
	{
		*NumRegisterUsageAlloced = NewNumRegisterUsageAlloced;
		uint16 *NewRegisters  = (uint16 *)OptContext->Init.ReallocFn(OptContext->Intermediate.RegisterUsageBuffer, sizeof(uint16) * NewNumRegisterUsageAlloced, __FILE__, __LINE__);
		uint8  *NewRegType    = (uint8  *)OptContext->Init.ReallocFn(OptContext->Intermediate.RegisterUsageType  , sizeof(uint8 ) * NewNumRegisterUsageAlloced, __FILE__, __LINE__);
		uint16 *NewSSA        = NULL;
		if (AllocSSA)
		{
			NewSSA           = (uint16 *)OptContext->Init.ReallocFn(OptContext->Intermediate.SSARegisterUsageBuffer, sizeof(uint16) * NewNumRegisterUsageAlloced, __FILE__, __LINE__);
		}
		if (NewRegisters == nullptr || NewRegType == nullptr || (AllocSSA && NewSSA == nullptr))
		{
			VectorVMOptimizerSetError(OptContext, VVMOptErr_OutOfMemory | VVMOptErr_RegisterUsage | VVMOptErr_Fatal);
			return;
		}
		OptContext->Intermediate.RegisterUsageBuffer = NewRegisters;
		OptContext->Intermediate.RegisterUsageType   = NewRegType;
		if (AllocSSA) {
			OptContext->Intermediate.SSARegisterUsageBuffer = NewSSA;
		}
	}
}

static uint32 VVMPushRegUsage_(FVectorVMOptimizeContext *OptContext, FVectorVMOptimizeInstruction *Instruction, uint16 RegIdx, uint32 Type, uint32 IOFlag, uint32 *NumRegisterUsageAlloced)
{
	if (Type == VVM_RT_CONST)
	{
		uint16 RemappedIdx = VectorVMOptimizeRemapConst(OptContext, RegIdx);
		if (OptContext->Error.Flags & VVMOptErr_Fatal)
		{
			return OptContext->Error.Flags;
		}
		OptContext->Intermediate.RegisterUsageBuffer[OptContext->Intermediate.NumRegistersUsed] = RemappedIdx;
	}
	else
	{
		OptContext->Intermediate.RegisterUsageBuffer[OptContext->Intermediate.NumRegistersUsed] = RegIdx;
	}

	OptContext->Intermediate.RegisterUsageType  [OptContext->Intermediate.NumRegistersUsed] = Type;
	if (IOFlag)
	{
		++Instruction->NumOutputRegisters;
	}
	else
	{
		++Instruction->NumInputRegisters;
	}
	++OptContext->Intermediate.NumRegistersUsed;
	return 0;
}

static int VVMRemoveAvailableOutput(uint16 *AvailableOutputs, int NumAvailableOutputs, uint16 OutputInsSerialIdx)
{
	for (int i = 0; i < NumAvailableOutputs; ++i)
	{
		if (AvailableOutputs[i] == OutputInsSerialIdx)
		{
			FMemory::Memmove(AvailableOutputs + i, AvailableOutputs + i + 1, sizeof(uint16) * (NumAvailableOutputs - i - 1));
			--NumAvailableOutputs;
		}
	}
	return NumAvailableOutputs;
}

static void VVMOptWriteReg_(FVectorVMOptimizeContext *OptContext, uint16 Idx, uint16 *FinalTempRegRemap, uint8 *OptimizedBytecode, int NumOptimizedBytesWritten)
{
	uint16 Reg = OptContext->Intermediate.RegisterUsageBuffer[Idx];
	uint8 Type = OptContext->Intermediate.RegisterUsageType[Idx];
	if (Reg == 0xFFFF || Type == VVM_RT_INVALID) { /* invalid reg */
		OptimizedBytecode[NumOptimizedBytesWritten + 0] = 0xFF;
		OptimizedBytecode[NumOptimizedBytesWritten + 1] = 0xFF;
	} else {
		if (Type == VVM_RT_CONST) {
			Reg += OptContext->NumTempRegisters;
		} else if (Type == VVM_RT_INPUT) {
			Reg += OptContext->NumTempRegisters + OptContext->NumConstsRemapped;
		} else if (Type == VVM_RT_OUTPUT) {
			Reg += OptContext->NumTempRegisters + OptContext->NumConstsRemapped + OptContext->NumInputsRemapped * 2;
		} else {
			Reg = FinalTempRegRemap[Reg];
		}
		OptimizedBytecode[NumOptimizedBytesWritten + 0] = (uint8)(Reg & 0xFF);
		OptimizedBytecode[NumOptimizedBytesWritten + 1] = (uint8)(Reg >> 8);
	}
}

static void VVMSetParentInstructionIndices(FVectorVMOptimizeContext *OptContext)
{
	for (int i = 0; i < (int)OptContext->Intermediate.NumInstructions; ++i)
	{
		FVectorVMOptimizeInstruction *Ins = OptContext->Intermediate.Instructions + i;
		for (int j = 0; j < Ins->NumOutputRegisters; ++j)
		{
			uint16 SSAReg = OptContext->Intermediate.SSARegisterUsageBuffer[Ins->RegPtrOffset + Ins->NumInputRegisters + j];
			if (SSAReg != 0xFFFF)
			{
				OptContext->Intermediate.ParentInstructionIdx[SSAReg] = i;
			}
		}
	}
}

static bool VVMIsInstructionDependentOnInstruction(FVectorVMOptimizeContext *OptContext, FVectorVMOptimizeInstruction *Ins, uint16 *InstructionIdxStack, int DependentIdx) {
	int NumInstructions = 0;
	int NumInstructionsChecked = 0;
	do {
		for (int i = 0; i < Ins->NumInputRegisters; ++i) {
			uint16 SSAReg  = OptContext->Intermediate.SSARegisterUsageBuffer[Ins->RegPtrOffset + i];
			uint16 RegType = OptContext->Intermediate.RegisterUsageType[Ins->RegPtrOffset + i];
			if (SSAReg != 0xFFFF && (RegType == VVM_RT_TEMPREG || RegType == VVM_RT_OUTPUT))
			{
				uint16 ParentInsIdx = OptContext->Intermediate.ParentInstructionIdx[SSAReg];
				check(ParentInsIdx != 0xFFFF);
				if (ParentInsIdx == DependentIdx) {
					return true;
				}
				bool InsAlreadyInStack = false;
				for (int j = 0; j < NumInstructions; ++j)
				{
					if (ParentInsIdx == InstructionIdxStack[j])
					{
						InsAlreadyInStack = true;
						break;
					}
				}
				if (!InsAlreadyInStack)
				{
					InstructionIdxStack[NumInstructions++] = ParentInsIdx;
				}
			}
		}
		Ins = OptContext->Intermediate.Instructions + InstructionIdxStack[NumInstructionsChecked++];
	} while (NumInstructionsChecked <= NumInstructions);
	return false;
}

static int VVMGetInstructionDependencyChain(FVectorVMOptimizeContext *OptContext, FVectorVMOptimizeInstruction *Ins, uint16 *InstructionIdxStack)
{	
	int NumInstructions = 0;
	int NumInstructionsChecked = 0;
	uint16 *UnSortedInstructionIdxStack = InstructionIdxStack + OptContext->Intermediate.NumRegistersUsed;
	do {
		for (int i = 0; i < Ins->NumInputRegisters; ++i) {
			uint16 SSAReg  = OptContext->Intermediate.SSARegisterUsageBuffer[Ins->RegPtrOffset + i];
			uint16 RegType = OptContext->Intermediate.RegisterUsageType[Ins->RegPtrOffset + i];
			if (SSAReg != 0xFFFF && (RegType == VVM_RT_TEMPREG || RegType == VVM_RT_OUTPUT))
			{
				uint16 ParentInsIdx = OptContext->Intermediate.ParentInstructionIdx[SSAReg];
				check(ParentInsIdx != 0xFFFF);
				bool InsAlreadyInStack = false;
				for (int j = 0; j < NumInstructions; ++j)
				{
					if (ParentInsIdx == UnSortedInstructionIdxStack[j])
					{
						InsAlreadyInStack = true;
						break;
					}
				}
				if (!InsAlreadyInStack)
				{
					//insert in sorted low-to-high order
					int InsertionSlot = NumInstructions;
					for (int k = 0; k < NumInstructions; ++k)
					{
						if (ParentInsIdx < InstructionIdxStack[k])
						{
							InsertionSlot = k;
							uint16 *RESTRICT s = InstructionIdxStack + NumInstructions - 1;
							uint16 *RESTRICT e = InstructionIdxStack + InsertionSlot;
							while (s >= e) {
								s[1] = s[0];
								--s;
							}
							break;
						}
					}
					InstructionIdxStack[InsertionSlot] = ParentInsIdx;
					UnSortedInstructionIdxStack[NumInstructions++] = ParentInsIdx;

				}
			}
		}
		Ins = OptContext->Intermediate.Instructions + UnSortedInstructionIdxStack[NumInstructionsChecked++];
	} while (NumInstructionsChecked <= NumInstructions);
	return NumInstructions;
}

static bool VVMDoesInstructionHaveDependencies(FVectorVMOptimizeContext *OptContext, FVectorVMOptimizeInstruction *Ins) {
	for (int i = 0; i < Ins->NumInputRegisters; ++i) {
		uint16 SSAReg  = OptContext->Intermediate.SSARegisterUsageBuffer[Ins->RegPtrOffset + i];
		uint16 RegType = OptContext->Intermediate.RegisterUsageType[Ins->RegPtrOffset + i];
		if (SSAReg != 0xFFFF && (RegType == VVM_RT_TEMPREG || RegType == VVM_RT_OUTPUT))
		{
			return true;
		}
	}
	return false;
}

#	define VVMPushRegUsage(RegIdx, Type, IOFlag)	          do { uint32 Res = VVMPushRegUsage_(OptContext, Instruction, RegIdx, Type, IOFlag, &NumRegisterUsageAlloced);                              if (Res != 0) { return Res; } } while(0);



#define TEMP_REG_NAME(idx) \
		TEMP_REG_TYPE_NAMES[OptContext->Intermediate.RegisterUsageType[OptContext->Intermediate.NumRegistersUsed - idx - 1]], \
		OptContext->Intermediate.RegisterUsageBuffer[OptContext->Intermediate.NumRegistersUsed - idx - 1]


#define VVMOptimizeVecIns1							Instruction->NumInputRegisters  = 1; Instruction->NumOutputRegisters = 1; goto handle_op_ins;
#define VVMOptimizeVecIns2							Instruction->NumInputRegisters  = 2; Instruction->NumOutputRegisters = 1; goto handle_op_ins;
#define VVMOptimizeVecIns3							Instruction->NumInputRegisters  = 3; Instruction->NumOutputRegisters = 1; goto handle_op_ins;

uint32 OptimizeVectorVMScript(const uint8 *InBytecode, int InBytecodeLen, FVectorVMExtFunctionData *ExtFnIOData, int NumExtFns, FVectorVMOptimizeContext *OptContext, uint64 HashId, uint32 Flags)
{
	FreeVectorVMOptimizeContext(OptContext);
	if (InBytecode == nullptr || InBytecodeLen <= 1)
	{
		return 0;
	}

	// skip the script if the input is empty
	if ((InBytecodeLen == 1) && (EVectorVMOp(InBytecode[0]) == EVectorVMOp::done))
	{
		return 0;
	}

	if (OptContext->Init.ReallocFn == nullptr)
	{
		OptContext->Init.ReallocFn = VVMDefaultRealloc;
	}
	if (OptContext->Init.FreeFn == nullptr)
	{
		OptContext->Init.FreeFn = VVMDefaultFree;
	}
	OptContext->HashId             = HashId;
	OptContext->Flags              = Flags;
	OptContext->MaxExtFnUsed       = -1;

	uint32 NumBytecodeBytesAlloced = 0;
	uint32 NumRegisterUsageAlloced = 0;
	uint16 NumOutputInstructions   = 0;
	int32  MaxDataSetIdx           = 0;

	const uint8 *OpPtrIn = InBytecode;
	const uint8 *OpPtrInEnd = InBytecode + InBytecodeLen;

	// todo - consider a flag on the op or to better futureproof merged ops by ensuring that
	// if a merged op incorporates one of these ops, then it too must be in this set
	const TArray<EVectorVMOp> RandInstructionSet =
	{
		EVectorVMOp::random,
		EVectorVMOp::randomi,
		EVectorVMOp::random_add,
		EVectorVMOp::random_2x
	};

	//Step 1: Create Intermediate representation of all Instructions
	while (OpPtrIn < OpPtrInEnd)
	{
		uint16 *VecIndices = (uint16 *)(OpPtrIn + 2);
		EVectorVMOp OpCode = (EVectorVMOp)*OpPtrIn;

		FVectorVMOptimizeInstruction *Instruction = VVMPushNewInstruction(OptContext, OpCode);
		if (Instruction == nullptr) {
			return VectorVMOptimizerSetError(OptContext, VVMOptErr_OutOfMemory | VVMOptErr_Instructions | VVMOptErr_Fatal);
		}
		Instruction->PtrOffsetInOrigBytecode = (uint32)(OpPtrIn - InBytecode);
		OpPtrIn++;
		VVMEnsureRegAlloced(OptContext, false, &NumRegisterUsageAlloced, 8);
		switch (OpCode)
		{
			case EVectorVMOp::done:                                                                 break;
			case EVectorVMOp::add:                                  VVMOptimizeVecIns2;             break;
			case EVectorVMOp::sub:                                  VVMOptimizeVecIns2;             break;
			case EVectorVMOp::mul:                                  VVMOptimizeVecIns2;             break;
			case EVectorVMOp::div:                                  VVMOptimizeVecIns2;             break;
			case EVectorVMOp::mad:                                  VVMOptimizeVecIns3;             break;
			case EVectorVMOp::lerp:                                 VVMOptimizeVecIns3;             break;
			case EVectorVMOp::rcp:                                  VVMOptimizeVecIns1;             break;
			case EVectorVMOp::rsq:                                  VVMOptimizeVecIns1;             break;
			case EVectorVMOp::sqrt:                                 VVMOptimizeVecIns1;             break;
			case EVectorVMOp::neg:                                  VVMOptimizeVecIns1;             break;
			case EVectorVMOp::abs:                                  VVMOptimizeVecIns1;             break;
			case EVectorVMOp::exp:                                  VVMOptimizeVecIns1;             break;
			case EVectorVMOp::exp2:                                 VVMOptimizeVecIns1;             break;
			case EVectorVMOp::log:                                  VVMOptimizeVecIns1;             break;
			case EVectorVMOp::log2:                                 VVMOptimizeVecIns1;             break;
			case EVectorVMOp::sin:                                  VVMOptimizeVecIns1;             break;
			case EVectorVMOp::cos:                                  VVMOptimizeVecIns1;             break;
			case EVectorVMOp::tan:                                  VVMOptimizeVecIns1;             break;
			case EVectorVMOp::asin:                                 VVMOptimizeVecIns1;             break;
			case EVectorVMOp::acos:                                 VVMOptimizeVecIns1;             break;
			case EVectorVMOp::atan:                                 VVMOptimizeVecIns1;             break;
			case EVectorVMOp::atan2:                                VVMOptimizeVecIns2;             break;
			case EVectorVMOp::ceil:                                 VVMOptimizeVecIns1;             break;
			case EVectorVMOp::floor:                                VVMOptimizeVecIns1;             break;
			case EVectorVMOp::fmod:                                 VVMOptimizeVecIns2;             break;
			case EVectorVMOp::frac:                                 VVMOptimizeVecIns1;             break;
			case EVectorVMOp::trunc:                                VVMOptimizeVecIns1;             break;
			case EVectorVMOp::clamp:                                VVMOptimizeVecIns3;             break;
			case EVectorVMOp::min:                                  VVMOptimizeVecIns2;             break;
			case EVectorVMOp::max:                                  VVMOptimizeVecIns2;             break;
			case EVectorVMOp::pow:                                  VVMOptimizeVecIns2;             break;
			case EVectorVMOp::round:                                VVMOptimizeVecIns1;             break;
			case EVectorVMOp::sign:                                 VVMOptimizeVecIns1;             break;
			case EVectorVMOp::step:                                 VVMOptimizeVecIns2;             break;
			case EVectorVMOp::random:                               VVMOptimizeVecIns1;             break;
			case EVectorVMOp::noise:                                check(false);                   break;
			case EVectorVMOp::cmplt:                                VVMOptimizeVecIns2;             break;
			case EVectorVMOp::cmple:                                VVMOptimizeVecIns2;             break;
			case EVectorVMOp::cmpgt:                                VVMOptimizeVecIns2;             break;
			case EVectorVMOp::cmpge:                                VVMOptimizeVecIns2;             break;
			case EVectorVMOp::cmpeq:                                VVMOptimizeVecIns2;             break;
			case EVectorVMOp::cmpneq:                               VVMOptimizeVecIns2;             break;
			case EVectorVMOp::select:                               VVMOptimizeVecIns3;             break;
			case EVectorVMOp::addi:                                 VVMOptimizeVecIns2;             break;
			case EVectorVMOp::subi:                                 VVMOptimizeVecIns2;             break;
			case EVectorVMOp::muli:                                 VVMOptimizeVecIns2;             break;
			case EVectorVMOp::divi:                                 VVMOptimizeVecIns2;             break;
			case EVectorVMOp::clampi:                               VVMOptimizeVecIns3;             break;
			case EVectorVMOp::mini:                                 VVMOptimizeVecIns2;             break;
			case EVectorVMOp::maxi:                                 VVMOptimizeVecIns2;             break;
			case EVectorVMOp::absi:                                 VVMOptimizeVecIns1;             break;
			case EVectorVMOp::negi:                                 VVMOptimizeVecIns1;             break;
			case EVectorVMOp::signi:                                VVMOptimizeVecIns1;             break;
			case EVectorVMOp::randomi:                              VVMOptimizeVecIns1;             break;
			case EVectorVMOp::cmplti:                               VVMOptimizeVecIns2;             break;
			case EVectorVMOp::cmplei:                               VVMOptimizeVecIns2;             break;
			case EVectorVMOp::cmpgti:                               VVMOptimizeVecIns2;             break;
			case EVectorVMOp::cmpgei:                               VVMOptimizeVecIns2;             break;
			case EVectorVMOp::cmpeqi:                               VVMOptimizeVecIns2;             break;
			case EVectorVMOp::cmpneqi:                              VVMOptimizeVecIns2;             break;
			case EVectorVMOp::bit_and:                              VVMOptimizeVecIns2;             break;
			case EVectorVMOp::bit_or:                               VVMOptimizeVecIns2;             break;
			case EVectorVMOp::bit_xor:                              VVMOptimizeVecIns2;             break;
			case EVectorVMOp::bit_not:                              VVMOptimizeVecIns1;             break;
			case EVectorVMOp::bit_lshift:                           VVMOptimizeVecIns2;             break;
			case EVectorVMOp::bit_rshift:                           VVMOptimizeVecIns2;             break;
			case EVectorVMOp::logic_and:                            VVMOptimizeVecIns2;             break;
			case EVectorVMOp::logic_or:                             VVMOptimizeVecIns2;             break;
			case EVectorVMOp::logic_xor:                            VVMOptimizeVecIns2;             break;
			case EVectorVMOp::logic_not:                            VVMOptimizeVecIns1;             break;
			case EVectorVMOp::f2i:                                  VVMOptimizeVecIns1;             break;
			case EVectorVMOp::i2f:                                  VVMOptimizeVecIns1;             break;
			case EVectorVMOp::f2b:                                  VVMOptimizeVecIns1;             break;
			case EVectorVMOp::b2f:                                  VVMOptimizeVecIns1;             break;
			case EVectorVMOp::i2b:                                  VVMOptimizeVecIns1;             break;
			case EVectorVMOp::b2i:                                  VVMOptimizeVecIns1;             break;
			case EVectorVMOp::fasi:                                 VVMOptimizeVecIns1;             break;
			case EVectorVMOp::iasf:                                 VVMOptimizeVecIns1;             break;
			case EVectorVMOp::inputdata_float:
			case EVectorVMOp::inputdata_int32:
			case EVectorVMOp::inputdata_half:
			case EVectorVMOp::inputdata_noadvance_float:
			case EVectorVMOp::inputdata_noadvance_int32:
			case EVectorVMOp::inputdata_noadvance_half:
			{
				uint16 DataSetIdx	= *(uint16 *)(OpPtrIn    );
				uint16 InputRegIdx	= *(uint16 *)(OpPtrIn + 2);
				uint16 DstRegIdx	= *(uint16 *)(OpPtrIn + 4);
				
				if (DataSetIdx > MaxDataSetIdx) {
					MaxDataSetIdx = DataSetIdx;
				}

				Instruction->RegPtrOffset       = OptContext->Intermediate.NumRegistersUsed;
				Instruction->Input.DataSetIdx   = DataSetIdx;
				Instruction->Input.InputIdx     = InputRegIdx;
				
				VVMPushRegUsage(DstRegIdx, VVM_RT_TEMPREG, 1);
				if (OpCode == EVectorVMOp::inputdata_noadvance_float || OpCode == EVectorVMOp::inputdata_noadvance_int32 || OpCode == EVectorVMOp::inputdata_noadvance_half)
				{
					++OptContext->NumNoAdvanceInputs;
				}

				OpPtrIn += 6;
			}
			break;
			case EVectorVMOp::outputdata_float:
			case EVectorVMOp::outputdata_int32:
			case EVectorVMOp::outputdata_half:
			{
				uint8 OpType        = *OpPtrIn & 1; //0: reg, 1: const
				uint16 DataSetIdx   = VecIndices[0];
				uint16 DstIdxRegIdx = VecIndices[1];
				uint16 SrcReg       = VecIndices[2];
				uint16 DstRegIdx    = VecIndices[3];
				check(DataSetIdx < 0xFF);
				OptContext->MaxOutputDataSet = FMath::Max(OptContext->MaxOutputDataSet, (uint32)(DataSetIdx + 1));
				if (DataSetIdx > MaxDataSetIdx) {
					MaxDataSetIdx = DataSetIdx;
				}

				Instruction->RegPtrOffset               = OptContext->Intermediate.NumRegistersUsed;
				Instruction->Output.DataSetIdx          = DataSetIdx;
				Instruction->Output.DstRegIdx           = DstRegIdx;
				Instruction->Output.SerialIdx           = NumOutputInstructions++;
				VVMPushRegUsage(DstIdxRegIdx, VVM_RT_TEMPREG, 0);
				VVMPushRegUsage(SrcReg, OpType == 0 ? VVM_RT_TEMPREG : VVM_RT_CONST, 0);
				OpPtrIn += 9;
			}
			break;
			case EVectorVMOp::acquireindex:
			{
				uint8 OpType        = *OpPtrIn & 1;							//0: reg, 1: const4
				uint16 DataSetIdx   = VecIndices[0];
				uint16 OutputReg    = VecIndices[2];
				uint16 InputRegIdx  = VecIndices[1];
				uint8  InputRegType = VVM_RT_TEMPREG;
				if (DataSetIdx > MaxDataSetIdx) {
					MaxDataSetIdx = DataSetIdx;
				}

				Instruction->RegPtrOffset        = OptContext->Intermediate.NumRegistersUsed;
				Instruction->IndexGen.DataSetIdx = DataSetIdx;
				
				VVMPushRegUsage(InputRegIdx, OpType == 0 ? VVM_RT_TEMPREG : VVM_RT_CONST, 0);
				VVMPushRegUsage(OutputReg, VVM_RT_TEMPREG, 1);
				OpPtrIn += 7;
			}
			break;
			case EVectorVMOp::external_func_call:
			{
				uint8 ExtFnIdx = *OpPtrIn;
				check(ExtFnIdx < NumExtFns);

				Instruction->RegPtrOffset           = OptContext->Intermediate.NumRegistersUsed;
				Instruction->ExtFnCall.ExtFnIdx     = ExtFnIdx;
				Instruction->ExtFnCall.NumInputs    = ExtFnIOData[ExtFnIdx].NumInputs;
				Instruction->ExtFnCall.NumOutputs   = ExtFnIOData[ExtFnIdx].NumOutputs;
				
				for (int i = 0; i < ExtFnIOData[ExtFnIdx].NumInputs; ++i)
				{
					if (VecIndices[i] == 0xFFFF)
					{
						VVMPushRegUsage(0xFFFF, VVM_RT_INVALID, 0);
					}
					else
					{
						if (VecIndices[i] & 0x8000) //register: high bit means input is a register in the original bytecode
						{ 
							VVMPushRegUsage(VecIndices[i] & 0x7FFF, VVM_RT_TEMPREG, 0);
						}
						else //constant
						{
							VVMPushRegUsage(VecIndices[i], VVM_RT_CONST, 0);
						}
					}
				}
				for (int i = 0; i < ExtFnIOData[ExtFnIdx].NumOutputs; ++i)
				{
					int Idx = ExtFnIOData[ExtFnIdx].NumInputs + i;
					check((VecIndices[Idx] & 0x8000) == 0 || VecIndices[Idx] == 0xFFFF); //can't output to a const... 0xFFFF is invalid
					VVMPushRegUsage(VecIndices[Idx], VVM_RT_TEMPREG, 1);
				}
				OptContext->MaxExtFnUsed      = FMath::Max(OptContext->MaxExtFnUsed, (int32)ExtFnIdx);
				OptContext->MaxExtFnRegisters = FMath::Max(OptContext->MaxExtFnRegisters, (uint32)(ExtFnIOData[ExtFnIdx].NumInputs + ExtFnIOData[ExtFnIdx].NumOutputs));
				OpPtrIn += 1 + (ExtFnIOData[ExtFnIdx].NumInputs + ExtFnIOData[ExtFnIdx].NumOutputs) * 2;
			}
			break;
			case EVectorVMOp::exec_index:
				Instruction->RegPtrOffset    = OptContext->Intermediate.NumRegistersUsed;
				VVMPushRegUsage(*OpPtrIn, VVM_RT_TEMPREG, 1);
				OpPtrIn += 2;
				break;
			case EVectorVMOp::noise2D:                              check(false);               break;
			case EVectorVMOp::noise3D:                              check(false);               break;
			case EVectorVMOp::enter_stat_scope:						
				Instruction->Stat.ID = *OpPtrIn;                    OpPtrIn += 2;               break;
			case EVectorVMOp::exit_stat_scope:                                                  break;
			case EVectorVMOp::update_id:                            //intentional fallthrough
			case EVectorVMOp::acquire_id:
			{
				uint16 DataSetIdx = ((uint16 *)OpPtrIn)[0];
				uint16 IDIdxReg = ((uint16 *)OpPtrIn)[1];
				uint16 IDTagReg = ((uint16 *)OpPtrIn)[2];

				if (DataSetIdx > MaxDataSetIdx) {
					MaxDataSetIdx = DataSetIdx;
				}
			
				Instruction->RegPtrOffset        = OptContext->Intermediate.NumRegistersUsed;
				Instruction->RWBuffer.DataSetIdx = DataSetIdx;
			
				uint32 RegIOFlag = Instruction->OpCode == EVectorVMOp::update_id ? 0 : 1;
				VVMPushRegUsage(IDIdxReg, VVM_RT_TEMPREG, RegIOFlag);
				VVMPushRegUsage(IDTagReg, VVM_RT_TEMPREG, RegIOFlag);
				OpPtrIn += 6;
			} break;
			default:												check(false);			     break;
		}

		continue;
	handle_op_ins:
		{
			uint8 ConstRegFlags = *OpPtrIn;
			VecIndices  = (uint16 *)(OpPtrIn + 1);
			OpPtrIn += 1 + ((Instruction->NumInputRegisters + Instruction->NumOutputRegisters) << 1);

			uint16 *RegUseBuff  = OptContext->Intermediate.RegisterUsageBuffer + OptContext->Intermediate.NumRegistersUsed;
			uint8  *RegTypeBuff = OptContext->Intermediate.RegisterUsageType   + OptContext->Intermediate.NumRegistersUsed;

			RegUseBuff[0]  = VecIndices[0];
			RegUseBuff[1]  = VecIndices[1];
			RegUseBuff[2]  = VecIndices[2];
			RegUseBuff[3]  = VecIndices[3];
			RegTypeBuff[0] = VVM_RT_TEMPREG;
			RegTypeBuff[1] = VVM_RT_TEMPREG;
			RegTypeBuff[2] = VVM_RT_TEMPREG;
			RegTypeBuff[3] = VVM_RT_TEMPREG;

			if (ConstRegFlags & 1)
			{
				RegUseBuff[0]  = VectorVMOptimizeRemapConst(OptContext, VecIndices[0]);
				RegTypeBuff[0] = VVM_RT_CONST;
			}
			if (Instruction->NumInputRegisters > 1 && (ConstRegFlags & 2))
			{
				RegUseBuff[1]  = VectorVMOptimizeRemapConst(OptContext, VecIndices[1]);
				RegTypeBuff[1] = VVM_RT_CONST;
			}
			if (Instruction->NumInputRegisters > 2 && (ConstRegFlags & 4))
			{
				RegUseBuff[2]  = VectorVMOptimizeRemapConst(OptContext, VecIndices[2]);
				RegTypeBuff[2] = VVM_RT_CONST;
			}

			//outputs are common
			RegUseBuff[Instruction->NumInputRegisters]  = VecIndices[Instruction->NumInputRegisters];
			RegTypeBuff[Instruction->NumInputRegisters] = VVM_RT_TEMPREG;
			OptContext->Intermediate.NumRegistersUsed += Instruction->NumInputRegisters + Instruction->NumOutputRegisters;
		}
	}

	//Step 2: Setup External Function Table
	if (NumExtFns > 0)
	{
		OptContext->ExtFnTable = (FVectorVMExtFunctionData *)OptContext->Init.ReallocFn(nullptr, sizeof(FVectorVMExtFunctionData) * NumExtFns, __FILE__, __LINE__);
		if (OptContext->ExtFnTable == nullptr)
		{
			return VectorVMOptimizerSetError(OptContext, VVMOptErr_OutOfMemory | VVMOptErr_ExternalFunction | VVMOptErr_Fatal);
		}
		for (int i = 0; i < NumExtFns; ++i)
		{
			OptContext->ExtFnTable[i].NumInputs  = ExtFnIOData[i].NumInputs;
			OptContext->ExtFnTable[i].NumOutputs = ExtFnIOData[i].NumOutputs;
		}
		OptContext->NumExtFns         = NumExtFns;
	}
	else
	{
		OptContext->ExtFnTable        = nullptr;
		OptContext->NumExtFns         = 0;
		OptContext->MaxExtFnRegisters = 0;
	}


	{ //Step 3: Verify everything is good
		{ //verify integirty of constant remap table
			if (OptContext->NumConstsRemapped > OptContext->NumConstsAlloced)
			{
				return VectorVMOptimizerSetError(OptContext, VVMOptErr_OutOfMemory | VVMOptErr_ConstRemap | VVMOptErr_Fatal);
			}
			if (OptContext->NumConstsRemapped < OptContext->NumConstsAlloced && OptContext->ConstRemap[1][OptContext->NumConstsRemapped] != 0xFFFF)
			{
				return VectorVMOptimizerSetError(OptContext, VVMOptErr_OutOfMemory | VVMOptErr_ConstRemap | VVMOptErr_Fatal);
			}
			for (int i = 0; i < OptContext->NumConstsRemapped; ++i)
			{
				if (OptContext->ConstRemap[1][i] >= OptContext->NumConstsAlloced)
				{
					return VectorVMOptimizerSetError(OptContext, VVMOptErr_OutOfMemory | VVMOptErr_ConstRemap | VVMOptErr_Fatal);
				}
				if (OptContext->ConstRemap[0][OptContext->ConstRemap[1][i]] != i)
				{
					return VectorVMOptimizerSetError(OptContext, VVMOptErr_OutOfMemory | VVMOptErr_ConstRemap | VVMOptErr_Fatal);
				}
			}
		}
		if (OptContext->Intermediate.NumRegistersUsed >= 0xFFFF) //16 bit indices
		{
			return VectorVMOptimizerSetError(OptContext, VVMOptErr_OutOfMemory | VVMOptErr_RegisterUsage | VVMOptErr_Fatal);
		}
	}

	{ //Step 4: Setup SSA register buffer
		OptContext->Intermediate.SSARegisterUsageBuffer = (uint16 *)OptContext->Init.ReallocFn(nullptr, sizeof(uint16) * NumRegisterUsageAlloced, __FILE__, __LINE__); //alloc as many are as allocated, not used because we can inject instructions later
		if (OptContext->Intermediate.SSARegisterUsageBuffer == nullptr)
		{
			return VectorVMOptimizerSetError(OptContext, VVMOptErr_OutOfMemory | VVMOptErr_InputMergeBuffer | VVMOptErr_Fatal);
		}
	}

	uint16 NumSSARegistersUsed = 0;
	bool Step6_InstructionsRemovedRun = false;

	{ //Step 5: SSA-like renaming of temp registers
	Step5_SSA:
		FMemory::Memcpy(OptContext->Intermediate.SSARegisterUsageBuffer, OptContext->Intermediate.RegisterUsageBuffer, sizeof(uint16) * OptContext->Intermediate.NumRegistersUsed);
	
		NumSSARegistersUsed = 0;
		uint16 SSARegCount = 0;
		for (uint32 OutputInsIdx = 0; OutputInsIdx < OptContext->Intermediate.NumInstructions; ++OutputInsIdx)
		{
			FVectorVMOptimizeInstruction *OutputIns = OptContext->Intermediate.Instructions + OutputInsIdx;
			//loop over each instruction's output
			for (int j = 0; j < OutputIns->NumOutputRegisters; ++j)
			{
				uint16 RegIdx  = OutputIns->RegPtrOffset + OutputIns->NumInputRegisters + j;
				uint16 OutReg  = OptContext->Intermediate.RegisterUsageBuffer[RegIdx];
				uint8  RegType = OptContext->Intermediate.RegisterUsageType[RegIdx];
				if (OutReg != 0xFFFF && RegType == VVM_RT_TEMPREG) {
					OptContext->Intermediate.SSARegisterUsageBuffer[OutputIns->RegPtrOffset + OutputIns->NumInputRegisters + j] = SSARegCount;
					int LastUsedAsInputInsIdx = -1;
					//check each instruction's output with the input of every instruction that follows it
					for (uint32 InputInsIdx = OutputInsIdx + 1; InputInsIdx < OptContext->Intermediate.NumInstructions; ++InputInsIdx)
					{
						FVectorVMOptimizeInstruction *InputIns = OptContext->Intermediate.Instructions + InputInsIdx;
						//check to see if the register we're currently looking at (OutReg) is overwritten by another instruction.  If it is,
						//we increment the SSA count, and move on to the next 
						for (int ii = 0; ii < InputIns->NumOutputRegisters; ++ii)
						{
							if (OptContext->Intermediate.RegisterUsageBuffer[InputIns->RegPtrOffset + InputIns->NumInputRegisters + ii] == OutReg)
							{
								//this register is overwritten, we need to generate a new register
								++SSARegCount;
								check(SSARegCount <= OptContext->Intermediate.NumRegistersUsed);
								goto DoneThisOutput;
							}
						}

						//if the Input instruction's input uses the Output instruction's output then assign them to the same SSA value
						for (int ii = 0; ii < InputIns->NumInputRegisters; ++ii)
						{
							if (OptContext->Intermediate.RegisterUsageBuffer[InputIns->RegPtrOffset + ii] == OutReg)
							{
								if (InputIns->OpCat == EVectorVMOpCategory::Output)
								{
									if (OutputIns->OpCode == EVectorVMOp::acquireindex)
									{
										if (j == ii)
										{
											check(j == 0);
											//if this assert hits it means that the output of the acquireindex is being written to a buffer...
											//on Dec 6, 2021 I asked about this in the #niagara-cpuvm-optimizations channel on slack and
											//at the time there was no way to hook up nodes to create this output... I was also informed
											//it was unlikly that this would ever be possible... so if this assert triggered either this feature
											//was added, or there's a bug.
											OptContext->Intermediate.SSARegisterUsageBuffer[InputIns->RegPtrOffset + j] = SSARegCount;
											LastUsedAsInputInsIdx = InputInsIdx;
										}
									}
									else
									{
										OptContext->Intermediate.SSARegisterUsageBuffer[InputIns->RegPtrOffset + 1] = SSARegCount;
										LastUsedAsInputInsIdx = InputInsIdx;
									}
								}
								else
								{
									LastUsedAsInputInsIdx = InputInsIdx;
									OptContext->Intermediate.SSARegisterUsageBuffer[InputIns->RegPtrOffset + ii] = SSARegCount;
								}
							}
						}
					}
					if (LastUsedAsInputInsIdx != -1)
					{
						++SSARegCount;
					}
					else
					{
						//this instruction will be removed later because its output isn't used.  Set the SSA to invalid to avoid messing up
						//dependency checks before the instruction is removed.
						OptContext->Intermediate.SSARegisterUsageBuffer[OutputIns->RegPtrOffset + OutputIns->NumInputRegisters + j] = 0xFFFF;
					}
				} else {
					OptContext->Intermediate.SSARegisterUsageBuffer[OutputIns->RegPtrOffset + OutputIns->NumInputRegisters + j] = 0xFFFF;
				}
				DoneThisOutput: ;
			}
		}
		if (SSARegCount >= 0xFFFF) {
			return VectorVMOptimizerSetError(OptContext, VVMOptErr_SSARemap | VVMOptErr_Overflow);
		}
		NumSSARegistersUsed = SSARegCount;
	}

	//Step 6: remove instructions where outputs are never used 
	if (!Step6_InstructionsRemovedRun) {
		int NumRemovedInstructions = 0;
		int NumRemovedInstructionsThisTime;
		int SanityCount = 0;
		do
		{
			//loop multiple times because sometimes an instruction can be removed that will make a previous instruction redundant as well
			NumRemovedInstructionsThisTime = 0;
			for (uint32 i = 0; i < OptContext->Intermediate.NumInstructions; ++i)
			{
				FVectorVMOptimizeInstruction *Ins = OptContext->Intermediate.Instructions + i;
				if (Ins->OpCat == EVectorVMOpCategory::Op)
				{
					for (int OutputIdx = 0; OutputIdx < Ins->NumOutputRegisters; ++OutputIdx)
					{
						uint16 RegIdx = OptContext->Intermediate.SSARegisterUsageBuffer[Ins->RegPtrOffset + Ins->NumInputRegisters + OutputIdx];
						for (uint32 j = i + 1; j < OptContext->Intermediate.NumInstructions; ++j)
						{
							FVectorVMOptimizeInstruction *Ins2 = OptContext->Intermediate.Instructions + j;
							for (int k = 0; k < Ins2->NumInputRegisters; ++k)
							{
								if (OptContext->Intermediate.RegisterUsageType[Ins2->RegPtrOffset + k] == VVM_RT_TEMPREG)
								{
									uint16 RegIdx2 = OptContext->Intermediate.SSARegisterUsageBuffer[Ins2->RegPtrOffset + k];
									if (RegIdx == RegIdx2)
									{
										goto InstructionRequired;
									}
								}
							}
						}
					}
					{ //instruction isn't required
						FMemory::Memmove(OptContext->Intermediate.Instructions + i, OptContext->Intermediate.Instructions + i + 1, sizeof(FVectorVMOptimizeInstruction) * (OptContext->Intermediate.NumInstructions - i - 1));
						++NumRemovedInstructionsThisTime;
						++NumRemovedInstructions;
						--OptContext->Intermediate.NumInstructions;
						i = 0;
					}
					InstructionRequired: ;
				}
			}
			if (++SanityCount >= 16384)
			{
				return VectorVMOptimizerSetError(OptContext, VVMOptErr_RedundantInstruction);
			}
		} while (NumRemovedInstructionsThisTime > 0);

		Step6_InstructionsRemovedRun = true;
		if (NumRemovedInstructions > 0)
		{
			//if any instructions were removed we need to re-compute the SSA, so go and do that.
			goto Step5_SSA;
		}
	}

	//Step 7: remove cast instructions and re-map the inputs of instructions that the casts feed into
	//to the inputs of the cast instructions.
	for (uint32 CastInsIdx = 0; CastInsIdx < OptContext->Intermediate.NumInstructions; ++CastInsIdx) {
		FVectorVMOptimizeInstruction *CastIns = OptContext->Intermediate.Instructions + CastInsIdx;
		if (CastIns->OpCode == EVectorVMOp::iasf || CastIns->OpCode == EVectorVMOp::fasi) {
			uint16 CastInputSSAReg   = OptContext->Intermediate.SSARegisterUsageBuffer[CastIns->RegPtrOffset + 0];
			uint16 CastOutputSSAReg  = OptContext->Intermediate.SSARegisterUsageBuffer[CastIns->RegPtrOffset + 1];
			uint16 CastInputReg      = OptContext->Intermediate.RegisterUsageBuffer[CastIns->RegPtrOffset + 0];
			uint16 CastOutputReg     = OptContext->Intermediate.RegisterUsageBuffer[CastIns->RegPtrOffset + 1];
			uint8  CastInputRegType  = OptContext->Intermediate.RegisterUsageType[CastIns->RegPtrOffset + 0];
			uint8  CastOutputRegType = OptContext->Intermediate.RegisterUsageType[CastIns->RegPtrOffset + 1];
			for (uint32 InsIdx = CastInsIdx + 1; InsIdx < OptContext->Intermediate.NumInstructions; ++InsIdx) {
				FVectorVMOptimizeInstruction *Ins = OptContext->Intermediate.Instructions + InsIdx;
				for (int i = 0; i < Ins->NumInputRegisters; ++i) {
					uint16 SSAReg  = OptContext->Intermediate.SSARegisterUsageBuffer[Ins->RegPtrOffset + i];
					uint16 Reg     = OptContext->Intermediate.RegisterUsageBuffer   [Ins->RegPtrOffset + i];
					uint8  RegType = OptContext->Intermediate.RegisterUsageType     [Ins->RegPtrOffset + i];
					if (SSAReg == CastOutputSSAReg && RegType == CastOutputRegType) {
						check(RegType == VVM_RT_TEMPREG);
						OptContext->Intermediate.SSARegisterUsageBuffer[Ins->RegPtrOffset + i] = CastInputSSAReg;
						OptContext->Intermediate.RegisterUsageBuffer   [Ins->RegPtrOffset + i] = CastInputReg;
						OptContext->Intermediate.RegisterUsageType     [Ins->RegPtrOffset + i] = CastInputRegType;
					}
				}
			}
			//remove this cast instruction
			FMemory::Memmove(OptContext->Intermediate.Instructions + CastInsIdx, OptContext->Intermediate.Instructions + CastInsIdx + 1, sizeof(FVectorVMOptimizeInstruction) * (OptContext->Intermediate.NumInstructions - CastInsIdx - 1));
			--OptContext->Intermediate.NumInstructions;
			--CastInsIdx;
		}
	}

	int OnePastLastInputIdx = -1;
	{ //Step 8: change temp registers that come directly from inputs to the input index
		for (uint32 i = 0; i < OptContext->Intermediate.NumInstructions; ++i)
		{
			FVectorVMOptimizeInstruction *InputIns = OptContext->Intermediate.Instructions + i;
			if (InputIns->OpCat != EVectorVMOpCategory::Input)
			{
				continue;
			}
			OnePastLastInputIdx = i + 1;
			uint16 InputSSARegIdx = OptContext->Intermediate.SSARegisterUsageBuffer[InputIns->RegPtrOffset];

			for (uint32 j = i + 1; j < OptContext->Intermediate.NumInstructions; ++j)
			{
				FVectorVMOptimizeInstruction *Ins = OptContext->Intermediate.Instructions + j;
				switch (Ins->OpCat) {
					case EVectorVMOpCategory::Input: //intentional fallthrough
					case EVectorVMOpCategory::Other: //intentional fallthrough
					case EVectorVMOpCategory::Stat:
						break;
					case EVectorVMOpCategory::Output:
						if (OptContext->Intermediate.RegisterUsageType[Ins->RegPtrOffset + 1]  == VVM_RT_TEMPREG && 
							OptContext->Intermediate.SSARegisterUsageBuffer[InputIns->RegPtrOffset] == OptContext->Intermediate.SSARegisterUsageBuffer[Ins->RegPtrOffset + 1])
						{
							check(InputIns->Index == i); //make sure the instruction is in its correct place.  Instructions could have moved, but this should have been corrected above.
							OptContext->Intermediate.SSARegisterUsageBuffer[Ins->RegPtrOffset + 1] = InputIns->Index;
							OptContext->Intermediate.RegisterUsageType[Ins->RegPtrOffset + 1]      = VVM_RT_INPUT;

							//deal with output instructions that read directly from inputs where there's a required half/float conversion
							if (InputIns->OpCode == EVectorVMOp::inputdata_half && Ins->OpCode == EVectorVMOp::outputdata_float)
							{ //input half, output 32 bit
								Ins->OpCode = EVectorVMOp::outputdata_float_from_half;
							}
							else if (InputIns->OpCode == EVectorVMOp::inputdata_half && Ins->OpCode == EVectorVMOp::outputdata_half)
							{
								Ins->OpCode = EVectorVMOp::outputdata_half_from_half;
							}
						}
					break;
					default:
					{
						for (int k = 0; k < Ins->NumInputRegisters; ++k) {
							uint16 RegPtrOffset = Ins->RegPtrOffset + k;
							uint16 SSAIdx       = OptContext->Intermediate.SSARegisterUsageBuffer[RegPtrOffset];
							if (SSAIdx == InputSSARegIdx && OptContext->Intermediate.RegisterUsageType[RegPtrOffset] == VVM_RT_TEMPREG)
							{
								if (InputIns->OpCode == EVectorVMOp::inputdata_half || InputIns->OpCode == EVectorVMOp::inputdata_noadvance_half) {
									//this instruction is coming directly from a half instruction, so inject a half_to_float instruction here, set the correct registers and SSA registers
									bool FoundExistingHalfToFloatInstruction = false;
									for (int ii = (int)j - 1; ii >= 0; --ii) {
										FVectorVMOptimizeInstruction *HalfToFloatIns = OptContext->Intermediate.Instructions + ii;
										if (HalfToFloatIns->OpCode == EVectorVMOp::half_to_float) {
											check(OptContext->Intermediate.RegisterUsageType[HalfToFloatIns->RegPtrOffset + 1] == VVM_RT_TEMPREG);
											uint16 HalfFloatOutputSSA = OptContext->Intermediate.SSARegisterUsageBuffer[HalfToFloatIns->RegPtrOffset + 1];
											if (SSAIdx == HalfFloatOutputSSA) {
												FoundExistingHalfToFloatInstruction = true;
												break;
											}
										}
									}
									if (!FoundExistingHalfToFloatInstruction)
									{
										FVectorVMOptimizeInstruction *Instruction = VVMPushNewInstruction(OptContext, EVectorVMOp::half_to_float);
										VVMEnsureRegAlloced(OptContext, true, &NumRegisterUsageAlloced, 8);

										if (Instruction == nullptr) {
											return VectorVMOptimizerSetError(OptContext, VVMOptErr_OutOfMemory | VVMOptErr_Instructions | VVMOptErr_Fatal);
										}
										Instruction->RegPtrOffset = OptContext->Intermediate.NumRegistersUsed;
										uint32 PrevNumRegistersAlloced = NumRegisterUsageAlloced;

										VVMPushRegUsage(InputIns->Index, VVM_RT_INPUT, 0);
										VVMPushRegUsage(OptContext->Intermediate.RegisterUsageBuffer[RegPtrOffset], VVM_RT_TEMPREG, 1);

										if (NumRegisterUsageAlloced > PrevNumRegistersAlloced) {
											OptContext->Intermediate.SSARegisterUsageBuffer = (uint16 *)OptContext->Init.ReallocFn(OptContext->Intermediate.SSARegisterUsageBuffer, sizeof(uint16) * NumRegisterUsageAlloced, __FILE__, __LINE__);
											if (OptContext->Intermediate.SSARegisterUsageBuffer == nullptr) {
												return VectorVMOptimizerSetError(OptContext, VVMOptErr_OutOfMemory | VVMOptErr_Instructions | VVMOptErr_Fatal);
											}
										}
										OptContext->Intermediate.SSARegisterUsageBuffer[Instruction->RegPtrOffset]     = InputIns->Index;
										OptContext->Intermediate.SSARegisterUsageBuffer[Instruction->RegPtrOffset + 1] = OptContext->Intermediate.SSARegisterUsageBuffer[RegPtrOffset];

										FVectorVMOptimizeInstruction TempIns = *Instruction;

										//move the instruction into the correct place
										FMemory::Memmove(Ins + 1, Ins, sizeof(FVectorVMOptimizeInstruction) * (OptContext->Intermediate.NumInstructions - j));
										*Ins = TempIns;
										++j;
									}
								} else {
									OptContext->Intermediate.SSARegisterUsageBuffer[RegPtrOffset] = InputIns->Index;
									OptContext->Intermediate.RegisterUsageType[RegPtrOffset]      = VVM_RT_INPUT;
								}
							}
						}
					} break;
				}
			}
		}
		if (OnePastLastInputIdx == -1) {
			OnePastLastInputIdx = 0;
		}
		while (OnePastLastInputIdx != -1 && OptContext->Intermediate.NumInstructions != 0 && (uint32)OnePastLastInputIdx < OptContext->Intermediate.NumInstructions - 1 && OptContext->Intermediate.Instructions[OnePastLastInputIdx].OpCat == EVectorVMOpCategory::Stat)
		{
			++OnePastLastInputIdx;
		}
	}

	{ //instruction re-ordering
		int NumParentInstructions = FMath::Max(NumSSARegistersUsed, (uint16)OptContext->Intermediate.NumRegistersUsed);
		int NumParentInstructions4Aligned = (int)(((uint32)NumParentInstructions + 3) & ~3);
		
		OptContext->Intermediate.ParentInstructionIdx = (uint16 *)OptContext->Init.ReallocFn(nullptr, sizeof(uint16) * (NumParentInstructions4Aligned + OptContext->Intermediate.NumRegistersUsed * 2 + 4), __FILE__, __LINE__);
		VVMSetParentInstructionIndices(OptContext);
		
		uint16 *InstructionIdxStack = OptContext->Intermediate.ParentInstructionIdx + NumParentInstructions4Aligned;

		int LowestInstructionIdxForAcquireIdx = OnePastLastInputIdx; //acquire index instructions will be sorted by whichever comes first in the IR... possibly worth checking if re-ordering is more efficient

		{ //Step 9: Find all the acquireindex instructions and re-order them to be executed ASAP
			int NumAcquireIndexInstructions = 0;
			for (uint32 i = 0; i < OptContext->Intermediate.NumInstructions; ++i)
			{
				FVectorVMOptimizeInstruction *Ins = OptContext->Intermediate.Instructions + i;
				if (Ins->OpCode == EVectorVMOp::acquireindex)
				{
					++NumAcquireIndexInstructions;
					int AcquireIndexInstructionIdx = i;
					int NumInstructions = VVMGetInstructionDependencyChain(OptContext, OptContext->Intermediate.Instructions + AcquireIndexInstructionIdx, InstructionIdxStack);
					
					//bubble up the dependent instructions at quickly as possible
					for (int j = 0; j < NumInstructions; ++j)
					{
						if (InstructionIdxStack[j] > LowestInstructionIdxForAcquireIdx)
						{
							FVectorVMOptimizeInstruction TempIns = OptContext->Intermediate.Instructions[InstructionIdxStack[j]];
							if (TempIns.OpCode == EVectorVMOp::external_func_call)
							{
								for (int k = LowestInstructionIdxForAcquireIdx; k < InstructionIdxStack[j]; ++k)
								{
									if (OptContext->Intermediate.Instructions[k].OpCode == EVectorVMOp::external_func_call) {
										LowestInstructionIdxForAcquireIdx = k + 1;
									}
								}
							}
							FMemory::Memmove(OptContext->Intermediate.Instructions + LowestInstructionIdxForAcquireIdx + 1, OptContext->Intermediate.Instructions + LowestInstructionIdxForAcquireIdx, sizeof(FVectorVMOptimizeInstruction) * (InstructionIdxStack[j] - LowestInstructionIdxForAcquireIdx));
							OptContext->Intermediate.Instructions[LowestInstructionIdxForAcquireIdx] = TempIns;
							VVMSetParentInstructionIndices(OptContext);
						}
						LowestInstructionIdxForAcquireIdx = LowestInstructionIdxForAcquireIdx + 1;
					}
					//move the acquire index instruction to immediately after the last instruction it depends on
					if (LowestInstructionIdxForAcquireIdx < AcquireIndexInstructionIdx)
					{
						FVectorVMOptimizeInstruction TempIns = OptContext->Intermediate.Instructions[AcquireIndexInstructionIdx];
						FMemory::Memmove(OptContext->Intermediate.Instructions + LowestInstructionIdxForAcquireIdx + 1, OptContext->Intermediate.Instructions + LowestInstructionIdxForAcquireIdx, sizeof(FVectorVMOptimizeInstruction) * (AcquireIndexInstructionIdx - LowestInstructionIdxForAcquireIdx));
						OptContext->Intermediate.Instructions[LowestInstructionIdxForAcquireIdx++] = TempIns;
						VVMSetParentInstructionIndices(OptContext);
					}
				}
			}
		}

		{ //Step 10: re-order the outputs to be done as early as possible: after the SSA's register's last usage
			for (uint32 OutputInsIdx = 0; OutputInsIdx < OptContext->Intermediate.NumInstructions; ++OutputInsIdx)
			{
				FVectorVMOptimizeInstruction *OutputIns = OptContext->Intermediate.Instructions + OutputInsIdx;
				if (OutputIns->OpCat == EVectorVMOpCategory::Output)
				{
					uint32 OutputInsertionIdx = 0xFFFFFFFF;
					uint16 IdxReg  = OptContext->Intermediate.SSARegisterUsageBuffer[OutputIns->RegPtrOffset];
					uint16 SrcReg  = OptContext->Intermediate.SSARegisterUsageBuffer[OutputIns->RegPtrOffset + 1];
					uint16 SrcType = OptContext->Intermediate.RegisterUsageType[OutputIns->RegPtrOffset + 1];

					int AcquireIdxIns = OptContext->Intermediate.ParentInstructionIdx[IdxReg];
					for (int i = (int)OutputInsIdx - 1; i >= AcquireIdxIns; --i) {
						FVectorVMOptimizeInstruction *Ins = OptContext->Intermediate.Instructions + i;
						for (int j = 0; j < Ins->NumInputRegisters + Ins->NumOutputRegisters; ++j)
						{
							if (OptContext->Intermediate.SSARegisterUsageBuffer[Ins->RegPtrOffset + j] == SrcReg)
							{
								OutputInsertionIdx = i + 1;
								goto done_finding_output_src_instruction;
							}
						}
					}
					done_finding_output_src_instruction:
					if (OutputInsertionIdx != 0xFFFFFFFF && OutputInsertionIdx < OptContext->Intermediate.NumInstructions - 1)
					{
						if (OutputInsIdx > OutputInsertionIdx)
						{
							uint32 NumInstructionsToMove = OutputInsIdx - OutputInsertionIdx;
							FVectorVMOptimizeInstruction TempIns = *OutputIns;
							FMemory::Memmove(OptContext->Intermediate.Instructions + OutputInsertionIdx + 1, OptContext->Intermediate.Instructions + OutputInsertionIdx, sizeof(FVectorVMOptimizeInstruction) * NumInstructionsToMove);
							OptContext->Intermediate.Instructions[OutputInsertionIdx] = TempIns;
							VVMSetParentInstructionIndices(OptContext);
						}
					}
				}
			}
		}

		{ //Step 11: re-order all dependent-less instructions to right before their output is used
			uint16 LastSwapInstructionIdx = 0xFFFF; //to prevent an infinite loop when one instruction has two or more dependencies and they keep swapping back and forth
			for (uint32 i = 0; i < OptContext->Intermediate.NumInstructions; ++i)
			{
				FVectorVMOptimizeInstruction *Ins = OptContext->Intermediate.Instructions + i;
				uint16 SkipInstructionSwap = LastSwapInstructionIdx;
				LastSwapInstructionIdx = 0xFFFF;
				if (Ins->OpCat == EVectorVMOpCategory::Op)
				{
					if (!VVMDoesInstructionHaveDependencies(OptContext, OptContext->Intermediate.Instructions + i))
					{

						for (uint32 j = i + 1; j < OptContext->Intermediate.NumInstructions; ++j)
						{
							if (VVMIsInstructionDependentOnInstruction(OptContext, OptContext->Intermediate.Instructions + j, InstructionIdxStack, i)) {
								if (j > i + 1 && j != SkipInstructionSwap)
								{
									check(i == OptContext->Intermediate.ParentInstructionIdx[OptContext->Intermediate.SSARegisterUsageBuffer[Ins->RegPtrOffset + Ins->NumInputRegisters]]);
									FVectorVMOptimizeInstruction TempIns = *Ins;
									FMemory::Memmove(Ins, Ins + 1, sizeof(FVectorVMOptimizeInstruction) * (j - i - 1));
									OptContext->Intermediate.Instructions[j - 1] = TempIns;
									LastSwapInstructionIdx = j;
									--i;
									VVMSetParentInstructionIndices(OptContext);
								}
								break;
							}
						}
					}
				}
			}
		}
	}

	{ //Step 12: group and sort all output instructions
		for (uint32 i = 0; i < OptContext->Intermediate.NumInstructions; ++i)
		{
			FVectorVMOptimizeInstruction *InsStart = OptContext->Intermediate.Instructions + i;
			if (InsStart->OpCat == EVectorVMOpCategory::Output)
			{
				for (uint32 j = i + 1; j < OptContext->Intermediate.NumInstructions; ++j)
				{
					FVectorVMOptimizeInstruction *InsEnd = OptContext->Intermediate.Instructions + j;
					if (InsEnd->OpCat != EVectorVMOpCategory::Output)
					{
						if (j - i > 1)
						{
							//these instruction indices are more than one apart so we can group them.
							int StartInstructionIdx = i;
							int LastInstructionIdx  = j - 1;
							int NumInstructions = j - i;
							FVectorVMOptimizeInstruction *StartInstruction = OptContext->Intermediate.Instructions + StartInstructionIdx;
							{ // small list, very stupid bubble sort
								bool sorted = false;
								while (!sorted) {
									sorted = true;
									for (int k = 0; k < NumInstructions - 1; ++k) {
										FVectorVMOptimizeInstruction *Ins0 = StartInstruction + k;
										FVectorVMOptimizeInstruction *Ins1 = Ins0 + 1;
										uint64 Key0 = VVMOutputInsGetSortKey(OptContext->Intermediate.SSARegisterUsageBuffer, Ins0);
										uint64 Key1 = VVMOutputInsGetSortKey(OptContext->Intermediate.SSARegisterUsageBuffer, Ins1);

										if (Key1 < Key0) {
											FVectorVMOptimizeInstruction Temp = *Ins0;
											*Ins0 = *Ins1;
											*Ins1 = Temp;
											sorted = false;
										}
									}
								}
							}
							i = j;
						}
						break;
					}
				}
			}
		}
	}
	
	bool ChangedMergedIns = false;

	{ //Step 13: figure out which instructions can be merged
		struct FMergableOp
		{
			uint32		InsIdx0;
			uint32		InsIdx1;
			int         Type;
		};
		TArray<FMergableOp> MergableOps;
		for (uint32 i = 0; i < OptContext->Intermediate.NumInstructions; ++i)
		{
			FVectorVMOptimizeInstruction *Ins = OptContext->Intermediate.Instructions + i;
			int InsMergeIdx = -1;
			if (Ins->OpCode == EVectorVMOp::exec_index)
			{
				//check to see if exec index can merge to specific ops
				uint16 ExecSSARegIdx = OptContext->Intermediate.SSARegisterUsageBuffer[Ins->RegPtrOffset];
				for (uint32 j = i + 1; j < OptContext->Intermediate.NumInstructions; ++j)
				{
					FVectorVMOptimizeInstruction *Ins2 = OptContext->Intermediate.Instructions + j;
					for (int k = 0; k < Ins2->NumInputRegisters; ++k)
					{
						// we're only looking for temporary registers
						if (OptContext->Intermediate.RegisterUsageType[Ins2->RegPtrOffset + k] != VVM_RT_TEMPREG)
						{
							continue;
						}
						uint16 InputSSAReg = OptContext->Intermediate.SSARegisterUsageBuffer[Ins2->RegPtrOffset + k];
						if (InputSSAReg == ExecSSARegIdx)
						{
							if (Ins2->OpCode == EVectorVMOp::i2f || Ins2->OpCode == EVectorVMOp::addi)
							{
								if (InsMergeIdx == -1)
								{
									InsMergeIdx = (int)j;
									break;
								}
								else
								{
									InsMergeIdx = -1;
									goto ExecIdxCannotMerge;
								}
							}
							else
							{
								InsMergeIdx = -1;
								goto ExecIdxCannotMerge;
							}
						}
					}
				}
				if (InsMergeIdx != -1)
				{
					FMergableOp MergableOp;
					MergableOp.InsIdx0 = i;
					MergableOp.InsIdx1 = InsMergeIdx;
					MergableOp.Type    = 0;
					MergableOps.Add(MergableOp);
				}
				ExecIdxCannotMerge: ;
			}
			else if (Ins->OpCat == EVectorVMOpCategory::Op)
			{
				{ //look for ops with the same inputs
					FVectorVMOptimizeInstruction *Ins2 = OptContext->Intermediate.Instructions + 1;
					if (Ins2->OpCat == EVectorVMOpCategory::Op)
					{
						if (Ins->NumInputRegisters == Ins2->NumInputRegisters)
						{
							for (int k = 0; k < Ins->NumInputRegisters; ++k)
							{
								if (OptContext->Intermediate.RegisterUsageType[Ins->RegPtrOffset + k] == VVM_RT_TEMPREG && OptContext->Intermediate.RegisterUsageType[Ins2->RegPtrOffset + k] == VVM_RT_TEMPREG) {
									if (OptContext->Intermediate.SSARegisterUsageBuffer[Ins->RegPtrOffset + k] != OptContext->Intermediate.SSARegisterUsageBuffer[Ins2->RegPtrOffset + k])
									{
										InsMergeIdx = -1;
										goto SameInputOpCannotMerge;
									}
								}
							}
							InsMergeIdx = i + 1;
							break;
						}
					}
				}
				if (InsMergeIdx != -1) //-V547
				{
					FMergableOp MergableOp;
					MergableOp.InsIdx0 = InsMergeIdx;
					MergableOp.InsIdx1 = i;
					MergableOp.Type    = -1;
					MergableOps.Add(MergableOp);
					continue;
				}
				SameInputOpCannotMerge:
				//loop for ops where the output of one is the input of another
				for (uint32 j = i + 1; j < OptContext->Intermediate.NumInstructions; ++j)
				{
					FVectorVMOptimizeInstruction *Ins2 = OptContext->Intermediate.Instructions + j;
					if (Ins2->OpCat == EVectorVMOpCategory::Op)
					{
						if (Ins2->NumInputRegisters < 4)
						{
							bool InputRegMatches[4] = { };
							int RegMatchCount = 0;
							for (int k = 0; k < Ins->NumOutputRegisters; ++k)
							{
								uint16 OutSSAReg = OptContext->Intermediate.SSARegisterUsageBuffer[Ins->RegPtrOffset + Ins->NumInputRegisters + k];
								if (OptContext->Intermediate.RegisterUsageType[Ins->RegPtrOffset + Ins->NumInputRegisters + k] == VVM_RT_TEMPREG)
								{
									for (int ii = 0; ii < Ins2->NumInputRegisters; ++ii)
									{
										if (OptContext->Intermediate.RegisterUsageType[Ins2->RegPtrOffset + ii] == VVM_RT_TEMPREG)
										{
											uint16 InSSAReg = OptContext->Intermediate.SSARegisterUsageBuffer[Ins2->RegPtrOffset + ii];
											if (OutSSAReg == InSSAReg)
											{
												InputRegMatches[ii] = true;
												if (RegMatchCount == 0)
												{
													RegMatchCount = (1 << ii);
												}
												else
												{
													//could be something like mul(a, a), and we can't merge
													//with the previous instruction since the two inputs
													//are the same and need to be computed.  This case is very
													//rare so there's no reason to create a specific instruction
													//to deal with it.
													goto CannotMergeInstructions;
												}
											}
										}
									}
								}
							}
							if (RegMatchCount > 0)
							{
								bool CanMergeInstructions = true;
								//first check to make sure the output of the first instruction isn't used elsewhere
								for (uint32 k = j + 1; k < OptContext->Intermediate.NumInstructions; ++k)
								{
									FVectorVMOptimizeInstruction *Ins3 = OptContext->Intermediate.Instructions + k;
									for (int ii = 0; ii < Ins->NumOutputRegisters; ++ii)
									{
										uint16 OutSSAReg = OptContext->Intermediate.SSARegisterUsageBuffer[Ins->RegPtrOffset + Ins->NumInputRegisters + ii];
										for (int jj = 0; jj < Ins3->NumInputRegisters; ++jj)
										{
											uint16 InSSAReg = OptContext->Intermediate.SSARegisterUsageBuffer[Ins3->RegPtrOffset + jj];
											uint16 InSSAType = OptContext->Intermediate.RegisterUsageType[Ins3->RegPtrOffset + jj];
											//these instruction may be technically mergable, but there's no reason to do it given
											//the output of the first instruction is used again.
											if (InSSAType == VVM_RT_TEMPREG && OutSSAReg == InSSAReg)
											{
												CanMergeInstructions = false;
												goto CannotMergeInstructions;
											}
										}
									}
								}
								if (j == i + 1)
								{
									//easy mode, the instructions follow each other so they can be merged
								}
								else
								{
									//if the instructions in between the two we're trying to merge require the first one as 
									//input, or if the second instruction requires one of the in-between instructions as 
									//input then we cannot merge these instructions
									for (uint32 k = i + 1; k < j; ++k)
									{
										FVectorVMOptimizeInstruction *Ins_InBetween = OptContext->Intermediate.Instructions + k;
										//check to see if the output from the first instruction is needed for this in-between instruction
										for (int oi = 0; oi < Ins_InBetween->NumOutputRegisters; ++oi)
										{
											uint16 OutSSAReg = OptContext->Intermediate.SSARegisterUsageBuffer[Ins->RegPtrOffset + Ins->NumInputRegisters + oi];
											for (int ii = 0; ii < Ins_InBetween->NumInputRegisters; ++ii)
											{
												uint16 InSSAReg = OptContext->Intermediate.SSARegisterUsageBuffer[Ins_InBetween->RegPtrOffset + ii];
												if (OutSSAReg == InSSAReg)
												{
													CanMergeInstructions = false;
													goto CannotMergeInstructions;
												}
											}
										}
									}
								}
								if (CanMergeInstructions)
								{
									FMergableOp MergableOp;
									MergableOp.InsIdx0 = i;
									MergableOp.InsIdx1 = j;
									MergableOp.Type    = RegMatchCount;
									MergableOps.Add(MergableOp);
								}
							}
							CannotMergeInstructions: ;
						}
					}
				}
			}
		}
		
		
		//Step 14: Merge all the statistically common pairs of instructions
#		define VVMDeclareRegs()					uint16* Ins0Regs = OptContext->Intermediate.RegisterUsageBuffer + Ins0->RegPtrOffset;                                 \
												uint16* Ins1Regs = OptContext->Intermediate.RegisterUsageBuffer + Ins1->RegPtrOffset;                                 \
												uint16* Ins0SSA = OptContext->Intermediate.SSARegisterUsageBuffer + Ins0->RegPtrOffset;                               \
												uint16* Ins1SSA = OptContext->Intermediate.SSARegisterUsageBuffer + Ins1->RegPtrOffset;                               \
												uint8* Ins0Type = OptContext->Intermediate.RegisterUsageType + Ins0->RegPtrOffset;                                    \
												uint8* Ins1Type = OptContext->Intermediate.RegisterUsageType + Ins1->RegPtrOffset;
#		define VVMCreateNewRegVars(NumNewRegs)	VVMEnsureRegAlloced(OptContext, true, &NumRegisterUsageAlloced, (NumNewRegs));                                        \
												VVMDeclareRegs();                                                                                                     \
												uint16 *NewRegs        = OptContext->Intermediate.RegisterUsageBuffer + OptContext->Intermediate.NumRegistersUsed;    \
												uint16 *NewSSA         = OptContext->Intermediate.SSARegisterUsageBuffer + OptContext->Intermediate.NumRegistersUsed; \
												uint8 *NewType         = OptContext->Intermediate.RegisterUsageType + OptContext->Intermediate.NumRegistersUsed;      \
												uint16 NewRegPtrOffset = OptContext->Intermediate.NumRegistersUsed;                                                   \
												OptContext->Intermediate.NumRegistersUsed += (NumNewRegs);
#		define VVMSetRegsFrom0(NewIdx, OrigIdx)  NewSSA[NewIdx] = Ins0SSA[OrigIdx];   \
												 NewRegs[NewIdx] = Ins0Regs[OrigIdx]; \
												 NewType[NewIdx] = Ins0Type[OrigIdx];
#		define VVMSetRegsFrom1(NewIdx, OrigIdx)  NewSSA[NewIdx] = Ins1SSA[OrigIdx];   \
												 NewRegs[NewIdx] = Ins1Regs[OrigIdx]; \
												 NewType[NewIdx] = Ins1Type[OrigIdx];
#		define VVMRegMatch(Ins0RegIdx, Ins1RegIdx) (Ins0SSA[Ins0RegIdx] == Ins1SSA[Ins1RegIdx] && Ins0Type[Ins0RegIdx] == Ins1Type[Ins1RegIdx])


		
#		define VVMSetMergedIns(NewIns, NumInputs_, NumOutputs_)	Ins0->InsMergedIdx       = MergableOps[i].InsIdx1;   \
																Ins1->NumInputRegisters  = NumInputs_;               \
																Ins1->NumOutputRegisters = NumOutputs_;              \
																Ins1->OpCode             = NewIns;                   \
																Ins1->RegPtrOffset       = NewRegPtrOffset;
		for (int i = 0; i < MergableOps.Num(); ++i)
		{
			FVectorVMOptimizeInstruction *Ins0 = OptContext->Intermediate.Instructions + MergableOps[i].InsIdx0;
			FVectorVMOptimizeInstruction *Ins1 = OptContext->Intermediate.Instructions + MergableOps[i].InsIdx1;
			if (Ins0->InsMergedIdx != -1 || Ins1->InsMergedIdx != -1)
			{
				continue; //instruction already merged
			}
			if (MergableOps[i].Type == 0)
			{
				if (Ins0->OpCode == EVectorVMOp::exec_index && Ins1->OpCode == EVectorVMOp::i2f)
				{
					VVMCreateNewRegVars(1);
					VVMSetRegsFrom1(0, 1);
					VVMSetMergedIns(EVectorVMOp::exec_indexf, 0, 1);
				}
				else if (Ins0->OpCode == EVectorVMOp::exec_index && Ins1->OpCode == EVectorVMOp::addi)
				{
					VVMCreateNewRegVars(2);
					if (!(VVMRegMatch(1, 0) && VVMRegMatch(1, 1)))
					{
						if (VVMRegMatch(0, 0))
						{
							VVMSetRegsFrom1(0, 1);
						}
						else
						{
							VVMSetRegsFrom1(0, 0);
						}
						VVMSetRegsFrom1(1, 2);
						VVMSetMergedIns(EVectorVMOp::exec_index_addi, 1, 1);
					}
				}
			}
			else if (MergableOps[i].Type == 1 && Ins1->OpCode == EVectorVMOp::select)
			{
				EVectorVMOp NewOpCode = EVectorVMOp::done;
				bool ReverseInputs    = false;
				switch (Ins0->OpCode)
				{
					case EVectorVMOp::cmplt:    NewOpCode = EVectorVMOp::cmplt_select;                             break;
					case EVectorVMOp::cmple:    NewOpCode = EVectorVMOp::cmple_select;                             break;
					case EVectorVMOp::cmpgt:    NewOpCode = EVectorVMOp::cmple_select;      ReverseInputs = true;  break;
					case EVectorVMOp::cmpge:    NewOpCode = EVectorVMOp::cmplt_select;      ReverseInputs = true;  break;
					case EVectorVMOp::cmpeq:    NewOpCode = EVectorVMOp::cmpeq_select;                             break;
					case EVectorVMOp::cmpneq:   NewOpCode = EVectorVMOp::cmpeq_select;      ReverseInputs = true;  break;
					case EVectorVMOp::cmplti:   NewOpCode = EVectorVMOp::cmplti_select;                            break;
					case EVectorVMOp::cmplei:   NewOpCode = EVectorVMOp::cmplei_select;                            break;
					case EVectorVMOp::cmpgti:   NewOpCode = EVectorVMOp::cmplei_select;     ReverseInputs = true;  break;
					case EVectorVMOp::cmpgei:   NewOpCode = EVectorVMOp::cmplti_select;     ReverseInputs = true;  break;
					case EVectorVMOp::cmpeqi:   NewOpCode = EVectorVMOp::cmpeqi_select;                            break;
					case EVectorVMOp::cmpneqi:  NewOpCode = EVectorVMOp::cmpeqi_select;     ReverseInputs = true;  break;
				}
				if (NewOpCode != EVectorVMOp::done)
				{
					VVMCreateNewRegVars(5);
					VVMSetRegsFrom0(0, 0);
					VVMSetRegsFrom0(1, 1);
					if (ReverseInputs)
					{
						VVMSetRegsFrom1(2, 2);
						VVMSetRegsFrom1(3, 1);
					}
					else
					{
						VVMSetRegsFrom1(2, 1);
						VVMSetRegsFrom1(3, 2);
					}
					VVMSetRegsFrom1(4, 3);
					VVMSetMergedIns(NewOpCode, 4, 1);
				}
			}
			else if (MergableOps[i].Type >= 1 && (Ins1->OpCode == EVectorVMOp::logic_and || Ins1->OpCode == EVectorVMOp::logic_or))
			{
				EVectorVMOp NewOpCode = EVectorVMOp::done;
				switch (Ins0->OpCode)
				{
					case EVectorVMOp::cmplt:    NewOpCode = (Ins1->OpCode == EVectorVMOp::logic_and ? EVectorVMOp::cmplt_logic_and  : EVectorVMOp::cmplt_logic_or );     break;
					case EVectorVMOp::cmple:    NewOpCode = (Ins1->OpCode == EVectorVMOp::logic_and ? EVectorVMOp::cmple_logic_and  : EVectorVMOp::cmple_logic_or );     break;
					case EVectorVMOp::cmpgt:    NewOpCode = (Ins1->OpCode == EVectorVMOp::logic_and ? EVectorVMOp::cmpgt_logic_and  : EVectorVMOp::cmpgt_logic_or );     break;
					case EVectorVMOp::cmpge:    NewOpCode = (Ins1->OpCode == EVectorVMOp::logic_and ? EVectorVMOp::cmpge_logic_and  : EVectorVMOp::cmpge_logic_or );     break;
					case EVectorVMOp::cmpeq:    NewOpCode = (Ins1->OpCode == EVectorVMOp::logic_and ? EVectorVMOp::cmpeq_logic_and  : EVectorVMOp::cmpeq_logic_or );     break;
					case EVectorVMOp::cmpneq:   NewOpCode = (Ins1->OpCode == EVectorVMOp::logic_and ? EVectorVMOp::cmpne_logic_and  : EVectorVMOp::cmpne_logic_or );     break;
					case EVectorVMOp::cmplti:   NewOpCode = (Ins1->OpCode == EVectorVMOp::logic_and ? EVectorVMOp::cmplti_logic_and : EVectorVMOp::cmplti_logic_or);     break;
					case EVectorVMOp::cmplei:   NewOpCode = (Ins1->OpCode == EVectorVMOp::logic_and ? EVectorVMOp::cmplei_logic_and : EVectorVMOp::cmplei_logic_or);     break;
					case EVectorVMOp::cmpgti:   NewOpCode = (Ins1->OpCode == EVectorVMOp::logic_and ? EVectorVMOp::cmpgti_logic_and : EVectorVMOp::cmpgti_logic_or);     break;
					case EVectorVMOp::cmpgei:   NewOpCode = (Ins1->OpCode == EVectorVMOp::logic_and ? EVectorVMOp::cmpgei_logic_and : EVectorVMOp::cmpgei_logic_or);     break;
					case EVectorVMOp::cmpeqi:   NewOpCode = (Ins1->OpCode == EVectorVMOp::logic_and ? EVectorVMOp::cmpeqi_logic_and : EVectorVMOp::cmpeqi_logic_or);     break;
					case EVectorVMOp::cmpneqi:  NewOpCode = (Ins1->OpCode == EVectorVMOp::logic_and ? EVectorVMOp::cmpnei_logic_and : EVectorVMOp::cmpnei_logic_or);     break;
				}
				if (NewOpCode != EVectorVMOp::done)
				{
					VVMCreateNewRegVars(4);
					VVMSetRegsFrom0(0, 0);
					VVMSetRegsFrom0(1, 1);
					if (VVMRegMatch(2, 0))
					{
						VVMSetRegsFrom1(2, 1);
					}
					else
					{
						VVMSetRegsFrom1(2, 0);
					}
					VVMSetRegsFrom1(3, 2);
					VVMSetMergedIns(NewOpCode, 3, 1);
				}
			}
			else if (MergableOps[i].Type >= 1 && Ins0->OpCode == EVectorVMOp::mad)
			{
				EVectorVMOp NewOpCode = EVectorVMOp::done;
				switch (Ins1->OpCode)
				{
					case EVectorVMOp::mad:
					{
						if (!RegDupeCheck(OptContext, Ins0, 3, Ins1, 0, 3))
						{
							VVMCreateNewRegVars(6);
							VVMSetRegsFrom0(0, 0);
							VVMSetRegsFrom0(1, 1);
							VVMSetRegsFrom0(2, 2);
							if (VVMRegMatch(3, 2))
							{
								//mad0 is the add in mad1
								NewOpCode = EVectorVMOp::mad_mad0;
								VVMSetRegsFrom1(3, 0);
								VVMSetRegsFrom1(4, 1);
							}
							else
							{
								//mad0 is a mul in mad1
								NewOpCode = EVectorVMOp::mad_mad1;
								if (VVMRegMatch(3, 0))
								{
									VVMSetRegsFrom1(3, 1);
								}
								else
								{
									VVMSetRegsFrom1(3, 0);
								}
								VVMSetRegsFrom1(4, 2);
							}
							VVMSetRegsFrom1(5, 3);
							if (NewOpCode != EVectorVMOp::done) //-V547
							{
								VVMSetMergedIns(NewOpCode, 5, 1);
							}
						}
					}
					break;
					case EVectorVMOp::add:
						NewOpCode = EVectorVMOp::mad_add;
						goto vvm_mad_add_sub_mul;
					case EVectorVMOp::mul:
						NewOpCode = EVectorVMOp::mad_mul;
						goto vvm_mad_add_sub_mul;
					case EVectorVMOp::sub:
						NewOpCode = EVectorVMOp::mad_sub0;
						vvm_mad_add_sub_mul:
						{
							VVMCreateNewRegVars(5);
							VVMSetRegsFrom0(0, 0);
							VVMSetRegsFrom0(1, 1);
							VVMSetRegsFrom0(2, 2);
							if (VVMRegMatch(3, 0))
							{
								VVMSetRegsFrom1(3, 1);
							}
							else
							{
								if (NewOpCode == EVectorVMOp::mad_sub0)
								{
									NewOpCode = EVectorVMOp::mad_sub1;
								}
								VVMSetRegsFrom1(3, 0);
							}
							VVMSetRegsFrom1(4, 2);	
							VVMSetMergedIns(NewOpCode, 4, 1);
						}
					break;
					case EVectorVMOp::sqrt:
					{
						VVMCreateNewRegVars(4);
						VVMSetRegsFrom0(0, 0);
						VVMSetRegsFrom0(1, 1);
						VVMSetRegsFrom0(2, 2);
						VVMSetRegsFrom1(3, 1);
						VVMSetMergedIns(EVectorVMOp::mad_sqrt, 3, 1);
					}
					break;
				}
			}
			else if (Ins0->OpCode == EVectorVMOp::mul)
			{
				if (MergableOps[i].Type >= 1)
				{
					EVectorVMOp NewOpCode = EVectorVMOp::done;
					switch (Ins1->OpCode)
					{
						case EVectorVMOp::mad:
						{
							VVMCreateNewRegVars(5);
							VVMSetRegsFrom0(0, 0);
							VVMSetRegsFrom0(1, 1);
							if (VVMRegMatch(2, 0))
							{
								NewOpCode = EVectorVMOp::mul_mad0;
								VVMSetRegsFrom1(2, 1);
								VVMSetRegsFrom1(3, 2);
							}
							else if (VVMRegMatch(2, 1))
							{
								NewOpCode = EVectorVMOp::mul_mad0;
								VVMSetRegsFrom1(2, 0);
								VVMSetRegsFrom1(3, 2);
							}
							else if (VVMRegMatch(2, 2))
							{
								NewOpCode = EVectorVMOp::mul_mad1;
								VVMSetRegsFrom1(2, 0);
								VVMSetRegsFrom1(3, 1);
							}
							VVMSetRegsFrom1(4, 3);
							if (NewOpCode != EVectorVMOp::done)
							{
								VVMSetMergedIns(NewOpCode, 4, 1);
							}
						} break;
						case EVectorVMOp::add:
							NewOpCode = EVectorVMOp::mul_add;
							goto vvm_mul_add_sub_mul_max;
						case EVectorVMOp::sub:
							NewOpCode = EVectorVMOp::mul_sub0;
							goto vvm_mul_add_sub_mul_max;
						case EVectorVMOp::mul:
							NewOpCode = EVectorVMOp::mul_mul;
							goto vvm_mul_add_sub_mul_max;
						case EVectorVMOp::max:
							NewOpCode = EVectorVMOp::mul_max;
							vvm_mul_add_sub_mul_max:
							if (NewOpCode != EVectorVMOp::done && !RegDupeCheck(OptContext, Ins0, 2, Ins1, 0, 1))
							{
								VVMCreateNewRegVars(4);
								VVMSetRegsFrom0(0, 0);
								VVMSetRegsFrom0(1, 1);
								if (VVMRegMatch(2, 0))
								{
									VVMSetRegsFrom1(2, 1);
								}
								else
								{
									VVMSetRegsFrom1(2, 0);
									if (NewOpCode == EVectorVMOp::mul_sub0)
									{
										NewOpCode = EVectorVMOp::mul_sub1;
									}
								}
								VVMSetRegsFrom1(3, 2);
								VVMSetMergedIns(NewOpCode, 3, 1);
							}
							break;
					}
				}
				else if (MergableOps[i].Type == -1 && Ins1->OpCode == EVectorVMOp::mul)
				{
					//two muls using the same inputs, output
					VVMCreateNewRegVars(4);
					VVMSetRegsFrom0(0, 0);
					VVMSetRegsFrom0(1, 1);
					VVMSetRegsFrom0(2, 2);
					VVMSetRegsFrom1(3, 2);
					VVMSetMergedIns(EVectorVMOp::mul_2x, 2, 2);
				}
			}
			else if (MergableOps[i].Type >= 1 && Ins0->OpCode == EVectorVMOp::add)
			{
				if (Ins1->OpCode == EVectorVMOp::mad)
				{
					//we only have a merged op if the output from the add is the input to the add op, if the op of ins0 is the mul operand from ins1, it's not statistically relevant in Fortnite so there's no instruction for it
					{
						VVMDeclareRegs();
						if (!VVMRegMatch(2 /*Output of the add*/, 2 /*add operand*/))
						{
							break;
						}
					}
					if (!RegDupeCheck(OptContext, Ins0, 2, Ins1, 0, 2))
					{
						VVMCreateNewRegVars(5);
						VVMSetRegsFrom0(0, 0);
						VVMSetRegsFrom0(1, 1);
						VVMSetRegsFrom1(2, 0);
						VVMSetRegsFrom1(3, 1);
						VVMSetRegsFrom1(4, 3);
						VVMSetMergedIns(EVectorVMOp::add_mad1, 4, 1);
					}
				}
				else if (Ins1->OpCode == EVectorVMOp::add)
				{
					VVMCreateNewRegVars(4);
					VVMSetRegsFrom0(0, 0);
					VVMSetRegsFrom0(1, 1);
					if (VVMRegMatch(2, 0))
					{
						VVMSetRegsFrom1(2, 1);
					}
					else
					{
						VVMSetRegsFrom1(2, 0);
					}
					VVMSetRegsFrom1(3, 2);
					VVMSetMergedIns(EVectorVMOp::add_add, 3, 1);
				}
			}
			else if (MergableOps[i].Type >= 1 && Ins0->OpCode == EVectorVMOp::sub)
			{
				if (Ins1->OpCode == EVectorVMOp::cmplt && MergableOps[i].Type == 2)
				{
					VVMCreateNewRegVars(4);
					VVMSetRegsFrom0(0, 0);
					VVMSetRegsFrom0(1, 1);
					VVMSetRegsFrom1(2, 0);
					VVMSetRegsFrom1(3, 2);
					VVMSetMergedIns(EVectorVMOp::sub_cmplt1, 3, 1);
				}
				else if (Ins1->OpCode == EVectorVMOp::neg)
				{
					VVMCreateNewRegVars(3);
					VVMSetRegsFrom0(0, 0);
					VVMSetRegsFrom0(1, 1);
					VVMSetRegsFrom1(2, 1);
					VVMSetMergedIns(EVectorVMOp::sub_neg, 2, 1);
				}
				else if (Ins1->OpCode == EVectorVMOp::mul)
				{
					VVMCreateNewRegVars(4);
					VVMSetRegsFrom0(0, 0);
					VVMSetRegsFrom0(1, 1);
					if (VVMRegMatch(2, 0))
					{
						VVMSetRegsFrom1(2, 1);
					}
					else
					{
						VVMSetRegsFrom1(2, 0);
					}
					VVMSetRegsFrom1(3, 2);
					VVMSetMergedIns(EVectorVMOp::sub_mul, 3, 1);
				}
			} else if (Ins0->OpCode == EVectorVMOp::div && MergableOps[i].Type >= 1) {
				switch (Ins1->OpCode) {
					case EVectorVMOp::mad:
					{
						// we only support merging div/mad when the result of the div is one of the operands to the mul portion of the mad
						{
							VVMDeclareRegs();
							if (!(VVMRegMatch(2 /*Output of the div*/, 0 /*mul operand 0*/) || VVMRegMatch(2 /*Output of the div*/, 1 /*mul operand 1*/)))
							{
								break;
							}
						}
						VVMCreateNewRegVars(5);
						VVMSetRegsFrom0(0, 0);
						VVMSetRegsFrom0(1, 1);
						if (VVMRegMatch(2, 0))
						{
							VVMSetRegsFrom1(2, 1);
						}
						else
						{
							VVMSetRegsFrom1(2, 0);
						}
						VVMSetRegsFrom1(3, 2);
						VVMSetRegsFrom1(4, 3);
						VVMSetMergedIns(EVectorVMOp::div_mad0, 4, 1);
					}
					break;
					case EVectorVMOp::f2i:
					{
						VVMCreateNewRegVars(3);
						VVMSetRegsFrom0(0, 0);
						VVMSetRegsFrom0(1, 1);
						VVMSetRegsFrom1(2, 1);
						VVMSetMergedIns(EVectorVMOp::div_f2i, 2, 1);
					}
					break;
					case EVectorVMOp::mul:
					{
						VVMCreateNewRegVars(4);
						VVMSetRegsFrom0(0, 0);
						VVMSetRegsFrom0(1, 1);
						if (VVMRegMatch(2, 0))
						{
							VVMSetRegsFrom1(2, 1);
						}
						else
						{
							VVMSetRegsFrom1(2, 0);
						}
						VVMSetRegsFrom1(3, 2);
						VVMSetMergedIns(EVectorVMOp::div_mul, 3, 1);
					}
					break;
					default: break;
				}
			}
			else if (Ins0->OpCode == EVectorVMOp::muli && Ins1->OpCode == EVectorVMOp::addi)
			{
				VVMCreateNewRegVars(4);
				VVMSetRegsFrom0(0, 0);
				VVMSetRegsFrom0(1, 1);
				if (VVMRegMatch(2, 0))
				{
					VVMSetRegsFrom1(2, 1);
				}
				else
				{
					VVMSetRegsFrom1(2, 0);
				}
				VVMSetRegsFrom1(3, 2);
				VVMSetMergedIns(EVectorVMOp::muli_addi, 3, 1);
			}
			else if (Ins0->OpCode == EVectorVMOp::addi && (Ins1->OpCode == EVectorVMOp::bit_rshift || Ins1->OpCode == EVectorVMOp::muli))
			{
				EVectorVMOp NewOpCode = Ins1->OpCode == EVectorVMOp::bit_rshift ? EVectorVMOp::addi_bit_rshift : EVectorVMOp::addi_muli;
				VVMCreateNewRegVars(4);
				VVMSetRegsFrom0(0, 0);
				VVMSetRegsFrom0(1, 1);
				if (VVMRegMatch(2, 0))
				{
					VVMSetRegsFrom1(2, 1);
				}
				else
				{
					VVMSetRegsFrom1(2, 0);
				}
				VVMSetRegsFrom1(3, 2);
				VVMSetMergedIns(NewOpCode, 3, 1);
			}
			else if (Ins0->OpCode == EVectorVMOp::b2i && Ins1->OpCode == EVectorVMOp::b2i && MergableOps[i].Type == -1)
			{
				VVMCreateNewRegVars(3);
				VVMSetRegsFrom0(0, 0);
				VVMSetRegsFrom0(1, 1);
				VVMSetRegsFrom1(2, 1);
				VVMSetMergedIns(EVectorVMOp::b2i_2x, 1, 2);
			}
			else if (Ins0->OpCode == EVectorVMOp::i2f && MergableOps[i].Type >= 1)
			{
				if (Ins1->OpCode == EVectorVMOp::div)
				{
					EVectorVMOp NewOpCode;
					VVMCreateNewRegVars(3);
					VVMSetRegsFrom0(0, 0);
					if (VVMRegMatch(1, 0))
					{
						VVMSetRegsFrom1(1, 1);
						NewOpCode = EVectorVMOp::i2f_div0;
					}
					else
					{
						VVMSetRegsFrom1(1, 0);
						NewOpCode = EVectorVMOp::i2f_div1;
					}
					VVMSetRegsFrom1(2, 2);
					VVMSetMergedIns(NewOpCode, 2, 1);
				}
				else if (Ins1->OpCode == EVectorVMOp::mul)
				{
					VVMCreateNewRegVars(3);
					VVMSetRegsFrom0(0, 0);
					if (VVMRegMatch(1, 0))
					{
						VVMSetRegsFrom1(1, 1);
					}
					else
					{
						VVMSetRegsFrom1(1, 0);
					}
					VVMSetRegsFrom1(2, 2);
					VVMSetMergedIns(EVectorVMOp::i2f_mul, 2, 1);
				}
				else if (Ins1->OpCode == EVectorVMOp::mad)
				{
					EVectorVMOp NewOpCode = EVectorVMOp::i2f_mad0;
					VVMCreateNewRegVars(4);
					if (VVMRegMatch(1, 0))
					{
						VVMSetRegsFrom0(0, 0);
						VVMSetRegsFrom1(1, 1);
						VVMSetRegsFrom1(2, 2);
					}
					else if (VVMRegMatch(1, 1))
					{
						VVMSetRegsFrom0(0, 0);
						VVMSetRegsFrom1(1, 0);
						VVMSetRegsFrom1(2, 2);
					}
					else
					{
						NewOpCode = EVectorVMOp::i2f_mad1;
						VVMSetRegsFrom1(0, 0);
						VVMSetRegsFrom1(1, 1);
						VVMSetRegsFrom0(2, 0);
					}
					VVMSetRegsFrom1(3, 3);
					VVMSetMergedIns(NewOpCode, 3, 1);
				}
			}
			else if (Ins0->OpCode == EVectorVMOp::f2i && MergableOps[i].Type >= 1)
			{
				if (Ins1->OpCode == EVectorVMOp::select && MergableOps[i].Type == 2)
				{
					VVMCreateNewRegVars(4);
					VVMSetRegsFrom1(0, 0); //mask
					VVMSetRegsFrom0(1, 0);
					VVMSetRegsFrom1(2, 2);
					VVMSetRegsFrom1(3, 3);
					VVMSetMergedIns(EVectorVMOp::f2i_select1, 3, 1);
				}
				else if (Ins1->OpCode == EVectorVMOp::maxi)
				{
					VVMCreateNewRegVars(3);
					VVMSetRegsFrom0(0, 0);
					if (VVMRegMatch(1, 0))
					{
						VVMSetRegsFrom1(1, 1);
					}
					else
					{
						VVMSetRegsFrom1(1, 0);
					}
					VVMSetRegsFrom1(2, 2);
					VVMSetMergedIns(EVectorVMOp::f2i_maxi, 2, 1);
				}
				else if (Ins1->OpCode == EVectorVMOp::addi)
				{
					VVMCreateNewRegVars(3);
					VVMSetRegsFrom0(0, 0);
					if (VVMRegMatch(1, 0))
					{
						VVMSetRegsFrom1(1, 1);
					}
					else
					{
						VVMSetRegsFrom1(1, 0);
					}
					VVMSetRegsFrom1(2, 2);
					VVMSetMergedIns(EVectorVMOp::f2i_addi, 2, 1);
				}
			}
			else if (Ins0->OpCode == EVectorVMOp::bit_and && Ins1->OpCode == EVectorVMOp::i2f)
			{
				VVMCreateNewRegVars(3);
				VVMSetRegsFrom0(0, 0);
				VVMSetRegsFrom0(1, 1);
				VVMSetRegsFrom1(2, 1);
				VVMSetMergedIns(EVectorVMOp::bit_and_i2f, 2, 1);
			}
			else if (Ins0->OpCode == EVectorVMOp::random && Ins1->OpCode == EVectorVMOp::random && MergableOps[i].Type == -1)
			{
				VVMCreateNewRegVars(3);
				VVMSetRegsFrom0(0, 0);
				VVMSetRegsFrom0(1, 1);
				VVMSetRegsFrom1(2, 1);
				VVMSetMergedIns(EVectorVMOp::random_2x, 1, 2);
			}
			else if (Ins0->OpCode == EVectorVMOp::select && Ins1->OpCode == EVectorVMOp::mul)
			{
				VVMCreateNewRegVars(5);
				VVMSetRegsFrom0(0, 0);
				VVMSetRegsFrom0(1, 1);
				VVMSetRegsFrom0(2, 2);
				
				if (VVMRegMatch(3, 0))
				{
					VVMSetRegsFrom1(3, 1);
				}
				else
				{
					VVMSetRegsFrom1(3, 0);
				}
				VVMSetRegsFrom1(4, 2);
				VVMSetMergedIns(EVectorVMOp::select_mul, 4, 1);
			}
			else if (Ins0->OpCode == EVectorVMOp::select && Ins1->OpCode == EVectorVMOp::add)
			{
				VVMCreateNewRegVars(5);
				VVMSetRegsFrom0(0, 0);
				VVMSetRegsFrom0(1, 1);
				VVMSetRegsFrom0(2, 2);
				if (VVMRegMatch(3, 0))
				{
					VVMSetRegsFrom1(3, 1);
				}
				else
				{
					VVMSetRegsFrom1(3, 0);
				}
				VVMSetRegsFrom1(4, 2);
				VVMSetMergedIns(EVectorVMOp::select_add, 4, 1);
			}
			else if (((Ins0->OpCode == EVectorVMOp::cos && Ins1->OpCode == EVectorVMOp::sin) || (Ins0->OpCode == EVectorVMOp::sin && Ins1->OpCode == EVectorVMOp::cos)) && MergableOps[i].Type == -1)
			{
				VVMCreateNewRegVars(3);
				VVMSetRegsFrom0(0, 0);
				if (Ins0->OpCode == EVectorVMOp::sin)
				{
					VVMSetRegsFrom0(1, 1);
					VVMSetRegsFrom1(2, 1);
				}
				else
				{
					VVMSetRegsFrom1(1, 1);
					VVMSetRegsFrom0(2, 1);
				}
				VVMSetMergedIns(EVectorVMOp::sin_cos, 1, 2);
			}
			else if (Ins0->OpCode == EVectorVMOp::neg && Ins1->OpCode == EVectorVMOp::cmplt && MergableOps[i].Type >= 1)
			{
				VVMCreateNewRegVars(3);
				VVMSetRegsFrom0(0, 0);
				if (MergableOps[i].Type == 1)
				{
					VVMSetRegsFrom1(1, 1);
				}
				else
				{
					VVMSetRegsFrom1(1, 0);
				}
				VVMSetRegsFrom1(2, 2);
				VVMSetMergedIns(EVectorVMOp::neg_cmplt, 2, 1);
			}
			else if (Ins0->OpCode == EVectorVMOp::random && Ins1->OpCode == EVectorVMOp::add && MergableOps[i].Type >= 1)
			{
				VVMCreateNewRegVars(3);
				VVMSetRegsFrom0(0, 0);
				if (MergableOps[i].Type == 1)
				{
					VVMSetRegsFrom1(1, 1);
				}
				else
				{
					VVMSetRegsFrom1(1, 0);
				}
				VVMSetRegsFrom1(2, 2);
				VVMSetMergedIns(EVectorVMOp::random_add, 2, 1);
			}
			else if (Ins0->OpCode == EVectorVMOp::max && Ins1->OpCode == EVectorVMOp::f2i && MergableOps[i].Type == 1)
			{
				VVMCreateNewRegVars(3);
				VVMSetRegsFrom0(0, 0);
				VVMSetRegsFrom0(1, 1);
				VVMSetRegsFrom1(2, 1);
				VVMSetMergedIns(EVectorVMOp::max_f2i, 2, 1);
			}
			else
			{
				//the common 2 instruction 3 op merges
				EVectorVMOp NewOpCode = EVectorVMOp::done;
				if (Ins0->OpCode == EVectorVMOp::bit_rshift && Ins1->OpCode == EVectorVMOp::bit_and)
				{
					NewOpCode = EVectorVMOp::bit_rshift_bit_and;
				}
				else if (Ins0->OpCode == EVectorVMOp::fmod && Ins1->OpCode == EVectorVMOp::add)
				{
					NewOpCode = EVectorVMOp::fmod_add;
				}
				else if (Ins0->OpCode == EVectorVMOp::bit_or && Ins1->OpCode == EVectorVMOp::muli)
				{
					NewOpCode = EVectorVMOp::bit_or_muli;
				}
				else if (Ins0->OpCode == EVectorVMOp::bit_lshift && Ins1->OpCode == EVectorVMOp::bit_or)
				{
					NewOpCode = EVectorVMOp::bit_lshift_bit_or;
				}
				if (NewOpCode != EVectorVMOp::done)
				{
					VVMCreateNewRegVars(4);
					VVMSetRegsFrom0(0, 0);
					VVMSetRegsFrom0(1, 1);
					if (VVMRegMatch(2, 0))
					{
						VVMSetRegsFrom1(2, 1);
					}
					else
					{
						VVMSetRegsFrom1(2, 0);
					}
					VVMSetRegsFrom1(3, 2);
					VVMSetMergedIns(NewOpCode, 3, 1);
				}
			}
		}	

#		undef VVMDeclareRegs
#		undef VVMCreateNewRegVars
#		undef VVMSetRegsFrom0
#		undef VVMSetRegsFrom1
#		undef VVMRegMatch
#		undef VVMSetMergedIns
	}
	
	VVMIOReg *InputRegs    = nullptr;
	VVMIOReg *OutputRegs   = nullptr;
	int NumInputRegisters  = 0;
	int NumOutputRegisters = 0;

	{ //Step 15: generate input remap table and output remaps
		int NumInputRegistersAlloced  = 256;
		int NumOutputRegistersAlloced = 256;

		InputRegs  = (VVMIOReg *)OptContext->Init.ReallocFn(NULL, sizeof(VVMIOReg) * NumInputRegistersAlloced , __FILE__, __LINE__);
		OutputRegs = (VVMIOReg *)OptContext->Init.ReallocFn(NULL, sizeof(VVMIOReg) * NumOutputRegistersAlloced, __FILE__, __LINE__);
		//Gather up all the input and output registers used
		int MaxInputIdx  = 0;
		int MaxOutputIdx = 0;
		for (uint32 i = 0; i < OptContext->Intermediate.NumInstructions; ++i)
		{
			FVectorVMOptimizeInstruction *Ins = OptContext->Intermediate.Instructions + i;
			if (Ins->OpCat == EVectorVMOpCategory::Input)
			{
				int FoundInputIdx = -1;
				for (int j = 0; j < NumInputRegisters; ++j) {
					if (Ins->OpCode           == InputRegs[j].InputIns->OpCode           &&
						Ins->Input.DataSetIdx == InputRegs[j].InputIns->Input.DataSetIdx && 
						Ins->Input.InputIdx   == InputRegs[j].InputIns->Input.InputIdx) {
						FoundInputIdx = j;
						break;
					}
				}
				if (FoundInputIdx == -1) {
					if (NumInputRegisters >= NumInputRegistersAlloced) {
						NumInputRegistersAlloced <<= 1;
						InputRegs = (VVMIOReg *)OptContext->Init.ReallocFn(InputRegs, sizeof(VVMIOReg) * NumInputRegistersAlloced, __FILE__, __LINE__);
					}
					VVMIOReg *Reg = InputRegs + NumInputRegisters;
					Reg->InputIns   = Ins;
					++NumInputRegisters;

					if (Ins->Index > MaxInputIdx) {
						MaxInputIdx = Ins->Index;
					}
				}
			}
			else if (Ins->OpCat == EVectorVMOpCategory::Output)
			{
				int FoundOutputIdx = -1;
				for (int j = 0; j < NumOutputRegisters; ++j) {
					if (Ins->OpCode            == OutputRegs[j].OutputIns->OpCode           &&
						Ins->Output.DataSetIdx == OutputRegs[j].OutputIns->Output.DataSetIdx && 
						Ins->Output.DstRegIdx  == OutputRegs[j].OutputIns->Output.DstRegIdx) {
						FoundOutputIdx = j;
						break;
					}
				}
				if (FoundOutputIdx == -1) {
					if (NumOutputRegisters >= NumOutputRegistersAlloced) {
						NumOutputRegistersAlloced <<= 1;
						OutputRegs = (VVMIOReg *)OptContext->Init.ReallocFn(OutputRegs, sizeof(VVMIOReg) * NumOutputRegistersAlloced, __FILE__, __LINE__);
					}
					VVMIOReg *Reg = OutputRegs + NumOutputRegisters;
					Reg->OutputIns = Ins;
					++NumOutputRegisters;
					if (Ins->Index > MaxOutputIdx) {
						MaxOutputIdx = Ins->Index;
					}
				}
			}
		}

		if (NumInputRegisters > 0) {
			qsort(InputRegs, NumInputRegisters, sizeof(VVMIOReg), VVMIOReg::SortInputReg_Fn);
			int MaxInputDataSet = 0;
			for (int i = 0; i < NumInputRegisters; ++i) {
				if (InputRegs[i].InputIns->Input.DataSetIdx > MaxInputDataSet) {
					MaxInputDataSet = InputRegs[i].InputIns->Input.DataSetIdx;
				}
			}
			check(MaxInputDataSet < 0xFFFE);

			uint16 *SSAInputMap             = (uint16 *)OptContext->Init.ReallocFn(NULL, sizeof(uint16) * (MaxInputIdx + 1), __FILE__, __LINE__);	
			OptContext->NumInputDataSets    = (uint16)MaxInputDataSet + 1;
			OptContext->InputDataSetOffsets = (uint16 *)OptContext->Init.ReallocFn(NULL, sizeof(uint16) * OptContext->NumInputDataSets * 8, __FILE__, __LINE__);
			//indices in OptContext->InputDataSetOffsets are:
			//DataSetIdx * 8 + 0: float
			//DataSetIdx * 8 + 1: int
			//DataSetIdx * 8 + 2: half
			//DataSetIdx * 8 + 3: no advance float
			//DataSetIdx * 8 + 4: no advance int
			//DataSetIdx * 8 + 5: no advance half
			//DataSetIdx * 8 + 6: no advance half count
			//DataSetIdx * 8 + 7: total no advance inputs
			//to get the number of input mapping for a given DataSetIndex and instruction type (EVectorVMOp::inputdata*), it's:
			//	OptContext->InputDataSetOffsets[DataSetIndex * 8 + InstructionType + 1] - OptContext->InputDataSetOffsets[DataSetIndex * 8 + InstructionType]

			EVectorVMOp PrevInputOpcode = EVectorVMOp::inputdata_float;
			int PrevInputDataSet        = 0;
			OptContext->InputDataSetOffsets[0] = 0;
			for (uint16 i = 0; i < NumInputRegisters; ++i)
			{
				FVectorVMOptimizeInstruction *InputIns = InputRegs[i].InputIns;
				check(InputIns->Index <= MaxInputIdx);
				SSAInputMap[InputIns->Index] = i;
				if (InputIns->Input.DataSetIdx != PrevInputDataSet)
				{
					//fill out the missing offsets when there's no inputs for a particular type on the previous DataSet
					for (int j = 0; j <= (int)EVectorVMOp::inputdata_noadvance_half - (int)PrevInputOpcode; ++j)
					{
						OptContext->InputDataSetOffsets[PrevInputDataSet * 8 + j + (int)PrevInputOpcode - (int)EVectorVMOp::inputdata_float + 1] = i;
					}
					//fill out any in-between DataSets that are empty
					for (int j = PrevInputDataSet + 1; j < InputIns->Input.DataSetIdx; ++j)
					{
						for (int k = 0; k < 8; ++k)
						{
							OptContext->InputDataSetOffsets[j * 8 + k] = i;
						}
					}
					//fill out the missing offsets when there's no inputs for a type in this DataSet
					for (int j = 0; j <= (int)InputIns->OpCode - (int)EVectorVMOp::inputdata_float; ++j)
					{
						OptContext->InputDataSetOffsets[InputIns->Input.DataSetIdx * 8 + j] = i;
					}
					PrevInputOpcode  = InputIns->OpCode;
					PrevInputDataSet = InputIns->Input.DataSetIdx;
				}
				else if (InputIns->OpCode != PrevInputOpcode)
				{
					for (int j = (int)PrevInputOpcode + 1; j <= (int)InputIns->OpCode; ++j)
					{
						OptContext->InputDataSetOffsets[InputIns->Input.DataSetIdx * 8 + j - (int)EVectorVMOp::inputdata_float] = i;
					}
					PrevInputOpcode  = InputIns->OpCode;
					PrevInputDataSet = InputIns->Input.DataSetIdx;
				}
			}
			//fill out the missing data in the last DataSet
			for (int j = 0; j <= (int)EVectorVMOp::inputdata_noadvance_half - (int)PrevInputOpcode; ++j)
			{
				OptContext->InputDataSetOffsets[PrevInputDataSet * 8 + j + (int)PrevInputOpcode - (int)EVectorVMOp::inputdata_float + 1] = NumInputRegisters;
			}
			for (int i = 0; i < OptContext->NumInputDataSets; ++i)
			{
				//fill out the noadvance counts for each dataset
				OptContext->InputDataSetOffsets[i * 8 + 7] = OptContext->InputDataSetOffsets[i * 8 + 6] - OptContext->InputDataSetOffsets[i * 8 + 3];
			}

			//generate the input remap table
			OptContext->InputRemapTable = (uint16 *)OptContext->Init.ReallocFn(NULL, sizeof(uint16) * NumInputRegisters, __FILE__, __LINE__);
			for (uint16 i = 0; i < NumInputRegisters; ++i)
			{
				OptContext->InputRemapTable[i] = InputRegs[i].InputIns->Input.InputIdx;
			}
			OptContext->NumInputsRemapped = NumInputRegisters;

			//fix up the Register Usage buffer's input registers to reference the new remapped indices
			for (uint32 i = 0; i < OptContext->Intermediate.NumRegistersUsed; ++i)
			{
				if (OptContext->Intermediate.RegisterUsageType[i] == VVM_RT_INPUT)
				{
					OptContext->Intermediate.RegisterUsageBuffer[i] = SSAInputMap[OptContext->Intermediate.SSARegisterUsageBuffer[i]];
				}
			}
			OptContext->Init.FreeFn(SSAInputMap, __FILE__, __LINE__);
		}
		else
		{
			OptContext->NumInputDataSets          = 0;
			OptContext->InputDataSetOffsets       = NULL;
			OptContext->NumInputsRemapped         = 0;
		}

		//generate output remaps
		if (NumOutputRegisters > 0) {
			int LargestSerialIdx = 0;

			EVectorVMOp PrevOutputOpcode = EVectorVMOp::outputdata_float;
			int PrevOutputDataSet        = 0;
			for (int i = 0; i < NumOutputRegisters; ++i)
			{
				FVectorVMOptimizeInstruction *OutputIns = OutputRegs[i].OutputIns;
				if (OutputIns->Output.SerialIdx > LargestSerialIdx) {
					LargestSerialIdx = OutputIns->Output.SerialIdx;
				}
				check(OutputIns->Index <= MaxOutputIdx);
				//SSAOutputMap[OutputIns->Index] = i;
				if (OutputIns->Output.DataSetIdx != PrevOutputDataSet)
				{
					PrevOutputOpcode  = OutputIns->OpCode;
					PrevOutputDataSet = OutputIns->Output.DataSetIdx;
				}
				else if (OutputIns->OpCode != PrevOutputOpcode)
				{
					PrevOutputOpcode  = OutputIns->OpCode;
					PrevOutputDataSet = OutputIns->Output.DataSetIdx;
				}
			}

			//generate the output remap table
			OptContext->OutputRemapDataSetIdx = (uint8  *)OptContext->Init.ReallocFn(NULL, sizeof(uint8)  * (LargestSerialIdx + 1), __FILE__, __LINE__);
			OptContext->OutputRemapDataType   = (uint16 *)OptContext->Init.ReallocFn(NULL, sizeof(uint16) * (LargestSerialIdx + 1), __FILE__, __LINE__);
			OptContext->OutputRemapDst        = (uint16 *)OptContext->Init.ReallocFn(NULL, sizeof(uint16) * (LargestSerialIdx + 1), __FILE__, __LINE__);
			FMemory::Memset(OptContext->OutputRemapDst, 0xFF, sizeof(uint16) * (LargestSerialIdx + 1));
			for (uint16 i = 0; i < NumOutputRegisters; ++i)
			{
				uint16 SerialIdx = OutputRegs[i].OutputIns->Output.SerialIdx;
				check(OutputRegs[i].OutputIns->Output.DataSetIdx < 0xFF); //storing 1 byte only
				OptContext->OutputRemapDataSetIdx[SerialIdx] = (uint8)OutputRegs[i].OutputIns->Output.DataSetIdx;
				switch (OutputRegs[i].OutputIns->OpCode)
				{
					case EVectorVMOp::outputdata_float:
					case EVectorVMOp::outputdata_float_from_half:
						OptContext->OutputRemapDataType[SerialIdx] = 0;
					break;

					case EVectorVMOp::outputdata_int32:
						OptContext->OutputRemapDataType[SerialIdx] = 1;
					break;

					case EVectorVMOp::outputdata_half:
					case EVectorVMOp::outputdata_half_from_half:
						OptContext->OutputRemapDataType[SerialIdx] = 2;
					break;

					default:
					break;
				}
				OptContext->OutputRemapDst[SerialIdx]        = OutputRegs[i].OutputIns->Output.DstRegIdx;
			}			

			OptContext->NumOutputsRemapped    = LargestSerialIdx + 1;
			OptContext->NumOutputInstructions = NumOutputRegisters;
		}
		else
		{
			OptContext->OutputRemapDataSetIdx   = NULL;
			OptContext->OutputRemapDataType     = NULL;
			OptContext->OutputRemapDst          = NULL;
			OptContext->NumOutputsRemapped      = 0;
			OptContext->NumOutputInstructions   = 0;
		}
	}
	
	{ //Step 16: use the SSA registers to compute the minimized registers required and write them back into the register usage buffer		
		uint16 *SSAUseMap2    = (uint16 *)OptContext->Init.ReallocFn(nullptr, sizeof(uint16) * NumSSARegistersUsed * 2, __FILE__, __LINE__);
		if (SSAUseMap2 == nullptr)
		{
			return VectorVMOptimizerSetError(OptContext, VVMOptErr_OutOfMemory | VVMOptErr_SSARemap | VVMOptErr_Fatal);
		}
		FMemory::Memset(SSAUseMap2, 0xFF, sizeof(uint16) * NumSSARegistersUsed * 2);
		uint16 *SSAUseMap2Inv = SSAUseMap2 + NumSSARegistersUsed;

		int NumSSARegistersUsed2 = 0;
		int MaxLiveRegisters2 = 0;
		for (int i = (int)OptContext->Intermediate.NumInstructions - 1; i >= 0; --i)
		{
			FVectorVMOptimizeInstruction *Ins = OptContext->Intermediate.Instructions + i;
			if (Ins->OpCat == EVectorVMOpCategory::Input || Ins->InsMergedIdx != -1 || (Ins->NumInputRegisters == 0 && Ins->NumOutputRegisters == 0))
			{
				continue;
			}
			for (int j = 0; j < Ins->NumOutputRegisters; ++j) {
				uint16 RegIdx = Ins->RegPtrOffset + Ins->NumInputRegisters + j;
				uint16 SSAReg = OptContext->Intermediate.SSARegisterUsageBuffer[RegIdx];
				uint8 RegType = OptContext->Intermediate.RegisterUsageType[RegIdx];
				if (RegType == VVM_RT_TEMPREG)
				{
					if (SSAReg != 0xFFFF && SSAUseMap2Inv[SSAReg] != 0xFFFF)
					{
						OptContext->Intermediate.RegisterUsageBuffer[RegIdx] = SSAUseMap2Inv[SSAReg];
						--NumSSARegistersUsed2;
						SSAUseMap2[SSAUseMap2Inv[SSAReg]] = 0xFFFF;
						SSAUseMap2Inv[SSAReg] = 0xFFFF;
					}
					else
					{
						check(Ins->OpCode == EVectorVMOp::external_func_call);
						OptContext->Intermediate.RegisterUsageBuffer[RegIdx] = 0xFFFF;
					}
				}
			}

			for (int j = 0; j < Ins->NumInputRegisters; ++j) {
				uint16 RegIdx = Ins->RegPtrOffset + j;
				uint16 SSAReg = OptContext->Intermediate.SSARegisterUsageBuffer[RegIdx];
				uint8 RegType = OptContext->Intermediate.RegisterUsageType[RegIdx];
				if (RegType == VVM_RT_TEMPREG)
				{
					if (SSAUseMap2Inv[SSAReg] == 0xFFFF)
					{
						for (uint16 k = 0; k < NumSSARegistersUsed; ++k)
						{
							if (SSAUseMap2[k] == 0xFFFF) {
								OptContext->Intermediate.RegisterUsageBuffer[RegIdx] = k;
								SSAUseMap2[k] = SSAReg;
								SSAUseMap2Inv[SSAReg] = k;
								++NumSSARegistersUsed2;
								break;
							}
						}
					}
					else
					{
						OptContext->Intermediate.RegisterUsageBuffer[RegIdx] = SSAUseMap2Inv[SSAReg];
					}
				}
			}
			
			
			if (NumSSARegistersUsed2 > MaxLiveRegisters2) {
				MaxLiveRegisters2 = NumSSARegistersUsed2;
			}
		}

		int MaxLiveRegisters = MaxLiveRegisters2;
		check(MaxLiveRegisters == MaxLiveRegisters2);
		OptContext->NumTempRegisters                 = (uint32)MaxLiveRegisters;
		OptContext->Init.FreeFn(SSAUseMap2, __FILE__, __LINE__);
	}
	
	//Step 17 alias temp registers to output registers 
	int NumRemappedOutputRegs   = FMath::Max(OptContext->NumOutputsRemapped, (uint16)OptContext->NumTempRegisters);
	uint16 *FinalTempRegRemap = (uint16 *)OptContext->Init.ReallocFn(nullptr, sizeof(uint16) * NumRemappedOutputRegs, __FILE__, __LINE__);
	for (int i = 0; i < NumRemappedOutputRegs; ++i)
	{
		FinalTempRegRemap[i] = i;
	}


	if (OptContext->NumOutputsRemapped > 0)
	{
		uint8 * KeepTempReg         = (uint8  *)OptContext->Init.ReallocFn(nullptr, sizeof(uint8)  * OptContext->NumTempRegisters, __FILE__, __LINE__);
		uint16 *AvailableOutputs    = (uint16 *)OptContext->Init.ReallocFn(nullptr, sizeof(uint16) * NumRemappedOutputRegs       , __FILE__, __LINE__);
		int NumAvailableOutputs     = 0;
		FMemory::Memset(KeepTempReg , 0   , sizeof(uint8) * OptContext->NumTempRegisters);

		for (int i = (int)OptContext->Intermediate.NumInstructions - 1; i >= 0; --i)
		{
			FVectorVMOptimizeInstruction *Ins = OptContext->Intermediate.Instructions + i;
			if (Ins->InsMergedIdx != -1)
			{
				continue; //this instruction is merged with another and is no longer needed
			}
			if (Ins->OpCode == EVectorVMOp::outputdata_float || Ins->OpCode == EVectorVMOp::outputdata_int32)
			{
				uint16 RegIdx  = Ins->RegPtrOffset + 1;
				uint16 Reg     = OptContext->Intermediate.RegisterUsageBuffer[RegIdx];
				uint16 RegType = OptContext->Intermediate.RegisterUsageType[RegIdx];
				if (RegType == VVM_RT_TEMPREG)
				{
					check(Reg < OptContext->NumTempRegisters);
					if (Reg != 0xFFFF && !KeepTempReg[Reg])
					{
						OptContext->Intermediate.RegisterUsageType[RegIdx]   = VVM_RT_OUTPUT;
						OptContext->Intermediate.RegisterUsageBuffer[RegIdx] = Ins->Output.SerialIdx;
						for (int ii = i - 1; ii >= 0; --ii)
						{
							FVectorVMOptimizeInstruction *Ins2 = OptContext->Intermediate.Instructions + ii;
							for (int j = 0; j < Ins2->NumInputRegisters + Ins2->NumOutputRegisters; ++j)
							{
								uint16 RegIdx2  = Ins2->RegPtrOffset + j;
								uint16 Reg2     = OptContext->Intermediate.RegisterUsageBuffer[RegIdx2];
								uint16 RegType2 = OptContext->Intermediate.RegisterUsageType[RegIdx2];
								if (Reg2 == Reg && RegType2 == RegType)
								{
									OptContext->Intermediate.RegisterUsageBuffer[RegIdx2] = Ins->Output.SerialIdx;
									OptContext->Intermediate.RegisterUsageType[RegIdx2]   = VVM_RT_OUTPUT;
								}
							}
						}
					}
				}
				else
				{
					bool OutputAlreadyInAvailableList = false;
					for (int k = 0; k < NumAvailableOutputs; ++k)
					{
						if (AvailableOutputs[k] == Ins->Output.SerialIdx)
						{
							OutputAlreadyInAvailableList = true;
							break;
						}
					}
					if (!OutputAlreadyInAvailableList)
					{
						check(NumAvailableOutputs < OptContext->NumOutputsRemapped);
						AvailableOutputs[NumAvailableOutputs++] = Ins->Output.SerialIdx;
					}
				}
			}
			else if (Ins->OpCat != EVectorVMOpCategory::Input)
			{
				int NumRegs = Ins->NumInputRegisters + Ins->NumOutputRegisters;
				if (Ins->OpCode == EVectorVMOp::acquireindex)
				{
					--NumRegs; //ignore the output register for acquireindex... it's not used anymore, but we need to keep track of it for dependecy purposes in the instruction re-ordering step
				}
				for (int j = 0; j < NumRegs; ++j)
				{
					uint16 RegIdx  = Ins->RegPtrOffset + j;
					uint16 Reg     = OptContext->Intermediate.RegisterUsageBuffer[RegIdx];
					uint16 RegType = OptContext->Intermediate.RegisterUsageType[RegIdx];
					if (Reg != 0xFFFF && RegType == VVM_RT_TEMPREG && !KeepTempReg[Reg])
					{
						if (NumAvailableOutputs > 0)
						{
							uint16 OutputIdx = AvailableOutputs[--NumAvailableOutputs];
							for (int ii = i; ii >= 0; --ii)
							{
								FVectorVMOptimizeInstruction *Ins2 = OptContext->Intermediate.Instructions + ii;
								for (int jj = 0; jj < Ins2->NumInputRegisters + Ins2->NumOutputRegisters; ++jj)
								{
									uint16 RegIdx2  = Ins2->RegPtrOffset + jj;
									uint16 Reg2     = OptContext->Intermediate.RegisterUsageBuffer[RegIdx2];
									uint16 RegType2 = OptContext->Intermediate.RegisterUsageType[RegIdx2];
									if (Reg2 == Reg && RegType2 == RegType)
									{
										OptContext->Intermediate.RegisterUsageBuffer[RegIdx2] = OutputIdx;
										OptContext->Intermediate.RegisterUsageType[RegIdx2]   = VVM_RT_OUTPUT;
									}
								}
							}
						}
						else
						{
							KeepTempReg[Reg] = true;
						}
					}
				}
			}
		}

		int NumTempRegsAfterOutputAlias = 0;
		for (uint32 i = 0; i < OptContext->NumTempRegisters; ++i) {
			if (KeepTempReg[i]) {
				FinalTempRegRemap[i] = NumTempRegsAfterOutputAlias++;
			}
		}
		OptContext->NumTempRegisters = NumTempRegsAfterOutputAlias;		
		OptContext->Init.FreeFn(KeepTempReg, __FILE__, __LINE__);
		OptContext->Init.FreeFn(AvailableOutputs, __FILE__, __LINE__);
	}
		
	{ //Step 18: write the final optimized bytecode
		//this goes over the instruction list twice.  The first time to figure out how many bytes are required for the bytecode, the second to write the bytecode.
		uint8 *OptimizedBytecode = nullptr;		
		int NumOptimizedBytesRequired = 0;
		int NumOptimizedBytesWritten = 0;
		
#		define VVMOptWriteOpCode(Opcode)	if (OptimizedBytecode) {                                                \
													check(NumOptimizedBytesWritten <= NumOptimizedBytesRequired);   \
													OptimizedBytecode[NumOptimizedBytesWritten++] = (uint8)Opcode;  \
												} else {                                                            \
													++NumOptimizedBytesRequired;                                    \
												}
#		define VVMOptWriteIdx(Idx)			if (OptimizedBytecode) {                                                    \
												OptimizedBytecode[NumOptimizedBytesWritten++] = (uint8)((Idx) & 0xFF);  \
												OptimizedBytecode[NumOptimizedBytesWritten++] = (uint8)((Idx) >> 8);    \
												} else { NumOptimizedBytesRequired += 2; }

#		define VVMOptWriteByte(b)			if (OptimizedBytecode) {                                         \
												OptimizedBytecode[NumOptimizedBytesWritten++] = (uint8)(b);  \
											} else { ++NumOptimizedBytesRequired; }

#		define VVMOptWriteReg(Idx)														                      \
		if (OptimizedBytecode) {														                      \
			VVMOptWriteReg_(OptContext, Idx, FinalTempRegRemap, OptimizedBytecode, NumOptimizedBytesWritten); \
			NumOptimizedBytesWritten += 2;                                                                    \
		} else {                                                                                              \
			NumOptimizedBytesRequired += 2;									                                  \
		} 

		WriteOptimizedBytecode:
		for (uint32 i = 0; i < OptContext->Intermediate.NumInstructions; ++i)
		{
			FVectorVMOptimizeInstruction *Ins = OptContext->Intermediate.Instructions + i;
			if (Ins->InsMergedIdx != -1) {
				continue; //this instruction is merged with another and is no longer needed
			}
			if (OptimizedBytecode)
			{
				Ins->PtrOffsetInOptimizedBytecode = (uint32)NumOptimizedBytesWritten;
				if (RandInstructionSet.Contains(Ins->OpCode))
				{
					OptContext->Flags |= VVMFlag_HasRandInstruction;
				}
			}
			switch (Ins->OpCat)
			{
				case EVectorVMOpCategory::Input:
					Ins->PtrOffsetInOptimizedBytecode = -1;
					break;
				case EVectorVMOpCategory::Output:
					{
						int NumOutputInstructionsToWrite = 1;
						//figure out how we can batch these
						for (uint32 j = i + 1; j < OptContext->Intermediate.NumInstructions; ++j)
						{
							FVectorVMOptimizeInstruction *NextIns = OptContext->Intermediate.Instructions + j;
							if (NextIns->OpCode == Ins->OpCode                       &&
								NextIns->Output.DataSetIdx == Ins->Output.DataSetIdx &&
								OptContext->Intermediate.SSARegisterUsageBuffer[NextIns->RegPtrOffset] == OptContext->Intermediate.SSARegisterUsageBuffer[Ins->RegPtrOffset])
							{
								++NumOutputInstructionsToWrite;
								if (NumOutputInstructionsToWrite >= 0xFF)
								{
									break; //we only write 1 byte so we can't group anymore
								}
							}
							else
							{
								break;
							}
						}
						VVMOptWriteOpCode(Ins->OpCode);
						VVMOptWriteByte(NumOutputInstructionsToWrite);       //0: Num Output Loops
						VVMOptWriteByte(Ins->Output.DataSetIdx);             //1: DataSet index
						for (int j = 0; j < NumOutputInstructionsToWrite; ++j)
						{
							VVMOptWriteReg(Ins[j].RegPtrOffset + 1);  //3; Input  Src
						}
						for (int j = 0; j < NumOutputInstructionsToWrite; ++j)
						{
							VVMOptWriteIdx(OptContext->NumTempRegisters + OptContext->NumConstsRemapped + OptContext->NumInputsRemapped * 2 + Ins[j].Output.SerialIdx);         //4: Remapped Output Register Idx
						}
						i += NumOutputInstructionsToWrite - 1;
					}
				break;
				case EVectorVMOpCategory::Op:
					VVMOptWriteOpCode(Ins->OpCode);
					for (int j = 0; j < Ins->NumInputRegisters + Ins->NumOutputRegisters; ++j) {
						VVMOptWriteReg(Ins->RegPtrOffset + j);
					}
					break;
				case EVectorVMOpCategory::IndexGen:
					VVMOptWriteOpCode(Ins->OpCode);
					VVMOptWriteReg(Ins->RegPtrOffset + 0);          //0: Input Register
					VVMOptWriteByte(Ins->IndexGen.DataSetIdx);      //1: DataSetIdx
					break;
				case EVectorVMOpCategory::ExtFnCall: {
					uint32 NumDummyRegs = 0;
					VVMOptWriteOpCode(Ins->OpCode);
					VVMOptWriteIdx(Ins->ExtFnCall.ExtFnIdx);
					for (int j = 0; j < ExtFnIOData[Ins->ExtFnCall.ExtFnIdx].NumInputs + ExtFnIOData[Ins->ExtFnCall.ExtFnIdx].NumOutputs; ++j)
					{
						if (OptContext->Intermediate.RegisterUsageBuffer[Ins->RegPtrOffset + j] == 0xFFFF) {
							++NumDummyRegs;
						}
						VVMOptWriteReg(Ins->RegPtrOffset + j);
					}
					if (NumDummyRegs > OptContext->NumDummyRegsReq) {
						OptContext->NumDummyRegsReq = NumDummyRegs;
					}
				} break;
				case EVectorVMOpCategory::Stat:
					break;
				case EVectorVMOpCategory::RWBuffer:
					check(Ins->OpCode == EVectorVMOp::update_id || Ins->OpCode == EVectorVMOp::acquire_id);
					VVMOptWriteOpCode(Ins->OpCode);
					VVMOptWriteReg(Ins->RegPtrOffset + 0);
					VVMOptWriteReg(Ins->RegPtrOffset + 1);
					VVMOptWriteByte(Ins->RWBuffer.DataSetIdx);
					break;
				case EVectorVMOpCategory::Other:
					if (Ins->OpCode == EVectorVMOp::done) {
						//write 8 bytes because the exec loop can read upto 6 bytes ahead.
						VVMOptWriteOpCode(Ins->OpCode);
						VVMOptWriteOpCode(Ins->OpCode);
						VVMOptWriteOpCode(Ins->OpCode);
						VVMOptWriteOpCode(Ins->OpCode);
						VVMOptWriteOpCode(Ins->OpCode);
						VVMOptWriteOpCode(Ins->OpCode);
						VVMOptWriteOpCode(Ins->OpCode);
						VVMOptWriteOpCode(Ins->OpCode);
						goto GotDoneInsWhileWritingByecode;
					}
					break;

			}
		}
		GotDoneInsWhileWritingByecode:
		if (OptimizedBytecode == nullptr)
		{
			check(NumOptimizedBytesWritten == 0);
			if (NumOptimizedBytesRequired > 0)
			{
				++NumOptimizedBytesRequired;
				OptimizedBytecode = (uint8 *)OptContext->Init.ReallocFn(nullptr, NumOptimizedBytesRequired, __FILE__, __LINE__);
				if (OptimizedBytecode == nullptr)
				{
					return VectorVMOptimizerSetError(OptContext, VVMOptErr_OutOfMemory | VVMOptErr_OptimizedBytecode | VVMOptErr_Fatal);
				}
				goto WriteOptimizedBytecode;
			}
		} else {
			OptimizedBytecode[NumOptimizedBytesWritten++] = 0;
		}
		check(NumOptimizedBytesWritten == NumOptimizedBytesRequired);
		OptContext->OutputBytecode   = OptimizedBytecode;
		OptContext->NumBytecodeBytes = NumOptimizedBytesWritten;
	}
	
	OptContext->Init.FreeFn(InputRegs, __FILE__, __LINE__);
	OptContext->Init.FreeFn(OutputRegs, __FILE__, __LINE__);
	OptContext->Init.FreeFn(FinalTempRegRemap, __FILE__, __LINE__);

	if (!(Flags & VVMFlag_OptSaveIntermediateState))
	{
		VectorVMFreeOptimizerIntermediateData(OptContext);
	}

#undef VVMOptimizeVecIns3
#undef VVMOptimizeVecIns2
#undef VVMOptimizeVecIns1
#undef VVMPushRegUsage
#undef VectorVMOptimizerSetError
	return 0;
}


#undef VectorVMOptimizerSetError

void FreezeVectorVMOptimizeContext(const FVectorVMOptimizeContext& Context, TArray<uint8>& ContextData)
{
	VectorVMSerializationHelper::FreezeContext(Context, ContextData);
}

#endif // WITH_EDITORONLY_DATA

void ReinterpretVectorVMOptimizeContextData(TConstArrayView<uint8> ContextData, FVectorVMOptimizeContext& Context)
{
	VectorVMSerializationHelper::ThawContext(ContextData, Context);
}

#endif //VECTORVM_SUPPORTS_EXPERIMENTAL
