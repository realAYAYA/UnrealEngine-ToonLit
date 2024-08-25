// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tests/GraphAuthoring/PCGGraphAuthoringTestHelperSettings.h"

UPCGGraphAuthoringTestHelperSettings::UPCGGraphAuthoringTestHelperSettings()
{
#if WITH_EDITORONLY_DATA
	bExposeToLibrary = false;
#endif
}

TArray<FPCGPinProperties> UPCGGraphAuthoringTestHelperSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PinProperties.Emplace_GetRef(PCGPinConstants::DefaultInputLabel, PinType);

	return PinProperties;
}

TArray<FPCGPinProperties> UPCGGraphAuthoringTestHelperSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PinProperties.Emplace_GetRef(PCGPinConstants::DefaultOutputLabel, PinType);

	return PinProperties;
}
