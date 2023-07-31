// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Misc/SecureHash.h"
#include "Misc/EnumClassFlags.h"
#include "Misc/EnumRange.h"
#include "Serialization/Archive.h"
#include "Data/ChunkData.h"
#include "BuildPatchFeatureLevel.h"

class FBuildPatchAppManifest;

namespace BuildPatchServices
{
	/**
	 * A flags enum for manifest headers which specify storage types.
	 */
	enum class EManifestStorageFlags : uint8
	{
		// Stored as raw data.
		None       = 0,
		// Flag for compressed data.
		Compressed = 1,
		// Flag for encrypted. If also compressed, decrypt first. Encryption will ruin compressibility.
		Encrypted  = 1 << 1,
	};
	ENUM_CLASS_FLAGS(EManifestStorageFlags);

	/**
	 * Helpers for switching logic based on manifest feature version.
	 */
	namespace ManifestVersionHelpers
	{
		/**
		 * Get the chunk subdirectory for used for a specific manifest version, e.g. Chunks, ChunksV2 etc.
		 * @param FeatureLevel   The version of the manifest.
		 * @return the subdirectory name that this manifest version will access.
		 */
		const TCHAR* GetChunkSubdir(EFeatureLevel FeatureLevel);

		/**
		 * Get the file data subdirectory for used for a specific manifest version, e.g. Files, FilesV2 etc.
		 * @param FeatureLevel   The version of the manifest.
		 * @return the subdirectory name that this manifest version will access.
		 */
		const TCHAR* GetFileSubdir(EFeatureLevel FeatureLevel);
	}

	struct FManifestHeader
	{
		FManifestHeader();
		/**
		 * Serialization operator.
		 * @param Ar        Archive to serialize to.
		 * @param Header    FManifestHeader to serialize.
		 * @return Passed in archive.
		 */
		friend FArchive& operator<<(FArchive& Ar, FManifestHeader& Header);
		// The version of this header and manifest data format, driven by the feature level.
		EFeatureLevel Version;
		// The size of this header.
		uint32 HeaderSize;
		// The size of this data compressed.
		uint32 DataSizeCompressed;
		// The size of this data uncompressed.
		uint32 DataSizeUncompressed;
		// How the chunk data is stored.
		EManifestStorageFlags StoredAs;
		// The SHA1 hash for the manifest data that follows.
		FSHAHash SHAHash;
	};

	struct FManifestMeta
	{
		FManifestMeta();
		/**
		 * Serialization operator.
		 * @param Ar        Archive to serialize to.
		 * @param Meta      FManifestMeta to serialize.
		 * @return Passed in archive.
		 */
		friend FArchive& operator<<(FArchive& Ar, FManifestMeta& Meta);
		// The feature level support this build was created with, regardless of the serialised format.
		EFeatureLevel FeatureLevel;
		// Whether this is a legacy 'nochunks' build.
		bool bIsFileData;
		// The app id provided at generation.
		uint32 AppID;
		// The app name string provided at generation.
		FString AppName;
		// The build version string provided at generation.
		FString BuildVersion;
		// The file in this manifest designated the application executable of the build.
		FString LaunchExe;
		// The command line required when launching the application executable.
		FString LaunchCommand;
		// The set of prerequisite ids for dependencies that this build's prerequisite installer will apply.
		TSet<FString> PrereqIds;
		// A display string for the prerequisite provided at generation.
		FString PrereqName;
		// The file in this manifest designated the launch executable of the prerequisite installer.
		FString PrereqPath;
		// The command line required when launching the prerequisite installer.
		FString PrereqArgs;
		// A unique build id generated at original chunking time to identify an exact build.
		FString BuildId;
	};

	struct FChunkDataList
	{
		FChunkDataList();
		/**
		 * Serialization operator.
		 * @param Ar                Archive to serialize to.
		 * @param ChunkDataList     FChunkDataList to serialize.
		 * @return Passed in archive.
		 */
		friend FArchive& operator<<(FArchive& Ar, FChunkDataList& ChunkDataList);
		// The list of chunks.
		TArray<FChunkInfo> ChunkList;
	};

	/**
	 * Declares flags for manifest headers which specify storage types.
	 */
	enum class EFileMetaFlags : uint8
	{
		None           = 0,
		// Flag for readonly file.
		ReadOnly       = 1,
		// Flag for natively compressed.
		Compressed     = 1 << 1,
		// Flag for unix executable.
		UnixExecutable = 1 << 2
	};
	ENUM_CLASS_FLAGS(EFileMetaFlags);

	struct FFileManifest
	{
		FFileManifest();
		// The build relative filename.
		FString Filename;
		// Whether this is a symlink to another file.
		FString SymlinkTarget;
		// The file SHA1.
		FSHAHash FileHash;
		// The flags for this file.
		EFileMetaFlags FileMetaFlags;
		// The install tags for this file.
		TArray<FString> InstallTags;
		// The list of chunk parts to stitch.
		TArray<FChunkPart> ChunkParts;
		// The size of this file.
		uint64 FileSize;
	};

	struct FFileManifestList
	{
		FFileManifestList();
		/**
		 * Serialization operator.
		 * @param Ar                Archive to serialize to.
		 * @param FileManifestList  FFileManifestList to serialize.
		 * @return Passed in archive.
		 */
		friend FArchive& operator<<(FArchive& Ar, FFileManifestList& FileManifestList);
		/**
		 * Helper to sort and calculate file sizes after loading.
		 */
		void OnPostLoad();
		// The list of files.
		TArray<FFileManifest> FileList;
	};

	struct FCustomFields
	{
		FCustomFields();
		/**
		 * Serialization operator.
		 * @param Ar            Archive to serialize to.
		 * @param CustomFields  FCustomFields to serialize.
		 * @return Passed in archive.
		 */
		friend FArchive& operator<<(FArchive& Ar, FCustomFields& CustomFields);
		// The map of field name to field data.
		TMap<FString, FString> Fields;
	};

	class FManifestData
	{
	public:
		static void Init();
		static bool Serialize(FArchive& Ar, FBuildPatchAppManifest& AppManifest, BuildPatchServices::EFeatureLevel SaveFormat /* Ignored for loading */ = BuildPatchServices::EFeatureLevel::Latest);
	};
}

ENUM_RANGE_BY_FIRST_AND_LAST(BuildPatchServices::EFeatureLevel, BuildPatchServices::EFeatureLevel::Original, BuildPatchServices::EFeatureLevel::Latest);
