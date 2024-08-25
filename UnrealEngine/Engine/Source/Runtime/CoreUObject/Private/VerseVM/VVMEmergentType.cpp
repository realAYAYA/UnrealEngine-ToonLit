// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMEmergentType.h"
#include "VerseVM/Inline/VVMAbstractVisitorInline.h"
#include "VerseVM/Inline/VVMCellInline.h"
#include "VerseVM/VVMCppClassInfo.h"
#include "VerseVM/VVMMarkStackVisitor.h"
#include "VerseVM/VVMShape.h"

namespace Verse
{
DEFINE_DERIVED_VCPPCLASSINFO(VEmergentType);

template <typename TVisitor>
void VEmergentType::VisitReferencesImpl(TVisitor& Visitor)
{
	Visitor.Visit(Shape, TEXT("Shape"));
	Visitor.Visit(Type, TEXT("Type"));
}

} // namespace Verse
#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)
