// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/Array.h"
#include "Math/Transform.h"
#include "Math/UnrealMathSSE.h"
#include "Math/Vector.h"


// Data produced by a clothing simulation
struct FClothSimulData
{
	void Reset()
	{
		Positions.Reset();
		Normals.Reset();
		LODIndex = INDEX_NONE;
	}

	// Positions of the simulation mesh particles (aligned for SIMD loads)
	TArray<FVector3f, TAlignedHeapAllocator<16>> Positions;

	// Normals at the simulation mesh particles (aligned for SIMD loads)
	TArray<FVector3f, TAlignedHeapAllocator<16>> Normals;

	// Transform applied per position/normal element when loaded
	FTransform Transform;

	// Transform relative to the component to update clothing root transform when not ticking clothing but rendering a component
	FTransform ComponentRelativeTransform;

	// Current LOD index the data is valid for
	int32 LODIndex;
};

enum class EClothingTeleportMode : uint8
{
	// No teleport, simulate as normal
	None = 0,
	// Teleport the simulation, causing no intertial effects but keep the sim mesh shape
	Teleport,
	// Teleport the simulation, causing no intertial effects and reset the sim mesh shape
	TeleportAndReset
};

