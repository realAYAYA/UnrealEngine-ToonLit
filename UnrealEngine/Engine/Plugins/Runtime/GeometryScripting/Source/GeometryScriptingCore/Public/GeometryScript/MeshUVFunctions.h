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

	/**
	* Set the number of UV Channels on the Target Mesh. If not already enabled, this will enable the mesh attributes.
	*/
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|UVs", meta=(ScriptMethod), DisplayName = "Set Num UV Channels")
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	SetNumUVSets( 
		UDynamicMesh* TargetMesh, 
		UPARAM(DisplayName = "Num UV Channels") int NumUVSets,
		UGeometryScriptDebug* Debug = nullptr );

	/**
	* Copy the data in one UV Channel to another UV Channel on the same Target Mesh.
	*/
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|UVs", meta=(ScriptMethod), DisplayName = "Copy UV Channel")
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	CopyUVSet( 
		UDynamicMesh* TargetMesh, 
		UPARAM(DisplayName = "From UV Channel") int FromUVSet,
		UPARAM(DisplayName = "To UV Channel")   int ToUVSet,
		UGeometryScriptDebug* Debug = nullptr );

	/**
	 * Sets the UVs of a mesh triangle in the given UV Channel. 
	 * This function will create new UV elements for each vertex of the triangle, meaning that
	 * the triangle will become an isolated UV island.
	 */
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|UVs", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	SetMeshTriangleUVs( 
		UDynamicMesh* TargetMesh, 
		UPARAM(DisplayName = "UV Channel") int UVSetIndex,
		int TriangleID, 
		FGeometryScriptUVTriangle UVs,
		bool& bIsValidTriangle, 
		bool bDeferChangeNotifications = false );


	/**
	 * Adds a new UV Element to the specified UV Channel of the Mesh and returns a new UV Element ID.
	 */
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|MeshEdits", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	AddUVElementToMesh( 
		UDynamicMesh* TargetMesh, 
		UPARAM(DisplayName = "UV Channel") int32 UVSetIndex,
		FVector2D NewUVPosition, 
		UPARAM(DisplayName = "New UV Element ID") int& NewUVElementID,
		bool& bIsValidUVSet,
		bool bDeferChangeNotifications = false );

	/**
	 * Sets the UV Element IDs for a given Triangle in the specified UV Channel, ie the "UV Triangle" indices.
	 * This function does not create new UVs, the provided UV Elements must already.
	 * The UV Triangle can only be set if the resulting topology would be valid, ie the Elements cannot be shared
	 * between different base Mesh Vertices, so they must either be unused by any other triangles, or already associated
	 * with the same mesh vertex in other UV triangles. 
	 * If any conditions are not met, bIsValidTriangle will be returned as false.
	 */
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|UVs", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	SetMeshTriangleUVElementIDs( 
		UDynamicMesh* TargetMesh, 
		UPARAM(DisplayName = "UV Channel") int UVSetIndex,
		int TriangleID, 
		FIntVector TriangleUVElements,
		bool& bIsValidTriangle, 
		bool bDeferChangeNotifications = false );


	/**
	 * Returns the UV Element IDs associated with the three vertices of the triangle in the specified UV Channel.
	 * If the Triangle does not exist in the mesh or if no UVs are set in the specified UV Channel for the triangle, bHaveValidUVs will be returned as false.
	 */
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|UVs", meta = (ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	GetMeshTriangleUVElementIDs(
		UDynamicMesh* TargetMesh, 
		UPARAM(DisplayName = "UV Channel") int32 UVSetIndex, 
		int32 TriangleID, 
		FIntVector& TriangleUVElements,
		bool& bHaveValidUVs);

	/**
	 * Returns the UV Position for a given UV Element ID in the specified UV Channel.
	 * If the UV Set or Element ID does not exist, bIsValidElementID will be returned as false.
	 */
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|UVs", meta = (ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	GetMeshUVElementPosition(
		UDynamicMesh* TargetMesh, 
		UPARAM(DisplayName = "UV Channel") int32 UVSetIndex, 
		int32 ElementID, 
		FVector2D& UVPosition,
		bool& bIsValidElementID);

	/**
	 * Sets the UV position of a specific ElementID in the given UV Set/Channel
	 * If the UV Set or Element ID does not exist, bIsValidElementID will be returned as false.
	 */
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|UVs", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	SetMeshUVElementPosition( 
		UDynamicMesh* TargetMesh, 
		UPARAM(DisplayName = "UV Channel") int UVSetIndex,
		int ElementID, 
		FVector2D NewUVPosition,
		bool& bIsValidElementID, 
		bool bDeferChangeNotifications = false );

	/**
	* Update all selected UV values in the specified UV Channel by adding the Translation value to each.
	* If the provided Selection is empty, the Translation is applied to the entire UV Channel.
	*/
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|UVs", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	TranslateMeshUVs( 
		UDynamicMesh* TargetMesh, 
		UPARAM(DisplayName = "UV Channel") int UVSetIndex,
		FVector2D Translation,
		FGeometryScriptMeshSelection Selection,
		UGeometryScriptDebug* Debug = nullptr );
	
	/**
	* Update all selected UV values in the specified UV Channel by Scale, mathematically the new value is given by (UV - ScaleOrigin) * Scale + ScaleOrigin
	* If the provided Selection is empty, the update is applied to the entire UV Channel.
	*/
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|UVs", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	ScaleMeshUVs( 
		UDynamicMesh* TargetMesh, 
		UPARAM(DisplayName = "UV Channel") int UVSetIndex,
		FVector2D Scale,
		FVector2D ScaleOrigin,
		FGeometryScriptMeshSelection Selection,
		UGeometryScriptDebug* Debug = nullptr );

	/**
	* Update all the selected UV values in the specified UV Channel by a rotation of Rotation Angle (in degrees) relative to the Rotation Origin.
	* If the provided Selection is empty, the update is applied to the entire UV Channel.
	*/
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|UVs", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	RotateMeshUVs( 
		UDynamicMesh* TargetMesh, 
		UPARAM(DisplayName = "UV Channel") int UVSetIndex,
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
		UPARAM(DisplayName = "UV Channel") int UVSetIndex,
		FTransform PlaneTransform,
		FGeometryScriptMeshSelection Selection,
		UGeometryScriptDebug* Debug = nullptr );


	/**
	* Using Box Projection, update the UVs in the UV Channel for an entire mesh or a subset defined by a non-empty Selection.
	*/
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|UVs", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	SetMeshUVsFromBoxProjection( 
		UDynamicMesh* TargetMesh, 
		UPARAM(DisplayName = "UV Channel") int UVSetIndex,
		FTransform BoxTransform,
		FGeometryScriptMeshSelection Selection,
		int MinIslandTriCount = 2,
		UGeometryScriptDebug* Debug = nullptr );

	/**
	* Using Cylinder Projection, update the UVs in the UV Channel for an entire mesh or a subset defined by a non-empty Selection.
	*/
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|UVs", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	SetMeshUVsFromCylinderProjection( 
		UDynamicMesh* TargetMesh, 
		UPARAM(DisplayName = "UV Channel") int UVSetIndex,
		FTransform CylinderTransform,
		FGeometryScriptMeshSelection Selection,
		float SplitAngle = 45.0,
		UGeometryScriptDebug* Debug = nullptr );


	/**
	* Recomputes UVs in the UV Channel for a Mesh based on different types of well-defined UV islands, such as existing UV islands, PolyGroups, 
	* or a subset of the mesh based on a non-empty Selection.
	*/
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|UVs", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	RecomputeMeshUVs( 
		UDynamicMesh* TargetMesh, 
		UPARAM(DisplayName = "UV Channel") int UVSetIndex,
		FGeometryScriptRecomputeUVsOptions Options,
		FGeometryScriptMeshSelection Selection,
		UGeometryScriptDebug* Debug = nullptr );

	/**
	* Packs the existing UV islands in the specified UV Channel into standard UV space based on the Repack Options.
	*/
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|UVs", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	RepackMeshUVs( 
		UDynamicMesh* TargetMesh, 
		UPARAM(DisplayName = "UV Channel") int UVSetIndex,
		FGeometryScriptRepackUVsOptions RepackOptions,
		UGeometryScriptDebug* Debug = nullptr );

	/**
	* Computes new UVs for the specified UV Channel using PatchBuilder method in the Options, and optionally packs.
	*/
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|UVs", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	AutoGeneratePatchBuilderMeshUVs( 
		UDynamicMesh* TargetMesh, 
		UPARAM(DisplayName = "UV Channel") int UVSetIndex,
		FGeometryScriptPatchBuilderOptions Options,
		UGeometryScriptDebug* Debug = nullptr );

	/**
	* Computes new UVs for the specified UV Channel using XAtlas, and optionally packs.
	*/
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|UVs", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	AutoGenerateXAtlasMeshUVs( 
		UDynamicMesh* TargetMesh, 
		UPARAM(DisplayName = "UV Channel") int UVSetIndex,
		FGeometryScriptXAtlasOptions Options,
		UGeometryScriptDebug* Debug = nullptr );

	/**
	 * Compute information about dimensions and areas for a UV Set of a Mesh, with an optional Mesh Selection
	 * @param UVSetIndex index of UV Channel to query
	 * @param Selection subset of triangles to process, whole mesh is used if selection is not provided
	 * @param MeshArea output 3D area of queried triangles
	 * @param UVArea output 2D UV-space area of queried triangles
	 * @param MeshBounds output 3D bounding box of queried triangles
	 * @param UVBounds output 2D UV-space bounding box of queried triangles
	 * @param bIsValidUVSet output flag set to false if UV Channel does not exist on the target mesh. In this case Areas and Bounds are not initialized.
	 * @param bFoundUnsetUVs output flag set to true if any of the queried triangles do not have valid UVs set
	 * @param bOnlyIncludeValidUVTris if true, only triangles with valid UVs are included in 3D Mesh Area/Bounds
	 */
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|UVs", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Copy From Mesh") UDynamicMesh* 
	GetMeshUVSizeInfo(  
		UDynamicMesh* TargetMesh, 
		UPARAM(DisplayName = "UV Channel") int UVSetIndex,
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
	 * Get a list of single vertex UVs for each mesh vertex in the TargetMesh, derived from the specified UV Channel.
	 * The UV Channel may store multiple UVs for a single vertex (along UV seams)
	 * In such cases an arbitrary UV will be stored for that vertex, and bHasSplitUVs will be returned as true
	 * @param UVSetIndex index of UV Channel to read
	 * @param UVList output UV list will be stored here. Size will be equal to the MaxVertexID of TargetMesh  (not the VertexCount!)
	 * @param bIsValidUVSet will be set to true if the UV Channel was valid
	 * @param bHasVertexIDGaps will be set to true if some vertex indices in TargetMesh were invalid, ie MaxVertexID > VertexCount 
	 * @param bHasSplitUVs will be set to true if there were split UVs in the UV Channel
	 */
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|UVs", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	GetMeshPerVertexUVs( 
		UDynamicMesh* TargetMesh, 
		UPARAM(DisplayName = "UV Channel") int UVSetIndex,
		FGeometryScriptUVList& UVList, 
		bool& bIsValidUVSet,
		bool& bHasVertexIDGaps,
		bool& bHasSplitUVs,
		UGeometryScriptDebug* Debug = nullptr);


	/**
	 * Copy the 2D UVs from the given UV Channel in CopyFromMesh to the 3D vertex positions in CopyToUVMesh,
	 * with the triangle mesh topology defined by the UV Channel. Generally this "UV Mesh" topology will not
	 * be the same as the 3D mesh topology. PolyGroup IDs and Material IDs are preserved in the UVMesh.
	 * 
	 * 2D UV Positions are copied to 3D as (X, Y, 0) 
	 * 
	 * CopyMeshToMeshUVChannel will copy the 3D UV Mesh back to the UV Channel. This pair of functions can
	 * then be used to implement UV generation/editing via other mesh functions.
	 * 
	 * @param bInvalidTopology will be returned true if any topological issues were found
	 * @param bIsValidUVSet will be returned false if UVSetIndex is not available
	 */
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|UVs", meta=(ScriptMethod), DisplayName="Copy Mesh UV Channel To Mesh")
	static UPARAM(DisplayName = "Copy From Mesh") UDynamicMesh* 
	CopyMeshUVLayerToMesh(  
		UDynamicMesh* CopyFromMesh, 
		UPARAM(DisplayName = "UV Channel") int UVSetIndex,
		UPARAM(DisplayName = "Copy To UV Mesh", ref) UDynamicMesh* CopyToUVMesh, 
		UPARAM(DisplayName = "Copy To UV Mesh") UDynamicMesh*& CopyToUVMeshOut,
		bool& bInvalidTopology,
		bool& bIsValidUVSet,
		UGeometryScriptDebug* Debug = nullptr);

	/**
	 * Transfer the 3D vertex positions and triangles of CopyFromUVMesh to the given UV Channel identified by ToUVChannel of CopyToMesh.
	 * 3D positions (X,Y,Z) will be copied as UV positions (X,Y), ie Z is ignored.
	 * 
	 * bOnlyUVPositions controls whether only UV positions will be updated, or if the UV topology will be fully replaced.
	 * When false, CopyFromUVMesh must currently have a MaxVertexID <= that of the UV Channel MaxElementID
	 * When true, CopyFromUVMesh must currently have a MaxTriangleID <= that of CopyToMesh
	 * 
	 * @param bInvalidTopology will be returned true if any topological inconsistencies are found (but the operation will generally continue)
	 * @param bIsValidUVSet will be returned false if To UV Channel is not available
	 * @param bOnlyUVPositions if true, only (valid, matching) UV positions are updated, a full new UV topology is created
	 */
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|UVs", meta=(ScriptMethod), DisplayName="Copy Mesh To Mesh UV Channel")
	static UPARAM(DisplayName = "Copy From Mesh") UDynamicMesh* 
	CopyMeshToMeshUVLayer(  
		UDynamicMesh* CopyFromUVMesh, 
		UPARAM(DisplayName = "To UV Channel")  int ToUVSetIndex,
		UPARAM(DisplayName = "Copy To Mesh", ref) UDynamicMesh* CopyToMesh, 
		UPARAM(DisplayName = "Copy To Mesh") UDynamicMesh*& CopyToMeshOut,
		bool& bFoundTopologyErrors,
		UPARAM(DisplayName = "Is Valid UV Channel") bool& bIsValidUVSet,
		bool bOnlyUVPositions = true,
		UGeometryScriptDebug* Debug = nullptr);


	/**
	 * Compute local UV parameterization on TargetMesh vertices around the given CenterPoint / Triangle. This method
	 * uses a Discrete Exponential Map parameterization, which unwraps the mesh locally based on geodesic distances and angles.
	 * The CenterPoint will have UV value (0,0), and the computed vertex UVs will be such that Length(UV) == geodesic distance.
	 * 
	 * @param CenterPoint the center point of the parameterization. This point must lie on the triangle specified by CenterPointTriangleID
	 * @param CenterPointTriangleID the ID of the Triangle that contains CenterPoint
	 * @param Radius the parameterization will be computed out to this geodesic radius
	 * @param bUseInterpolatedNormal if true (default false), the normal frame used for the parameterization will be taken from the normal overlay, otherwise the CenterPointTriangleID normal will be used
	 * @param VertexIDs output list of VertexIDs that UVs have been computed for, ie are within geodesic distance Radius from the CenterPoint
	 * @param VertexUVs output list of Vertex UVs that corresponds to VertexIDs
	 */
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|UVs", meta=(ScriptMethod, AdvancedDisplay = "bUseInterpolatedNormal, TangentYDirection"))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	ComputeMeshLocalUVParam( 
		UDynamicMesh* TargetMesh, 
		FVector CenterPoint,
		int32 CenterPointTriangleID,
		TArray<int>& VertexIDs,
		TArray<FVector2D>& VertexUVs,
		double Radius = 1,
		bool bUseInterpolatedNormal = false,
		FVector TangentYDirection = FVector(0,0,0),
		double UVRotationDeg = 0.0,
		UGeometryScriptDebug* Debug = nullptr );


};