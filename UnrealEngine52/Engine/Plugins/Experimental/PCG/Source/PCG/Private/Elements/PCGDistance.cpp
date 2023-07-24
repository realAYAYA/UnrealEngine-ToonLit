// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGDistance.h"
#include "Data/PCGSpatialData.h"
#include "Helpers/PCGAsync.h"
#include "Data/PCGPointData.h"
#include "PCGContext.h"
#include "PCGPin.h"

#define LOCTEXT_NAMESPACE "PCGDistanceElement"

namespace PCGDistance
{
	const FName SourceLabel = TEXT("Source");
	const FName TargetLabel = TEXT("Target");

	FVector CalcPosition(PCGDistanceShape Shape, const FPCGPoint& SourcePoint, const FPCGPoint& TargetPoint, const FVector SourceCenter, const FVector TargetCenter)
	{
		if (Shape == PCGDistanceShape::SphereBounds)
		{
			FVector Dir = TargetCenter - SourceCenter;
			Dir.Normalize();

			return SourceCenter + Dir * SourcePoint.GetScaledExtents().Length();
		}
		else if (Shape == PCGDistanceShape::BoxBounds)
		{
			const FVector LocalTargetCenter = SourcePoint.Transform.InverseTransformPosition(TargetCenter);

			const double DistanceSquared = ComputeSquaredDistanceFromBoxToPoint(SourcePoint.BoundsMin, SourcePoint.BoundsMax, LocalTargetCenter);

			FVector Dir = -LocalTargetCenter;
			Dir.Normalize();

			const FVector LocalClosestPoint = LocalTargetCenter + Dir * FMath::Sqrt(DistanceSquared);

			return SourcePoint.Transform.TransformPosition(LocalClosestPoint);
		}

		// PCGDistanceShape::Center
		return SourceCenter;
	}
}
	
TArray<FPCGPinProperties> UPCGDistanceSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PinProperties.Emplace(PCGDistance::SourceLabel, EPCGDataType::Point);
	PinProperties.Emplace(PCGDistance::TargetLabel, EPCGDataType::Point);

	return PinProperties;
}

TArray<FPCGPinProperties> UPCGDistanceSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PinProperties.Emplace(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Spatial);

	return PinProperties;
}

FPCGElementPtr UPCGDistanceSettings::CreateElement() const
{
	return MakeShared<FPCGDistanceElement>();
}

bool FPCGDistanceElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGDistanceElement::Execute);

	const UPCGDistanceSettings* Settings = Context->GetInputSettings<UPCGDistanceSettings>();
	check(Settings);

	const FName AttributeName = Settings->AttributeName;
	const bool bSetDensity = Settings->bSetDensity;
	const bool bOutputDistanceVector = Settings->bOutputDistanceVector;
	const double MaximumDistance = Settings->MaximumDistance;
	const PCGDistanceShape SourceShape = Settings->SourceShape;
	const PCGDistanceShape TargetShape = Settings->TargetShape;

	TArray<FPCGTaggedData> Sources = Context->InputData.GetInputsByPin(PCGDistance::SourceLabel);
	TArray<FPCGTaggedData> Targets = Context->InputData.GetInputsByPin(PCGDistance::TargetLabel);
	TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;

	FBox InputBounds(EForceInit::ForceInit);
	int32 InputPointCount = 0;

	TArray<const UPCGPointData*> TargetPointDatas;
	TargetPointDatas.Reserve(Targets.Num());

	for (const FPCGTaggedData& Target : Targets)
	{
		const UPCGSpatialData* TargetData = Cast<UPCGSpatialData>(Target.Data);

		if (!TargetData)
		{
			PCGE_LOG(Error, GraphAndLog, FText::Format(LOCTEXT("TargetMustBeSpatial", "Target must be Spatial data, found '{0}'"), FText::FromString(Target.Data->GetClass()->GetName())));
			continue;
		}

		const UPCGPointData* TargetPointData = TargetData->ToPointData(Context);
		if (!TargetPointData)
		{
			PCGE_LOG(Error, GraphAndLog, FText::Format(LOCTEXT("CannotConvertToPoint", "Cannot convert target '{0}' into Point data"), FText::FromString(Target.Data->GetClass()->GetName())));
			continue;			
		}

		TargetPointDatas.Add(TargetPointData);
	}

	// first find the total Input bounds which will determine the size of each cell
	for (const FPCGTaggedData& Source : Sources) 
	{
		// add the point bounds to the input cell

		const UPCGSpatialData* SourceData = Cast<UPCGSpatialData>(Source.Data);

		if (!SourceData)
		{
			PCGE_LOG(Error, GraphAndLog, LOCTEXT("InvalidInputData", "Invalid input data"));
			continue;
		}

		const UPCGPointData* SourcePointData = SourceData->ToPointData(Context);
		if (!SourcePointData)
		{
			PCGE_LOG(Error, GraphAndLog, LOCTEXT("CannotConvertToPointData", "Cannot convert input Spatial data to Point data"));
			continue;			
		}

		UPCGPointData* OutputData = NewObject<UPCGPointData>();
		OutputData->InitializeFromData(SourcePointData);
		Outputs.Add_GetRef(Source).Data = OutputData;

		FPCGMetadataAttribute<float>* ScalarAttribute = nullptr;
		FPCGMetadataAttribute<FVector>* VectorAttribute = nullptr;

		if (AttributeName != NAME_None && !bOutputDistanceVector)
		{
			ScalarAttribute = OutputData->Metadata->FindOrCreateAttribute<float>(AttributeName, 0.0f);
		}

		if (AttributeName != NAME_None && bOutputDistanceVector)
		{
			VectorAttribute = OutputData->Metadata->FindOrCreateAttribute<FVector>(AttributeName, FVector::ZeroVector);
		}

		FPCGAsync::AsyncPointProcessing(Context, SourcePointData->GetPoints(), OutputData->GetMutablePoints(),
			[OutputData, SourceShape, TargetShape, &TargetPointDatas, MaximumDistance, ScalarAttribute, VectorAttribute, bSetDensity](const FPCGPoint& SourcePoint, FPCGPoint& OutPoint) {

				OutPoint = SourcePoint;

				const FBoxSphereBounds SourceQueryBounds = FBoxSphereBounds(FBox(SourcePoint.BoundsMin - FVector(MaximumDistance), SourcePoint.BoundsMax + FVector(MaximumDistance))).TransformBy(SourcePoint.Transform);

				const FVector SourceCenter = SourcePoint.Transform.TransformPosition(SourcePoint.GetLocalCenter());

				double MinDistanceSquared = MaximumDistance*MaximumDistance;
				FVector MinDistanceVector = FVector::ZeroVector;

				auto CalculateSDF = [&MinDistanceSquared, &MinDistanceVector, &SourcePoint, SourceCenter, SourceShape, TargetShape](const FPCGPointRef& TargetPointRef)
				{
					const FPCGPoint& TargetPoint = *TargetPointRef.Point;

					const FVector& TargetCenter = TargetPointRef.Bounds.Origin;

					const FVector SourceShapePos = PCGDistance::CalcPosition(SourceShape, SourcePoint, TargetPoint, SourceCenter, TargetCenter);
					const FVector TargetShapePos = PCGDistance::CalcPosition(TargetShape, TargetPoint, SourcePoint, TargetCenter, SourceCenter);

					const FVector ToTargetShapeDir = TargetShapePos - SourceShapePos;
					const FVector ToTargetCenterDir = TargetCenter - SourceCenter;

					const double Sign = FMath::Sign(ToTargetShapeDir.Dot(ToTargetCenterDir));
					const double ThisDistanceSquared = ToTargetShapeDir.SquaredLength() * Sign;

					if (ThisDistanceSquared < MinDistanceSquared)
					{
						MinDistanceSquared = ThisDistanceSquared;
						MinDistanceVector = ToTargetShapeDir;
					}
				};

				for (const UPCGPointData* TargetPointData : TargetPointDatas)
				{
					const UPCGPointData::PointOctree& Octree = TargetPointData->GetOctree();

					Octree.FindElementsWithBoundsTest(
						FBoxCenterAndExtent(SourceQueryBounds.Origin, SourceQueryBounds.BoxExtent),
						CalculateSDF
					);
				}

				const float Distance = FMath::Sign(MinDistanceSquared) * FMath::Sqrt(FMath::Abs(MinDistanceSquared));

				if (ScalarAttribute || VectorAttribute)
				{
					OutputData->Metadata->InitializeOnSet(OutPoint.MetadataEntry);
				}

				if (ScalarAttribute)
				{
					ScalarAttribute->SetValue(OutPoint.MetadataEntry, Distance);
				}

				if (VectorAttribute)
				{
					VectorAttribute->SetValue(OutPoint.MetadataEntry, MinDistanceVector);
				}
				
				if (bSetDensity)
				{
					// set density instead
					OutPoint.Density = FMath::Clamp(Distance, -MaximumDistance, MaximumDistance) / MaximumDistance;
				}

				return true;
			}
		);
	}

	return true;
}

#undef LOCTEXT_NAMESPACE
