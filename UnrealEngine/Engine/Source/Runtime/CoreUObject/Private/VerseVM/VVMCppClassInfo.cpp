// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_VERSE_VM || defined(__INTELLISENSE__)
#include "VerseVM/VVMCppClassInfo.h"
#include "Async/Mutex.h"
#include "Async/UniqueLock.h"
#include "Containers/Map.h"
#include "Containers/UnrealString.h"
#include "HAL/Thread.h"
#include <atomic>

namespace Verse
{
FString VCppClassInfo::DebugName() const
{
	return Name;
}

namespace Registry
{
std::atomic<VCppClassInfoRegister*> List;
UE::FMutex Mutex;
TMap<FStringView, VCppClassInfo*> Map;

// The map lock should be held while calling this method.
void UpdateMap()
{
	for (VCppClassInfoRegister* Head = List.exchange(nullptr); Head; Head = Head->Next)
	{
		Map.Add(FStringView(Head->CppClassInfo->Name), Head->CppClassInfo);
	}
}
} // namespace Registry

VCppClassInfoRegister::VCppClassInfoRegister(VCppClassInfo* InCppClassInfo)
{
	CppClassInfo = InCppClassInfo;
	for (Next = Registry::List.load(std::memory_order_relaxed);;)
	{
		if (LIKELY(Registry::List.compare_exchange_weak(Next, this, std::memory_order_release, std::memory_order_relaxed)))
		{
			break;
		}
	}
}

VCppClassInfoRegister::~VCppClassInfoRegister()
{
	// This needs to be improved in the future
	UE::TUniqueLock Lock(Registry::Mutex);
	Registry::UpdateMap();
	VCppClassInfo** Found = Registry::Map.Find(FStringView(CppClassInfo->Name));
	if (Found != nullptr && *Found == CppClassInfo)
	{
		Registry::Map.Remove(CppClassInfo->Name);
	}
}

VCppClassInfo* VCppClassInfoRegistry::GetCppClassInfo(FStringView Name)
{
	UE::TUniqueLock Lock(Registry::Mutex);
	Registry::UpdateMap();
	VCppClassInfo** Found = Registry::Map.Find(Name);
	return Found != nullptr ? *Found : nullptr;
}

} // namespace Verse
#endif // WITH_VERSE_VM || defined(__INTELLISENSE__)