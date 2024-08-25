// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGCullPointsOutsideActorBounds.h"

#include "PCGComponent.h"
#include "PCGContext.h"
#include "PCGPin.h"
#include "Data/PCGIntersectionData.h"
#include "Data/PCGPointData.h"
#include "Data/PCGVolumeData.h"

#define LOCTEXT_NAMESPACE "PCGCullPointsOutsideActorBoundsElement"

#if WITH_EDITOR
FText UPCGCullPointsOutsideActorBoundsSettings::GetDefaultNodeTitle() const
{
	return LOCTEXT("NodeTitle", "Cull Points Outside Actor Bounds");
}

FText UPCGCullPointsOutsideActorBoundsSettings::GetNodeTooltipText() const
{
	return LOCTEXT("NodeTooltip", "Culls points that lie outside the current actor bounds.");
}
#endif

TArray<FPCGPinProperties> UPCGCullPointsOutsideActorBoundsSettings::InputPinProperties() const
{
	return Super::DefaultPointInputPinProperties();
}

TArray<FPCGPinProperties> UPCGCullPointsOutsideActorBoundsSettings::OutputPinProperties() const
{
	return Super::DefaultPointOutputPinProperties();
}

FPCGElementPtr UPCGCullPointsOutsideActorBoundsSettings::CreateElement() const
{
	return MakeShared<FPCGCullPointsOutsideActorBoundsElement>();
}

bool FPCGCullPointsOutsideActorBoundsElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGCullPointsOutsideActorBoundsElement::Execute);

	if (!Context->SourceComponent.IsValid())
	{
		return true;
	}

	UPCGVolumeData* VolumeData = NewObject<UPCGVolumeData>();
	check(VolumeData);

	const UPCGCullPointsOutsideActorBoundsSettings* Settings = Context->GetInputSettings<UPCGCullPointsOutsideActorBoundsSettings>();
	check(Settings);

	// Initialize directly. Could also have gone through PCGComponent::CreateActorPCGDataCollection.
	VolumeData->Initialize(Context->SourceComponent->GetGridBounds().ExpandBy(Settings->BoundsExpansion));

	const TArray<FPCGTaggedData>& Inputs = Context->InputData.TaggedData;
	for (const FPCGTaggedData& Input : Inputs)
	{
		const UPCGPointData* InputPointData = Cast<UPCGPointData>(Input.Data);

		// Skip non-points or empty point data
		if (!InputPointData || InputPointData->GetPoints().IsEmpty())
		{
			continue;
		}

		UPCGIntersectionData* Intersection = InputPointData->IntersectWith(VolumeData);
		check(Intersection);

		const UPCGPointData* IntersectedPoints = Intersection->ToPointData(Context);
		check(IntersectedPoints);

		// Skip empty result
		if (IntersectedPoints->GetPoints().IsEmpty())
		{
			continue;
		}

		FPCGTaggedData& Output = Context->OutputData.TaggedData.Emplace_GetRef(Input);
		Output.Data = IntersectedPoints;
	}

	return true;
}

void FPCGCullPointsOutsideActorBoundsElement::GetDependenciesCrc(const FPCGDataCollection& InInput, const UPCGSettings* InSettings, UPCGComponent* InComponent, FPCGCrc& OutCrc) const
{
	FPCGCrc Crc;
	IPCGElement::GetDependenciesCrc(InInput, InSettings, InComponent, Crc);

	// The culling volume depends on the component transform.
	const UPCGData* ActorData = InComponent ? InComponent->GetActorPCGData() : nullptr;
	if (ActorData)
	{
		Crc.Combine(ActorData->GetOrComputeCrc(/*bFullDataCrc=*/false));
	}

	OutCrc = Crc;
}

#undef LOCTEXT_NAMESPACE
