// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGTypedGetter.h"

#include "Landscape.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGTypedGetter)

#define LOCTEXT_NAMESPACE "PCGTypedGetterElements"

UPCGGetLandscapeSettings::UPCGGetLandscapeSettings()
{
	bDisplayModeSettings = false;
	Mode = EPCGGetDataFromActorMode::ParseActorComponents;

	// We want to apply different defaults to newly placed nodes. We detect new object if they are not a default object/archetype
	// and/or they do not need load. Followed similar pattern to UPCGComponent::PostInitProperties().
	if (!HasAnyFlags(RF_ClassDefaultObject | RF_NeedLoad | RF_NeedPostLoad))
	{
		// This setup replicates what was implemented on the Landscape input node pin
		ActorSelector.ActorFilter = EPCGActorFilter::AllWorldActors;
		ActorSelector.bMustOverlapSelf = true;
		ActorSelector.bSelectMultiple = true;
		ActorSelector.ActorSelection = EPCGActorSelection::ByClass;
		ActorSelector.ActorSelectionClass = ALandscapeProxy::StaticClass();
	}
}

TArray<FPCGPinProperties> UPCGGetLandscapeSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PinProperties.Emplace(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Landscape);

	return PinProperties;
}

#if WITH_EDITOR
FText UPCGGetLandscapeSettings::GetNodeTooltipText() const
{
	return LOCTEXT("GetLandscapeTooltip", "Builds a collection of landscapes from the selected actors.");
}
#endif

UPCGGetSplineSettings::UPCGGetSplineSettings()
{
	bDisplayModeSettings = false;
	Mode = EPCGGetDataFromActorMode::ParseActorComponents;
}

TArray<FPCGPinProperties> UPCGGetSplineSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PinProperties.Emplace(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Spline);

	return PinProperties;
}

#if WITH_EDITOR
FText UPCGGetSplineSettings::GetNodeTooltipText() const
{
	return LOCTEXT("GetSplineTooltip", "Builds a collection of splines from the selected actors.");
}
#endif

UPCGGetVolumeSettings::UPCGGetVolumeSettings()
{
	bDisplayModeSettings = false;
	Mode = EPCGGetDataFromActorMode::ParseActorComponents;
}

TArray<FPCGPinProperties> UPCGGetVolumeSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PinProperties.Emplace(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Volume);

	return PinProperties;
}

#if WITH_EDITOR
FText UPCGGetVolumeSettings::GetNodeTooltipText() const
{
	return LOCTEXT("GetVolumeTooltip", "Builds a collection of volumes from the selected actors.");
}
#endif

UPCGGetPrimitiveSettings::UPCGGetPrimitiveSettings()
{
	bDisplayModeSettings = false;
	Mode = EPCGGetDataFromActorMode::ParseActorComponents;
}

TArray<FPCGPinProperties> UPCGGetPrimitiveSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PinProperties.Emplace(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Primitive);

	return PinProperties;
}

#if WITH_EDITOR
FText UPCGGetPrimitiveSettings::GetNodeTooltipText() const
{
	return LOCTEXT("GetPrimitiveTooltip", "Builds a collection of primitive data from primitive components on the selected actors.");
}
#endif

#undef LOCTEXT_NAMESPACE
