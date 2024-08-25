// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMBytecodePrinting.h"
#include "Containers/Map.h"
#include "Containers/UnrealString.h"
#include "VerseVM/Inline/VVMValueInline.h"
#include "VerseVM/VVMBytecode.h"
#include "VerseVM/VVMBytecodeDispatcher.h"
#include "VerseVM/VVMLog.h"
#include "VerseVM/VVMProcedure.h"
#include "VerseVM/VVMValuePrinting.h"
#include <inttypes.h>

namespace Verse
{
namespace
{
struct FBytecodeCellFormatter : public FDefaultCellFormatter
{
	TMap<VCell*, FString> CellSymbolMap;

	// FCellFormatter interface.
	virtual void Append(FStringBuilderBase& Builder, FAllocationContext Context, VCell& Cell) const override
	{
		if (const FString* Symbol = CellSymbolMap.Find(&Cell))
		{
			Builder.Append(*Symbol);
		}
		else
		{
			FDefaultCellFormatter::Append(Builder, Context, Cell);
		}
	}
};

struct FJumpTargetHandler
{
	TMap<const FOp*, FString> JumpTargetToLabelIndexMap;

	template <typename OpType>
	void operator()(const OpType& Op)
	{
		Op.ForEachJump([&](const FLabelOffset& LabelOffset) {
			const FOp* TargetOp = LabelOffset.GetLabeledPC();
			if (!JumpTargetToLabelIndexMap.Contains(TargetOp))
			{
				JumpTargetToLabelIndexMap.Add(TargetOp, FString::Printf(TEXT("L%u"), JumpTargetToLabelIndexMap.Num()));
			}
		});
	}
};

struct FBytecodePrinter
{
	FBytecodePrinter(FAllocationContext Context, VProcedure& Procedure)
		: Context(Context)
		, Procedure(Procedure)
	{
		CellFormatter.CellSymbolMap.Add(&Procedure, TEXT("F"));

		JumpTargetHandler.JumpTargetToLabelIndexMap.Add(Procedure.GetOpsBegin(), TEXT("Entry"));
	}

	FString Print()
	{
		// Do a pre-pass over the procedure's ops to find jump targets.
		DispatchOps(Procedure.GetOpsBegin(), Procedure.GetOpsEnd(), JumpTargetHandler);

		// Print the procedure definition.
		String += FString::Printf(
			TEXT("%s = procedure(0x%" PRIxPTR "):\n"),
			*CellFormatter.ToString(Context, Procedure),
			&Procedure);

		// Print the procedure constant table.
		for (uint32 ConstantIndex = 0; ConstantIndex < Procedure.NumConstants; ++ConstantIndex)
		{
			String += FString::Printf(TEXT("    c%u = %s\n"),
				ConstantIndex,
				*ToString(Context, CellFormatter, Procedure.GetConstant(FConstantIndex{ConstantIndex})));
		}

		// Print info about the procedure's frame.
		if (Procedure.NumRegisters)
		{
			String += FString::Printf(TEXT("    # Frame contains %u registers: r0..r%u\n"),
				Procedure.NumRegisters,
				Procedure.NumRegisters - 1);
		}
		if (Procedure.NumParameters)
		{
			String += FString::Printf(TEXT("    # Frame contains %u parameters: r0..r%u\n"),
				Procedure.NumParameters,
				Procedure.NumParameters - 1);
		}
		else
		{
			String += FString::Printf(TEXT("    # Frame contains 0 parameters\n"));
		}

		// Print the procedure's ops.
		DispatchOps(Procedure.GetOpsBegin(), Procedure.GetOpsEnd(), *this);
		PrintLabelIfNeeded(Procedure.GetOpsEnd());

		return MoveTemp(String);
	}

	void PrintLabelIfNeeded(const FOp* Op)
	{
		// If this op is the target of a jump, print a label before it.
		if (FString* Label = JumpTargetHandler.JumpTargetToLabelIndexMap.Find(Op))
		{
			String += TEXT("  ");
			String += *Label;
			String += TEXT(":\n");
		}
	}

	template <typename OpType>
	void operator()(const OpType& Op)
	{
		PrintLabelIfNeeded(&Op);

		String += FString::Printf(TEXT("    %5u | "), Procedure.BytecodeOffset(Op));

		PrintOpWithOperands(Op);

		String += TEXT('\n');
	}

private:
	FAllocationContext Context;
	VProcedure& Procedure;
	FString String;

	FBytecodeCellFormatter CellFormatter;

	FJumpTargetHandler JumpTargetHandler;

	void PrintRegister(FRegisterIndex Register)
	{
		String += FString::Printf(TEXT("r%u"), Register.Index);
	}

	void PrintValueOperand(FValueOperand ValueOperand)
	{
		if (ValueOperand.IsConstant())
		{
			FConstantIndex ConstantIndex = ValueOperand.AsConstant();
			String += FString::Printf(TEXT("c%u="), ConstantIndex.Index);
			String += ToString(Context, CellFormatter, Procedure.GetConstant(ConstantIndex));
		}
		else
		{
			PrintRegister(ValueOperand.AsRegister());
		}
	}

	template <typename OpType>
	void PrintOpWithOperands(const OpType& Op)
	{
		FString Separator = TEXT("");
		auto ArgSeparator = [&] {
			FString Result = Separator;
			Separator = TEXT(", ");
			return Result;
		};

		bool bPrintedOp = false;
		auto PrintOp = [&] {
			if (!bPrintedOp)
			{
				String += ToString(Op.Opcode);
				String += TEXT("(");
				bPrintedOp = true;
			}
		};

		// Right now we just assume that Defs come before Uses, but we could rework this
		// if this ever breaks printing.
		Op.ForEachOperandWithName([&](EOperandRole Role, auto& Operand, const char* Name) {
			using DecayedType = std::decay_t<decltype(Operand)>;
			if constexpr (std::is_same_v<DecayedType, FValueOperand> || std::is_same_v<DecayedType, FRegisterIndex>)
			{
				switch (Role)
				{
					case EOperandRole::ClobberDef:
						PrintValueOperand(Operand);
						String += TEXT(" <- ");
						break;
					case EOperandRole::UnifyDef:
						PrintValueOperand(Operand);
						String += TEXT(" = ");
						break;
					case EOperandRole::Use:
						PrintOp();
						String += ArgSeparator();
						String += FString::Printf(TEXT("%s: "), *FString(Name));
						PrintValueOperand(Operand);
						break;
					case EOperandRole::Immediate:
					default:
						VERSE_UNREACHABLE();
				}
			}
			else
			{
				V_DIE_IF(Role != EOperandRole::Immediate);
				PrintOp();
				String += ArgSeparator();
				String += FString::Printf(TEXT("%s: "), *FString(Name));
				// We can safely assume that all immediates are wrapped in a `TWriteBarrier`.
				String += ToString(Context, FDefaultCellFormatter{}, *Operand.Get());
			}
		});

		PrintOp();

		Op.ForEachJumpWithName([&](const FLabelOffset& Label, const char* Name) {
			FString* TargetLabel = JumpTargetHandler.JumpTargetToLabelIndexMap.Find(Label.GetLabeledPC());
			check(TargetLabel);
			String += ArgSeparator();
			String += FString::Printf(TEXT("%s: %s"), *FString(Name), **TargetLabel);
		});

		String += TEXT(")");
	}
};
} // namespace
} // namespace Verse

FString Verse::PrintProcedure(FAllocationContext Context, VProcedure& Procedure)
{
	FBytecodePrinter Printer{Context, Procedure};
	return Printer.Print();
}
#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)
