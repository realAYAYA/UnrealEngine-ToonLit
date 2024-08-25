// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMGlobalTrivialEmergentTypePtr.h"
#include "VerseVM/Inline/VVMAbstractVisitorInline.h"
#include "VerseVM/VVMMarkStackVisitor.h"

namespace Verse
{

void FGlobalTrivialEmergentTypePtrRoot::Visit(FAbstractVisitor& Visitor)
{
	VisitImpl(Visitor);
}

void FGlobalTrivialEmergentTypePtrRoot::Visit(FMarkStackVisitor& Visitor)
{
	VisitImpl(Visitor);
}

template <typename TVisitor>
void FGlobalTrivialEmergentTypePtrRoot::VisitImpl(TVisitor& Visitor)
{
	Visitor.Visit(EmergentType, TEXT("EmergentType"));
}

} // namespace Verse
#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)