// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "Containers/Set.h"

struct IVirtualFileCache;
namespace BuildPatchServices
{
	class IBuildManifestSet;
	class IChunkSource;
	class IChunkReferenceTracker;
	class IInstallerError;
	class IFileConstructorStat;

	struct FVirtualFileConstructorConfiguration
	{
	public:
		TSet<FString> FilesToConstruct;
	};

	struct FVirtualFileConstructorDependencies
	{
	public:
		IBuildManifestSet* ManifestSet;
		IVirtualFileCache* VirtualFileCache;
		IChunkSource* ChunkSource;
		IChunkReferenceTracker* ChunkReferenceTracker;
		IInstallerError* InstallerError;
		IFileConstructorStat* FileConstructorStat;
	};

	class FVirtualFileConstructor
	{
	public:
		FVirtualFileConstructor(FVirtualFileConstructorConfiguration Configuration, FVirtualFileConstructorDependencies Dependencies);
		~FVirtualFileConstructor();

	public:
		bool Run();

	private:

	private:
		const FVirtualFileConstructorConfiguration Configuration;
		const FVirtualFileConstructorDependencies Dependencies;
	};
	
	class FVirtualFileConstructorFactory
	{
	public:
		static FVirtualFileConstructor* Create(FVirtualFileConstructorConfiguration Configuration, FVirtualFileConstructorDependencies Dependencies);
	};
}