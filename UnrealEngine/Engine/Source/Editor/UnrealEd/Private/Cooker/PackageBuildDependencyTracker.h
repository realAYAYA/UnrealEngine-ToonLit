// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/CriticalSection.h"
#include "Misc/PackageAccessTracking.h"
#include "Templates/UnrealTemplate.h"
#include "UObject/ObjectHandle.h"

#if UE_WITH_PACKAGE_ACCESS_TRACKING

struct FBuildDependencyAccessData
{
	FName ReferencedPackage;
	const ITargetPlatform* TargetPlatform;
	friend uint32 GetTypeHash(const FBuildDependencyAccessData& Data)
	{
		return HashCombine(GetTypeHash(Data.ReferencedPackage), GetTypeHash(Data.TargetPlatform));
	}
	bool operator==(const FBuildDependencyAccessData& Other) const
	{
		return ReferencedPackage == Other.ReferencedPackage && TargetPlatform == Other.TargetPlatform;
	}
	bool operator!=(const FBuildDependencyAccessData& Other) const
	{
		return !(*this == Other); 
	}
};

class FPackageBuildDependencyTracker: public FNoncopyable
{
public:
	static FPackageBuildDependencyTracker& Get() { return Singleton; }

	void DumpStats() const;
	TArray<FBuildDependencyAccessData> GetAccessDatas(FName ReferencerPackage) const;

private:
	FPackageBuildDependencyTracker();
	virtual ~FPackageBuildDependencyTracker();

	/** Track object reference reads */
	static void StaticOnObjectHandleRead(UObject* ReadObject);

	mutable FCriticalSection RecordsLock;
	TMap<FName, TSet<FBuildDependencyAccessData>> Records;
	FName LastReferencer = NAME_None;
	FBuildDependencyAccessData LastAccessData{ NAME_None, nullptr };
	TSet<FBuildDependencyAccessData>* LastReferencerSet = nullptr;
	FDelegateHandle ObjectHandleReadHandle;
	static FPackageBuildDependencyTracker Singleton;
};

#endif // UE_WITH_PACKAGE_ACCESS_TRACKING

void DumpBuildDependencyTrackerStats();
