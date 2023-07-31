// Copyright Epic Games, Inc. All Rights Reserved.

#include "ClothConfigNvCustomVersion.h"
#include "Serialization/CustomVersion.h"

const FGuid FClothConfigNvCustomVersion::GUID(0x9950B70E, 0xB41A4E17, 0xBBCCFA0D, 0x57817FD6);

// Register the custom version with core
FCustomVersionRegistration GRegisterClothConfigNvCustomVersion(FClothConfigNvCustomVersion::GUID, FClothConfigNvCustomVersion::LatestVersion, TEXT("ClothConfigNvVer"));
