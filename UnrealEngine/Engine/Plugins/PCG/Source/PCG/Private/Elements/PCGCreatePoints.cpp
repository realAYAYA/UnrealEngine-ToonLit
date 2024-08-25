// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGCreatePoints.h"

#include "PCGComponent.h"
#include "PCGContext.h"
#include "Helpers/PCGAsync.h"
#include "Helpers/PCGBlueprintHelpers.h"
#include "Helpers/PCGHelpers.h"
#include "Helpers/PCGSettingsHelpers.h"

#include "GameFramework/Actor.h"

#define LOCTEXT_NAMESPACE "PCGCreatePointsElement"

UPCGCreatePointsSettings::UPCGCreatePointsSettings()
{
	// Add one default point in the array
	PointsToCreate.Add(FPCGPoint());
}

void UPCGCreatePointsSettings::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITOR
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	if (GridPivot_DEPRECATED != EPCGLocalGridPivot::Global)
	{
		CoordinateSpace = static_cast<EPCGCoordinateSpace>(static_cast<uint8>(GridPivot_DEPRECATED));
		GridPivot_DEPRECATED = EPCGLocalGridPivot::Global;
	}
PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif
}

TArray<FPCGPinProperties> UPCGCreatePointsSettings::InputPinProperties() const
{
	return TArray<FPCGPinProperties>();
}

FPCGElementPtr UPCGCreatePointsSettings::CreateElement() const
{
	return MakeShared<FPCGCreatePointsElement>();
}

bool FPCGCreatePointsElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGCreatePointsElement::Execute);
	
	check(Context && Context->SourceComponent.Get());

	const UPCGCreatePointsSettings* Settings = Context->GetInputSettings<UPCGCreatePointsSettings>();
	check(Settings);

	// Used for culling, regardless of generation coordinate space
	const UPCGSpatialData* CullingShape = Settings->bCullPointsOutsideVolume ? Cast<UPCGSpatialData>(Context->SourceComponent->GetActorPCGData()) : nullptr;

	// Early out if the culling shape isn't valid
	if (!CullingShape && Settings->bCullPointsOutsideVolume)
	{
		PCGE_LOG(Error, GraphAndLog, LOCTEXT("CannotCullWithoutAShape", "Unable to cull since the supporting actor has no data."));
		return true;
	}

	FTransform LocalTransform = FTransform::Identity;

	if(Settings->CoordinateSpace == EPCGCoordinateSpace::OriginalComponent)
	{
		check(Context->SourceComponent->GetOriginalComponent() && Context->SourceComponent->GetOriginalComponent()->GetOwner());
		LocalTransform = Context->SourceComponent->GetOriginalComponent()->GetOwner()->GetActorTransform();
	}
	else if (Settings->CoordinateSpace == EPCGCoordinateSpace::LocalComponent)
	{
		check(Context->SourceComponent->GetOwner());
		LocalTransform = Context->SourceComponent->GetOwner()->GetActorTransform();
	}

	// Reset scale as we are not going to derive the points size from it
	LocalTransform.SetScale3D(FVector::One());

	const TArray<FPCGPoint>& PointsToLoopOn = Settings->PointsToCreate;
	
	TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;
	FPCGTaggedData& Output = Outputs.Emplace_GetRef();

	UPCGPointData* PointData = NewObject<UPCGPointData>();
	check(PointData); 

	TArray<FPCGPoint>& OutputPoints = PointData->GetMutablePoints();
	Output.Data = PointData;

	if (Settings->CoordinateSpace == EPCGCoordinateSpace::World)
	{
		if (CullingShape)
		{
			OutputPoints.Reserve(PointsToLoopOn.Num());

			for (const FPCGPoint& Point : PointsToLoopOn)
			{
				if (CullingShape->GetDensityAtPosition(Point.Transform.GetLocation()) > 0)
				{
					OutputPoints.Add(Point);
				}
			}
		}
		else
		{
			OutputPoints = PointsToLoopOn;
		}
		
		for (FPCGPoint& Point : OutputPoints)
		{
			if (Point.Seed == 0)
			{
				// If the seed is the default value, generate a new seed based on the its transform
				Point.Seed = UPCGBlueprintHelpers::ComputeSeedFromPosition(Point.Transform.GetLocation());
			}
		}
	}
	else
	{
		check(Settings->CoordinateSpace == EPCGCoordinateSpace::LocalComponent || Settings->CoordinateSpace == EPCGCoordinateSpace::OriginalComponent);

		FPCGAsync::AsyncPointProcessing(Context, PointsToLoopOn.Num(), OutputPoints, [&PointsToLoopOn, &LocalTransform, CullingShape](int32 Index, FPCGPoint& OutPoint)
		{
			const FPCGPoint& InPoint = PointsToLoopOn[Index];
			OutPoint = InPoint;
			OutPoint.Transform *= LocalTransform;

			const int SeedFromPosition = UPCGBlueprintHelpers::ComputeSeedFromPosition(OutPoint.Transform.GetLocation());
			OutPoint.Seed = (InPoint.Seed == 0 ? SeedFromPosition : PCGHelpers::ComputeSeed(InPoint.Seed, SeedFromPosition));

			// Discards all points that are outside the volume
			return !CullingShape || (CullingShape->GetDensityAtPosition(OutPoint.Transform.GetLocation()) > 0.0f);
		});
	}

	return true;
}

bool FPCGCreatePointsElement::IsCacheable(const UPCGSettings* InSettings) const
{
	const UPCGCreatePointsSettings* Settings = Cast<const UPCGCreatePointsSettings>(InSettings);

	return Settings && Settings->CoordinateSpace == EPCGCoordinateSpace::World;
}

#undef LOCTEXT_NAMESPACE
