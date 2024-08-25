// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GeometryScript/GeometryScriptTypes.h"
#include "MeshSimplifyFunctions.generated.h"

class UDynamicMesh;

USTRUCT(BlueprintType)
struct GEOMETRYSCRIPTINGCORE_API FGeometryScriptPlanarSimplifyOptions
{
	GENERATED_BODY()
public:
	/** */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	float AngleThreshold = 0.001;

	/** If enabled, the simplified mesh is automatically compacted to remove gaps in the index space. This is expensive and can be disabled by advanced users. */
	UPROPERTY(BlueprintReadWrite, Category = Options)
	bool bAutoCompact = true;
};

USTRUCT(BlueprintType)
struct GEOMETRYSCRIPTINGCORE_API FGeometryScriptPolygroupSimplifyOptions
{
	GENERATED_BODY()
public:
	/** */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	float AngleThreshold = 0.001;

	/** If enabled, the simplified mesh is automatically compacted to remove gaps in the index space. This is expensive and can be disabled by advanced users. */
	UPROPERTY(BlueprintReadWrite, Category = Options)
	bool bAutoCompact = true;
};


UENUM(BlueprintType)
enum class EGeometryScriptRemoveMeshSimplificationType : uint8
{
	StandardQEM = 0,
	VolumePreserving = 1,
	AttributeAware = 2
};


USTRUCT(Blueprintable)
struct GEOMETRYSCRIPTINGCORE_API FGeometryScriptSimplifyMeshOptions
{
	GENERATED_BODY()
public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	EGeometryScriptRemoveMeshSimplificationType Method = EGeometryScriptRemoveMeshSimplificationType::AttributeAware;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	bool bAllowSeamCollapse = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	bool bAllowSeamSmoothing = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	bool bAllowSeamSplits = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	bool bPreserveVertexPositions = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	bool bRetainQuadricMemory = false;

	/** If enabled, the simplified mesh is automatically compacted to remove gaps in the index space. This is expensive and can be disabled by advanced users. */
	UPROPERTY(BlueprintReadWrite, Category = Options)
	bool bAutoCompact = true;
};




UCLASS(meta = (ScriptName = "GeometryScript_MeshSimplification"))
class GEOMETRYSCRIPTINGCORE_API UGeometryScriptLibrary_MeshSimplifyFunctions : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:

	/**
	* Simplifies planar areas of the mesh that have more triangles than necessary. Note that it does not change the 3D shape of the mesh.
	* Planar regions are identified by comparison of face normals using a Angle Threshold in the Options.
	*/
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|Simplification", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	ApplySimplifyToPlanar(  
		UDynamicMesh* TargetMesh, 
		FGeometryScriptPlanarSimplifyOptions Options,
		UGeometryScriptDebug* Debug = nullptr);
	
	/**
	* Simplifies the mesh down to the PolyGroup Topology. For example, the high-level faces of the mesh PolyGroups. 
	* Another example would be on a default Box-Sphere where simplifying to PolyGroup topology produces a box.
	*/
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|Simplification", meta=(ScriptMethod, DisplayName = "ApplySimplifyToPolyGroupTopology"))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	ApplySimplifyToPolygroupTopology(  
		UDynamicMesh* TargetMesh, 
		FGeometryScriptPolygroupSimplifyOptions Options,
		FGeometryScriptGroupLayer GroupLayer,
		UGeometryScriptDebug* Debug = nullptr);

	/**
	* Simplifies the mesh until a target triangle count is reached. Behavior can be additionally controlled with the Options. 
	*/
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|Simplification", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	ApplySimplifyToTriangleCount(  
		UDynamicMesh* TargetMesh, 
		int32 TriangleCount,
		FGeometryScriptSimplifyMeshOptions Options,
		UGeometryScriptDebug* Debug = nullptr);

	/**
	* Simplifies the mesh until a target vertex count is reached. Behavior can be additionally controlled with the Options. 
	*/
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|Simplification", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	ApplySimplifyToVertexCount(  
		UDynamicMesh* TargetMesh, 
		int32 VertexCount,
		FGeometryScriptSimplifyMeshOptions Options,
		UGeometryScriptDebug* Debug = nullptr);

	/**
	* Simplifies the mesh to a target geometric tolerance. Stops when any further simplification would result in a deviation from the input mesh larger than the tolerance.
	* Behavior can be additionally controlled with the Options. 
	*/
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|Simplification", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	ApplySimplifyToTolerance(  
		UDynamicMesh* TargetMesh, 
		float Tolerance,
		FGeometryScriptSimplifyMeshOptions Options,
		UGeometryScriptDebug* Debug = nullptr);

};