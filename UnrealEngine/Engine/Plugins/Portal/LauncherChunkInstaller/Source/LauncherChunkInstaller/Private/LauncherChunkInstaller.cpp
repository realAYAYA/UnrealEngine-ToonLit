// Copyright Epic Games, Inc. All Rights Reserved.

#include "LauncherChunkInstaller.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "IPlatformFilePak.h"
#include "Misc/CommandLine.h"
#include "Modules/ModuleManager.h"

/**
 * Module for the Launcher Chunk Installer
 */
class FLauncherChunkInstallerModule : public IPlatformChunkInstallModule
{
public:
	TUniquePtr<IPlatformChunkInstall> ChunkInstaller;

	FLauncherChunkInstallerModule()
		: ChunkInstaller(new FLauncherChunkInstaller())
	{

	}

	virtual IPlatformChunkInstall* GetPlatformChunkInstall()
	{
		return ChunkInstaller.Get();
	}
};

IMPLEMENT_MODULE(FLauncherChunkInstallerModule, LauncherChunkInstaller);

EChunkLocation::Type FLauncherChunkInstaller::GetChunkLocation(uint32 ChunkID)
{
	EChunkLocation::Type Result = EChunkLocation::LocalFast;

	// get the platform pak file management API.
	FPakPlatformFile* PakPlatformFile = (FPakPlatformFile*)(FPlatformFileManager::Get().FindPlatformFile(FPakPlatformFile::GetTypeName()));
	if (PakPlatformFile && PakPlatformFile->AnyChunksAvailable())
	{
		Result = PakPlatformFile->GetPakChunkLocation(ChunkID);
	}

	return Result;

#if 0
	// Removed this code for Fortnite to allow encrypted chunks to work. Need to understand
	// why this was doing what it was doing to know whether this logic needs to change

	// This is a fall back for builds to fail on shipping or testing.
	// This code will also force available to trigger when we're running editor
#if UE_BUILD_SHIPPING || UE_BUILD_TEST
	return EChunkLocation::NotAvailable;
#else
	return EChunkLocation::LocalFast;
#endif
#endif
}
