// Copyright Epic Games, Inc. All Rights Reserved.

#include "RemoteControlProtocolDMXObjectVersion.h"

#include "Serialization/CustomVersion.h"


const FGuid FRemoteControlProtocolDMXObjectVersion::GUID(0x077636E9, 0x44119FD9, 0x26477F9D, 0x0A7E9F6E);

// Register the custom version with core
FCustomVersionRegistration GRegisterRemoteControlProtocolDMXCustomVersion(FRemoteControlProtocolDMXObjectVersion::GUID, FRemoteControlProtocolDMXObjectVersion::LatestVersion, TEXT("RemoteControlProtocolDMXObjectVersion"));
