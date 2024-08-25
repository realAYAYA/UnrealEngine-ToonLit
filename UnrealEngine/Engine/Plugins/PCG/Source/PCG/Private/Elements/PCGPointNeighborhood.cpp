// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGPointNeighborhood.h"

#include "PCGContext.h"
#include "Data/PCGPointData.h"
#include "Helpers/PCGAsync.h"
#include "Metadata/Accessors/PCGAttributeAccessorHelpers.h"

#define LOCTEXT_NAMESPACE "PCGPointNeighborhoodElement"

namespace PCGPointNeighborhood
{
	template<typename T>
	void SetAttributeHelper(UPCGPointData* Data, const FName& AttributeName, const TArrayView<const T> Values)
	{
		// Discard None attributes
		if (AttributeName == NAME_None)
		{
			return;
		}

		ensure(Data->Metadata->FindOrCreateAttribute<T>(AttributeName));

		FPCGAttributePropertySelector AttributeSelector = FPCGAttributePropertySelector::CreateAttributeSelector(AttributeName);

		TUniquePtr<IPCGAttributeAccessor> Accessor = PCGAttributeAccessorHelpers::CreateAccessor(Data, AttributeSelector);
		TUniquePtr<IPCGAttributeAccessorKeys> Keys = PCGAttributeAccessorHelpers::CreateKeys(Data, AttributeSelector);
		
		if (Accessor.IsValid() && Keys.IsValid())
		{
			Accessor->SetRange(Values, 0, *Keys);
		}
		else
		{
			return;
		}
	}
}

FPCGElementPtr UPCGPointNeighborhoodSettings::CreateElement() const
{
	return MakeShared<FPCGPointNeighborhoodElement>();
}

bool FPCGPointNeighborhoodElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGPointNeighborhoodElement::Execute);

	const UPCGPointNeighborhoodSettings* Settings = Context->GetInputSettings<UPCGPointNeighborhoodSettings>();
	check(Settings);

	TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputs();
	TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;

	const double SearchDistance = Settings->SearchDistance;
	if (SearchDistance < UE_DOUBLE_SMALL_NUMBER)
	{
		PCGE_LOG(Error, GraphAndLog, LOCTEXT("InvalidSearchDistance", "Search Distance must be greater than 0."));
		return true;
	}

	struct FProcessResults
	{
		TArray<double> Distances;
		TArray<FVector> AveragePositions;
	};

	FProcessResults OutputDataBuffers;

	for (int i = 0; i < Inputs.Num(); ++i)
	{
		const UPCGPointData* InputPointData = Cast<UPCGPointData>(Inputs[i].Data);
		if (!InputPointData)
		{
			PCGE_LOG(Verbose, GraphAndLog, FText::Format(LOCTEXT("InvalidPointData", "Input {0} is not point data"), i));
			continue;
		}

		const TArray<FPCGPoint>& SrcPoints = InputPointData->GetPoints();
		FPCGTaggedData& Output = Outputs.Add_GetRef(Inputs[i]);
		UPCGPointData* OutputPointData = NewObject<UPCGPointData>();
		OutputPointData->InitializeFromData(InputPointData);
		TArray<FPCGPoint>& OutputPoints = OutputPointData->GetMutablePoints();
		OutputPoints.SetNumUninitialized(SrcPoints.Num());
		Output.Data = OutputPointData;
		
		const FBox SearchBounds(FVector(SearchDistance * -1.0), FVector(SearchDistance));

		FPCGProjectionParams Params{};
		Params.bProjectRotations = Params.bProjectScales = false;
		Params.ColorBlendMode = Settings->bSetAverageColor ? EPCGProjectionColorBlendMode::TargetValue : EPCGProjectionColorBlendMode::SourceValue;

		auto InitializeBuffers = [Settings, &OutputDataBuffers, Count = SrcPoints.Num()]()
		{
			if (Settings->bSetDistanceToAttribute)
			{
				OutputDataBuffers.Distances.SetNumUninitialized(Count);
			}

			if (Settings->bSetAveragePositionToAttribute)
			{
				OutputDataBuffers.AveragePositions.SetNumUninitialized(Count);
			}
		};

		auto ProcessPoint = [Settings, &OutputPoints, &OutputDataBuffers, &SrcPoints, &SearchBounds, &SearchDistance, &Params, &InputPointData, &OutputPointData](const int32 ReadIndex, const int32 WriteIndex)
		{

			const FPCGPoint& InPoint = SrcPoints[ReadIndex];
			FPCGPoint& OutPoint = OutputPoints[WriteIndex];
			OutPoint = InPoint;

			const FTransform InTransform(InPoint.Transform.GetLocation());
			UPCGMetadata* OutMetadata = OutputPointData->Metadata;

			FPCGPoint ProjectionPoint = FPCGPoint();
			InputPointData->ProjectPoint(InTransform, SearchBounds, Params, ProjectionPoint, OutMetadata, Settings->bWeightedAverage);

			const double NormalizedDistance = FVector::Distance(InPoint.Transform.GetLocation(), ProjectionPoint.Transform.GetLocation()) / SearchDistance;

			if (Settings->SetDensity == EPCGPointNeighborhoodDensityMode::SetNormalizedDistanceToDensity)
			{
				OutPoint.Density = FMath::Clamp(NormalizedDistance, 0.0, 1.0);
			}
			else if (Settings->SetDensity == EPCGPointNeighborhoodDensityMode::SetAverageDensity)
			{
				OutPoint.Density = ProjectionPoint.Density;
			}

			if (Settings->bSetDistanceToAttribute)
			{
				OutputDataBuffers.Distances[WriteIndex] = FVector::Distance(InPoint.Transform.GetLocation(), ProjectionPoint.Transform.GetLocation());
			}

			if (Settings->bSetAveragePosition)
			{
				OutPoint.Transform.SetLocation(ProjectionPoint.Transform.GetLocation());
			}

			if (Settings->bSetAveragePositionToAttribute)
			{
				OutputDataBuffers.AveragePositions[WriteIndex] = ProjectionPoint.Transform.GetLocation();
			}

			if (Settings->bSetAverageColor)
			{
				OutPoint.Color = ProjectionPoint.Color;
			}
		};

		FPCGAsync::AsyncProcessingOneToOneEx(&Context->AsyncState, SrcPoints.Num(), InitializeBuffers, ProcessPoint, /* bEnableTimeSlicing=*/false);

		if (Settings->bSetDistanceToAttribute)
		{
			PCGPointNeighborhood::SetAttributeHelper<double>(OutputPointData, Settings->DistanceAttribute, OutputDataBuffers.Distances);
		}
		if (Settings->bSetAveragePositionToAttribute)
		{
			PCGPointNeighborhood::SetAttributeHelper<FVector>(OutputPointData, Settings->AveragePositionAttribute, OutputDataBuffers.AveragePositions);
		}
	}

	return true;
}

#undef LOCTEXT_NAMESPACE
