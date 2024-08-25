// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGSelfPruning.h"

#include "PCGContext.h"
#include "PCGCustomVersion.h"
#include "Data/PCGPointData.h"
#include "Data/PCGSpatialData.h"
#include "Metadata/Accessors/IPCGAttributeAccessor.h"
#include "Metadata/Accessors/PCGAttributeAccessorHelpers.h"
#include "Metadata/Accessors/PCGAttributeAccessorKeys.h"
#include "Metadata/Accessors/PCGAttributeExtractor.h"

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

	/**
	* Keep Extents sort alive since it is a bit faster than the SortByAttribute.
	*/
	void SortExtents(TArray<FPCGPointRef>& SortedPoints, const FPCGSelfPruningParameters& Parameters, FVector::FReal SquaredRadiusEquality)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FPCGSelfPruningElement::Execute::SortExtents);

		if (Parameters.PruningType == EPCGSelfPruningType::LargeToSmall)
		{
			if (Parameters.bRandomizedPruning)
			{
				Algo::Sort(SortedPoints, [SquaredRadiusEquality](const FPCGPointRef& A, const FPCGPointRef& B) { return !PCGSelfPruningAlgorithms::SortSmallToLargeWithRandom(A, B, SquaredRadiusEquality); });
			}
			else
			{
				Algo::Sort(SortedPoints, [SquaredRadiusEquality](const FPCGPointRef& A, const FPCGPointRef& B) { return !PCGSelfPruningAlgorithms::SortSmallToLargeNoRandom(A, B, SquaredRadiusEquality); });
			}
		}
		else //if (Parameters.PruningType == EPCGSelfPruningType::SmallToLarge)
		{
			if (Parameters.bRandomizedPruning)
			{
				Algo::Sort(SortedPoints, [SquaredRadiusEquality](const FPCGPointRef& A, const FPCGPointRef& B) { return PCGSelfPruningAlgorithms::SortSmallToLargeWithRandom(A, B, SquaredRadiusEquality); });
			}
			else
			{
				Algo::Sort(SortedPoints, [SquaredRadiusEquality](const FPCGPointRef& A, const FPCGPointRef& B) { return PCGSelfPruningAlgorithms::SortSmallToLargeNoRandom(A, B, SquaredRadiusEquality); });
			}
		}
	}
}

namespace PCGSelfPruningElement
{
	void FPointBitSet::Initialize(const TArray<FPCGPoint>& Points)
	{
		Bits.SetNumZeroed(Points.Num() / 32 + 1);
		FirstPoint = Points.Num() > 0 ? &Points[0] : nullptr;
	}

	void FPointBitSet::Add(const FPCGPoint* Point)
	{
		const uint32 Index = Point - FirstPoint;
		const uint32 BitsIndex = Index / 32;
		const uint32 Bit = Index % 32;

		Bits[BitsIndex] |= (1 << Bit);
	}

	bool FPointBitSet::Contains(const FPCGPoint* Point)
	{
		const uint32 Index = Point - FirstPoint;
		const uint32 BitsIndex = Index / 32;
		const uint32 Bit = Index % 32;

		return (Bits[BitsIndex] & (1 << Bit)) != 0;
	}

	static constexpr int32 TimeSliceFrequencyCheck = 255;

	bool ShouldStop(FPCGContext* InOptionalContext)
	{
		return InOptionalContext && InOptionalContext->TimeSliceIsEnabled() && InOptionalContext->ShouldStop();
	}

	bool DensityBoundsExclusion(FIterationState& IterationState, FPCGContext* InOptionalContext)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FPCGSelfPruningElement::Execute::DensityBoundsExclusion);

		check(IterationState.InputPointData);
		const UPCGPointData::PointOctree& Octree = IterationState.InputPointData->GetOctree();

		int32 CheckTimeSlicingCount = 0;

		while (IterationState.CurrentPointIndex < IterationState.SortedPoints.Num())
		{
			// Don't check too many times. Pre-increment will make sure we always at least process "TimeSliceFrequencyCheck" elements at each iteration.
			if (++CheckTimeSlicingCount >= TimeSliceFrequencyCheck)
			{
				if (ShouldStop(InOptionalContext))
				{
					return false;
				}

				CheckTimeSlicingCount = 0;
			}

			const FPCGPointRef& PointRef = IterationState.SortedPoints[IterationState.CurrentPointIndex++];
			if (IterationState.ExcludedPoints.Contains(PointRef.Point))
			{
				continue;
			}

			IterationState.ExclusionPoints.Add(PointRef.Point);

			Octree.FindElementsWithBoundsTest(FBoxCenterAndExtent(PointRef.Bounds.Origin, PointRef.Bounds.BoxExtent), [&IterationState](const FPCGPointRef& InPointRef)
			{
				// TODO: check on an oriented-box basis?
				if (!IterationState.ExclusionPoints.Contains(InPointRef.Point))
				{
					IterationState.ExcludedPoints.Add(InPointRef.Point);
				}
			});
		}

		return true;
	}

	bool DuplicatePointsExclusion(FIterationState& IterationState, FPCGContext* InOptionalContext)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FPCGSelfPruningElement::Execute::DuplicatePointsExclusion);

		check(IterationState.InputPointData);
		const UPCGPointData::PointOctree& Octree = IterationState.InputPointData->GetOctree();

		int32 CheckTimeSlicingCount = 0;

		while (IterationState.CurrentPointIndex < IterationState.SortedPoints.Num())
		{
			// Don't check too many times. Pre-increment will make sure we always at least process "TimeSliceFrequencyCheck" elements at each iteration.
			if (++CheckTimeSlicingCount >= TimeSliceFrequencyCheck)
			{
				if (ShouldStop(InOptionalContext))
				{
					return false;
				}

				CheckTimeSlicingCount = 0;
			}

			const FPCGPointRef& PointRef = IterationState.SortedPoints[IterationState.CurrentPointIndex++];
			if (IterationState.ExcludedPoints.Contains(PointRef.Point))
			{
				continue;
			}

			IterationState.ExclusionPoints.Add(PointRef.Point);

			Octree.FindElementsWithBoundsTest(FBoxCenterAndExtent(PointRef.Point->Transform.TransformPosition(PointRef.Point->GetLocalCenter()), FVector::Zero()), [&IterationState, PointRef](const FPCGPointRef& InPointRef)
			{
				if ((PointRef.Point->Transform.GetLocation() - InPointRef.Point->Transform.GetLocation()).SquaredLength() <= SMALL_NUMBER &&
				!IterationState.ExclusionPoints.Contains(InPointRef.Point))
				{
					IterationState.ExcludedPoints.Add(InPointRef.Point);
				}
			});
		}

		return true;
	}

	void Execute(FPCGContext* Context, EPCGSelfPruningType PruningType, float RadiusSimilarityFactor, bool bRandomizedPruning)
	{
		FPCGSelfPruningParameters Parameters;
		Parameters.PruningType = PruningType;
		Parameters.RadiusSimilarityFactor = RadiusSimilarityFactor;
		Parameters.bRandomizedPruning = bRandomizedPruning;
		Parameters.ComparisonSource.SetPointProperty(EPCGPointProperties::Extents);
		return Execute(Context, Parameters);
	}

	void Execute(FPCGContext* Context, const FPCGSelfPruningParameters& InParameters)
	{
		check(Context);

		// Early out: if pruning is disabled
		if (InParameters.PruningType == EPCGSelfPruningType::None)
		{
			Context->OutputData = Context->InputData;
			PCGE_LOG_C(Verbose, LogOnly, Context, LOCTEXT("TypeNotSpecified", "Skipped - Type is None"));
			return;
		}

		TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;

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
			FIterationState IterationState;
			IterationState.InputPointData = InputPointData;

			while (!ExecuteSlice(IterationState, InParameters, Context)) {}

			FPCGTaggedData& Output = Context->OutputData.TaggedData.Add_GetRef(Input);
			if (IterationState.OutputPointData != nullptr)
			{
				Output.Data = IterationState.OutputPointData;
			}
		}
	}

	bool ExecuteSlice(FIterationState& InState, const FPCGSelfPruningParameters& InParameters, FPCGContext* InOptionalContext)
	{
		// Early out: if pruning is disabled
		if (InParameters.PruningType == EPCGSelfPruningType::None)
		{
			if (InOptionalContext)
			{
				PCGE_LOG_C(Verbose, LogOnly, InOptionalContext, LOCTEXT("TypeNotSpecified", "Skipped - Type is None"));
			}
			return true;
		}

		const FVector::FReal RadiusEquality = 1.0f + InParameters.RadiusSimilarityFactor;
		const FVector::FReal SquaredRadiusEquality = FMath::Square(RadiusEquality);
		const TArray<FPCGPoint>& Points = InState.InputPointData->GetPoints();

		// Force octree computation, and check if we need to stop after that if it was dirty in the first place.
		const bool bOctreeWasDirty = InState.InputPointData->IsOctreeDirty();
		const UPCGPointData::PointOctree& Octree = InState.InputPointData->GetOctree();
		if (bOctreeWasDirty && ShouldStop(InOptionalContext))
		{
			return false;
		}

		// Self-pruning will be done as follows:
		// For each point:
		//  if in its vicinity, there is >=1 non-rejected point with a radius significantly larger
		//  or in its range + has a randomly assigned index -> we'll look at its seed
		//  then remove this point
		if (!InState.bSortDone)
		{
			InState.SortedPoints.Empty();
			InState.SortedPoints.Reserve(Points.Num());
			for (const FPCGPoint& Point : Points)
			{
				InState.SortedPoints.Add(FPCGPointRef(Point));
			}

			FPCGAttributePropertySelector ComparisonSource = InParameters.ComparisonSource.CopyAndFixLast(InState.InputPointData);

			// First sort the points if we need sorting. 
			// Randomize for other cases than LargeToSmall and SmallToLarge are just a seed sort, so special case for that.
			// Special case for extents too, since the SortByAttribute is slightly slower (~10%) due to the overhead of working with accessors.
			// To not impact existing graphs (and since it is already one of the slowest operations in the graph), we will keep the old sorting directly on the extents.
			// Note that sorting is not time sliced.
			// TODO: Might be a good thing to have a future there.
			bool bSortAttribute = InParameters.PruningType == EPCGSelfPruningType::LargeToSmall || InParameters.PruningType == EPCGSelfPruningType::SmallToLarge;
			if (InParameters.bRandomizedPruning && !bSortAttribute)
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(FPCGSelfPruningElement::Execute::SortingSeeds);
				Algo::Sort(InState.SortedPoints, PCGSelfPruningAlgorithms::RandomSort);
			}
			else if (ComparisonSource.GetSelection() == EPCGAttributePropertySelection::PointProperty && ComparisonSource.GetPointProperty() == EPCGPointProperties::Extents && ComparisonSource.GetExtraNames().IsEmpty())
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(FPCGSelfPruningElement::Execute::SortingExtents);
				PCGSelfPruningAlgorithms::SortExtents(InState.SortedPoints, InParameters, SquaredRadiusEquality);
			}
			else
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(FPCGSelfPruningElement::Execute::SortingGeneric);

				const bool bAscending = InParameters.PruningType != EPCGSelfPruningType::LargeToSmall;

				TUniquePtr<const IPCGAttributeAccessor> InputAccessor = PCGAttributeAccessorHelpers::CreateConstAccessor(InState.InputPointData, ComparisonSource);

				if (!InputAccessor.IsValid())
				{
					PCGLog::LogErrorOnGraph(FText::Format(LOCTEXT("InvalidAttribute", "Attribute/Property '{0}' was not found."), ComparisonSource.GetDisplayText()), InOptionalContext);
					return true;
				}

				// For vector attributes, collapse them to their square length for comparison (was the previous behavior with extents).
				if (PCG::Private::IsOfTypes<FVector2D, FVector, FVector4>(InputAccessor->GetUnderlyingType()))
				{
					// Force squared length extraction on vectors
					ComparisonSource.GetExtraNamesMutable().Add(PCGAttributeExtractorConstants::VectorSquaredLength.ToString());
					InputAccessor = PCGAttributeAccessorHelpers::CreateConstAccessor(InState.InputPointData, ComparisonSource);
				}

				if (!PCG::Private::IsOfTypes<int32, int64, float, double, uint8>(InputAccessor->GetUnderlyingType()))
				{
					PCGLog::LogErrorOnGraph(FText::Format(LOCTEXT("NotNumericAttribute", "Attribute/Property '{0}' is not numeric ({1})"), ComparisonSource.GetDisplayText(), PCG::Private::GetTypeNameText(InputAccessor->GetUnderlyingType())), InOptionalContext);
					return true;
				}

				if (InParameters.bRandomizedPruning)
				{
					auto CompareLessWithRandom = [SquaredRadiusEquality, &Points](const auto& A, const auto& B, int32 IndexA, int32 IndexB, bool)
					{
						static_assert(std::is_same_v<decltype(A), decltype(B)>);

						using ValueType = std::remove_const_t<std::remove_reference_t<decltype(A)>>;

						if constexpr (std::is_arithmetic_v<ValueType>)
						{
							ValueType ACopy = static_cast<ValueType>(static_cast<double>(A) * SquaredRadiusEquality);
							if (PCG::Private::MetadataTraits<ValueType>::Less(ACopy, B))
							{
								return true;
							}

							ValueType BCopy = static_cast<ValueType>(static_cast<double>(B) * SquaredRadiusEquality);
							if (PCG::Private::MetadataTraits<ValueType>::Less(BCopy, A))
							{
								return false;
							}
						}
						else
						{
							if (PCG::Private::MetadataTraits<ValueType>::Less(A, B))
							{
								return true;
							}
							else if (PCG::Private::MetadataTraits<ValueType>::Less(B, A))
							{
								return false;
							}
						}

						return Points[IndexA].Seed < Points[IndexB].Seed;
					};

					PCGAttributeAccessorHelpers::SortByAttribute(*InputAccessor, FPCGAttributeAccessorKeysPoints(Points), InState.SortedPoints, bAscending, PCGAttributeAccessorHelpers::Private::DefaultIndexGetter, std::move(CompareLessWithRandom));
				}
				else
				{
					auto CompareLess = [SquaredRadiusEquality](const auto& A, const auto& B, int32, int32, bool)
					{
						static_assert(std::is_same_v<decltype(A), decltype(B)>);

						using ValueType = std::remove_const_t<std::remove_reference_t<decltype(A)>>;
						using CompareType = std::conditional_t<std::is_arithmetic_v<ValueType>, ValueType, const ValueType&>;

						CompareType ACopy = A;

						if constexpr (std::is_arithmetic_v<ValueType>)
						{
							ACopy = static_cast<ValueType>(static_cast<double>(A) * SquaredRadiusEquality);
						}

						return PCG::Private::MetadataTraits<ValueType>::Less(ACopy, B);
					};

					PCGAttributeAccessorHelpers::SortByAttribute(*InputAccessor, FPCGAttributeAccessorKeysPoints(Points), InState.SortedPoints, bAscending, PCGAttributeAccessorHelpers::Private::DefaultIndexGetter, std::move(CompareLess));
				}
			}

			InState.ExclusionPoints.Initialize(Points);
			InState.ExcludedPoints.Initialize(Points);

			InState.bSortDone = true;

			// After sorting, allow ourselves to stop if needed.
			if (ShouldStop(InOptionalContext))
			{
				return false;
			}
		}

		// Find excluded/duplicate points. Time sliced.
		const bool bIsDuplicateTest = (InParameters.PruningType == EPCGSelfPruningType::RemoveDuplicates);
		bool bIsDone = false;
		if (bIsDuplicateTest)
		{
			bIsDone = PCGSelfPruningElement::DuplicatePointsExclusion(InState, InOptionalContext);
		}
		else
		{
			bIsDone = PCGSelfPruningElement::DensityBoundsExclusion(InState, InOptionalContext);
		}

		// Finally, output all points that are present in the ExclusionPoints. This part is not time sliced, should it be too?
		if (bIsDone)
		{
			UPCGPointData* PrunedData = NewObject<UPCGPointData>();
			PrunedData->InitializeFromData(InState.InputPointData);
			InState.OutputPointData = PrunedData;

			TArray<FPCGPoint>& OutputPoints = PrunedData->GetMutablePoints();

			for (const FPCGPoint& Point : Points)
			{
				if (InState.ExclusionPoints.Contains(&Point))
				{
					OutputPoints.Add(Point);
				}
			}

			if (InOptionalContext)
			{
				if (bIsDuplicateTest)
				{
					PCGE_LOG_C(Verbose, LogOnly, InOptionalContext, FText::Format(LOCTEXT("GenerationInfoDuplicate", "Removed {0} duplicate points from {1} source points"), Points.Num() - OutputPoints.Num(), Points.Num()));
				}
				else
				{
					PCGE_LOG_C(Verbose, LogOnly, InOptionalContext, FText::Format(LOCTEXT("GenerationInfo", "Generated {0} points from {1} source points"), OutputPoints.Num(), Points.Num()));
				}
			}
		}

		return bIsDone;
	}
}

UPCGSelfPruningSettings::UPCGSelfPruningSettings()
{
	// Previous Default behavior was Extents
	Parameters.ComparisonSource.SetPointProperty(EPCGPointProperties::Extents);
}

void UPCGSelfPruningSettings::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITOR
	if (PruningType_DEPRECATED != EPCGSelfPruningType::LargeToSmall)
	{
		Parameters.PruningType = PruningType_DEPRECATED;
		PruningType_DEPRECATED = EPCGSelfPruningType::LargeToSmall;
	}

	if (RadiusSimilarityFactor_DEPRECATED != 0.25f)
	{
		Parameters.RadiusSimilarityFactor = RadiusSimilarityFactor_DEPRECATED;
		RadiusSimilarityFactor_DEPRECATED = 0.25;
	}

	if (!bRandomizedPruning_DEPRECATED)
	{
		Parameters.bRandomizedPruning = bRandomizedPruning_DEPRECATED;
		bRandomizedPruning_DEPRECATED = true;
	}
#endif // WITH_EDITOR
}

FPCGElementPtr UPCGSelfPruningSettings::CreateElement() const
{
	return MakeShared<FPCGSelfPruningElement>();
}

bool FPCGSelfPruningElement::PrepareDataInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGSelfPruningElement::PrepareDataInternal);

	FPCGSelfPruningElement::ContextType* TimeSlicedContext = static_cast<FPCGSelfPruningElement::ContextType*>(InContext);
	check(TimeSlicedContext);

	TimeSlicedContext->SetTimeSliceIsEnabled(true);

	TArray<FPCGTaggedData> Inputs = InContext->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel);

	// No global execution state.
	EPCGTimeSliceInitResult InitResult = TimeSlicedContext->InitializePerExecutionState();

	TimeSlicedContext->InitializePerIterationStates(Inputs.Num(), [&Inputs, InContext](PCGSelfPruningElement::FIterationState& OutState, const FPCGSelfPruningElement::ExecStateType&, int32 Index) -> EPCGTimeSliceInitResult
	{
		const UPCGSpatialData* SpatialData = Cast<UPCGSpatialData>(Inputs[Index].Data);
		if (!SpatialData)
		{
			PCGE_LOG_C(Error, GraphAndLog, InContext, FText::Format(LOCTEXT("InvalidInputDataType", "Input {0}: Input data must be of type Spatial"), FText::AsNumber(Index)));
			return EPCGTimeSliceInitResult::NoOperation;
		}

		// Retro-compatibility
		OutState.InputPointData = SpatialData->ToPointData();
		if (!OutState.InputPointData)
		{
			return EPCGTimeSliceInitResult::NoOperation;
		}

		return EPCGTimeSliceInitResult::Success;
	});

	return true;
}

bool FPCGSelfPruningElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGSelfPruningElement::Execute);

	const UPCGSelfPruningSettings* Settings = Context->GetInputSettings<UPCGSelfPruningSettings>();
	check(Settings);

	FPCGSelfPruningElement::ContextType* TimeSlicedContext = static_cast<FPCGSelfPruningElement::ContextType*>(Context);
	check(TimeSlicedContext);

	// Prepare data failed, no need to execute. Return an empty output
	if (!TimeSlicedContext->DataIsPreparedForExecution())
	{
		TimeSlicedContext->OutputData.TaggedData.Empty();
		return true;
	}

	TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel);

	return ExecuteSlice(TimeSlicedContext, [Settings, &Inputs](FPCGSelfPruningElement::ContextType* Context, const FPCGSelfPruningElement::ExecStateType&, PCGSelfPruningElement::FIterationState& IterState, const uint32 IterationIndex) -> bool
	{
		const bool bIsDone = PCGSelfPruningElement::ExecuteSlice(IterState, Settings->Parameters, Context);

		if (bIsDone)
		{
			FPCGTaggedData& Output = Context->OutputData.TaggedData.Add_GetRef(Inputs[IterationIndex]);
			if (IterState.OutputPointData != nullptr)
			{
				Output.Data = IterState.OutputPointData;
			}
		}

		return bIsDone;
	});

	return true;
}

#undef LOCTEXT_NAMESPACE
