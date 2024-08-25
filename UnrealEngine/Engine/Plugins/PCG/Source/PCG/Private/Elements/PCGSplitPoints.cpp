// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGSplitPoints.h"

#include "PCGContext.h"
#include "PCGPin.h"
#include "Data/PCGPointData.h"
#include "Helpers/PCGAsync.h"
#include "Metadata/PCGMetadataAccessor.h"

#define LOCTEXT_NAMESPACE "PCGSplitPointsElement"

namespace PCGSplitPointsConstants
{
	const FName OutputALabel = TEXT("Before Split");
	const FName OutputBLabel = TEXT("After Split");
}

TArray<FPCGPinProperties> UPCGSplitPointsSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PinProperties.Emplace(PCGSplitPointsConstants::OutputALabel, 
		EPCGDataType::Point, 
		/*bAllowMultipleConnections=*/true, 
		/*bAllowMultipleData=*/true, 
		LOCTEXT("PinATooltip", "The portion of each point before the split plane."));

	PinProperties.Emplace(PCGSplitPointsConstants::OutputBLabel, 
		EPCGDataType::Point, 
		/*bAllowMultipleConnections=*/true, 
		/*bAllowMultipleData=*/true, 
		LOCTEXT("PinBTooltip", "The portion of each point after the split plane."));

	return PinProperties;
}

FPCGElementPtr UPCGSplitPointsSettings::CreateElement() const
{
	return MakeShared<FPCGSplitPointsElement>();
}

bool FPCGSplitPointsElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGSplitPointsElement::Execute);
	check(Context);

	const UPCGSplitPointsSettings* Settings = Context->GetInputSettings<UPCGSplitPointsSettings>();
	check(Settings);

	const TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputs();
	TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;

	const float SplitPosition = FMath::Clamp(Settings->SplitPosition, 0.0f, 1.0f);

	for (int i = 0; i < Inputs.Num(); ++i)
	{
		const UPCGPointData* InputPointData = Cast<UPCGPointData>(Inputs[i].Data);
		if (!InputPointData)
		{
			PCGE_LOG(Verbose, GraphAndLog, FText::Format(LOCTEXT("InvalidPointData", "Input {0} is not point data"), i));
			continue;
		}

		const TArray<FPCGPoint>& InputPoints = InputPointData->GetPoints();

		FPCGTaggedData& OutputA = Outputs.Add_GetRef(Inputs[i]);
		UPCGPointData* OutPointDataA = NewObject<UPCGPointData>();
		OutPointDataA->InitializeFromData(InputPointData);
		TArray<FPCGPoint>& PointsA = OutPointDataA->GetMutablePoints();
		PointsA.SetNumUninitialized(InputPoints.Num());
		OutputA.Data = OutPointDataA;
		OutputA.Pin = PCGSplitPointsConstants::OutputALabel;

		FPCGTaggedData& OutputB = Outputs.Add_GetRef(Inputs[i]);
		UPCGPointData* OutPointDataB = NewObject<UPCGPointData>();
		OutPointDataB->InitializeFromData(InputPointData);
		TArray<FPCGPoint>& PointsB = OutPointDataB->GetMutablePoints();
		PointsB.SetNumUninitialized(InputPoints.Num());
		OutputB.Data = OutPointDataB;
		OutputB.Pin = PCGSplitPointsConstants::OutputBLabel;

		auto ProcessPoint = [Settings, SplitPosition, &InputPoints, &InputPointData, &OutPointDataA, &OutPointDataB, &PointsA, &PointsB](const int32 ReadIndex, const int32 WriteIndex)
		{
			const FPCGPoint& InPoint = InputPoints[ReadIndex];
			FPCGPoint PointA = InPoint;
			FPCGPoint PointB = InPoint;

			const FVector SplitterPosition = (InPoint.BoundsMax - InPoint.BoundsMin) * SplitPosition;
			const FVector MinPlusSplit = InPoint.BoundsMin + SplitterPosition;

			FVector SplitValues = FVector::ZeroVector;
			const int AxisIndex = static_cast<int>(Settings->SplitAxis);
			if (ensure(AxisIndex >= 0 && AxisIndex <= 2))
			{
				SplitValues[AxisIndex] = 1.0;
			}

			// Execution for PointsA portion
			PointA.BoundsMax = PointA.BoundsMax + SplitValues * (MinPlusSplit - PointA.BoundsMax);
			PointsA[WriteIndex] = PointA;

			// Execution of the PointsB portion
			PointB.BoundsMin = PointB.BoundsMin + SplitValues * (MinPlusSplit - PointB.BoundsMin);
			PointsB[WriteIndex] = PointB;
		};

		FPCGAsync::AsyncProcessingOneToOneEx(&Context->AsyncState, InputPoints.Num(), /*Initialize=*/[]() {}, ProcessPoint, /*bEnableTimeSlicing=*/false);
	}

	return true;
}

#undef LOCTEXT_NAMESPACE