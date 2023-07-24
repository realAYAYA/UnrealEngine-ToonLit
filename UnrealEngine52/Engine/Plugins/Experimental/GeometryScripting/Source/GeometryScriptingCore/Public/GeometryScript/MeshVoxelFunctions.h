// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GeometryScript/GeometryScriptTypes.h"
#include "MeshVoxelFunctions.generated.h"

class UDynamicMesh;

UENUM(BlueprintType)
enum class EGeometryScriptGridSizingMethod : uint8
{
	GridCellSize = 0,
	GridResolution = 1
};


/***
 * Parameters for 3D grids, eg grids used for sampling, SDFs, voxelization, etc
 */
USTRUCT(BlueprintType)
struct GEOMETRYSCRIPTINGCORE_API FGeometryScript3DGridParameters
{
	GENERATED_BODY()
public:
	/** SizeMethod determines how the parameters below will be interpreted to define the size of a 3D sampling/voxel grid */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	EGeometryScriptGridSizingMethod SizeMethod = EGeometryScriptGridSizingMethod::GridResolution;

	/** Use a specific grid cell size, and construct a grid with dimensions large enough to contain the target object */
	UPROPERTY(BlueprintReadWrite, Category = Options, meta = (EditCondition = "SizeMethod == EGeometryScriptGridSizingMethod::GridCellSize"));
	float GridCellSize = 0.5;

	/** Use a specific grid resolution, with the grid cell size derived form the target object bounds such that this is the number of cells along the longest box dimension */
	UPROPERTY(BlueprintReadWrite, Category = Options, meta = (EditCondition = "SizeMethod == EGeometryScriptGridSizingMethod::GridResolution"))
	int GridResolution = 64;
};


USTRUCT(BlueprintType)
struct GEOMETRYSCRIPTINGCORE_API FGeometryScriptSolidifyOptions
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	FGeometryScript3DGridParameters GridParameters;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	float WindingThreshold = 0.5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	bool bSolidAtBoundaries = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	float ExtendBounds = 1.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	int SurfaceSearchSteps = 3;

	/** When enabled, regions of the input mesh that have open boundaries (ie "shells") are thickened by extruding them into closed solids. This may be expensive on large meshes. */
	UPROPERTY(BlueprintReadWrite, Category = Options)
	bool bThickenShells = false;

	/** Open Shells are Thickened by offsetting vertices along their averaged vertex normals by this amount. Dimension is but clamped to twice the grid cell size. */
	UPROPERTY(BlueprintReadWrite, Category = Options)
	double ShellThickness = 1.0;

};


UENUM(BlueprintType)
enum class EGeometryScriptMorphologicalOpType : uint8
{
	/** Expand the shapes outward */
	Dilate = 0,
	/** Shrink the shapes inward */
	Contract = 1,
	/** Dilate and then contract, to delete small negative features (sharp inner corners, small holes) */
	Close = 2,
	/** Contract and then dilate, to delete small positive features (sharp outer corners, small isolated pieces) */
	Open = 3

	// note: keep above compatible with TImplicitMorphology
};




USTRUCT(BlueprintType)
struct GEOMETRYSCRIPTINGCORE_API FGeometryScriptMorphologyOptions
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	FGeometryScript3DGridParameters SDFGridParameters;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	bool bUseSeparateMeshGrid = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	FGeometryScript3DGridParameters MeshGridParameters;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	EGeometryScriptMorphologicalOpType Operation = EGeometryScriptMorphologicalOpType::Dilate;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	float Distance = 1.0;

};




UCLASS(meta = (ScriptName = "GeometryScript_MeshVoxelProcessing"))
class GEOMETRYSCRIPTINGCORE_API UGeometryScriptLibrary_MeshVoxelFunctions : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:

	UFUNCTION(BlueprintCallable, Category = "GeometryScript|Voxel", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh*
	ApplyMeshSolidify(
		UDynamicMesh* TargetMesh,
		FGeometryScriptSolidifyOptions Options,
		UGeometryScriptDebug* Debug = nullptr);

	UFUNCTION(BlueprintCallable, Category = "GeometryScript|Voxel", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh*
	ApplyMeshMorphology(
		UDynamicMesh* TargetMesh,
		FGeometryScriptMorphologyOptions Options,
		UGeometryScriptDebug* Debug = nullptr);

};