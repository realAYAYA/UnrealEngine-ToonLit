// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Math/Box.h"

struct FDataflowTransformSelection;
struct FManagedArrayCollection;


class FRACTUREENGINE_API FFractureEngineFracturing
{
public:

	static void GenerateExplodedViewAttribute(FManagedArrayCollection& InOutCollection, const FVector& InScale, float InUniformScale);

	static void VoronoiFracture(FManagedArrayCollection& InOutCollection,
		const FDataflowTransformSelection& InTransformSelection,
		const TArray<FVector>& InSites,
		float InRandomSeed,
		float InChanceToFracture,
		bool InGroupFracture,
		float InGrout,
		float InAmplitude,
		float InFrequency,
		float InPersistence,
		float InLacunarity,
		int32 InOctaveNumber,
		float InPointSpacing,
		bool InAddSamplesForCollision,
		float InCollisionSampleSpacing);

	static void PlaneCutter(FManagedArrayCollection& InOutCollection,
		const FDataflowTransformSelection& InTransformSelection,
		const FBox& InBoundingBox,
		int32 InNumPlanes,
		float InRandomSeed,
		float InGrout,
		float InAmplitude,
		float InFrequency,
		float InPersistence,
		float InLacunarity,
		int32 InOctaveNumber,
		float InPointSpacing,
		bool InAddSamplesForCollision,
		float InCollisionSampleSpacing);

};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "Algo/Count.h"
#include "CoreMinimal.h"
#include "Dataflow/DataflowSelection.h"
#include "GeometryCollection/GeometryCollection.h"
#include "GeometryCollection/ManagedArrayCollection.h"
#endif
