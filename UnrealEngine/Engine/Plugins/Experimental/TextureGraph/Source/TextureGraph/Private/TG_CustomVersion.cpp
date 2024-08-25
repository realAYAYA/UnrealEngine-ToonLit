// Copyright Epic Games, Inc. All Rights Reserved.

#include "TG_CustomVersion.h"
#include "Serialization/CustomVersion.h"

//
// auto theOne = FGuid::NewGuid();
// UE_LOG(LogTextureGraph, Log, TEXT("%s"), *theOne.ToString(EGuidFormats::UniqueObjectGuid));

const FGuid FTG_CustomVersion::GUID(0x84F72F2E, 0x4181C255, 0x26721CB5, 0x37DFB222);

// Register the custom version with core
FCustomVersionRegistration GRegisterTG_CustomVersion(FTG_CustomVersion::GUID, FTG_CustomVersion::LatestVersion, TEXT("TG_BaseVer"));
