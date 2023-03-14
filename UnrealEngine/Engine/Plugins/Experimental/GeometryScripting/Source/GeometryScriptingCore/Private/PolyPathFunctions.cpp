// Copyright Epic Games, Inc. All Rights Reserved.

#include "GeometryScript/PolyPathFunctions.h"

#include "Curve/CurveUtil.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PolyPathFunctions)

#define LOCTEXT_NAMESPACE "UGeometryScriptLibrary_PolyPathFunctions"


int UGeometryScriptLibrary_PolyPathFunctions::GetPolyPathNumVertices(FGeometryScriptPolyPath PolyPath)
{
	return (PolyPath.Path.IsValid()) ? PolyPath.Path->Num() : 0;
}

int UGeometryScriptLibrary_PolyPathFunctions::GetPolyPathLastIndex(FGeometryScriptPolyPath PolyPath)
{
	return (PolyPath.Path.IsValid()) ? FMath::Max(PolyPath.Path->Num()-1,0) : 0;
}

FVector UGeometryScriptLibrary_PolyPathFunctions::GetPolyPathVertex(FGeometryScriptPolyPath PolyPath, int Index, bool& bIsValidIndex)
{
	bIsValidIndex = false;
	if (PolyPath.Path.IsValid() && Index >= 0 && Index < PolyPath.Path->Num())
	{
		bIsValidIndex = true;
		return (*PolyPath.Path)[Index];
	}
	return FVector::ZeroVector;
}

FVector UGeometryScriptLibrary_PolyPathFunctions::GetPolyPathTangent(FGeometryScriptPolyPath PolyPath, int Index, bool& bIsValidIndex)
{
	bIsValidIndex = false;
	if (PolyPath.Path.IsValid() && Index >= 0 && Index < PolyPath.Path->Num())
	{
		bIsValidIndex = true;
		FVector Tangent = UE::Geometry::CurveUtil::Tangent<double, FVector>(*PolyPath.Path, Index, PolyPath.bClosedLoop);
		return Tangent;
	}
	return FVector::ZeroVector;
}

double UGeometryScriptLibrary_PolyPathFunctions::GetPolyPathArcLength(FGeometryScriptPolyPath PolyPath)
{
	if (PolyPath.Path.IsValid())
	{
		return UE::Geometry::CurveUtil::ArcLength<double, FVector>(*PolyPath.Path, PolyPath.bClosedLoop);
	}
	return 0;
}

int32 UGeometryScriptLibrary_PolyPathFunctions::GetNearestVertexIndex(FGeometryScriptPolyPath PolyPath, FVector Point)
{
	if (PolyPath.Path.IsValid())
	{
		return UE::Geometry::CurveUtil::FindNearestIndex<double, FVector>(*PolyPath.Path, Point);
	}
	return -1;
}

FGeometryScriptPolyPath UGeometryScriptLibrary_PolyPathFunctions::FlattenTo2DOnAxis(FGeometryScriptPolyPath PolyPath, EGeometryScriptAxis DropAxis)
{
	FGeometryScriptPolyPath ToReturn;
	ToReturn.Reset();
	if (PolyPath.Path.IsValid())
	{
		ToReturn.bClosedLoop = PolyPath.bClosedLoop;
		ToReturn.Path = PolyPath.Path;

		int32 Keep0 = int32(DropAxis == EGeometryScriptAxis::X);
		int32 Keep1 = 1 + int32(DropAxis != EGeometryScriptAxis::Z);
		for (FVector& V : *ToReturn.Path)
		{
			V[0] = V[Keep0];
			V[1] = V[Keep1];
			V.Z = 0;
		}
	}
	return ToReturn;
}

namespace PolyPathInternal
{
	
TArray<FVector> CreateArcPoints(FTransform Transform, float Radius, int NumPoints, float StartAngle, float EndAngle, bool bIncludeEndPoint)
{
	TArray<FVector> Points;
	NumPoints = FMath::Max(2, NumPoints);
	Points.Reserve(NumPoints);

	double AngleStartRad = FMathd::DegToRad * double(StartAngle);
	double AngleRangeRad = FMathd::DegToRad * double(EndAngle) - AngleStartRad;
	double ToAngle = AngleRangeRad / double(NumPoints - (int)bIncludeEndPoint); // if bIncludeEndPoint, adjust fraction to reach end at PtIdx = NumPoints-1
	for (int PtIdx = 0; PtIdx < NumPoints; PtIdx++)
	{
		double Angle = AngleStartRad + PtIdx * ToAngle;
		Points.Emplace(Transform.TransformPosition(FVector3d(FMathd::Cos(Angle) * (double)Radius, FMathd::Sin(Angle) * (double)Radius, 0.0)));
	}

	return Points;
}

}

FGeometryScriptPolyPath UGeometryScriptLibrary_PolyPathFunctions::CreateCirclePath3D(FTransform Transform, float Radius, int NumPoints)
{
	FGeometryScriptPolyPath PolyPath;
	PolyPath.Reset();
	PolyPath.bClosedLoop = true;

	*PolyPath.Path = PolyPathInternal::CreateArcPoints(Transform, Radius, NumPoints, 0, 360, false);

	return PolyPath;
}

FGeometryScriptPolyPath UGeometryScriptLibrary_PolyPathFunctions::CreateArcPath3D(FTransform Transform, float Radius, int NumPoints, float StartAngle, float EndAngle)
{
	FGeometryScriptPolyPath PolyPath;
	PolyPath.Reset();

	PolyPath.bClosedLoop = false;

	*PolyPath.Path = PolyPathInternal::CreateArcPoints(Transform, Radius, NumPoints, StartAngle, EndAngle, true);

	return PolyPath;
}

FGeometryScriptPolyPath UGeometryScriptLibrary_PolyPathFunctions::CreateCirclePath2D(FVector2D Center, float Radius, int NumPoints)
{
	FGeometryScriptPolyPath PolyPath;
	PolyPath.Reset();

	PolyPath.bClosedLoop = true;

	FTransform TransformCenter(FVector(Center.X, Center.Y, 0));
	*PolyPath.Path = PolyPathInternal::CreateArcPoints(TransformCenter, Radius, NumPoints, 0, 360, false);

	return PolyPath;
}

FGeometryScriptPolyPath UGeometryScriptLibrary_PolyPathFunctions::CreateArcPath2D(FVector2D Center, float Radius, int NumPoints, float StartAngle, float EndAngle)
{
	FGeometryScriptPolyPath PolyPath;
	PolyPath.Reset();

	PolyPath.bClosedLoop = false;

	FTransform TransformCenter(FVector(Center.X, Center.Y, 0));
	*PolyPath.Path = PolyPathInternal::CreateArcPoints(TransformCenter, Radius, NumPoints, StartAngle, EndAngle, true);

	return PolyPath;
}

void UGeometryScriptLibrary_PolyPathFunctions::ConvertPolyPathToArray(FGeometryScriptPolyPath PolyPath, TArray<FVector>& PathVertices)
{
	PathVertices.Reset();
	if (PolyPath.Path.IsValid())
	{
		PathVertices.Append(*PolyPath.Path);
	}
}

void UGeometryScriptLibrary_PolyPathFunctions::ConvertArrayToPolyPath(const TArray<FVector>& PathVertices, FGeometryScriptPolyPath& PolyPath)
{
	PolyPath.Reset();
	PolyPath.Path->Append(PathVertices);
}

void UGeometryScriptLibrary_PolyPathFunctions::ConvertPolyPathToArrayOfVector2D(FGeometryScriptPolyPath PolyPath, TArray<FVector2D>& PathVertices)
{
	PathVertices.Reset();
	if (PolyPath.Path.IsValid())
	{
		PathVertices.Reserve(PolyPath.Path->Num());
		for (const FVector& V : *PolyPath.Path)
		{
			PathVertices.Emplace(V.X, V.Y);
		}
	}
}

void UGeometryScriptLibrary_PolyPathFunctions::ConvertArrayOfVector2DToPolyPath(const TArray<FVector2D>& PathVertices, FGeometryScriptPolyPath& PolyPath)
{
	PolyPath.Reset();
	PolyPath.Path->Reserve(PathVertices.Num());
	for (const FVector2D& V : PathVertices)
	{
		PolyPath.Path->Emplace(V.X, V.Y, 0);
	}
}

void UGeometryScriptLibrary_PolyPathFunctions::ConvertSplineToPolyPath(const USplineComponent* Spline, FGeometryScriptPolyPath& PolyPath, FGeometryScriptSplineSamplingOptions SamplingOptions)
{
	PolyPath.Reset();
	if (Spline)
	{
		bool bIsLoop = Spline->IsClosedLoop();
		PolyPath.bClosedLoop = bIsLoop;
		if (SamplingOptions.SampleSpacing == EGeometryScriptSampleSpacing::ErrorTolerance)
		{
			float SquaredErrorTolerance = FMath::Max(KINDA_SMALL_NUMBER, SamplingOptions.ErrorTolerance * SamplingOptions.ErrorTolerance);
			Spline->ConvertSplineToPolyLine(SamplingOptions.CoordinateSpace, SquaredErrorTolerance, *PolyPath.Path);
			if (bIsLoop)
			{
				PolyPath.Path->Pop(); // delete the duplicate end-point for loops
			}
		}
		else
		{
			float Duration = Spline->Duration;
			
			bool bUseConstantVelocity = SamplingOptions.SampleSpacing == EGeometryScriptSampleSpacing::UniformDistance;
			int32 UseSamples = FMath::Max(2, SamplingOptions.NumSamples); // Always use at least 2 samples
			// In non-loops, we adjust DivNum so we exactly sample the end of the spline
			// In loops we don't sample the endpoint, by convention, as it's the same as the start
			float DivNum = float(UseSamples - (int32)!bIsLoop);
			PolyPath.Path->Reserve(UseSamples);
			for (int32 Idx = 0; Idx < UseSamples; Idx++)
			{
				float Time = Duration * ((float)Idx / DivNum);
				PolyPath.Path->Add(Spline->GetLocationAtTime(Time, SamplingOptions.CoordinateSpace, bUseConstantVelocity));
			}
		}
	}
}


void UGeometryScriptLibrary_PolyPathFunctions::SampleSplineToTransforms(
	const USplineComponent* Spline, 
	TArray<FTransform>& Frames, 
	TArray<double>& FrameTimes,
	FGeometryScriptSplineSamplingOptions SamplingOptions,
	FTransform RelativeTransform,
	bool bIncludeScale)
{
	Frames.Reset();
	FrameTimes.Reset();

	// Currently ErrorTolerance sampling can only be done via Spline->ConvertSplineToPolyLine, which only returns a list of points.
	// To convert to Transforms we would have to reverse-engineer the Time at each Point which could be very expensive...
	if (SamplingOptions.SampleSpacing == EGeometryScriptSampleSpacing::ErrorTolerance)
	{
		UE_LOG(LogGeometry, Warning, TEXT("SampleSplineToTransforms: ErrorTolerance sampling mode is currently not supported, falling back to UniformDistance"));
		SamplingOptions.SampleSpacing = EGeometryScriptSampleSpacing::UniformDistance;
	}

	if (Spline != nullptr )
	{
		bool bIsLoop = Spline->IsClosedLoop();

		float Duration = Spline->Duration;

		bool bUseConstantVelocity = SamplingOptions.SampleSpacing == EGeometryScriptSampleSpacing::UniformDistance;
		int32 UseSamples = FMath::Max(2, SamplingOptions.NumSamples); // Always use at least 2 samples
																		// In non-loops, we adjust DivNum so we exactly sample the end of the spline
																		// In loops we don't sample the endpoint, by convention, as it's the same as the start
		float DivNum = float(UseSamples - (int32)!bIsLoop);
		Frames.Reserve(UseSamples);
		FrameTimes.Reserve(UseSamples);
		for (int32 Idx = 0; Idx < UseSamples; Idx++)
		{
			float Time = Duration * ((float)Idx / DivNum);
			FTransform Transform = Spline->GetTransformAtTime(Time, SamplingOptions.CoordinateSpace, bUseConstantVelocity, bIncludeScale);
			FTransform::Multiply(&Transform, &RelativeTransform, &Transform);
			Frames.Add(Transform);
			FrameTimes.Add(Time);
		}
	}
}



TArray<FVector> UGeometryScriptLibrary_PolyPathFunctions::Conv_GeometryScriptPolyPathToArray(FGeometryScriptPolyPath PolyPath)
{
	TArray<FVector> PathVertices;
	ConvertPolyPathToArray(PolyPath, PathVertices);
	return PathVertices;
}

TArray<FVector2D> UGeometryScriptLibrary_PolyPathFunctions::Conv_GeometryScriptPolyPathToArrayOfVector2D(FGeometryScriptPolyPath PolyPath)
{
	TArray<FVector2D> PathVertices;
	ConvertPolyPathToArrayOfVector2D(PolyPath, PathVertices);
	return PathVertices;
}

FGeometryScriptPolyPath UGeometryScriptLibrary_PolyPathFunctions::Conv_ArrayToGeometryScriptPolyPath(const TArray<FVector>& PathVertices)
{
	FGeometryScriptPolyPath PolyPath;
	ConvertArrayToPolyPath(PathVertices, PolyPath);
	return PolyPath;
}

FGeometryScriptPolyPath UGeometryScriptLibrary_PolyPathFunctions::Conv_ArrayOfVector2DToGeometryScriptPolyPath(const TArray<FVector2D>& PathVertices)
{
	FGeometryScriptPolyPath PolyPath;
	ConvertArrayOfVector2DToPolyPath(PathVertices, PolyPath);
	return PolyPath;
}


#undef LOCTEXT_NAMESPACE
