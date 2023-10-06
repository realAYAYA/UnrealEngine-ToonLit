// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "CoreTypes.h"
#include "Memory/MemoryFwd.h"


namespace UE::Cook
{

enum class ECookMetadataStateVersion : uint8
{
	PluginHierarchy = 1,

	// Add new versions above this.
	VersionCount,
	LatestVersion = VersionCount - 1
};


/** The name and dependency information for a plugin that was enabled during cooking. */
struct COOKMETADATA_API FCookMetadataPluginEntry
{
	FString Name;

	// The dependencies are stored in the FCookMetadataPluginHierarchy::PluginDependencies array,
	// and this is an index into it. From there you can get a further index into PluginsEnabledAtCook
	// to get the plugin information.
	// Example:
	//	for (uint16 DependencyIndex = Plugin->DependencyIndexStart; DependencyIndex < Plugin->DependencyIndexEnd; DependencyIndex++)
	//	{
	//		const UE::Cook::FCookMetadataPluginEntry& DependentPlugin = PluginHierarchy.PluginsEnabledAtCook[PluginHierarchy.PluginDependencies[DependencyIndex]];
	//	}
	//
	uint16 DependencyIndexStart;
	uint16 DependencyIndexEnd;

	uint16 DependencyCount() const { return DependencyIndexEnd - DependencyIndexStart; }

	friend FArchive& operator<<(FArchive& Ar, FCookMetadataPluginEntry& Entry)
	{
		return Ar << Entry.Name << Entry.DependencyIndexStart << Entry.DependencyIndexEnd;
	}
};

struct COOKMETADATA_API FCookMetadataPluginHierarchy
{
	// The list of plugins that were enabled during the cook that generated the FCookMetadataState,
	// pruned to remove plugins that aren't enabled on the cook platform. If the cook was DLC, this
	// is further pruned to only the DLC plugin and its dependencies.
	TArray<FCookMetadataPluginEntry> PluginsEnabledAtCook;

	// The list of plugin dependencies. FCookMetadataPluginEntry::DependencyIndexStart indexes into this
	// array.
	TArray<uint16> PluginDependencies;

	// The list of root plugins for the project as defined by the Editor.ini file.
	TArray<uint16> RootPlugins;

	friend FArchive& operator<<(FArchive& Ar, FCookMetadataPluginHierarchy& Hierarchy)
	{
		return Ar << Hierarchy.PluginsEnabledAtCook << Hierarchy.PluginDependencies << Hierarchy.RootPlugins;
	}
};

/**
*	Structure serialized to disk to contain non-asset related metadata about a cook. This should always
*	exist alongside a Development Asset Registry, and to ensure that the pair is not out of sync, users
*	should validate the development asset registry they are using with GetAssociatedDevelopmentAssetRegistryHash().
*/
class COOKMETADATA_API FCookMetadataState
{
public:
	FCookMetadataState() = default;
	FCookMetadataState(const FCookMetadataState&) = delete;
	FCookMetadataState(FCookMetadataState&& Rhs) = delete;
	~FCookMetadataState() = default;

	FCookMetadataState& operator=(const FCookMetadataState&) = delete;
	FCookMetadataState& operator=(FCookMetadataState&& O) = delete;

	
	bool Serialize(FArchive& Ar);

	// Plugin hierarchy information
	void SetPluginHierarchyInfo(FCookMetadataPluginHierarchy&& InPluginHierarchy) { PluginHierarchy = MoveTemp(InPluginHierarchy); }
	const FCookMetadataPluginHierarchy& GetPluginHierarchy() const { return PluginHierarchy; }

	/**
	*	Associated DevAR Hash.
	* 
	*	This is computed by reading the development asset registry file into a memory buffer and calling
	*	ComputeHashOfDevelopmentAssetRegistry on the data.
	* 
	*	Use this to ensure that the files you are working with were produced by the same cook and didn't
	*	get out of sync somehow.
	* 
	*	e.g.
	*	uint64 CheckHash = GetAssociatedDevelopmentAssetRegistryHash();
	*	bValidDevAr = ComputeHashOfDevelopmentAssetRegistry(MakeMemoryView(SerializedAssetRegistry)) == CheckHash;
	*/
	void SetAssociatedDevelopmentAssetRegistryHash(uint64 InHash) { AssociatedDevelopmentAssetRegistryHash = InHash; }
	uint64 GetAssociatedDevelopmentAssetRegistryHash() const { return AssociatedDevelopmentAssetRegistryHash; }

	static uint64 ComputeHashOfDevelopmentAssetRegistry(FMemoryView InSerializedDevelopmentAssetRegistry);
private:
	ECookMetadataStateVersion Version;
	FCookMetadataPluginHierarchy PluginHierarchy;

	uint64 AssociatedDevelopmentAssetRegistryHash;
};

COOKMETADATA_API const FString& GetCookMetadataFilename();

}; // namespace