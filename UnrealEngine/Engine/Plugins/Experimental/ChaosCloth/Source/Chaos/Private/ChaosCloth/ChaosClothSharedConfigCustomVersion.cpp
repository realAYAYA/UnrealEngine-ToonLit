// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothSharedConfigCustomVersion.h"
#include "Serialization/CustomVersion.h"

const FGuid FChaosClothSharedConfigCustomVersion::GUID(0x5FBC6907, 0x55C840AE, 0x8E67F184, 0x5EFFF13F);

// Register the custom version with core
FCustomVersionRegistration GRegisterChaosClothSharedConfigCustomVersion(FChaosClothSharedConfigCustomVersion::GUID, FChaosClothSharedConfigCustomVersion::LatestVersion, TEXT("ChaosClothSharedConfigVer"));
