// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryState.h"
#include "Containers/Map.h"
#include "Cooker/PackageResultsMessage.h"
#include "Misc/AssetRegistryInterface.h"
#include "Misc/Paths.h"
#include "Templates/UniquePtr.h"
#include "UObject/Object.h"
#include "UObject/UObjectHash.h"

class FSandboxPlatformFile;
class IAssetRegistry;
class ITargetPlatform;
class IChunkDataGenerator;
class UChunkDependencyInfo;
struct FChunkDependencyTreeNode;
struct FCookTagList;
struct FSoftObjectPath;
namespace UE::Cook { class FAssetRegistryPackageMessage; }
namespace UE::Cook { class FCookWorkerClient; }
namespace UE::Cook { struct FPackageData; }

/**
 * Helper class for generating streaming install manifests
 */
class FAssetRegistryGenerator
{
public:
	/**
	 * Constructor
	 */
	FAssetRegistryGenerator(const ITargetPlatform* InPlatform);

	/**
	 * Destructor
	 */
	~FAssetRegistryGenerator();

	/**
	 * Initializes manifest generator - creates manifest lists, hooks up delegates.
	 */
	void Initialize(const TArray<FName> &StartupPackages, bool bInitializeFromExisting);

	const ITargetPlatform* GetTargetPlatform() const { return TargetPlatform; }

	/** 
	 * Sets asset registry from a previous run that is used for iterative cooking
	 */
	void SetPreviousAssetRegistry(TUniquePtr<FAssetRegistryState>&& PreviousState);

	/**
	 * Options when computing the differences between current and previous state.
	 */
	struct FComputeDifferenceOptions
	{
		/** if true, modified packages are recursed to X in X->Y->Z chains. Otherwise, only Y and Z are seen as modified */
		bool bRecurseModifications;
		/** if true, modified script / c++ packages are recursed, if false only asset references are recursed */
		bool bRecurseScriptModifications;
	};

	/**
	 * Differences between the current and the previous state.
	 */
	struct FAssetRegistryDifference
	{
		/** ModifiedPackages list of packages which existed beforeand now, but need to be recooked */
		TSet<FName> ModifiedPackages;
		/** NewPackages list of packages that did not exist before, but exist now */
		TSet<FName> NewPackages;
		/** RemovedPackages list of packages that existed before, but do not any more */
		TSet<FName> RemovedPackages;
		/** IdenticalCookedPackages list of cooked packages that have not changed */
		TSet<FName> IdenticalCookedPackages;
		/** IdenticalUncookedPackages list of uncooked packages that have not changed.These were filtered out by platform or editor only */
		TSet<FName> IdenticalUncookedPackages;
	};

	/**
	 * Computes differences between the current asset registry state and the provided previous state.
	 *
	 * @param Options options to use when computing the differences
	 * @param PreviousAssetPackageDataMap previously cooked asset package data
	 * @param OutDifference the differences between the current and the previous state
	 */
	void ComputePackageDifferences(const FComputeDifferenceOptions& Options, const FAssetRegistryState& PreviousState, FAssetRegistryDifference& OutDifference);

	/** Computes just the list of packages in the PreviousState that no longer exist in the current state. */
	void ComputePackageRemovals(const FAssetRegistryState& PreviousState, TArray<FName>& RemovedPackages);

	/**
	 * FinalizeChunkIDs 
	 * Create the list of packages to store in each Chunk; each Chunk corresponds to a pak file, or group of pak files if the chunk is split.
	 * Each package may be in multiple chunks. The selection of chunks for each package is based on
	 * ChunkIds explicitly assigned to the package in the editor (UPackage::GetChunkIds()) and
	 * on assignment rules defined by the AssetManager
	 *
	 * @param CookedPackages list of packages which were cooked
	 * @param DevelopmentOnlyPackages list of packages that were specifically not cooked, but to add to the development asset registry
	 * @param InSandboxFile sandbox to load/save data
	 * @param bGenerateStreamingInstallManifest should we build a streaming install manifest
	 *        If false, no manifest is written and all packages are implicitly assigned to chunk 0 by the automation tool.
	 *        If true, packages are assigned to chunks based on settings (possibly all in chunk 0 if settings are empty)
	 *        and a manifest of packagenames is written for each chunk
	 */
	void FinalizeChunkIDs(const TSet<FName>& CookedPackages, const TSet<FName>& DevelopmentOnlyPackages,
		FSandboxPlatformFile& InSandboxFile, bool bGenerateStreamingInstallManifest);

	/**
	 * Register a chunk data generator with this generator.
	 * @note Should be called prior to SaveManifests.
	 */
	void RegisterChunkDataGenerator(TSharedRef<IChunkDataGenerator> InChunkDataGenerator);

	/**
	* PreSave
	* Notify generator that we are about to save the registry and chunk manifests
	*/
	void PreSave(const TSet<FName>& InCookedPackages);

	/**
	* PostSave
	* Notify generator that we are finished saving registry
	*/
	void PostSave();

	/**
	 * ContainsMap
	 * Does this package contain a map file (determined by finding if this package contains a UWorld / ULevel object)
	 *
	 * @param PackageName long package name of the package we want to determine if contains a map 
	 * @return return if the package contains a UWorld / ULevel object (contains a map)
	 */
	bool ContainsMap(const FName& PackageName) const;

	/** 
	 * Returns editable version of the asset package state being generated 
	 */
	FAssetPackageData* GetAssetPackageData(const FName& PackageName);

	/**
	 * Deletes temporary manifest directories.
	 */
	void CleanManifestDirectories();

	/**
	 * Saves all generated manifests for each target platform.
	 * 
	 * @param InSandboxFile the InSandboxFile used during cook
	 * @param InOverrideChunkSize the ChunkSize used during chunk division.
	 *        If greater than 0, this overrides the default chunksize derived from the platform.
	 * @param InManifestSubDir If non-null, the manifests are written into this subpath
	 *        of the usual location.
	 */
	bool SaveManifests(FSandboxPlatformFile& InSandboxFile, int64 InOverrideChunkSize = 0,
		const TCHAR* InManifestSubDir = nullptr);

	/**
	* Saves generated asset registry data for each platform.
	*/
	bool SaveAssetRegistry(const FString& SandboxPath, bool bSerializeDevelopmentAssetRegistry = true, bool bForceNoFilterAssets = false);

	/** 
	 * Writes out CookerOpenOrder.log file 
	 */
	bool WriteCookerOpenOrder(FSandboxPlatformFile& InSandboxFile);

	/**
	 * Follows an assets dependency chain to build up a list of package names in the same order as the runtime would attempt to load them
	 * 
	 * @param InPackageName - The name of the package containing the asset to (potentially) add to the file order
	 * @param OutFileOrder - Output array which collects the package names, maintaining order
	 * @param OutEncounteredArray - Temporary collection of package names we've seen. Similar to OutFileOrder but updated BEFORE following dependencies so as to avoid circular references
	 * @param InPackageNameSet - The source package name list. Used to distinguish between dependencies on other packages and internal objects
	 * @param InTopLevelAssets - Names of packages containing top level assets such as maps
	 */
	void AddAssetToFileOrderRecursive(const FName& InPackageName, TArray<FName>& OutFileOrder, TSet<FName>& OutEncounteredNames, const TSet<FName>& InPackageNameSet, const TSet<FName>& InTopLevelAssets);

	/**
	 * Get pakchunk file index from ChunkID
	 *
	 * @param ChunkID
	 * @return Index of target pakchunk file
	 */
	int32 GetPakchunkIndex(int32 ChunkId) const;

	/**
	 * Returns the chunks
	 */
	void GetChunkAssignments(TArray<TSet<FName>>& OutAssignments) const;

	/**
	 * Attempts to update the metadata for a package in an asset registry generator.
	 * This is only called for CookByTheBooks.
	 *
	 * @param Package The package to update info on
	 * @param SavePackageResult The metadata to associate with the given package name
	 */
	void UpdateAssetRegistryData(const UPackage& Package, FSavePackageResultStruct& SavePackageResult, FCookTagList&& InArchiveCookTagList);
	void UpdateAssetRegistryData(UE::Cook::FPackageData& PackageData, UE::Cook::FAssetRegistryPackageMessage&& Message);

	/**
	 * Check config to see whether chunk assignments use the AssetManager. If so, run the once-per-process construction
	 * of ManageReferences and store them in the global AssetRegistry.
	 */
	static void UpdateAssetManagerDatabase();

private:
	/**
	 * Ensures all assets in the input package are present in the registry
	 * @param Package - Package to process
	 * @return - Array of FAssetData entries for all assets in the input package
	 */
	typedef TArray<const FAssetData*, TInlineAllocator<1>> FCreateOrFindArray;
	FCreateOrFindArray CreateOrFindAssetDatas(const UPackage& Package);

	/**
	 * Updates all asset package flags in the specified package
	 *
	 * @param PackageName The name of the package
	 * @param PackageFlags Package flags to set
	 * @return True if any assets exists in the package
	 */
	bool UpdateAssetPackageFlags(const FName& PackageName, const uint32 PackageFlags);

	/**
	 * Updates AssetData with previous TagsAndValues, and updates PackageData values with previous PackageData,
	 * for all packages kept from a previous cook.
	 */
	void UpdateKeptPackages();

	static void InitializeUseAssetManager();

	/** State of the asset registry that is being built for this platform */
	FAssetRegistryState State;
	
	/** 
	 * The list of tags to add for each asset. This is populated during cook by the books,
	 * and is only added to development registries.
	 */
	TMap<FSoftObjectPath, TArray<TPair<FName, FString>>> CookTagsToAdd;

	TMap<FName, TPair<TArray<FAssetData>, FAssetPackageData>> PreviousPackagesToUpdate;
	/** List of packages that were loaded at startup */
	TSet<FName> StartupPackages;
	/** List of packages that were successfully cooked */
	TSet<FName> CookedPackages;
	/** List of packages that were filtered out from cooking */
	TSet<FName> DevelopmentOnlyPackages;
	/** List of packages that were kept from a previous cook */
	TArray<FName> KeptPackages;
	/** Map of Package name to Sandbox Paths */
	typedef TMap<FName, FString> FChunkPackageSet;
	/** Holds a reference to asset registry */
	IAssetRegistry& AssetRegistry;
	/** Platform to generate the manifest for */
	const ITargetPlatform* TargetPlatform;
	/** List of all asset packages that were created while loading the last package in the cooker. */
	TSet<FName> AssetsLoadedWithLastPackage;
	/** Lookup for the ChunkIDs that were explicitly set by the user in the editor */
	TMap<FName, TArray<int32>> ExplicitChunkIDs;
	/** Set of packages containing a map */
	TSet<FName> PackagesContainingMaps;
	/** Should the chunks be generated or only asset registry */
	bool bGenerateChunks;
	/** Highest chunk id, being used for geneating dependency tree */
	int32 HighestChunkId;
	/** Array of Maps with chunks<->packages assignments */
	TArray<TUniquePtr<FChunkPackageSet>> ChunkManifests;
	/** Map of packages that has not been assigned to chunks */
	FChunkPackageSet UnassignedPackageSet;
	/** Map of all cooked Packages */
	FChunkPackageSet AllCookedPackageSet;
	/** Array of Maps with chunks<->packages assignments. This version contains all dependent packages */
	TArray<TUniquePtr<FChunkPackageSet>> FinalChunkManifests;
	/** Additional data generators used when creating chunks */
	TArray<TSharedRef<IChunkDataGenerator>> ChunkDataGenerators;
	/** Lookup table of used package names used when searching references. */
	TSet<FName> InspectedNames;
	/** Source of the config-driven parent-child relationships between chunks. */
	UChunkDependencyInfo& DependencyInfo;

	/** Required flags a dependency must have if it is to be followed when adding package dependencies to chunks.*/
	UE::AssetRegistry::EDependencyQuery DependencyQuery;

	/** Mapping from chunk id to pakchunk file index. If not defined, Pakchunk index will be the same as chunk id by default */
	TMap<int32, int32> ChunkIdPakchunkIndexMapping;

	/** True if we should use the AssetManager, false to use the deprecated path */
	static bool bUseAssetManager;

	struct FReferencePair
	{
		FReferencePair() {}

		FReferencePair(const FName& InName, uint32 InParentIndex)
			: PackageName(InName)
			, ParentNodeIndex(InParentIndex)
		{}

		bool operator == (const FReferencePair& RHS) const
		{
			return PackageName == RHS.PackageName;
		}

		FName		PackageName;
		uint32		ParentNodeIndex;
	};

	/**
	 * Updates AssetData with TagsAndValues corresponding to any collections 
	 * flagged for inclusion as asset registry tags.
	 */
	void UpdateCollectionAssetData();

	/**
	 * Adds a package to chunk manifest
	 * 
	 * @param The sandbox filepath of the package
	 * @param The package name
	 * @param The ID of the chunk to assign it to
	 */
	void AddPackageToManifest(const FString& PackageSandboxPath, FName PackageName, int32 ChunkId);

	/**
	* Remove a package from a chunk manifest. Does nothing if package doesn't exist in chunk.
	*
	* @param The package name
	* @param The ID of the chunk to assign it to
	*/
	void RemovePackageFromManifest(FName PackageName, int32 ChunkId);

	/**
	 * Walks the dependency graph of assets and assigns packages to correct chunks.
	 * 
	 * @param the InSandboxFile used during cook
	 */
	void FixupPackageDependenciesForChunks(FSandboxPlatformFile& InSandboxFile);

	/**
	 * Attaches encryption key guids into the registry data for encrypted primary assets
	 */
	void InjectEncryptionData(FAssetRegistryState& TargetState);

	void AddPackageAndDependenciesToChunk(FChunkPackageSet& ThisPackageSet, FName InPkgName,
		const FString& InSandboxFile, int32 PakchunkIndex, FSandboxPlatformFile& SandboxPlatformFile);

	/**
	 * Returns the path of the temporary packaging directory for the specified platform.
	 */
	FString GetTempPackagingDirectoryForPlatform(const FString& Platform) const
	{
		return FPaths::ProjectSavedDir() / TEXT("TmpPackaging") / Platform;
	}

	/** Returns the config-driven max size of a chunk for the given platform, or -1 for no limit. */
	int64 GetMaxChunkSizePerPlatform( const ITargetPlatform* Platform ) const;

	/** Returns an array of chunks ID for a package name that have been assigned during the cook process. */
	TArray<int32> GetExistingPackageChunkAssignments(FName PackageFName);

	/**
	 * Get chunks IDs for a package that were assigned to the package in the editor from AssetFileContextMenu.
	 * These explicit chunkids are unioned with the chunkids calculated by the AssetManager.
	 */
	TArray<int32> GetExplicitChunkIDs(const FName& PackageFName);

	/** Calculate the final ChunkIds used by the package and store the package in the manifest for each of those chunks. */
	void CalculateChunkIdsAndAssignToManifest(const FName& PackageFName, const FString& PackagePathName,
		const FString& SandboxFilename, const FString& LastLoadedMapName, FSandboxPlatformFile& InSandboxFile);

	/** Deletes the temporary packaging directory for the specified platform */
	bool CleanTempPackagingDirectory(const FString& Platform) const;

	/** Returns true if the specific platform desires multiple chunks suitable for streaming install */
	bool ShouldPlatformGenerateStreamingInstallManifest(const ITargetPlatform* Platform) const;

	/** Generates and saves streaming install chunk manifest */
	bool GenerateStreamingInstallManifest(int64 InOverrideChunkSize, const TCHAR* InManifestSubDir,
		FSandboxPlatformFile& InSandboxFile);

	/** Gather a list of dependencies required by to completely load this package */
	bool GatherAllPackageDependencies(FName PackageName, TArray<FName>& DependentPackageNames);

	/** Gather the list of dependencies that link the source to the target.  Output array includes the target */
	bool GetPackageDependencyChain(FName SourcePackage, FName TargetPackage, TSet<FName>& VisitedPackages, TArray<FName>& OutDependencyChain);

	/** Get an array of Packages this package will import */
	bool GetPackageDependencies(FName PackageName, TArray<FName>& DependentPackageNames, UE::AssetRegistry::EDependencyQuery InDependencyQuery);

	/** Save a CSV dump of chunk asset information, if bWriteIndividualFiles is true it writes a CSV per chunk in addition to AllChunksInfo */
	bool GenerateAssetChunkInformationCSV(const FString& OutputPath, bool bWriteIndividualFiles = false);

	/** Finds the asset belonging to ChunkID with the smallest number of links to Packages In PackageNames */
	void FindShortestReferenceChain(TArray<FReferencePair> PackageNames, int32 ChunkID, uint32& OutParentIndex, FString& OutChainPath);

	/** Helper function for FindShortestReferenceChain */
	FString	GetShortestReferenceChain(FName PackageName, int32 ChunkID);

	/** Recursively remove redundant packages from child chunks based on the chunk dependency tree. */
	void SubtractParentChunkPackagesFromChildChunks(const FChunkDependencyTreeNode& Node,
		const TSet<FName>& CumulativeParentPackages, TArray<TArray<FName>>& OutPackagesMovedBetweenChunks);

	/** Helper function to verify Chunk asset assignment is valid */
	bool CheckChunkAssetsAreNotInChild(const FChunkDependencyTreeNode& Node);

	/** Helper function to create a given collection. */
	bool CreateOrEmptyCollection(FName CollectionName);

	/** Helper function to fill a given collection with a set of packages */
	void WriteCollection(FName CollectionName, const TArray<FName>& PackageNames);
	
	/** Initialize ChunkIdPakchunkIndexMapping and PakchunkIndexChunkIdMapping. */
	void InitializeChunkIdPakchunkIndexMapping();

	/**
	 * Helper function to find or create asset data for the input object. If the asset is not in the registry it will be added.
	 */
	const FAssetData* CreateOrFindAssetData(UObject& Object);
};

namespace UE::Cook
{

class IAssetRegistryReporter
{
public:
	virtual ~IAssetRegistryReporter() {}

	virtual void UpdateAssetRegistryData(FPackageData& PackageData, const UPackage& Package,
		FSavePackageResultStruct& SavePackageResult, FCookTagList&& InArchiveCookTagList) = 0;

};

class FAssetRegistryReporterLocal : public IAssetRegistryReporter
{
public:
	FAssetRegistryReporterLocal(FAssetRegistryGenerator& InGenerator)
		: Generator(InGenerator)
	{
	}

	virtual void UpdateAssetRegistryData(FPackageData& PackageData, const UPackage& Package,
		FSavePackageResultStruct& SavePackageResult, FCookTagList&& InArchiveCookTagList) override
	{
		Generator.UpdateAssetRegistryData(Package, SavePackageResult, MoveTemp(InArchiveCookTagList));
	}

private:
	FAssetRegistryGenerator& Generator;
};

class FAssetRegistryReporterRemote : public IAssetRegistryReporter
{
public:
	FAssetRegistryReporterRemote(FCookWorkerClient& InClient, const ITargetPlatform* InTargetPlatform);

	virtual void UpdateAssetRegistryData(FPackageData& PackageData, const UPackage& Package,
		FSavePackageResultStruct& SavePackageResult, FCookTagList&& InArchiveCookTagList) override;

private:
	FCookWorkerClient& Client;
	const ITargetPlatform* TargetPlatform = nullptr;
};

class FAssetRegistryPackageMessage : public IPackageMessage
{
public:
	virtual void Write(FCbWriter& Writer, const FPackageData& PackageData, const ITargetPlatform* TargetPlatform) const override;
	virtual bool TryRead(FCbObject&& Object, FPackageData& PackageData, const ITargetPlatform* TargetPlatform) override;
	virtual FGuid GetMessageType() const override { return MessageType; }

	TArray<FAssetData> AssetDatas;
	TMap<FSoftObjectPath, TArray<TPair<FName, FString>>> CookTags;
	uint32 PackageFlags = 0;
	int64 DiskSize = -1;

public:
	static FGuid MessageType;
};



}