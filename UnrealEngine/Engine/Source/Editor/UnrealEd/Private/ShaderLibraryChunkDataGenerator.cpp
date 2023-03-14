// Copyright Epic Games, Inc. All Rights Reserved.

#include "ShaderLibraryChunkDataGenerator.h"

#include "CookOnTheSide/CookOnTheFlyServer.h"
#include "Interfaces/ITargetPlatform.h"
#include "IPlatformFileSandboxWrapper.h"
#include "Misc/AssertionMacros.h"
#include "Misc/ConfigCacheIni.h"
#include "ShaderCodeLibrary.h"

class FName;

FShaderLibraryChunkDataGenerator::FShaderLibraryChunkDataGenerator(UCookOnTheFlyServer& InCOTFS, const ITargetPlatform* TargetPlatform)
	: COTFS(InCOTFS)
{
	// Find out if this platform requires stable shader keys, by reading the platform setting file.
	bOptedOut = false;
	PlatformNameUsedForIni = TargetPlatform->IniPlatformName();

	FConfigFile PlatformIniFile;
	FConfigCacheIni::LoadLocalIniFile(PlatformIniFile, TEXT("Engine"), true, *PlatformNameUsedForIni);
	PlatformIniFile.GetBool(TEXT("DevOptions.Shaders"), TEXT("bDoNotChunkShaderLib"), bOptedOut);
}

void FShaderLibraryChunkDataGenerator::GenerateChunkDataFiles(const int32 InChunkId, const TSet<FName>& InPackagesInChunk,
	const ITargetPlatform* TargetPlatform, FSandboxPlatformFile* InSandboxFile, TArray<FString>& OutChunkFilenames)
{
	if (!bOptedOut && InPackagesInChunk.Num() > 0)
	{
		checkf(PlatformNameUsedForIni == TargetPlatform->IniPlatformName(),
			TEXT("Mismatch between platform names in shaderlib chunk generator. Ini settings might have been applied incorrectly."));

		// get the sandbox content directory here, to relieve shaderlib from including the wrapper
		FString ShaderCodeDir;
		FString MetaDataPath;
		COTFS.GetShaderLibraryPaths(TargetPlatform, ShaderCodeDir, MetaDataPath, true /* bUseProjectDirForDLC */);

		bool bHasData;
		FShaderLibraryCooker::SaveShaderLibraryChunk(InChunkId, InPackagesInChunk, TargetPlatform, ShaderCodeDir,
			MetaDataPath, OutChunkFilenames, bHasData);
	}
}
	