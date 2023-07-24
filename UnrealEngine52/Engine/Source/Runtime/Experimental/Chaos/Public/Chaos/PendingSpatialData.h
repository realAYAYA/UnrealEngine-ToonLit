// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/ParticleHandle.h"
#include "Chaos/Transform.h"
#include "Chaos/ISpatialAcceleration.h"
#include "Chaos/Defines.h"

namespace Chaos
{ 
	
/** Used for updating intermediate spatial structures when they are finished */
struct FPendingSpatialData
{
	FAccelerationStructureHandle AccelerationHandle;
	FSpatialAccelerationIdx SpatialIdx;
	int32 SyncTimestamp;	//indicates the inputs timestamp associated with latest change. Only relevant for external queue
	bool bDelete;

	FPendingSpatialData()
	: SyncTimestamp(0)
	, bDelete(false)
	{}

	void Serialize(FChaosArchive& Ar)
	{
		/*Ar.UsingCustomVersion(FExternalPhysicsCustomObjectVersion::GUID);
		if (Ar.CustomVer(FExternalPhysicsCustomObjectVersion::GUID) >= FExternalPhysicsCustomObjectVersion::SerializeHashResult)
		{
			Ar << UpdateAccelerationHandle;
			Ar << DeleteAccelerationHandle;
		}
		else
		{
			Ar << UpdateAccelerationHandle;
			DeleteAccelerationHandle = UpdateAccelerationHandle;
		}

		Ar << bUpdate;
		Ar << bDelete;

		Ar << UpdatedSpatialIdx;
		Ar << DeletedSpatialIdx;*/
		ensure(false);	//Serialization of transient data like this is currently broken. Need to reevaluate
	}

	FUniqueIdx UniqueIdx() const
	{
		return AccelerationHandle.UniqueIdx();
	}
};

struct FPendingSpatialDataQueue
{
	TArray<FPendingSpatialData> PendingData;
	TArrayAsMap<FUniqueIdx,int32> ParticleToPendingData;

	void Reset()
	{
		PendingData.Reset();
		ParticleToPendingData.Reset();
	}

	int32 Num() const
	{
		return PendingData.Num();
	}

	FPendingSpatialData& FindOrAdd(const FUniqueIdx UniqueIdx)
	{
		if(int32* Existing = ParticleToPendingData.Find(UniqueIdx))
		{
			return PendingData[*Existing];
		} else
		{
			const int32 NewIdx = PendingData.AddDefaulted(1);
			ParticleToPendingData.Add(UniqueIdx,NewIdx);
			return PendingData[NewIdx];
		}
	}

	void Remove(const FUniqueIdx UniqueIdx)
	{
		if(int32* Existing = ParticleToPendingData.Find(UniqueIdx))
		{
			const int32 SlotIdx = *Existing;
			if(SlotIdx + 1 < PendingData.Num())
			{
				const FUniqueIdx LastElemUniqueIdx = PendingData.Last().UniqueIdx();
				ParticleToPendingData.FindChecked(LastElemUniqueIdx) = SlotIdx;	//We're going to swap elements so the last element is now in the position of the element we removed
			}

			PendingData.RemoveAtSwap(SlotIdx);
			ParticleToPendingData.RemoveChecked(UniqueIdx);
		}
	}
};

}