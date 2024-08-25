// Copyright Epic Games, Inc. All Rights Reserved.

#include "InstancedActorsCustomVersion.h"

#include "Serialization/CustomVersion.h"

const FGuid FInstancedActorsCustomVersion::GUID(0xA6EBC74A, 0x3BAD46B8, 0xAA30D6B2, 0x7D58EA25);

// Register the custom version with core
FCustomVersionRegistration GRegisterInstancedActorsCustomVersion(FInstancedActorsCustomVersion::GUID, FInstancedActorsCustomVersion::LatestVersion, TEXT("InstancedActors"));
