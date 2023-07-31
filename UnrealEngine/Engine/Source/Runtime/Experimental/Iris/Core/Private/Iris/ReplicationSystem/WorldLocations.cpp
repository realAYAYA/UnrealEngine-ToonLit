// Copyright Epic Games, Inc. All Rights Reserved.

#include "Iris/ReplicationSystem/WorldLocations.h"
#include "Iris/Core/IrisMemoryTracker.h"

namespace UE::Net
{

void FWorldLocations::Init(const FWorldLocationsInitParams& InitParams)
{
	ValidWorldLocations.Init(InitParams.MaxObjectCount);
}

void FWorldLocations::SetHasWorldLocation(uint32 ObjectIndex, bool bHasWorldLocation)
{
	ValidWorldLocations.SetBitValue(ObjectIndex, bHasWorldLocation);
	if (bHasWorldLocation)
	{
		if (ObjectIndex >= uint32(WorldLocations.Num()))
		{
			LLM_SCOPE_BYTAG(Iris);
			WorldLocations.Add(ObjectIndex + 1U - WorldLocations.Num());
		}
		WorldLocations[ObjectIndex] = FVector::Zero();
	}
}

void FWorldLocations::SetWorldLocation(uint32 ObjectIndex, const FVector& WorldLocation)
{
	checkSlow(ValidWorldLocations.GetBit(ObjectIndex));
	WorldLocations[ObjectIndex] = WorldLocation;
}

}
