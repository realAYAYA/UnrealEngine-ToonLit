// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GeometryScript/GeometryScriptTypes.h"
#include "GeometryScript/GeometryScriptSelectionTypes.h"
#include "MeshDeformFunctions.generated.h"

class UDynamicMesh;


USTRUCT(BlueprintType)
struct GEOMETRYSCRIPTINGCORE_API FGeometryScriptBendWarpOptions
{
	GENERATED_BODY()
public:
	/** Symmetric extents are [-BendExtent,BendExtent], if disabled, then [-LowerExtent,BendExtent] is used  */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	bool bSymmetricExtents = true;

	/** Lower extent used when bSymmetricExtents = false */
	UPROPERTY(BlueprintReadWrite, Category = Options, meta = (EditCondition = "bSymmetricExtents == false"))
	float LowerExtent = 10;

	/** If true, the Bend is "centered" at the Origin, ie the regions on either side of the extents are rigidly transformed. If false, the Bend begins at the start of the Lower Extents, and the "lower" region is not affected. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	bool bBidirectional = true;
};



USTRUCT(BlueprintType)
struct GEOMETRYSCRIPTINGCORE_API FGeometryScriptTwistWarpOptions
{
	GENERATED_BODY()
public:
	/** Symmetric extents are [-BendExtent,BendExtent], if disabled, then [-LowerExtent,BendExtent] is used  */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	bool bSymmetricExtents = true;

	/** Lower extent used when bSymmetricExtents = false */
	UPROPERTY(BlueprintReadWrite, Category = Options, meta = (EditCondition = "bSymmetricExtents == false"))
	float LowerExtent = 10;

	/** If true, the Twist is "centered" at the Origin, ie the regions on either side of the extents are rigidly transformed. If false, the Twist begins at the start of the Lower Extents, and the "lower" region is not affected. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	bool bBidirectional = true;
};

UENUM(BlueprintType)
enum class EGeometryScriptFlareType : uint8
{
	//Displaced by sin(pi x) with x in 0 to 1
	SinMode = 0,

	//Displaced by sin(pi x)*sin(pi x) with x in 0 to 1. This provides a smooth normal transition.
	SinSquaredMode = 1,

	// Displaced by piecewise-linear trianglular mode
	TriangleMode = 2
};

USTRUCT(BlueprintType)
struct GEOMETRYSCRIPTINGCORE_API FGeometryScriptFlareWarpOptions
{
	GENERATED_BODY()
public:
	/** Symmetric extents are [-BendExtent,BendExtent], if disabled, then [-LowerExtent,BendExtent] is used  */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	bool bSymmetricExtents = true;

	/** Lower extent used when bSymmetricExtents = false */
	UPROPERTY(BlueprintReadWrite, Category = Options, meta = (EditCondition = "bSymmetricExtents == false"))
	float LowerExtent = 10;

	/** Determines the profile used as a displacement */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	EGeometryScriptFlareType FlareType = EGeometryScriptFlareType::SinMode;
};



USTRUCT(BlueprintType)
struct GEOMETRYSCRIPTINGCORE_API FGeometryScriptPerlinNoiseLayerOptions
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	float Magnitude = 5.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	float Frequency = 0.25;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	FVector FrequencyShift = FVector::Zero();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	int RandomSeed = 0;
};



UENUM(BlueprintType)
enum class EGeometryScriptMathWarpType : uint8
{
	SinWave1D = 0,
	SinWave2D = 1,
	SinWave3D = 2
};


USTRUCT(BlueprintType)
struct GEOMETRYSCRIPTINGCORE_API FGeometryScriptMathWarpOptions
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	float Magnitude = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	float Frequency = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	float FrequencyShift = 0.0;
};




USTRUCT(BlueprintType)
struct GEOMETRYSCRIPTINGCORE_API FGeometryScriptPerlinNoiseOptions
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	FGeometryScriptPerlinNoiseLayerOptions BaseLayer;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	bool bApplyAlongNormal = true;

	/** EmptyBehavior Defines how an empty input selection should be interpreted */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	EGeometryScriptEmptySelectionBehavior EmptyBehavior = EGeometryScriptEmptySelectionBehavior::FullMeshSelection;
};


USTRUCT(BlueprintType)
struct GEOMETRYSCRIPTINGCORE_API FGeometryScriptIterativeMeshSmoothingOptions
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	int NumIterations = 10;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	float Alpha = 0.2;

	/** EmptyBehavior Defines how an empty input selection should be interpreted */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	EGeometryScriptEmptySelectionBehavior EmptyBehavior = EGeometryScriptEmptySelectionBehavior::FullMeshSelection;
};




USTRUCT(BlueprintType)
struct GEOMETRYSCRIPTINGCORE_API FGeometryScriptDisplaceFromTextureOptions
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	float Magnitude = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	FVector2D UVScale = FVector2D(1,1);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	FVector2D UVOffset = FVector2D(0,0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	float Center = 0.5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	int ImageChannel = 0;

	/** EmptyBehavior Defines how an empty input selection should be interpreted */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	EGeometryScriptEmptySelectionBehavior EmptyBehavior = EGeometryScriptEmptySelectionBehavior::FullMeshSelection;
};





UCLASS(meta = (ScriptName = "GeometryScript_MeshDeformers"))
class GEOMETRYSCRIPTINGCORE_API UGeometryScriptLibrary_MeshDeformFunctions : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:

	UFUNCTION(BlueprintCallable, Category = "GeometryScript|Deformations", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	ApplyBendWarpToMesh(  
		UDynamicMesh* TargetMesh, 
		FGeometryScriptBendWarpOptions Options,
		FTransform BendOrientation,
		float BendAngle = 45,
		float BendExtent = 50,
		UGeometryScriptDebug* Debug = nullptr);

	UFUNCTION(BlueprintCallable, Category = "GeometryScript|Deformations", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	ApplyTwistWarpToMesh(  
		UDynamicMesh* TargetMesh, 
		FGeometryScriptTwistWarpOptions Options,
		FTransform TwistOrientation,
		float TwistAngle = 45,
		float TwistExtent = 50,
		UGeometryScriptDebug* Debug = nullptr);

	UFUNCTION(BlueprintCallable, Category = "GeometryScript|Deformations", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	ApplyFlareWarpToMesh(  
		UDynamicMesh* TargetMesh, 
		FGeometryScriptFlareWarpOptions Options,
		FTransform FlareOrientation,
		float FlarePercentX = 0,
		float FlarePercentY = 0,
		float FlareExtent = 50,
		UGeometryScriptDebug* Debug = nullptr);


	UFUNCTION(BlueprintCallable, Category = "GeometryScript|Deformations", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	ApplyMathWarpToMesh(  
		UDynamicMesh* TargetMesh, 
		FTransform WarpOrientation,
		EGeometryScriptMathWarpType WarpType,
		FGeometryScriptMathWarpOptions Options,
		UGeometryScriptDebug* Debug = nullptr);


	UFUNCTION(BlueprintCallable, Category = "GeometryScript|Deformations", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	ApplyPerlinNoiseToMesh(  
		UDynamicMesh* TargetMesh, 
		FGeometryScriptMeshSelection Selection,
		FGeometryScriptPerlinNoiseOptions Options,
		UGeometryScriptDebug* Debug = nullptr);

	UFUNCTION(BlueprintCallable, Category = "GeometryScript|Deformations", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	ApplyIterativeSmoothingToMesh(  
		UDynamicMesh* TargetMesh, 
		FGeometryScriptMeshSelection Selection,
		FGeometryScriptIterativeMeshSmoothingOptions Options,
		UGeometryScriptDebug* Debug = nullptr);


	UFUNCTION(BlueprintCallable, Category = "GeometryScript|Deformations", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	ApplyDisplaceFromTextureMap(  
		UDynamicMesh* TargetMesh, 
		UTexture2D* Texture,
		FGeometryScriptMeshSelection Selection,
		FGeometryScriptDisplaceFromTextureOptions Options,
		int32 UVLayer = 0,
		UGeometryScriptDebug* Debug = nullptr);

	/**
	 * Add the vectors in VectorList, scaled by Magnitude, to the vertex positions in TargetMesh.
	 * VectorList Length must be >= the as TargetMesh MaxVertexID.
	 * @param Selection if non-empty, only the vertices identified by the selection will be displaced. The VectorList must still be the same size as the whole mesh, this is just a filter on which vertices are updated.
	 */
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|Deformations", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	ApplyDisplaceFromPerVertexVectors(  
		UDynamicMesh* TargetMesh,
		FGeometryScriptMeshSelection Selection,
		const FGeometryScriptVectorList& VectorList, 
		float Magnitude = 5.0,
		UGeometryScriptDebug* Debug = nullptr);

};