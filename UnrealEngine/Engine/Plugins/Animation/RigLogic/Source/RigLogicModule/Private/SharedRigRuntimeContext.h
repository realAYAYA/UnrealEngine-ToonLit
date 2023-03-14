// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DNAReader.h"
#include "RigLogic.h"

struct FSharedRigRuntimeContext
{
	template <typename T>
	struct TNestedArray
	{
		TArray<T> Values;
	};

	// Redundant but stored here as well so the runtime context can be updated / queried atomically
	TSharedPtr<IBehaviorReader> BehaviorReader;

	/** RigLogic itself is stateless, and is designed to be shared between
	  * multiple rig instances based on the same DNA.
	**/
	TSharedPtr<FRigLogic> RigLogic;

	/** Cached joint indices that need to be updated for each LOD **/
	TArray<TNestedArray<uint16>> VariableJointIndicesPerLOD;

	void CacheVariableJointIndices();
};
