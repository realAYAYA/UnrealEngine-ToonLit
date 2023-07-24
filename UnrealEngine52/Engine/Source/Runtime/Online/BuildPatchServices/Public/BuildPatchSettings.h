// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "BuildPatchDelta.h"
#include "BuildPatchFeatureLevel.h"
#include "BuildPatchInstall.h"
#include "BuildPatchVerify.h"
#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Containers/Set.h"
#include "Containers/SparseArray.h"
#include "Containers/UnrealString.h"
#include "CoreMinimal.h"
#include "HAL/Platform.h"
#include "Interfaces/IBuildManifest.h"
#include "Misc/Variant.h"
#include "Templates/UnrealTemplate.h"

class FVariant;

namespace BuildPatchServices
{
	enum class EInstallActionIntent : int32;

	/**
	 * Defines a list of all build patch services initialization settings, can be used to override default init behaviors.
	 */
	struct BUILDPATCHSERVICES_API FBuildPatchServicesInitSettings
	{
	public:
		/**
		 * Default constructor. Initializes all members with default behavior values.
		 */
		FBuildPatchServicesInitSettings();

	public:
		// The application settings directory.
		FString ApplicationSettingsDir;
		// The application project name.
		FString ProjectName;
		// The local machine config file name.
		FString LocalMachineConfigFileName;
	};

	struct BUILDPATCHSERVICES_API FInstallerAction
	{
	public:

		/**
		 * Creates an install action.
		 * @param Manifest          The manifest for the build to be installed.
		 * @param InstallTags       The install tags to use if selectively installing files. If empty set, all files will be installed.
		 * @return the action setup for performing an installation.
		 */
		static FInstallerAction MakeInstall(const IBuildManifestRef& Manifest, TSet<FString> InstallTags = TSet<FString>());

		/**
		 * Creates an update action.
		 * @param CurrentManifest   The manifest for the build currently installed.
		 * @param InstallManifest   The manifest for the build to be installed.
		 * @param InstallTags       The install tags to use if selectively installing files. If empty set, all files will be updated, or added if missing.
		 * @return the action setup for performing an update.
		 */
		static FInstallerAction MakeUpdate(const IBuildManifestRef& CurrentManifest, const IBuildManifestRef& InstallManifest, TSet<FString> InstallTags = TSet<FString>());

		/**
		 * Creates an install action.
		 * @param Manifest          The manifest for the build to be installed.
		 * @param InstallTags       The install tags to use if selectively installing files. If empty set, all files will be repaired.
		 * @return the action setup forcing an SHA check, and repair of all tagged files.
		 */
		static FInstallerAction MakeRepair(const IBuildManifestRef& Manifest, TSet<FString> InstallTags = TSet<FString>());


		/**
		 * Creates an uninstall action.
		 * @param Manifest          The manifest for the build currently installed.
		 * @return the action setup for performing an uninstall, deleting all files referenced by the manifest.
		 */
		static FInstallerAction MakeUninstall(const IBuildManifestRef& Manifest);

		/**
		 * Helper for creating an install action or update action based on validity of CurrentManifest.
		 * @param CurrentManifest   The manifest for the build currently installed, invalid if no build installed.
		 * @param InstallManifest   The manifest for the build to be installed.
		 * @param InstallTags       The install tags to use if selectively installing files. If empty set, all files will be installed/updated, or added if missing.
		 * @return the action setup for performing an install or update.
		 */
		static FInstallerAction MakeInstallOrUpdate(const IBuildManifestPtr& CurrentManifest, const IBuildManifestRef& InstallManifest, TSet<FString> InstallTags = TSet<FString>())
		{
			if (!CurrentManifest.IsValid())
			{
				return MakeInstall(InstallManifest, MoveTemp(InstallTags));
			}
			else
			{
				return MakeUpdate(CurrentManifest.ToSharedRef(), InstallManifest, MoveTemp(InstallTags));
			}
		}

		/**
		 * Copy constructor.
		 */
		FInstallerAction(const FInstallerAction& CopyFrom);

		/**
		 * RValue constructor to allow move semantics.
		 */
		FInstallerAction(FInstallerAction&& MoveFrom);

	public:
		/**
		 * @return true if this action intent is to perform a fresh installation.
		 */
		bool IsInstall() const;

		/**
		 * @return true if this action intent is to update an existing installation.
		 */
		bool IsUpdate() const;

		/**
		 * @return true if this action intent is to repair an existing installation.
		 */
		bool IsRepair() const;

		/**
		 * @return true if this action intent is to uninstall an installation.
		 */
		bool IsUninstall() const;

		/**
		 * @return the install tags for the action.
		 */
		const TSet<FString>& GetInstallTags() const;

		/**
		 * @return the manifest for the current installation, this will runtime assert if called invalidly (see TryGetCurrentManifest).
		 */
		IBuildManifestRef GetCurrentManifest() const;

		/**
		 * @return the manifest for the desired installation, this will runtime assert if called invalidly (see TryGetInstallManifest).
		 */
		IBuildManifestRef GetInstallManifest() const;

	public:
		/**
		 * Helper for getting the current manifest if nullable is preferred.
		 * @return the manifest for the current installation, if valid.
		 */
		IBuildManifestPtr TryGetCurrentManifest() const
		{
			if (IsUpdate() || IsRepair() || IsUninstall())
			{
				return GetCurrentManifest();
			}
			return nullptr;
		}

		/**
		 * Helper for getting the install manifest if nullable is preferred.
		 * @return true is this action intent it to perform a fresh installation.
		 */
		IBuildManifestPtr TryGetInstallManifest() const
		{
			if (IsInstall() || IsUpdate() || IsRepair())
			{
				return GetInstallManifest();
			}
			return nullptr;
		}

		/**
		 * Helper for getting the current manifest, or install manifest if no current manifest. One will always be valid.
		 * @return the current, or the install manifest, based on validity respectively.
		 */
		IBuildManifestRef GetCurrentOrInstallManifest() const
		{
			return (CurrentManifest.IsValid() ? CurrentManifest : InstallManifest).ToSharedRef();
		}

		/**
		 * Helper for getting the install manifest, or current manifest if no install manifest. One will always be valid.
		 * @return the install, or the current manifest, based on validity respectively.
		 */
		IBuildManifestRef GetInstallOrCurrentManifest() const
		{
			return (InstallManifest.IsValid() ? InstallManifest : CurrentManifest).ToSharedRef();
		}

	private:
		FInstallerAction();
		IBuildManifestPtr CurrentManifest;
		IBuildManifestPtr InstallManifest;
		TSet<FString> InstallTags;
		EInstallActionIntent ActionIntent;
	};

	/**
	 * DEPRECATED STRUCT. Please use FBuildInstallerConfiguration.
	 */
	struct BUILDPATCHSERVICES_API FInstallerConfiguration
	{
		FInstallerConfiguration(const IBuildManifestRef& InInstallManifest);
		FInstallerConfiguration(const FInstallerConfiguration& CopyFrom);
		FInstallerConfiguration(FInstallerConfiguration&& MoveFrom);

	public:
		IBuildManifestPtr CurrentManifest;
		IBuildManifestRef InstallManifest;
		FString InstallDirectory;
		FString StagingDirectory;
		FString BackupDirectory;
		TArray<FString> ChunkDatabaseFiles;
		TArray<FString> CloudDirectories;
		TSet<FString> InstallTags;
		EInstallMode InstallMode;
		EVerifyMode VerifyMode;
		EDeltaPolicy DeltaPolicy;
		bool bIsRepair;
		bool bRunRequiredPrereqs;
		bool bAllowConcurrentExecution;
	};

	/**
	 * Defines a list of all the options of an installation task.
	 */
	struct BUILDPATCHSERVICES_API FBuildInstallerConfiguration
	{
		/**
		 * Construct with an array of action objects
		 */
		FBuildInstallerConfiguration(TArray<FInstallerAction> InstallerActions);

		/**
		 * Copy constructor.
		 */
		FBuildInstallerConfiguration(const FBuildInstallerConfiguration& CopyFrom);

		/**
		 * RValue constructor to allow move semantics.
		 */
		FBuildInstallerConfiguration(FBuildInstallerConfiguration&& MoveFrom);

	public:
		// The array of intended actions to perform.
		TArray<FInstallerAction> InstallerActions;
		// The directory to install to.
		FString InstallDirectory;
		// The directory for storing the intermediate files. This would usually be inside the InstallDirectory. Empty string will use module's global setting.
		FString StagingDirectory;
		// The directory for placing files that are believed to have local changes, before we overwrite them. Empty string will use module's global setting. If both empty, the feature disables.
		FString BackupDirectory;
		// The list of chunk database filenames that will be used to pull patch data from.
		TArray<FString> ChunkDatabaseFiles;
		// The list of cloud directory roots that will be used to pull patch data from. Empty array will use module's global setting.
		TArray<FString> CloudDirectories;
		// The mode for installation.
		EInstallMode InstallMode;
		// The mode for verification. This will be overridden for any files referenced by a repair action.
		EVerifyMode VerifyMode;
		// The policy to follow for requesting an optimised delta.
		EDeltaPolicy DeltaPolicy;
		// Whether to run the prerequisite installer provided if it hasn't been ran before on this machine.
		bool bRunRequiredPrereqs;
		// Whether to allow this installation to run concurrently with any existing installations.
		bool bAllowConcurrentExecution;
	};

	/**
	 * Defines a list of all options for the build chunking task.
	 */
	struct BUILDPATCHSERVICES_API FChunkBuildConfiguration
	{
	public:
		/**
		 * Default constructor
		 */
		FChunkBuildConfiguration();

	public:
		// The client feature level to output data for.
		EFeatureLevel FeatureLevel;
		// The directory to analyze.
		FString RootDirectory;
		// The ID of the app of this build.
		uint32 AppId;
		// The name of the app of this build.
		FString AppName;
		// The version string for this build.
		FString BuildVersion;
		// The local exe path that would launch this build.
		FString LaunchExe;
		// The command line that would launch this build.
		FString LaunchCommand;
		// The path to a file containing a \r\n separated list of RootDirectory relative files to read.
		FString InputListFile;
		// The path to a file containing a \r\n separated list of RootDirectory relative files to ignore.
		FString IgnoreListFile;
		// The path to a file containing a \r\n separated list of RootDirectory relative files followed by attribute keywords.
		FString AttributeListFile;
		// The set of identifiers which the prerequisites satisfy.
		TSet<FString> PrereqIds;
		// The display name of the prerequisites installer.
		FString PrereqName;
		// The path to the prerequisites installer.
		FString PrereqPath;
		// The command line arguments for the prerequisites installer.
		FString PrereqArgs;
		// The maximum age (in days) of existing data files which can be reused in this build.
		float DataAgeThreshold;
		// Indicates whether data age threshold should be honored. If false, ALL data files can be reused.
		bool bShouldHonorReuseThreshold;
		// The chunk window size to be used when saving out new data.
		uint32 OutputChunkWindowSize;
		// Indicates whether any window size chunks should be matched, rather than just out output window size.
		bool bShouldMatchAnyWindowSize;
		// Map of custom fields to add to the manifest.
		TMap<FString, FVariant> CustomFields;
		// The cloud directory that all patch data will be saved to. An empty value will use module's global setting.
		FString CloudDirectory;
		// The output manifest filename.
		FString OutputFilename;
		// Allow Manifest Creation for builds with no data
		bool bAllowEmptyBuild;
	};

	// Temporary for use with deprecated module function.
	typedef FChunkBuildConfiguration FGenerationConfiguration;

	/**
	 * Defines a list of all options for the chunk delta optimisation task.
	 */
	struct BUILDPATCHSERVICES_API FChunkDeltaOptimiserConfiguration
	{
	public:
		/**
		 * Default constructor
		 */
		FChunkDeltaOptimiserConfiguration();

	public:
		// A full file or http path for the manifest to be used as the source build.
		FString ManifestAUri;
		// A full file or http path for the manifest to be used as the destination build.
		FString ManifestBUri;
		// The cloud directory that all patch data will be saved to. An empty value will use ManifestB's directory.
		FString CloudDirectory;
		// The window size to use for find new matches.
		uint32 ScanWindowSize;
		// The chunk size to use for saving new diff data.
		uint32 OutputChunkSize;
		// A threshold for the original delta size, for which we would abort and not process.
		uint64 DiffAbortThreshold;
	};

	/**
	 * Defines a list of all options for the patch data enumeration task.
	 */
	struct BUILDPATCHSERVICES_API FPatchDataEnumerationConfiguration
	{
	public:
		/**
		 * Default constructor
		 */
		FPatchDataEnumerationConfiguration();

	public:
		// A full file path for the manifest or chunkdb to enumerate referenced data for.
		FString InputFile;
		// A full file path to a file where the list will be saved out to.
		FString OutputFile;
		// Whether to include files sizes.
		bool bIncludeSizes;
	};

	/**
	 * Defines a list of all options for the diff manifests task.
	 */
	struct BUILDPATCHSERVICES_API FDiffManifestsConfiguration
	{
	public:
		/**
		 * Default constructor
		 */
		FDiffManifestsConfiguration();

	public:
		// A full file or http path for the manifest to be used as the source build.
		FString ManifestAUri;
		// A full file or http path for the manifest to be used as the destination build.
		FString ManifestBUri;
		// The tag set to use to filter desired files from ManifestA.
		TSet<FString> TagSetA;
		// The tag set to use to filter desired files from ManifestB.
		TSet<FString> TagSetB;
		// Tag sets that will be used to calculate additional differential size statistics between manifests.
		// They must all be a subset of anything used in TagSetB.
		TArray<TSet<FString>> CompareTagSets;
		// A full file path where a JSON object will be saved for the diff details.Empty string if not desired.
		FString OutputFilePath;
	};

	/**
	 * Defines a list of all options for the cloud directory compactifier task.
	 */
	struct BUILDPATCHSERVICES_API FCompactifyConfiguration
	{
	public:
		/**
		 * Default constructor
		 */
		FCompactifyConfiguration();

	public:
		// The path to the directory to compactify.
		FString CloudDirectory;
		// Chunks which are not referenced by a valid manifest, and which are older than this age(in days), will be deleted.
		float DataAgeThreshold;
		// The full path to a file to which a list of all chunk files deleted by compactify will be written.The output filenames will be relative to the cloud directory.
		FString DeletedChunkLogFile;
		// If ran in preview mode, then the process will run in logging mode only - no files will be deleted.
		bool bRunPreview;
	};

	/**
	 * Defines a list of all options for the chunk packaging task.
	 */
	struct BUILDPATCHSERVICES_API FPackageChunksConfiguration
	{
	public:
		/**
		 * Default constructor
		 */
		FPackageChunksConfiguration();

	public:
		// The client feature level to output data for.
		EFeatureLevel FeatureLevel;
		// A full file path to the manifest to enumerate chunks from.
		FString ManifestFilePath;
		// A full file path to a manifest describing a previous build, which will filter out saved chunks for patch only chunkdbs.
		FString PrevManifestFilePath;
		// Optional list of tagsets to split chunkdb files on. Empty array will include all data as normal.
		TArray<TSet<FString>> TagSetArray;
		// Optional tagset to filter the files used from PrevManifestFilePath, potentially increasing the number of chunks saved.
		TSet<FString> PrevTagSet;
		// A full file path to the chunkdb file to save. Extension of .chunkdb will be added if not present.
		FString OutputFile;
		// Cloud directory where chunks to be packaged can be found.
		FString CloudDir;
		// The maximum desired size for each chunkdb file.
		uint64 MaxOutputFileSize;
		// A full file path to use when saving the json output data.
		FString ResultDataFilePath;
	};
}

