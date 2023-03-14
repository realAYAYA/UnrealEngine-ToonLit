// Copyright Epic Games, Inc. All Rights Reserved.

#include "Blueprint/DMXPixelMappingSubsystem.h"
#include "DMXPixelMapping.h"
#include "Components/DMXPixelMappingRendererComponent.h"
#include "Components/DMXPixelMappingOutputComponent.h"
#include "Components/DMXPixelMappingOutputDMXComponent.h"
#include "Components/DMXPixelMappingFixtureGroupComponent.h"
#include "Components/DMXPixelMappingMatrixComponent.h"

#include "Engine/Engine.h"

#define DMX_PIXEL_MAPPING_GET_SUBSYSTEM_COMPONENT(ComponentType)	\
UDMXPixelMapping##ComponentType* UDMXPixelMappingSubsystem::Get##ComponentType(UDMXPixelMapping* InDMXPixelMapping, const FName& InComponentName) \
{ \
if (InDMXPixelMapping != nullptr) \
{ \
return InDMXPixelMapping->FindComponentOfClass<UDMXPixelMapping##ComponentType>(InComponentName); \
} \
return nullptr; \
}

/*static*/ UDMXPixelMappingSubsystem* UDMXPixelMappingSubsystem::GetDMXPixelMappingSubsystem_Pure()
{
	return GEngine->GetEngineSubsystem<UDMXPixelMappingSubsystem>();
}

/*static*/ UDMXPixelMappingSubsystem* UDMXPixelMappingSubsystem::GetDMXPixelMappingSubsystem_Callable()
{
	return GetDMXPixelMappingSubsystem_Pure();
}

UDMXPixelMapping* UDMXPixelMappingSubsystem::GetDMXPixelMapping(UDMXPixelMapping* InPixelMapping)
{
	return InPixelMapping;
}

DMX_PIXEL_MAPPING_GET_SUBSYSTEM_COMPONENT(RendererComponent)
DMX_PIXEL_MAPPING_GET_SUBSYSTEM_COMPONENT(OutputDMXComponent)
DMX_PIXEL_MAPPING_GET_SUBSYSTEM_COMPONENT(FixtureGroupComponent)
DMX_PIXEL_MAPPING_GET_SUBSYSTEM_COMPONENT(MatrixComponent)
