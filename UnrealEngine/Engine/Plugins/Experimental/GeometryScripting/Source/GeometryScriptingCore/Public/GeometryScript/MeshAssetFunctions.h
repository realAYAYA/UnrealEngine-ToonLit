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
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	bool bApplyBuildSettings = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	bool bRequestTangents = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	bool bIgnoreRemoveDegenerates = true;
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

	UFUNCTION(BlueprintCallable, Category = "GeometryScript|StaticMesh", meta = (ExpandEnumAsExecs = "Outcome"))
	static UPARAM(DisplayName = "Dynamic Mesh") UDynamicMesh* 
	CopyMeshFromStaticMesh(
		UStaticMesh* FromStaticMeshAsset, 
		UDynamicMesh* ToDynamicMesh, 
		FGeometryScriptCopyMeshFromAssetOptions AssetOptions,
		FGeometryScriptMeshReadLOD RequestedLOD,
		TEnumAsByte<EGeometryScriptOutcomePins>& Outcome,
		UGeometryScriptDebug* Debug = nullptr);


	UFUNCTION(BlueprintCallable, Category = "GeometryScript|StaticMesh", meta = (ExpandEnumAsExecs = "Outcome"))
	static UPARAM(DisplayName = "Dynamic Mesh") UDynamicMesh* 
	CopyMeshToStaticMesh(
		UDynamicMesh* FromDynamicMesh, 
		UStaticMesh* ToStaticMeshAsset, 
		FGeometryScriptCopyMeshToAssetOptions Options,
		FGeometryScriptMeshWriteLOD TargetLOD,
		TEnumAsByte<EGeometryScriptOutcomePins>& Outcome,
		UGeometryScriptDebug* Debug = nullptr);



	UFUNCTION(BlueprintCallable, Category = "GeometryScript|StaticMesh", meta = (ExpandEnumAsExecs = "Outcome"))
	static void
	GetSectionMaterialListFromStaticMesh(
		UStaticMesh* FromStaticMeshAsset, 
		FGeometryScriptMeshReadLOD RequestedLOD,
		TArray<UMaterialInterface*>& MaterialList,
		TArray<int32>& MaterialIndex,
		TEnumAsByte<EGeometryScriptOutcomePins>& Outcome,
		UGeometryScriptDebug* Debug = nullptr);


	UFUNCTION(BlueprintCallable, Category = "GeometryScript|SkeletalMesh", meta = (ExpandEnumAsExecs = "Outcome"))
	static UPARAM(DisplayName = "Dynamic Mesh") UDynamicMesh* 
	CopyMeshFromSkeletalMesh(
		USkeletalMesh* FromSkeletalMeshAsset, 
		UDynamicMesh* ToDynamicMesh,
		FGeometryScriptCopyMeshFromAssetOptions AssetOptions,
		FGeometryScriptMeshReadLOD RequestedLOD,
		TEnumAsByte<EGeometryScriptOutcomePins>& Outcome,
		UGeometryScriptDebug* Debug = nullptr);


	UFUNCTION(BlueprintCallable, Category = "GeometryScript|SkeletalMesh", meta = (ExpandEnumAsExecs = "Outcome"))
	static UPARAM(DisplayName = "Dynamic Mesh") UDynamicMesh* 
	CopyMeshToSkeletalMesh(
		UDynamicMesh* FromDynamicMesh, 
		USkeletalMesh* ToSkeletalMeshAsset,
		FGeometryScriptCopyMeshToAssetOptions Options,
		FGeometryScriptMeshWriteLOD TargetLOD,
		TEnumAsByte<EGeometryScriptOutcomePins>& Outcome,
		UGeometryScriptDebug* Debug = nullptr);
};


