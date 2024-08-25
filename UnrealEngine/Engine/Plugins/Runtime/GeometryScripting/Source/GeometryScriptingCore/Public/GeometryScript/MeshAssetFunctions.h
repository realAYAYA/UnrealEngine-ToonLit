// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GeometryScript/GeometryScriptTypes.h"
#include "Engine/EngineTypes.h"   // FMeshNaniteSettings
#include "MeshAssetFunctions.generated.h"

class UStaticMesh;
class UDynamicMesh;
class UMaterialInterface;



USTRUCT(BlueprintType)
struct GEOMETRYSCRIPTINGCORE_API FGeometryScriptCopyMeshFromAssetOptions
{
	GENERATED_BODY()
public:
	// Whether to apply Build Settings during the mesh copy.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	bool bApplyBuildSettings = true;

	// Whether to request tangents on the copied mesh. If tangents are not requested, tangent-related build settings will also be ignored.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	bool bRequestTangents = true;

	// Whether to ignore the 'remove degenerates' option from Build Settings. Note: Only applies if 'Apply Build Settings' is enabled.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	bool bIgnoreRemoveDegenerates = true;

	// Whether to scale the copied mesh by the Build Setting's 'Build Scale'. Note: This is considered separately from the 'Apply Build Settings' option.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	bool bUseBuildScale = true;
};

/**
 * Configuration settings for Nanite Rendering on StaticMesh Assets
 */
USTRUCT(BlueprintType)
struct GEOMETRYSCRIPTINGCORE_API FGeometryScriptNaniteOptions
{
	GENERATED_BODY()

	/** Set Nanite to Enabled/Disabled */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	bool bEnabled = true;

	/** Percentage of triangles to maintain in Fallback Mesh used when Nanite is unavailable */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	float FallbackPercentTriangles = 100.0f;

	/** Relative Error to maintain in Fallback Mesh used when Nanite is unavailable. Overrides FallbackPercentTriangles. Set to 0 to only use FallbackPercentTriangles (default). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	float FallbackRelativeError = 0.0f;
};


USTRUCT(BlueprintType)
struct GEOMETRYSCRIPTINGCORE_API FGeometryScriptCopyMeshToAssetOptions
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	bool bEnableRecomputeNormals = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	bool bEnableRecomputeTangents = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	bool bEnableRemoveDegenerates = false;

	// Whether to use the build scale on the target asset. If enabled, the inverse scale will be applied when saving to the asset, and the BuildScale will be preserved. Otherwise, BuildScale will be set to 1.0 on the asset BuildSettings.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	bool bUseBuildScale = true;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	bool bReplaceMaterials = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	TArray<TObjectPtr<UMaterialInterface>> NewMaterials;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	TArray<FName> NewMaterialSlotNames;

	/** If enabled, NaniteSettings will be applied to the target Asset if possible */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	bool bApplyNaniteSettings = false;

	/** Replaced FGeometryScriptNaniteOptions with usage of Engine FMeshNaniteSettings */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Deprecated, AdvancedDisplay, meta=(DisplayName = "DEPRECATED NANITE SETTING") )
	FGeometryScriptNaniteOptions NaniteSettings = FGeometryScriptNaniteOptions();

	/** Nanite Settings applied to the target Asset, if bApplyNaniteSettings = true */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options, meta = (DisplayName = "Nanite Settings"))
	FMeshNaniteSettings NewNaniteSettings;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	bool bEmitTransaction = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	bool bDeferMeshPostEditChange = false;
};



// Although the class name indicates StaticMeshFunctions, that was a naming mistake that is difficult
// to correct. This class is intended to serve as a generic asset utils function library. The naming
// issue is only visible at the C++ level. It is not visible in Python or BP.
UCLASS(meta = (ScriptName = "GeometryScript_AssetUtils"))
class GEOMETRYSCRIPTINGCORE_API UGeometryScriptLibrary_StaticMeshFunctions : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	
	/** 
	* Check if a Static Mesh Asset has the RequestedLOD available, ie if CopyMeshFromStaticMesh will be able to
	* succeed for the given LODType and LODIndex. 
	*/
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|StaticMesh", meta = (ExpandEnumAsExecs = "Outcome"))
	static bool
	CheckStaticMeshHasAvailableLOD(
		UStaticMesh* StaticMeshAsset, 
		FGeometryScriptMeshReadLOD RequestedLOD,
		EGeometryScriptSearchOutcomePins& Outcome,
		UGeometryScriptDebug* Debug = nullptr);

	/**
	 * Determine the number of available LODs of the requested LODType in a Static Mesh Asset
	 */
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|StaticMesh")
	static int
	GetNumStaticMeshLODsOfType(
		UStaticMesh* StaticMeshAsset, 
		EGeometryScriptLODType LODType = EGeometryScriptLODType::SourceModel);

	/** 
	* Extracts a Dynamic Mesh from a Static Mesh Asset. 
	* 
	* Note that the LOD Index in RequestedLOD will be silently clamped to the available number of LODs (SourceModel or RenderData)
	*/
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|StaticMesh", meta = (ExpandEnumAsExecs = "Outcome"))
	static UPARAM(DisplayName = "Dynamic Mesh") UDynamicMesh* 
	CopyMeshFromStaticMesh(
		UStaticMesh* FromStaticMeshAsset, 
		UDynamicMesh* ToDynamicMesh, 
		FGeometryScriptCopyMeshFromAssetOptions AssetOptions,
		FGeometryScriptMeshReadLOD RequestedLOD,
		EGeometryScriptOutcomePins& Outcome,
		UGeometryScriptDebug* Debug = nullptr);

	/** 
	* Updates a Static Mesh Asset with new geometry converted from a Dynamic Mesh.
	*/
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|StaticMesh", meta = (ExpandEnumAsExecs = "Outcome"))
	static UPARAM(DisplayName = "Dynamic Mesh") UDynamicMesh* 
	CopyMeshToStaticMesh(
		UDynamicMesh* FromDynamicMesh, 
		UStaticMesh* ToStaticMeshAsset, 
		FGeometryScriptCopyMeshToAssetOptions Options,
		FGeometryScriptMeshWriteLOD TargetLOD,
		EGeometryScriptOutcomePins& Outcome,
		UGeometryScriptDebug* Debug = nullptr);


    /** 
	* Extracts the Material List and corresponding Material Indices from the specified LOD of the Static Mesh Asset. 
	* The MaterialList is sorted by Section, so if CopyMeshToStaticMesh was used to create a DynamicMesh, then the returned
	* MaterialList here will correspond to the MaterialIDs in that DynamicMesh (as each Static Mesh Section becomes a MaterialID, in-order). 
	* So, the returned MaterialList can be passed directly to (eg) a DynamicMeshComponent.
	* 
	* @param MaterialIndex this returned array is the same size as MaterialList, with each value the index of that Material in the StaticMesh Material List
	* @param MateriaSlotNames this returned array is the same size as MaterialList, with each value the Slot Name of that Material in the StaticMesh Material List
	*
	* Note that the LOD Index in RequestedLOD will be silently clamped to the available number of LODs (SourceModel or RenderData)
	*/
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|StaticMesh", meta = (ExpandEnumAsExecs = "Outcome"))
	static void
	GetSectionMaterialListFromStaticMesh(
		UStaticMesh* FromStaticMeshAsset, 
		FGeometryScriptMeshReadLOD RequestedLOD,
		TArray<UMaterialInterface*>& MaterialList,
		TArray<int32>& MaterialIndex,
		TArray<FName>& MaterialSlotNames,
		EGeometryScriptOutcomePins& Outcome,
		UGeometryScriptDebug* Debug = nullptr);

	/** 
	* Extracts a Dynamic Mesh from a Skeletal Mesh Asset. 
	*/
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|SkeletalMesh", meta = (ExpandEnumAsExecs = "Outcome"))
	static UPARAM(DisplayName = "Dynamic Mesh") UDynamicMesh* 
	CopyMeshFromSkeletalMesh(
		USkeletalMesh* FromSkeletalMeshAsset, 
		UDynamicMesh* ToDynamicMesh,
		FGeometryScriptCopyMeshFromAssetOptions AssetOptions,
		FGeometryScriptMeshReadLOD RequestedLOD,
		EGeometryScriptOutcomePins& Outcome,
		UGeometryScriptDebug* Debug = nullptr);

	/** 
	* Updates a Skeletal Mesh Asset with new geometry and bone weights data from a Dynamic Mesh.
	*/
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|SkeletalMesh", meta = (ExpandEnumAsExecs = "Outcome"))
	static UPARAM(DisplayName = "Dynamic Mesh") UDynamicMesh* 
	CopyMeshToSkeletalMesh(
		UDynamicMesh* FromDynamicMesh, 
		USkeletalMesh* ToSkeletalMeshAsset,
		FGeometryScriptCopyMeshToAssetOptions Options,
		FGeometryScriptMeshWriteLOD TargetLOD,
		EGeometryScriptOutcomePins& Outcome,
		UGeometryScriptDebug* Debug = nullptr);
};


