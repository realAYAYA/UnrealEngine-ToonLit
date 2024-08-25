// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMProcedure.h"
#include "VerseVM/Inline/VVMAbstractVisitorInline.h"
#include "VerseVM/Inline/VVMCellInline.h"
#include "VerseVM/VVMBytecodeOps.h"
#include "VerseVM/VVMBytecodesAndCaptures.h"
#include "VerseVM/VVMCppClassInfo.h"
#include "VerseVM/VVMLog.h"
#include "VerseVM/VVMMarkStackVisitor.h"

namespace Verse
{

DEFINE_DERIVED_VCPPCLASSINFO(VProcedure);
TGlobalTrivialEmergentTypePtr<&VProcedure::StaticCppClassInfo> VProcedure::GlobalTrivialEmergentType;

VProcedure::~VProcedure()
{
	// NOTE: (yiliang.siew) This could be raised to a location such as in `VProgram` when that
	// exists and each opcode store only the index + size to index into that array, so that we don't
	// need to store a separate `TArray` of operand values per-opcode struct.
	for (const FOp* CurrentOp = GetOpsBegin(); CurrentOp != GetOpsEnd();)
	{
		checkf(CurrentOp != nullptr, TEXT("The current opcode was invalid!"));
		switch (CurrentOp->Opcode)
		{
#define VISIT_OP(Name)                                                                \
	case EOpcode::Name:                                                               \
	{                                                                                 \
		const FOp##Name* CurrentDerivedOp = static_cast<const FOp##Name*>(CurrentOp); \
		CurrentDerivedOp->~FOp##Name();                                               \
		CurrentOp = BitCast<const FOp*>(CurrentDerivedOp + 1);                        \
		break;                                                                        \
	}
			VERSE_ENUM_OPS(VISIT_OP)
#undef VISIT_OP
			default:
				V_DIE("Invalid opcode encountered: %u during function destruction!", static_cast<FOpcodeInt>(CurrentOp->Opcode));
				break;
		}
	}
}

template <typename TVisitor>
void VProcedure::VisitReferencesImpl(TVisitor& Visitor)
{
	if constexpr (TVisitor::bIsAbstractVisitor)
	{
		uint64 ScratchNumConstants = NumConstants;
		Visitor.BeginArray(TEXT("Constants"), ScratchNumConstants);
		Visitor.Visit(Constants, Constants + NumConstants);
		Visitor.EndArray();
	}
	else
	{
		Visitor.Visit(Constants, Constants + NumConstants);
	}

	// We also need to mark the immediate operands for each opcode to make sure that the GC doesn't sweep them.
	for (FOp* CurrentOp = GetOpsBegin(); CurrentOp != GetOpsEnd();)
	{
		checkf(CurrentOp != nullptr, TEXT("The current opcode was invalid!"));
		switch (CurrentOp->Opcode)
		{
#define VISIT_OP(Name)                                                                                               \
	case EOpcode::Name:                                                                                              \
	{                                                                                                                \
		FOp##Name* CurrentDerivedOp = static_cast<FOp##Name*>(CurrentOp);                                            \
		CurrentDerivedOp->ForEachOperand([&Visitor](EOperandRole Role, auto& Operand) {                              \
			using DecayedType = std::decay_t<decltype(Operand)>;                                                     \
			if constexpr (std::is_same_v<DecayedType, FValueOperand> || std::is_same_v<DecayedType, FRegisterIndex>) \
			{                                                                                                        \
				return;                                                                                              \
			}                                                                                                        \
			else if (Role == EOperandRole::Immediate)                                                                \
			{                                                                                                        \
				Visitor.Visit(Operand, TEXT(#Name));                                                                 \
			}                                                                                                        \
		});                                                                                                          \
		CurrentOp = BitCast<FOp*>(CurrentDerivedOp + 1);                                                             \
		break;                                                                                                       \
	}
			VERSE_ENUM_OPS(VISIT_OP)
#undef VISIT_OP
			default:
				V_DIE("Invalid opcode encountered: %u during marking!", static_cast<FOpcodeInt>(CurrentOp->Opcode));
				break;
		}
	}
}

} // namespace Verse

#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)
