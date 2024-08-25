// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMNativeFunction.h"
#include "VerseVM/Inline/VVMCellInline.h"
#include "VerseVM/VVMCppClassInfo.h"

namespace Verse
{

DEFINE_DERIVED_VCPPCLASSINFO(VNativeFunction);
TGlobalTrivialEmergentTypePtr<&VNativeFunction::StaticCppClassInfo> VNativeFunction::GlobalTrivialEmergentType;

template <typename TVisitor>
void VNativeFunction::VisitReferencesImpl(TVisitor& Visitor)
{
	Visitor.Visit(ParentScope, TEXT("ParentScope"));
}

} // namespace Verse
#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)