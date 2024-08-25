// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaSequenceVersion.h"
#include "Serialization/CustomVersion.h"

const FGuid FAvaSequenceVersion::GUID(0xD3B4C40C, 0xD15A4DA1, 0xADF25940, 0xB2C65167);

FCustomVersionRegistration GRegisterAvaSequenceVersion(FAvaSequenceVersion::GUID
	, FAvaSequenceVersion::LatestVersion
	, TEXT("AvalancheSequenceVersion"));
