// Copyright Epic Games, Inc. All Rights Reserved.

#include "Physics/CollisionGeometryVisualization.h"
#include "Generators/LineSegmentGenerators.h"
#include "Util/ColorConstants.h"
#include "Chaos/Levelset.h"

using namespace UE::Geometry;

void UE::PhysicsTools::InitializePreviewGeometryLines(const FPhysicsDataCollection& PhysicsData, UPreviewGeometry* PreviewGeom,
	const FColor& LineColor, float LineThickness, float DepthBias, int32 CircleStepResolution, bool bRandomColors)
{
	int32 CircleSteps = FMath::Max(4, CircleStepResolution);
	FColor SphereColor = LineColor;
	FColor BoxColor = LineColor;
	FColor ConvexColor = LineColor;
	FColor LevelSetColor = LineColor;
	FColor CapsuleColor = LineColor;
	int32 OverrideColorIdx = 0;

	auto GetNextColor = [&OverrideColorIdx, bRandomColors](FColor DefaultColor)
	{
		if (bRandomColors)
		{
			return LinearColors::SelectFColor(OverrideColorIdx++);
		}
		return DefaultColor;
	};

	const FKAggregateGeom& AggGeom = PhysicsData.AggGeom;

	// spheres are draw as 3 orthogonal circles
	for (int32 Index = 0; Index < AggGeom.SphereElems.Num(); Index++)
	{
		PreviewGeom->CreateOrUpdateLineSet(FString::Printf(TEXT("Spheres %d"), Index), 1, [&](int32 UnusedIndex, TArray<FRenderableLine>& LinesOut)
		{
			FColor Color = GetNextColor(SphereColor);

			const FKSphereElem& Sphere = AggGeom.SphereElems[Index];
			FTransform ElemTransform = Sphere.GetTransform();
			ElemTransform.ScaleTranslation(PhysicsData.ExternalScale3D);
			FTransformSRT3f ElemTransformf(ElemTransform);
			float Radius = PhysicsData.ExternalScale3D.GetAbsMin() * Sphere.Radius;
			UE::Geometry::GenerateCircleSegments<float>(CircleSteps, Radius, FVector3f::Zero(), FVector3f::UnitX(), FVector3f::UnitY(), ElemTransformf,
				[&](const FVector3f& A, const FVector3f& B) { LinesOut.Add(FRenderableLine((FVector)A, (FVector)B, Color, LineThickness, DepthBias)); });
			UE::Geometry::GenerateCircleSegments<float>(CircleSteps, Radius, FVector3f::Zero(), FVector3f::UnitX(), FVector3f::UnitZ(), ElemTransformf,
				[&](const FVector3f& A, const FVector3f& B) { LinesOut.Add(FRenderableLine((FVector)A, (FVector)B, Color, LineThickness, DepthBias)); });
			UE::Geometry::GenerateCircleSegments<float>(CircleSteps, Radius, FVector3f::Zero(), FVector3f::UnitY(), FVector3f::UnitZ(), ElemTransformf,
				[&](const FVector3f& A, const FVector3f& B) { LinesOut.Add(FRenderableLine((FVector)A, (FVector)B, Color, LineThickness, DepthBias)); });
		});
	}


	// boxes are drawn as boxes
	for (int32 Index = 0; Index < AggGeom.BoxElems.Num(); Index++)
	{
		PreviewGeom->CreateOrUpdateLineSet(FString::Printf(TEXT("Boxes %d"), Index), 1, [&](int32 UnusedIndex, TArray<FRenderableLine>& LinesOut)
		{
			FColor Color = GetNextColor(BoxColor);

			const FKBoxElem& Box = AggGeom.BoxElems[Index];
			FTransform ElemTransform = Box.GetTransform();
			ElemTransform.ScaleTranslation(PhysicsData.ExternalScale3D);
			FTransformSRT3f ElemTransformf(ElemTransform);
			FVector3f HalfDimensions(
				PhysicsData.ExternalScale3D.X * Box.X * 0.5f,
				PhysicsData.ExternalScale3D.Y * Box.Y * 0.5f,
				PhysicsData.ExternalScale3D.Z * Box.Z * 0.5f);
			UE::Geometry::GenerateBoxSegments<float>(HalfDimensions, FVector3f::Zero(), FVector3f::UnitX(), FVector3f::UnitY(), FVector3f::UnitZ(), ElemTransformf,
				[&](const FVector3f& A, const FVector3f& B) { LinesOut.Add(FRenderableLine((FVector)A, (FVector)B, Color, LineThickness, DepthBias)); });
		});
	}


	// capsules are draw as two hemispheres (with 3 intersecting arcs/circles) and connecting lines
	for (int32 Index = 0; Index < AggGeom.SphylElems.Num(); Index++)
	{
		PreviewGeom->CreateOrUpdateLineSet(FString::Printf(TEXT("Capsules %d"), Index), 1, [&](int32 UnusedIndex, TArray<FRenderableLine>& LinesOut)
		{
			FColor Color = GetNextColor(CapsuleColor);

			const FKSphylElem& Capsule = AggGeom.SphylElems[Index];
			FTransform ElemTransform = Capsule.GetTransform();
			ElemTransform.ScaleTranslation(PhysicsData.ExternalScale3D);
			FTransformSRT3f ElemTransformf(ElemTransform);
			const float HalfLength = Capsule.GetScaledCylinderLength(PhysicsData.ExternalScale3D) * .5f;
			const float Radius = Capsule.GetScaledRadius(PhysicsData.ExternalScale3D);
			FVector3f Top(0, 0, HalfLength), Bottom(0, 0, -HalfLength);

			// top and bottom circles
			UE::Geometry::GenerateCircleSegments<float>(CircleSteps, Radius, Top, FVector3f::UnitX(), FVector3f::UnitY(), ElemTransformf,
				[&](const FVector3f& A, const FVector3f& B) { LinesOut.Add(FRenderableLine((FVector)A, (FVector)B, Color, LineThickness, DepthBias)); });
			UE::Geometry::GenerateCircleSegments<float>(CircleSteps, Radius, Bottom, FVector3f::UnitX(), FVector3f::UnitY(), ElemTransformf,
				[&](const FVector3f& A, const FVector3f& B) { LinesOut.Add(FRenderableLine((FVector)A, (FVector)B, Color, LineThickness, DepthBias)); });

			// top dome
			UE::Geometry::GenerateArcSegments<float>(CircleSteps, Radius, 0.0, PI, Top, FVector3f::UnitY(), FVector3f::UnitZ(), ElemTransformf,
				[&](const FVector3f& A, const FVector3f& B) { LinesOut.Add(FRenderableLine((FVector)A, (FVector)B, Color, LineThickness, DepthBias)); });
			UE::Geometry::GenerateArcSegments<float>(CircleSteps, Radius, 0.0, PI, Top, FVector3f::UnitX(), FVector3f::UnitZ(), ElemTransformf,
				[&](const FVector3f& A, const FVector3f& B) { LinesOut.Add(FRenderableLine((FVector)A, (FVector)B, Color, LineThickness, DepthBias)); });

			// bottom dome
			UE::Geometry::GenerateArcSegments<float>(CircleSteps, Radius, 0.0, -PI, Bottom, FVector3f::UnitY(), FVector3f::UnitZ(), ElemTransformf,
				[&](const FVector3f& A, const FVector3f& B) { LinesOut.Add(FRenderableLine((FVector)A, (FVector)B, Color, LineThickness, DepthBias)); });
			UE::Geometry::GenerateArcSegments<float>(CircleSteps, Radius, 0.0, -PI, Bottom, FVector3f::UnitX(), FVector3f::UnitZ(), ElemTransformf,
				[&](const FVector3f& A, const FVector3f& B) { LinesOut.Add(FRenderableLine((FVector)A, (FVector)B, Color, LineThickness, DepthBias)); });

			// connecting lines
			for (int k = 0; k < 2; ++k)
			{
				FVector DX = (k < 1) ? FVector(-Radius, 0, 0) : FVector(Radius, 0, 0);
				LinesOut.Add(FRenderableLine(
					ElemTransform.TransformPosition((FVector)Top + DX),
					ElemTransform.TransformPosition((FVector)Bottom + DX), Color, LineThickness, DepthBias));
				FVector DY = (k < 1) ? FVector(0, -Radius, 0) : FVector(0, Radius, 0);
				LinesOut.Add(FRenderableLine(
					ElemTransform.TransformPosition((FVector)Top + DY),
					ElemTransform.TransformPosition((FVector)Bottom + DY), Color, LineThickness, DepthBias));
			}
		});
	}

	// convexes are drawn as mesh edges
	for (int32 Index = 0; Index < AggGeom.ConvexElems.Num(); Index++)
	{
		PreviewGeom->CreateOrUpdateLineSet(FString::Printf(TEXT("Convex %d"), Index), 1, [&](int32 UnusedIndex, TArray<FRenderableLine>& LinesOut)
		{
			FColor Color = GetNextColor(ConvexColor);

			const FKConvexElem& Convex = AggGeom.ConvexElems[Index];
			FTransform ElemTransform = Convex.GetTransform();
			ElemTransform.ScaleTranslation(PhysicsData.ExternalScale3D);
			ElemTransform.SetScale3D(PhysicsData.ExternalScale3D);
			int32 NumTriangles = Convex.IndexData.Num() / 3;
			for (int32 k = 0; k < NumTriangles; ++k)
			{
				FVector A = ElemTransform.TransformPosition(Convex.VertexData[Convex.IndexData[3 * k]]);
				FVector B = ElemTransform.TransformPosition(Convex.VertexData[Convex.IndexData[3 * k + 1]]);
				FVector C = ElemTransform.TransformPosition(Convex.VertexData[Convex.IndexData[3 * k + 2]]);
				LinesOut.Add(FRenderableLine(A, B, Color, LineThickness, DepthBias));
				LinesOut.Add(FRenderableLine(B, C, Color, LineThickness, DepthBias));
				LinesOut.Add(FRenderableLine(C, A, Color, LineThickness, DepthBias));
			}
		});
	}
	
	// for Level Sets draw the grid cells where phi < 0
	for (int32 Index = 0; Index < AggGeom.LevelSetElems.Num(); Index++)
	{
		PreviewGeom->CreateOrUpdateLineSet(FString::Printf(TEXT("Level Set %d"), Index), 1, [&](int32 UnusedIndex, TArray<FRenderableLine>& LinesOut)
		{
			const FColor Color = GetNextColor(LevelSetColor);
			const FKLevelSetElem& LevelSet = AggGeom.LevelSetElems[Index];
			
			FTransform ElemTransform = LevelSet.GetTransform();
			ElemTransform.ScaleTranslation(PhysicsData.ExternalScale3D);
			ElemTransform.MultiplyScale3D(PhysicsData.ExternalScale3D);

			auto GenerateBoxSegmentsFromFBox = [&](const FBox& Box)
			{
				const FVector3d Center = Box.GetCenter();
				const FVector3d HalfDimensions = 0.5 * (Box.Max - Box.Min);

				UE::Geometry::GenerateBoxSegments<double>(HalfDimensions, Center, FVector3d::UnitX(), FVector3d::UnitY(), FVector3d::UnitZ(), ElemTransform,
					[&](const FVector3d& A, const FVector3d& B) { LinesOut.Add(FRenderableLine((FVector)A, (FVector)B, Color, LineThickness, DepthBias)); });
			};

			const FBox TotalGridBox = LevelSet.UntransformedAABB();
			GenerateBoxSegmentsFromFBox(TotalGridBox);

			TArray<FBox> CellBoxes;
			const double Threshold = UE_KINDA_SMALL_NUMBER;		// allow slightly greater than zero for visualization purposes
			LevelSet.GetInteriorGridCells(CellBoxes, Threshold);

			for (const FBox& CellBox : CellBoxes)
			{
				GenerateBoxSegmentsFromFBox(CellBox);
			}

		});
	}

	// Unclear whether we actually use these in the Engine, for UBodySetup? Does not appear to be supported by UxX import system,
	// and online documentation suggests they may only be supported for cloth?
	ensure(AggGeom.TaperedCapsuleElems.Num() == 0);
}