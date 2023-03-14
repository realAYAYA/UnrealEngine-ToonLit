// Copyright Epic Games, Inc. All Rights Reserved.

#include "PackageBuildDependencyTracker.h"

#include "HAL/Platform.h"
#include "Logging/LogMacros.h"
#include "Misc/PackageAccessTrackingOps.h"
#include "UObject/Package.h"

#if UE_WITH_PACKAGE_ACCESS_TRACKING

DEFINE_LOG_CATEGORY_STATIC(LogPackageBuildDependencyTracker, Log, All);

FPackageBuildDependencyTracker FPackageBuildDependencyTracker::Singleton;

void FPackageBuildDependencyTracker::DumpStats() const
{
	FScopeLock RecordsScopeLock(&RecordsLock);
	uint64 ReferencingPackageCount = 0;
	uint64 ReferenceCount = 0;
	for (const TPair<FName, TSet<FBuildDependencyAccessData>>& PackageAccessRecord : Records)
	{
		++ReferencingPackageCount;
		for (const FBuildDependencyAccessData& AccessedData : PackageAccessRecord.Value)
		{
			++ReferenceCount;
		}
	}
	UE_LOG(LogPackageBuildDependencyTracker, Display, TEXT("Package Accesses (%u referencing packages with a total of %u unique accesses)"), ReferencingPackageCount, ReferenceCount);

	constexpr bool bDetailedDump = false;
	if (bDetailedDump)
	{
		UE_LOG(LogPackageBuildDependencyTracker, Display, TEXT("========================================================================="));
		for (const TPair<FName, TSet<FBuildDependencyAccessData>>& PackageAccessRecord : Records)
		{
			UE_LOG(LogPackageBuildDependencyTracker, Display, TEXT("%s:"), *PackageAccessRecord.Key.ToString());
			for (const FBuildDependencyAccessData& AccessedData : PackageAccessRecord.Value)
			{
				UE_LOG(LogPackageBuildDependencyTracker, Display, TEXT("    %s"), *AccessedData.ReferencedPackage.ToString());
			}
		}
	}
}

TArray<FBuildDependencyAccessData> FPackageBuildDependencyTracker::GetAccessDatas(FName ReferencerPackage) const
{
	FScopeLock RecordsScopeLock(&Singleton.RecordsLock);
	const TSet<FBuildDependencyAccessData>* ReferencerSet = Records.Find(ReferencerPackage);
	if (!ReferencerSet)
	{
		return TArray<FBuildDependencyAccessData>();
	}
	return ReferencerSet->Array();
}

FPackageBuildDependencyTracker::FPackageBuildDependencyTracker()
{
	ObjectHandleReadHandle = AddObjectHandleReadCallback(FObjectHandleReadDelegate::CreateStatic(StaticOnObjectHandleRead));
}

FPackageBuildDependencyTracker::~FPackageBuildDependencyTracker()
{
	RemoveObjectHandleReadCallback(ObjectHandleReadHandle);
}

void FPackageBuildDependencyTracker::StaticOnObjectHandleRead(UObject* ReadObject)
{
	if (!ReadObject)
	{
		return;
	}

	PackageAccessTracking_Private::FTrackedData* AccumulatedScopeData = PackageAccessTracking_Private::FPackageAccessRefScope::GetCurrentThreadAccumulatedData();
	if (!AccumulatedScopeData)
	{
		return;
	}

	if (AccumulatedScopeData->BuildOpName.IsNone())
	{
		return;
	}

	if (!ReadObject->HasAnyFlags(RF_Public))
	{
		return;
	}

	FName Referencer = AccumulatedScopeData->PackageName;
	FName Referenced = ReadObject->GetOutermost()->GetFName();
	if (Referencer == Referenced)
	{
		return;
	}

	if (AccumulatedScopeData->OpName == PackageAccessTrackingOps::NAME_NoAccessExpected)
	{
		UE_LOG(LogPackageBuildDependencyTracker, Warning, TEXT("Object %s is referencing object %s inside of a NAME_NoAccessExpected scope. Programmer should narrow the scope or debug the reference."),
			*Referencer.ToString(), *Referenced.ToString());
	}

	LLM_SCOPE_BYNAME(TEXTVIEW("TObjectPtr"));

	FBuildDependencyAccessData AccessData{Referenced, AccumulatedScopeData->TargetPlatform};
	FScopeLock RecordsScopeLock(&Singleton.RecordsLock);
	if (Referencer == Singleton.LastReferencer)
	{
		if (AccessData != Singleton.LastAccessData)
		{
			Singleton.LastAccessData = AccessData;
			Singleton.LastReferencerSet->Add(AccessData);
		}
	}
	else
	{
		Singleton.LastAccessData = AccessData;
		Singleton.LastReferencer = Referencer;
		Singleton.LastReferencerSet = &Singleton.Records.FindOrAdd(Referencer);
		Singleton.LastReferencerSet->Add(AccessData);
	}
}

void DumpBuildDependencyTrackerStats()
{
	FPackageBuildDependencyTracker::Get().DumpStats();
}

#else
void DumpBuildDependencyTrackerStats()
{
}

#endif // UE_WITH_OBJECT_HANDLE_TRACKING
