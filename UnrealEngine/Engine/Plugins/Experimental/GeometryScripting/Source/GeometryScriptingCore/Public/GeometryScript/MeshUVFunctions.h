// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GeometryScript/GeometryScriptTypes.h"
#include "GeometryScript/GeometryScriptSelectionTypes.h"
#include "MeshUVFunctions.generated.h"

class UDynamicMesh;


USTRUCT(BlueprintType)
struct GEOMETRYSCRIPTINGCORE_API FGeometryScriptRepackUVsOptions
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	int TargetImageWidth = 512;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	bool bOptimizeIslandRotation = true;
};

UENUM(BlueprintType)
enum class EGeometryScriptUVFlattenMethod : uint8
{
	ExpMap = 0,
	Conformal = 1,
	SpectralConformal = 2
};

UENUM(BlueprintType)
enum class EGeometryScriptUVIslandSource : uint8
{
	PolyGroups = 0,
	UVIslands = 1
};


USTRUCT(BlueprintType)
struct GEOMETRYSCRIPTINGCORE_API FGeometryScriptExpMapUVOptions
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	int NormalSmoothingRounds = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	float NormalSmoothingAlpha = 0.25f;
};

USTRUCT(BlueprintType)
struct GEOMETRYSCRIPTINGCORE_API FGeometryScriptSpectralConformalUVOptions
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	bool bPreserveIrregularity = true;
};


USTRUCT(BlueprintType)
struct GEOMETRYSCRIPTINGCORE_API FGeometryScriptRecomputeUVsOptions
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	EGeometryScriptUVFlattenMethod Method = EGeometryScriptUVFlattenMethod::SpectralConformal;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	EGeometryScriptUVIslandSource IslandSource = EGeometryScriptUVIslandSource::UVIslands;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	FGeometryScriptExpMapUVOptions ExpMapOptions;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	FGeometryScriptSpectralConformalUVOptions SpectralConformalOptions;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	FGeometryScriptGroupLayer GroupLayer;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	bool bAutoAlignIslandsWithAxes = true;
};





USTRUCT(BlueprintType)
struct GEOMETRYSCRIPTINGCORE_API FGeometryScriptPatchBuilderOptions
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	int InitialPatchCount = 100;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	int MinPatchSize = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	float PatchCurvatureAlignmentWeight = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	float PatchMergingMetricThresh = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	float PatchMergingAngleThresh = 45.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	FGeometryScriptExpMapUVOptions ExpMapOptions;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	bool bRespectInputGroups = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	FGeometryScriptGroupLayer GroupLayer;


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	bool bAutoPack = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	FGeometryScriptRepackUVsOptions PackingOptions;
};




USTRUCT(BlueprintType)
struct GEOMETRYSCRIPTINGCORE_API FGeometryScriptXAtlasOptions
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	int MaxIterations = 2;
};



UCLASS(meta = (ScriptName = "GeometryScript_UVs"))
class GEOMETRYSCRIPTINGCORE_API UGeometryScriptLibrary_MeshUVFunctions : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:

	UFUNCTION(BlueprintCallable, Category = "GeometryScript|UVs", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	SetNumUVSets( 
		UDynamicMesh* TargetMesh, 
		int NumUVSets,
		UGeometryScriptDebug* Debug = nullptr );

	UFUNCTION(BlueprintCallable, Category = "GeometryScript|UVs", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	CopyUVSet( 
		UDynamicMesh* TargetMesh, 
		int FromUVSet,
		int ToUVSet,
		UGeometryScriptDebug* Debug = nullptr );


	UFUNCTION(BlueprintCallable, Category = "GeometryScript|UVs", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	SetMeshTriangleUVs( 
		UDynamicMesh* TargetMesh, 
		int UVSetIndex,
		int TriangleID, 
		FGeometryScriptUVTriangle UVs,
		bool& bIsValidTriangle, 
		bool bDeferChangeNotifications = false );


	UFUNCTION(BlueprintCallable, Category = "GeometryScript|UVs", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	TranslateMeshUVs( 
		UDynamicMesh* TargetMesh, 
		int UVSetIndex,
		FVector2D Translation,
		FGeometryScriptMeshSelection Selection,
		UGeometryScriptDebug* Debug = nullptr );

	UFUNCTION(BlueprintCallable, Category = "GeometryScript|UVs", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	ScaleMeshUVs( 
		UDynamicMesh* TargetMesh, 
		int UVSetIndex,
		FVector2D Scale,
		FVector2D ScaleOrigin,
		FGeometryScriptMeshSelection Selection,
		UGeometryScriptDebug* Debug = nullptr );

	UFUNCTION(BlueprintCallable, Category = "GeometryScript|UVs", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	RotateMeshUVs( 
		UDynamicMesh* TargetMesh, 
		int UVSetIndex,
		float RotationAngle,
		FVector2D RotationOrigin,
		FGeometryScriptMeshSelection Selection,
		UGeometryScriptDebug* Debug = nullptr );


	/**
	 * Scale of PlaneTransform defines world-space dimension that maps to 1 UV dimension
	 */
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|UVs", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	SetMeshUVsFromPlanarProjection( 
		UDynamicMesh* TargetMesh, 
		int UVSetIndex,
		FTransform PlaneTransform,
		FGeometryScriptMeshSelection Selection,
		UGeometryScriptDebug* Debug = nullptr );


	UFUNCTION(BlueprintCallable, Category = "GeometryScript|UVs", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	SetMeshUVsFromBoxProjection( 
		UDynamicMesh* TargetMesh, 
		int UVSetIndex,
		FTransform BoxTransform,
		FGeometryScriptMeshSelection Selection,
		int MinIslandTriCount = 2,
		UGeometryScriptDebug* Debug = nullptr );


	UFUNCTION(BlueprintCallable, Category = "GeometryScript|UVs", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	SetMeshUVsFromCylinderProjection( 
		UDynamicMesh* TargetMesh, 
		int UVSetIndex,
		FTransform CylinderTransform,
		FGeometryScriptMeshSelection Selection,
		float SplitAngle = 45.0,
		UGeometryScriptDebug* Debug = nullptr );



	UFUNCTION(BlueprintCallable, Category = "GeometryScript|UVs", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	RecomputeMeshUVs( 
		UDynamicMesh* TargetMesh, 
		int UVSetIndex,
		FGeometryScriptRecomputeUVsOptions Options,
		FGeometryScriptMeshSelection Selection,
		UGeometryScriptDebug* Debug = nullptr );


	UFUNCTION(BlueprintCallable, Category = "GeometryScript|UVs", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	RepackMeshUVs( 
		UDynamicMesh* TargetMesh, 
		int UVSetIndex,
		FGeometryScriptRepackUVsOptions RepackOptions,
		UGeometryScriptDebug* Debug = nullptr );

	UFUNCTION(BlueprintCallable, Category = "GeometryScript|UVs", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	AutoGeneratePatchBuilderMeshUVs( 
		UDynamicMesh* TargetMesh, 
		int UVSetIndex,
		FGeometryScriptPatchBuilderOptions Options,
		UGeometryScriptDebug* Debug = nullptr );

	UFUNCTION(BlueprintCallable, Category = "GeometryScript|UVs", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	AutoGenerateXAtlasMeshUVs( 
		UDynamicMesh* TargetMesh, 
		int UVSetIndex,
		FGeometryScriptXAtlasOptions Options,
		UGeometryScriptDebug* Debug = nullptr );

	/**
	 * Compute information about dimensions and areas for a UV Set of a Mesh, with an optional Mesh Selection
	 * @param UVSetIndex index of UV Set to query
	 * @param Selection subset of triangles to process, whole mesh is used if selection is not provided
	 * @param MeshArea output 3D area of queried triangles
	 * @param UVArea output 2D UV-space area of queried triangles
	 * @param MeshBounds output 3D bounding box of queried triangles
	 * @param UVBounds output 2D UV-space bounding box of queried triangles
	 * @param bIsValidUVSet output flag set to false if UVSetIndex does not exist on the target mesh. In this case Areas and Bounds are not initialized.
	 * @param bFoundUnsetUVs output flag set to true if any of the queried triangles do not have valid UVs set
	 * @param bOnlyIncludeValidUVTris if true, only triangles with valid UVs are included in 3D Mesh Area/Bounds
	 */
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|UVs", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Copy From Mesh") UDynamicMesh* 
	GetMeshUVSizeInfo(  
		UDynamicMesh* TargetMesh, 
		int UVSetIndex,
		FGeometryScriptMeshSelection Selection,
		double& MeshArea,
		double& UVArea,
		FBox& MeshBounds,
		FBox2D& UVBounds,
		bool& bIsValidUVSet,
		bool& bFoundUnsetUVs,
		bool bOnlyIncludeValidUVTris = true,
		UGeometryScriptDebug* Debug = nullptr);	


	/**
	 * Get a list of single vertex UVs for each mesh vertex in the TargetMesh, derived from the specified UV Overlay.
	 * The UV Overlay may store multiple UVs for a single vertex (along UV seams)
	 * In such cases an arbitrary UV will be stored for that vertex, and bHasSplitUVs will be returned as true
	 * @param UVSetIndex index of UV Set to read
	 * @param UVList output UV list will be stored here. Size will be equal to the MaxVertexID of TargetMesh  (not the VertexCount!)
	 * @param bIsValidUVSet will be set to true if the UV Overlay was valid
	 * @param bHasVertexIDGaps will be set to true if some vertex indices in TargetMesh were invalid, ie MaxVertexID > VertexCount 
	 * @param bHasSplitUVs will be set to true if there were split UVs in the UV overlay
	 */
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|UVs", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	GetMeshPerVertexUVs( 
		UDynamicMesh* TargetMesh, 
		int UVSetIndex,
		FGeometryScriptUVList& UVList, 
		bool& bIsValidUVSet,
		bool& bHasVertexIDGaps,
		bool& bHasSplitUVs,
		UGeometryScriptDebug* Debug = nullptr);


	/**
	 * Copy the 2D UVs from the given UVSetIndex in CopyFromMesh to the 3D vertex positions in CopyToUVMesh,
	 * with the triangle mesh topolgoy defined by the UV Set. Generally this "UV Mesh" topolgoy will not
	 * be the same as the 3D mesh topology. Polygroup IDs and Material IDs are preserved in the UVMesh.
	 * 
	 * 2D UV Positions are copied to 3D as (X, Y, 0) 
	 * 
	 * CopyMeshToMeshUVLayer will copy the 3D UV Mesh back to the UV Set. This pair of functions can
	 * then be used to implement UV generation/editing via other mesh functions.
	 * 
	 * @param bInvalidTopology will be returned true if any topological issues were found
	 * @param bIsValidUVSet will be returned false if UVSetIndex is not available
	 */
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|UVs", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Copy From Mesh") UDynamicMesh* 
	CopyMeshUVLayerToMesh(  
		UDynamicMesh* CopyFromMesh, 
		int UVSetIndex,
		UPARAM(DisplayName = "Copy To UV Mesh", ref) UDynamicMesh* CopyToUVMesh, 
		UPARAM(DisplayName = "Copy To UV Mesh") UDynamicMesh*& CopyToUVMeshOut,
		bool& bInvalidTopology,
		bool& bIsValidUVSet,
		UGeometryScriptDebug* Debug = nullptr);

	/**
	 * Transfer the 3D vertex positions and triangles of CopyFromUVMesh to the given UV Layer identified by ToUVSetIndex of CopyToMesh.
	 * 3D positions (X,Y,Z) will be copied as UV positions (X,Y), ie Z is ignored.
	 * 
	 * bOnlyUVPositions controls whether only UV positions will be updated, or if the UV topology will be fully replaced.
	 * When false, CopyFromUVMesh must currently have a MaxVertexID <= that of the UV Layer MaxElementID
	 * When true, CopyFromUVMesh must currently have a MaxTriangleID <= that of CopyToMesh
	 * 
	 * @param bInvalidTopology will be returned true if any topological inconsistencies are found (but the operation will generally continue)
	 * @param bIsValidUVSet will be returned false if ToUVSetIndex is not available
	 * @param bOnlyUVPositions if true, only (valid, matching) UV positions are updated, a full new UV topology is created
	 */
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|UVs", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Copy From Mesh") UDynamicMesh* 
	CopyMeshToMeshUVLayer(  
		UDynamicMesh* CopyFromUVMesh, 
		int ToUVSetIndex,
		UPARAM(DisplayName = "Copy To Mesh", ref) UDynamicMesh* CopyToMesh, 
		UPARAM(DisplayName = "Copy To Mesh") UDynamicMesh*& CopyToMeshOut,
		bool& bFoundTopologyErrors,
		bool& bIsValidUVSet,
		bool bOnlyUVPositions = true,
		UGeometryScriptDebug* Debug = nullptr);


};