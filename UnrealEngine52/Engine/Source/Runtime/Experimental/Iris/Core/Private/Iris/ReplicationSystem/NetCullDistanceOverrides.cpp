// Copyright Epic Games, Inc. All Rights Reserved.

#include "Iris/ReplicationSystem/NetCullDistanceOverrides.h"
#include "Iris/Core/IrisMemoryTracker.h"

namespace UE::Net
{

void FNetCullDistanceOverrides::Init(const FNetCullDistanceOverridesInitParams& InitParams)
{
	ValidCullDistanceSqr.Init(InitParams.MaxObjectCount);
}

void FNetCullDistanceOverrides::ClearCullDistanceSqr(uint32 ObjectIndex)
{
	ValidCullDistanceSqr.ClearBit(ObjectIndex);
}

void FNetCullDistanceOverrides::SetCullDistanceSqr(uint32 ObjectIndex, float CullDistSqr)
{
	ValidCullDistanceSqr.SetBit(ObjectIndex);
	if (ObjectIndex >= uint32(CullDistanceSqr.Num()))
	{
		LLM_SCOPE_BYTAG(Iris);
		CullDistanceSqr.Add(ObjectIndex + 1U - CullDistanceSqr.Num());
	}
	CullDistanceSqr[ObjectIndex] = CullDistSqr;
}

}
