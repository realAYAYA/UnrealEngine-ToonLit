// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GeometryScript/GeometryScriptTypes.h"
#include "MeshSpatialFunctions.generated.h"

class UDynamicMesh;


USTRUCT(BlueprintType, meta = (DisplayName = "BVH Query Options"))
struct GEOMETRYSCRIPTINGCORE_API FGeometryScriptSpatialQueryOptions
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	float MaxDistance = 0;

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
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|Spatial", meta = (ScriptMethod))
	static void ResetBVH(UPARAM(ref) FGeometryScriptDynamicMeshBVH& ResetBVH);

	UFUNCTION(BlueprintCallable, Category = "GeometryScript|Spatial", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	BuildBVHForMesh(
		UDynamicMesh* TargetMesh,
		FGeometryScriptDynamicMeshBVH& OutputBVH,
		UGeometryScriptDebug* Debug = nullptr );

	UFUNCTION(BlueprintCallable, Category = "GeometryScript|Spatial", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	IsBVHValidForMesh(
		UDynamicMesh* TargetMesh,
		UPARAM(ref) const FGeometryScriptDynamicMeshBVH& TestBVH,
		bool& bIsValid,
		UGeometryScriptDebug* Debug = nullptr );

	UFUNCTION(BlueprintCallable, Category = "GeometryScript|Spatial", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	RebuildBVHForMesh(
		UDynamicMesh* TargetMesh,
		UPARAM(ref) FGeometryScriptDynamicMeshBVH& UpdateBVH,
		bool bOnlyIfInvalid = true,
		UGeometryScriptDebug* Debug = nullptr );

	UFUNCTION(BlueprintCallable, Category = "GeometryScript|Spatial", meta=(ScriptMethod, ExpandEnumAsExecs = "Outcome"))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	FindNearestPointOnMesh(
		UDynamicMesh* TargetMesh,
		UPARAM(ref) const FGeometryScriptDynamicMeshBVH& QueryBVH,
		FVector QueryPoint,
		FGeometryScriptSpatialQueryOptions Options,
		FGeometryScriptTrianglePoint& NearestResult,
		TEnumAsByte<EGeometryScriptSearchOutcomePins>& Outcome,
		UGeometryScriptDebug* Debug = nullptr );


	UFUNCTION(BlueprintCallable, Category = "GeometryScript|Spatial", meta=(ScriptMethod, ExpandEnumAsExecs = "Outcome"))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	FindNearestRayIntersectionWithMesh(
		UDynamicMesh* TargetMesh,
		UPARAM(ref) const FGeometryScriptDynamicMeshBVH& QueryBVH,
		FVector RayOrigin,
		FVector RayDirection,
		FGeometryScriptSpatialQueryOptions Options,
		FGeometryScriptRayHitResult& HitResult,
		TEnumAsByte<EGeometryScriptSearchOutcomePins>& Outcome,
		UGeometryScriptDebug* Debug = nullptr );


	UFUNCTION(BlueprintCallable, Category = "GeometryScript|Spatial", meta=(ScriptMethod, ExpandEnumAsExecs = "Outcome"))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	IsPointInsideMesh(
		UDynamicMesh* TargetMesh,
		UPARAM(ref) const FGeometryScriptDynamicMeshBVH& QueryBVH,
		FVector QueryPoint,
		FGeometryScriptSpatialQueryOptions Options,
		bool& bIsInside,
		TEnumAsByte<EGeometryScriptContainmentOutcomePins>& Outcome,
		UGeometryScriptDebug* Debug = nullptr );

};