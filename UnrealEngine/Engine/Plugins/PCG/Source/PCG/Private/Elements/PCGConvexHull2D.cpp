// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGConvexHull2D.h"

#include "PCGContext.h"
#include "PCGModule.h"
#include "Data/PCGPointData.h"

#include "Math/ConvexHull2d.h"

#define LOCTEXT_NAMESPACE "PCGConvexHull2DElement"

FPCGElementPtr UPCGConvexHull2DSettings::CreateElement() const
{
	return MakeShared<FPCGConvexHull2DElement>();
}

bool FPCGConvexHull2DElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGConvexHullElement::Execute);

	check(Context);

	const UPCGConvexHull2DSettings* Settings = Context->GetInputSettings<UPCGConvexHull2DSettings>();
	check(Settings);

	TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;

	for (const FPCGTaggedData& Input : Context->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel))
	{
		const UPCGPointData* PointData = Cast<UPCGPointData>(Input.Data);

		if (!PointData)
		{
			PCGE_LOG(Error, GraphAndLog, LOCTEXT("InputNotPointData", "Input is not a point data"));
			continue;
		}

		const TArray<FPCGPoint>& Points = PointData->GetPoints();

		TArray<FVector> PointsPositions;
		TArray<int32> ConvexHullIndices;
		Algo::Transform(Points, PointsPositions, [](const FPCGPoint& Point) { return Point.Transform.GetLocation(); });

		ConvexHull2D::ComputeConvexHull(PointsPositions, ConvexHullIndices);

		FPCGTaggedData& Output = Outputs.Add_GetRef(Input);
		UPCGPointData* OutputPointData = NewObject<UPCGPointData>();
		OutputPointData->InitializeFromData(PointData);
		TArray<FPCGPoint>& OutPoints = OutputPointData->GetMutablePoints();
		OutPoints.Reserve(ConvexHullIndices.Num());
		for (int32 Index : ConvexHullIndices)
		{
			OutPoints.Add(Points[Index]);
		}

		Output.Data = OutputPointData;
		Output.Pin = PCGPinConstants::DefaultOutputLabel;
	}

	return true;
}

#undef LOCTEXT_NAMESPACE