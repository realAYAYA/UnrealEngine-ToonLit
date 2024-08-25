// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMUTF8String.h"
#include "Async/UniqueLock.h"
#include "Containers/StringConv.h"
#include "Containers/StringView.h"
#include "VerseVM/Inline/VVMAbstractVisitorInline.h"
#include "VerseVM/Inline/VVMCellInline.h"
#include "VerseVM/Inline/VVMUTF8StringInline.h"
#include "VerseVM/VVMCppClassInfo.h"
#include "VerseVM/VVMMarkStackVisitor.h"
#include "VerseVM/VVMValuePrinting.h"

namespace Verse
{
UE::FMutex VStringInternPool::Mutex;

VUniqueString& VStringInternPool::Intern(FAllocationContext Context, FUtf8StringView String)
{
	UE::TUniqueLock Lock(Mutex);
	if (TWeakBarrier<VUniqueString>* UniqueStringEntry = UniqueStrings.Find(String))
	{
		// If we found an entry, but GC clears the weak reference before we can use it, fall through
		// to add a new entry for the string.
		if (VUniqueString* UniqueString = UniqueStringEntry->Get(Context))
		{
			return *UniqueString;
		}
	}

	VUniqueString& UniqueString = VUniqueString::Make(Context, String);
	UniqueStrings.Add(TWeakBarrier<VUniqueString>(UniqueString));
	return UniqueString;
}

void VStringInternPool::ConductCensus()
{
	UE::TUniqueLock Lock(Mutex);
	for (auto It = UniqueStrings.CreateIterator(); It; ++It)
	{
		// If the cell that the string is allocated in is not marked (i.e. non-live) during GC marking
		// the weak reference will be removed and thus we can remove the map entry from the pool as well.
		if (It->ClearWeakDuringCensus())
		{
			It.RemoveCurrent();
		}
	}
}

uint32 VUTF8String::GetTypeHashImpl()
{
	return GetTypeHash(*this);
}

template <typename TVisitor>
void VUTF8String::VisitReferencesImpl(TVisitor& Visitor)
{
	if constexpr (TVisitor::bIsAbstractVisitor)
	{
		if (Visitor.IsLoading())
		{
			V_DIE("VUTF8String isn't mutable and can not be loaded through the abstract visitors, use the Serialization method");
		}
		else
		{
			FString ScratchString(AsStringView());
			Visitor.Visit(ScratchString, TEXT("Value"));
		}
	}
}

void VUTF8String::ToStringImpl(FStringBuilderBase& Builder, FAllocationContext Context, const FCellFormatter& Formatter)
{
	Builder.Append(TEXT("\"")).Append(AsCString()).Append(TEXT("\""));
}

void VUTF8String::SerializeImpl(VUTF8String*& This, FAllocationContext Context, FAbstractVisitor& Visitor)
{
	if (Visitor.IsLoading())
	{
		FString ScratchString;
		Visitor.Visit(ScratchString, TEXT("Value"));
		This = &VUTF8String::New(Context, TCHAR_TO_UTF8(*ScratchString));
	}
	else
	{
		FString ScratchString(This->AsStringView());
		Visitor.Visit(ScratchString, TEXT("Value"));
	}
}

DEFINE_DERIVED_VCPPCLASSINFO(VUTF8String);
TGlobalTrivialEmergentTypePtr<&VUTF8String::StaticCppClassInfo> VUTF8String::GlobalTrivialEmergentType;

DEFINE_DERIVED_VCPPCLASSINFO(VUniqueString);
DEFINE_TRIVIAL_VISIT_REFERENCES(VUniqueString);
TGlobalTrivialEmergentTypePtr<&VUniqueString::StaticCppClassInfo> VUniqueString::GlobalTrivialEmergentType;

void VUniqueString::ToStringImpl(FStringBuilderBase& Builder, FAllocationContext Context, const FCellFormatter& Formatter)
{
	Builder.Append(TEXT("\"")).Append(AsCString()).Append(TEXT("\""));
}

void VUniqueString::SerializeImpl(VUniqueString*& This, FAllocationContext Context, FAbstractVisitor& Visitor)
{
	if (Visitor.IsLoading())
	{
		FString ScratchString;
		Visitor.Visit(ScratchString, TEXT("Value"));
		This = &VUniqueString::New(Context, TCHAR_TO_UTF8(*ScratchString));
	}
	else
	{
		FString ScratchString(This->AsStringView());
		Visitor.Visit(ScratchString, TEXT("Value"));
	}
}

TLazyInitialized<VStringInternPool> VUniqueString::StringPool;

bool VUniqueStringSet::Equals(const VUniqueStringSet& Other) const
{
	if (Num() != Other.Num())
	{
		return false;
	}
	for (const TWriteBarrier<VUniqueString>& String : *this)
	{
		if (!Other.IsValidId(Other.FindId(String->AsStringView())))
		{
			return false;
		}
	}
	return true;
}

VUniqueStringSet& VUniqueStringSetInternPool::Intern(FAllocationContext Context, const TSet<VUniqueString*>& InSet)
{
	UE::TUniqueLock Lock(Mutex);
	if (TWeakBarrier<VUniqueStringSet>* UniqueSet = Sets.Find({InSet}))
	{
		// If we found an entry, but GC clears the weak reference before we can use it, fall through
		// to add a new entry for the set.
		if (VUniqueStringSet* CurrentSet = UniqueSet->Get(Context))
		{
			return *CurrentSet;
		}
	}

	VUniqueStringSet& UniqueStringSet = VUniqueStringSet::Make(Context, InSet);
	Sets.Add({UniqueStringSet});
	return UniqueStringSet;
}

void VUniqueStringSetInternPool::ConductCensus()
{
	UE::TUniqueLock Lock(Mutex);
	for (auto It = Sets.CreateIterator(); It; ++It)
	{
		// If the cell that the string is allocated in is not marked (i.e. non-live) during GC marking
		// the weak reference will be removed and thus we can remove the map entry from the pool as well.
		if (It->ClearWeakDuringCensus())
		{
			It.RemoveCurrent();
		}
	}
}

UE::FMutex VUniqueStringSetInternPool::Mutex;

DEFINE_DERIVED_VCPPCLASSINFO(VUniqueStringSet);
TGlobalTrivialEmergentTypePtr<&VUniqueStringSet::StaticCppClassInfo> VUniqueStringSet::GlobalTrivialEmergentType;

TLazyInitialized<VUniqueStringSetInternPool> VUniqueStringSet::Pool;

template <typename TVisitor>
void VUniqueStringSet::VisitReferencesImpl(TVisitor& Visitor)
{
	// We still have to mark each of the strings in the set as being used.
	Visitor.Visit(Strings, TEXT("Strings"));
}

void VUniqueStringSet::ToStringImpl(FStringBuilderBase& Builder, FAllocationContext Context, const FCellFormatter& Formatter)
{
	int Index = 0;
	for (auto& CurrentString : *this)
	{
		if (Index++ != 0)
		{
			Builder.Append(TEXT(", "));
		}
		Builder.Append(TEXT("("));
		Formatter.Append(Builder, Context, *CurrentString);
		Builder.Append(TEXT(")"));
	}
}

void VUniqueStringSet::SerializeImpl(VUniqueStringSet*& This, FAllocationContext Context, FAbstractVisitor& Visitor)
{
	if (Visitor.IsLoading())
	{
		uint64 ScratchNumValues = 0;
		Visitor.BeginArray(TEXT("Strings"), ScratchNumValues);
		TSet<VUniqueString*> Strings;
		for (uint32 Index = (uint32)ScratchNumValues; Index > 0; --Index)
		{
			VCell* ScratchCell;
			Visitor.Visit(ScratchCell, TEXT(""));
			Strings.Add(&ScratchCell->StaticCast<VUniqueString>());
		}
		Visitor.EndArray();
		This = &VUniqueStringSet::New(Context, Strings);
	}
	else
	{
		uint64 ScratchNumValues = This->Num();
		Visitor.BeginArray(TEXT("Strings"), ScratchNumValues);
		Visitor.Visit(This->Strings.begin(), This->Strings.end());
		Visitor.EndArray();
	}
}

} // namespace Verse
#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)
