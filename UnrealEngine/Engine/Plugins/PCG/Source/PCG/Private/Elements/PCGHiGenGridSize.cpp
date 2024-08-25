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
	return LOCTEXT("NodeTooltip", "Set the execution grid size for downstream nodes. Enables executing a single graph across a hierarchy of grids."
		"\n\nHas no effect if any of the following are true:"
		"\n\t* Generating PCG component is not set to Partitioned."
		"\n\t* Hierarchical Generation is disabled in the graph settings."
		"\n\t* Executed in a subgraph, as subgraphs are always invoked on parent grid level.");
}
#endif

EPCGDataType UPCGHiGenGridSizeSettings::GetCurrentPinTypes(const UPCGPin* InPin) const
{
	check(InPin);
	// Non-dynamically-typed pins
	if (!InPin->IsOutputPin() || InPin->Properties.Label == PCGHiGenGridSizeConstants::CellVolumeOutputLabel)
	{
		return InPin->Properties.AllowedTypes;
	}

	// Output pin narrows to union of inputs on first pin
	const EPCGDataType InputTypeUnion = GetTypeUnionOfIncidentEdges(PCGPinConstants::DefaultInputLabel);
	return InputTypeUnion != EPCGDataType::None ? InputTypeUnion : EPCGDataType::Any;
}

TArray<FPCGPinProperties> UPCGHiGenGridSizeSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	FPCGPinProperties& InputPinProperty = PinProperties.Emplace_GetRef(PCGPinConstants::DefaultInputLabel, EPCGDataType::Any);
	InputPinProperty.SetRequiredPin();

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

FString UPCGHiGenGridSizeSettings::GetAdditionalTitleInformation() const
{
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		return FString();
	}

	const UEnum* EnumPtr = StaticEnum<EPCGHiGenGrid>();
	if (ensure(EnumPtr))
	{
		return EnumPtr->GetDisplayNameTextByValue(static_cast<int64>(HiGenGridSize)).ToString();
	}
	else
	{
		return FString::Printf(TEXT("%d"), static_cast<int32>(HiGenGridSize));
	}
}

#if WITH_EDITOR
EPCGChangeType UPCGHiGenGridSizeSettings::GetChangeTypeForProperty(const FName& InPropertyName) const
{
	EPCGChangeType ChangeType = Super::GetChangeTypeForProperty(InPropertyName) | EPCGChangeType::Cosmetic;

	// Grid sizes are processed during graph compilation and is part of the graph structure.
	if (InPropertyName == GET_MEMBER_NAME_CHECKED(UPCGHiGenGridSizeSettings, bEnabled)
		|| InPropertyName == GET_MEMBER_NAME_CHECKED(UPCGHiGenGridSizeSettings, HiGenGridSize))
	{
		ChangeType |= EPCGChangeType::Structural | EPCGChangeType::GenerationGrid;
	}

	return ChangeType;
}
#endif

bool FPCGHiGenGridSizeElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGHiGenGridSizeElement::Execute);
	check(Context);

	const UPCGHiGenGridSizeSettings* Settings = Context->GetInputSettings<UPCGHiGenGridSizeSettings>();
	check(Settings);

	const UPCGGraph* Graph = Context->Node ? Context->Node->GetGraph() : nullptr;
	if (Graph && Graph->IsHierarchicalGenerationEnabled() && (Graph->GetDefaultGrid() < Settings->GetGrid()))
	{
		PCGE_LOG(Warning, GraphAndLog, LOCTEXT("GridSizeLargerThanGraphGridSize", "Grid size is larger than graph default grid size and will be automatically clamped."));
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
	IPCGElement::GetDependenciesCrc(InInput, InSettings, InComponent, Crc);

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
