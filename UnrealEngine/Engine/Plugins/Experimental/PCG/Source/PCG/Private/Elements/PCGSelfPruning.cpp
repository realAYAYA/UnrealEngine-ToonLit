// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGSelfPruning.h"

#include "PCGContext.h"
#include "PCGCustomVersion.h"
#include "Data/PCGPointData.h"
#include "Data/PCGSpatialData.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGSelfPruning)

#define LOCTEXT_NAMESPACE "PCGSelfPruningElement"

namespace PCGSelfPruningAlgorithms
{
	bool RandomSort(const FPCGPointRef& A, const FPCGPointRef& B)
	{
		return A.Point->Seed < B.Point->Seed;
	}

	bool SortSmallToLargeNoRandom(const FPCGPointRef& A, const FPCGPointRef& B, FVector::FReal SquaredRadiusEquality)
	{
		return A.Bounds.BoxExtent.SquaredLength() * SquaredRadiusEquality < B.Bounds.BoxExtent.SquaredLength();
	}

	bool SortSmallToLargeWithRandom(const FPCGPointRef& A, const FPCGPointRef& B, FVector::FReal SquaredRadiusEquality)
	{
		const FVector::FReal SqrLenA = A.Bounds.BoxExtent.SquaredLength();
		const FVector::FReal SqrLenB = B.Bounds.BoxExtent.SquaredLength();
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
	// all points are in contiguous memory so this will create a bitset to just look up the index to set/test a bit
	// this should reduce the 'set' cost to O[1]
	struct FPointBitSet
	{
		TArray<uint32> Bits;
		const FPCGPoint* FirstPoint = nullptr;

		FPointBitSet(const TArray<FPCGPoint>& Points)
		{
			Bits.SetNumZeroed(Points.Num()/32 + 1);
			FirstPoint = Points.Num() > 0 ? &Points[0] : nullptr;
		}

		void Add(const FPCGPoint* Point)
		{
			const uint32 Index = Point-FirstPoint;
			const uint32 BitsIndex = Index / 32;
			const uint32 Bit = Index % 32;

			Bits[BitsIndex] |= (1<<Bit);
		}

		bool Contains(const FPCGPoint* Point)
		{
			const uint32 Index = Point-FirstPoint;
			const uint32 BitsIndex = Index / 32;
			const uint32 Bit = Index % 32;

			return (Bits[BitsIndex] & (1<<Bit)) != 0;
		}
	};

	void DensityBoundsExclusion(const TArray<FPCGPoint>& Points, const TArray<FPCGPointRef>& SortedPoints, const UPCGPointData::PointOctree& Octree, FPointBitSet& ExclusionPoints)
	{
		FPointBitSet ExcludedPoints(Points);
		
		for (const FPCGPointRef& PointRef : SortedPoints)
		{
			if (ExcludedPoints.Contains(PointRef.Point))
			{
				continue;
			}

			ExclusionPoints.Add(PointRef.Point);

			Octree.FindElementsWithBoundsTest(FBoxCenterAndExtent(PointRef.Bounds.Origin, PointRef.Bounds.BoxExtent), [&ExclusionPoints, &ExcludedPoints](const FPCGPointRef& InPointRef)
			{
				// TODO: check on an oriented-box basis?
				if (!ExclusionPoints.Contains(InPointRef.Point))
				{
					ExcludedPoints.Add(InPointRef.Point);
				}
			});
		}
	}

	void DuplicatePointsExclusion(const TArray<FPCGPoint>& Points, const TArray<FPCGPointRef>& SortedPoints, const UPCGPointData::PointOctree& Octree, FPointBitSet& ExclusionPoints)
	{
		FPointBitSet ExcludedPoints(Points);

		for (const FPCGPointRef& PointRef : SortedPoints)
		{
			if (ExcludedPoints.Contains(PointRef.Point))
			{
				continue;
			}

			ExclusionPoints.Add(PointRef.Point);

			Octree.FindElementsWithBoundsTest(FBoxCenterAndExtent(PointRef.Point->Transform.TransformPosition(PointRef.Point->GetLocalCenter()), FVector::Zero()), [&ExclusionPoints, &ExcludedPoints, PointRef](const FPCGPointRef& InPointRef)
			{
				if ((PointRef.Point->Transform.GetLocation() - InPointRef.Point->Transform.GetLocation()).SquaredLength() <= SMALL_NUMBER &&
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
			PCGE_LOG_C(Verbose, LogOnly, Context, LOCTEXT("TypeNotSpecified", "Skipped - Type is None"));
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
				PCGE_LOG_C(Error, GraphAndLog, Context, LOCTEXT("InvalidInputData", "Invalid input data"));
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
			TArray<FPCGPointRef> SortedPoints;
			SortedPoints.Reserve(Points.Num());
			for (const FPCGPoint& Point : Points)
			{
				SortedPoints.Add(FPCGPointRef(Point));
			}

			// Apply proper sort algorithm
			if (PruningType == EPCGSelfPruningType::LargeToSmall)
			{
				if (bRandomizedPruning)
				{
					Algo::Sort(SortedPoints, [SquaredRadiusEquality](const FPCGPointRef& A, const FPCGPointRef& B) { return !PCGSelfPruningAlgorithms::SortSmallToLargeWithRandom(A, B, SquaredRadiusEquality); });
				}
				else
				{
					Algo::Sort(SortedPoints, [SquaredRadiusEquality](const FPCGPointRef& A, const FPCGPointRef& B) { return !PCGSelfPruningAlgorithms::SortSmallToLargeNoRandom(A, B, SquaredRadiusEquality); });
				}
			}
			else if (PruningType == EPCGSelfPruningType::SmallToLarge)
			{
				if (bRandomizedPruning)
				{
					Algo::Sort(SortedPoints, [SquaredRadiusEquality](const FPCGPointRef& A, const FPCGPointRef& B) { return PCGSelfPruningAlgorithms::SortSmallToLargeWithRandom(A, B, SquaredRadiusEquality); });
				}
				else
				{
					Algo::Sort(SortedPoints, [SquaredRadiusEquality](const FPCGPointRef& A, const FPCGPointRef& B) { return PCGSelfPruningAlgorithms::SortSmallToLargeNoRandom(A, B, SquaredRadiusEquality); });
				}
			}
			else
			{
				if (bRandomizedPruning)
				{
					Algo::Sort(SortedPoints, PCGSelfPruningAlgorithms::RandomSort);
				}
			}

			FPointBitSet ExclusionPoints(Points);

			const bool bIsDuplicateTest = (PruningType == EPCGSelfPruningType::RemoveDuplicates);

			if (bIsDuplicateTest)
			{
				PCGSelfPruningElement::DuplicatePointsExclusion(Points, SortedPoints, Octree, ExclusionPoints);
			}
			else
			{
				PCGSelfPruningElement::DensityBoundsExclusion(Points, SortedPoints, Octree, ExclusionPoints);
			}

			// Finally, output all points that are present in the ExclusionPoints.
			FPCGTaggedData& Output = Outputs.Emplace_GetRef();
			Output = Input;

			UPCGPointData* PrunedData = NewObject<UPCGPointData>();
			PrunedData->InitializeFromData(InputPointData);
			Output.Data = PrunedData;

			TArray<FPCGPoint>& OutputPoints = PrunedData->GetMutablePoints();

			for (const FPCGPoint& Point : Points)
			{
				if (ExclusionPoints.Contains(&Point))
				{
					OutputPoints.Add(Point);
				}
			}				

			if (bIsDuplicateTest)
			{
				PCGE_LOG_C(Verbose, LogOnly, Context, FText::Format(LOCTEXT("GenerationInfoDuplicate", "Removed {0} duplicate points from {1} source points"), Points.Num() - OutputPoints.Num(), Points.Num()));
			}
			else
			{
				PCGE_LOG_C(Verbose, LogOnly, Context, FText::Format(LOCTEXT("GenerationInfo", "Generated {0} points from {1} source points"), OutputPoints.Num(), Points.Num()));
			}
		}
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

	const EPCGSelfPruningType PruningType = Settings->PruningType;
	const float RadiusSimilarityFactor = Settings->RadiusSimilarityFactor;
	const bool bRandomizedPruning = Settings->bRandomizedPruning;

	PCGSelfPruningElement::Execute(Context, PruningType, RadiusSimilarityFactor, bRandomizedPruning);

	return true;
}

#undef LOCTEXT_NAMESPACE
