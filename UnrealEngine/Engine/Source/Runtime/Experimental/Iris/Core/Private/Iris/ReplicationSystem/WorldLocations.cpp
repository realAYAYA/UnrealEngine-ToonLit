// Copyright Epic Games, Inc. All Rights Reserved.

#include "Iris/ReplicationSystem/WorldLocations.h"
#include "Iris/Core/IrisMemoryTracker.h"

namespace UE::Net
{

void FWorldLocations::Init(const FWorldLocationsInitParams& InitParams)
{
	ValidInfoIndexes.Init(InitParams.MaxObjectCount);
}

void FWorldLocations::InitObjectInfoCache(uint32 ObjectIndex)
{
	if (ValidInfoIndexes.IsBitSet(ObjectIndex))
	{
		// Only init on first assignment
		return;
	}

	ValidInfoIndexes.SetBit(ObjectIndex);
	
	if (ObjectIndex >= uint32(StoredObjectInfo.Num()))
	{
		LLM_SCOPE_BYTAG(Iris);
		StoredObjectInfo.Add(ObjectIndex + 1U - StoredObjectInfo.Num());
	}

	StoredObjectInfo[ObjectIndex].WorldLocation = FVector::Zero();
	StoredObjectInfo[ObjectIndex].CullDistance = 0.0f;
}

void FWorldLocations::RemoveObjectInfoCache(uint32 ObjectIndex)
{
	ValidInfoIndexes.ClearBit(ObjectIndex);
}

void FWorldLocations::SetObjectInfo(uint32 ObjectIndex, const FWorldLocations::FObjectInfo& ObjectInfo)
{
	checkSlow(ValidInfoIndexes.IsBitSet(ObjectIndex));
	StoredObjectInfo[ObjectIndex] = ObjectInfo;
}

void FWorldLocations::UpdateWorldLocation(uint32 ObjectIndex, const FVector& WorldLocation)
{
	checkSlow(ValidInfoIndexes.GetBit(ObjectIndex));
	StoredObjectInfo[ObjectIndex].WorldLocation = WorldLocation;
}

}
