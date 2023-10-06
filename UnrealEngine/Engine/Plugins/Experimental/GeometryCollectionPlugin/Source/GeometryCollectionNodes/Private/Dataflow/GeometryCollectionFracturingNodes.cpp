// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/GeometryCollectionFracturingNodes.h"
#include "Dataflow/DataflowCore.h"

#include "Engine/StaticMesh.h"
#include "GeometryCollection/GeometryCollectionObject.h"
#include "GeometryCollection/ManagedArrayCollection.h"
#include "GeometryCollection/GeometryCollection.h"
#include "GeometryCollection/GeometryCollectionEngineUtility.h"
#include "GeometryCollection/GeometryCollectionEngineConversion.h"
#include "Logging/LogMacros.h"
#include "Templates/SharedPointer.h"
#include "UObject/UnrealTypePrivate.h"
#include "DynamicMeshToMeshDescription.h"
#include "MeshDescriptionToDynamicMesh.h"
#include "StaticMeshAttributes.h"
#include "DynamicMeshEditor.h"
#include "Operations/MeshBoolean.h"

#include "EngineGlobals.h"
#include "GeometryCollection/GeometryCollectionAlgo.h"
#include "GeometryCollection/GeometryCollectionClusteringUtility.h"
#include "GeometryCollection/GeometryCollectionConvexUtility.h"
#include "Voronoi/Voronoi.h"
#include "PlanarCut.h"
#include "GeometryCollection/GeometryCollectionProximityUtility.h"
#include "FractureEngineClustering.h"
#include "FractureEngineSelection.h"
#include "FractureEngineFracturing.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GeometryCollectionFracturingNodes)

namespace Dataflow
{

	void GeometryCollectionFracturingNodes()
	{
		static const FLinearColor CDefaultNodeBodyTintColor = FLinearColor(0.f, 0.f, 0.f, 0.5f);

		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FUniformScatterPointsDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FRadialScatterPointsDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FVoronoiFractureDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FPlaneCutterDataflowNode);
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FExplodedViewDataflowNode);

		// GeometryCollection|Fracture
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY_NODE_COLORS_BY_CATEGORY("GeometryCollection|Fracture", FLinearColor(1.f, 1.f, 0.8f), CDefaultNodeBodyTintColor);
	}
}

void FUniformScatterPointsDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<TArray<FVector>>(&Points))
	{
		const FBox& BBox = GetValue<FBox>(Context, &BoundingBox);
		if (BBox.GetVolume() > 0.f)
		{
			FRandomStream RandStream(GetValue<float>(Context, &RandomSeed));

			const FVector Extent(BBox.Max - BBox.Min);
			const int32 NumPoints = RandStream.RandRange(GetValue<int32>(Context, &MinNumberOfPoints), GetValue<int32>(Context, &MaxNumberOfPoints));

			TArray<FVector> PointsArr;
			PointsArr.Reserve(NumPoints);
			for (int32 Idx = 0; Idx < NumPoints; ++Idx)
			{
				PointsArr.Emplace(BBox.Min + FVector(RandStream.FRand(), RandStream.FRand(), RandStream.FRand()) * Extent);
			}

			SetValue(Context, MoveTemp(PointsArr), &Points);
		}
		else
		{
			// ERROR: Invalid BoundingBox input
			SetValue(Context, TArray<FVector>(), &Points);
		}
	}
}

void FRadialScatterPointsDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<TArray<FVector>>(&Points))
	{
		const FVector::FReal RadialStep = GetValue<float>(Context, &Radius) / GetValue<int32>(Context, &RadialSteps);
		const FVector::FReal AngularStep = 2 * PI / GetValue<int32>(Context, &AngularSteps);

		FRandomStream RandStream(GetValue<float>(Context, &RandomSeed));
		FVector UpVector(GetValue<FVector>(Context, &Normal));
		UpVector.Normalize();
		FVector BasisX, BasisY;
		UpVector.FindBestAxisVectors(BasisX, BasisY);

		TArray<FVector> PointsArr;

		FVector::FReal Len = RadialStep * .5;
		for (int32 ii = 0; ii < GetValue<int32>(Context, &RadialSteps); ++ii, Len += RadialStep)
		{
			FVector::FReal Angle = FMath::DegreesToRadians(GetValue<float>(Context, &AngleOffset));
			for (int32 kk = 0; kk < AngularSteps; ++kk, Angle += AngularStep)
			{
				FVector RotatingOffset = Len * (FMath::Cos(Angle) * BasisX + FMath::Sin(Angle) * BasisY);
				PointsArr.Emplace(GetValue<FVector>(Context, &Center) + RotatingOffset + (RandStream.VRand() * RandStream.FRand() * Variability));
			}
		}

		SetValue(Context, MoveTemp(PointsArr), &Points);
	}
}


void FVoronoiFractureDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FManagedArrayCollection>(&Collection))
	{
		const FDataflowTransformSelection& InTransformSelection = GetValue<FDataflowTransformSelection>(Context, &TransformSelection);

		if (IsConnected<FDataflowTransformSelection>(&TransformSelection))
		{
			if (InTransformSelection.AnySelected())
			{
				FManagedArrayCollection InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);

				FFractureEngineFracturing::VoronoiFracture(InCollection,
					InTransformSelection,
					GetValue<TArray<FVector>>(Context, &Points),
					GetValue<float>(Context, &RandomSeed),
					GetValue<float>(Context, &ChanceToFracture),
					GroupFracture,
					GetValue<float>(Context, &Grout),
					GetValue<float>(Context, &Amplitude),
					GetValue<float>(Context, &Frequency),
					GetValue<float>(Context, &Persistence),
					GetValue<float>(Context, &Lacunarity),
					GetValue<int32>(Context, &OctaveNumber),
					GetValue<float>(Context, &PointSpacing),
					AddSamplesForCollision,
					GetValue<float>(Context, &CollisionSampleSpacing));

				SetValue(Context, MoveTemp(InCollection), &Collection);

				return;
			}
		}

		const FManagedArrayCollection& InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);
		SetValue(Context, InCollection, &Collection);
	}
}


void FPlaneCutterDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FManagedArrayCollection>(&Collection))
	{
		const FDataflowTransformSelection& InTransformSelection = GetValue<FDataflowTransformSelection>(Context, &TransformSelection);

		if (IsConnected<FDataflowTransformSelection>(&TransformSelection))
		{
			if (InTransformSelection.AnySelected())
			{
				FManagedArrayCollection InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);

				FFractureEngineFracturing::PlaneCutter(InCollection,
					InTransformSelection,
					GetValue<FBox>(Context, &BoundingBox),
					NumPlanes,
					GetValue<float>(Context, &RandomSeed),
					GetValue<float>(Context, &Grout),
					GetValue<float>(Context, &Amplitude),
					GetValue<float>(Context, &Frequency),
					GetValue<float>(Context, &Persistence),
					GetValue<float>(Context, &Lacunarity),
					GetValue<int32>(Context, &OctaveNumber),
					GetValue<float>(Context, &PointSpacing),
					GetValue<bool>(Context, &AddSamplesForCollision),
					GetValue<float>(Context, &CollisionSampleSpacing));

				SetValue(Context, MoveTemp(InCollection), &Collection);

				return;
			}
		}

		const FManagedArrayCollection& InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);
		SetValue(Context, InCollection, &Collection);
	}
}


void FExplodedViewDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<FManagedArrayCollection>(&Collection))
	{
		FManagedArrayCollection InCollection = GetValue<FManagedArrayCollection>(Context, &Collection);

		FFractureEngineFracturing::GenerateExplodedViewAttribute(InCollection, GetValue<FVector>(Context, &Scale), GetValue<float>(Context, &UniformScale));

		SetValue(Context, MoveTemp(InCollection), &Collection);
	}
}




