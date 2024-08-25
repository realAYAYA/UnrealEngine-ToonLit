// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMArrayBase.h"
#include "Templates/TypeHash.h"
#include "VerseVM/Inline/VVMAbstractVisitorInline.h"
#include "VerseVM/Inline/VVMArrayBaseInline.h"
#include "VerseVM/Inline/VVMEqualInline.h"
#include "VerseVM/Inline/VVMValueInline.h"
#include "VerseVM/VVMCppClassInfo.h"
#include "VerseVM/VVMMarkStackVisitor.h"
#include "VerseVM/VVMMutableArray.h"
#include "VerseVM/VVMOpResult.h"

namespace Verse
{
DEFINE_DERIVED_VCPPCLASSINFO(VArrayBase);
TGlobalTrivialEmergentTypePtr<&VArrayBase::StaticCppClassInfo> VArrayBase::GlobalTrivialEmergentType;

bool VArrayBase::EqualImpl(FRunningContext Context, VCell* Other, const TFunction<void(::Verse::VValue, ::Verse::VValue)>& HandlePlaceholder)
{
	if (!Other->IsA<VArrayBase>())
	{
		return false;
	}

	VArrayBase& OtherArray = Other->StaticCast<VArrayBase>();
	if (Num() != OtherArray.Num())
	{
		return false;
	}
	for (uint32 Index = 0, End = Num(); Index < End; ++Index)
	{
		if (!VValue::Equal(Context, GetValue(Index), OtherArray.GetValue(Index), HandlePlaceholder))
		{
			return false;
		}
	}
	return true;
}

FOpResult VArrayBase::MeltImpl(FRunningContext Context)
{
	VMutableArray& MeltedArray = VMutableArray::New(Context, Num());
	for (uint32 I = 0; I < Num(); ++I)
	{
		FOpResult ValueResult = VValue::Melt(Context, GetValue(I));
		if (ValueResult.Kind == FOpResult::ShouldSuspend)
		{
			return ValueResult;
		}
		MeltedArray.AddValue(Context, ValueResult.Value);
	}
	return {FOpResult::Normal, VValue(MeltedArray)};
}

uint32 VArrayBase::GetTypeHashImpl()
{
	const TWriteBarrier<VValue>* Ptr = GetData();
	const uint32 Size = Num();
	return ::GetArrayHash(Ptr, Size);
}

void VArrayBase::ToStringImpl(FStringBuilderBase& Builder, FAllocationContext Context, const FCellFormatter& Formatter)
{
	for (uint32 I = 0; I < Num(); ++I)
	{
		if (I > 0)
		{
			Builder.Append(TEXT(", "));
		}
		GetValue(I).ToString(Builder, Context, Formatter);
	}
}

VArrayBase::FConstIterator VArrayBase::begin() const
{
	return GetData();
}

VArrayBase::FConstIterator VArrayBase::end() const
{
	return GetData() + Num();
}

} // namespace Verse
#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)
