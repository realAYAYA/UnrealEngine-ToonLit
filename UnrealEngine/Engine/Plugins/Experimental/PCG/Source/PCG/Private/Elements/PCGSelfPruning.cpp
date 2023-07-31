// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGSelfPruning.h"
#include "Data/PCGPointData.h"
#include "Data/PCGSpatialData.h"
#include "Helpers/PCGSettingsHelpers.h"
#include "PCGHelpers.h"
#include "Math/RandomStream.h"

namespace PCGSelfPruningAlgorithms
{
	bool RandomSort(const FPCGPoint* A, const FPCGPoint* B)
	{
		return A->Seed < B->Seed;
	}

	bool SortSmallToLargeNoRandom(const FPCGPoint* A, const FPCGPoint* B, FVector::FReal SquaredRadiusEquality)
	{
		return A->GetDensityBounds().BoxExtent.SquaredLength() * SquaredRadiusEquality < B->GetDensityBounds().BoxExtent.SquaredLength();
	}

	bool SortSmallToLargeWithRandom(const FPCGPoint* A, const FPCGPoint* B, FVector::FReal SquaredRadiusEquality)
	{
		const FVector::FReal SqrLenA = A->GetDensityBounds().BoxExtent.SquaredLength();
		const FVector::FReal SqrLenB = B->GetDensityBounds().BoxExtent.SquaredLength();
		if (SqrLenA * SquaredRadiusEquality < SqrLenB)
		{
			return true;
		}
		else if (SqrLenB * SquaredRadiusEquality < SqrLenA)
		{
			return false;
		}
		else
		{
			return RandomSort(A, B);
		}
	}
}

namespace PCGSelfPruningElement
{
	void DensityBoundsExclusion(const TArray<const FPCGPoint*>& SortedPoints, const UPCGPointData::PointOctree& Octree, TSet<const FPCGPoint*>& ExclusionPoints)
	{
		TSet<const FPCGPoint*> ExcludedPoints;
		ExcludedPoints.Reserve(SortedPoints.Num());
		
		for (const FPCGPoint* Point : SortedPoints)
		{
			if (ExcludedPoints.Contains(Point))
			{
				continue;
			}

			ExclusionPoints.Add(Point);

			const FBoxSphereBounds PointBounds = Point->GetDensityBounds();
			Octree.FindElementsWithBoundsTest(FBoxCenterAndExtent(PointBounds.Origin, PointBounds.BoxExtent), [&ExclusionPoints, &ExcludedPoints](const FPCGPointRef& InPointRef)
			{
				// TODO: check on an oriented-box basis?
				if (!ExclusionPoints.Contains(InPointRef.Point))
				{
					ExcludedPoints.Add(InPointRef.Point);
				}
			});
		}
	}

	void DuplicatePointsExclusion(const TArray<const FPCGPoint*>& SortedPoints, const UPCGPointData::PointOctree& Octree, TSet<const FPCGPoint*>& ExclusionPoints)
	{
		TSet<const FPCGPoint*> ExcludedPoints;
		ExcludedPoints.Reserve(SortedPoints.Num());

		for (const FPCGPoint* Point : SortedPoints)
		{
			if (ExcludedPoints.Contains(Point))
			{
				continue;
			}

			ExclusionPoints.Add(Point);

			Octree.FindElementsWithBoundsTest(FBoxCenterAndExtent(Point->Transform.TransformPosition(Point->GetLocalCenter()), FVector::Zero()), [&ExclusionPoints, &ExcludedPoints, Point](const FPCGPointRef& InPointRef)
			{
				if ((Point->Transform.GetLocation() - InPointRef.Point->Transform.GetLocation()).SquaredLength() <= SMALL_NUMBER &&
					!ExclusionPoints.Contains(InPointRef.Point))
				{
					ExcludedPoints.Add(InPointRef.Point);
				}
			});
		}
	}

	void Execute(FPCGContext* Context, EPCGSelfPruningType PruningType, float RadiusSimilarityFactor, bool bRandomizedPruning)
	{
		// Early out: if pruning is disabled
		if (PruningType == EPCGSelfPruningType::None)
		{
			Context->OutputData = Context->InputData;
			PCGE_LOG_C(Verbose, Context, "Skipped - Type is none");
			return;
		}

		const FVector::FReal RadiusEquality = 1.0f + RadiusSimilarityFactor;
		const FVector::FReal SquaredRadiusEquality = FMath::Square(RadiusEquality);

		TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;

		// TODO: embarassingly parallel loop
		TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputs();
		for (const FPCGTaggedData& Input : Inputs)
		{
			const UPCGSpatialData* SpatialInput = Cast<UPCGSpatialData>(Input.Data);

			if (!SpatialInput)
			{
				PCGE_LOG_C(Error, Context, "Invalid input data");
				continue;
			}

			const UPCGPointData* InputPointData = SpatialInput->ToPointData(Context);
			const TArray<FPCGPoint>& Points = InputPointData->GetPoints();
			const UPCGPointData::PointOctree& Octree = InputPointData->GetOctree();

			// Self-pruning will be done as follows:
			// For each point:
			//  if in its vicinity, there is >=1 non-rejected point with a radius significantly larger
			//  or in its range + has a randomly assigned index -> we'll look at its seed
			//  then remove this point
			TArray<const FPCGPoint*> SortedPoints;
			SortedPoints.Reserve(Points.Num());
			for (const FPCGPoint& Point : Points)
			{
				SortedPoints.Add(&Point);
			}

			// Apply proper sort algorithm
			if (PruningType == EPCGSelfPruningType::LargeToSmall)
			{
				if (bRandomizedPruning)
				{
					Algo::Sort(SortedPoints, [SquaredRadiusEquality](const FPCGPoint* A, const FPCGPoint* B) { return !PCGSelfPruningAlgorithms::SortSmallToLargeWithRandom(A, B, SquaredRadiusEquality); });
				}
				else
				{
					Algo::Sort(SortedPoints, [SquaredRadiusEquality](const FPCGPoint* A, const FPCGPoint* B) { return !PCGSelfPruningAlgorithms::SortSmallToLargeNoRandom(A, B, SquaredRadiusEquality); });
				}
			}
			else if (PruningType == EPCGSelfPruningType::SmallToLarge)
			{
				if (bRandomizedPruning)
				{
					Algo::Sort(SortedPoints, [SquaredRadiusEquality](const FPCGPoint* A, const FPCGPoint* B) { return PCGSelfPruningAlgorithms::SortSmallToLargeWithRandom(A, B, SquaredRadiusEquality); });
				}
				else
				{
					Algo::Sort(SortedPoints, [SquaredRadiusEquality](const FPCGPoint* A, const FPCGPoint* B) { return PCGSelfPruningAlgorithms::SortSmallToLargeNoRandom(A, B, SquaredRadiusEquality); });
				}
			}
			else
			{
				if (bRandomizedPruning)
				{
					Algo::Sort(SortedPoints, PCGSelfPruningAlgorithms::RandomSort);
				}
			}

			TSet<const FPCGPoint*> ExclusionPoints;
			ExclusionPoints.Reserve(Points.Num());

			const bool bIsDuplicateTest = (PruningType == EPCGSelfPruningType::RemoveDuplicates);

			if (bIsDuplicateTest)
			{
				PCGSelfPruningElement::DuplicatePointsExclusion(SortedPoints, Octree, ExclusionPoints);
			}
			else
			{
				PCGSelfPruningElement::DensityBoundsExclusion(SortedPoints, Octree, ExclusionPoints);
			}

			// Finally, output all points that are present in the ExclusionPoints.
			FPCGTaggedData& Output = Outputs.Emplace_GetRef();
			Output = Input;

			UPCGPointData* PrunedData = NewObject<UPCGPointData>();
			PrunedData->InitializeFromData(InputPointData);
			Output.Data = PrunedData;

			TArray<FPCGPoint>& OutputPoints = PrunedData->GetMutablePoints();
			OutputPoints.Reserve(ExclusionPoints.Num());

			for (const FPCGPoint* Point : ExclusionPoints)
			{
				OutputPoints.Add(*Point);
			}

			if (bIsDuplicateTest)
			{
				PCGE_LOG_C(Verbose, Context, "Removed %d duplicate points from %d source points", Points.Num() - OutputPoints.Num(), Points.Num());
			}
			else
			{
				PCGE_LOG_C(Verbose, Context, "Generated %d points from %d source points", OutputPoints.Num(), Points.Num());
			}
		}

		// Finally, forward any settings
		Outputs.Append(Context->InputData.GetAllSettings());
	}
}

FPCGElementPtr UPCGSelfPruningSettings::CreateElement() const
{
	return MakeShared<FPCGSelfPruningElement>();
}

bool FPCGSelfPruningElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGSelfPruningElement::Execute);
	// TODO: time-sliced implementation
	const UPCGSelfPruningSettings* Settings = Context->GetInputSettings<UPCGSelfPruningSettings>();
	check(Settings);

	UPCGParamData* Params = Context->InputData.GetParams();

	const EPCGSelfPruningType PruningType = PCGSettingsHelpers::GetValue(GET_MEMBER_NAME_CHECKED(UPCGSelfPruningSettings, PruningType), Settings->PruningType, Params);
	const float RadiusSimilarityFactor = PCGSettingsHelpers::GetValue(GET_MEMBER_NAME_CHECKED(UPCGSelfPruningSettings, RadiusSimilarityFactor), Settings->RadiusSimilarityFactor, Params);
	const bool bRandomizedPruning = PCGSettingsHelpers::GetValue(GET_MEMBER_NAME_CHECKED(UPCGSelfPruningSettings, bRandomizedPruning), Settings->bRandomizedPruning, Params);

	PCGSelfPruningElement::Execute(Context, PruningType, RadiusSimilarityFactor, bRandomizedPruning);

	return true;
}