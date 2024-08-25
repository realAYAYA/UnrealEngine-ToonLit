// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMFailureContext.h"
#include "VVMFrame.h"
#include "VerseVM/Inline/VVMAbstractVisitorInline.h"
#include "VerseVM/Inline/VVMCellInline.h"
#include "VerseVM/VVMCppClassInfo.h"
#include "VerseVM/VVMMarkStackVisitor.h"

namespace Verse
{

DEFINE_DERIVED_VCPPCLASSINFO(VFailureContext);
TGlobalTrivialEmergentTypePtr<&VFailureContext::StaticCppClassInfo> VFailureContext::GlobalTrivialEmergentType;

template <typename TVisitor>
void VFailureContext::VisitReferencesImpl(TVisitor& Visitor)
{
	Visitor.Visit(FirstChild, TEXT("FirstChild"));
	Visitor.Visit(Next, TEXT("Next"));
	Visitor.Visit(Prev, TEXT("Prev"));
	Visitor.Visit(Parent, TEXT("Parent"));
	Visitor.Visit(Frame, TEXT("Frame"));
	Visitor.Visit(IncomingEffectToken, TEXT("IncomingEffectToken"));
	Visitor.Visit(BeforeThenEffectToken, TEXT("BeforeThenEffectToken"));
	Visitor.Visit(DoneEffectToken, TEXT("DoneEffectToken"));
}

} // namespace Verse
#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)