// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Containers/RingBuffer.h"
#include "Containers/Set.h"
#include "CookRequests.h"
#include "CookTypes.h"
#include "HAL/CriticalSection.h"
#include "HAL/Platform.h"
#include "Misc/ScopeLock.h"
#include "Templates/UnrealTemplate.h"
#include "UObject/NameTypes.h"
#include "UObject/UObjectArray.h"

class ITargetPlatform;
class UPackage;
namespace UE::Cook { struct FInstigator; }
namespace UE::Cook { struct FPackageData; }
namespace UE::Cook { struct FRecompileShaderRequest; }

namespace UE::Cook
{

struct FPackageDatas;

template<typename Type>
struct FThreadSafeQueue
{
private:
	mutable FCriticalSection SynchronizationObject; // made this mutable so this class can have const functions and still be thread safe
	TRingBuffer<Type> Items;
public:
	void Enqueue(const Type& Item)
	{
		FScopeLock ScopeLock(&SynchronizationObject);
		Items.Add(Item);
	}
		
	void Enqueue(Type&& Item)
	{
		FScopeLock ScopeLock(&SynchronizationObject);
		Items.Add(MoveTempIfPossible(Item));
	}

	void EnqueueUnique(const Type& Item)
	{
		FScopeLock ScopeLock(&SynchronizationObject);
		for (const Type& Existing : Items)
		{
			if (Existing == Item)
			{
				return;
			}
		}
		Items.PushBack(Item);
	}

	bool Dequeue(Type* Result)
	{
		FScopeLock ScopeLock(&SynchronizationObject);
		if (Items.Num())
		{
			*Result = Items.PopFrontValue();
			return true;
		}
		return false;
	}

	void DequeueAll(TArray<Type>& Results)
	{
		FScopeLock ScopeLock(&SynchronizationObject);
		Results.Reserve(Results.Num() + Items.Num());
		while (!Items.IsEmpty())
		{
			Results.Add(Items.PopFrontValue());
		}
	}

	bool HasItems() const
	{
		FScopeLock ScopeLock(&SynchronizationObject);
		return Items.Num() > 0;
	}

	void Remove(const Type& Item)
	{
		FScopeLock ScopeLock(&SynchronizationObject);
		Items.Remove(Item);
	}

	void CopyItems(const TArray<Type>& InItems) const
	{
		FScopeLock ScopeLock(&SynchronizationObject);
		Items.Empty(InItems.Num());
		for (const Type& Item : InItems)
		{
			Items.PushBack(Item);
		}
	}

	int Num() const
	{
		FScopeLock ScopeLock(&SynchronizationObject);
		return Items.Num();
	}

	void Empty()
	{
		FScopeLock ScopeLock(&SynchronizationObject);
		Items.Empty();
	}
};

/** Simple thread safe proxy for TSet<FName> */
template <typename T>
class FThreadSafeSet
{
	TSet<T> InnerSet;
	FCriticalSection SetCritical;
public:
	void Add(T InValue)
	{
		FScopeLock SetLock(&SetCritical);
		InnerSet.Add(InValue);
	}
	bool AddUnique(T InValue)
	{
		FScopeLock SetLock(&SetCritical);
		if (!InnerSet.Contains(InValue))
		{
			InnerSet.Add(InValue);
			return true;
		}
		return false;
	}
	bool Contains(T InValue)
	{
		FScopeLock SetLock(&SetCritical);
		return InnerSet.Contains(InValue);
	}
	void Remove(T InValue)
	{
		FScopeLock SetLock(&SetCritical);
		InnerSet.Remove(InValue);
	}
	void Empty()
	{
		FScopeLock SetLock(&SetCritical);
		InnerSet.Empty();
	}

	void GetValues(TSet<T>& OutSet)
	{
		FScopeLock SetLock(&SetCritical);
		OutSet.Append(InnerSet);
	}
};

struct FThreadSafeUnsolicitedPackagesList
{
	void AddCookedPackage(const FFilePlatformRequest& PlatformRequest);
	void GetPackagesForPlatformAndRemove(const ITargetPlatform* Platform, TArray<FName>& PackageNames);
	void Empty();

private:
	FCriticalSection				SyncObject;
	TArray<FFilePlatformRequest>	CookedPackages;
};

struct FPackageTracker : public FUObjectArray::FUObjectCreateListener, public FUObjectArray::FUObjectDeleteListener
{
public:
	FPackageTracker(UCookOnTheFlyServer& InCOTFS);
	~FPackageTracker();

	/** Returns all packages that have been loaded since the last time GetNewPackages was called */
	TMap<UPackage*, FInstigator> GetNewPackages();

	/**
	 * Copy all LoadedPackages into NewPackages. Called when reachability of all packages needs to be recalculated
	 * because a new session platform was added.
	 */
	void MarkLoadedPackagesAsNew();

	virtual void NotifyUObjectCreated(const class UObjectBase* Object, int32 Index) override;
	virtual void NotifyUObjectDeleted(const class UObjectBase* Object, int32 Index) override;
	virtual void OnUObjectArrayShutdown() override;

	/** Swap all ITargetPlatform* stored on this instance according to the mapping in @param Remap. */
	void RemapTargetPlatforms(const TMap<ITargetPlatform*, ITargetPlatform*>& Remap);

	UCookOnTheFlyServer& COTFS;

	FThreadSafeUnsolicitedPackagesList UnsolicitedCookedPackages;
	FThreadSafeQueue<FRecompileShaderRequest> RecompileRequests;

	/** Packages to never cook - entries are localpaths in FPaths::MakeStandardFilename format. */
	FThreadSafeSet<FName> NeverCookPackageList;
	FThreadSafeSet<FName> UncookedEditorOnlyPackages; // set of packages that have been rejected due to being referenced by editor-only properties
	TFastPointerMap<const ITargetPlatform*, TSet<FName>> PlatformSpecificNeverCookPackages;

	// Thread-safe enumeration of loaded package. 
	// A lock is held during enumeration, keep code simple and optimal so the lock is released as fast as possible.
	template <typename FunctionType>
	void ForEachLoadedPackage(FunctionType Function)
	{
		FReadScopeLock ScopeLock(Lock);
		for (UPackage* Package : LoadedPackages)
		{
			Function(Package);
		}
	}
	void AddExpectedNeverLoadPackages(TArrayView<FName> PackageNames)
	{
		FWriteScopeLock ScopeLock(Lock);
		ExpectedNeverLoadPackages.Append(PackageNames);
	}
	void ClearExpectedNeverLoadPackages()
	{
		FWriteScopeLock ScopeLock(Lock);
		ExpectedNeverLoadPackages.Empty();
	}
private:
	void InitializeTracking();

	// Protects data for thread-safety
	FRWLock Lock;

	// This is a complete list of currently loaded UPackages
	TFastPointerSet<UPackage*> LoadedPackages;
	TSet<FName> ExpectedNeverLoadPackages;

	// This list contains the UPackages loaded since last call to GetNewPackages
	TMap<UPackage*, FInstigator> NewPackages;
	bool bTrackingInitialized = false;
};

} // namespace UE::Cook
