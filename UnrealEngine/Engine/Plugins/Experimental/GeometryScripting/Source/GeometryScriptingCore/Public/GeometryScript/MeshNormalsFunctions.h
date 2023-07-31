// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GeometryScript/GeometryScriptTypes.h"
#include "MeshNormalsFunctions.generated.h"

class UDynamicMesh;

USTRUCT(BlueprintType)
struct GEOMETRYSCRIPTINGCORE_API FGeometryScriptCalculateNormalsOptions
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	bool bAngleWeighted = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	bool bAreaWeighted = true;
};



USTRUCT(BlueprintType)
struct GEOMETRYSCRIPTINGCORE_API FGeometryScriptSplitNormalsOptions
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	bool bSplitByOpeningAngle = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	float OpeningAngleDeg = 15.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	bool bSplitByFaceGroup = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	FGeometryScriptGroupLayer GroupLayer;
};


UENUM(BlueprintType)
enum class EGeometryScriptTangentTypes : uint8
{
	FastMikkT = 0,
	PerTriangle = 1
};


USTRUCT(BlueprintType)
struct GEOMETRYSCRIPTINGCORE_API FGeometryScriptTangentsOptions
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	EGeometryScriptTangentTypes Type = EGeometryScriptTangentTypes::FastMikkT;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	int UVLayer = 0;
};



UCLASS(meta = (ScriptName = "GeometryScript_Normals"))
class GEOMETRYSCRIPTINGCORE_API UGeometryScriptLibrary_MeshNormalsFunctions : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:

	UFUNCTION(BlueprintCallable, Category = "GeometryScript|Normals", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	FlipNormals( 
		UDynamicMesh* TargetMesh, 
		UGeometryScriptDebug* Debug = nullptr);

	UFUNCTION(BlueprintCallable, Category = "GeometryScript|Normals", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	AutoRepairNormals( 
		UDynamicMesh* TargetMesh, 
		UGeometryScriptDebug* Debug = nullptr);

	UFUNCTION(BlueprintCallable, Category = "GeometryScript|Normals", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	SetPerVertexNormals( 
		UDynamicMesh* TargetMesh, 
		UGeometryScriptDebug* Debug = nullptr);

	UFUNCTION(BlueprintCallable, Category = "GeometryScript|Normals", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	SetPerFaceNormals( 
		UDynamicMesh* TargetMesh, 
		UGeometryScriptDebug* Debug = nullptr);

	UFUNCTION(BlueprintCallable, Category = "GeometryScript|Normals", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	RecomputeNormals(  
		UDynamicMesh* TargetMesh, 
		FGeometryScriptCalculateNormalsOptions CalculateOptions,
		UGeometryScriptDebug* Debug = nullptr);

	UFUNCTION(BlueprintCallable, Category = "GeometryScript|Normals", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	ComputeSplitNormals( 
		UDynamicMesh* TargetMesh, 
		FGeometryScriptSplitNormalsOptions SplitOptions,
		FGeometryScriptCalculateNormalsOptions CalculateOptions,
		UGeometryScriptDebug* Debug = nullptr);

	UFUNCTION(BlueprintCallable, Category = "GeometryScript|Normals", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	ComputeTangents( 
		UDynamicMesh* TargetMesh, 
		FGeometryScriptTangentsOptions Options,
		UGeometryScriptDebug* Debug = nullptr);


	UFUNCTION(BlueprintCallable, Category = "GeometryScript|Normals", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	SetMeshTriangleNormals( 
		UDynamicMesh* TargetMesh, 
		int TriangleID, 
		FGeometryScriptTriangle Normals,
		bool& bIsValidTriangle, 
		bool bDeferChangeNotifications = false );



	/**
	 * Get a list of single normal vectors for each mesh vertex in the TargetMesh, derived from the Normals Overlay.
	 * The Normals Overlay may store multiple normals for a single vertex (ie split normals)
	 * In such cases the normals can either be averaged, or the last normal seen will be used, depending on the bAverageSplitVertexValues parameter.
	 * @param NormalList output normal list will be stored here. Size will be equal to the MaxVertexID of TargetMesh  (not the VertexCount!)
	 * @param bIsValidNormalSet will be set to true if the Normal Overlay was valid
	 * @param bHasVertexIDGaps will be set to true if some vertex indices in TargetMesh were invalid, ie MaxVertexID > VertexCount 
	 * @param bAverageSplitVertexValues control how multiple normals at the same vertex should be interpreted
	 */
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|Normals", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	GetMeshPerVertexNormals( 
		UDynamicMesh* TargetMesh, 
		FGeometryScriptVectorList& NormalList, 
		bool& bIsValidNormalSet,
		bool& bHasVertexIDGaps,
		bool bAverageSplitVertexValues = true);

};