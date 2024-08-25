// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMVar.h"
#include "VerseVM/Inline/VVMAbstractVisitorInline.h"
#include "VerseVM/Inline/VVMCellInline.h"
#include "VerseVM/VVMCppClassInfo.h"
#include "VerseVM/VVMMarkStackVisitor.h"

namespace Verse
{

DEFINE_DERIVED_VCPPCLASSINFO(VVar);
TGlobalTrivialEmergentTypePtr<&VVar::StaticCppClassInfo> VVar::GlobalTrivialEmergentType;

template <typename TVisitor>
void VVar::VisitReferencesImpl(TVisitor& Visitor)
{
	Visitor.Visit(Value, TEXT("Value"));
}

void VVar::ToStringImpl(FStringBuilderBase& Builder, FAllocationContext Context, const FCellFormatter& Formatter)
{
	Get(Context).ToString(Builder, Context, Formatter);
}

} // namespace Verse
#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)
