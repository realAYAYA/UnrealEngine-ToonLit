// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "VerseVM/VVMCVars.h"
#include "VerseVM/VVMClass.h"
#include "VerseVM/VVMEmergentTypeCreator.h"
#include "VerseVM/VVMPackage.h"
#include "VerseVM/VVMShape.h"
#include "VerseVM/VVMTypeCreator.h"

namespace Verse
{
struct VShape;

inline bool FEmergentTypesCacheKeyFuncs::Matches(FEmergentTypesCacheKeyFuncs::KeyInitType A, FEmergentTypesCacheKeyFuncs::KeyInitType B)
{
	return A == B;
}

inline bool FEmergentTypesCacheKeyFuncs::Matches(FEmergentTypesCacheKeyFuncs::KeyInitType A, const VUniqueStringSet& B)
{
	return *(A.Get()) == B;
}

inline uint32 FEmergentTypesCacheKeyFuncs::GetKeyHash(FEmergentTypesCacheKeyFuncs::KeyInitType Key)
{
	return GetTypeHash(Key);
}

inline uint32 FEmergentTypesCacheKeyFuncs::GetKeyHash(const VUniqueStringSet& Key)
{
	return GetTypeHash(Key);
}

inline VClass& VClass::New(FAllocationContext Context, VUTF8String* Name, EKind Kind, VConstructor& Constructor, const TArray<VClass*>& Inherited, VPackage* Scope)
{
	const size_t NumBytes = offsetof(VClass, Inherited) + Inherited.Num() * sizeof(Inherited[0]);
	return *new (Context.AllocateFastCell(NumBytes)) VClass(Context, Name, Kind, Constructor, Inherited, Scope);
}

inline VClass::VClass(FAllocationContext Context, VUTF8String* InName, EKind InKind, VConstructor& InConstructor, const TArray<VClass*>& InInherited, VPackage* InScope)
	: VType(Context, &GlobalTrivialEmergentType.Get(Context))
	, ClassName(Context, InName)
	, Scope(Context, InScope)
	, Kind(InKind)
	, NumInherited(InInherited.Num())
{
	if (InInherited.IsEmpty())
	{
		Constructor.Set(Context, InConstructor);
	}
	else
	{
		// Elements of this class override later superclasses, which override earlier superclasses.
		TSet<VUniqueString*> Fields;
		TArray<VConstructor::VEntry> Entries;
		Entries.Reserve(InConstructor.NumEntries);
		Extend(Fields, Entries, InConstructor);
		for (uint32 Index = InInherited.Num(); Index-- > 0;)
		{
			Extend(Fields, Entries, *InInherited[Index]->Constructor.Get());
		}
		Constructor.Set(Context, VConstructor::New(Context, Entries));
	}

	for (uint32 Index = 0; Index < NumInherited; ++Index)
	{
		new (&Inherited[Index]) TWriteBarrier<VClass>(Context, InInherited[Index]);
	}
}

} // namespace Verse
#endif // WITH_VERSE_VM
