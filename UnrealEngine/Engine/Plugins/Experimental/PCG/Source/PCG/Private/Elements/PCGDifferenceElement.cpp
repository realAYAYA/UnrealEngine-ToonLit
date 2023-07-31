// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGDifferenceElement.h"

#include "PCGCommon.h"
#include "Data/PCGPointData.h"
#include "Data/PCGUnionData.h"
#include "Helpers/PCGSettingsHelpers.h"

TArray<FPCGPinProperties> UPCGDifferenceSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PinProperties.Emplace(PCGDifferenceConstants::SourceLabel, EPCGDataType::Any);
	PinProperties.Emplace(PCGDifferenceConstants::DifferencesLabel, EPCGDataType::Spatial);

	return PinProperties;
}

FPCGElementPtr UPCGDifferenceSettings::CreateElement() const
{
	return MakeShared<FPCGDifferenceElement>();
}

bool FPCGDifferenceElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGDifferenceElement::Execute);

	// Early-out for previous behavior (without labeled edges)
	if (Context->Node && !Context->Node->IsInputPinConnected(PCGDifferenceConstants::SourceLabel) && !Context->Node->IsInputPinConnected(PCGDifferenceConstants::DifferencesLabel))
	{
		LabellessProcessing(Context);
		return true;
	}

	const UPCGDifferenceSettings* Settings = Context->GetInputSettings<UPCGDifferenceSettings>();
	check(Settings);

	TArray<FPCGTaggedData> Sources = Context->InputData.GetInputsByPin(PCGDifferenceConstants::SourceLabel);
	TArray<FPCGTaggedData> Differences = Context->InputData.GetInputsByPin(PCGDifferenceConstants::DifferencesLabel);
	TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;

	// Get only spatial data or build an union from the sources
	const UPCGSpatialData* FirstSpatialData = nullptr;
	UPCGUnionData* UnionData = nullptr;
	int32 DifferenceTaggedDataIndex = -1;

	bool bHasPointsInSource = false;
	bool bHasPointsInDifferences = false;

	// Start by either selecting a source or building a union
	for (FPCGTaggedData& Source : Sources)
	{
		const UPCGSpatialData* SpatialData = Cast<UPCGSpatialData>(Source.Data);

		// Non-spatial data, we're not going to touch
		if (!SpatialData)
		{
			Outputs.Add(Source);
			continue;
		}

		bHasPointsInSource |= (Cast<UPCGPointData>(SpatialData) != nullptr);

		if (!FirstSpatialData)
		{
			FirstSpatialData = SpatialData;
			DifferenceTaggedDataIndex = Outputs.Num();
			Outputs.Add(Source);
		}
		else
		{
			if (!UnionData)
			{
				UnionData = FirstSpatialData->UnionWith(SpatialData);
				// TODO: expose union settings?

				// Replace source by union
				Outputs[DifferenceTaggedDataIndex].Data = UnionData;
			}
			else
			{
				UnionData->AddData(SpatialData);
			}
		}
	}
	
	if (FirstSpatialData || UnionData)
	{
		check(DifferenceTaggedDataIndex >= 0);
		// Then, depending on the presence of differences, we'll create a difference as needed
		UPCGDifferenceData* DifferenceData = nullptr;
		for (FPCGTaggedData& Difference : Differences)
		{
			const UPCGSpatialData* SpatialData = Cast<UPCGSpatialData>(Difference.Data);

			if (SpatialData)
			{
				bHasPointsInDifferences |= (Cast<UPCGPointData>(SpatialData) != nullptr);

				if (!DifferenceData)
				{
					DifferenceData = (UnionData ? UnionData : FirstSpatialData)->Subtract(SpatialData);
					DifferenceData->SetDensityFunction(Settings->DensityFunction);
					DifferenceData->bDiffMetadata = Settings->bDiffMetadata;
#if WITH_EDITORONLY_DATA
					DifferenceData->bKeepZeroDensityPoints = Settings->bKeepZeroDensityPoints;
#endif

					Outputs[DifferenceTaggedDataIndex].Data = DifferenceData;
				}
				else
				{
					DifferenceData->AddDifference(SpatialData);
				}
			}
			else
			{
				// We are not propagating data from the differences if they don't contribute
			}
		}

		// Finally, apply any discretization based on the mode
		if (DifferenceData && 
			(Settings->Mode == EPCGDifferenceMode::Discrete || 
			(Settings->Mode == EPCGDifferenceMode::Inferred && bHasPointsInSource && bHasPointsInDifferences)))
		{
			Outputs[DifferenceTaggedDataIndex].Data = DifferenceData->ToPointData(Context);
		}
	}

	return true;
}

void FPCGDifferenceElement::LabellessProcessing(FPCGContext* Context) const
{
	const UPCGDifferenceSettings* Settings = Context->GetInputSettings<UPCGDifferenceSettings>();
	check(Settings);

	TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputs();
	UPCGParamData* Params = Context->InputData.GetParams();

	const EPCGDifferenceDensityFunction DensityFunction = PCGSettingsHelpers::GetValue(GET_MEMBER_NAME_CHECKED(UPCGDifferenceSettings, DensityFunction), Settings->DensityFunction, Params);
#if WITH_EDITORONLY_DATA
	const bool bKeepZeroDensityPoints = PCGSettingsHelpers::GetValue(GET_MEMBER_NAME_CHECKED(UPCGDifferenceSettings, bKeepZeroDensityPoints), Settings->bKeepZeroDensityPoints, Params);
#else
	const bool bKeepZeroDensityPoints = false;
#endif

	TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;

	const UPCGSpatialData* FirstSpatialData = nullptr;
	UPCGDifferenceData* DifferenceData = nullptr;
	int32 DifferenceTaggedDataIndex = -1;

	auto AddToDifference = [&FirstSpatialData, &DifferenceData, &DifferenceTaggedDataIndex, DensityFunction, bKeepZeroDensityPoints, &Outputs](const UPCGSpatialData* SpatialData) {
		if (!DifferenceData)
		{
			DifferenceData = FirstSpatialData->Subtract(SpatialData);
			DifferenceData->SetDensityFunction(DensityFunction);
#if WITH_EDITORONLY_DATA
			DifferenceData->bKeepZeroDensityPoints = bKeepZeroDensityPoints;
#endif

			FPCGTaggedData& DifferenceTaggedData = Outputs[DifferenceTaggedDataIndex];
			DifferenceTaggedData.Data = DifferenceData;
		}
		else
		{
			DifferenceData->AddDifference(SpatialData);
		}
	};

	// TODO: it might not make sense to perform the difference if the first
	// data isn't a spatial data, otherwise, what would it really mean?
	for (FPCGTaggedData& Input : Inputs)
	{
		const UPCGSpatialData* SpatialData = Cast<UPCGSpatialData>(Input.Data);

		// Non-spatial data, we're not going to touch
		if (!SpatialData)
		{
			Outputs.Add(Input);
			continue;
		}

		if (!FirstSpatialData)
		{
			FirstSpatialData = SpatialData;
			DifferenceTaggedDataIndex = Outputs.Num();
			Outputs.Add(Input);

			continue;
		}

		AddToDifference(SpatialData);
	}

	// Finally, pass-through settings
	Outputs.Append(Context->InputData.GetAllSettings());
}