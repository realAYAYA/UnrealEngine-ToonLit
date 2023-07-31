// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigVMCore/RigVMByteCode.h"
#include "UObject/AnimObjectVersion.h"
#include "UObject/UE5MainStreamObjectVersion.h"
#include "UObject/FortniteMainBranchObjectVersion.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigVMByteCode)

void FRigVMExecuteOp::Serialize(FArchive& Ar)
{
	Ar << OpCode;
	Ar << FunctionIndex;
}

void FRigVMUnaryOp::Serialize(FArchive& Ar)
{
	Ar << OpCode;
	Ar << Arg;
}

void FRigVMBinaryOp::Serialize(FArchive& Ar)
{
	Ar << OpCode;
	Ar << ArgA;
	Ar << ArgB;
}

void FRigVMTernaryOp::Serialize(FArchive& Ar)
{
	Ar << OpCode;
	Ar << ArgA;
	Ar << ArgB;
	Ar << ArgC;
}

void FRigVMQuaternaryOp::Serialize(FArchive& Ar)
{
	Ar << OpCode;
	Ar << ArgA;
	Ar << ArgB;
	Ar << ArgC;
	Ar << ArgD;
}

void FRigVMQuinaryOp::Serialize(FArchive& Ar)
{
	Ar << OpCode;
	Ar << ArgA;
	Ar << ArgB;
	Ar << ArgC;
	Ar << ArgD;
	Ar << ArgE;
}

void FRigVMSenaryOp::Serialize(FArchive& Ar)
{
	Ar << OpCode;
	Ar << ArgA;
	Ar << ArgB;
	Ar << ArgC;
	Ar << ArgD;
	Ar << ArgE;
	Ar << ArgF;
}

void FRigVMCopyOp::Serialize(FArchive& Ar)
{
	Ar.UsingCustomVersion(FUE5MainStreamObjectVersion::GUID);
	
	Ar << OpCode;
	Ar << Source;
	Ar << Target;

	if(Ar.IsLoading())
	{
		if (Ar.CustomVer(FUE5MainStreamObjectVersion::GUID) < FUE5MainStreamObjectVersion::RigVMCopyOpStoreNumBytes)
		{
			NumBytes = 0;
			RegisterType = ERigVMRegisterType::Invalid;
		}
		else
		{
			Ar << NumBytes;
			Ar << RegisterType;
		}
	}
	else
	{
		Ar << NumBytes;
		Ar << RegisterType;
	}
}

void FRigVMComparisonOp::Serialize(FArchive& Ar)
{
	Ar << OpCode;
	Ar << A;
	Ar << B;
	Ar << Result;
}

void FRigVMJumpOp::Serialize(FArchive& Ar)
{
	Ar << OpCode;
	Ar << InstructionIndex;
}

void FRigVMJumpIfOp::Serialize(FArchive& Ar)
{
	Ar << OpCode;
	Ar << Arg;
	Ar << InstructionIndex;
	Ar << Condition;
}

void FRigVMChangeTypeOp::Serialize(FArchive& Ar)
{
	ensure(false);
}

void FRigVMInvokeEntryOp::Serialize(FArchive& Ar)
{
	if(Ar.IsLoading())
	{
		FString EntryNameString;
		Ar << EntryNameString;
		EntryName = *EntryNameString;
	}
	else
	{
		FString EntryNameString = EntryName.ToString();
		Ar << EntryNameString;
	}
}

FRigVMInstructionArray::FRigVMInstructionArray()
{
}

FRigVMInstructionArray::FRigVMInstructionArray(const FRigVMByteCode& InByteCode, bool bByteCodeIsAligned)
{
	uint64 ByteIndex = 0;
	while (ByteIndex < InByteCode.Num())
	{
		ERigVMOpCode OpCode = InByteCode.GetOpCodeAt(ByteIndex);
		if ((int32)OpCode >= (int32)ERigVMOpCode::Invalid)
		{
			checkNoEntry();
			Instructions.Reset();
			break;
		}

		uint8 OperandAlignment = 0;

		if (bByteCodeIsAligned)
		{
			uint64 Alignment = InByteCode.GetOpAlignment(OpCode);
			if (Alignment > 0)
			{
				while (!IsAligned(&InByteCode[ByteIndex], Alignment))
				{
					ByteIndex++;
				}
			}

			if (OpCode >= ERigVMOpCode::Execute_0_Operands && OpCode < ERigVMOpCode::Execute_64_Operands)
			{
				uint64 OperandByteIndex = ByteIndex + (uint64)sizeof(FRigVMExecuteOp);

				Alignment = InByteCode.GetOperandAlignment();
				if (Alignment > 0)
				{
					while (!IsAligned(&InByteCode[OperandByteIndex + OperandAlignment], Alignment))
					{
						OperandAlignment++;
					}
				}
			}
		}

		Instructions.Add(FRigVMInstruction(OpCode, ByteIndex, OperandAlignment));
		ByteIndex += InByteCode.GetOpNumBytesAt(ByteIndex, true);
	}
}

void FRigVMInstructionArray::Reset()
{
	Instructions.Reset();
}

void FRigVMInstructionArray::Empty()
{
	Instructions.Empty();
}

TArray<int32> FRigVMByteCode::EmptyInstructionIndices;

FString FRigVMByteCodeEntry::GetSanitizedName() const
{
	FString SanitizedName = Name.ToString();
	SanitizedName.ReplaceInline(TEXT(" "), TEXT("_"));
	SanitizedName.TrimStartAndEndInline();
	return SanitizedName;
}

FRigVMByteCode::FRigVMByteCode()
	: NumInstructions(0)
	, bByteCodeIsAligned(false)
{
}

void FRigVMByteCode::Serialize(FArchive& Ar)
{
	Ar.UsingCustomVersion(FAnimObjectVersion::GUID);
	Ar.UsingCustomVersion(FFortniteMainBranchObjectVersion::GUID);
	Ar.UsingCustomVersion(FUE5MainStreamObjectVersion::GUID);

	if (Ar.CustomVer(FAnimObjectVersion::GUID) < FAnimObjectVersion::StoreMarkerNamesOnSkeleton)
	{
		return;
	}

	if (Ar.IsSaving() || Ar.IsObjectReferenceCollector() || Ar.IsCountingMemory())
	{
		Save(Ar);
	}
	else if (Ar.IsLoading())
	{
		Load(Ar);
	}
	else
	{
		checkNoEntry();
	}
}

void FRigVMByteCode::Save(FArchive& Ar) const
{
	FRigVMInstructionArray Instructions;

	int32 InstructionCount = 0;
	
	Instructions = GetInstructions();
	InstructionCount = Instructions.Num();
	
	Ar << InstructionCount;

	for (int32 InstructionIndex = 0; InstructionIndex < InstructionCount; InstructionIndex++)
	{
		FRigVMInstruction Instruction;
		ERigVMOpCode OpCode = ERigVMOpCode::Invalid;

		Instruction = Instructions[InstructionIndex];
		OpCode = Instruction.OpCode;
		Ar << OpCode;

		switch (OpCode)
		{
			case ERigVMOpCode::Execute_0_Operands:
			case ERigVMOpCode::Execute_1_Operands:
			case ERigVMOpCode::Execute_2_Operands:
			case ERigVMOpCode::Execute_3_Operands:
			case ERigVMOpCode::Execute_4_Operands:
			case ERigVMOpCode::Execute_5_Operands:
			case ERigVMOpCode::Execute_6_Operands:
			case ERigVMOpCode::Execute_7_Operands:
			case ERigVMOpCode::Execute_8_Operands:
			case ERigVMOpCode::Execute_9_Operands:
			case ERigVMOpCode::Execute_10_Operands:
			case ERigVMOpCode::Execute_11_Operands:
			case ERigVMOpCode::Execute_12_Operands:
			case ERigVMOpCode::Execute_13_Operands:
			case ERigVMOpCode::Execute_14_Operands:
			case ERigVMOpCode::Execute_15_Operands:
			case ERigVMOpCode::Execute_16_Operands:
			case ERigVMOpCode::Execute_17_Operands:
			case ERigVMOpCode::Execute_18_Operands:
			case ERigVMOpCode::Execute_19_Operands:
			case ERigVMOpCode::Execute_20_Operands:
			case ERigVMOpCode::Execute_21_Operands:
			case ERigVMOpCode::Execute_22_Operands:
			case ERigVMOpCode::Execute_23_Operands:
			case ERigVMOpCode::Execute_24_Operands:
			case ERigVMOpCode::Execute_25_Operands:
			case ERigVMOpCode::Execute_26_Operands:
			case ERigVMOpCode::Execute_27_Operands:
			case ERigVMOpCode::Execute_28_Operands:
			case ERigVMOpCode::Execute_29_Operands:
			case ERigVMOpCode::Execute_30_Operands:
			case ERigVMOpCode::Execute_31_Operands:
			case ERigVMOpCode::Execute_32_Operands:
			case ERigVMOpCode::Execute_33_Operands:
			case ERigVMOpCode::Execute_34_Operands:
			case ERigVMOpCode::Execute_35_Operands:
			case ERigVMOpCode::Execute_36_Operands:
			case ERigVMOpCode::Execute_37_Operands:
			case ERigVMOpCode::Execute_38_Operands:
			case ERigVMOpCode::Execute_39_Operands:
			case ERigVMOpCode::Execute_40_Operands:
			case ERigVMOpCode::Execute_41_Operands:
			case ERigVMOpCode::Execute_42_Operands:
			case ERigVMOpCode::Execute_43_Operands:
			case ERigVMOpCode::Execute_44_Operands:
			case ERigVMOpCode::Execute_45_Operands:
			case ERigVMOpCode::Execute_46_Operands:
			case ERigVMOpCode::Execute_47_Operands:
			case ERigVMOpCode::Execute_48_Operands:
			case ERigVMOpCode::Execute_49_Operands:
			case ERigVMOpCode::Execute_50_Operands:
			case ERigVMOpCode::Execute_51_Operands:
			case ERigVMOpCode::Execute_52_Operands:
			case ERigVMOpCode::Execute_53_Operands:
			case ERigVMOpCode::Execute_54_Operands:
			case ERigVMOpCode::Execute_55_Operands:
			case ERigVMOpCode::Execute_56_Operands:
			case ERigVMOpCode::Execute_57_Operands:
			case ERigVMOpCode::Execute_58_Operands:
			case ERigVMOpCode::Execute_59_Operands:
			case ERigVMOpCode::Execute_60_Operands:
			case ERigVMOpCode::Execute_61_Operands:
			case ERigVMOpCode::Execute_62_Operands:
			case ERigVMOpCode::Execute_63_Operands:
			case ERigVMOpCode::Execute_64_Operands:
			{
				FRigVMExecuteOp Op = GetOpAt<FRigVMExecuteOp>(Instruction.ByteCodeIndex);
				Ar << Op;

				FRigVMOperandArray Operands = GetOperandsForExecuteOp(Instruction);
				int32 OperandCount = (int32)Op.GetOperandCount();
				ensure(OperandCount == Operands.Num());

				for (int32 OperandIndex = 0; OperandIndex < OperandCount; OperandIndex++)
				{
					FRigVMOperand Operand = Operands[OperandIndex];
					Ar << Operand;
				}
				break;
			}
			case ERigVMOpCode::Copy:
			{
				FRigVMCopyOp Op = GetOpAt<FRigVMCopyOp>(Instruction.ByteCodeIndex);
				Ar << Op;
				break;
			}
			case ERigVMOpCode::Zero:
			case ERigVMOpCode::BoolFalse:
			case ERigVMOpCode::BoolTrue:
			case ERigVMOpCode::Increment:
			case ERigVMOpCode::Decrement:
			case ERigVMOpCode::ArrayReset:
			case ERigVMOpCode::ArrayReverse:
			{
				FRigVMUnaryOp Op = GetOpAt<FRigVMUnaryOp>(Instruction.ByteCodeIndex);
				Ar << Op;
				break;
			}
			case ERigVMOpCode::Equals:
			case ERigVMOpCode::NotEquals:
			{
				FRigVMComparisonOp Op = GetOpAt<FRigVMComparisonOp>(Instruction.ByteCodeIndex);
				Ar << Op;
				break;
			}
			case ERigVMOpCode::JumpAbsolute:
			case ERigVMOpCode::JumpForward:
			case ERigVMOpCode::JumpBackward:
			{
				FRigVMJumpOp Op = GetOpAt<FRigVMJumpOp>(Instruction.ByteCodeIndex);
				Ar << Op;
				break;
			}
			case ERigVMOpCode::JumpAbsoluteIf:
			case ERigVMOpCode::JumpForwardIf:
			case ERigVMOpCode::JumpBackwardIf:
			{
				FRigVMJumpIfOp Op = GetOpAt<FRigVMJumpIfOp>(Instruction.ByteCodeIndex);
				Ar << Op;
				break;
			}
			case ERigVMOpCode::Exit:
			{
				// nothing todo, the ExitOp has no custom data inside of it
				// so all we need is the previously saved OpCode.
				break;
			}
			case ERigVMOpCode::BeginBlock:
			case ERigVMOpCode::ArrayGetNum:
			case ERigVMOpCode::ArraySetNum:
			case ERigVMOpCode::ArrayAppend:
			case ERigVMOpCode::ArrayClone:
			case ERigVMOpCode::ArrayRemove:
			case ERigVMOpCode::ArrayUnion:
			{
				FRigVMBinaryOp Op = GetOpAt<FRigVMBinaryOp>(Instruction.ByteCodeIndex);
				Ar << Op;
				break;
			}
			case ERigVMOpCode::ArrayAdd:
			case ERigVMOpCode::ArrayGetAtIndex:
			case ERigVMOpCode::ArraySetAtIndex:
			case ERigVMOpCode::ArrayInsert:
			case ERigVMOpCode::ArrayDifference:
			case ERigVMOpCode::ArrayIntersection:
			{
				FRigVMTernaryOp Op = GetOpAt<FRigVMTernaryOp>(Instruction.ByteCodeIndex);
				Ar << Op;
				break;
			}
			case ERigVMOpCode::ArrayFind:
			{
				FRigVMQuaternaryOp Op = GetOpAt<FRigVMQuaternaryOp>(Instruction.ByteCodeIndex);
				Ar << Op;
				break;
			}
			case ERigVMOpCode::ArrayIterator:
			{
				FRigVMSenaryOp Op = GetOpAt<FRigVMSenaryOp>(Instruction.ByteCodeIndex);
				Ar << Op;
				break;
			}
			case ERigVMOpCode::EndBlock:
			{
				// nothing todo, the EndBlock has no custom data inside of it
				// so all we need is the previously saved OpCode.
				break;
			}
			case ERigVMOpCode::InvokeEntry:
			{
				FRigVMInvokeEntryOp Op = GetOpAt<FRigVMInvokeEntryOp>(Instruction.ByteCodeIndex);
				Ar << Op;
				break;
			}
			default:
			{
				ensure(false);
			}
		}
	}

	UScriptStruct* ScriptStruct = FRigVMByteCodeEntry::StaticStruct();
	TArray<uint8, TAlignedHeapAllocator<16>> DefaultStructData;
	DefaultStructData.AddZeroed(ScriptStruct->GetStructureSize());
	ScriptStruct->InitializeDefaultValue(DefaultStructData.GetData());

	TArray<FString> View;
	for (uint16 EntryIndex = 0; EntryIndex < Entries.Num(); EntryIndex++)
	{
		FString Value;
		ScriptStruct->ExportText(Value, &Entries[EntryIndex], DefaultStructData.GetData(), nullptr, PPF_None, nullptr);
		View.Add(Value);
	}

	Ar << View;
}

void FRigVMByteCode::Load(FArchive& Ar)
{
	if (Ar.CustomVer(FFortniteMainBranchObjectVersion::GUID) < FFortniteMainBranchObjectVersion::RigVMByteCodeDeterminism)
	{
		Ar << ByteCode;
		return;
	}

	FRigVMInstructionArray Instructions;

	int32 InstructionCount = 0;
	
	ByteCode.Reset();
	bByteCodeIsAligned = false;
	Ar << InstructionCount;

	for (int32 InstructionIndex = 0; InstructionIndex < InstructionCount; InstructionIndex++)
	{
		FRigVMInstruction Instruction;
		ERigVMOpCode OpCode = ERigVMOpCode::Invalid;

		Ar << OpCode;

		switch (OpCode)
		{
			case ERigVMOpCode::Execute_0_Operands:
			case ERigVMOpCode::Execute_1_Operands:
			case ERigVMOpCode::Execute_2_Operands:
			case ERigVMOpCode::Execute_3_Operands:
			case ERigVMOpCode::Execute_4_Operands:
			case ERigVMOpCode::Execute_5_Operands:
			case ERigVMOpCode::Execute_6_Operands:
			case ERigVMOpCode::Execute_7_Operands:
			case ERigVMOpCode::Execute_8_Operands:
			case ERigVMOpCode::Execute_9_Operands:
			case ERigVMOpCode::Execute_10_Operands:
			case ERigVMOpCode::Execute_11_Operands:
			case ERigVMOpCode::Execute_12_Operands:
			case ERigVMOpCode::Execute_13_Operands:
			case ERigVMOpCode::Execute_14_Operands:
			case ERigVMOpCode::Execute_15_Operands:
			case ERigVMOpCode::Execute_16_Operands:
			case ERigVMOpCode::Execute_17_Operands:
			case ERigVMOpCode::Execute_18_Operands:
			case ERigVMOpCode::Execute_19_Operands:
			case ERigVMOpCode::Execute_20_Operands:
			case ERigVMOpCode::Execute_21_Operands:
			case ERigVMOpCode::Execute_22_Operands:
			case ERigVMOpCode::Execute_23_Operands:
			case ERigVMOpCode::Execute_24_Operands:
			case ERigVMOpCode::Execute_25_Operands:
			case ERigVMOpCode::Execute_26_Operands:
			case ERigVMOpCode::Execute_27_Operands:
			case ERigVMOpCode::Execute_28_Operands:
			case ERigVMOpCode::Execute_29_Operands:
			case ERigVMOpCode::Execute_30_Operands:
			case ERigVMOpCode::Execute_31_Operands:
			case ERigVMOpCode::Execute_32_Operands:
			case ERigVMOpCode::Execute_33_Operands:
			case ERigVMOpCode::Execute_34_Operands:
			case ERigVMOpCode::Execute_35_Operands:
			case ERigVMOpCode::Execute_36_Operands:
			case ERigVMOpCode::Execute_37_Operands:
			case ERigVMOpCode::Execute_38_Operands:
			case ERigVMOpCode::Execute_39_Operands:
			case ERigVMOpCode::Execute_40_Operands:
			case ERigVMOpCode::Execute_41_Operands:
			case ERigVMOpCode::Execute_42_Operands:
			case ERigVMOpCode::Execute_43_Operands:
			case ERigVMOpCode::Execute_44_Operands:
			case ERigVMOpCode::Execute_45_Operands:
			case ERigVMOpCode::Execute_46_Operands:
			case ERigVMOpCode::Execute_47_Operands:
			case ERigVMOpCode::Execute_48_Operands:
			case ERigVMOpCode::Execute_49_Operands:
			case ERigVMOpCode::Execute_50_Operands:
			case ERigVMOpCode::Execute_51_Operands:
			case ERigVMOpCode::Execute_52_Operands:
			case ERigVMOpCode::Execute_53_Operands:
			case ERigVMOpCode::Execute_54_Operands:
			case ERigVMOpCode::Execute_55_Operands:
			case ERigVMOpCode::Execute_56_Operands:
			case ERigVMOpCode::Execute_57_Operands:
			case ERigVMOpCode::Execute_58_Operands:
			case ERigVMOpCode::Execute_59_Operands:
			case ERigVMOpCode::Execute_60_Operands:
			case ERigVMOpCode::Execute_61_Operands:
			case ERigVMOpCode::Execute_62_Operands:
			case ERigVMOpCode::Execute_63_Operands:
			case ERigVMOpCode::Execute_64_Operands:
			{
				FRigVMExecuteOp Op;
				Ar << Op;

				int32 OperandCount = Op.GetOperandCount();
				TArray<FRigVMOperand> Operands;
				for (int32 OperandIndex = 0; OperandIndex < OperandCount; OperandIndex++)
				{
					FRigVMOperand Operand;
					Ar << Operand;
					Operands.Add(Operand);
				}

				AddExecuteOp(Op.FunctionIndex, Operands);
				break;
			}
			case ERigVMOpCode::Copy:
			{
				FRigVMCopyOp Op;
				Ar << Op;
				AddOp<FRigVMCopyOp>(Op);
				break;
			}
			case ERigVMOpCode::Zero:
			case ERigVMOpCode::BoolFalse:
			case ERigVMOpCode::BoolTrue:
			case ERigVMOpCode::Increment:
			case ERigVMOpCode::Decrement:
			case ERigVMOpCode::ArrayReset:
			case ERigVMOpCode::ArrayReverse:
			{
				FRigVMUnaryOp Op;
				Ar << Op;
				AddOp<FRigVMUnaryOp>(Op);
				break;
			}
			case ERigVMOpCode::Equals:
			case ERigVMOpCode::NotEquals:
			{
				FRigVMComparisonOp Op;
				Ar << Op;
				AddOp<FRigVMComparisonOp>(Op);
				break;
			}
			case ERigVMOpCode::JumpAbsolute:
			case ERigVMOpCode::JumpForward:
			case ERigVMOpCode::JumpBackward:
			{
				FRigVMJumpOp Op;
				Ar << Op;
				AddOp<FRigVMJumpOp>(Op);
				break;
			}
			case ERigVMOpCode::JumpAbsoluteIf:
			case ERigVMOpCode::JumpForwardIf:
			case ERigVMOpCode::JumpBackwardIf:
			{
				FRigVMJumpIfOp Op;
				Ar << Op;
				AddOp<FRigVMJumpIfOp>(Op);
				break;
			}
			case ERigVMOpCode::Exit:
			{	
				AddExitOp();	
				break;
			}
			case ERigVMOpCode::BeginBlock:
			case ERigVMOpCode::ArrayGetNum:
			case ERigVMOpCode::ArraySetNum:
			case ERigVMOpCode::ArrayAppend:
			case ERigVMOpCode::ArrayClone:
			case ERigVMOpCode::ArrayRemove:
			case ERigVMOpCode::ArrayUnion:
			{
				FRigVMBinaryOp Op;
				Ar << Op;
				AddOp<FRigVMBinaryOp>(Op);
				break;
			}
			case ERigVMOpCode::ArrayAdd:
			case ERigVMOpCode::ArrayGetAtIndex:
			case ERigVMOpCode::ArraySetAtIndex:
			case ERigVMOpCode::ArrayInsert:
			case ERigVMOpCode::ArrayDifference:
			case ERigVMOpCode::ArrayIntersection:
			{
				FRigVMTernaryOp Op;
				Ar << Op;
				AddOp<FRigVMTernaryOp>(Op);
				break;
			}
			case ERigVMOpCode::ArrayFind:
			{
				FRigVMQuaternaryOp Op;
				Ar << Op;
				AddOp<FRigVMQuaternaryOp>(Op);
				break;
			}
			case ERigVMOpCode::ArrayIterator:
			{
				FRigVMSenaryOp Op;
				Ar << Op;
				AddOp<FRigVMSenaryOp>(Op);
				break;
			}
			case ERigVMOpCode::EndBlock:
			{
				AddEndBlockOp();
				break;
			}
			case ERigVMOpCode::InvokeEntry:
			{
				FRigVMInvokeEntryOp Op;
				Ar << Op;
				AddOp<FRigVMInvokeEntryOp>(Op);
				break;
			}
			default:
			{
				ensure(false);
			}
		}
	}
	
	AlignByteCode();

	Entries.Reset();
	if (Ar.CustomVer(FAnimObjectVersion::GUID) >= FAnimObjectVersion::SerializeRigVMEntries)
	{
		UScriptStruct* ScriptStruct = FRigVMByteCodeEntry::StaticStruct();

		TArray<FString> View;
		Ar << View;

		for (int32 EntryIndex = 0; EntryIndex < View.Num(); EntryIndex++)
		{
			FRigVMByteCodeEntry Entry;
			ScriptStruct->ImportText(*View[EntryIndex], &Entry, nullptr, PPF_None, nullptr, ScriptStruct->GetName());
			Entries.Add(Entry);
		}
	}
}

void FRigVMByteCode::Reset()
{
	ByteCode.Reset();
	bByteCodeIsAligned = false;
	NumInstructions = 0;
	Entries.Reset();

#if WITH_EDITORONLY_DATA
	SubjectPerInstruction.Reset();
	SubjectToInstructions.Reset();
	CallPathPerInstruction.Reset();
	CallPathToInstructions.Reset();
	CallstackPerInstruction.Reset();
	CallstackHashToInstructions.Reset();
	CallstackHashPerInstruction.Reset();
	InputOperandsPerInstruction.Reset();
	OutputOperandsPerInstruction.Reset();
#endif
}

void FRigVMByteCode::Empty()
{
	ByteCode.Empty();
	bByteCodeIsAligned = false;
	NumInstructions = 0;
	Entries.Empty();

#if WITH_EDITORONLY_DATA
	SubjectPerInstruction.Empty();
	SubjectToInstructions.Empty();
	CallPathPerInstruction.Empty();
	CallPathToInstructions.Empty();
	CallstackPerInstruction.Empty();
	CallstackHashToInstructions.Empty();
	CallstackHashPerInstruction.Empty();
	InputOperandsPerInstruction.Empty();
	OutputOperandsPerInstruction.Empty();
#endif
}

uint32 FRigVMByteCode::GetByteCodeHash() const
{
	uint32 Hash = 0;

	for(int32 EntryIndex = 0; EntryIndex < NumEntries(); EntryIndex++)
	{
		Hash = HashCombine(Hash, GetTypeHash(GetEntry(EntryIndex).Name.ToString()));
		Hash = HashCombine(Hash, GetTypeHash(GetEntry(EntryIndex).InstructionIndex));
	}

	const FRigVMInstructionArray Instructions = GetInstructions();
	for(const FRigVMInstruction& Instruction : Instructions)
	{
		Hash = HashCombine(Hash, GetOperatorHash(Instruction));
	}

	return Hash;
}

uint32 FRigVMByteCode::GetOperatorHash(const FRigVMInstruction& InInstruction) const
{
	const ERigVMOpCode OpCode = GetOpCodeAt(InInstruction.ByteCodeIndex);
	switch (OpCode)
	{
		case ERigVMOpCode::Execute_0_Operands:
		case ERigVMOpCode::Execute_1_Operands:
		case ERigVMOpCode::Execute_2_Operands:
		case ERigVMOpCode::Execute_3_Operands:
		case ERigVMOpCode::Execute_4_Operands:
		case ERigVMOpCode::Execute_5_Operands:
		case ERigVMOpCode::Execute_6_Operands:
		case ERigVMOpCode::Execute_7_Operands:
		case ERigVMOpCode::Execute_8_Operands:
		case ERigVMOpCode::Execute_9_Operands:
		case ERigVMOpCode::Execute_10_Operands:
		case ERigVMOpCode::Execute_11_Operands:
		case ERigVMOpCode::Execute_12_Operands:
		case ERigVMOpCode::Execute_13_Operands:
		case ERigVMOpCode::Execute_14_Operands:
		case ERigVMOpCode::Execute_15_Operands:
		case ERigVMOpCode::Execute_16_Operands:
		case ERigVMOpCode::Execute_17_Operands:
		case ERigVMOpCode::Execute_18_Operands:
		case ERigVMOpCode::Execute_19_Operands:
		case ERigVMOpCode::Execute_20_Operands:
		case ERigVMOpCode::Execute_21_Operands:
		case ERigVMOpCode::Execute_22_Operands:
		case ERigVMOpCode::Execute_23_Operands:
		case ERigVMOpCode::Execute_24_Operands:
		case ERigVMOpCode::Execute_25_Operands:
		case ERigVMOpCode::Execute_26_Operands:
		case ERigVMOpCode::Execute_27_Operands:
		case ERigVMOpCode::Execute_28_Operands:
		case ERigVMOpCode::Execute_29_Operands:
		case ERigVMOpCode::Execute_30_Operands:
		case ERigVMOpCode::Execute_31_Operands:
		case ERigVMOpCode::Execute_32_Operands:
		case ERigVMOpCode::Execute_33_Operands:
		case ERigVMOpCode::Execute_34_Operands:
		case ERigVMOpCode::Execute_35_Operands:
		case ERigVMOpCode::Execute_36_Operands:
		case ERigVMOpCode::Execute_37_Operands:
		case ERigVMOpCode::Execute_38_Operands:
		case ERigVMOpCode::Execute_39_Operands:
		case ERigVMOpCode::Execute_40_Operands:
		case ERigVMOpCode::Execute_41_Operands:
		case ERigVMOpCode::Execute_42_Operands:
		case ERigVMOpCode::Execute_43_Operands:
		case ERigVMOpCode::Execute_44_Operands:
		case ERigVMOpCode::Execute_45_Operands:
		case ERigVMOpCode::Execute_46_Operands:
		case ERigVMOpCode::Execute_47_Operands:
		case ERigVMOpCode::Execute_48_Operands:
		case ERigVMOpCode::Execute_49_Operands:
		case ERigVMOpCode::Execute_50_Operands:
		case ERigVMOpCode::Execute_51_Operands:
		case ERigVMOpCode::Execute_52_Operands:
		case ERigVMOpCode::Execute_53_Operands:
		case ERigVMOpCode::Execute_54_Operands:
		case ERigVMOpCode::Execute_55_Operands:
		case ERigVMOpCode::Execute_56_Operands:
		case ERigVMOpCode::Execute_57_Operands:
		case ERigVMOpCode::Execute_58_Operands:
		case ERigVMOpCode::Execute_59_Operands:
		case ERigVMOpCode::Execute_60_Operands:
		case ERigVMOpCode::Execute_61_Operands:
		case ERigVMOpCode::Execute_62_Operands:
		case ERigVMOpCode::Execute_63_Operands:
		case ERigVMOpCode::Execute_64_Operands:
		{
			const FRigVMExecuteOp& Op = GetOpAt<FRigVMExecuteOp>(InInstruction);
			uint32 Hash = GetTypeHash(Op);
			const FRigVMOperandArray Operands = GetOperandsForExecuteOp(InInstruction);
			for(const FRigVMOperand& Operand : Operands)
			{
				Hash = HashCombine(Hash, GetTypeHash(Operand));
			}
			return Hash;
		}
		case ERigVMOpCode::Copy:
		{
			return GetTypeHash(GetOpAt<FRigVMCopyOp>(InInstruction));
		}
		case ERigVMOpCode::Zero:
		case ERigVMOpCode::BoolFalse:
		case ERigVMOpCode::BoolTrue:
		case ERigVMOpCode::Increment:
		case ERigVMOpCode::Decrement:
		case ERigVMOpCode::ArrayReset:
		case ERigVMOpCode::ArrayReverse:
		{
			return GetTypeHash(GetOpAt<FRigVMUnaryOp>(InInstruction));
		}
		case ERigVMOpCode::Equals:
		case ERigVMOpCode::NotEquals:
		{
			return GetTypeHash(GetOpAt<FRigVMComparisonOp>(InInstruction));
		}
		case ERigVMOpCode::JumpAbsolute:
		case ERigVMOpCode::JumpForward:
		case ERigVMOpCode::JumpBackward:
		{
			return GetTypeHash(GetOpAt<FRigVMJumpOp>(InInstruction));
		}
		case ERigVMOpCode::JumpAbsoluteIf:
		case ERigVMOpCode::JumpForwardIf:
		case ERigVMOpCode::JumpBackwardIf:
		{
			return GetTypeHash(GetOpAt<FRigVMJumpIfOp>(InInstruction));
		}
		case ERigVMOpCode::ChangeType:
		{
			checkNoEntry();
			return 0;
		}
		case ERigVMOpCode::Exit:
		{
			return GetTypeHash(GetOpAt<FRigVMBaseOp>(InInstruction));
		}
		case ERigVMOpCode::BeginBlock:
		case ERigVMOpCode::ArrayGetNum:
		case ERigVMOpCode::ArraySetNum:
		case ERigVMOpCode::ArrayAppend:
		case ERigVMOpCode::ArrayClone:
		case ERigVMOpCode::ArrayRemove:
		case ERigVMOpCode::ArrayUnion:
		{
			return GetTypeHash(GetOpAt<FRigVMBinaryOp>(InInstruction));
		}
		case ERigVMOpCode::ArrayAdd:
		case ERigVMOpCode::ArrayGetAtIndex:
		case ERigVMOpCode::ArraySetAtIndex:
		case ERigVMOpCode::ArrayInsert:
		case ERigVMOpCode::ArrayDifference:
		case ERigVMOpCode::ArrayIntersection:
		{				
			return GetTypeHash(GetOpAt<FRigVMTernaryOp>(InInstruction));
		}
		case ERigVMOpCode::ArrayFind:
		{				
			return GetTypeHash(GetOpAt<FRigVMQuaternaryOp>(InInstruction));
		}
		case ERigVMOpCode::ArrayIterator:
		{				
			return GetTypeHash(GetOpAt<FRigVMSenaryOp>(InInstruction));
		}
		case ERigVMOpCode::EndBlock:
		{
			return GetTypeHash(GetOpAt<FRigVMBaseOp>(InInstruction));
		}
		case ERigVMOpCode::InvokeEntry:
		{
			return GetTypeHash(GetOpAt<FRigVMInvokeEntryOp>(InInstruction));
		}
		case ERigVMOpCode::Invalid:
		{
			ensure(false);
			return 0;
		}
	}
	return 0;
}

uint64 FRigVMByteCode::Num() const
{
	return (uint64)ByteCode.Num();
}

uint64 FRigVMByteCode::NumEntries() const
{
	return Entries.Num();
}

const FRigVMByteCodeEntry& FRigVMByteCode::GetEntry(int32 InEntryIndex) const
{
	return Entries[InEntryIndex];
}

int32 FRigVMByteCode::FindEntryIndex(const FName& InEntryName) const
{
	for (int32 EntryIndex = 0; EntryIndex < Entries.Num(); EntryIndex++)
	{
		if (Entries[EntryIndex].Name == InEntryName)
		{
			return EntryIndex;
		}
	}
	return INDEX_NONE;
}

uint64 FRigVMByteCode::GetOpNumBytesAt(uint64 InByteCodeIndex, bool bIncludeOperands) const
{
	ERigVMOpCode OpCode = GetOpCodeAt(InByteCodeIndex);
	switch (OpCode)
	{
		case ERigVMOpCode::Execute_0_Operands:
		case ERigVMOpCode::Execute_1_Operands:
		case ERigVMOpCode::Execute_2_Operands:
		case ERigVMOpCode::Execute_3_Operands:
		case ERigVMOpCode::Execute_4_Operands:
		case ERigVMOpCode::Execute_5_Operands:
		case ERigVMOpCode::Execute_6_Operands:
		case ERigVMOpCode::Execute_7_Operands:
		case ERigVMOpCode::Execute_8_Operands:
		case ERigVMOpCode::Execute_9_Operands:
		case ERigVMOpCode::Execute_10_Operands:
		case ERigVMOpCode::Execute_11_Operands:
		case ERigVMOpCode::Execute_12_Operands:
		case ERigVMOpCode::Execute_13_Operands:
		case ERigVMOpCode::Execute_14_Operands:
		case ERigVMOpCode::Execute_15_Operands:
		case ERigVMOpCode::Execute_16_Operands:
		case ERigVMOpCode::Execute_17_Operands:
		case ERigVMOpCode::Execute_18_Operands:
		case ERigVMOpCode::Execute_19_Operands:
		case ERigVMOpCode::Execute_20_Operands:
		case ERigVMOpCode::Execute_21_Operands:
		case ERigVMOpCode::Execute_22_Operands:
		case ERigVMOpCode::Execute_23_Operands:
		case ERigVMOpCode::Execute_24_Operands:
		case ERigVMOpCode::Execute_25_Operands:
		case ERigVMOpCode::Execute_26_Operands:
		case ERigVMOpCode::Execute_27_Operands:
		case ERigVMOpCode::Execute_28_Operands:
		case ERigVMOpCode::Execute_29_Operands:
		case ERigVMOpCode::Execute_30_Operands:
		case ERigVMOpCode::Execute_31_Operands:
		case ERigVMOpCode::Execute_32_Operands:
		case ERigVMOpCode::Execute_33_Operands:
		case ERigVMOpCode::Execute_34_Operands:
		case ERigVMOpCode::Execute_35_Operands:
		case ERigVMOpCode::Execute_36_Operands:
		case ERigVMOpCode::Execute_37_Operands:
		case ERigVMOpCode::Execute_38_Operands:
		case ERigVMOpCode::Execute_39_Operands:
		case ERigVMOpCode::Execute_40_Operands:
		case ERigVMOpCode::Execute_41_Operands:
		case ERigVMOpCode::Execute_42_Operands:
		case ERigVMOpCode::Execute_43_Operands:
		case ERigVMOpCode::Execute_44_Operands:
		case ERigVMOpCode::Execute_45_Operands:
		case ERigVMOpCode::Execute_46_Operands:
		case ERigVMOpCode::Execute_47_Operands:
		case ERigVMOpCode::Execute_48_Operands:
		case ERigVMOpCode::Execute_49_Operands:
		case ERigVMOpCode::Execute_50_Operands:
		case ERigVMOpCode::Execute_51_Operands:
		case ERigVMOpCode::Execute_52_Operands:
		case ERigVMOpCode::Execute_53_Operands:
		case ERigVMOpCode::Execute_54_Operands:
		case ERigVMOpCode::Execute_55_Operands:
		case ERigVMOpCode::Execute_56_Operands:
		case ERigVMOpCode::Execute_57_Operands:
		case ERigVMOpCode::Execute_58_Operands:
		case ERigVMOpCode::Execute_59_Operands:
		case ERigVMOpCode::Execute_60_Operands:
		case ERigVMOpCode::Execute_61_Operands:
		case ERigVMOpCode::Execute_62_Operands:
		case ERigVMOpCode::Execute_63_Operands:
		case ERigVMOpCode::Execute_64_Operands:
		{
			uint64 NumBytes = (uint64)sizeof(FRigVMExecuteOp);
			if(bIncludeOperands)
			{
				FRigVMExecuteOp ExecuteOp;
				uint8* ExecuteOpPtr = (uint8*)&ExecuteOp;
				for (int32 ByteIndex = 0; ByteIndex < sizeof(FRigVMExecuteOp); ByteIndex++)
				{
					ExecuteOpPtr[ByteIndex] = ByteCode[InByteCodeIndex + ByteIndex];
				}

				if (bByteCodeIsAligned)
				{
					static const uint64 OperandAlignment = GetOperandAlignment();
					if (OperandAlignment > 0)
					{
						while (!IsAligned(&ByteCode[InByteCodeIndex + NumBytes], OperandAlignment))
						{
							NumBytes++;
						}
					}
				}
				NumBytes += (uint64)ExecuteOp.GetOperandCount() * (uint64)sizeof(FRigVMOperand);
			}
			return NumBytes;
		}
		case ERigVMOpCode::Copy:
		{
			return (uint64)sizeof(FRigVMCopyOp);
		}
		case ERigVMOpCode::Zero:
		case ERigVMOpCode::BoolFalse:
		case ERigVMOpCode::BoolTrue:
		case ERigVMOpCode::Increment:
		case ERigVMOpCode::Decrement:
		case ERigVMOpCode::ArrayReset:
		case ERigVMOpCode::ArrayReverse:
		{
			return (uint64)sizeof(FRigVMUnaryOp);
		}
		case ERigVMOpCode::Equals:
		case ERigVMOpCode::NotEquals:
		{
			return (uint64)sizeof(FRigVMComparisonOp);
		}
		case ERigVMOpCode::JumpAbsolute:
		case ERigVMOpCode::JumpForward:
		case ERigVMOpCode::JumpBackward:
		{
			return (uint64)sizeof(FRigVMJumpOp);
		}
		case ERigVMOpCode::JumpAbsoluteIf:
		case ERigVMOpCode::JumpForwardIf:
		case ERigVMOpCode::JumpBackwardIf:
		{
			return (uint64)sizeof(FRigVMJumpIfOp);
		}
		case ERigVMOpCode::ChangeType:
		{
			checkNoEntry();
			return 0;
		}
		case ERigVMOpCode::Exit:
		{
			return (uint64)sizeof(FRigVMBaseOp);
		}
		case ERigVMOpCode::BeginBlock:
		case ERigVMOpCode::ArrayGetNum:
		case ERigVMOpCode::ArraySetNum:
		case ERigVMOpCode::ArrayAppend:
		case ERigVMOpCode::ArrayClone:
		case ERigVMOpCode::ArrayRemove:
		case ERigVMOpCode::ArrayUnion:
		{
			return (uint64)sizeof(FRigVMBinaryOp);
		}
		case ERigVMOpCode::ArrayAdd:
		case ERigVMOpCode::ArrayGetAtIndex:
		case ERigVMOpCode::ArraySetAtIndex:
		case ERigVMOpCode::ArrayInsert:
		case ERigVMOpCode::ArrayDifference:
		case ERigVMOpCode::ArrayIntersection:
		{				
			return (uint64)sizeof(FRigVMTernaryOp);
		}
		case ERigVMOpCode::ArrayFind:
		{				
			return (uint64)sizeof(FRigVMQuaternaryOp);
		}
		case ERigVMOpCode::ArrayIterator:
		{				
			return (uint64)sizeof(FRigVMSenaryOp);
		}
		case ERigVMOpCode::EndBlock:
		{
			return (uint64)sizeof(FRigVMBaseOp);
		}
		case ERigVMOpCode::InvokeEntry:
		{
			return (uint64)sizeof(FRigVMInvokeEntryOp);
		}
		case ERigVMOpCode::Invalid:
		{
			ensure(false);
			return 0;
		}
	}
	return 0;
}

uint64 FRigVMByteCode::AddZeroOp(const FRigVMOperand& InArg)
{
	FRigVMUnaryOp Op(ERigVMOpCode::Zero, InArg);
	return AddOp(Op);
}

uint64 FRigVMByteCode::AddFalseOp(const FRigVMOperand& InArg)
{
	FRigVMUnaryOp Op(ERigVMOpCode::BoolFalse, InArg);
	return AddOp(Op);
}

uint64 FRigVMByteCode::AddTrueOp(const FRigVMOperand& InArg)
{
	FRigVMUnaryOp Op(ERigVMOpCode::BoolTrue, InArg);
	return AddOp(Op);
}

uint64 FRigVMByteCode::AddCopyOp(const FRigVMOperand& InSource, const FRigVMOperand& InTarget)
{
	check(InTarget.GetMemoryType() != ERigVMMemoryType::Literal);
	check(InSource != InTarget);

	const FRigVMCopyOp Op(InSource, InTarget);
	const uint64 ByteIndex = AddOp(Op);

#if WITH_EDITORONLY_DATA
	const FRigVMOperandArray InputOperands(&InSource, 1);
	const FRigVMOperandArray OutputOperands(&InTarget, 1);
	SetOperandsForInstruction(GetNumInstructions() - 1, InputOperands, OutputOperands);
#endif
	
	return ByteIndex;
}

uint64 FRigVMByteCode::AddCopyOp(const FRigVMCopyOp& InCopyOp)
{
	return AddCopyOp(InCopyOp.Source, InCopyOp.Target);
}

uint64 FRigVMByteCode::AddIncrementOp(const FRigVMOperand& InArg)
{
	ensure(InArg.GetMemoryType() != ERigVMMemoryType::Literal);
	FRigVMUnaryOp Op(ERigVMOpCode::Increment, InArg);
	return AddOp(Op);
}

uint64 FRigVMByteCode::AddDecrementOp(const FRigVMOperand& InArg)
{
	ensure(InArg.GetMemoryType() != ERigVMMemoryType::Literal);
	FRigVMUnaryOp Op(ERigVMOpCode::Decrement, InArg);
	return AddOp(Op);
}

uint64 FRigVMByteCode::AddEqualsOp(const FRigVMOperand& InA, const FRigVMOperand& InB, const FRigVMOperand& InResult)
{
	const FRigVMComparisonOp Op(ERigVMOpCode::Equals, InA, InB, InResult);
	const uint64 ByteIndex = AddOp(Op);
	
#if WITH_EDITORONLY_DATA
	TArray<FRigVMOperand> Inputs;
	Inputs.Add(InA);
	Inputs.Add(InB);
	const FRigVMOperandArray InputOperands(&Inputs[0], Inputs.Num());
	const FRigVMOperandArray OutputOperands(&InResult, 1);
	SetOperandsForInstruction(GetNumInstructions() - 1, InputOperands, OutputOperands);
#endif

	return ByteIndex;
}

uint64 FRigVMByteCode::AddNotEqualsOp(const FRigVMOperand& InA, const FRigVMOperand& InB, const FRigVMOperand& InResult)
{
	const FRigVMComparisonOp Op(ERigVMOpCode::NotEquals, InA, InB, InResult);
	const uint64 ByteIndex = AddOp(Op);

#if WITH_EDITORONLY_DATA
	TArray<FRigVMOperand> Inputs;
	Inputs.Add(InA);
	Inputs.Add(InB);
	const FRigVMOperandArray InputOperands(&Inputs[0], Inputs.Num());
	const FRigVMOperandArray OutputOperands(&InResult, 1);
	SetOperandsForInstruction(GetNumInstructions() - 1, InputOperands, OutputOperands);
#endif

	return ByteIndex;
}

uint64 FRigVMByteCode::AddJumpOp(ERigVMOpCode InOpCode, uint16 InInstructionIndex)
{
	FRigVMJumpOp Op(InOpCode, InInstructionIndex);
	return AddOp(Op);
}

uint64 FRigVMByteCode::AddJumpIfOp(ERigVMOpCode InOpCode, uint16 InInstructionIndex, const FRigVMOperand& InConditionArg, bool bJumpWhenConditionIs)
{
	FRigVMJumpIfOp Op(InOpCode, InConditionArg, InInstructionIndex, bJumpWhenConditionIs);
	return AddOp(Op);
}

uint64 FRigVMByteCode::AddExecuteOp(uint16 InFunctionIndex, const FRigVMOperandArray& InOperands)
{
	FRigVMExecuteOp Op(InFunctionIndex, (uint8)InOperands.Num());
	uint64 OpByteIndex = AddOp(Op);

	uint64 OperandsByteIndex = (uint64)ByteCode.AddZeroed(sizeof(FRigVMOperand) * InOperands.Num());
	FMemory::Memcpy(ByteCode.GetData() + OperandsByteIndex, InOperands.GetData(), sizeof(FRigVMOperand) * InOperands.Num());
	return OpByteIndex;
}

uint64 FRigVMByteCode::AddExitOp()
{
	FRigVMBaseOp Op(ERigVMOpCode::Exit);
	return AddOp(Op);
}

FString FRigVMByteCode::DumpToText() const
{
	TArray<FString> Lines;

	FRigVMInstructionArray Instructions = GetInstructions();
	for (int32 InstructionIndex = 0; InstructionIndex < Instructions.Num(); InstructionIndex++)
	{
		const FRigVMInstruction& Instruction = Instructions[InstructionIndex];
		ERigVMOpCode OpCode = Instruction.OpCode;

		FString Line = StaticEnum<ERigVMOpCode>()->GetNameByValue((int64)OpCode).ToString();

		switch (OpCode)
		{
			case ERigVMOpCode::Execute_0_Operands:
			case ERigVMOpCode::Execute_1_Operands:
			case ERigVMOpCode::Execute_2_Operands:
			case ERigVMOpCode::Execute_3_Operands:
			case ERigVMOpCode::Execute_4_Operands:
			case ERigVMOpCode::Execute_5_Operands:
			case ERigVMOpCode::Execute_6_Operands:
			case ERigVMOpCode::Execute_7_Operands:
			case ERigVMOpCode::Execute_8_Operands:
			case ERigVMOpCode::Execute_9_Operands:
			case ERigVMOpCode::Execute_10_Operands:
			case ERigVMOpCode::Execute_11_Operands:
			case ERigVMOpCode::Execute_12_Operands:
			case ERigVMOpCode::Execute_13_Operands:
			case ERigVMOpCode::Execute_14_Operands:
			case ERigVMOpCode::Execute_15_Operands:
			case ERigVMOpCode::Execute_16_Operands:
			case ERigVMOpCode::Execute_17_Operands:
			case ERigVMOpCode::Execute_18_Operands:
			case ERigVMOpCode::Execute_19_Operands:
			case ERigVMOpCode::Execute_20_Operands:
			case ERigVMOpCode::Execute_21_Operands:
			case ERigVMOpCode::Execute_22_Operands:
			case ERigVMOpCode::Execute_23_Operands:
			case ERigVMOpCode::Execute_24_Operands:
			case ERigVMOpCode::Execute_25_Operands:
			case ERigVMOpCode::Execute_26_Operands:
			case ERigVMOpCode::Execute_27_Operands:
			case ERigVMOpCode::Execute_28_Operands:
			case ERigVMOpCode::Execute_29_Operands:
			case ERigVMOpCode::Execute_30_Operands:
			case ERigVMOpCode::Execute_31_Operands:
			case ERigVMOpCode::Execute_32_Operands:
			case ERigVMOpCode::Execute_33_Operands:
			case ERigVMOpCode::Execute_34_Operands:
			case ERigVMOpCode::Execute_35_Operands:
			case ERigVMOpCode::Execute_36_Operands:
			case ERigVMOpCode::Execute_37_Operands:
			case ERigVMOpCode::Execute_38_Operands:
			case ERigVMOpCode::Execute_39_Operands:
			case ERigVMOpCode::Execute_40_Operands:
			case ERigVMOpCode::Execute_41_Operands:
			case ERigVMOpCode::Execute_42_Operands:
			case ERigVMOpCode::Execute_43_Operands:
			case ERigVMOpCode::Execute_44_Operands:
			case ERigVMOpCode::Execute_45_Operands:
			case ERigVMOpCode::Execute_46_Operands:
			case ERigVMOpCode::Execute_47_Operands:
			case ERigVMOpCode::Execute_48_Operands:
			case ERigVMOpCode::Execute_49_Operands:
			case ERigVMOpCode::Execute_50_Operands:
			case ERigVMOpCode::Execute_51_Operands:
			case ERigVMOpCode::Execute_52_Operands:
			case ERigVMOpCode::Execute_53_Operands:
			case ERigVMOpCode::Execute_54_Operands:
			case ERigVMOpCode::Execute_55_Operands:
			case ERigVMOpCode::Execute_56_Operands:
			case ERigVMOpCode::Execute_57_Operands:
			case ERigVMOpCode::Execute_58_Operands:
			case ERigVMOpCode::Execute_59_Operands:
			case ERigVMOpCode::Execute_60_Operands:
			case ERigVMOpCode::Execute_61_Operands:
			case ERigVMOpCode::Execute_62_Operands:
			case ERigVMOpCode::Execute_63_Operands:
			case ERigVMOpCode::Execute_64_Operands:
			{
				const FRigVMExecuteOp& Op = GetOpAt<FRigVMExecuteOp>(Instruction.ByteCodeIndex);
				Line += FString::Printf(TEXT(", FunctionIndex %d"), (int32)Op.FunctionIndex);

				FRigVMOperandArray Operands = GetOperandsForExecuteOp(Instruction);
				if (Operands.Num() > 0)
				{
					TArray<FString> OperandsContent;
					for (const FRigVMOperand& Operand : Operands)
					{
						FString OperandContent;
						FRigVMOperand::StaticStruct()->ExportText(OperandContent, &Operand, &Operand, nullptr, PPF_None, nullptr);
						OperandsContent.Add(FString::Printf(TEXT("\t%s"), *OperandContent));
					}

					Line += FString::Printf(TEXT("(\n%s\n)"), *FString::Join(OperandsContent, TEXT("\n")));
				}
				break;
			}
			case ERigVMOpCode::Copy:
			{
				const FRigVMCopyOp& Op = GetOpAt<FRigVMCopyOp>(Instruction.ByteCodeIndex);
				FString SourceContent;
				FRigVMOperand::StaticStruct()->ExportText(SourceContent, &Op.Source, &Op.Source, nullptr, PPF_None, nullptr);
				Line += FString::Printf(TEXT(", Source %s"), *SourceContent);
				FString TargetContent;
				FRigVMOperand::StaticStruct()->ExportText(TargetContent, &Op.Target, &Op.Target, nullptr, PPF_None, nullptr);
				Line += FString::Printf(TEXT(", Target %s"), *TargetContent);
				break;
			}
			case ERigVMOpCode::Zero:
			case ERigVMOpCode::BoolFalse:
			case ERigVMOpCode::BoolTrue:
			case ERigVMOpCode::Increment:
			case ERigVMOpCode::Decrement:
			case ERigVMOpCode::ArrayReset:
			case ERigVMOpCode::ArrayReverse:
			{
				const FRigVMUnaryOp& Op = GetOpAt<FRigVMUnaryOp>(Instruction.ByteCodeIndex);
				FString ArgContent;
				FRigVMOperand::StaticStruct()->ExportText(ArgContent, &Op.Arg, &Op.Arg, nullptr, PPF_None, nullptr);
				Line += FString::Printf(TEXT(", Source %s"), *ArgContent);
				break;
			}
			case ERigVMOpCode::Equals:
			case ERigVMOpCode::NotEquals:
			{
				const FRigVMComparisonOp& Op = GetOpAt<FRigVMComparisonOp>(Instruction.ByteCodeIndex);
				FString AContent;
				FRigVMOperand::StaticStruct()->ExportText(AContent, &Op.A, &Op.A, nullptr, PPF_None, nullptr);
				Line += FString::Printf(TEXT(", A %s"), *AContent);
				FString BContent;
				FRigVMOperand::StaticStruct()->ExportText(BContent, &Op.B, &Op.B, nullptr, PPF_None, nullptr);
				Line += FString::Printf(TEXT(", B %s"), *BContent);
				FString ResultContent;
				FRigVMOperand::StaticStruct()->ExportText(ResultContent, &Op.Result, &Op.Result, nullptr, PPF_None, nullptr);
				Line += FString::Printf(TEXT(", Result %s"), *ResultContent);
				break;
			}
			case ERigVMOpCode::JumpAbsolute:
			case ERigVMOpCode::JumpForward:
			case ERigVMOpCode::JumpBackward:
			{
				const FRigVMJumpOp& Op = GetOpAt<FRigVMJumpOp>(Instruction.ByteCodeIndex);
				Line += FString::Printf(TEXT(", InstructionIndex %d"), (int32)Op.InstructionIndex);
				break;
			}
			case ERigVMOpCode::JumpAbsoluteIf:
			case ERigVMOpCode::JumpForwardIf:
			case ERigVMOpCode::JumpBackwardIf:
			{
				const FRigVMJumpIfOp& Op = GetOpAt<FRigVMJumpIfOp>(Instruction.ByteCodeIndex);
				Line += FString::Printf(TEXT(", InstructionIndex %d"), (int32)Op.InstructionIndex);
				FString ArgContent;
				FRigVMOperand::StaticStruct()->ExportText(ArgContent, &Op.Arg, &Op.Arg, nullptr, PPF_None, nullptr);
				Line += FString::Printf(TEXT(", Source %s"), *ArgContent);
				Line += FString::Printf(TEXT(", Condition %d"), (int32)(Op.Condition ? 0 : 1));
				break;
			}
			case ERigVMOpCode::Exit:
			{
				break;
			}
			case ERigVMOpCode::BeginBlock:
			case ERigVMOpCode::ArrayGetNum:
			case ERigVMOpCode::ArraySetNum:
			case ERigVMOpCode::ArrayAppend:
			case ERigVMOpCode::ArrayClone:
			case ERigVMOpCode::ArrayRemove:
			case ERigVMOpCode::ArrayUnion:
			{
				const FRigVMBinaryOp& Op = GetOpAt<FRigVMBinaryOp>(Instruction.ByteCodeIndex);
				FString ArgA, ArgB;
				FRigVMOperand::StaticStruct()->ExportText(ArgA, &Op.ArgA, &Op.ArgA, nullptr, PPF_None, nullptr);
				FRigVMOperand::StaticStruct()->ExportText(ArgB, &Op.ArgB, &Op.ArgB, nullptr, PPF_None, nullptr);
				Line += FString::Printf(TEXT(", ArgA %s"), *ArgA);
				Line += FString::Printf(TEXT(", ArgB %s"), *ArgB);
				break;
			}
			case ERigVMOpCode::ArrayAdd:
			case ERigVMOpCode::ArrayGetAtIndex:
			case ERigVMOpCode::ArraySetAtIndex:
			case ERigVMOpCode::ArrayInsert:
			case ERigVMOpCode::ArrayDifference:
			case ERigVMOpCode::ArrayIntersection:
			{
				const FRigVMTernaryOp& Op = GetOpAt<FRigVMTernaryOp>(Instruction.ByteCodeIndex);
				FString ArgA, ArgB, ArgC;
				FRigVMOperand::StaticStruct()->ExportText(ArgA, &Op.ArgA, &Op.ArgA, nullptr, PPF_None, nullptr);
				FRigVMOperand::StaticStruct()->ExportText(ArgB, &Op.ArgB, &Op.ArgB, nullptr, PPF_None, nullptr);
				FRigVMOperand::StaticStruct()->ExportText(ArgC, &Op.ArgC, &Op.ArgC, nullptr, PPF_None, nullptr);
				Line += FString::Printf(TEXT(", ArgA %s"), *ArgA);
				Line += FString::Printf(TEXT(", ArgB %s"), *ArgB);
				Line += FString::Printf(TEXT(", ArgC %s"), *ArgC);
				break;
			}
			case ERigVMOpCode::ArrayFind:
			{
				const FRigVMQuaternaryOp& Op = GetOpAt<FRigVMQuaternaryOp>(Instruction.ByteCodeIndex);
				FString ArgA, ArgB, ArgC, ArgD;
				FRigVMOperand::StaticStruct()->ExportText(ArgA, &Op.ArgA, &Op.ArgA, nullptr, PPF_None, nullptr);
				FRigVMOperand::StaticStruct()->ExportText(ArgB, &Op.ArgB, &Op.ArgB, nullptr, PPF_None, nullptr);
				FRigVMOperand::StaticStruct()->ExportText(ArgC, &Op.ArgC, &Op.ArgC, nullptr, PPF_None, nullptr);
				FRigVMOperand::StaticStruct()->ExportText(ArgD, &Op.ArgD, &Op.ArgD, nullptr, PPF_None, nullptr);
				Line += FString::Printf(TEXT(", ArgA %s"), *ArgA);
				Line += FString::Printf(TEXT(", ArgB %s"), *ArgB);
				Line += FString::Printf(TEXT(", ArgC %s"), *ArgC);
				Line += FString::Printf(TEXT(", ArgD %s"), *ArgD);
				break;
			}
			case ERigVMOpCode::ArrayIterator:
			{
				const FRigVMSenaryOp& Op = GetOpAt<FRigVMSenaryOp>(Instruction.ByteCodeIndex);
				FString ArgA, ArgB, ArgC, ArgD, ArgE, ArgF;
				FRigVMOperand::StaticStruct()->ExportText(ArgA, &Op.ArgA, &Op.ArgA, nullptr, PPF_None, nullptr);
				FRigVMOperand::StaticStruct()->ExportText(ArgB, &Op.ArgB, &Op.ArgB, nullptr, PPF_None, nullptr);
				FRigVMOperand::StaticStruct()->ExportText(ArgC, &Op.ArgC, &Op.ArgC, nullptr, PPF_None, nullptr);
				FRigVMOperand::StaticStruct()->ExportText(ArgD, &Op.ArgD, &Op.ArgD, nullptr, PPF_None, nullptr);
				FRigVMOperand::StaticStruct()->ExportText(ArgE, &Op.ArgE, &Op.ArgE, nullptr, PPF_None, nullptr);
				FRigVMOperand::StaticStruct()->ExportText(ArgF, &Op.ArgF, &Op.ArgF, nullptr, PPF_None, nullptr);
				Line += FString::Printf(TEXT(", ArgA %s"), *ArgA);
				Line += FString::Printf(TEXT(", ArgB %s"), *ArgB);
				Line += FString::Printf(TEXT(", ArgC %s"), *ArgC);
				Line += FString::Printf(TEXT(", ArgD %s"), *ArgD);
				Line += FString::Printf(TEXT(", ArgE %s"), *ArgE);
				Line += FString::Printf(TEXT(", ArgF %s"), *ArgF);
				break;
			}
			case ERigVMOpCode::InvokeEntry:
			{
				const FRigVMInvokeEntryOp& Op = GetOpAt<FRigVMInvokeEntryOp>(Instruction.ByteCodeIndex);
				Line += FString::Printf(TEXT(", Entry '%s'"), *Op.EntryName.ToString());
				break;
			}
			case ERigVMOpCode::Invalid:
			{
				ensure(false);
			}
			default:
			{
				break;
			}
		}

		Lines.Add(Line);
	}

	if (Lines.Num() == 0)
	{
		return FString();
	}

	return FString::Join(Lines, TEXT("\n"));
}

uint64 FRigVMByteCode::AddBeginBlockOp(FRigVMOperand InCountArg, FRigVMOperand InIndexArg)
{
	FRigVMBinaryOp Op(ERigVMOpCode::BeginBlock, InCountArg, InIndexArg);
	return AddOp(Op);
}

uint64 FRigVMByteCode::AddEndBlockOp()
{
	FRigVMBaseOp Op(ERigVMOpCode::EndBlock);
	return AddOp(Op);
}

uint64 FRigVMByteCode::AddArrayResetOp(FRigVMOperand InArrayArg)
{
	return AddOp(FRigVMUnaryOp(ERigVMOpCode::ArrayReset, InArrayArg));
}

uint64 FRigVMByteCode::AddArrayGetNumOp(FRigVMOperand InArrayArg, FRigVMOperand InNumArg)
{
	return AddOp(FRigVMBinaryOp(ERigVMOpCode::ArrayGetNum, InArrayArg, InNumArg));
}

uint64 FRigVMByteCode::AddArraySetNumOp(FRigVMOperand InArrayArg, FRigVMOperand InNumArg)
{
	return AddOp(FRigVMBinaryOp(ERigVMOpCode::ArraySetNum, InArrayArg, InNumArg));
}

uint64 FRigVMByteCode::AddArrayGetAtIndexOp(FRigVMOperand InArrayArg, FRigVMOperand InIndexArg, FRigVMOperand InElementArg)
{
	return AddOp(FRigVMTernaryOp(ERigVMOpCode::ArrayGetAtIndex, InArrayArg, InIndexArg, InElementArg));
}

uint64 FRigVMByteCode::AddArraySetAtIndexOp(FRigVMOperand InArrayArg, FRigVMOperand InIndexArg, FRigVMOperand InElementArg)
{
	return AddOp(FRigVMTernaryOp(ERigVMOpCode::ArraySetAtIndex, InArrayArg, InIndexArg, InElementArg));
}

uint64 FRigVMByteCode::AddArrayAddOp(FRigVMOperand InArrayArg, FRigVMOperand InElementArg, FRigVMOperand InIndexArg)
{
	return AddOp(FRigVMTernaryOp(ERigVMOpCode::ArrayAdd, InArrayArg, InElementArg, InIndexArg));	
}

uint64 FRigVMByteCode::AddArrayInsertOp(FRigVMOperand InArrayArg, FRigVMOperand InIndexArg, FRigVMOperand InElementArg)
{
	return AddOp(FRigVMTernaryOp(ERigVMOpCode::ArrayInsert, InArrayArg, InIndexArg, InElementArg));
}

uint64 FRigVMByteCode::AddArrayRemoveOp(FRigVMOperand InArrayArg, FRigVMOperand InIndexArg)
{
	return AddOp(FRigVMBinaryOp(ERigVMOpCode::ArrayRemove, InArrayArg, InIndexArg));
}

uint64 FRigVMByteCode::AddArrayFindOp(FRigVMOperand InArrayArg, FRigVMOperand InElementArg, FRigVMOperand InIndexArg, FRigVMOperand InSuccessArg)
{
	return AddOp(FRigVMQuaternaryOp(ERigVMOpCode::ArrayFind, InArrayArg, InElementArg, InIndexArg, InSuccessArg));	
}

uint64 FRigVMByteCode::AddArrayAppendOp(FRigVMOperand InArrayArg, FRigVMOperand InOtherArrayArg)
{
	return AddOp(FRigVMBinaryOp(ERigVMOpCode::ArrayAppend, InArrayArg, InOtherArrayArg));
}

uint64 FRigVMByteCode::AddArrayCloneOp(FRigVMOperand InArrayArg, FRigVMOperand InClonedArrayArg)
{
	return AddOp(FRigVMBinaryOp(ERigVMOpCode::ArrayClone, InArrayArg, InClonedArrayArg));
}

uint64 FRigVMByteCode::AddArrayIteratorOp(FRigVMOperand InArrayArg, FRigVMOperand InElementArg, FRigVMOperand InIndexArg, FRigVMOperand InCountArg, FRigVMOperand InRatioArg, FRigVMOperand InContinueArg)
{
	return AddOp(FRigVMSenaryOp(ERigVMOpCode::ArrayIterator, InArrayArg, InElementArg, InIndexArg, InCountArg, InRatioArg, InContinueArg));
}

uint64 FRigVMByteCode::AddArrayUnionOp(FRigVMOperand InArrayArg, FRigVMOperand InOtherArrayArg)
{
	return AddOp(FRigVMBinaryOp(ERigVMOpCode::ArrayUnion, InArrayArg, InOtherArrayArg));
}
	
uint64 FRigVMByteCode::AddArrayDifferenceOp(FRigVMOperand InArrayArg, FRigVMOperand InOtherArrayArg, FRigVMOperand InResultArrayArg)
{
	return AddOp(FRigVMTernaryOp(ERigVMOpCode::ArrayDifference, InArrayArg, InOtherArrayArg, InResultArrayArg));
}
	
uint64 FRigVMByteCode::AddArrayIntersectionOp(FRigVMOperand InArrayArg, FRigVMOperand InOtherArrayArg, FRigVMOperand InResultArrayArg)
{
	return AddOp(FRigVMTernaryOp(ERigVMOpCode::ArrayIntersection, InArrayArg, InOtherArrayArg, InResultArrayArg));
}
	
uint64 FRigVMByteCode::AddArrayReverseOp(FRigVMOperand InArrayArg)
{
	return AddOp(FRigVMUnaryOp(ERigVMOpCode::ArrayReverse, InArrayArg));
}

uint64 FRigVMByteCode::AddInvokeEntryOp(const FName& InEntryName)
{
	return AddOp(FRigVMInvokeEntryOp(InEntryName));
}

FRigVMOperandArray FRigVMByteCode::GetOperandsForOp(const FRigVMInstruction& InInstruction) const
{
	switch(InInstruction.OpCode)
	{
		case ERigVMOpCode::Execute_0_Operands:
		case ERigVMOpCode::Execute_1_Operands:
		case ERigVMOpCode::Execute_2_Operands:
		case ERigVMOpCode::Execute_3_Operands:
		case ERigVMOpCode::Execute_4_Operands:
		case ERigVMOpCode::Execute_5_Operands:
		case ERigVMOpCode::Execute_6_Operands:
		case ERigVMOpCode::Execute_7_Operands:
		case ERigVMOpCode::Execute_8_Operands:
		case ERigVMOpCode::Execute_9_Operands:
		case ERigVMOpCode::Execute_10_Operands:
		case ERigVMOpCode::Execute_11_Operands:
		case ERigVMOpCode::Execute_12_Operands:
		case ERigVMOpCode::Execute_13_Operands:
		case ERigVMOpCode::Execute_14_Operands:
		case ERigVMOpCode::Execute_15_Operands:
		case ERigVMOpCode::Execute_16_Operands:
		case ERigVMOpCode::Execute_17_Operands:
		case ERigVMOpCode::Execute_18_Operands:
		case ERigVMOpCode::Execute_19_Operands:
		case ERigVMOpCode::Execute_20_Operands:
		case ERigVMOpCode::Execute_21_Operands:
		case ERigVMOpCode::Execute_22_Operands:
		case ERigVMOpCode::Execute_23_Operands:
		case ERigVMOpCode::Execute_24_Operands:
		case ERigVMOpCode::Execute_25_Operands:
		case ERigVMOpCode::Execute_26_Operands:
		case ERigVMOpCode::Execute_27_Operands:
		case ERigVMOpCode::Execute_28_Operands:
		case ERigVMOpCode::Execute_29_Operands:
		case ERigVMOpCode::Execute_30_Operands:
		case ERigVMOpCode::Execute_31_Operands:
		case ERigVMOpCode::Execute_32_Operands:
		case ERigVMOpCode::Execute_33_Operands:
		case ERigVMOpCode::Execute_34_Operands:
		case ERigVMOpCode::Execute_35_Operands:
		case ERigVMOpCode::Execute_36_Operands:
		case ERigVMOpCode::Execute_37_Operands:
		case ERigVMOpCode::Execute_38_Operands:
		case ERigVMOpCode::Execute_39_Operands:
		case ERigVMOpCode::Execute_40_Operands:
		case ERigVMOpCode::Execute_41_Operands:
		case ERigVMOpCode::Execute_42_Operands:
		case ERigVMOpCode::Execute_43_Operands:
		case ERigVMOpCode::Execute_44_Operands:
		case ERigVMOpCode::Execute_45_Operands:
		case ERigVMOpCode::Execute_46_Operands:
		case ERigVMOpCode::Execute_47_Operands:
		case ERigVMOpCode::Execute_48_Operands:
		case ERigVMOpCode::Execute_49_Operands:
		case ERigVMOpCode::Execute_50_Operands:
		case ERigVMOpCode::Execute_51_Operands:
		case ERigVMOpCode::Execute_52_Operands:
		case ERigVMOpCode::Execute_53_Operands:
		case ERigVMOpCode::Execute_54_Operands:
		case ERigVMOpCode::Execute_55_Operands:
		case ERigVMOpCode::Execute_56_Operands:
		case ERigVMOpCode::Execute_57_Operands:
		case ERigVMOpCode::Execute_58_Operands:
		case ERigVMOpCode::Execute_59_Operands:
		case ERigVMOpCode::Execute_60_Operands:
		case ERigVMOpCode::Execute_61_Operands:
		case ERigVMOpCode::Execute_62_Operands:
		case ERigVMOpCode::Execute_63_Operands:
		case ERigVMOpCode::Execute_64_Operands:
		{
			return GetOperandsForExecuteOp(InInstruction);
		}
		case ERigVMOpCode::Copy:
		{
			const FRigVMCopyOp& Op = GetOpAt<FRigVMCopyOp>(InInstruction.ByteCodeIndex);
			return FRigVMOperandArray(&Op.Source, 2);
		}
		case ERigVMOpCode::Zero:
		case ERigVMOpCode::BoolFalse:
		case ERigVMOpCode::BoolTrue:
		case ERigVMOpCode::Increment:
		case ERigVMOpCode::Decrement:
		case ERigVMOpCode::ArrayReset:
		case ERigVMOpCode::ArrayReverse:
		{
			const FRigVMUnaryOp& Op = GetOpAt<FRigVMUnaryOp>(InInstruction.ByteCodeIndex);
			return FRigVMOperandArray(&Op.Arg, 1);
		}
		case ERigVMOpCode::Equals:
		case ERigVMOpCode::NotEquals:
		{
			const FRigVMComparisonOp& Op = GetOpAt<FRigVMComparisonOp>(InInstruction.ByteCodeIndex);
			return FRigVMOperandArray(&Op.A, 3);
		}
		case ERigVMOpCode::JumpAbsoluteIf:
		case ERigVMOpCode::JumpForwardIf:
		case ERigVMOpCode::JumpBackwardIf:
		{
			const FRigVMJumpIfOp& Op = GetOpAt<FRigVMJumpIfOp>(InInstruction.ByteCodeIndex);
			return FRigVMOperandArray(&Op.Arg, 1);
		}
		case ERigVMOpCode::BeginBlock:
		case ERigVMOpCode::ArrayGetNum:
		case ERigVMOpCode::ArraySetNum:
		case ERigVMOpCode::ArrayAppend:
		case ERigVMOpCode::ArrayClone:
		case ERigVMOpCode::ArrayRemove:
		case ERigVMOpCode::ArrayUnion:
		{
			const FRigVMBinaryOp& Op = GetOpAt<FRigVMBinaryOp>(InInstruction.ByteCodeIndex);
			return FRigVMOperandArray(&Op.ArgA, 2);
		}
		case ERigVMOpCode::ArrayAdd:
		case ERigVMOpCode::ArrayGetAtIndex:
		case ERigVMOpCode::ArraySetAtIndex:
		case ERigVMOpCode::ArrayInsert:
		case ERigVMOpCode::ArrayDifference:
		case ERigVMOpCode::ArrayIntersection:
		{
			const FRigVMTernaryOp& Op = GetOpAt<FRigVMTernaryOp>(InInstruction.ByteCodeIndex);
			return FRigVMOperandArray(&Op.ArgA, 3);
		}
		case ERigVMOpCode::ArrayFind:
		{
			const FRigVMQuaternaryOp& Op = GetOpAt<FRigVMQuaternaryOp>(InInstruction.ByteCodeIndex);
			return FRigVMOperandArray(&Op.ArgA, 4);
		}
		case ERigVMOpCode::ArrayIterator:
		{
			const FRigVMSenaryOp& Op = GetOpAt<FRigVMSenaryOp>(InInstruction.ByteCodeIndex);
			return FRigVMOperandArray(&Op.ArgA, 6);
		}
		case ERigVMOpCode::JumpAbsolute:
		case ERigVMOpCode::JumpForward:
		case ERigVMOpCode::JumpBackward:
		case ERigVMOpCode::ChangeType:
		case ERigVMOpCode::Exit:
		case ERigVMOpCode::EndBlock:
		case ERigVMOpCode::Invalid:
		default:
		{
			break;
		}
	}

	return FRigVMOperandArray();
}

TArray<int32> FRigVMByteCode::GetInstructionsForOperand(const FRigVMOperand& InOperand) const
{
	TArray<int32> InstructionIndices;
	
	const FRigVMInstructionArray Instructions = GetInstructions();
	for(int32 InstructionIndex = 0; InstructionIndex < GetNumInstructions(); InstructionIndex++)
	{
		if(GetOperandsForOp(Instructions[InstructionIndex]).Contains(InOperand))
		{
			InstructionIndices.Add(InstructionIndex);
		}

	}
	return InstructionIndices;
}

uint64 FRigVMByteCode::GetOpAlignment(ERigVMOpCode InOpCode) const
{
	switch (InOpCode)
	{
		case ERigVMOpCode::Execute_0_Operands:
		case ERigVMOpCode::Execute_1_Operands:
		case ERigVMOpCode::Execute_2_Operands:
		case ERigVMOpCode::Execute_3_Operands:
		case ERigVMOpCode::Execute_4_Operands:
		case ERigVMOpCode::Execute_5_Operands:
		case ERigVMOpCode::Execute_6_Operands:
		case ERigVMOpCode::Execute_7_Operands:
		case ERigVMOpCode::Execute_8_Operands:
		case ERigVMOpCode::Execute_9_Operands:
		case ERigVMOpCode::Execute_10_Operands:
		case ERigVMOpCode::Execute_11_Operands:
		case ERigVMOpCode::Execute_12_Operands:
		case ERigVMOpCode::Execute_13_Operands:
		case ERigVMOpCode::Execute_14_Operands:
		case ERigVMOpCode::Execute_15_Operands:
		case ERigVMOpCode::Execute_16_Operands:
		case ERigVMOpCode::Execute_17_Operands:
		case ERigVMOpCode::Execute_18_Operands:
		case ERigVMOpCode::Execute_19_Operands:
		case ERigVMOpCode::Execute_20_Operands:
		case ERigVMOpCode::Execute_21_Operands:
		case ERigVMOpCode::Execute_22_Operands:
		case ERigVMOpCode::Execute_23_Operands:
		case ERigVMOpCode::Execute_24_Operands:
		case ERigVMOpCode::Execute_25_Operands:
		case ERigVMOpCode::Execute_26_Operands:
		case ERigVMOpCode::Execute_27_Operands:
		case ERigVMOpCode::Execute_28_Operands:
		case ERigVMOpCode::Execute_29_Operands:
		case ERigVMOpCode::Execute_30_Operands:
		case ERigVMOpCode::Execute_31_Operands:
		case ERigVMOpCode::Execute_32_Operands:
		case ERigVMOpCode::Execute_33_Operands:
		case ERigVMOpCode::Execute_34_Operands:
		case ERigVMOpCode::Execute_35_Operands:
		case ERigVMOpCode::Execute_36_Operands:
		case ERigVMOpCode::Execute_37_Operands:
		case ERigVMOpCode::Execute_38_Operands:
		case ERigVMOpCode::Execute_39_Operands:
		case ERigVMOpCode::Execute_40_Operands:
		case ERigVMOpCode::Execute_41_Operands:
		case ERigVMOpCode::Execute_42_Operands:
		case ERigVMOpCode::Execute_43_Operands:
		case ERigVMOpCode::Execute_44_Operands:
		case ERigVMOpCode::Execute_45_Operands:
		case ERigVMOpCode::Execute_46_Operands:
		case ERigVMOpCode::Execute_47_Operands:
		case ERigVMOpCode::Execute_48_Operands:
		case ERigVMOpCode::Execute_49_Operands:
		case ERigVMOpCode::Execute_50_Operands:
		case ERigVMOpCode::Execute_51_Operands:
		case ERigVMOpCode::Execute_52_Operands:
		case ERigVMOpCode::Execute_53_Operands:
		case ERigVMOpCode::Execute_54_Operands:
		case ERigVMOpCode::Execute_55_Operands:
		case ERigVMOpCode::Execute_56_Operands:
		case ERigVMOpCode::Execute_57_Operands:
		case ERigVMOpCode::Execute_58_Operands:
		case ERigVMOpCode::Execute_59_Operands:
		case ERigVMOpCode::Execute_60_Operands:
		case ERigVMOpCode::Execute_61_Operands:
		case ERigVMOpCode::Execute_62_Operands:
		case ERigVMOpCode::Execute_63_Operands:
		case ERigVMOpCode::Execute_64_Operands:
		{
			static const uint64 Alignment = FRigVMExecuteOp::StaticStruct()->GetCppStructOps()->GetAlignment();
			return Alignment;
		}
		case ERigVMOpCode::Copy:
		{
			static const uint64 Alignment = FRigVMCopyOp::StaticStruct()->GetCppStructOps()->GetAlignment();
			return Alignment;
		}
		case ERigVMOpCode::Zero:
		case ERigVMOpCode::BoolFalse:
		case ERigVMOpCode::BoolTrue:
		case ERigVMOpCode::Increment:
		case ERigVMOpCode::Decrement:
		case ERigVMOpCode::ArrayReset:
		case ERigVMOpCode::ArrayReverse:
		{
			static const uint64 Alignment = FRigVMUnaryOp::StaticStruct()->GetCppStructOps()->GetAlignment();
			return Alignment;
		}
		case ERigVMOpCode::Equals:
		case ERigVMOpCode::NotEquals:
		{
			static const uint64 Alignment = FRigVMComparisonOp::StaticStruct()->GetCppStructOps()->GetAlignment();
			return Alignment;
		}
		case ERigVMOpCode::JumpAbsolute:
		case ERigVMOpCode::JumpForward:
		case ERigVMOpCode::JumpBackward:
		{
			static const uint64 Alignment = FRigVMJumpOp::StaticStruct()->GetCppStructOps()->GetAlignment();
			return Alignment;
		}
		case ERigVMOpCode::JumpAbsoluteIf:
		case ERigVMOpCode::JumpForwardIf:
		case ERigVMOpCode::JumpBackwardIf:
		{
			static const uint64 Alignment = FRigVMJumpIfOp::StaticStruct()->GetCppStructOps()->GetAlignment();
			return Alignment;
		}
		case ERigVMOpCode::ChangeType:
		{
			checkNoEntry();
			return 0;
		}
		case ERigVMOpCode::Exit:
		{
			static const uint64 Alignment = FRigVMBaseOp::StaticStruct()->GetCppStructOps()->GetAlignment();
			return Alignment;
		}
		case ERigVMOpCode::BeginBlock:
		case ERigVMOpCode::ArrayGetNum:
		case ERigVMOpCode::ArraySetNum:
		case ERigVMOpCode::ArrayAppend:
		case ERigVMOpCode::ArrayClone:
		case ERigVMOpCode::ArrayRemove:
		case ERigVMOpCode::ArrayUnion:
		{
			static const uint64 Alignment = FRigVMBinaryOp::StaticStruct()->GetCppStructOps()->GetAlignment();
			return Alignment;
		}
		case ERigVMOpCode::ArrayAdd:
		case ERigVMOpCode::ArrayGetAtIndex:
		case ERigVMOpCode::ArraySetAtIndex:
		case ERigVMOpCode::ArrayInsert:
		case ERigVMOpCode::ArrayDifference:
		case ERigVMOpCode::ArrayIntersection:
		{				
			static const uint64 Alignment = FRigVMTernaryOp::StaticStruct()->GetCppStructOps()->GetAlignment();
			return Alignment;
		}
		case ERigVMOpCode::ArrayFind:
		{				
			static const uint64 Alignment = FRigVMQuaternaryOp::StaticStruct()->GetCppStructOps()->GetAlignment();
			return Alignment;
		}
		case ERigVMOpCode::ArrayIterator:
		{				
			static const uint64 Alignment = FRigVMSenaryOp::StaticStruct()->GetCppStructOps()->GetAlignment();
			return Alignment;
		}
		case ERigVMOpCode::EndBlock:
		{
			static const uint64 Alignment = FRigVMBaseOp::StaticStruct()->GetCppStructOps()->GetAlignment();
			return Alignment;
		}
		case ERigVMOpCode::InvokeEntry:
		{
			static const uint64 Alignment = FRigVMInvokeEntryOp::StaticStruct()->GetCppStructOps()->GetAlignment();
			return Alignment;
		}
		case ERigVMOpCode::Invalid:
		{
			ensure(false);
			return 0;
		}
	}
	return 0;
}

uint64 FRigVMByteCode::GetOperandAlignment() const
{
	static const uint64 OperandAlignment = !FRigVMOperand::StaticStruct()->GetCppStructOps()->GetAlignment();
	return OperandAlignment;
}

void FRigVMByteCode::AlignByteCode()
{
	if (bByteCodeIsAligned)
	{
		return;
	}

	if (ByteCode.Num() == 0)
	{
		return;
	}

	FRigVMInstructionArray Instructions(*this, false);
	uint64 BytesToReserve = ByteCode.Num();

	for (int32 InstructionIndex = 0; InstructionIndex < Instructions.Num(); InstructionIndex++)
	{
		const FRigVMInstruction& Instruction = Instructions[InstructionIndex];
		BytesToReserve += GetOpAlignment(Instruction.OpCode);

		if (Instruction.OpCode >= ERigVMOpCode::Execute_0_Operands && Instruction.OpCode < ERigVMOpCode::Execute_64_Operands)
		{
			BytesToReserve += GetOperandAlignment();
		}
	}

	TArray<uint8> AlignedByteCode;
	AlignedByteCode.Reserve(BytesToReserve);
	AlignedByteCode.AddZeroed(ByteCode.Num());

	uint64 ShiftedBytes = 0;
	for (int32 InstructionIndex = 0; InstructionIndex < Instructions.Num(); InstructionIndex++)
	{
		const FRigVMInstruction& Instruction = Instructions[InstructionIndex];
		uint64 OriginalByteCodeIndex = Instruction.ByteCodeIndex;
		uint64 AlignedByteCodeIndex = OriginalByteCodeIndex + ShiftedBytes;
		uint64 OpAlignment = GetOpAlignment(Instruction.OpCode);

		if (OpAlignment > 0)
		{
			while (!IsAligned(&AlignedByteCode[AlignedByteCodeIndex], OpAlignment))
			{
				AlignedByteCode[AlignedByteCodeIndex] = (uint8)Instruction.OpCode;
				AlignedByteCodeIndex++;
				ShiftedBytes++;
				AlignedByteCode.AddZeroed(1);
			}
		}

		uint64 NumBytesToCopy = GetOpNumBytesAt(OriginalByteCodeIndex, false);
		for (int32 ByteIndex = 0; ByteIndex < NumBytesToCopy; ByteIndex++)
		{
			AlignedByteCode[AlignedByteCodeIndex + ByteIndex] = ByteCode[OriginalByteCodeIndex + ByteIndex];
		}

		if (Instruction.OpCode >= ERigVMOpCode::Execute_0_Operands && Instruction.OpCode < ERigVMOpCode::Execute_64_Operands)
		{
			AlignedByteCodeIndex += NumBytesToCopy;

			static const uint64 OperandAlignment = GetOperandAlignment();
			if (OperandAlignment > 0)
			{
				while (!IsAligned(&AlignedByteCode[AlignedByteCodeIndex], OperandAlignment))
				{
					AlignedByteCodeIndex++;
					ShiftedBytes++;
					AlignedByteCode.AddZeroed(1);
				}
			}

			FRigVMExecuteOp ExecuteOp;
			uint8* ExecuteOpPtr = (uint8*)&ExecuteOp;
			for (int32 ByteIndex = 0; ByteIndex < sizeof(FRigVMExecuteOp); ByteIndex++)
			{
				ExecuteOpPtr[ByteIndex] = ByteCode[OriginalByteCodeIndex + ByteIndex];
			}

			OriginalByteCodeIndex += NumBytesToCopy;
			NumBytesToCopy = sizeof(FRigVMOperand) * ExecuteOp.GetOperandCount();

			for (int32 ByteIndex = 0; ByteIndex < NumBytesToCopy; ByteIndex++)
			{
				AlignedByteCode[AlignedByteCodeIndex + ByteIndex] = ByteCode[OriginalByteCodeIndex + ByteIndex];
			}
		}
	}

	Swap(ByteCode, AlignedByteCode);
	bByteCodeIsAligned = true;
}

#if WITH_EDITOR

UObject* FRigVMByteCode::GetSubjectForInstruction(int32 InInstructionIndex) const
{
	if (SubjectPerInstruction.IsValidIndex(InInstructionIndex))
	{
		return SubjectPerInstruction[InInstructionIndex];
	}
	return nullptr;
}

int32 FRigVMByteCode::GetFirstInstructionIndexForSubject(UObject* InSubject) const
{
	const TArray<int32>& InstructionIndices = GetAllInstructionIndicesForSubject(InSubject);
	if (InstructionIndices.Num() > 0)
	{
		return InstructionIndices[0];
	}
	return INDEX_NONE;
}

const TArray<int32>& FRigVMByteCode::GetAllInstructionIndicesForSubject(UObject* InSubject) const
{
	if (const TArray<int32>* InstructionIndices = SubjectToInstructions.Find(InSubject))
	{
		return *InstructionIndices;
	}
	return EmptyInstructionIndices;
}

FString FRigVMByteCode::GetCallPathForInstruction(int32 InInstructionIndex) const
{
	if (CallPathPerInstruction.IsValidIndex(InInstructionIndex))
	{
		return CallPathPerInstruction[InInstructionIndex];
	}
	return FString();
}

int32 FRigVMByteCode::GetFirstInstructionIndexForCallPath(const FString& InCallPath, bool bStartsWith, bool bEndsWith) const
{
	const TArray<int32> InstructionIndices = GetAllInstructionIndicesForCallPath(InCallPath, bStartsWith, bEndsWith);
	if (InstructionIndices.Num() > 0)
	{
		return InstructionIndices[0];
	}
	return INDEX_NONE;
}

TArray<int32> FRigVMByteCode::GetAllInstructionIndicesForCallPath(const FString& InCallPath, bool bStartsWith, bool bEndsWith) const
{
	if(InCallPath.IsEmpty())
	{
		return EmptyInstructionIndices;
	}

	TArray<int32> MatchedInstructions;
	if (const TArray<int32>* InstructionIndices = CallPathToInstructions.Find(InCallPath))
	{
		MatchedInstructions.Append(*InstructionIndices);
	}
	
	if(bStartsWith || bEndsWith)
	{
		const FString CallPathStart = FString::Printf(TEXT("%s|"), *InCallPath);
		const FString CallPathEnd = FString::Printf(TEXT("|%s"), *InCallPath);
		for(int32 InstructionIndex = 0; InstructionIndex < CallPathPerInstruction.Num(); InstructionIndex++)
		{
			if(bStartsWith)
			{
				if(CallPathPerInstruction[InstructionIndex].StartsWith(CallPathStart))
				{
					MatchedInstructions.Add(InstructionIndex);
				}
			}
			else if(bEndsWith)
			{
				if(CallPathPerInstruction[InstructionIndex].EndsWith(CallPathEnd))
				{
					MatchedInstructions.Add(InstructionIndex);
				}
			}
		}
	}

	return MatchedInstructions;
}

int32 FRigVMByteCode::GetFirstInstructionIndexForCallstack(const TArray<UObject*>& InCallstack) const
{
	const TArray<int32>& InstructionIndices = GetAllInstructionIndicesForCallstack(InCallstack);
	if (InstructionIndices.Num() > 0)
	{
		return InstructionIndices[0];
	}
	return INDEX_NONE;
}

const TArray<int32>& FRigVMByteCode::GetAllInstructionIndicesForCallstack(const TArray<UObject*>& InCallstack) const
{
	if(InCallstack.IsEmpty())
	{
		return EmptyInstructionIndices;
	}

	const uint32 Hash = GetCallstackHash(InCallstack);
	if(const TArray<int32>* Instructions = CallstackHashToInstructions.Find(Hash))
	{
		return *Instructions;
	}
	
	return EmptyInstructionIndices;
}

void FRigVMByteCode::SetSubject(int32 InInstructionIndex, const FString& InCallPath, const TArray<UObject*>& InCallstack)
{
	UObject* Subject = InCallstack.Last();
	if (SubjectPerInstruction.Num() <= InInstructionIndex)
	{
		SubjectPerInstruction.AddZeroed(1 + InInstructionIndex - SubjectPerInstruction.Num());
	}
	SubjectPerInstruction[InInstructionIndex] = Subject;
	SubjectToInstructions.FindOrAdd(Subject).AddUnique(InInstructionIndex);

	if (CallPathPerInstruction.Num() <= InInstructionIndex)
	{
		CallPathPerInstruction.AddZeroed(1 + InInstructionIndex - CallPathPerInstruction.Num());
	}
	CallPathPerInstruction[InInstructionIndex] = InCallPath;
	CallPathToInstructions.FindOrAdd(InCallPath).AddUnique(InInstructionIndex);

	if (CallstackPerInstruction.Num() <= InInstructionIndex)
	{
		CallstackPerInstruction.AddZeroed(1 + InInstructionIndex - CallstackPerInstruction.Num());
	}
	CallstackPerInstruction[InInstructionIndex] = InCallstack;

	if (CallstackHashPerInstruction.Num() <= InInstructionIndex)
	{
		CallstackHashPerInstruction.AddZeroed(1 + InInstructionIndex - CallstackHashPerInstruction.Num());
	}
	CallstackHashPerInstruction[InInstructionIndex] = GetCallstackHash(InCallstack);

	for(int32 CallstackLength = InCallstack.Num(); CallstackLength > 0; CallstackLength--)
	{
		UObject* const* DataPtr = &InCallstack[InCallstack.Num() - CallstackLength];
		TArrayView<UObject* const> View(DataPtr, CallstackLength);
		uint32 Hash = GetCallstackHash(View);
		CallstackHashToInstructions.FindOrAdd(Hash).AddUnique(InInstructionIndex);
	}
}

const TArray<UObject*>* FRigVMByteCode::GetCallstackForInstruction(int32 InInstructionIndex) const
{
	if (CallstackPerInstruction.IsValidIndex(InInstructionIndex))
	{
		return &CallstackPerInstruction[InInstructionIndex];
	}
	return nullptr;
}

uint32 FRigVMByteCode::GetCallstackHashForInstruction(int32 InInstructionIndex) const
{
	if (CallstackHashPerInstruction.IsValidIndex(InInstructionIndex))
	{
		return CallstackHashPerInstruction[InInstructionIndex];
	}
	return 0;
}

uint32 FRigVMByteCode::GetCallstackHash(const TArray<UObject*>& InCallstack)
{
	UObject*const * DataPtr = nullptr;
	if(InCallstack.Num() > 0)
	{
		DataPtr = &InCallstack[0];
	}
	TArrayView<UObject* const> View(DataPtr, InCallstack.Num());
	return GetCallstackHash(View);
}

uint32 FRigVMByteCode::GetCallstackHash(const TArrayView<UObject* const>& InCallstack)
{
	uint32 Hash = GetTypeHash(InCallstack.Num());
	for(const UObject* Object : InCallstack)
	{
		Hash = HashCombine(Hash, GetTypeHash(Object));
	}
	return Hash;
}

void FRigVMByteCode::SetOperandsForInstruction(int32 InInstructionIndex, const FRigVMOperandArray& InputOperands,
	const FRigVMOperandArray& OutputOperands)
{
	if (InputOperandsPerInstruction.Num() <= InInstructionIndex)
	{
		InputOperandsPerInstruction.AddZeroed(1 + InInstructionIndex - InputOperandsPerInstruction.Num());
	}
	InputOperandsPerInstruction[InInstructionIndex].Reset();
	InputOperandsPerInstruction[InInstructionIndex].Reserve(InputOperands.Num());

	for(int32 OperandIndex = 0; OperandIndex < InputOperands.Num(); OperandIndex++)
	{
		// we are only interested in memory here which can change over time
		if(InputOperands[OperandIndex].GetMemoryType() == ERigVMMemoryType::Literal)
		{
			continue;;
		}
		InputOperandsPerInstruction[InInstructionIndex].Add(InputOperands[OperandIndex]);
	}

	if (OutputOperandsPerInstruction.Num() <= InInstructionIndex)
	{
		OutputOperandsPerInstruction.AddZeroed(1 + InInstructionIndex - OutputOperandsPerInstruction.Num());
	}
	OutputOperandsPerInstruction[InInstructionIndex].Reset();
	OutputOperandsPerInstruction[InInstructionIndex].Reserve(OutputOperands.Num());

	for(int32 OperandIndex = 0; OperandIndex < OutputOperands.Num(); OperandIndex++)
	{
		// we are only interested in memory here which can change over time
		if(OutputOperands[OperandIndex].GetMemoryType() == ERigVMMemoryType::Literal)
		{
			continue;;
		}
		OutputOperandsPerInstruction[InInstructionIndex].Add(OutputOperands[OperandIndex]);
	}
}

#endif

