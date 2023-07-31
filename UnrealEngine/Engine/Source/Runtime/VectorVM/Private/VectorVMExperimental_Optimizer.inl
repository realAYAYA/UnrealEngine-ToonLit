// Copyright Epic Games, Inc. All Rights Reserved.

/*
	OptimizeVectorVMScript() takes the original VM's bytecode as input and outputs a new bytecode,
a ConstRemapTable, an External Function Table and computes the number of TempRegs and ConstBuffs 
required.  This function is not particularly efficient, but I don't think it needs to be, (I would 
advise against optimizing it, as clarity is *MUCH* more important in this case imo).

	Unlike in the original VM, this optimized bytecode can be saved and used in cooked builds.  
There's no reason to keep the original bytecode around other than for editing.  This function 
effectively acts as a back-end compiler, using the original VM's bytecode as an intermediate 
representation.

	The FVectorVMOptimizeContext has a struct called "Intermediate" which is some internal state that 
	the optimizer needs for the duration of the OptimizeVectorVMScript().  The intermediate structure 
	is usually free'd at the end of the OptimizeVectorVMScript() function, however it can be saved by 
	passing VVMOptFlag_SaveIntermediateState in the Flags argument.  You may want to save it for 
	debugging purposes, but there's no reason to save it during normal runtime execution in UE.

	This document will list each step the Optimizer takes, what it does, roughly how it does it and 
why.

1. Create an Intermediate representation of all Instructions.
	- Parse the input bytecode and generate an array of FVectorVMOptimizeInstructions.
	- Any ConstBuff operands are saved in the OptimizeContext->ConstRemap table
	- Any TempRegs operands are saved in the OptimizeContext->Intermediate.RegisterUsageBuffer 
	  table
	- Count the number of External functions

	Instructions that have operands store an index: RegPtrOffset.  RegPtrOffset serves as a lookup 
into the OptimizeContext->Intermediate.RegisterUsageBuffer table to see which TempRegs or 
ConstBuffs an instruction uses.

2. Alloc memory for the External Function Table and set the number of Inputs and Outputs each 
function requires.  The function pointer is left NULL forever.  This External Function Table gets 
copied to the FVectorVMState in VectorVMInit() where the function pointers are set in the 
FVectorVMState for runtime execution.

3. Perform some sanity checks: verify the ConstRemapTable is correct.  There's two parallel arrays 
in ConstRemapTable.  The first maps the original sparse ConstBuff index to the new tightly packed 
index.  The second array is the reverse mapping.

4. Setup additional buffers:
	- OptimizeContext->Intermedate.SSARegisterUsageBuffer - This is a parallel array to the 
      RegisterUsageBuffer.  The RegPtrOffsets stored in each FVectorVMOptimizeInstruction serve as 
	  in index into the SSA SSARegisterUsageBuffer as well.
	- OptContext->Intermediate.InputRegisterFuseBuffer - uses the same RegPtrOffsetes as the two 
      RegisterUsageBuffer.  This represents the index into the 
	  OptimizeContext->Intermediate.Instructions array that a particular operand should replace 
	  with an input instruction.  ie. an add instruction (AddIns) should replace operand 1's 
	  TempReg with an Input instruction that's 8th in the list: 
	  OptContext->Intermediate.InputRegisterFuseBuffer[AddIns->Op.RegPtrOffset + 1] = 8;

5. Fill the SSARegisterUsageBuffer.  Loop through the Intermediate.Instructions array and fill out 
the SSARegisterUsageBuffer with a single static assignment register index.  Each output for an 
instruction gets a new SSA register.

6. Input Fusing.  Loop through the Intermediate.Instructions array and find all the places where an
Op's operands' TempRegs can be replaced with an Input instruction.  Set the InputFuseBits to a 
bitfield of which operands can be replaced.
	In the original VM Inputs into TempRegs are often directly written to an output.  This is 
inefficient.  The new VM has a copy_to_output instruction.  If an input can be copied directly to 
an output it's figured out in this step, and FVectorVMInstruction::Output.CopyFromInputInsIdx is 
set.  Keep track of which Inputs are still required after this step.  Most input instructions will 
not make it into the final bytecode.

7. Remove unnecessary instructions.  Occasionally the original VM emitted instructions with outputs 
that are never used.  We removed those here.

8. Fixup SSA registers for removed instructions.  Instructions that get removed might have 
unnecessary registers taking a name in the SSA table.  They are removed here.

9. Re-order acquireindex instructions to be executed ASAP.  In order to minimize the state of the 
VM while executing we want to output data we no longer need ASAP to free up the TempRegs for other 
instructions.  The first thing we need to do is figure out which instances we're keeping, and which
we're discarding.  We re-order the acquireindex instructions and their dependenices to happen ASAP.

10. Re-order update_id instructions to happen as soon after the acquireindex instruction as 
possible.  update_id uses acquireindex to free persistent IDs.

11. Re-order the output instructions to happen ASAP.  The output instructions are placed 
immediately after the register is last used.  We use the SSA table to figure this out.

12. Re-order instructions that have no dependenices to immediately before their output is used.  
This creates a tighter packing of SSA registers and allows registers to be freed for re-use.

13. Re-order Input instructions directly before they're used.  Inputs to external functions and 
acquire_id are not fused (maybe we could add this?).  This step places them immediately before 
they're used.

14. Group and sort all Output instructions that copy directly from an Input.  The copy_from_input 
instruction takes a count parameter and will loop over each Input to copy during execution.

15. Group all "normal" output instructions.  There's several new output_batch instructions that 
can output more than one register at a time.  In order to write them efficiently the output 
instructions are sorted here.

16. Since we've re-ordered instructions the 
OptimizeContext->Intermediate.InputRegisterFuseBuffer indices are wrong.  This step corrects all 
Input instruction references to their new Index.

17. Use the SSA registers to compute the minimum set of registers required to execute this script.
This step writes the new minimized index back into the 
OptContext->Intermediate.RegisterUsageBuffer array.  An instructions output TempReg can now alias 
with its input.

18. Write the final optimized bytecode.  This loops over the Instruction array twice.  The first 
time counts how may bytes are required, the second pass actually writes the bytecode.  
Instructions with fused inputs write two instruction: a fused_inputX_y and the operation itself.  
Outputs can write either a copy_to_output, output_batch or a regular output instruction.

*/

#define VVM_OPT_MAX_REGS_PER_INS 256 //this is absurdly high, but still only 512 bytes on the stack

struct FVectorVMOptimizeInsRegUsage
{
	uint16  RegIndices[VVM_OPT_MAX_REGS_PER_INS]; //Index into FVectorVMOptimizeContext::RegisterUsageBuffer.  Output follows input
	int     NumInputRegisters;
	int     NumOutputRegisters;
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

void FreezeVectorVMOptimizeContext(const FVectorVMOptimizeContext& Context, TArray<uint8>& ContextData)
{
	const uint32 BytecodeSize = Context.NumBytecodeBytes + 16;
	const uint32 ConstRemapSize = Context.NumConstsRemapped * sizeof(uint16);
	const uint32 InputRemapSize = Context.NumInputsRemapped * sizeof(uint16);
	const uint32 InputDataSetOffsetsSize = Context.NumInputDataSets * 8 * sizeof(uint16);
	const uint32 ExtFnSize = Context.NumExtFns * sizeof(FVectorVMExtFunctionData);

	const size_t BytecodeOffset = Align(sizeof(FVectorVMOptimizeContext), 16);
	const size_t ConstRemapOffset = Align(BytecodeOffset + BytecodeSize, 16);
	const size_t InputRemapOffset = Align(ConstRemapOffset + ConstRemapSize, 16);
	const size_t InputDataSetOffsetsOffset = Align(InputRemapOffset + InputRemapSize, 16);
	const size_t ExtFnOffset = Align(InputDataSetOffsetsOffset + InputDataSetOffsetsSize, 16);
	const size_t TotalSize = Align(ExtFnOffset + ExtFnSize, 16);

	ContextData.SetNumZeroed(TotalSize);
	uint8* DestData = ContextData.GetData();
	FMemory::Memcpy(DestData, &Context, sizeof(Context));
	FMemory::Memcpy(DestData + BytecodeOffset, Context.OutputBytecode, Context.NumBytecodeBytes);
	FMemory::Memcpy(DestData + ConstRemapOffset, Context.ConstRemap[1], ConstRemapSize);
	FMemory::Memcpy(DestData + InputRemapOffset, Context.InputRemapTable, InputRemapSize);
	FMemory::Memcpy(DestData + InputDataSetOffsetsOffset, Context.InputDataSetOffsets, InputDataSetOffsetsSize);
	FMemory::Memcpy(DestData + ExtFnOffset, Context.ExtFnTable, ExtFnSize);

	// we want to clear out the pointers to any callbacks
	FVectorVMOptimizeContext& DestContext = *reinterpret_cast<FVectorVMOptimizeContext*>(DestData);
	DestContext.Init.ReallocFn = nullptr;
	DestContext.Init.FreeFn = nullptr;
	DestContext.Error.CallbackFn = nullptr;
}

static void* VectorVMFrozenRealloc(void* Ptr, size_t NumBytes, const char* Filename, int LineNum)
{
	check(false);
	return nullptr;
}

static void VectorVMFrozenFree(void* Ptr, const char* Filename, int LineNum)
{
	check(false);
}

void ReinterpretVectorVMOptimizeContextData(TConstArrayView<uint8> ContextData, FVectorVMOptimizeContext& Context)
{
	const uint8* SrcData = ContextData.GetData();
	const FVectorVMOptimizeContext& SrcContext = *reinterpret_cast<const FVectorVMOptimizeContext*>(SrcData);

	const uint32 BytecodeSize = SrcContext.NumBytecodeBytes + 16;
	const uint32 ConstRemapSize = SrcContext.NumConstsRemapped * sizeof(uint16);
	const uint32 InputRemapSize = SrcContext.NumInputsRemapped * sizeof(uint16);
	const uint32 InputDataSetOffsetsSize = SrcContext.NumInputDataSets * 8 * sizeof(uint16);
	const uint32 ExtFnSize = SrcContext.NumExtFns * sizeof(FVectorVMExtFunctionData);

	const size_t BytecodeOffset = Align(sizeof(FVectorVMOptimizeContext), 16);
	const size_t ConstRemapOffset = Align(BytecodeOffset + BytecodeSize, 16);
	const size_t InputRemapOffset = Align(ConstRemapOffset + ConstRemapSize, 16);
	const size_t InputDataSetOffsetsOffset = Align(InputRemapOffset + InputRemapSize, 16);
	const size_t ExtFnOffset = Align(InputDataSetOffsetsOffset + InputDataSetOffsetsSize, 16);
	const size_t TotalSize = Align(ExtFnOffset + ExtFnSize, 16);

	FMemory::Memcpy(&Context, SrcData, sizeof(Context));
	Context.OutputBytecode = const_cast<uint8*>(reinterpret_cast<const uint8*>(SrcData + BytecodeOffset));
	Context.NumConstsAlloced = Context.NumConstsRemapped;
	Context.ConstRemap[0] = NULL;
	Context.ConstRemap[1] = const_cast<uint16*>(reinterpret_cast<const uint16*>(SrcData + ConstRemapOffset));
	Context.InputRemapTable = const_cast<uint16*>(reinterpret_cast<const uint16*>(SrcData + InputRemapOffset));
	Context.InputDataSetOffsets = const_cast<uint16*>(reinterpret_cast<const uint16*>(SrcData + InputDataSetOffsetsOffset));
	Context.ExtFnTable = const_cast<FVectorVMExtFunctionData*>(reinterpret_cast<const FVectorVMExtFunctionData*>(SrcData + ExtFnOffset));
	Context.Init.ReallocFn = VectorVMFrozenRealloc;
	Context.Init.FreeFn = VectorVMFrozenFree;
	Context.Error.CallbackFn = nullptr;
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
	//free and zero everything
	if (FreeFn)
	{
		FreeFn(OptContext->OutputBytecode, __FILE__, __LINE__);
		FreeFn(OptContext->ConstRemap[0] , __FILE__, __LINE__);
		FreeFn(OptContext->ConstRemap[1] , __FILE__, __LINE__);
		FreeFn(OptContext->InputRemapTable, __FILE__, __LINE__);
		FreeFn(OptContext->InputDataSetOffsets, __FILE__, __LINE__);
		FreeFn(OptContext->ExtFnTable    , __FILE__, __LINE__);
	}
	else
	{
		check(OptContext->OutputBytecode == nullptr);
		check(OptContext->ConstRemap[0]  == nullptr);
		check(OptContext->ConstRemap[1]  == nullptr);
		check(OptContext->InputRemapTable == nullptr);
		check(OptContext->InputDataSetOffsets== nullptr);
		check(OptContext->ExtFnTable     == nullptr);
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

static int GetRegistersUsedForInstruction(FVectorVMOptimizeContext *OptContext, FVectorVMOptimizeInstruction *Ins, FVectorVMOptimizeInsRegUsage *OutRegUsage) {
	OutRegUsage->NumInputRegisters  = 0;
	OutRegUsage->NumOutputRegisters = 0;

	switch (Ins->OpCat)
	{
	case EVectorVMOpCategory::Input:
		//if (Ins->Input.FirstInsInsertIdx != -1)
		{
			OutRegUsage->RegIndices[OutRegUsage->NumOutputRegisters++] = Ins->Input.DstRegPtrOffset;
		}
		break;
	case EVectorVMOpCategory::Output:
		OutRegUsage->RegIndices[OutRegUsage->NumInputRegisters++] = Ins->Output.RegPtrOffset;
		OutRegUsage->RegIndices[OutRegUsage->NumInputRegisters++] = Ins->Output.RegPtrOffset + 1;
		break;
	case EVectorVMOpCategory::Op:
		{ //Input registers
			int InputCount = 0;
			for (int i = 0; i < Ins->Op.NumInputs; ++i) {
				OutRegUsage->RegIndices[InputCount++] = Ins->Op.RegPtrOffset + i;
			}
			OutRegUsage->NumInputRegisters = InputCount;
		}
		{ //Output registers
			int OutputCount = 0;
			for (int i = 0; i < Ins->Op.NumOutputs; ++i) {
				OutRegUsage->RegIndices[OutRegUsage->NumInputRegisters + i] = Ins->Op.RegPtrOffset + Ins->Op.NumInputs + i;
			}
			OutRegUsage->NumOutputRegisters = Ins->Op.NumOutputs;
		}
		break;
	case EVectorVMOpCategory::ExtFnCall:
		check(Ins->ExtFnCall.NumInputs + Ins->ExtFnCall.NumOutputs < VVM_OPT_MAX_REGS_PER_INS);
		//if this check fails (*EXTREMELY* unlikely), just increase VVM_OPT_MAX_REGS_PER_INS
		for (int i = 0; i < Ins->ExtFnCall.NumInputs; ++i)
		{
			OutRegUsage->RegIndices[OutRegUsage->NumInputRegisters++] = Ins->ExtFnCall.RegPtrOffset + i;
		}
		for (int i = 0; i < Ins->ExtFnCall.NumOutputs; ++i)
		{
			OutRegUsage->RegIndices[OutRegUsage->NumInputRegisters + i] = Ins->ExtFnCall.RegPtrOffset + Ins->ExtFnCall.NumInputs + i;
		}
		OutRegUsage->NumOutputRegisters = Ins->ExtFnCall.NumOutputs;
		break;
	case EVectorVMOpCategory::IndexGen:
		OutRegUsage->RegIndices[OutRegUsage->NumInputRegisters++] = Ins->IndexGen.RegPtrOffset + 0;
		OutRegUsage->RegIndices[OutRegUsage->NumInputRegisters + OutRegUsage->NumOutputRegisters++] = Ins->IndexGen.RegPtrOffset + 1;
		break;
	case EVectorVMOpCategory::RWBuffer:
		OutRegUsage->RegIndices[0] = Ins->RWBuffer.RegPtrOffset + 0;
		OutRegUsage->RegIndices[1] = Ins->RWBuffer.RegPtrOffset + 1;
		if (Ins->OpCode == EVectorVMOp::acquire_id)
		{
			OutRegUsage->NumOutputRegisters = 2;
		} 
		else if (Ins->OpCode == EVectorVMOp::update_id)
		{
			OutRegUsage->NumInputRegisters = 2;
		}
		else
		{
			check(false);
		}
		break;
	case EVectorVMOpCategory::Stat:
		break;
	}
	check(OutRegUsage->NumInputRegisters + OutRegUsage->NumOutputRegisters < VVM_OPT_MAX_REGS_PER_INS);
	return OutRegUsage->NumInputRegisters + OutRegUsage->NumOutputRegisters;
}

static int GetInstructionDependencyChain(FVectorVMOptimizeContext *OptContext, int InsIdxToCheck, int *RegToCheckStack, int *InstructionIdxStack)
{
	int NumRegistersToCheck           = 0;
	int NumInstructions               = 0;
	FVectorVMOptimizeInstruction *Ins = OptContext->Intermediate.Instructions + InsIdxToCheck;
	FVectorVMOptimizeInsRegUsage InsRegUse = { };
	FVectorVMOptimizeInsRegUsage OpRegUse  = { };

	GetRegistersUsedForInstruction(OptContext, Ins, &InsRegUse);
	for (int i = 0; i < InsRegUse.NumInputRegisters; ++i)
	{
		RegToCheckStack[NumRegistersToCheck++] = OptContext->Intermediate.SSARegisterUsageBuffer[InsRegUse.RegIndices[i]];
	}
	while (NumRegistersToCheck > 0)
	{
		uint16 RegToCheck = RegToCheckStack[--NumRegistersToCheck];
		for (int InsIdx = InsIdxToCheck - 1; InsIdx >= 0; --InsIdx)
		{
			GetRegistersUsedForInstruction(OptContext, OptContext->Intermediate.Instructions + InsIdx, &OpRegUse);
			for (int j = 0; j < OpRegUse.NumOutputRegisters; ++j)
			{
				uint16 OutputReg = OptContext->Intermediate.SSARegisterUsageBuffer[OpRegUse.RegIndices[OpRegUse.NumInputRegisters + j]];
				if (RegToCheck == OutputReg)
				{
					bool InsAlreadyInStack = false;
					for (int i = 0; i < NumInstructions; ++i)
					{
						if (InstructionIdxStack[i] == InsIdx)
						{
							InsAlreadyInStack = true;
							break;
						}
					}
					if (!InsAlreadyInStack)
					{
						{ //insert in sorted low-to-high order
							int InsertionSlot = NumInstructions;
							for (int i = 0; i < NumInstructions; ++i)
							{
								if (InsIdx < InstructionIdxStack[i])
								{
									InsertionSlot = i;
									FMemory::Memmove(InstructionIdxStack + InsertionSlot + 1, InstructionIdxStack + InsertionSlot, sizeof(int) * (NumInstructions - InsertionSlot));
									break;
								}
							}
							InstructionIdxStack[InsertionSlot] = InsIdx;
							++NumInstructions;
						}
						for (int k = 0; k < OpRegUse.NumInputRegisters; ++k)
						{
							bool RegAlreadyInStack = false;
							uint16 Reg = OptContext->Intermediate.SSARegisterUsageBuffer[OpRegUse.RegIndices[k]];
							for (int i = 0; i < NumRegistersToCheck; ++i)
							{
								if (RegToCheckStack[i] == Reg)
								{
									RegAlreadyInStack = true;
									break;
								}
							}
							if (!RegAlreadyInStack)
							{
								RegToCheckStack[NumRegistersToCheck++] = Reg;
							}
						}
					}
				}
			}
		}
	}
	return NumInstructions;
}

inline uint64 VVMOutputInsGetSortKey(uint16 *SSARegisters, FVectorVMOptimizeInstruction *OutputIns)
{
	check(OutputIns->OpCat == EVectorVMOpCategory::Output);
	check(OutputIns->Output.DataSetIdx < (1 << 14));                                                // max 14 bits for DataSet Index (In reality this number is < 5... ie 3 bits)
	check((int)OutputIns->OpCode >= (int)EVectorVMOp::outputdata_float);
	uint64 key = (((uint64)OutputIns->OpCode - (uint64)EVectorVMOp::outputdata_float) << 62ULL) +
		         ((uint64)OutputIns->Output.DataSetIdx                                << 48ULL) +
		         ((uint64)SSARegisters[OutputIns->Output.RegPtrOffset]                << 16ULL) +
		         ((uint64)OutputIns->Output.DstRegIdx)                                          ;
	return key;
}

static FVectorVMOptimizeInstruction *VVMPushNewInstruction(FVectorVMOptimizeContext *OptContext, EVectorVMOp OpCode, uint32 *NumInstructionsAlloced)
{
	if (OptContext->Intermediate.NumInstructions >= *NumInstructionsAlloced)
	{
		if (*NumInstructionsAlloced == 0)
			*NumInstructionsAlloced = 16384;
		else
			*NumInstructionsAlloced <<= 1;
		FVectorVMOptimizeInstruction *NewInstructions = (FVectorVMOptimizeInstruction *)OptContext->Init.ReallocFn(OptContext->Intermediate.Instructions, sizeof(FVectorVMOptimizeInstruction) * *NumInstructionsAlloced, __FILE__, __LINE__);
		if (NewInstructions == nullptr)
		{
			if (OptContext->Intermediate.Instructions)
			{
				FMemory::Free(OptContext->Intermediate.Instructions);
				OptContext->Intermediate.Instructions = nullptr;
			}
			return nullptr;
		}
		FMemory::Memset(NewInstructions + OptContext->Intermediate.NumInstructions, 0, sizeof(FVectorVMOptimizeInstruction) * (*NumInstructionsAlloced - OptContext->Intermediate.NumInstructions));
		OptContext->Intermediate.Instructions = NewInstructions;
	}
	FVectorVMOptimizeInstruction *Ins = OptContext->Intermediate.Instructions + OptContext->Intermediate.NumInstructions;
	Ins->Index        = OptContext->Intermediate.NumInstructions++;
	Ins->OpCode       = OpCode;
	Ins->OpCat        = VVM_OP_CATEGORIES[(int)OpCode];
	Ins->InsMergedIdx = -1;
	return Ins;
}

#define VVM_RT_TEMPREG		0
#define VVM_RT_CONST		1
#define VVM_RT_INPUT		2
#define VVM_RT_INVALID		255

#		define VVMRegDupeCheck(RegIdx0, RegIdx1)   (Ins1SSA[RegIdx0]    == Ins1SSA[RegIdx1]    && Ins1Type[RegIdx0]    == Ins1Type[RegIdx1])

static inline bool RegDupeCheck(FVectorVMOptimizeContext *OptContext, FVectorVMOptimizeInstruction *Ins0, int Ins0OutputRegIdx, FVectorVMOptimizeInstruction *Ins1, int FirstRegIdx, int LastRegIdx) {
	uint16 Ins0OutputSSA = OptContext->Intermediate.SSARegisterUsageBuffer[Ins0->Op.RegPtrOffset + Ins0OutputRegIdx];

	uint16 *SSA = OptContext->Intermediate.SSARegisterUsageBuffer + Ins1->Op.RegPtrOffset;
	uint8 *Type = OptContext->Intermediate.RegisterUsageType + Ins1->Op.RegPtrOffset;

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

#pragma warning(disable:4883)
VECTORVM_API uint32 OptimizeVectorVMScript(const uint8 *InBytecode, int InBytecodeLen, FVectorVMExtFunctionData *ExtFnIOData, int NumExtFns, FVectorVMOptimizeContext *OptContext, uint32 Flags)
{
#define VVMAllocRegisterUse(NumRegistersToAlloc, AllocSSA)	if (OptContext->Intermediate.NumRegistersUsed + (NumRegistersToAlloc) >= NumRegisterUsageAlloced)                                                                               \
															{                                                                                                                                                                               \
																if (NumRegisterUsageAlloced == 0)                                                                                                                                           \
																{                                                                                                                                                                           \
																	NumRegisterUsageAlloced = 512;                                                                                                                                          \
																}                                                                                                                                                                           \
																else                                                                                                                                                                        \
																{                                                                                                                                                                           \
																	NumRegisterUsageAlloced <<= 1;                                                                                                                                          \
																}                                                                                                                                                                           \
																uint16 *NewRegisters = (uint16 *)OptContext->Init.ReallocFn(OptContext->Intermediate.RegisterUsageBuffer, sizeof(uint16) * NumRegisterUsageAlloced, __FILE__, __LINE__);    \
																uint8  *NewRegType    = (uint8  *)OptContext->Init.ReallocFn(OptContext->Intermediate.RegisterUsageType  , sizeof(uint8 ) * NumRegisterUsageAlloced, __FILE__, __LINE__);   \
																uint16 *NewSSA        = NULL;                                                                                                                                               \
																if (AllocSSA) {                                                                                                                                                             \
																	NewSSA           = (uint16 *)OptContext->Init.ReallocFn(OptContext->Intermediate.SSARegisterUsageBuffer, sizeof(uint16) * NumRegisterUsageAlloced, __FILE__, __LINE__); \
																}                                                                                                                                                                           \
																if (NewRegisters == nullptr || NewRegType == nullptr || (AllocSSA && NewSSA == nullptr))                                                                                    \
																{                                                                                                                                                                           \
																	return VectorVMOptimizerSetError(OptContext, VVMOptErr_OutOfMemory | VVMOptErr_RegisterUsage | VVMOptErr_Fatal);                                                        \
																}                                                                                                                                                                           \
																else                                                                                                                                                                        \
																{                                                                                                                                                                           \
																	OptContext->Intermediate.RegisterUsageBuffer = NewRegisters;                                                                                                            \
																	OptContext->Intermediate.RegisterUsageType   = NewRegType;                                                                                                              \
                                                                    if (AllocSSA) {	                                                                	                                                                	                \
																		OptContext->Intermediate.SSARegisterUsageBuffer = NewSSA;	                                                                	                                    \
																	}	                                                                	                                                                	                            \
																}                                                                                                                                                                           \
															}
#define VVMPushRegUsage(RegIdx, Type)	VVMAllocRegisterUse(1, false)                                                                                    \
										if ((Type) == VVM_RT_CONST) {																					 \
												uint16 RemappedIdx = VectorVMOptimizeRemapConst(OptContext, (RegIdx));                                   \
												if (OptContext->Error.Flags & VVMOptErr_Fatal)                                                           \
												{                                                                                                        \
													return OptContext->Error.Flags;                                                                      \
												}                                                                                                        \
												OptContext->Intermediate.RegisterUsageBuffer[OptContext->Intermediate.NumRegistersUsed] = RemappedIdx;   \
										} else {																										 \
											OptContext->Intermediate.RegisterUsageBuffer[OptContext->Intermediate.NumRegistersUsed] = (RegIdx);          \
										}																												 \
										OptContext->Intermediate.RegisterUsageType  [OptContext->Intermediate.NumRegistersUsed] = (Type);                \
										++OptContext->Intermediate.NumRegistersUsed;

#define VVMOptimizeDecodeRegIdx(OpIpVecIdx, IOFlag)	if (*OpPtrIn & (1 << OpIpVecIdx))                                                                                       \
													{                                                                                                                       \
														if (Instruction->Op.NumInputs == 0 && Instruction->Op.NumOutputs == 0)                                              \
														{                                                                                                                   \
															Instruction->Op.RegPtrOffset = OptContext->Intermediate.NumRegistersUsed;                                       \
														}                                                                                                                   \
														++Instruction->Op.NumInputs;                                                                                        \
                                                        VVMPushRegUsage(VecIndices[OpIpVecIdx], VVM_RT_CONST);                                                              \
													} else {                                                                                                                \
														if (Instruction->Op.NumInputs == 0 && Instruction->Op.NumOutputs == 0)                                              \
														{                                                                                                                   \
															Instruction->Op.RegPtrOffset = OptContext->Intermediate.NumRegistersUsed;                                       \
														}                                                                                                                   \
														if (IOFlag)                                                                                                         \
														{                                                                                                                   \
															++Instruction->Op.NumOutputs;                                                                                   \
														}                                                                                                                   \
														else                                                                                                                \
														{                                                                                                                   \
															++Instruction->Op.NumInputs;                                                                                    \
														}                                                                                                                   \
                                                        VVMPushRegUsage(VecIndices[OpIpVecIdx], VVM_RT_TEMPREG);                                                            \
													}                                                                                                                       \
													OptContext->Intermediate.NumBytecodeBytes += 2;


#define VVMOptimizeVecIns1							VVMOptimizeDecodeRegIdx(0, 0);		\
													VVMOptimizeDecodeRegIdx(1, 1);		\
													OpPtrIn += 5;

#define VVMOptimizeVecIns2							VVMOptimizeDecodeRegIdx(0, 0);		\
													VVMOptimizeDecodeRegIdx(1, 0);		\
													VVMOptimizeDecodeRegIdx(2, 1);		\
													OpPtrIn += 7;

#define VVMOptimizeVecIns3							VVMOptimizeDecodeRegIdx(0, 0);		\
													VVMOptimizeDecodeRegIdx(1, 0);		\
													VVMOptimizeDecodeRegIdx(2, 0);		\
													VVMOptimizeDecodeRegIdx(3, 1);		\
													OpPtrIn += 9;
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
	OptContext->Flags              = Flags;
	OptContext->MaxExtFnUsed       = -1;

	uint32 NumInstructionsAlloced  = 0;
	uint32 NumBytecodeBytesAlloced = 0;
	uint32 NumRegisterUsageAlloced = 0;
	int32  MaxDataSetIdx           = 0;

	const uint8 *OpPtrIn = InBytecode;
	const uint8 *OpPtrInEnd = InBytecode + InBytecodeLen;

	//Step 1: Create Intermediate representation of all Instructions
	while (OpPtrIn < OpPtrInEnd)
	{
		uint16 *VecIndices = (uint16 *)(OpPtrIn + 2);
		EVectorVMOp OpCode = (EVectorVMOp)*OpPtrIn;

		FVectorVMOptimizeInstruction *Instruction = VVMPushNewInstruction(OptContext, OpCode, &NumInstructionsAlloced);
		if (Instruction == nullptr) {
			return VectorVMOptimizerSetError(OptContext, VVMOptErr_OutOfMemory | VVMOptErr_Instructions | VVMOptErr_Fatal);
		}
		Instruction->PtrOffsetInOrigBytecode = (uint32)(OpPtrIn - InBytecode);
		OpPtrIn++;

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

				VVMAllocRegisterUse(1, false);
				Instruction->Input.DataSetIdx                                                           = DataSetIdx;
				Instruction->Input.InputIdx                                                             = InputRegIdx;
				Instruction->Input.DstRegPtrOffset                                                      = OptContext->Intermediate.NumRegistersUsed;
				VVMPushRegUsage(DstRegIdx, VVM_RT_TEMPREG);
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
				OptContext->NumOutputDataSets = VVM_MAX(OptContext->NumOutputDataSets, (uint32)(DataSetIdx + 1));
				if (DataSetIdx > MaxDataSetIdx) {
					MaxDataSetIdx = DataSetIdx;
				}

				Instruction->Output.DataSetIdx          = DataSetIdx;
				Instruction->Output.RegPtrOffset        = OptContext->Intermediate.NumRegistersUsed;
				Instruction->Output.DstRegIdx           = DstRegIdx;
				VVMPushRegUsage(DstIdxRegIdx, VVM_RT_TEMPREG);
				VVMPushRegUsage(SrcReg, OpType == 0 ? VVM_RT_TEMPREG : VVM_RT_CONST)
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

				Instruction->IndexGen.DataSetIdx   = DataSetIdx;
				Instruction->IndexGen.RegPtrOffset = OptContext->Intermediate.NumRegistersUsed;
				VVMPushRegUsage(InputRegIdx, OpType == 0 ? VVM_RT_TEMPREG : VVM_RT_CONST);
				VVMPushRegUsage(OutputReg, VVM_RT_TEMPREG);
				OpPtrIn += 7;
			}
			break;
			case EVectorVMOp::external_func_call:
			{
				uint8 ExtFnIdx = *OpPtrIn;
				check(ExtFnIdx < NumExtFns);

				Instruction->ExtFnCall.RegPtrOffset = OptContext->Intermediate.NumRegistersUsed;
				Instruction->ExtFnCall.ExtFnIdx     = ExtFnIdx;
				Instruction->ExtFnCall.NumInputs    = ExtFnIOData[ExtFnIdx].NumInputs;
				Instruction->ExtFnCall.NumOutputs   = ExtFnIOData[ExtFnIdx].NumOutputs;
				
				VVMAllocRegisterUse(ExtFnIOData[ExtFnIdx].NumInputs + ExtFnIOData[ExtFnIdx].NumOutputs, false);
				for (int i = 0; i < ExtFnIOData[ExtFnIdx].NumInputs; ++i)
				{
					if (VecIndices[i] == 0xFFFF)
					{
						VVMPushRegUsage(0xFFFF, VVM_RT_INVALID);
					}
					else
					{
						if (VecIndices[i] & 0x8000) //register: high bit means input is a register in the original bytecode
						{ 
							VVMPushRegUsage(VecIndices[i] & 0x7FFF, VVM_RT_TEMPREG);
						}
						else //constant
						{
							VVMPushRegUsage(VecIndices[i], VVM_RT_CONST);
						}
					}
				}
				for (int i = 0; i < ExtFnIOData[ExtFnIdx].NumOutputs; ++i)
				{
					int Idx = ExtFnIOData[ExtFnIdx].NumInputs + i;
					check((VecIndices[Idx] & 0x8000) == 0 || VecIndices[Idx] == 0xFFFF); //can't output to a const... 0xFFFF is invalid
					VVMPushRegUsage(VecIndices[Idx], VVM_RT_TEMPREG);
				}
				OptContext->MaxExtFnUsed      = VVM_MAX(OptContext->MaxExtFnUsed, ExtFnIdx);
				OptContext->MaxExtFnRegisters = VVM_MAX(OptContext->MaxExtFnRegisters, (uint32)(ExtFnIOData[ExtFnIdx].NumInputs + ExtFnIOData[ExtFnIdx].NumOutputs));
				OpPtrIn += 1 + (ExtFnIOData[ExtFnIdx].NumInputs + ExtFnIOData[ExtFnIdx].NumOutputs) * 2;
			}
			break;
			case EVectorVMOp::exec_index:
				Instruction->Op.RegPtrOffset = OptContext->Intermediate.NumRegistersUsed;
				Instruction->Op.NumInputs    = 0;
				Instruction->Op.NumOutputs   = 1;
				VVMPushRegUsage(*OpPtrIn, VVM_RT_TEMPREG);
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
			
				Instruction->RWBuffer.DataSetIdx   = DataSetIdx;
				Instruction->RWBuffer.RegPtrOffset = OptContext->Intermediate.NumRegistersUsed;
				VVMPushRegUsage(IDIdxReg, VVM_RT_TEMPREG);
				VVMPushRegUsage(IDTagReg, VVM_RT_TEMPREG);
				OpPtrIn += 6;
			} break;
			default:												check(false);			     break;
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
			return VectorVMOptimizerSetError(OptContext, VVMOptErr_OutOfMemory | VVMOptErr_InputFuseBuffer | VVMOptErr_Fatal);
		}
	}


	uint16 NumSSARegistersUsed = 0;
	bool Step6_InstructionsRemovedRun = false;

	{ //Step 5: SSA-like renaming of temp registers
	Step5_SSA:
		FMemory::Memcpy(OptContext->Intermediate.SSARegisterUsageBuffer, OptContext->Intermediate.RegisterUsageBuffer, sizeof(uint16) * OptContext->Intermediate.NumRegistersUsed);

		NumSSARegistersUsed = 0;
		FVectorVMOptimizeInsRegUsage InputInsRegUse;
		FVectorVMOptimizeInsRegUsage OutputInsRegUse;
				
		uint16 SSARegCount = 0;
		for (uint32 OutputInsIdx = 0; OutputInsIdx < OptContext->Intermediate.NumInstructions; ++OutputInsIdx)
		{
			FVectorVMOptimizeInstruction *OutputIns = OptContext->Intermediate.Instructions + OutputInsIdx;
			GetRegistersUsedForInstruction(OptContext, OutputIns, &OutputInsRegUse);
			//loop over each instruction's output
			for (int j = 0; j < OutputInsRegUse.NumOutputRegisters; ++j)
			{
				uint16 RegIdx  = OutputInsRegUse.RegIndices[OutputInsRegUse.NumInputRegisters + j];
				uint16 OutReg  = OptContext->Intermediate.RegisterUsageBuffer[RegIdx];
				uint8  RegType = OptContext->Intermediate.RegisterUsageType[RegIdx];
				if (OutReg != 0xFFFF && RegType == VVM_RT_TEMPREG) {
					OptContext->Intermediate.SSARegisterUsageBuffer[OutputInsRegUse.RegIndices[OutputInsRegUse.NumInputRegisters + j]] = SSARegCount;
					int LastUsedAsInputInsIdx = -1;
					//check each instruction's output with the input of every instruction that follows it
					for (uint32 InputInsIdx = OutputInsIdx + 1; InputInsIdx < OptContext->Intermediate.NumInstructions; ++InputInsIdx)
					{
						FVectorVMOptimizeInstruction *InputIns = OptContext->Intermediate.Instructions + InputInsIdx;
						GetRegistersUsedForInstruction(OptContext, InputIns, &InputInsRegUse);
					
						//check to see if the register we're currently looking at (OutReg) is overwritten by another instruction.  If it is,
						//we increment the SSA count, and move on to the next 
						for (int ii = 0; ii < InputInsRegUse.NumOutputRegisters; ++ii)
						{
							if (OptContext->Intermediate.RegisterUsageBuffer[InputInsRegUse.RegIndices[InputInsRegUse.NumInputRegisters + ii]] == OutReg)
							{
								//this register is overwritten, we need to generate a new register
								++SSARegCount;
								check(SSARegCount <= OptContext->Intermediate.NumRegistersUsed);
								goto DoneThisOutput;
							}
						}

						//if the Input instruction's input uses the Output instruction's output then assign them to the same SSA value
						for (int ii = 0; ii < InputInsRegUse.NumInputRegisters; ++ii)
						{
							if (OptContext->Intermediate.RegisterUsageBuffer[InputInsRegUse.RegIndices[ii]] == OutReg)
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
											OptContext->Intermediate.SSARegisterUsageBuffer[InputIns->Output.RegPtrOffset + j] = SSARegCount;
											LastUsedAsInputInsIdx = InputInsIdx;
										}
									}
									else
									{
										OptContext->Intermediate.SSARegisterUsageBuffer[InputIns->Output.RegPtrOffset + 1] = SSARegCount;
										LastUsedAsInputInsIdx = InputInsIdx;
									}
								}
								else
								{
									LastUsedAsInputInsIdx = InputInsIdx;
									OptContext->Intermediate.SSARegisterUsageBuffer[InputInsRegUse.RegIndices[ii]] = SSARegCount;
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
						OptContext->Intermediate.SSARegisterUsageBuffer[OutputInsRegUse.RegIndices[OutputInsRegUse.NumInputRegisters + j]] = 0xFFFF;
					}
				} else {
					OptContext->Intermediate.SSARegisterUsageBuffer[OutputInsRegUse.RegIndices[OutputInsRegUse.NumInputRegisters + j]] = 0xFFFF;
				}
				DoneThisOutput: ;
			}
		}
		if (SSARegCount >= 0xFFFF) {
			return VectorVMOptimizerSetError(OptContext, VVMOptErr_SSARemap | VVMOptErr_Overflow);
		}
		NumSSARegistersUsed = SSARegCount;
	}

	if (1 && !Step6_InstructionsRemovedRun) { //Step 6: remove instructions where outputs are never used 
		int NumRemovedInstructions = 0;
		FVectorVMOptimizeInsRegUsage RegUsage;
		FVectorVMOptimizeInsRegUsage RegUsage2;
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
					GetRegistersUsedForInstruction(OptContext, Ins, &RegUsage);
					for (int OutputIdx = 0; OutputIdx < RegUsage.NumOutputRegisters; ++OutputIdx)
					{
						uint16 RegIdx = OptContext->Intermediate.SSARegisterUsageBuffer[RegUsage.RegIndices[RegUsage.NumInputRegisters + OutputIdx]];
						for (uint32 j = i + 1; j < OptContext->Intermediate.NumInstructions; ++j)
						{
							GetRegistersUsedForInstruction(OptContext, OptContext->Intermediate.Instructions + j, &RegUsage2);
							for (int k = 0; k < RegUsage2.NumInputRegisters; ++k)
							{
								if (OptContext->Intermediate.RegisterUsageType[RegUsage2.RegIndices[k]] == VVM_RT_TEMPREG)
								{
									uint16 RegIdx2 = OptContext->Intermediate.SSARegisterUsageBuffer[RegUsage2.RegIndices[k]];
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
						//--i;
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

	int OnePastLastInputIdx = -1;
	{ //Step 7: change temp registers that come directly from inputs to the input index
		FVectorVMOptimizeInsRegUsage RegUsage;
		for (uint32 i = 0; i < OptContext->Intermediate.NumInstructions; ++i)
		{
			FVectorVMOptimizeInstruction *InputIns = OptContext->Intermediate.Instructions + i;
			if (InputIns->OpCat != EVectorVMOpCategory::Input)
			{
				continue;
			}
			OnePastLastInputIdx = i + 1;
			uint16 InputSSARegIdx = OptContext->Intermediate.SSARegisterUsageBuffer[InputIns->Input.DstRegPtrOffset];

			for (uint32 j = i + 1; j < OptContext->Intermediate.NumInstructions; ++j)
			{
				FVectorVMOptimizeInstruction *Ins = OptContext->Intermediate.Instructions + j;
				switch (Ins->OpCat) {
					case EVectorVMOpCategory::Input: //intentional fallthrough
					case EVectorVMOpCategory::Other: //intentional fallthrough
					case EVectorVMOpCategory::Stat:
						break;
					case EVectorVMOpCategory::Output:
						if (OptContext->Intermediate.RegisterUsageType[Ins->Output.RegPtrOffset + 1] == VVM_RT_TEMPREG && 
							OptContext->Intermediate.SSARegisterUsageBuffer[InputIns->Input.DstRegPtrOffset] == OptContext->Intermediate.SSARegisterUsageBuffer[Ins->Output.RegPtrOffset + 1])
						{
							check(InputIns->Index == i); //make sure the instruction is in its correct place.  Instructions could have moved, but this should have been corrected above.
							OptContext->Intermediate.SSARegisterUsageBuffer[Ins->Output.RegPtrOffset + 1] = InputIns->Index;
							OptContext->Intermediate.RegisterUsageType[Ins->Output.RegPtrOffset + 1]      = VVM_RT_INPUT;
						}
					break;
					default:
					{
						int NumRegs = GetRegistersUsedForInstruction(OptContext, Ins, &RegUsage);
						for (int k = 0; k < RegUsage.NumInputRegisters; ++k) {
							uint16 RegPtrOffset = RegUsage.RegIndices[k];
							uint16 SSAIdx       = OptContext->Intermediate.SSARegisterUsageBuffer[RegPtrOffset];
							if (SSAIdx == InputSSARegIdx && OptContext->Intermediate.RegisterUsageType[RegPtrOffset] == VVM_RT_TEMPREG)
							{
								if (InputIns->OpCode == EVectorVMOp::inputdata_half || InputIns->OpCode == EVectorVMOp::inputdata_noadvance_half) {
									//this instruction is coming directly from a half instruction, so inject a half_to_float instruction here, set the correct registers and SSA registers
									FVectorVMOptimizeInstruction *NewIns = VVMPushNewInstruction(OptContext, EVectorVMOp::half_to_float, &NumInstructionsAlloced);
									if (NewIns == nullptr) {
										return VectorVMOptimizerSetError(OptContext, VVMOptErr_OutOfMemory | VVMOptErr_Instructions | VVMOptErr_Fatal);
									}
									NewIns->Op.RegPtrOffset = OptContext->Intermediate.NumRegistersUsed;
									NewIns->Op.NumInputs    = 1;
									NewIns->Op.NumOutputs   = 1;

									uint32 PrevNumRegistersAlloced = NumRegisterUsageAlloced;

									VVMPushRegUsage(InputIns->Index, VVM_RT_INPUT);
									VVMPushRegUsage(OptContext->Intermediate.RegisterUsageType[RegPtrOffset], VVM_RT_TEMPREG);
									if (NumRegisterUsageAlloced > PrevNumRegistersAlloced) {
										OptContext->Intermediate.SSARegisterUsageBuffer = (uint16 *)OptContext->Init.ReallocFn(OptContext->Intermediate.SSARegisterUsageBuffer, sizeof(uint16) * NumRegisterUsageAlloced, __FILE__, __LINE__);
										if (OptContext->Intermediate.SSARegisterUsageBuffer == nullptr) {
											return VectorVMOptimizerSetError(OptContext, VVMOptErr_OutOfMemory | VVMOptErr_Instructions | VVMOptErr_Fatal);
										}
									}
									OptContext->Intermediate.SSARegisterUsageBuffer[NewIns->Op.RegPtrOffset]     = InputIns->Index;
									OptContext->Intermediate.SSARegisterUsageBuffer[NewIns->Op.RegPtrOffset + 1] = OptContext->Intermediate.SSARegisterUsageBuffer[RegPtrOffset];

									FVectorVMOptimizeInstruction TempIns = *NewIns;

									//move the instruction into the correct place
									FMemory::Memmove(Ins + 1, Ins, sizeof(FVectorVMOptimizeInstruction) * (OptContext->Intermediate.NumInstructions - j));
									*Ins = TempIns;
									++j;
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
	 
	if (1) { //instruction re-ordering
		int *RegToCheckStack = (int *)OptContext->Init.ReallocFn(nullptr, sizeof(int) * OptContext->Intermediate.NumRegistersUsed * 2, __FILE__, __LINE__);	//these two could actually be a single array, 1/2 the size, one starting from 0 and counting up, the other one
		if (RegToCheckStack == nullptr)
		{
			return VectorVMOptimizerSetError(OptContext, VVMOptErr_OutOfMemory | VVMOptErr_InstructionReOrder | VVMOptErr_Fatal);
		}
		int *InstructionIdxStack = RegToCheckStack + OptContext->Intermediate.NumRegistersUsed;
		int LowestInstructionIdxForAcquireIdx = OnePastLastInputIdx; //acquire index instructions will be sorted by whichever comes first in the IR... possibly worth checking if re-ordering is more efficient

		if (1) { //Step 9: Find all the acquireindex instructions and re-order them to be executed ASAP
			int NumAcquireIndexInstructions = 0;
			for (uint32 i = 0; i < OptContext->Intermediate.NumInstructions; ++i)
			{
				FVectorVMOptimizeInstruction *Ins = OptContext->Intermediate.Instructions + i;
				if (Ins->OpCode == EVectorVMOp::acquireindex)
				{
					++NumAcquireIndexInstructions;
					int AcquireIndexInstructionIdx = i;
					int NumInstructions = GetInstructionDependencyChain(OptContext, AcquireIndexInstructionIdx, RegToCheckStack, InstructionIdxStack);
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
						}
						LowestInstructionIdxForAcquireIdx = LowestInstructionIdxForAcquireIdx + 1;
					}
					//move the acquire index instruction to immediately after the last instruction it depends on
					if (LowestInstructionIdxForAcquireIdx < AcquireIndexInstructionIdx)
					{
						FVectorVMOptimizeInstruction TempIns = OptContext->Intermediate.Instructions[AcquireIndexInstructionIdx];
						FMemory::Memmove(OptContext->Intermediate.Instructions + LowestInstructionIdxForAcquireIdx + 1, OptContext->Intermediate.Instructions + LowestInstructionIdxForAcquireIdx, sizeof(FVectorVMOptimizeInstruction) * (AcquireIndexInstructionIdx - LowestInstructionIdxForAcquireIdx));
						OptContext->Intermediate.Instructions[LowestInstructionIdxForAcquireIdx++] = TempIns;
					}
				}
			}
			//there's a potential race condition if two acquireindex instructions are in a script and they're run multithreaded... ie:
			//threadA: acquireindex0
			//threadB: acquireindex0
			//threadB: acquireindex1
			//threadA: acquireindex1
			//in this situation the output data will not match between two datasets.. ie: instance0 in dataset0 will not be correlated to instance0 in dataset1.  If they were running single threaded that wouldn't happen.
		}

		if (1) { //Step 11: re-order the outputs to be done as early as possible: after the SSA's register's last usage
			for (uint32 OutputInsIdx = 0; OutputInsIdx < OptContext->Intermediate.NumInstructions; ++OutputInsIdx)
			{
				FVectorVMOptimizeInstruction *OutputIns = OptContext->Intermediate.Instructions + OutputInsIdx;
				if (OutputIns->OpCat == EVectorVMOpCategory::Output)
				{
					uint32 OutputInsertionIdx = 0xFFFFFFFF;
					bool FoundAcquireIndex = false;
					uint16 IdxReg = OptContext->Intermediate.SSARegisterUsageBuffer[OutputIns->Output.RegPtrOffset];
					uint16 SrcReg = OptContext->Intermediate.SSARegisterUsageBuffer[OutputIns->Output.RegPtrOffset + 1];
					for (uint32 i = 0; i < OutputInsIdx; ++i)
					{
						FVectorVMOptimizeInstruction *Ins = OptContext->Intermediate.Instructions + i;
						FVectorVMOptimizeInsRegUsage RegUsage;
						int NumRegisters = GetRegistersUsedForInstruction(OptContext, Ins, &RegUsage);
						
						for (int j = 0; j < RegUsage.NumOutputRegisters; ++j)
						{
							if (OptContext->Intermediate.SSARegisterUsageBuffer[RegUsage.RegIndices[RegUsage.NumInputRegisters + j]] == IdxReg)
							{
								FoundAcquireIndex = true;
								OutputInsertionIdx = i + 1;
							}
						}
						if (FoundAcquireIndex)
						{
							for (int j = 0; j < NumRegisters; ++j)
							{
								if (OptContext->Intermediate.SSARegisterUsageBuffer[RegUsage.RegIndices[j]] == SrcReg)
								{
									OutputInsertionIdx = i + 1;
								}
							}
						}
					}
					if (OutputInsertionIdx != 0xFFFFFFFF && OutputInsertionIdx < OptContext->Intermediate.NumInstructions - 1)
					{
						if (OutputInsIdx > OutputInsertionIdx)
						{
							uint32 NumInstructionsToMove = OutputInsIdx - OutputInsertionIdx;
							FVectorVMOptimizeInstruction TempIns = *OutputIns;
							FMemory::Memmove(OptContext->Intermediate.Instructions + OutputInsertionIdx + 1, OptContext->Intermediate.Instructions + OutputInsertionIdx, sizeof(FVectorVMOptimizeInstruction) * NumInstructionsToMove);
							OptContext->Intermediate.Instructions[OutputInsertionIdx] = TempIns;
						}
					}
				}
			}
		}
	
		if (1)
		{ //Step 12: re-order all dependent-less instructions to right before their output is used
			int LastSwapInstructionIdx = -1; //to prevent an infinite loop when one instruction has two or more dependencies and they keep swapping back and forth
			for (uint32 i = 0; i < OptContext->Intermediate.NumInstructions; ++i)
			{
				FVectorVMOptimizeInstruction *Ins = OptContext->Intermediate.Instructions + i;
				int SkipInstructionSwap = LastSwapInstructionIdx;
				LastSwapInstructionIdx = -1;
				if (Ins->OpCat == EVectorVMOpCategory::Op)
				{
					int OpNumDependents = GetInstructionDependencyChain(OptContext, i, RegToCheckStack, InstructionIdxStack);
					if (OpNumDependents == 0)
					{
						for (uint32 j = i + 1; j < OptContext->Intermediate.NumInstructions; ++j)
						{
							FVectorVMOptimizeInstruction *DepIns = OptContext->Intermediate.Instructions + j;
							int NumDependents = GetInstructionDependencyChain(OptContext, j, RegToCheckStack, InstructionIdxStack);
							uint32 InsDepIdx = 0xFFFFFFFF;
							for (int k = 0; k < NumDependents; ++k)
							{
								if (InstructionIdxStack[k] == i)
								{
									InsDepIdx = j;
									break;
								}
							}
							if (InsDepIdx != 0xFFFFFFFF)
							{
								if (InsDepIdx > i + 1 && InsDepIdx != SkipInstructionSwap) //DepIns is depdenent on Ins.  Move Ins to be right before DepIns
								{
									FVectorVMOptimizeInstruction TempIns = *Ins;
									FMemory::Memmove(Ins, Ins + 1, sizeof(FVectorVMOptimizeInstruction) * (InsDepIdx - i - 1));
									OptContext->Intermediate.Instructions[InsDepIdx - 1] = TempIns;
									LastSwapInstructionIdx = InsDepIdx;
									--i;
								}
								break; //we stop checking even if we don't move the instruction because it's already immediately before its first usage
							}
						}
					}
				}
			}
		}

		OptContext->Init.FreeFn(RegToCheckStack, __FILE__, __LINE__);
	}

	{ //Step 16: group and sort all output instructions
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
							//these instructions are more than one apart so we can group them.
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
	if (1)
	{ //Step whatever: figure out which instructions can be fused
		struct FFusableOp
		{
			uint32		InsIdx0;
			uint32		InsIdx1;
			int         Type;
		};
		TArray<FFusableOp> FusableOps;

		FVectorVMOptimizeInsRegUsage InsRegUse;
		FVectorVMOptimizeInsRegUsage InsRegUse2;
		FVectorVMOptimizeInsRegUsage InsRegUse3;
		for (uint32 i = 0; i < OptContext->Intermediate.NumInstructions; ++i)
		{
			FVectorVMOptimizeInstruction *Ins = OptContext->Intermediate.Instructions + i;
			GetRegistersUsedForInstruction(OptContext, Ins, &InsRegUse);

			int InsFuseIdx = -1;
			if (Ins->OpCode == EVectorVMOp::exec_index)
			{
				//check to see if exec index can fuse to specific ops
				uint16 ExecSSARegIdx = OptContext->Intermediate.SSARegisterUsageBuffer[Ins->Op.RegPtrOffset];
				for (uint32 j = i + 1; j < OptContext->Intermediate.NumInstructions; ++j)
				{
					FVectorVMOptimizeInstruction *Ins2 = OptContext->Intermediate.Instructions + j;
					GetRegistersUsedForInstruction(OptContext, Ins2, &InsRegUse2);
					for (int k = 0; k < InsRegUse2.NumInputRegisters; ++k)
					{
						uint16 InputSSAReg = OptContext->Intermediate.SSARegisterUsageBuffer[InsRegUse2.RegIndices[k]];
						if (InputSSAReg == ExecSSARegIdx)
						{
							if (Ins2->OpCode == EVectorVMOp::i2f || Ins2->OpCode == EVectorVMOp::addi)
							{
								if (InsFuseIdx == -1)
								{
									InsFuseIdx = (int)j;
									break;
								}
								else
								{
									InsFuseIdx = -1;
									goto ExecIdxCannotFuse;
								}
							}
							else
							{
								InsFuseIdx = -1;
								goto ExecIdxCannotFuse;
							}
						}
					}
				}
				if (InsFuseIdx != -1)
				{
					FFusableOp FusableOp;
					FusableOp.InsIdx0 = i;
					FusableOp.InsIdx1 = InsFuseIdx;
					FusableOp.Type    = 0;
					FusableOps.Add(FusableOp);
				}
				ExecIdxCannotFuse: ;
			}
			else if (Ins->OpCat == EVectorVMOpCategory::Op)
			{
				{ //look for ops with the same inputs
					FVectorVMOptimizeInstruction *Ins2 = OptContext->Intermediate.Instructions + 1;
					if (Ins2->OpCat == EVectorVMOpCategory::Op)
					{
						GetRegistersUsedForInstruction(OptContext, Ins2, &InsRegUse2);
						if (InsRegUse.NumInputRegisters == InsRegUse2.NumInputRegisters)
						{
							for (int k = 0; k < InsRegUse.NumInputRegisters; ++k)
							{
								if (OptContext->Intermediate.SSARegisterUsageBuffer[InsRegUse.RegIndices[k]] != OptContext->Intermediate.SSARegisterUsageBuffer[InsRegUse2.RegIndices[k]])
								{
									InsFuseIdx = -1;
									goto SameInputOpCannotFuse;
								}
							}
							InsFuseIdx = i + 1;
							break;
						}
					}
				}
				if (InsFuseIdx != -1) //-V547
				{
					FFusableOp FusableOp;
					FusableOp.InsIdx0 = InsFuseIdx;
					FusableOp.InsIdx1 = i;
					FusableOp.Type    = -1;
					FusableOps.Add(FusableOp);
					continue;
				}
				SameInputOpCannotFuse:
				//loop for ops where the output of one is the input of another
				for (uint32 j = i + 1; j < OptContext->Intermediate.NumInstructions; ++j)
				{
					FVectorVMOptimizeInstruction *Ins2 = OptContext->Intermediate.Instructions + j;
					if (Ins2->OpCat == EVectorVMOpCategory::Op)
					{
						GetRegistersUsedForInstruction(OptContext, Ins2, &InsRegUse2);
						if (InsRegUse2.NumInputRegisters < 4)
						{
							bool InputRegMatches[4] = { };
							int RegMatchCount = 0;
							for (int k = 0; k < InsRegUse.NumOutputRegisters; ++k)
							{
								uint16 OutSSAReg = OptContext->Intermediate.SSARegisterUsageBuffer[InsRegUse.RegIndices[InsRegUse.NumInputRegisters + k]];
								if (OptContext->Intermediate.RegisterUsageType[InsRegUse.RegIndices[InsRegUse.NumInputRegisters + k]] == VVM_RT_TEMPREG)
								{
									for (int ii = 0; ii < InsRegUse2.NumInputRegisters; ++ii)
									{
										if (OptContext->Intermediate.RegisterUsageType[InsRegUse2.RegIndices[ii]] == VVM_RT_TEMPREG)
										{
											uint16 InSSAReg = OptContext->Intermediate.SSARegisterUsageBuffer[InsRegUse2.RegIndices[ii]];
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
									GetRegistersUsedForInstruction(OptContext, Ins3, &InsRegUse3);
									for (int ii = 0; ii < InsRegUse.NumOutputRegisters; ++ii)
									{
										uint16 OutSSAReg = OptContext->Intermediate.SSARegisterUsageBuffer[InsRegUse.RegIndices[InsRegUse.NumInputRegisters + ii]];
										for (int jj = 0; jj < InsRegUse3.NumInputRegisters; ++jj)
										{
											uint16 InSSAReg = OptContext->Intermediate.SSARegisterUsageBuffer[InsRegUse3.RegIndices[jj]];
											//these instruction may be technically mergable, but there's no reason to do it given
											//the output of the first instruction is used again.
											if (OutSSAReg == InSSAReg)
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
									FVectorVMOptimizeInsRegUsage InsRegUse_InBetween;
									for (uint32 k = i + 1; k < j; ++k)
									{
										FVectorVMOptimizeInstruction *Ins_InBetween = OptContext->Intermediate.Instructions + k;
										GetRegistersUsedForInstruction(OptContext, Ins_InBetween, &InsRegUse_InBetween);

										//check to see if the output from the first instruction is needed for this in-between instruction
										for (int oi = 0; oi < InsRegUse.NumOutputRegisters; ++oi)
										{
											uint16 OutSSAReg = OptContext->Intermediate.SSARegisterUsageBuffer[InsRegUse.RegIndices[InsRegUse.NumInputRegisters + oi]];
											for (int ii = 0; ii < InsRegUse_InBetween.NumInputRegisters; ++ii)
											{
												uint16 InSSAReg = OptContext->Intermediate.SSARegisterUsageBuffer[InsRegUse_InBetween.RegIndices[ii]];
												if (OutSSAReg == InSSAReg)
												{
													CanMergeInstructions = false;
													goto CannotMergeInstructions;
												}
											}
										}
										
										//if we're still good, check to see if the output from this in-between instruction is needed
										//as an input for the second instruction we want to merge.
										if (CanMergeInstructions)
										{
											for (int oi = 0; oi < InsRegUse_InBetween.NumOutputRegisters; ++oi)
											{
												uint16 OutSSAReg = OptContext->Intermediate.SSARegisterUsageBuffer[InsRegUse_InBetween.RegIndices[InsRegUse_InBetween.NumInputRegisters + oi]];
												for (int ii = 0; ii < InsRegUse2.NumInputRegisters; ++ii)
												{
													uint16 InSSAReg = OptContext->Intermediate.SSARegisterUsageBuffer[InsRegUse2.RegIndices[ii]];
													if (OutSSAReg == InSSAReg)
													{
														CanMergeInstructions = false;
														goto CannotMergeInstructions;
													}
												}
											}
										}
									}
								}
								if (CanMergeInstructions)
								{
									FFusableOp FusableOp;
									FusableOp.InsIdx0 = i;
									FusableOp.InsIdx1 = j;
									FusableOp.Type    = RegMatchCount;
									FusableOps.Add(FusableOp);
								}
							}
							CannotMergeInstructions: ;
						}
					}
				}
			}
		}

		//these are the most common pairs of instructions as per Fortnite data
#		define VVMCreateNewRegVars(NumNewRegs)	VVMAllocRegisterUse((NumNewRegs), true);                                                                                \
												uint16 *Ins0Regs       = OptContext->Intermediate.RegisterUsageBuffer + Ins0->Op.RegPtrOffset;                        \
												uint16 *Ins1Regs       = OptContext->Intermediate.RegisterUsageBuffer + Ins1->Op.RegPtrOffset;                        \
												uint16 *NewRegs        = OptContext->Intermediate.RegisterUsageBuffer + OptContext->Intermediate.NumRegistersUsed;    \
												uint16 *Ins0SSA        = OptContext->Intermediate.SSARegisterUsageBuffer + Ins0->Op.RegPtrOffset;                     \
												uint16 *Ins1SSA        = OptContext->Intermediate.SSARegisterUsageBuffer + Ins1->Op.RegPtrOffset;                     \
												uint16 *NewSSA         = OptContext->Intermediate.SSARegisterUsageBuffer + OptContext->Intermediate.NumRegistersUsed; \
												uint8 *Ins0Type        = OptContext->Intermediate.RegisterUsageType + Ins0->Op.RegPtrOffset;                          \
												uint8 *Ins1Type        = OptContext->Intermediate.RegisterUsageType + Ins1->Op.RegPtrOffset;                          \
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


#		define VVMSetMergedIns(NewIns, NumInputs_, NumOutputs_)	Ins0->InsMergedIdx    = FusableOps[i].InsIdx1;   \
																Ins1->Op.NumInputs    = NumInputs_;              \
																Ins1->Op.NumOutputs   = NumOutputs_;             \
																Ins1->OpCode          = NewIns;                  \
																Ins1->Op.RegPtrOffset = NewRegPtrOffset;
		for (int i = 0; i < FusableOps.Num(); ++i)
		{
			FVectorVMOptimizeInstruction *Ins0 = OptContext->Intermediate.Instructions + FusableOps[i].InsIdx0;
			FVectorVMOptimizeInstruction *Ins1 = OptContext->Intermediate.Instructions + FusableOps[i].InsIdx1;
			if (Ins0->InsMergedIdx != -1 || Ins1->InsMergedIdx != -1)
			{
				continue; //instruction already merged
			}
			if (FusableOps[i].Type == 0)
			{
				if (Ins0->OpCode == EVectorVMOp::exec_index && Ins1->OpCode == EVectorVMOp::i2f)
				{
					for (uint32 j = FusableOps[i].InsIdx0 + 1; j < OptContext->Intermediate.NumInstructions; ++j)
					{
						if (j != FusableOps[i].InsIdx1)
						{
							FVectorVMOptimizeInstruction *OtherIns = OptContext->Intermediate.Instructions + j;
							FVectorVMOptimizeInsRegUsage RegUsage;
							GetRegistersUsedForInstruction(OptContext, OtherIns, &RegUsage);
							for (int k = 0; k < RegUsage.NumInputRegisters; ++k)
							{
								uint16 SSA = OptContext->Intermediate.SSARegisterUsageBuffer[RegUsage.RegIndices[k]];
								check(SSA != OptContext->Intermediate.SSARegisterUsageBuffer[Ins0->Op.RegPtrOffset]);
							}
						}
					}
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
			else if (FusableOps[i].Type == 1 && Ins1->OpCode == EVectorVMOp::select)
			{
				EVectorVMOp NewOpCode = EVectorVMOp::done;
				bool ReverseInputs    = false;
				switch (Ins0->OpCode)
				{
					case EVectorVMOp::cmplt:    NewOpCode = EVectorVMOp::cmplt_select;                             break;
					case EVectorVMOp::cmple:    NewOpCode = EVectorVMOp::cmple_select;                             break;
					case EVectorVMOp::cmpgt:    NewOpCode = EVectorVMOp::cmplt_select;      ReverseInputs = true;  break;
					case EVectorVMOp::cmpge:    NewOpCode = EVectorVMOp::cmple_select;      ReverseInputs = true;  break;
					case EVectorVMOp::cmpeq:    NewOpCode = EVectorVMOp::cmpeq_select;                             break;
					case EVectorVMOp::cmpneq:   NewOpCode = EVectorVMOp::cmpeq_select;      ReverseInputs = true;  break;
					case EVectorVMOp::cmplti:   NewOpCode = EVectorVMOp::cmplti_select;                            break;
					case EVectorVMOp::cmplei:   NewOpCode = EVectorVMOp::cmplei_select;                            break;
					case EVectorVMOp::cmpgti:   NewOpCode = EVectorVMOp::cmplti_select;     ReverseInputs = true;  break;
					case EVectorVMOp::cmpgei:   NewOpCode = EVectorVMOp::cmplei_select;     ReverseInputs = true;  break;
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
			else if (FusableOps[i].Type >= 1 && (Ins1->OpCode == EVectorVMOp::logic_and || Ins1->OpCode == EVectorVMOp::logic_or))
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
			else if (FusableOps[i].Type >= 1 && Ins0->OpCode == EVectorVMOp::mad)
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
				if (FusableOps[i].Type >= 1)
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
				else if (FusableOps[i].Type == -1 && Ins1->OpCode == EVectorVMOp::mul)
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
			else if (FusableOps[i].Type >= 1 && Ins0->OpCode == EVectorVMOp::add)
			{
				if (Ins1->OpCode == EVectorVMOp::mad)
				{
					//we only have a fused op if the output from the add is the input to the add op, if the op of ins0 is the mul operand from ins1, it's not statistically relevant in Fortnite so there's no instruction for it
					if (OptContext->Intermediate.SSARegisterUsageBuffer[Ins0->Op.RegPtrOffset + 2] == OptContext->Intermediate.SSARegisterUsageBuffer[Ins1->Op.RegPtrOffset + 2] && 
						OptContext->Intermediate.RegisterUsageType[Ins0->Op.RegPtrOffset + 2] == OptContext->Intermediate.RegisterUsageType[Ins1->Op.RegPtrOffset + 2])
					{
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
			else if (FusableOps[i].Type >= 1 && Ins0->OpCode == EVectorVMOp::sub)
			{
				if (Ins1->OpCode == EVectorVMOp::cmplt && FusableOps[i].Type == 2)
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
			} else if (Ins0->OpCode == EVectorVMOp::div && FusableOps[i].Type == 1) {
				switch (Ins1->OpCode) {
					case EVectorVMOp::mad:
					{
						VVMCreateNewRegVars(5);
						VVMSetRegsFrom0(0, 0);
						VVMSetRegsFrom0(1, 1);
						if (VVMRegMatch(2, 0)) {
							VVMSetRegsFrom1(2, 1);
						} else {
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
			else if (Ins0->OpCode == EVectorVMOp::b2i && Ins1->OpCode == EVectorVMOp::b2i && FusableOps[i].Type == -1)
			{
				VVMCreateNewRegVars(3);
				VVMSetRegsFrom0(0, 0);
				VVMSetRegsFrom0(1, 1);
				VVMSetRegsFrom1(2, 1);
				VVMSetMergedIns(EVectorVMOp::b2i_2x, 1, 2);
			}
			else if (Ins0->OpCode == EVectorVMOp::i2f && FusableOps[i].Type >= 1)
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
			else if (Ins0->OpCode == EVectorVMOp::f2i && FusableOps[i].Type >= 1)
			{
				if (Ins1->OpCode == EVectorVMOp::select && FusableOps[i].Type == 2)
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
			else if (Ins0->OpCode == EVectorVMOp::random && Ins1->OpCode == EVectorVMOp::random && FusableOps[i].Type == -1)
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
			else if (((Ins0->OpCode == EVectorVMOp::cos && Ins1->OpCode == EVectorVMOp::sin) || (Ins0->OpCode == EVectorVMOp::sin && Ins1->OpCode == EVectorVMOp::cos)) && FusableOps[i].Type == -1)
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
			else if (Ins0->OpCode == EVectorVMOp::neg && Ins1->OpCode == EVectorVMOp::cmplt && FusableOps[i].Type >= 1)
			{
				VVMCreateNewRegVars(3);
				VVMSetRegsFrom0(0, 0);
				if (FusableOps[i].Type == 1)
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
			else if (Ins0->OpCode == EVectorVMOp::random && Ins1->OpCode == EVectorVMOp::add && FusableOps[i].Type >= 1)
			{
				VVMCreateNewRegVars(3);
				VVMSetRegsFrom0(0, 0);
				if (FusableOps[i].Type == 1)
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
			else if (Ins0->OpCode == EVectorVMOp::max && Ins1->OpCode == EVectorVMOp::f2i && FusableOps[i].Type == 1)
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

#		undef VVMCreateNewRegVars
#		undef VVMSetRegsFrom0
#		undef VVMSetRegsFrom1
#		undef VVMRegMatch
#		undef VVMSetMergedIns
	}

	
	{ //Step 18: use the SSA registers to compute the minimized registers required and write them back into the register usage buffer
		int MaxLiveRegisters = 0;
		uint16 *SSAUseMap = (uint16 *)OptContext->Init.ReallocFn(nullptr, sizeof(uint16) * NumSSARegistersUsed, __FILE__, __LINE__);
		if (SSAUseMap == nullptr)
		{
			return VectorVMOptimizerSetError(OptContext, VVMOptErr_OutOfMemory | VVMOptErr_SSARemap | VVMOptErr_Fatal);
		}
		
		FMemory::Memset(SSAUseMap, 0xFF, sizeof(uint16) * NumSSARegistersUsed);

		FVectorVMOptimizeInsRegUsage InsRegUse;
		FVectorVMOptimizeInsRegUsage InsRegUse2;
		for (uint32 i = 0; i < OptContext->Intermediate.NumInstructions; ++i)
		{
			FVectorVMOptimizeInstruction *Ins = OptContext->Intermediate.Instructions + i;
			if (Ins->OpCat == EVectorVMOpCategory::Input || Ins->InsMergedIdx != -1) {
				continue;
			}
			int NumRegistersUsedForThisInstruction = GetRegistersUsedForInstruction(OptContext, Ins, &InsRegUse);
			if (NumRegistersUsedForThisInstruction == 0) {
				continue;
			}
			//check to see if any of the inputs are ever used again
			for (int j = 0; j < InsRegUse.NumInputRegisters; ++j)
			{
				uint16 SSAInputReg = OptContext->Intermediate.SSARegisterUsageBuffer[InsRegUse.RegIndices[j]];
				uint8 RegType      = OptContext->Intermediate.RegisterUsageType[InsRegUse.RegIndices[j]];
				if (RegType == VVM_RT_TEMPREG)
				{
					//check to see if the SSA Reg is currently valid, if so, update it
					for (uint16 k = 0; k < NumSSARegistersUsed; ++k)
					{
						if (SSAUseMap[k] == SSAInputReg)
						{
							OptContext->Intermediate.RegisterUsageBuffer[InsRegUse.RegIndices[j]] = k;
							break;
						}
					}

					bool SSARegStillLive = false;
					//we need to check this instruction too, because if its output aliases with its input we can't mark it as unused
					//first check if the input and output alias, if they do, the SSA register is still active
					for (int k = j + 1; k < InsRegUse.NumInputRegisters + InsRegUse.NumOutputRegisters; ++k) {
						if (SSAInputReg == OptContext->Intermediate.SSARegisterUsageBuffer[InsRegUse.RegIndices[k]] &&
							RegType     == OptContext->Intermediate.RegisterUsageType[InsRegUse.RegIndices[k]])
						{
							SSARegStillLive = true;
							goto DoneCheckingIfRegIsAlive;
						}
					}
					//next check the instructions after this one
					for (uint32 i2 = i + 1; i2 < OptContext->Intermediate.NumInstructions; ++i2)
					{
						FVectorVMOptimizeInstruction *Ins2 = OptContext->Intermediate.Instructions + i2;
						int NumRegisters = GetRegistersUsedForInstruction(OptContext, Ins2, &InsRegUse2);
						for (int k = 0; k < NumRegisters; ++k)
						{
							if (OptContext->Intermediate.RegisterUsageType[InsRegUse2.RegIndices[k]] == VVM_RT_TEMPREG && OptContext->Intermediate.SSARegisterUsageBuffer[InsRegUse2.RegIndices[k]] == SSAInputReg)
							{
								SSARegStillLive = true;
								goto DoneCheckingIfRegIsAlive;
							}
						}
					}
					DoneCheckingIfRegIsAlive:
					if (!SSARegStillLive)
					{
						//register is no longer required, so mark it as free to use
						for (int k = 0; k < NumSSARegistersUsed; ++k)
						{
							if (SSAUseMap[k] == SSAInputReg)
							{
								SSAUseMap[k] = 0xFFFF;
								break;
							}
						}
					}
				}
			}

			for (int j = 0; j < InsRegUse.NumOutputRegisters; ++j)
			{
				uint16 OutputRegIdx = InsRegUse.RegIndices[InsRegUse.NumInputRegisters + j];
				uint16 SSARegIdx = OptContext->Intermediate.SSARegisterUsageBuffer[OutputRegIdx];
				check(OptContext->Intermediate.RegisterUsageType[OutputRegIdx] == VVM_RT_TEMPREG);

				if (SSARegIdx == 0xFFFF) //"invalid" flag for external functions
				{ 
					OptContext->Intermediate.RegisterUsageBuffer[OutputRegIdx] = 0xFFFF;
				}
				else
				{
					uint16 MinimizedRegIdx = 0xFFFF;
					for (uint16 k = 0; k < NumSSARegistersUsed; ++k)
					{
						if (SSAUseMap[k] == 0xFFFF)
						{
							SSAUseMap[k] = SSARegIdx;
							MinimizedRegIdx = k;
							break;
						}
					}
					if (MinimizedRegIdx == 0xFFFF) {
						return VectorVMOptimizerSetError(OptContext, VVMOptErr_RegisterUsage);
					}
					OptContext->Intermediate.RegisterUsageBuffer[OutputRegIdx] = MinimizedRegIdx;
					
					{
						//check to see if this output register is still required.  Some instructions like external_func_call cannot be removed
						//even if their output is unnecessary because they could have external side effects outside the VM, so we need to check
						//to see if this register is required anymore
						bool OutputRegisterStillRequired = false;
						for (uint32 i2 = i + 1; i2 < OptContext->Intermediate.NumInstructions; ++i2)
						{
							FVectorVMOptimizeInstruction *Ins2 = OptContext->Intermediate.Instructions + i2;
							GetRegistersUsedForInstruction(OptContext, Ins2, &InsRegUse2);
							for (int k = 0; k < InsRegUse2.NumInputRegisters; ++k)
							{
								if (OptContext->Intermediate.SSARegisterUsageBuffer[InsRegUse2.RegIndices[k]] == OptContext->Intermediate.SSARegisterUsageBuffer[OutputRegIdx] &&
									OptContext->Intermediate.RegisterUsageType[InsRegUse2.RegIndices[k]] == VVM_RT_TEMPREG)
								{
									OutputRegisterStillRequired = true;
									goto DoneCheckingOutputInstructions;
								}
							}
						}
						DoneCheckingOutputInstructions:
						if (!OutputRegisterStillRequired) {
							for (uint16 k = 0; k < NumSSARegistersUsed; ++k)
							{
								if (SSAUseMap[k] == SSARegIdx)
								{
									SSAUseMap[k] = 0xFFFF;
									break;
								}
							}
						}
					}
				}
			}

			{ //count the live registers
				int NumLiveRegisters = 0;
				for (int j = 0; j < NumSSARegistersUsed; ++j)
				{
					NumLiveRegisters += (SSAUseMap[j] != 0xFFFF);
				}
				if (NumLiveRegisters > MaxLiveRegisters)
				{
					MaxLiveRegisters = NumLiveRegisters;
				}
			}
		}
		OptContext->NumTempRegisters = (uint32)MaxLiveRegisters;

		OptContext->Init.FreeFn(SSAUseMap, __FILE__, __LINE__);
	}

	struct InputReg {
		FVectorVMOptimizeInstruction *InputIns;   //this is safe to keep as a pointer because no re-ordering happens after this
		static int SortInputReg_Fn(const void *a, const void *b) {
			InputReg *R0 = (InputReg *)a;
			InputReg *R1 = (InputReg *)b;
				
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

	InputReg *InputRegs = nullptr;
	int NumInputRegisters = 0;
	{ //Step 19: generate input remap table
		int NumInputRegistersAlloced = 256;
		
		InputRegs = (InputReg *)OptContext->Init.ReallocFn(NULL, sizeof(InputReg) * NumInputRegistersAlloced, __FILE__, __LINE__);
		
		//Gather up all the input registers used
		int MaxInputIdx = 0;
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
						InputRegs = (InputReg *)OptContext->Init.ReallocFn(InputRegs, sizeof(InputReg) * NumInputRegistersAlloced, __FILE__, __LINE__);
					}
					check(NumInputRegisters < (int)NumInstructionsAlloced);
					InputReg *Reg = InputRegs + NumInputRegisters;
					Reg->InputIns   = Ins;
					++NumInputRegisters;

					if (Ins->Index > MaxInputIdx) {
						MaxInputIdx = Ins->Index;
					}
				}
			}
		}
		
		if (NumInputRegisters > 0) {
			qsort(InputRegs, NumInputRegisters, sizeof(InputReg), InputReg::SortInputReg_Fn);
			
			//sort them by type data set index and count how many input DataSets there are
			int MaxInputDataSet = 0;
			for (int i = 0; i < NumInputRegisters; ++i) {
				if (InputRegs[i].InputIns->Input.DataSetIdx > MaxInputDataSet) {
					MaxInputDataSet = InputRegs[i].InputIns->Input.DataSetIdx;
				}
			}
			check(MaxInputDataSet < 0xFFFE);

			uint16 *SSAInputMap = (uint16 *)OptContext->Init.ReallocFn(NULL, sizeof(uint16) * (MaxInputIdx + 1), __FILE__, __LINE__);	
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
			for (uint16 i = 0; i < NumInputRegisters; ++i) {
				FVectorVMOptimizeInstruction *InputIns = InputRegs[i].InputIns;
				check(InputIns->Index <= MaxInputIdx);
				SSAInputMap[InputIns->Index] = i;
				if (InputIns->Input.DataSetIdx != PrevInputDataSet) {
					//fill out the missing offsets when there's no inputs for a particular type on the previous DataSet
					for (int j = 0; j <= (int)EVectorVMOp::inputdata_noadvance_half - (int)PrevInputOpcode; ++j) {
						OptContext->InputDataSetOffsets[(PrevInputDataSet << 3) + j + (int)PrevInputOpcode - (int)EVectorVMOp::inputdata_float + 1] = i;
					}
					//fill out any in-between DataSets that are empty
					for (int j = PrevInputDataSet + 1; j < InputIns->Input.DataSetIdx; ++j) {
						for (int k = 0; k < 8; ++k) {
							OptContext->InputDataSetOffsets[(j << 3) + k] = i;
						}
					}
					//fill out the missing offsets when there's no inputs for a type in this DataSet
					for (int j = 0; j <= (int)InputIns->OpCode - (int)EVectorVMOp::inputdata_float; ++j) {
						OptContext->InputDataSetOffsets[(InputIns->Input.DataSetIdx << 3) + j] = i;
					}
					PrevInputOpcode  = InputIns->OpCode;
					PrevInputDataSet = InputIns->Input.DataSetIdx;
				} else if (InputIns->OpCode != PrevInputOpcode) {
					for (int j = (int)PrevInputOpcode + 1; j <= (int)InputIns->OpCode; ++j) {
						OptContext->InputDataSetOffsets[(InputIns->Input.DataSetIdx << 3) + j - (int)EVectorVMOp::inputdata_float] = i;
					}
					PrevInputOpcode  = InputIns->OpCode;
					PrevInputDataSet = InputIns->Input.DataSetIdx;
				}
			}
			//fill out the missing data in the last DataSet
			for (int j = 0; j <= (int)EVectorVMOp::inputdata_noadvance_half - (int)PrevInputOpcode; ++j) {
				OptContext->InputDataSetOffsets[(PrevInputDataSet << 3) + j + (int)PrevInputOpcode - (int)EVectorVMOp::inputdata_float + 1] = NumInputRegisters;
			}
			for (int i = 0; i < OptContext->NumInputDataSets; ++i) {
				//fill out the noadvance counts for each dataset
				OptContext->InputDataSetOffsets[(i << 3) + 7] = OptContext->InputDataSetOffsets[(i << 3) + 6] - OptContext->InputDataSetOffsets[(i << 3) + 3];
			}

			//generate the input remap table
			OptContext->InputRemapTable = (uint16 *)OptContext->Init.ReallocFn(NULL, sizeof(uint16) * NumInputRegisters, __FILE__, __LINE__);
			for (uint16 i = 0; i < NumInputRegisters; ++i) {
				OptContext->InputRemapTable[i] = InputRegs[i].InputIns->Input.InputIdx;
			}
			OptContext->NumInputsRemapped = NumInputRegisters;

			//fix up the Register Usage buffer's input registers to reference the new remapped indices
			for (uint32 i = 0; i < OptContext->Intermediate.NumRegistersUsed; ++i) {
				if (OptContext->Intermediate.RegisterUsageType[i] == VVM_RT_INPUT) {
					OptContext->Intermediate.RegisterUsageBuffer[i] = SSAInputMap[OptContext->Intermediate.SSARegisterUsageBuffer[i]];
				}
			}

			OptContext->Init.FreeFn(SSAInputMap, __FILE__, __LINE__);
		} else {
			OptContext->NumInputDataSets          = 0;
			OptContext->InputDataSetOffsets       = NULL;
			OptContext->NumInputsRemapped         = 0;
		}
	}

	{ //Step 20: write the final optimized bytecode
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

#		define VVMOptWriteReg(Idx)														\
		if (OptimizedBytecode) {														\
			uint16 Reg = OptContext->Intermediate.RegisterUsageBuffer[Idx];				\
			uint8 Type = OptContext->Intermediate.RegisterUsageType[Idx];				\
			if (Reg == 0xFFFF || Type == VVM_RT_INVALID) { /* invalid reg */			\
				OptimizedBytecode[NumOptimizedBytesWritten++] = 0xFF;					\
				OptimizedBytecode[NumOptimizedBytesWritten++] = 0xFF;					\
			} else {																	\
				if (Type == VVM_RT_CONST) {												\
					Reg += OptContext->NumTempRegisters;								\
				} else if (Type == VVM_RT_INPUT) {										\
					Reg += OptContext->NumTempRegisters + OptContext->NumConstsRemapped;\
				}																		\
				OptimizedBytecode[NumOptimizedBytesWritten++] = (uint8)((Reg) & 0xFF);  \
				OptimizedBytecode[NumOptimizedBytesWritten++] = (uint8)((Reg) >> 8);    \
			}																			\
		} else {																		\
			NumOptimizedBytesRequired += 2;									            \
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
				if (Ins->OpCode == EVectorVMOp::random || Ins->OpCode == EVectorVMOp::randomi) {
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
						int NumOutputInstructions = 1;
						//figure out how we can batch these
						for (uint32 j = i + 1; j < OptContext->Intermediate.NumInstructions; ++j)
						{
							FVectorVMOptimizeInstruction *NextIns = OptContext->Intermediate.Instructions + j;
							if (NextIns->OpCode == Ins->OpCode                                                                                                                              &&
								NextIns->Output.DataSetIdx == Ins->Output.DataSetIdx                                                                                                        &&
								OptContext->Intermediate.SSARegisterUsageBuffer[NextIns->Output.RegPtrOffset] == OptContext->Intermediate.SSARegisterUsageBuffer[Ins->Output.RegPtrOffset])
							{
								++NumOutputInstructions;
								if (NumOutputInstructions >= 0xFF)
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
						VVMOptWriteByte(NumOutputInstructions);              //0: Num Output Loops
						VVMOptWriteByte(Ins->Output.DataSetIdx);             //1: DataSet index
						VVMOptWriteReg(Ins->Output.RegPtrOffset);            //2: Index Reg
						for (int j = 0; j < NumOutputInstructions; ++j) {
							VVMOptWriteReg(Ins[j].Output.RegPtrOffset + 1);  //3; Input  Src
						}
						for (int j = 0; j < NumOutputInstructions; ++j) {
							VVMOptWriteIdx(Ins[j].Output.DstRegIdx);         //4: Output Dst
						}
						i += NumOutputInstructions - 1;
					}
				break;
				case EVectorVMOpCategory::Op:
					VVMOptWriteOpCode(Ins->OpCode);
					for (int j = 0; j < Ins->Op.NumInputs + Ins->Op.NumOutputs; ++j) {
						uint16 Idx = Ins->Op.RegPtrOffset + j;
						check(OptContext->Intermediate.RegisterUsageBuffer[Idx] != 0xFFFF);
						uint16 Reg = OptContext->Intermediate.RegisterUsageBuffer[Idx];
						uint8 Type = OptContext->Intermediate.RegisterUsageType[Idx];

						if (OptimizedBytecode) {
							if (Reg == 0xFFFF || Type == VVM_RT_INVALID) { /* invalid reg */
								OptimizedBytecode[NumOptimizedBytesWritten++] = 0xFF;
								OptimizedBytecode[NumOptimizedBytesWritten++] = 0xFF;
							} else {
								if (Type == VVM_RT_CONST) {
									Reg += OptContext->NumTempRegisters;
								} else if (Type == VVM_RT_INPUT) {
									Reg += OptContext->NumTempRegisters + OptContext->NumConstsRemapped;
								}
								OptimizedBytecode[NumOptimizedBytesWritten++] = (uint8)((Reg) & 0xFF);
								OptimizedBytecode[NumOptimizedBytesWritten++] = (uint8)((Reg) >> 8);
							}
						} else {
							NumOptimizedBytesRequired += 2;
						}
					}
					break;
				case EVectorVMOpCategory::IndexGen:
					VVMOptWriteOpCode(Ins->OpCode);
					VVMOptWriteReg(Ins->IndexGen.RegPtrOffset + 0); //1: Input Register
					VVMOptWriteReg(Ins->IndexGen.RegPtrOffset + 1); //2: Write-gather Output Register
					VVMOptWriteByte(Ins->IndexGen.DataSetIdx);      //0: DataSetIdx
					break;
				case EVectorVMOpCategory::ExtFnCall: {
					uint32 NumDummyRegs = 0;
					VVMOptWriteOpCode(Ins->OpCode);
					VVMOptWriteIdx(Ins->ExtFnCall.ExtFnIdx);
					for (int j = 0; j < ExtFnIOData[Ins->ExtFnCall.ExtFnIdx].NumInputs + ExtFnIOData[Ins->ExtFnCall.ExtFnIdx].NumOutputs; ++j)
					{
						if (OptContext->Intermediate.RegisterUsageBuffer[Ins->Op.RegPtrOffset + j] == 0xFFFF) {
							++NumDummyRegs;
						}
						VVMOptWriteReg(Ins->Op.RegPtrOffset + j);
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
					VVMOptWriteReg(Ins->RWBuffer.RegPtrOffset + 0);
					VVMOptWriteReg(Ins->RWBuffer.RegPtrOffset + 1);
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

	if (!(Flags & VVMFlag_OptSaveIntermediateState))
	{
		VectorVMFreeOptimizerIntermediateData(OptContext);
	}

#undef VVMOptimizeVecIns3
#undef VVMOptimizeVecIns2
#undef VVMOptimizeVecIns1
#undef VVMOptimizeDecodeRegIdx
#undef VVMPushRegUsage
#undef VVMAllocRegisterUse
#undef VectorVMOptimizerSetError

	return 0;
}


#undef VectorVMOptimizerSetError

