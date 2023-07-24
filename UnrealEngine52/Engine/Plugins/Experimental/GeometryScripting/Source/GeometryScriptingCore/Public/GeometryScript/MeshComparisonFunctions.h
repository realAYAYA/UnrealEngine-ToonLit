// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GeometryScript/GeometryScriptTypes.h"
#include "MeshComparisonFunctions.generated.h"

class UDynamicMesh;


USTRUCT(BlueprintType)
struct GEOMETRYSCRIPTINGCORE_API FGeometryScriptIsSameMeshOptions
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	bool bCheckConnectivity = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	bool bCheckEdgeIDs = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	bool bCheckNormals = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	bool bCheckColors = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	bool bCheckUVs = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	bool bCheckGroups = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	bool bCheckAttributes = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	float Epsilon = 1e-06;
};



USTRUCT(BlueprintType)
struct GEOMETRYSCRIPTINGCORE_API FGeometryScriptMeasureMeshDistanceOptions
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	bool bSymmetric = true;
};



UCLASS(meta = (ScriptName = "GeometryScript_MeshComparison"))
class GEOMETRYSCRIPTINGCORE_API UGeometryScriptLibrary_MeshComparisonFunctions : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:

	UFUNCTION(BlueprintCallable, Category = "GeometryScript|Comparison", meta = (ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh*
	IsSameMeshAs(
		UDynamicMesh* TargetMesh,
		UDynamicMesh* OtherMesh,
		FGeometryScriptIsSameMeshOptions Options,
		bool &bIsSameMesh,
		UGeometryScriptDebug* Debug = nullptr);

	UFUNCTION(BlueprintCallable, Category = "GeometryScript|Comparison", meta = (ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh*
	MeasureDistancesBetweenMeshes(
		UDynamicMesh* TargetMesh,
		UDynamicMesh* OtherMesh,
		FGeometryScriptMeasureMeshDistanceOptions Options,
		double& MaxDistance,
		double& MinDistance,
		double& AverageDistance,
		double& RootMeanSqrDeviation,
		UGeometryScriptDebug* Debug = nullptr);

	UFUNCTION(BlueprintCallable, Category = "GeometryScript|Comparison", meta = (ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh*
	IsIntersectingMesh(
		UDynamicMesh* TargetMesh,
		FTransform TargetTransform,
		UDynamicMesh* OtherMesh,
		FTransform OtherTransform,
		bool &bIsIntersecting,
		UGeometryScriptDebug* Debug = nullptr);

};