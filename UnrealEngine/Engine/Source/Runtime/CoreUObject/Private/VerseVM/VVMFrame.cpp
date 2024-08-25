// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMFrame.h"
#include "VerseVM/Inline/VVMAbstractVisitorInline.h"
#include "VerseVM/Inline/VVMCellInline.h"
#include "VerseVM/VVMCppClassInfo.h"
#include "VerseVM/VVMMarkStackVisitor.h"
#include "VerseVM/VVMProcedure.h"

namespace Verse
{

DEFINE_DERIVED_VCPPCLASSINFO(VFrame);
TGlobalTrivialEmergentTypePtr<&VFrame::StaticCppClassInfo> VFrame::GlobalTrivialEmergentType;

template <typename TVisitor>
void VFrame::VisitReferencesImpl(TVisitor& Visitor)
{
	if constexpr (TVisitor::bIsAbstractVisitor)
	{
		Visitor.Visit(ReturnEffectToken, TEXT("ReturnEffectToken"));
		Visitor.Visit(Procedure, TEXT("Procedure"));
		if (ReturnKind == EReturnKind::Value)
		{
			Visitor.Visit(Return.Value, TEXT("ReturnSlot"));
		}
		Visitor.Visit(CallerFrame, TEXT("CallerFrame"));
		uint64 ScratchNumRegisters = NumRegisters;
		Visitor.BeginArray(TEXT("Registers"), ScratchNumRegisters);
		Visitor.Visit(Registers, Registers + NumRegisters);
		Visitor.EndArray();
	}
	else
	{
		Visitor.Visit(ReturnEffectToken, TEXT("ReturnEffectToken"));
		Visitor.Visit(Procedure, TEXT("Procedure"));
		if (ReturnKind == EReturnKind::Value)
		{
			Visitor.Visit(Return.Value, TEXT("ReturnSlot"));
		}
		Visitor.Visit(CallerFrame, TEXT("CallerFrame"));
		Visitor.Visit(Registers, Registers + NumRegisters);
	}
}

} // namespace Verse
#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)
