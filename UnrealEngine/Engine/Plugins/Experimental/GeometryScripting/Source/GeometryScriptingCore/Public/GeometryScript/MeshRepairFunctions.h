// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GeometryScript/GeometryScriptTypes.h"
#include "MeshRepairFunctions.generated.h"

class UDynamicMesh;

USTRUCT(BlueprintType)
struct GEOMETRYSCRIPTINGCORE_API FGeometryScriptWeldEdgesOptions
{
	GENERATED_BODY()
public:
	/** Edges are coincident if both pairs of endpoint vertices, and their midpoint, are closer than this distance */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	float Tolerance = 1e-06f;

	/** Only merge unambiguous pairs that have unique duplicate-edge matches */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	bool bOnlyUniquePairs = true;
};



USTRUCT(BlueprintType)
struct GEOMETRYSCRIPTINGCORE_API FGeometryScriptResolveTJunctionOptions
{
	GENERATED_BODY()
public:
	/**  */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	float Tolerance = 1e-03f;
};


UENUM(BlueprintType)
enum class EGeometryScriptFillHolesMethod : uint8
{
	Automatic = 0,
	MinimalFill = 1,
	PolygonTriangulation = 2,
	TriangleFan = 3,
	PlanarProjection = 4
};

USTRUCT(BlueprintType)
struct GEOMETRYSCRIPTINGCORE_API FGeometryScriptFillHolesOptions
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	EGeometryScriptFillHolesMethod FillMethod = EGeometryScriptFillHolesMethod::Automatic;

	/** Delete floating, disconnected triangles, as they produce a "hole" that cannot be filled */
	bool bDeleteIsolatedTriangles = true;
};



USTRUCT(BlueprintType)
struct GEOMETRYSCRIPTINGCORE_API FGeometryScriptRemoveSmallComponentOptions
{
	GENERATED_BODY()
public:
	/**  */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	float MinVolume = 0.0001;

	/**  */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	float MinArea = 0.0001;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	int MinTriangleCount = 1;
};




UENUM(BlueprintType)
enum class EGeometryScriptRemoveHiddenTrianglesMethod : uint8
{
	FastWindingNumber = 0,
	RaycastOcclusionTest = 1
};


USTRUCT(BlueprintType)
struct GEOMETRYSCRIPTINGCORE_API FGeometryScriptRemoveHiddenTrianglesOptions
{
	GENERATED_BODY()
public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	EGeometryScriptRemoveHiddenTrianglesMethod Method = EGeometryScriptRemoveHiddenTrianglesMethod::FastWindingNumber;

	// add triangle samples per triangle (in addition to TriangleSamplingMethod)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	int SamplesPerTriangle = 0;

	// once triangles to remove are identified, do iterations of boundary erosion, ie contract selection by boundary vertex one-rings
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	int ShrinkSelection = 0;

	// use this as winding isovalue for WindingNumber mode
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	float WindingIsoValue = 0.5;

	// random rays to add beyond +/- major axes, for raycast sampling
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	int RaysPerSample = 0;


	/** Nudge sample points out by this amount to try to counteract numerical issues */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	float NormalOffset = 1e-6f;

	/**  */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	bool bCompactResult = true;
};



UENUM(BlueprintType)
enum class EGeometryScriptRepairMeshMode : uint8
{
	DeleteOnly = 0,
	RepairOrDelete = 1,
	RepairOrSkip = 2
};



USTRUCT(BlueprintType)
struct GEOMETRYSCRIPTINGCORE_API FGeometryScriptDegenerateTriangleOptions
{
	GENERATED_BODY()
public:
	/**  */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	EGeometryScriptRepairMeshMode Mode = EGeometryScriptRepairMeshMode::RepairOrDelete;

	/**  */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	double MinTriangleArea = 0.001;

	/**  */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	double MinEdgeLength = 0.0001;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	bool bCompactOnCompletion = true;
};



UCLASS(meta = (ScriptName = "GeometryScript_MeshRepair"))
class GEOMETRYSCRIPTINGCORE_API UGeometryScriptLibrary_MeshRepairFunctions : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:

	UFUNCTION(BlueprintCallable, Category = "GeometryScript|Repair", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	CompactMesh(  
		UDynamicMesh* TargetMesh, 
		UGeometryScriptDebug* Debug = nullptr);

	UFUNCTION(BlueprintCallable, Category = "GeometryScript|Repair", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	ResolveMeshTJunctions(  
		UDynamicMesh* TargetMesh, 
		FGeometryScriptResolveTJunctionOptions ResolveOptions,
		UGeometryScriptDebug* Debug = nullptr);

	UFUNCTION(BlueprintCallable, Category = "GeometryScript|Repair", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	WeldMeshEdges(  
		UDynamicMesh* TargetMesh, 
		FGeometryScriptWeldEdgesOptions WeldOptions,
		UGeometryScriptDebug* Debug = nullptr);

	UFUNCTION(BlueprintCallable, Category = "GeometryScript|Repair", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	FillAllMeshHoles(  
		UDynamicMesh* TargetMesh, 
		FGeometryScriptFillHolesOptions FillOptions,
		int32& NumFilledHoles,
		int32& NumFailedHoleFills,
		UGeometryScriptDebug* Debug = nullptr);

	UFUNCTION(BlueprintCallable, Category = "GeometryScript|Repair", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	RemoveSmallComponents(  
		UDynamicMesh* TargetMesh, 
		FGeometryScriptRemoveSmallComponentOptions Options,
		UGeometryScriptDebug* Debug = nullptr);

	UFUNCTION(BlueprintCallable, Category = "GeometryScript|Repair", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	RemoveHiddenTriangles(  
		UDynamicMesh* TargetMesh, 
		FGeometryScriptRemoveHiddenTrianglesOptions Options,
		UGeometryScriptDebug* Debug = nullptr);

	UFUNCTION(BlueprintCallable, Category = "GeometryScript|Repair", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	SplitMeshBowties(  
		UDynamicMesh* TargetMesh, 
		bool bMeshBowties = true,
		bool bAttributeBowties = true,
		UGeometryScriptDebug* Debug = nullptr);

	UFUNCTION(BlueprintCallable, Category = "GeometryScript|Repair", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	RepairMeshDegenerateGeometry(  
		UDynamicMesh* TargetMesh, 
		FGeometryScriptDegenerateTriangleOptions Options,
		UGeometryScriptDebug* Debug = nullptr);
};