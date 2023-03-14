// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGDensityFilter.h"
#include "Data/PCGPointData.h"
#include "Helpers/PCGAsync.h"
#include "Helpers/PCGSettingsHelpers.h"

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
	UPCGParamData* Params = Context->InputData.GetParams();

	// Forward any non-input data
	Outputs.Append(Context->InputData.GetAllSettings());

	const bool bInvertFilter = PCGSettingsHelpers::GetValue(GET_MEMBER_NAME_CHECKED(UPCGDensityFilterSettings, bInvertFilter), Settings->bInvertFilter, Params);
	const float LowerBound = PCGSettingsHelpers::GetValue(GET_MEMBER_NAME_CHECKED(UPCGDensityFilterSettings, LowerBound), Settings->LowerBound, Params);
	const float UpperBound = PCGSettingsHelpers::GetValue(GET_MEMBER_NAME_CHECKED(UPCGDensityFilterSettings, UpperBound), Settings->UpperBound, Params);
#if WITH_EDITORONLY_DATA
	const bool bKeepZeroDensityPoints = PCGSettingsHelpers::GetValue(GET_MEMBER_NAME_CHECKED(UPCGDensityFilterSettings, bKeepZeroDensityPoints), Settings->bKeepZeroDensityPoints, Params);
#else
	const bool bKeepZeroDensityPoints = false;
#endif

	const float MinBound = FMath::Min(LowerBound, UpperBound);
	const float MaxBound = FMath::Max(LowerBound, UpperBound);

	const bool bNoResults = (MaxBound <= 0.0f && !bInvertFilter) || (MinBound == 0.0f && MaxBound >= 1.0f && bInvertFilter);
	const bool bTrivialFilter = (MinBound <= 0.0f && MaxBound >= 1.0f && !bInvertFilter) || (MinBound == 0.0f && MaxBound == 0.0f && bInvertFilter);

	if (bNoResults && !bKeepZeroDensityPoints)
	{
		PCGE_LOG(Verbose, "Skipped - all inputs rejected");
		return true;
	}

	// TODO: embarassingly parallel loop
	for (const FPCGTaggedData& Input : Inputs)
	{
		FPCGTaggedData& Output = Outputs.Add_GetRef(Input);

		if (!Input.Data || Cast<UPCGSpatialData>(Input.Data) == nullptr)
		{
			PCGE_LOG(Error, "Invalid input data");
			continue;
		}

		// Skip processing if the transformation is trivial
		if (bTrivialFilter)
		{
			PCGE_LOG(Verbose, "Skipped - trivial filter");
			continue;
		}

		const UPCGPointData* OriginalData = Cast<UPCGSpatialData>(Input.Data)->ToPointData(Context);

		if (!OriginalData)
		{
			PCGE_LOG(Error, "Unable to get point data from input");
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
#if WITH_EDITORONLY_DATA
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

		PCGE_LOG(Verbose, "Generated %d points out of %d source points", FilteredPoints.Num(), Points.Num());
	}

	return true;
}