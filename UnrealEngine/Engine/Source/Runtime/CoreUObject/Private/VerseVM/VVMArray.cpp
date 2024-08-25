// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMArray.h"
#include "VerseVM/Inline/VVMAbstractVisitorInline.h"
#include "VerseVM/Inline/VVMArrayBaseInline.h"
#include "VerseVM/Inline/VVMCellInline.h"
#include "VerseVM/VVMCppClassInfo.h"
#include "VerseVM/VVMMarkStackVisitor.h"

namespace Verse
{
DEFINE_DERIVED_VCPPCLASSINFO(VTypeArray)
DEFINE_TRIVIAL_VISIT_REFERENCES(VTypeArray);
TGlobalTrivialEmergentTypePtr<&VTypeArray::StaticCppClassInfo> VTypeArray::GlobalTrivialEmergentType;

DEFINE_DERIVED_VCPPCLASSINFO(VArray);
DEFINE_TRIVIAL_VISIT_REFERENCES(VArray);
TGlobalTrivialEmergentTypePtr<&VArray::StaticCppClassInfo> VArray::GlobalTrivialEmergentType;

void VArray::SerializeImpl(VArray*& This, FAllocationContext Context, FAbstractVisitor& Visitor)
{
	if (Visitor.IsLoading())
	{
		uint64 ScratchNumValues = 0;
		Visitor.BeginArray(TEXT("Values"), ScratchNumValues);
		This = &VArray::New(Context, (uint32)ScratchNumValues);
		Visitor.Visit(This->GetData(), This->GetData() + This->Num());
		Visitor.EndArray();
	}
	else
	{
		uint64 ScratchNumValues = This->Num();
		Visitor.BeginArray(TEXT("Values"), ScratchNumValues);
		Visitor.Visit(This->GetData(), This->GetData() + This->Num());
		Visitor.EndArray();
	}
}

} // namespace Verse
#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)
