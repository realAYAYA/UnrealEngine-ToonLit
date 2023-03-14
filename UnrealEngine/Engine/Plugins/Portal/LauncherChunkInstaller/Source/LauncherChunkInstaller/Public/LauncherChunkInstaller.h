// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GenericPlatform/GenericPlatformChunkInstall.h"

/**
* Launcher Implementation of the platform chunk install module
**/
class LAUNCHERCHUNKINSTALLER_API FLauncherChunkInstaller : public FGenericPlatformChunkInstall
{
public:
	virtual EChunkLocation::Type GetChunkLocation(uint32 ChunkID) override;
};