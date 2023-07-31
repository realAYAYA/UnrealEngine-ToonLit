// Copyright Epic Games, Inc. All Rights Reserved.
#include "Chaos/TrackedGeometryManager.h"
#include "Chaos/ChaosArchive.h"
#include "Misc/ScopeLock.h"

namespace Chaos
{
	FTrackedGeometryManager& FTrackedGeometryManager::Get()
	{
		static FTrackedGeometryManager Singleton;
		return Singleton;
	}

	void FTrackedGeometryManager::DumpMemoryUsage(FOutputDevice* Ar) const
	{
		struct FMemInfo
		{
			uint32 NumBytes;
			FString DebugInfo;
			FString ToString;

			bool operator<(const FMemInfo& Other) const { return NumBytes < Other.NumBytes; }
		};

		TArray<FMemInfo> MemEntries;
		uint32 TotalBytes = 0;
		for (const auto& Itr : SharedGeometry)
		{
			FMemInfo Info;
			Info.DebugInfo = Itr.Value;
			TArray<uint8> Data;
			FMemoryWriter MemAr(Data);
			FChaosArchive ChaosAr(MemAr);
			FImplicitObject* NonConst = const_cast<FImplicitObject*>(Itr.Key.Get());	//only doing this to write out, serialize is non const for read in
			NonConst->Serialize(ChaosAr);
			Info.ToString = NonConst->ToString();
			Info.NumBytes = Data.Num();
			MemEntries.Add(Info);
			TotalBytes += Info.NumBytes;
		}

		MemEntries.Sort();

		Ar->Logf(TEXT(""));
		Ar->Logf(TEXT("Chaos Tracked Geometry:"));
		Ar->Logf(TEXT(""));

		for (const FMemInfo& Info : MemEntries)
		{
			Ar->Logf(TEXT("%-10d %s ToString:%s"), Info.NumBytes, *Info.DebugInfo, *Info.ToString);
		}

		Ar->Logf(TEXT("%-10d Total"), TotalBytes);
	}

	void FTrackedGeometryManager::AddGeometry(TSerializablePtr<FImplicitObject> Geometry, const FString& DebugInfo)
	{
		FScopeLock Lock(&CriticalSection);
		SharedGeometry.Add(Geometry, DebugInfo);
	}

	void FTrackedGeometryManager::RemoveGeometry(const FImplicitObject* Geometry)
	{
		FScopeLock Lock(&CriticalSection);
		TSerializablePtr<FImplicitObject> Dummy;
		Dummy.SetFromRawLowLevel(Geometry);
		SharedGeometry.Remove(Dummy);
	}

}
