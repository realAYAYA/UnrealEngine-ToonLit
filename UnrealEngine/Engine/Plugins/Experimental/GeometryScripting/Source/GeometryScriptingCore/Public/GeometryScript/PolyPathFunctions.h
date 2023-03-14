// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GeometryScript/GeometryScriptTypes.h"
#include "Components/SplineComponent.h"
#include "PolyPathFunctions.generated.h"


UENUM(BlueprintType)
enum class EGeometryScriptSampleSpacing : uint8
{
	UniformDistance,
	UniformTime,
	ErrorTolerance
};


USTRUCT(BlueprintType)
struct GEOMETRYSCRIPTINGCORE_API FGeometryScriptSplineSamplingOptions
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintReadWrite, Category = Options, meta = (EditCondition = "SampleSpacing != EGeometryScriptSampleSpacing::ErrorTolerance"))
	int32 NumSamples = 10;

	UPROPERTY(BlueprintReadWrite, Category = Options, meta = (EditCondition = "SampleSpacing == EGeometryScriptSampleSpacing::ErrorTolerance"))
	float ErrorTolerance = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	EGeometryScriptSampleSpacing SampleSpacing = EGeometryScriptSampleSpacing::UniformDistance;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	TEnumAsByte<ESplineCoordinateSpace::Type> CoordinateSpace = ESplineCoordinateSpace::Type::Local;
};


UCLASS(meta = (ScriptName = "GeometryScript_PolyPath"))
class GEOMETRYSCRIPTINGCORE_API UGeometryScriptLibrary_PolyPathFunctions : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:

	UFUNCTION(BlueprintPure, Category = "GeometryScript|PolyPath", meta=(ScriptMethod))
	static int GetPolyPathNumVertices(FGeometryScriptPolyPath PolyPath);

	UFUNCTION(BlueprintPure, Category = "GeometryScript|PolyPath", meta=(ScriptMethod))
	static int GetPolyPathLastIndex(FGeometryScriptPolyPath PolyPath);

	UFUNCTION(BlueprintPure, Category = "GeometryScript|PolyPath", meta=(ScriptMethod))
	static FVector GetPolyPathVertex(FGeometryScriptPolyPath PolyPath, int Index, bool& bIsValidIndex);

	UFUNCTION(BlueprintPure, Category = "GeometryScript|PolyPath", meta = (ScriptMethod))
	static FVector GetPolyPathTangent(FGeometryScriptPolyPath PolyPath, int Index, bool& bIsValidIndex);

	UFUNCTION(BlueprintCallable, Category = "GeometryScript|PolyPath", meta = (ScriptMethod))
	static double GetPolyPathArcLength(FGeometryScriptPolyPath PolyPath);

	/** Find the index of the vertex closest to a given point.  Returns -1 if path has no vertices. */
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|PolyPath", meta = (ScriptMethod))
	static int32 GetNearestVertexIndex(FGeometryScriptPolyPath PolyPath, FVector Point);

	/** Create a 2D, flattened copy of the path by dropping the given axis, and using the other two coordinates as the new X, Y coordinates. */
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|PolyPath", meta = (ScriptMethod, DisplayName = "Flatten To 2D On Axis"))
	static UPARAM(DisplayName = "Poly Path") FGeometryScriptPolyPath FlattenTo2DOnAxis(FGeometryScriptPolyPath PolyPath, EGeometryScriptAxis DropAxis = EGeometryScriptAxis::Z);

	/**
	 * Create a closed circle around the origin on the XY plane, then transformed by Transform.
	 * By our convention for closed paths, the end vertex is *not* a duplicate of the start vertex.
	 */
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|PolyPath")
	static UPARAM(DisplayName = "Poly Path") FGeometryScriptPolyPath CreateCirclePath3D(FTransform Transform, float Radius = 10, int NumPoints = 10);

	/**
	 * Create an open arc around the origin on the XY plane, then transformed by Transform.
	 * As it is an open path, the end vertex exactly hits the target EndAngle (so will be positioned on the start vertex if the end aligns to the start)
	 */
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|PolyPath")
	static UPARAM(DisplayName = "Poly Path") FGeometryScriptPolyPath CreateArcPath3D(FTransform Transform, float Radius = 10, int NumPoints = 10, float StartAngle = 0, float EndAngle = 90);
	
	/**
	 * Create a closed circle on the XY plane around the given Center.
	 * By our convention for closed paths, the end vertex is *not* a duplicate of the start vertex.
	 */
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|PolyPath")
	static UPARAM(DisplayName = "Poly Path") FGeometryScriptPolyPath CreateCirclePath2D(FVector2D Center = FVector2D(0, 0), float Radius = 10, int NumPoints = 10);

	/**
	 * Create an open arc on the XY plane around the given Center.
	 * As it is an open path, the end vertex exactly hits the target EndAngle (so will be positioned on the start vertex if the end aligns to the start)
	 */
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|PolyPath")
	static UPARAM(DisplayName = "Poly Path") FGeometryScriptPolyPath CreateArcPath2D(FVector2D Center = FVector2D(0, 0), float Radius = 10, int NumPoints = 10, float StartAngle = 0, float EndAngle = 90);

	/**
	 * Sample positions from a USplineComponent into a PolyPath, based on the given SamplingOptions
	 */
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|PolyPath")
	static void ConvertSplineToPolyPath(const USplineComponent* Spline, FGeometryScriptPolyPath& PolyPath, FGeometryScriptSplineSamplingOptions SamplingOptions);

	/**
	 * Sample a USplineComponent into a list of FTransforms, based on the given SamplingOptions.
	 * @param Frames Transforms are returned here, with X axis oriented along spline Tangent and Z as the 'Up' vector.
	 * @param FrameTimes the spline Time value used for each Frame
	 * @param RelativeTransform a constant Transform applied to each sample Transform in its local frame of reference. So, eg, an X Rotation will rotate each frame around the local spline Tangent vector
	 * @param bIncludeScale if true, the Scale of each FTransform is taken from the Spline, otherwise the Transforms have unit scale
	 */
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|PolyPath")
	static void SampleSplineToTransforms(
		const USplineComponent* Spline, 
		TArray<FTransform>& Frames, 
		TArray<double>& FrameTimes,
		FGeometryScriptSplineSamplingOptions SamplingOptions,
		FTransform RelativeTransform,
		bool bIncludeScale = true);


	UFUNCTION(BlueprintCallable, Category = "GeometryScript|PolyPath", meta=(ScriptMethod))
	static void ConvertPolyPathToArray(FGeometryScriptPolyPath PolyPath, TArray<FVector>& VertexArray);

	UFUNCTION(BlueprintCallable, Category = "GeometryScript|PolyPath")
	static void ConvertArrayToPolyPath(const TArray<FVector>& VertexArray, FGeometryScriptPolyPath& PolyPath);

	UFUNCTION(BlueprintCallable, Category = "GeometryScript|PolyPath", meta = (ScriptMethod))
	static void ConvertPolyPathToArrayOfVector2D(FGeometryScriptPolyPath PolyPath, TArray<FVector2D>& VertexArray);

	UFUNCTION(BlueprintCallable, Category = "GeometryScript|PolyPath")
	static void ConvertArrayOfVector2DToPolyPath(const TArray<FVector2D>& VertexArray, FGeometryScriptPolyPath& PolyPath);

	UFUNCTION(BlueprintPure, meta = (DisplayName = "PolyPath To Array Of Vector", CompactNodeTitle = "->", BlueprintAutocast), Category = "GeometryScript|PolyPath")
	static TArray<FVector> Conv_GeometryScriptPolyPathToArray(FGeometryScriptPolyPath PolyPath);

	UFUNCTION(BlueprintPure, meta = (DisplayName = "PolyPath To Array Of Vector2D", CompactNodeTitle = "->", BlueprintAutocast), Category = "GeometryScript|PolyPath")
	static TArray<FVector2D> Conv_GeometryScriptPolyPathToArrayOfVector2D(FGeometryScriptPolyPath PolyPath);

	UFUNCTION(BlueprintPure, meta = (DisplayName = "Array Of Vector To PolyPath", CompactNodeTitle = "->", BlueprintAutocast), Category = "GeometryScript|PolyPath")
	static FGeometryScriptPolyPath Conv_ArrayToGeometryScriptPolyPath(const TArray<FVector>& PathVertices);

	UFUNCTION(BlueprintPure, meta = (DisplayName = "Array Of Vector2D To PolyPath", CompactNodeTitle = "->", BlueprintAutocast), Category = "GeometryScript|PolyPath")
	static FGeometryScriptPolyPath Conv_ArrayOfVector2DToGeometryScriptPolyPath(const TArray<FVector2D>& PathVertices);
};
