// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMPlaceholder.h"
#include "VerseVM/Inline/VVMAbstractVisitorInline.h"
#include "VerseVM/Inline/VVMCellInline.h"
#include "VerseVM/VVMCppClassInfo.h"
#include "VerseVM/VVMMarkStackVisitor.h"

namespace Verse
{

DEFINE_DERIVED_VCPPCLASSINFO(VPlaceholder);
TGlobalTrivialEmergentTypePtr<&VPlaceholder::StaticCppClassInfo> VPlaceholder::GlobalTrivialEmergentType;

template <typename TVisitor>
void VPlaceholder::VisitReferencesImpl(TVisitor& Visitor)
{
	Visitor.Visit(Value, TEXT("Value"));
}

} // namespace Verse
#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)
