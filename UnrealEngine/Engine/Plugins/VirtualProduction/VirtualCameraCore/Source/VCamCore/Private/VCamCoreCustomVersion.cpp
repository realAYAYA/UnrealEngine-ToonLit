// Copyright Epic Games, Inc. All Rights Reserved.

#include "VCamCoreCustomVersion.h"
#include "Serialization/CustomVersion.h"

const FGuid UE::VCamCore::FVCamCoreCustomVersion::GUID(0x31234DBD, 0x21CAF31F, 0xAAA7A38F, 0xB4A32531);
FCustomVersionRegistration GRegisterVcamCoreCustomVersion(UE::VCamCore::FVCamCoreCustomVersion::GUID, UE::VCamCore::FVCamCoreCustomVersion::LatestVersion, TEXT("VCamCoreCustomVersion"));
