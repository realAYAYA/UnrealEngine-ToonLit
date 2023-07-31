// Copyright Epic Games, Inc. All Rights Reserved.

#include "ClothLODData.h"
#include "ClothPhysicalMeshData.h"
#include "ClothingAssetCustomVersion.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ClothLODData)

#if WITH_EDITORONLY_DATA
void FClothLODDataCommon::GetParameterMasksForTarget(
	const uint8 InTarget, 
	TArray<FPointWeightMap*>& OutMasks)
{
	for(FPointWeightMap& Mask : PointWeightMaps)
	{
		if(Mask.CurrentTarget == InTarget)
		{
			OutMasks.Add(&Mask);
		}
	}
}
#endif // WITH_EDITORONLY_DATA

bool FClothLODDataCommon::Serialize(FArchive& Ar)
{
	Ar.UsingCustomVersion(FClothingAssetCustomVersion::GUID);

	// Serialize normal tagged property data
	if (Ar.IsLoading() || Ar.IsSaving())
	{
		UScriptStruct* const Struct = FClothLODDataCommon::StaticStruct();
		Struct->SerializeTaggedProperties(Ar, (uint8*)this, Struct, nullptr);
	}

	// Serialize the mesh to mesh data (not a USTRUCT)
	Ar << TransitionUpSkinData
	   << TransitionDownSkinData;

#if WITH_EDITORONLY_DATA
	const int32 ClothingCustomVersion = Ar.CustomVer(FClothingAssetCustomVersion::GUID);
	if (ClothingCustomVersion < FClothingAssetCustomVersion::MovePropertiesToCommonBaseClasses)
	{
		// Migrate maps
		PhysicalMeshData.GetWeightMap(EWeightMapTargetCommon::MaxDistance).Values = MoveTemp(PhysicalMeshData.MaxDistances_DEPRECATED);
		PhysicalMeshData.GetWeightMap(EWeightMapTargetCommon::BackstopDistance).Values = MoveTemp(PhysicalMeshData.BackstopDistances_DEPRECATED);
		PhysicalMeshData.GetWeightMap(EWeightMapTargetCommon::BackstopRadius).Values = MoveTemp(PhysicalMeshData.BackstopRadiuses_DEPRECATED);
		PhysicalMeshData.GetWeightMap(EWeightMapTargetCommon::AnimDriveStiffness).Values = MoveTemp(PhysicalMeshData.AnimDriveMultipliers_DEPRECATED);

		// Migrate editor maps
		PointWeightMaps.SetNum(ParameterMasks_DEPRECATED.Num());
		for (int32 i = 0; i < PointWeightMaps.Num(); ++i)
		{
			ParameterMasks_DEPRECATED[i].MigrateTo(PointWeightMaps[i]);
		}
		ParameterMasks_DEPRECATED.Empty();

		// Migrate Convex Planes from the legacy Apex collision (used for box collision)
		// Note: This code is not enough to get Apex convex working (it's missing surface points and face indices),
		//       but it is better to keep the data alive for now in case the Apex deprecation causes any issues.
		for (FClothCollisionPrim_Convex& Convex : CollisionData.Convexes)
		{
			const int32 NumDeprecatedPlanes = Convex.Planes_DEPRECATED.Num();
			if (NumDeprecatedPlanes)
			{
				Convex.Faces.SetNum(NumDeprecatedPlanes);
				for (int32 FaceIndex = 0; FaceIndex < NumDeprecatedPlanes; ++FaceIndex)
				{
					Convex.Faces[FaceIndex].Plane = Convex.Planes_DEPRECATED[FaceIndex];
				}
				Convex.Planes_DEPRECATED.Empty();
			}
		}
	}
#endif // WITH_EDITORONLY_DATA
	return true;
}

#if WITH_EDITOR
void FClothLODDataCommon::PushWeightsToMesh()
{
	PhysicalMeshData.ClearWeightMaps();
	for (const FPointWeightMap& Weights : PointWeightMaps)
	{
		if (Weights.bEnabled)
		{
			FPointWeightMap& TargetWeightMap = PhysicalMeshData.FindOrAddWeightMap(Weights.CurrentTarget);
			TargetWeightMap.Values = Weights.Values;
		}
	}
}
#endif

