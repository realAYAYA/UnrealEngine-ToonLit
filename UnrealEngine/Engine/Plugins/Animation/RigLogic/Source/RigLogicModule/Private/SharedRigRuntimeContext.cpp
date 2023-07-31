// Copyright Epic Games, Inc. All Rights Reserved.

#include "SharedRigRuntimeContext.h"

#include "CoreMinimal.h"
#include "HAL/LowLevelMemTracker.h"
#include "DNAReader.h"
#include "RigLogic.h"

static constexpr uint16 NUM_ATTRS_PER_JOINT = 9;

void FSharedRigRuntimeContext::CacheVariableJointIndices()
{
	LLM_SCOPE_BYNAME(TEXT("Animation/RigLogic"));

	const uint16 LODCount = BehaviorReader->GetLODCount();
	VariableJointIndicesPerLOD.Reset();
	VariableJointIndicesPerLOD.AddDefaulted(LODCount);
	TSet<uint16> DistinctVariableJointIndices;
	for (uint16 LODIndex = 0; LODIndex < LODCount; ++LODIndex)
	{
		TArrayView<const uint16> VariableAttributeIndices = BehaviorReader->GetJointVariableAttributeIndices(LODIndex);
		DistinctVariableJointIndices.Reset();
		DistinctVariableJointIndices.Reserve(VariableAttributeIndices.Num());
		for (const uint16 AttrIndex : VariableAttributeIndices)
		{
			const uint16 JointIndex = AttrIndex / NUM_ATTRS_PER_JOINT;
			DistinctVariableJointIndices.Add(JointIndex);
		}
		VariableJointIndicesPerLOD[LODIndex].Values = DistinctVariableJointIndices.Array();
	}
}
