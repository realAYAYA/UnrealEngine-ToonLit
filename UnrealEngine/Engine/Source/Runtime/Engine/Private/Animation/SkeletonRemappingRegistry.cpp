// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/SkeletonRemappingRegistry.h"
#include "Misc/ScopeRWLock.h"
#include "Animation/SkeletonRemapping.h"
#include "Animation/Skeleton.h"

namespace UE::Anim
{

FSkeletonRemappingRegistry* GSkeletonRemappingRegistry = nullptr;
FSkeletonRemapping DefaultMapping;

struct FSkeletonRemappingRegistryPrivate
{
	static FDelegateHandle PostGarbageCollectHandle;

	static void HandlePostGarbageCollect()
	{
		// Compact the registry on GC
		if(GSkeletonRemappingRegistry)
		{
			FRWScopeLock WriteLock(GSkeletonRemappingRegistry->MappingsLock, SLT_Write);
			
			for(auto Iter = GSkeletonRemappingRegistry->Mappings.CreateIterator(); Iter; ++Iter)
			{
				const FSkeletonRemappingRegistry::FWeakSkeletonPair& SkeletonPair = Iter.Key();
				if(SkeletonPair.Key.Get() == nullptr || SkeletonPair.Value.Get() == nullptr)
				{
					Iter.RemoveCurrent();
				}
			}

			for(auto Iter = GSkeletonRemappingRegistry->PerSkeletonMappings.CreateIterator(); Iter; ++Iter)
			{
				const TWeakObjectPtr<const USkeleton>& SkeletonPtr = Iter.Key();
				if(SkeletonPtr.Get() == nullptr)
				{
					Iter.RemoveCurrent();
				}
			}
		}
	}
};

FDelegateHandle FSkeletonRemappingRegistryPrivate::PostGarbageCollectHandle;

FSkeletonRemappingRegistry& FSkeletonRemappingRegistry::Get()
{
	checkf(GSkeletonRemappingRegistry, TEXT("Skeleton remapping registry is not instanced. It is only valid to access this while the engine module is loaded."));
	return *GSkeletonRemappingRegistry;
}

void FSkeletonRemappingRegistry::Init()
{
	GSkeletonRemappingRegistry = new FSkeletonRemappingRegistry();
	FSkeletonRemappingRegistryPrivate::PostGarbageCollectHandle = FCoreUObjectDelegates::GetPostGarbageCollect().AddStatic(&FSkeletonRemappingRegistryPrivate::HandlePostGarbageCollect);
}

void FSkeletonRemappingRegistry::Destroy()
{
	FCoreUObjectDelegates::GetPostGarbageCollect().Remove(FSkeletonRemappingRegistryPrivate::PostGarbageCollectHandle);
	delete GSkeletonRemappingRegistry;
	GSkeletonRemappingRegistry = nullptr;
}

const FSkeletonRemapping& FSkeletonRemappingRegistry::GetRemapping(const USkeleton* InSourceSkeleton, const USkeleton* InTargetSkeleton)
{
	if(InSourceSkeleton == InTargetSkeleton)
	{
		return DefaultMapping;
	}
	
	const FWeakSkeletonPair Pair(InSourceSkeleton, InTargetSkeleton);

	{
		FRWScopeLock ReadLock(MappingsLock, SLT_ReadOnly);
		const TSharedPtr<FSkeletonRemapping>* ExistingMapping = Mappings.Find(Pair);
		if(ExistingMapping && ExistingMapping->IsValid())
		{
			return *(*ExistingMapping).Get();
		}
	}

	// No existing, so create a new mapping
	TSharedPtr<FSkeletonRemapping> NewMapping = MakeShared<FSkeletonRemapping>(InSourceSkeleton, InTargetSkeleton);
	
	{
		FRWScopeLock WriteLock(MappingsLock, SLT_Write);

		// Add to global mapping
		Mappings.Add(Pair, NewMapping);

		// Add to per-skeleton mappings
		PerSkeletonMappings.Add(InSourceSkeleton, NewMapping);
		PerSkeletonMappings.Add(InTargetSkeleton, NewMapping);

		return *NewMapping;
	}
}

void FSkeletonRemappingRegistry::RefreshMappings(USkeleton* InSkeleton)
{
	TArray<TSharedPtr<FSkeletonRemapping>> ExistingMappings;
	{
		FRWScopeLock ReadLock(MappingsLock, SLT_ReadOnly);
		PerSkeletonMappings.MultiFind(InSkeleton, ExistingMappings);
	}
	{
		FRWScopeLock WriteLock(MappingsLock, SLT_Write);
		for(const TSharedPtr<FSkeletonRemapping>& ExistingMapping : ExistingMappings)
		{
			ExistingMapping->RegenerateMapping();
		}
	}
}

}
