// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Containers/Array.h"
#include "Containers/ContainersFwd.h"
#include "Math/UnrealMathSSE.h"
#include "Templates/Tuple.h"
#include "UObject/Class.h"
#include "UObject/ObjectMacros.h"

#include "ClothTetherData.generated.h"

class FArchive;

/**
 * Long range attachment tether pathfinding based on Dijkstra's algorithm.
 * For more information about the long range attachment, see:
 * https://matthias-research.github.io/pages/publications/sca2012cloth.pdf
 */
USTRUCT()
struct FClothTetherData
{
	GENERATED_BODY()

	/** Long range attachment tether start/end/length, sorted in sequential batches of independant tethers. */
	TArray<TArray<TTuple<int32, int32, float>>> Tethers;
	
	/**
	 * Generate the tethers by following the triangle mesh network from the closest kinematic to each dynamic point.
	 * Inside array items can be processed concurrently, but the outside array must be iterated on sequentially.
	 */
	void GenerateTethers(
		const TConstArrayView<FVector3f>& Points,  // Reference pose
		const TConstArrayView<uint32>& Indices,  // Triangle mesh
		const TConstArrayView<float>& MaxDistances,  // Mask for sorting the kinematic from the dynamic points
		bool bUseGeodesicDistance);  // Whether to use geodesic (walking along the sruface) or euclidean (beeline) distances to find the tethers.

	/** Custom serializer, since neiher an array of array nor a tuple can be set as a UPROPERTY. */ 
	bool Serialize(FArchive& Ar);
};

template<>
struct TStructOpsTypeTraits<FClothTetherData> : public TStructOpsTypeTraitsBase2<FClothTetherData>
{
	enum
	{
		WithSerializer = true,
	};
};
