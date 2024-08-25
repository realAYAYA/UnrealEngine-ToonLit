// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGDuplicatePoint.h"

#include "PCGContext.h"
#include "PCGPin.h"
#include "Data/PCGPointData.h"
#include "Helpers/PCGAsync.h"
#include "Helpers/PCGHelpers.h"
#include "Metadata/PCGMetadataAccessor.h"

#define LOCTEXT_NAMESPACE "PCGDuplicatePointElement"

FPCGElementPtr UPCGDuplicatePointSettings::CreateElement() const
{
	return MakeShared<FPCGDuplicatePointElement>();
}

bool FPCGDuplicatePointElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGDuplicatePointElement::Execute);
	check(Context);

	const UPCGDuplicatePointSettings* Settings = Context->GetInputSettings<UPCGDuplicatePointSettings>();
	check(Settings);

	const TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputs();
	TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;

	if (Settings->Iterations <= 0)
	{
		PCGE_LOG(Verbose, GraphAndLog, LOCTEXT("InvalidNumberOfIterations", "The number of interations must be at least 1."));
		return true;
	}

	const int Iterations = Settings->Iterations;

	const FVector Direction(FMath::Clamp(Settings->Direction.X, -1.0, 1.0), FMath::Clamp(Settings->Direction.Y, -1.0, 1.0), FMath::Clamp(Settings->Direction.Z, -1.0, 1.0));

	for (int i = 0; i < Inputs.Num(); ++i)
	{
		const UPCGPointData* InputPointData = Cast<UPCGPointData>(Inputs[i].Data);
		if (!InputPointData)
		{
			PCGE_LOG(Warning, GraphAndLog, LOCTEXT("InvalidInputPointData", "The input is not point data, skipped."));
			continue;
		}

		const TArray<FPCGPoint>& InputPoints = InputPointData->GetPoints();

		// Determines whether or not to include the source point in data
		const bool bKeepSourcePoint = Settings->bOutputSourcePoint;
		const int DuplicatesPerPoint = Iterations + (bKeepSourcePoint ? 1 : 0);
		const int NumIterations = DuplicatesPerPoint * InputPoints.Num();
		const int FirstDuplicateIndex = Settings->bOutputSourcePoint ? 0 : 1;

		FPCGTaggedData& Output = Outputs.Add_GetRef(Inputs[i]);
		if (InputPoints.IsEmpty())
		{
			Output.Data = InputPointData;
			return true;
		}

		UPCGPointData* OutPointData = NewObject<UPCGPointData>();
		OutPointData->InitializeFromData(InputPointData);
		TArray<FPCGPoint>& OutputPoints = OutPointData->GetMutablePoints();
		OutputPoints.SetNumUninitialized(NumIterations);
		Output.Data = OutPointData;

		const FTransform& SourceDuplicateTransform = Settings->PointTransform;

		if (Settings->bDirectionAppliedInRelativeSpace)
		{
			auto ProcessPoint = [SourceDuplicateTransform, DuplicatesPerPoint, bKeepSourcePoint, &Direction, &InputPoints, &OutputPoints](const int32 ReadIndex, const int32 WriteIndex)
			{
				const FPCGPoint& ReadPoint = InputPoints[ReadIndex];
				TArrayView<FPCGPoint> WritePoints(OutputPoints.GetData() + WriteIndex * DuplicatesPerPoint, DuplicatesPerPoint);

				const FTransform DuplicateAxisTransform = FTransform((ReadPoint.BoundsMax - ReadPoint.BoundsMin) * Direction);
				const FTransform DuplicateTransform = DuplicateAxisTransform * SourceDuplicateTransform;
				FTransform CurrentTransform = ReadPoint.Transform;

				int WritePointIndex = 0;

				if (bKeepSourcePoint)
				{
					WritePoints[WritePointIndex++] = ReadPoint;
				}

				while (WritePointIndex < DuplicatesPerPoint)
				{
					FPCGPoint& WritePoint = WritePoints[WritePointIndex++];
					WritePoint = ReadPoint;

					CurrentTransform = DuplicateTransform * CurrentTransform;
					WritePoint.Transform = CurrentTransform;
					WritePoint.Seed = PCGHelpers::ComputeSeedFromPosition(CurrentTransform.GetLocation());
				}
			};

			FPCGAsync::AsyncProcessingOneToOneEx(&Context->AsyncState, InputPoints.Num(), /*Initialize=*/[]() {}, ProcessPoint, /*bEnableTimeSlicing=*/false);
		}
		else
		{
			auto ProcessPoint = [SourceDuplicateTransform, FirstDuplicateIndex, OutPointData, InputPointData, &Direction, &InputPoints, &OutputPoints](const int32 ReadIndex, const int32 WriteIndex)
			{
				const FPCGPoint& InPoint = InputPoints[ReadIndex % InputPoints.Num()];
				FPCGPoint& OutputPoint = OutputPoints[WriteIndex];
				const int DuplicateIndex = FirstDuplicateIndex + ReadIndex / InputPoints.Num();

				OutputPoint = InPoint;

				if (DuplicateIndex != 0)
				{
					const FVector DuplicateLocationOffset = ((OutputPoint.BoundsMax - OutputPoint.BoundsMin) * Direction + SourceDuplicateTransform.GetLocation()) * DuplicateIndex;
					const FRotator DuplicateRotationOffset = SourceDuplicateTransform.Rotator() * DuplicateIndex;
					const FVector DuplicateScaleMultiplier = FVector(
						FMath::Pow(SourceDuplicateTransform.GetScale3D().X, DuplicateIndex), 
						FMath::Pow(SourceDuplicateTransform.GetScale3D().Y, DuplicateIndex), 
						FMath::Pow(SourceDuplicateTransform.GetScale3D().Z, DuplicateIndex));

					OutputPoint.Transform = FTransform(DuplicateRotationOffset, DuplicateLocationOffset, DuplicateScaleMultiplier) * InPoint.Transform;
					OutputPoint.Seed = PCGHelpers::ComputeSeedFromPosition(OutputPoint.Transform.GetLocation());
				}
			};

			FPCGAsync::AsyncProcessingOneToOneEx(&Context->AsyncState, NumIterations, /*Initialize=*/[]() {}, ProcessPoint, /*bEnableTimeSlicing=*/false);
		}
	}

	return true;
}

#undef LOCTEXT_NAMESPACE