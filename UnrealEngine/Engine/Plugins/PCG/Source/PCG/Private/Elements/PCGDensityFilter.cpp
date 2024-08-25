// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGDensityFilter.h"

#include "Data/PCGSpatialData.h"
#include "PCGCustomVersion.h"
#include "Data/PCGPointData.h"
#include "Helpers/PCGAsync.h"
#include "PCGContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGDensityFilter)

#define LOCTEXT_NAMESPACE "PCGDensityFilterElement"

FPCGElementPtr UPCGDensityFilterSettings::CreateElement() const
{
	return MakeShared<FPCGDensityFilterElement>();
}

bool FPCGDensityFilterElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGDensityFilterElement::Execute);

	const UPCGDensityFilterSettings* Settings = Context->GetInputSettings<UPCGDensityFilterSettings>();
	check(Settings);

	TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputs();
	TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;

	const bool bInvertFilter = Settings->bInvertFilter;
	const float LowerBound = Settings->LowerBound;
	const float UpperBound = Settings->UpperBound;
#if WITH_EDITOR
	const bool bKeepZeroDensityPoints = Settings->bKeepZeroDensityPoints;
#else
	const bool bKeepZeroDensityPoints = false;
#endif

	const float MinBound = FMath::Min(LowerBound, UpperBound);
	const float MaxBound = FMath::Max(LowerBound, UpperBound);

	const bool bNoResults = (MaxBound <= 0.0f && !bInvertFilter) || (MinBound == 0.0f && MaxBound >= 1.0f && bInvertFilter);
	const bool bTrivialFilter = (MinBound <= 0.0f && MaxBound >= 1.0f && !bInvertFilter) || (MinBound == 0.0f && MaxBound == 0.0f && bInvertFilter);

	if (bNoResults && !bKeepZeroDensityPoints)
	{
		PCGE_LOG(Verbose, LogOnly, LOCTEXT("AllInputsRejected", "Skipped - all inputs rejected"));
		return true;
	}

	// TODO: embarassingly parallel loop
	for (const FPCGTaggedData& Input : Inputs)
	{
		FPCGTaggedData& Output = Outputs.Add_GetRef(Input);

		if (!Input.Data || Cast<UPCGSpatialData>(Input.Data) == nullptr)
		{
			PCGE_LOG(Error, GraphAndLog, LOCTEXT("InvalidInputData", "Invalid input data"));
			continue;
		}

		// Skip processing if the transformation is trivial
		if (bTrivialFilter)
		{
			PCGE_LOG(Verbose, LogOnly, LOCTEXT("TrivialFilter", "Skipped - trivial filter"));
			continue;
		}

		const UPCGPointData* OriginalData = Cast<UPCGSpatialData>(Input.Data)->ToPointData(Context);

		if (!OriginalData)
		{
			PCGE_LOG(Error, GraphAndLog, LOCTEXT("NoPointDataInInput", "Unable to get point data from input"));
			continue;
		}

		const TArray<FPCGPoint>& Points = OriginalData->GetPoints();
		
		UPCGPointData* FilteredData = NewObject<UPCGPointData>();
		FilteredData->InitializeFromData(OriginalData);
		TArray<FPCGPoint>& FilteredPoints = FilteredData->GetMutablePoints();

		Output.Data = FilteredData;

		FPCGAsync::AsyncPointProcessing(Context, Points.Num(), FilteredPoints, [&Points, MinBound, MaxBound, bInvertFilter, bKeepZeroDensityPoints](int32 Index, FPCGPoint& OutPoint)
		{
			const FPCGPoint& Point = Points[Index];

			bool bInRange = (Point.Density >= MinBound && Point.Density <= MaxBound);
			if (bInRange != bInvertFilter)
			{
				OutPoint = Point;
				return true;
			}
#if WITH_EDITOR
			else if (bKeepZeroDensityPoints)
			{
				OutPoint = Point;
				OutPoint.Density = 0;
				return true;
			}
#endif
			else
			{
				return false;
			}
		});

		PCGE_LOG(Verbose, LogOnly, FText::Format(LOCTEXT("GenerationInfo", "Generated {0} points out of {1} source points"), FilteredPoints.Num(), Points.Num()));
	}

	return true;
}

#undef LOCTEXT_NAMESPACE
