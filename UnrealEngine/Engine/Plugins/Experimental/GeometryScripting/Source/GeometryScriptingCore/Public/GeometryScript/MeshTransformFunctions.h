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


};