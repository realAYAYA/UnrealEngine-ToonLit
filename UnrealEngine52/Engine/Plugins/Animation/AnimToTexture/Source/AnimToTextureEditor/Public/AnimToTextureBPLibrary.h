// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AnimToTextureDataAsset.h"
#include "AnimToTextureMeshMapping.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Materials/MaterialLayersFunctions.h"

#include "AnimToTextureBPLibrary.generated.h"

class UStaticMesh;
class USkeletalMesh;

/*
* TODO: 
*	- Right now it is saving data per-vertex instead of per-point.
*     This will require more pixels if the mesh has lots of material slots or uv-shells.
*     FStaticMeshOperations::FindOverlappingCorners(FOverlappingCorners& OutOverlappingCorners, const FMeshDescription& MeshDescription, float ComparisonThreshold)
*/
UCLASS()
class UAnimToTextureBPLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()

#if WITH_EDITOR

	/**
	* Bakes Animation Data into Textures.
	*/
	UFUNCTION(BlueprintCallable, meta = (Category = "AnimToTexture"))
	static void AnimationToTexture(UAnimToTextureDataAsset* DataAsset, const FTransform RootTransform, bool& bSuccess);

	/** 
	* Utility for converting SkeletalMesh into a StaticMesh
	*/
	UFUNCTION(BlueprintCallable, Category = "AnimToTexture")
	static UStaticMesh* ConvertSkeletalMeshToStaticMesh(USkeletalMesh* SkeletalMesh, const FString PackageName, const int32 LODIndex = -1);

	// FIXME: you cannot set index to 2 if there is no index 1
	UFUNCTION(BlueprintCallable, Category = "AnimToTexture")
	static bool SetLightMapIndex(UStaticMesh* StaticMesh, const int32 LODIndex, const int32 LightmapIndex=1, bool bGenerateLightmapUVs=true);

	/**
	* Updates a material's parameters to match those of an animToTexture data asset
	*/
	UFUNCTION(BlueprintCallable, meta = (Category = "AnimToTexture"))
	static void UpdateMaterialInstanceFromDataAsset(UAnimToTextureDataAsset* DataAsset, class UMaterialInstanceConstant* MaterialInstance, 
		const bool bAnimate=false, const EAnimToTextureNumBoneInfluences NumBoneInfluences = EAnimToTextureNumBoneInfluences::One,
		const EMaterialParameterAssociation MaterialParameterAssociation = EMaterialParameterAssociation::LayerParameter);

#endif // WITH_EDITOR

private:

	// Get Vertex and Normals from Current Pose
	// The VertexDelta is returned from the RefPose
	static void GetVertexDeltasAndNormals(const USkeletalMeshComponent* SkeletalMeshComponent, const int32 LODIndex, 
		const TArray<FVector3f>& Vertices, const TArray<FVector3f>& Normals, const TArray<AnimToTexture_Private::FVertexToMeshMapping>& Mapping,
		const FTransform RootTransform,
		TArray<FVector3f>& OutVertexDeltas, TArray<FVector3f>& OutVertexNormals);

	// Gets RefPose Bone Position and Rotations.
	static int32 GetRefBonePositionsAndRotations(const USkeletalMeshComponent* SkeletalMeshComponent, 
		TArray<FVector3f>& OutBoneRefPositions, TArray<FVector4>& OutBoneRefRotations);

	// Gets Bone Position and Rotations for Current Pose.	
	// The BonePosition is returned relative to the RefPose
	static int32 GetBonePositionsAndRotations(const USkeletalMeshComponent* SkeletalMeshComponent, const TArray<FVector3f>& BoneRefPositions,
		TArray<FVector3f>& BonePositions, TArray<FVector4>& BoneRotations);

	// Normalizes Deltas and Normals between [0-1] with Bounding Box
	static void NormalizeVertexData(
		const TArray<FVector3f>& Deltas, const TArray<FVector3f>& Normals,
		FVector& OutMinBBox, FVector& OutSizeBBox,
		TArray<FVector3f>& OutNormalizedDeltas, TArray<FVector3f>& OutNormalizedNormals);

	// Normalizes Positions and Rotations between [0-1] with Bounding Box
	static void NormalizeBoneData(
		const TArray<FVector3f>& Positions, const TArray<FVector4>& Rotations,
		FVector& OutMinBBox, FVector& OutSizeBBox,
		TArray<FVector3f>& OutNormalizedPositions, TArray<FVector4>& OutNormalizedRotations);

	/* Returns best resolution for the given data. 
	*  Returns false if data doesnt fit in the the max range */
	static bool FindBestResolution(const int32 NumFrames, const int32 NumElements,
								   int32& OutHeight, int32& OutWidth, int32& OutRowsPerFrame,
								   const int32 MaxHeight = 4096, const int32 MaxWidth = 4096, bool bEnforcePowerOfTwo = false);

	/* Sets Static Mesh FullPrecisionUVs Property*/
	static void SetFullPrecisionUVs(UStaticMesh* StaticMesh, const int32 LODIndex, bool bFullPrecision=true);

	/* Sets Static Mesh Bound Extensions */
	static void SetBoundsExtensions(UStaticMesh* StaticMesh, const FVector& MinBBox, const FVector& SizeBBox);

	/* Creates UV Coord with vertices */
	static bool CreateUVChannel(UStaticMesh* StaticMesh, const int32 LODIndex, const int32 UVChannelIndex,
		const int32 Height, const int32 Width);

};
