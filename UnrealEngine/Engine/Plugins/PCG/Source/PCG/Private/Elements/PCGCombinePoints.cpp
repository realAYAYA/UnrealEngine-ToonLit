// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGCombinePoints.h"

#include "PCGContext.h"
#include "PCGPin.h"
#include "Data/PCGPointData.h"
#include "Helpers/PCGHelpers.h"

#define LOCTEXT_NAMESPACE "PCGCombinePointsElement"

FPCGElementPtr UPCGCombinePointsSettings::CreateElement() const
{
	return MakeShared<FPCGCombinePointsElement>();
}

bool FPCGCombinePointsElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGCombinePointsElement::Execute);
	check(Context);

	const UPCGCombinePointsSettings* Settings = Context->GetInputSettings<UPCGCombinePointsSettings>();
	check(Settings);

	const TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel);
	TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;

	for (int i = 0; i < Inputs.Num(); ++i)
	{
		const UPCGPointData* InputPointData = Cast<UPCGPointData>(Inputs[i].Data);
		if (!InputPointData)
		{
			PCGE_LOG(Warning, GraphAndLog, LOCTEXT("InvalidInputPointData", "The input is not point data, skipped."));
			continue;
		}

		const TArray<FPCGPoint>& InputPoints = InputPointData->GetPoints();
		if (InputPoints.IsEmpty())
		{
			PCGE_LOG(Warning, GraphAndLog, LOCTEXT("NoPointsFound", "No points were found in the input data, skipped."));
			continue;
		}

		FPCGPoint InputPoint = InputPoints[0];
		InputPoint.ApplyScaleToBounds();

		const FTransform& PointTransform = Settings->bUseFirstPointTransform ? InputPoint.Transform : Settings->PointTransform;
		const FMatrix InversePointTransform = PointTransform.ToInverseMatrixWithScale();

		FPCGTaggedData& Output = Outputs.Add_GetRef(Inputs[i]);
		UPCGPointData* OutputPointData = NewObject<UPCGPointData>();
		OutputPointData->InitializeFromData(InputPointData);
		FPCGPoint& OutputPoint = OutputPointData->GetMutablePoints().Emplace_GetRef();
		Output.Data = OutputPointData;

		FPCGPoint OutPoint = FPCGPoint();
		FBox OutBox(EForceInit::ForceInit);

		for (int j = 0; j < InputPoints.Num(); ++j)
		{
			const FPCGPoint& InPoint = InputPoints[j];

			OutBox += InPoint.GetLocalBounds().TransformBy(InPoint.Transform.ToMatrixWithScale() * InversePointTransform);
		};

		OutPoint.SetLocalBounds(OutBox);
		OutPoint.Transform = PointTransform;
		OutPoint.Seed = PCGHelpers::ComputeSeedFromPosition(OutPoint.Transform.GetLocation());

		if (Settings->bCenterPivot)
		{
			OutPoint.ResetPointCenter(FVector(0.5));
		}

		OutputPoint = OutPoint;
	}

	return true;
}

#undef LOCTEXT_NAMESPACE