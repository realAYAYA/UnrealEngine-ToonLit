// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GeometryScript/GeometryScriptTypes.h"
#include "GeometryScript/GeometryScriptSelectionTypes.h"
#include "MeshSpatialFunctions.generated.h"

class UDynamicMesh;


USTRUCT(BlueprintType, meta = (DisplayName = "BVH Query Options"))
struct GEOMETRYSCRIPTINGCORE_API FGeometryScriptSpatialQueryOptions
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	float MaxDistance = 0;

	/** When true, allows the provided BHV to be used even if the mesh has been updated */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	bool bAllowUnsafeModifiedQueries = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	float WindingIsoThreshold = 0.5;
};





USTRUCT(BlueprintType, meta = (DisplayName = "Ray Hit Result"))
struct GEOMETRYSCRIPTINGCORE_API FGeometryScriptRayHitResult
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	bool bHit = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	float RayParameter = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	int HitTriangleID = -1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	FVector HitPosition = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	FVector HitBaryCoords = FVector::ZeroVector;
};



UCLASS(meta = (ScriptName = "GeometryScript_MeshSpatial"))
class GEOMETRYSCRIPTINGCORE_API UGeometryScriptLibrary_MeshSpatial : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:

	/**
	* Reset the Bounding Volume Hierarchy (BVH) by clearing all the internal data.
	*/
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|Spatial", meta = (ScriptMethod))
	static void ResetBVH(UPARAM(ref) FGeometryScriptDynamicMeshBVH& ResetBVH);
	
	/**
	* Builds a Bounding Volume Hierarchy (BVH) object for a mesh that can be used with multiple spatial queries.
	*/
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|Spatial", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	BuildBVHForMesh(
		UDynamicMesh* TargetMesh,
		FGeometryScriptDynamicMeshBVH& OutputBVH,
		UGeometryScriptDebug* Debug = nullptr );
	
	/**
	* Checks if the provided Bounding Volume Hierarchy (BVH) can still be used with the Mesh — it generally returns false if the mesh has been changed.
	*/
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|Spatial", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	IsBVHValidForMesh(
		UDynamicMesh* TargetMesh,
		UPARAM(ref) const FGeometryScriptDynamicMeshBVH& TestBVH,
		bool& bIsValid,
		UGeometryScriptDebug* Debug = nullptr );

	/**
	* Rebuilds the Bounding Volume Hierarchy (BVH) for the mesh in-place, which can reduce memory allocations, compared to building a new BVH.
	*/
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|Spatial", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	RebuildBVHForMesh(
		UDynamicMesh* TargetMesh,
		UPARAM(ref) FGeometryScriptDynamicMeshBVH& UpdateBVH,
		bool bOnlyIfInvalid = true,
		UGeometryScriptDebug* Debug = nullptr );

	/**
	* Finds the nearest point (Nearest Result) on the Target Mesh to a given 3D point (Query Point) by using the Query BVH.
	* @param QueryBVH a BVH associated with the Target Mesh
	* @param QueryPoint a 3D location relative to the local space of the mesh
	* @param NearestResult on return, holds the nearest point on the mesh to the QueryPoint
	* @param Outcome will be either Found or Not Found depending on the success of the query.
	* Note NearestResult.bValid will be false if the query failed. 
	*/
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|Spatial", meta=(ScriptMethod, ExpandEnumAsExecs = "Outcome"))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	FindNearestPointOnMesh(
		UDynamicMesh* TargetMesh,
		UPARAM(ref) const FGeometryScriptDynamicMeshBVH& QueryBVH,
		FVector QueryPoint,
		FGeometryScriptSpatialQueryOptions Options,
		FGeometryScriptTrianglePoint& NearestResult,
		EGeometryScriptSearchOutcomePins& Outcome,
		UGeometryScriptDebug* Debug = nullptr );

	/**
	* Finds the nearest intersection of a 3D ray with the mesh by using the Query BVH.  
	* Note, depending on the Ray Origin and Ray Direction, there is the possibility that the ray might not intersect with the Target Mesh.  
	* Should the ray miss, the HitResult.bHit will be false and the Outcome  will be Not Found.
	*/
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|Spatial", meta=(ScriptMethod, ExpandEnumAsExecs = "Outcome"))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	FindNearestRayIntersectionWithMesh(
		UDynamicMesh* TargetMesh,
		UPARAM(ref) const FGeometryScriptDynamicMeshBVH& QueryBVH,
		FVector RayOrigin,
		FVector RayDirection,
		FGeometryScriptSpatialQueryOptions Options,
		FGeometryScriptRayHitResult& HitResult,
		EGeometryScriptSearchOutcomePins& Outcome,
		UGeometryScriptDebug* Debug = nullptr );

	/**
	* Tests if a point is inside the mesh using the Fast Winding Number query and data stored in the BVH.
	* @param QueryBVH is an acceleration structure previously built with this mesh.
	* @param QueryPoint the point in the mesh's 3D local space.
	* @param Options control the fast winding number threshold 
	*/
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|Spatial", meta=(ScriptMethod, ExpandEnumAsExecs = "Outcome"))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	IsPointInsideMesh(
		UDynamicMesh* TargetMesh,
		UPARAM(ref) const FGeometryScriptDynamicMeshBVH& QueryBVH,
		FVector QueryPoint,
		FGeometryScriptSpatialQueryOptions Options,
		bool& bIsInside,
		EGeometryScriptContainmentOutcomePins& Outcome,
		UGeometryScriptDebug* Debug = nullptr );


	/**
	* Create Mesh Selection of mesh elements in TargetMesh contained by QueryBox, using QueryBVH to accellerate the computation.
	* Triangles are selected if MinNumTrianglePoints or more vertices are inside the box. 
	* PolyGroups are selected if any of their triangles are inside the box
	* 
	* @param QueryBVH is an acceleration structure previously built with TargetMesh.
	* @param QueryPoint the point in the mesh's 3D local space.
	* @param Options control the fast winding number threshold 
	*/
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|Spatial", meta=(ScriptMethod, AdvancedDisplay = "MinNumTrianglePoints"))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	SelectMeshElementsInBoxWithBVH(
		UDynamicMesh* TargetMesh,
		UPARAM(ref) const FGeometryScriptDynamicMeshBVH& QueryBVH,
		FBox QueryBox,
		FGeometryScriptSpatialQueryOptions Options,
		FGeometryScriptMeshSelection& Selection,
		EGeometryScriptMeshSelectionType SelectionType = EGeometryScriptMeshSelectionType::Vertices,
		int MinNumTrianglePoints = 3,
		UGeometryScriptDebug* Debug = nullptr );



};