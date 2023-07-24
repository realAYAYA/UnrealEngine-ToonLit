// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GeometryScript/GeometryScriptTypes.h"
#include "SkeletalMeshAttributes.h"
#include "MeshBoneWeightFunctions.generated.h"

class UDynamicMesh;


USTRUCT(BlueprintType, meta = (DisplayName = "Bone Weights"))
struct GEOMETRYSCRIPTINGCORE_API FGeometryScriptBoneWeight
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = BoneWeights)
	int32 BoneIndex = 0;

	UPROPERTY(BlueprintReadWrite, Category = BoneWeights)
	float Weight = 0;
};


USTRUCT(BlueprintType, meta = (DisplayName = "Bone Weights Profile"))
struct GEOMETRYSCRIPTINGCORE_API FGeometryScriptBoneWeightProfile
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = BoneWeights)
	FName ProfileName = FSkeletalMeshAttributes::DefaultSkinWeightProfileName;

	FName GetProfileName() const { return ProfileName; }
};


UENUM(BlueprintType)
enum class EGeometryScriptSmoothBoneWeightsType : uint8
{
	DirectDistance = 0,		/** Compute weighting by using Euclidean distance from bone to vertex */
	GeodesicVoxel = 1,		/** Compute weighting by using geodesic distance from bone to vertex */
};


USTRUCT(BlueprintType)
struct GEOMETRYSCRIPTINGCORE_API FGeometryScriptSmoothBoneWeightsOptions
{
	GENERATED_BODY()

	/** The type of algorithm to use for computing the bone weight for each vertex */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	EGeometryScriptSmoothBoneWeightsType DistanceWeighingType = EGeometryScriptSmoothBoneWeightsType::DirectDistance;

	/** How rigid the binding should be. Higher values result in a more rigid binding (greater influence by bones
	 *  closer to the vertex than those further away).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	float Stiffness = 0.2f;

	/** Maximum number of bones that contribute to each weight. Set to 1 for a completely rigid binding. Higher values
	 *  to have more distant bones make additional contributions to the deformation at each vertex. 
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	int32 MaxInfluences = 5;

	/** The resolution to build the voxelized representation of the mesh, for computing geodesic distance. Higher values
	 *  result in greater fidelity and less chance of disconnected parts contributing, but slower rate of computation and
	 *  more memory usage.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options, meta=(EditCondition="DistanceWeighingType==EGeometryScriptSmoothBoneWeightsType::GeodesicVoxel"))
	int32 VoxelResolution = 256;
};




UCLASS(meta = (ScriptName = "GeometryScript_BoneWeights"))
class GEOMETRYSCRIPTINGCORE_API UGeometryScriptLibrary_MeshBoneWeightFunctions : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:

	/**
	 * Check whether the TargetMesh has a per-vertex Bone/Skin Weight Attribute set
	 * @param bHasBoneWeights will be returned true if the requested bone weight profile exists
	 * @param Profile identifier for the bone/skin weight profile
	 */
	UFUNCTION(BlueprintPure, Category = "GeometryScript|MeshQueries|BoneWeights", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	MeshHasBoneWeights( 
		UDynamicMesh* TargetMesh,
		bool& bHasBoneWeights,
		FGeometryScriptBoneWeightProfile Profile = FGeometryScriptBoneWeightProfile() );

	/**
	 * Create a new BoneWeights attribute on the TargetMesh, if it does not already exist. If it does exist, 
	 * and bReplaceExistingProfile is passed as true, the attribute will be removed and re-added, to reset it. 
	 * @param bProfileExisted will be returned true if the requested bone weight profile already existed
	 * @param bReplaceExistingProfile if true, if the Profile already exists, it is reset
	 * @param Profile identifier for the bone/skin weight profile
	 */
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|MeshQueries|BoneWeights", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	MeshCreateBoneWeights( 
		UDynamicMesh* TargetMesh,
		bool& bProfileExisted,
		bool bReplaceExistingProfile = false,
		FGeometryScriptBoneWeightProfile Profile = FGeometryScriptBoneWeightProfile() );
	
	/**
	 * Determine the largest bone weight index that exists on the Mesh
	 * @param bHasBoneWeights will be returned true if the requested bone weight profile exists
	 * @param MaxBoneIndex maximum bone index will be returned here, or -1 if no bone indices exist
	 * @param Profile identifier for the bone/skin weight profile
	 */
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|MeshQueries|BoneWeights", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	GetMaxBoneWeightIndex( 
		UDynamicMesh* TargetMesh,
		bool& bHasBoneWeights,
		int& MaxBoneIndex,
		FGeometryScriptBoneWeightProfile Profile = FGeometryScriptBoneWeightProfile() );

	/**
	 * Return an array of Bone/Skin Weights at a given vertex of TargetMesh
	 * @param VertexID requested vertex
	 * @param BoneWeights output array of bone index/weight pairs for the given Vertex
	 * @param bHasValidBoneWeights will be returned as true if the vertex has bone weights in the given profile, ie BoneWeights is valid
	 * @param Profile identifier for the bone/skin weight profile
	 */
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|MeshQueries|BoneWeights", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	GetVertexBoneWeights( 
		UDynamicMesh* TargetMesh,
		int VertexID,
		TArray<FGeometryScriptBoneWeight>& BoneWeights,
		bool& bHasValidBoneWeights,
		FGeometryScriptBoneWeightProfile Profile = FGeometryScriptBoneWeightProfile() );

	/**
	 * Return the Bone/Skin Weight with the maximum weight at a given vertex of TargetMesh
	 * @param VertexID requested vertex
	 * @param BoneWeight the bone index and weight that was found to have the maximum weight
	 * @param bHasValidBoneWeights will be returned as true if the vertex has bone weights in the given profile and a largest weight was found
	 * @param Profile identifier for the bone/skin weight profile
	 */
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|MeshQueries|BoneWeights", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	GetLargestVertexBoneWeight( 
		UDynamicMesh* TargetMesh,
		int VertexID,
		FGeometryScriptBoneWeight& BoneWeight,
		bool& bHasValidBoneWeights,
		FGeometryScriptBoneWeightProfile Profile = FGeometryScriptBoneWeightProfile() );

	/**
	 * Set the Bone/Skin Weights at a given vertex of TargetMesh
	 * @param VertexID vertex to update
	 * @param BoneWeights input array of bone index/weight pairs for the Vertex
	 * @param bIsValidVertexID will be returned as true if the vertex ID is valid
	 * @param Profile identifier for the bone/skin weight profile
	 */
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|MeshQueries|BoneWeights", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	SetVertexBoneWeights( 
		UDynamicMesh* TargetMesh,
		int VertexID,
		const TArray<FGeometryScriptBoneWeight>& BoneWeights,
		bool& bIsValidVertexID,
		FGeometryScriptBoneWeightProfile Profile = FGeometryScriptBoneWeightProfile() );

	/**
	 * Set all vertices of the TargetMesh to the given Bone/Skin Weights
	 * @param BoneWeights input array of bone index/weight pairs for the Vertex
	 * @param Profile identifier for the bone/skin weight profile
	 */
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|MeshQueries|BoneWeights", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	SetAllVertexBoneWeights( 
		UDynamicMesh* TargetMesh,
		const TArray<FGeometryScriptBoneWeight>& BoneWeights,
		FGeometryScriptBoneWeightProfile Profile = FGeometryScriptBoneWeightProfile() );

	/** 
	 *  Computes a smooth skin binding for the given mesh to the skeleton provided.
	 *  @param Skeleton The skeleton to compute binding for the skin weights.
	 *  @param Options The options to set for the binding algorithm.
	 *  @param Profile The skin weight profile to update with the smooth binding.
	 */
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|MeshQueries|BoneWeights", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh*
	ComputeSmoothBoneWeights(
		UDynamicMesh* TargetMesh, 
		USkeleton* Skeleton,
		FGeometryScriptSmoothBoneWeightsOptions Options,
		FGeometryScriptBoneWeightProfile Profile = FGeometryScriptBoneWeightProfile(),
		UGeometryScriptDebug* Debug = nullptr);
	
};