// Copyright Epic Games, Inc. All Rights Reserved.

#include "DMXPixelMappingRuntimeObjectVersion.h"

#include "Serialization/CustomVersion.h"


const FGuid FDMXPixelMappingRuntimeObjectVersion::GUID(0x75A7B683, 0x4F4C7E28, 0xDDE54885, 0x85323940);

// Register the custom version with core
FCustomVersionRegistration GRegisterDMXPixelMappingRuntimeCustomVersion(FDMXPixelMappingRuntimeObjectVersion::GUID, FDMXPixelMappingRuntimeObjectVersion::LatestVersion, TEXT("DMXRuntimeObjectVer"));
