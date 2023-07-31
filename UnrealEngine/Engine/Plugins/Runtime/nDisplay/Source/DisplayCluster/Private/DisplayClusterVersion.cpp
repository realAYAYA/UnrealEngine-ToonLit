// Copyright Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterVersion.h"
#include "Serialization/CustomVersion.h"


const FGuid FDisplayClusterCustomVersion::GUID(0x06C4F6DC, 0xF518B974, 0x87B0C096, 0xE3855B05);
FCustomVersionRegistration GRegisterDisplayClusterCustomVersion(FDisplayClusterCustomVersion::GUID, FDisplayClusterCustomVersion::LatestVersion, TEXT("nDisplayVer"));
