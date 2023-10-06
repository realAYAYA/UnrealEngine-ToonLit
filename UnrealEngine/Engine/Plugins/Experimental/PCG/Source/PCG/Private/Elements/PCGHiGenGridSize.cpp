// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGHiGenGridSize.h"

#include "PCGComponent.h"
#include "PCGContext.h"
#include "PCGGraph.h"
#include "PCGModule.h"
#include "PCGPin.h"
#include "Data/PCGVolumeData.h"

#define LOCTEXT_NAMESPACE "PCGHiGenGridSizeElement"

namespace PCGHiGenGridSizeConstants
{
	const FName NodeName = TEXT("HiGenGridSize");
	const FText NodeTitle = LOCTEXT("NodeTitle", "Grid Size");
	const FName CellVolumeOutputLabel = TEXT("Grid Cell Volume");
}

#if WITH_EDITOR
FName UPCGHiGenGridSizeSettings::GetDefaultNodeName() const
{
	return PCGHiGenGridSizeConstants::NodeName;
}

FText UPCGHiGenGridSizeSettings::GetDefaultNodeTitle() const
{
	return PCGHiGenGridSizeConstants::NodeTitle;
}

FText UPCGHiGenGridSizeSettings::GetNodeTooltipText() const
{
	return LOCTEXT("NodeTooltip", "Set the execution grid size for downstream nodes. Enables executing a single graph across a hierarchy of grids. Has no effect if generating component is not partitioned.");
}
#endif

EPCGDataType UPCGHiGenGridSizeSettings::GetCurrentPinTypes(const UPCGPin* InPin) const
{
	check(InPin);
	// Non-dynamically-typed pins
	if (!InPin->IsOutputPin() || InPin->Properties.Label == PCGHiGenGridSizeConstants::CellVolumeOutputLabel)
	{
		return Super::GetCurrentPinTypes(InPin);
	}

	// Output pin narrows to union of inputs on first pin
	const EPCGDataType InputTypeUnion = GetTypeUnionOfIncidentEdges(PCGPinConstants::DefaultInputLabel);
	return InputTypeUnion != EPCGDataType::None ? InputTypeUnion : EPCGDataType::Any;
}

TArray<FPCGPinProperties> UPCGHiGenGridSizeSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PinProperties.Emplace(PCGPinConstants::DefaultInputLabel, EPCGDataType::Any);

	return PinProperties;
}

TArray<FPCGPinProperties> UPCGHiGenGridSizeSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PinProperties.Emplace(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Any);
	PinProperties.Emplace(
		PCGHiGenGridSizeConstants::CellVolumeOutputLabel,
		EPCGDataType::Spatial,
		/*bAllowMultipleConnections=*/true,
		/*bAllowMultipleData=*/true,
		LOCTEXT("VolumeOutputPinTooltip", "The volume of the current grid cell."));

	return PinProperties;
}

FPCGElementPtr UPCGHiGenGridSizeSettings::CreateElement() const
{
	return MakeShared<FPCGHiGenGridSizeElement>();
}

FName UPCGHiGenGridSizeSettings::AdditionalTaskName() const
{
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		return PCGHiGenGridSizeConstants::NodeName;
	}

	const UEnum* EnumPtr = StaticEnum<EPCGHiGenGrid>();
	if (ensure(EnumPtr))
	{
		FText GridSizeDisplayName = EnumPtr->GetDisplayNameTextByValue(static_cast<int64>(HiGenGridSize));
		return FName(FString::Printf(TEXT("%s: %s"), *PCGHiGenGridSizeConstants::NodeTitle.ToString(), *GridSizeDisplayName.ToString()));
	}
	else
	{
		return FName(FString::Printf(TEXT("%s: %d"), *PCGHiGenGridSizeConstants::NodeTitle.ToString(), static_cast<int32>(HiGenGridSize)));
	}
}

#if WITH_EDITOR
bool UPCGHiGenGridSizeSettings::IsStructuralProperty(const FName& InPropertyName) const
{
	// Grid sizes are processed during graph compilation and is part of the graph structure.
	return InPropertyName == GET_MEMBER_NAME_CHECKED(UPCGHiGenGridSizeSettings, bEnabled)
		|| InPropertyName == GET_MEMBER_NAME_CHECKED(UPCGHiGenGridSizeSettings, HiGenGridSize);
}
#endif

bool FPCGHiGenGridSizeElement::ExecuteInternal(FPCGContext* Context) const
{
	// Validation
	check(Context);
	if (Context->Node && Context->Node->GetGraph() && !Context->Node->GetGraph()->IsHierarchicalGenerationEnabled())
	{
		PCGE_LOG(Warning, GraphAndLog, LOCTEXT("GridSizeUsedInNonHiGenGraph", "Grid Size node used in a non-hierarchical graph. Enable hierarchical generation in the graph settings or remove this node."));
	}
	else if (!Context->SourceComponent->IsPartitioned() && !Context->SourceComponent->IsLocalComponent())
	{
		// Warning if component is not partitioned (and not local component) as this node will otherwise be silently ignored.
		// Also serves as a hint if the user forgot to enable higen for this graph.
		PCGE_LOG(Warning, GraphAndLog, LOCTEXT("NonPartitionedComponent", "Grid Size node used on a non-partitioned component and will have no effect. Is Partitioned must be enabled on the component."));
	}

	// Trivial pass through. Will only execute on the prescribed grid.
	Context->OutputData = Context->InputData;
	for (FPCGTaggedData& Data : Context->OutputData.TaggedData)
	{
		Data.Pin = PCGPinConstants::DefaultOutputLabel;
	}

	UPCGVolumeData* VolumeData = NewObject<UPCGVolumeData>();
	check(VolumeData);
	VolumeData->Initialize(Context->SourceComponent->GetGridBounds());

	FPCGTaggedData& VolumeOutput = Context->OutputData.TaggedData.Emplace_GetRef();
	VolumeOutput.Data = VolumeData;
	VolumeOutput.Pin = PCGHiGenGridSizeConstants::CellVolumeOutputLabel;

	return true;
}

void FPCGHiGenGridSizeElement::GetDependenciesCrc(const FPCGDataCollection& InInput, const UPCGSettings* InSettings, UPCGComponent* InComponent, FPCGCrc& OutCrc) const
{
	FPCGCrc Crc;
	FSimplePCGElement::GetDependenciesCrc(InInput, InSettings, InComponent, Crc);

	// The grid cell volume output depends on the component transform.
	// NOTE: It might be interesting to only incorporate the transform if the cell volume output pin is connected (and if we have a node obviously).
	const UPCGData* ActorData = InComponent ? InComponent->GetActorPCGData() : nullptr;
	if (ActorData)
	{
		Crc.Combine(ActorData->GetOrComputeCrc(/*bFullDataCrc=*/false));
	}

	OutCrc = Crc;
}

#undef LOCTEXT_NAMESPACE
