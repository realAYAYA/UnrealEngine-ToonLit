// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GeometryScript/GeometryScriptTypes.h"
#include "GeometryScript/GeometryScriptSelectionTypes.h"
#include "MeshTransformFunctions.generated.h"

class UDynamicMesh;


UCLASS(meta = (ScriptName = "GeometryScript_MeshTransforms"))
class GEOMETRYSCRIPTINGCORE_API UGeometryScriptLibrary_MeshTransformFunctions : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:

	UFUNCTION(BlueprintCallable, Category = "GeometryScript|Transforms", meta=(ScriptMethod, AdvancedDisplay = "bFixOrientationForNegativeScale"))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh*
	TransformMesh(
		UDynamicMesh* TargetMesh,
		FTransform Transform,
		bool bFixOrientationForNegativeScale = true,
		UGeometryScriptDebug* Debug = nullptr);

	UFUNCTION(BlueprintCallable, Category = "GeometryScript|Transforms", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	TranslateMesh(
		UDynamicMesh* TargetMesh,
		FVector Translation,
		UGeometryScriptDebug* Debug = nullptr );

	UFUNCTION(BlueprintCallable, Category = "GeometryScript|Transforms", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	RotateMesh(
		UDynamicMesh* TargetMesh,
		FRotator Rotation,
		FVector RotationOrigin = FVector(0,0,0),
		UGeometryScriptDebug* Debug = nullptr );

	UFUNCTION(BlueprintCallable, Category = "GeometryScript|Transforms", meta=(ScriptMethod, AdvancedDisplay = "bFixOrientationForNegativeScale"))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	ScaleMesh(
		UDynamicMesh* TargetMesh,
		FVector Scale = FVector(1,1,1),
		FVector ScaleOrigin = FVector(0,0,0),
		bool bFixOrientationForNegativeScale = true,
		UGeometryScriptDebug* Debug = nullptr);




	UFUNCTION(BlueprintCallable, Category = "GeometryScript|Transforms", meta=(ScriptMethod, AdvancedDisplay = "bFixOrientationForNegativeScale"))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh*
	TransformMeshSelection(
		UDynamicMesh* TargetMesh,
		FGeometryScriptMeshSelection Selection,
		FTransform Transform,
		UGeometryScriptDebug* Debug = nullptr);

	UFUNCTION(BlueprintCallable, Category = "GeometryScript|Transforms", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	TranslateMeshSelection(
		UDynamicMesh* TargetMesh,
		FGeometryScriptMeshSelection Selection,
		FVector Translation,
		UGeometryScriptDebug* Debug = nullptr );

	UFUNCTION(BlueprintCallable, Category = "GeometryScript|Transforms", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	RotateMeshSelection(
		UDynamicMesh* TargetMesh,
		FGeometryScriptMeshSelection Selection,
		FRotator Rotation,
		FVector RotationOrigin = FVector(0,0,0),
		UGeometryScriptDebug* Debug = nullptr );

	UFUNCTION(BlueprintCallable, Category = "GeometryScript|Transforms", meta=(ScriptMethod, AdvancedDisplay = "bFixOrientationForNegativeScale"))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	ScaleMeshSelection(
		UDynamicMesh* TargetMesh,
		FGeometryScriptMeshSelection Selection,
		FVector Scale = FVector(1,1,1),
		FVector ScaleOrigin = FVector(0,0,0),
		UGeometryScriptDebug* Debug = nullptr);


	/**
	 * Set the Pivot Location for the Mesh. Since the Pivot of a Mesh object is always the point at (0,0,0),
	 * this function simply translates the mesh by -PivotLocation.
	 */
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|Transforms", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	TranslatePivotToLocation(
		UDynamicMesh* TargetMesh,
		FVector PivotLocation,
		UGeometryScriptDebug* Debug = nullptr)
	{
		// note: this function is redundant, however *many* users do not intuitively understand the relationship
		// between "setting the pivot" and translation. This function is for those users.
		return TranslateMesh(TargetMesh, -PivotLocation, Debug);
	}

};