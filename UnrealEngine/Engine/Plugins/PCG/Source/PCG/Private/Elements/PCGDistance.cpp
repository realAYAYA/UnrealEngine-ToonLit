// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGDistance.h"

#include "PCGContext.h"
#include "PCGPin.h"
#include "Data/PCGSpatialData.h"
#include "Data/PCGPointData.h"
#include "Elements/PCGGather.h"
#include "Helpers/PCGAsync.h"
#include "Metadata/Accessors/PCGAttributeAccessorHelpers.h"

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

#if WITH_EDITOR
FText UPCGDistanceSettings::GetNodeTooltipText() const
{
	return LOCTEXT("PCGDistanceTooltip", "Calculates and appends a signed 'Distance' attribute to the source data. For each of the source points, a distance attribute will be calculated between it and the nearest target point.");
}
#endif // WITH_EDITOR

TArray<FPCGPinProperties> UPCGDistanceSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	FPCGPinProperties& PinPropertySource = PinProperties.Emplace_GetRef(PCGDistance::SourceLabel, EPCGDataType::Point);
	PinPropertySource.SetRequiredPin();
	FPCGPinProperties& PinPropertyTarget = PinProperties.Emplace_GetRef(PCGDistance::TargetLabel, EPCGDataType::Point);

#if WITH_EDITOR
	PinPropertySource.Tooltip = LOCTEXT("PCGSourcePinTooltip", "For each of the source points, a distance attribute will be calculated between it and the nearest target point.");
	PinPropertyTarget.Tooltip = LOCTEXT("PCGTargetPinTooltip", "The target points to conduct a distance check with each source point.");
#endif // WITH_EDITOR

	return PinProperties;
}

TArray<FPCGPinProperties> UPCGDistanceSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	FPCGPinProperties& PinPropertyOutput = PinProperties.Emplace_GetRef(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Point);

#if WITH_EDITOR
	PinPropertyOutput.Tooltip = LOCTEXT("PCGOutputPinTooltip", "The source points will be output with the newly added 'Distance' attribute as well as have their density set to [0,1] based on the 'Maximum Distance' if 'Set Density' is enabled.");
#endif // WITH_EDITOR
	
	return PinProperties;
}

FPCGElementPtr UPCGDistanceSettings::CreateElement() const
{
	return MakeShared<FPCGDistanceElement>();
}

void UPCGDistanceSettings::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITOR
	if (AttributeName_DEPRECATED != PCGDistanceConstants::DefaultOutputAttributeName)
	{
		// "None" was previously used to indicate that nothing should be written to attribute
		if (AttributeName_DEPRECATED == NAME_None)
		{
			bOutputToAttribute = false;
			OutputAttribute.SetAttributeName(PCGDistanceConstants::DefaultOutputAttributeName);
		}
		else
		{
			bOutputToAttribute = true;
			OutputAttribute.SetAttributeName(AttributeName_DEPRECATED);
		}

		AttributeName_DEPRECATED = PCGDistanceConstants::DefaultOutputAttributeName;
	}
#endif // WITH_EDITOR
}

bool FPCGDistanceElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGDistanceElement::Execute);
	check(Context);

	if (Context->Node && !Context->Node->IsInputPinConnected(PCGDistance::TargetLabel))
	{
		// If Target pin is unconnected then we no-op and pass through all data from Target pin.
		Context->OutputData = PCGGather::GatherDataForPin(Context->InputData, PCGDistance::SourceLabel);
		return true;
	}

	const UPCGDistanceSettings* Settings = Context->GetInputSettings<UPCGDistanceSettings>();
	check(Settings);

	const bool bSetDensity = Settings->bSetDensity;
	const bool bOutputDistanceVector = Settings->bOutputDistanceVector;
	const PCGDistanceShape SourceShape = Settings->SourceShape;
	const PCGDistanceShape TargetShape = Settings->TargetShape;

	const double MaximumDistance = FMath::Max(0.0, Settings->MaximumDistance);
	const double MaximumDistanceRecip = MaximumDistance > UE_DOUBLE_SMALL_NUMBER ? 1.0 / MaximumDistance : 0.0;

	TArray<FPCGTaggedData> Sources = Context->InputData.GetInputsByPin(PCGDistance::SourceLabel);
	TArray<FPCGTaggedData> Targets = Context->InputData.GetInputsByPin(PCGDistance::TargetLabel);
	TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;

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

	// First find the total Input bounds which will determine the size of each cell
	for (const FPCGTaggedData& Source : Sources) 
	{
		// Add the point bounds to the input cell

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

		if (Settings->bOutputToAttribute && Settings->OutputAttribute.IsBasicAttribute())
		{
			if (bOutputDistanceVector)
			{
				OutputData->Metadata->FindOrCreateAttribute<FVector>(Settings->OutputAttribute.GetAttributeName());
			}
			else
			{
				OutputData->Metadata->FindOrCreateAttribute<double>(Settings->OutputAttribute.GetAttributeName());
			}
		}

		TUniquePtr<IPCGAttributeAccessor> Accessor;
		TUniquePtr<IPCGAttributeAccessorKeys> Keys;

		if (Settings->bOutputToAttribute)
		{
			Accessor = PCGAttributeAccessorHelpers::CreateAccessor(OutputData, Settings->OutputAttribute);
		}

		// If the selected attribute is a property or extra property and not the correct type, invalidate the accessor
		if (Accessor.IsValid())
		{
			using PCG::Private::MetadataTypes;

			if ((bOutputDistanceVector && !IsBroadcastableOrConstructible(Accessor->GetUnderlyingType(), MetadataTypes<FVector>::Id)) ||
				(!bOutputDistanceVector && !IsBroadcastableOrConstructible(Accessor->GetUnderlyingType(), MetadataTypes<double>::Id)))
			{
				PCGE_LOG(Warning, GraphAndLog, LOCTEXT("InvalidAccessorType", "Selected type for Output Attribute is incompatible with distance as output."));
				Accessor = nullptr;
			}
		}

		TArray<FPCGPoint>& OutPoints = OutputData->GetMutablePoints();
		const TArray<FPCGPoint>& SourcePoints = SourcePointData->GetPoints();
		OutPoints.SetNumUninitialized(SourcePoints.Num());

		if (Accessor.IsValid())
		{
			Keys = PCGAttributeAccessorHelpers::CreateKeys(OutputData, Settings->OutputAttribute);

			if (!Keys.IsValid())
			{
				PCGE_LOG(Warning, GraphAndLog, LOCTEXT("CannotCreateAccessorKeys", "Cannot create accessor keys on output points."));
				Accessor = nullptr;
			}
		}

		struct TemporaryResultCache
		{
			TArray<double> Distances;
			TArray<FVector> DistanceVectors;
		} ResultCache;

		// Set up a cache so we can set all the attributes in a single range set
		if (Accessor.IsValid())
		{
			if (bOutputDistanceVector)
			{
				ResultCache.DistanceVectors.SetNumUninitialized(SourcePoints.Num());
			}
			else
			{
				ResultCache.Distances.SetNumUninitialized(SourcePoints.Num());
			}
		}

		auto ProcessDistance = [SourceShape, TargetShape, &TargetPointDatas, MaximumDistance, MaximumDistanceRecip, bSetDensity, bOutputDistanceVector, &OutPoints, &SourcePoints, &ResultCache, bWriteToAttribute = Accessor.IsValid()](int32 ReadIndex, int32 WriteIndex)
		{
			FPCGPoint& OutPoint = OutPoints[WriteIndex];
			const FPCGPoint& SourcePoint = SourcePoints[ReadIndex];

			OutPoint = SourcePoint;

			const FBoxSphereBounds SourceQueryBounds = FBoxSphereBounds(FBox(SourcePoint.BoundsMin - FVector(MaximumDistance), SourcePoint.BoundsMax + FVector(MaximumDistance))).TransformBy(SourcePoint.Transform);

			const FVector SourceCenter = SourcePoint.Transform.TransformPosition(SourcePoint.GetLocalCenter());

			double MinDistanceSquared = MaximumDistance * MaximumDistance;
			FVector MinDistanceVector = FVector::ZeroVector;

			// Signed distance field for calculating the closest point of source and target
			auto CalculateSDF = [&MinDistanceSquared, &MinDistanceVector, &SourcePoint, SourceCenter, SourceShape, TargetShape](const FPCGPointRef& TargetPointRef)
			{
				// If the source pointer and target pointer are the same, ignore distance to the exact same point
				if (&SourcePoint == TargetPointRef.Point)
				{
					return;
				}

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
						CalculateSDF);
			}

			const double Distance = FMath::Sign(MinDistanceSquared) * FMath::Sqrt(FMath::Abs(MinDistanceSquared));

			if (bWriteToAttribute)
			{
				if (bOutputDistanceVector)
				{
					ResultCache.DistanceVectors[WriteIndex] = MinDistanceVector;
				}
				else
				{
					ResultCache.Distances[WriteIndex] = Distance;
				}
			}

			if (bSetDensity)
			{
				OutPoint.Density = MaximumDistance > UE_DOUBLE_SMALL_NUMBER ? (FMath::Clamp(Distance, -MaximumDistance, MaximumDistance) * MaximumDistanceRecip) : 1.0f;
			}

			return true;
		};

		if (FPCGAsync::AsyncProcessingOneToOneEx(&Context->AsyncState, SourcePoints.Num(), /*InitializeFunc=*/[]{}, ProcessDistance, /*bEnableTimeSlicing=*/false))
		{
			if (Accessor.IsValid())
			{
				// Set all the attributes at once
				if (bOutputDistanceVector)
				{
					Accessor->SetRange<FVector>(ResultCache.DistanceVectors, 0, *Keys);
				}
				else
				{
					Accessor->SetRange<double>(ResultCache.Distances, 0, *Keys);
				}
			}
		}
	}

	return true;
}

#undef LOCTEXT_NAMESPACE
