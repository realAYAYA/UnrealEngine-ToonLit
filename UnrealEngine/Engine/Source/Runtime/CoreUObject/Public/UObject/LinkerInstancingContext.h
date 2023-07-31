// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/SoftObjectPath.h"

/**
 * Helper class to remap package imports during loading.
 * This is usually when objects in a package are outer-ed to object in another package or vice versa.
 * Instancing such a package without a instance remapping would resolve imports to the original package which is not desirable in an instancing context (i.e. loading a level instance)
 * This is because an instanced package has a different name than the package file name on disk, this class is used in the linker to remaps reference to the package name as stored in import tables on disk to the corresponding instanced package or packages we are loading.
 */
class FLinkerInstancingContext
{
public:
	FLinkerInstancingContext() = default;
	explicit FLinkerInstancingContext(TSet<FName> InTags)
		: Tags(MoveTemp(InTags))
	{
	}
	explicit FLinkerInstancingContext(bool bInSoftObjectPathRemappingEnabled)
		: bSoftObjectPathRemappingEnabled(bInSoftObjectPathRemappingEnabled)
	{
	}

	bool IsInstanced() const
	{
		return PackageMapping.Num() > 0 || PathMapping.Num() > 0;
	}

	/** Remap the package name from the import table to its instanced counterpart, otherwise return the name unmodified. */
	FName RemapPackage(const FName& PackageName) const
	{
		if (const FName* RemappedName = PackageMapping.Find(PackageName))
		{
			return *RemappedName;
		}
		return PackageName;
	}

	/**
	 * Remap the top level asset part of the path name to its instanced counterpart, otherwise return the name unmodified. 
	 * i.e. remaps /Path/To/Package.AssetName:Inner to /NewPath/To/NewPackage.NewAssetName:Inner 
	 */
	FSoftObjectPath RemapPath(const FSoftObjectPath& Path) const
	{
		if (const FTopLevelAssetPath* Remapped = PathMapping.Find(Path.GetAssetPath()))
		{
			return FSoftObjectPath(*Remapped, Path.GetSubPathString());
		}
		return Path;
	}

	/** Add a mapping from a package name to a new package name. There should be no separators (. or :) in these strings. */
	void AddPackageMapping(FName Original, FName Instanced)
	{
		PackageMapping.Add(Original, Instanced);
	}

	/** Add a mapping from a top level asset path (/Path/To/Package.AssetName) to another. */
	void AddPathMapping(FSoftObjectPath Original, FSoftObjectPath Instanced)
	{
		ensureAlwaysMsgf(Original.GetSubPathString().IsEmpty(), 
			TEXT("Linker instance remap paths should be top-level assets only: %s->"), *Original.ToString());
		ensureAlwaysMsgf(Instanced.GetSubPathString().IsEmpty(), 
			TEXT("Linker instance remap paths should be top-level assets only: ->%s"), *Instanced.ToString());
	
		PathMapping.Emplace(Original.GetAssetPath(), Instanced.GetAssetPath());
	}

	void AddTag(FName NewTag)
	{
		Tags.Add(NewTag);
	}

	void AppendTags(const TSet<FName>& NewTags)
	{
		Tags.Append(NewTags);
	}

	bool HasTag(FName Tag) const
	{
		return Tags.Contains(Tag);
	}

	void SetSoftObjectPathRemappingEnabled(bool bInSoftObjectPathRemappingEnabled)
	{
		bSoftObjectPathRemappingEnabled = bInSoftObjectPathRemappingEnabled;
	}

	bool GetSoftObjectPathRemappingEnabled() const 
	{ 
		return bSoftObjectPathRemappingEnabled; 
	}

	void SetRegenerateUniqueBulkDataGuids(bool bFlag)
	{
		bRegenerateUniqueBulkDataGuids = bFlag;
	}

	bool ShouldRegenerateUniqueBulkDataGuids() const
	{
		return bRegenerateUniqueBulkDataGuids;
	}

	/** Return the instanced package name for a given instanced outer package and an object package name */
	static FString GetInstancedPackageName(const FString& InOuterPackageName, const FString& InPackageName)
	{
		return FString::Printf(TEXT("%s_InstanceOf_%s"), *InOuterPackageName, *InPackageName);
	}

private:
	/** Used internally by the linker to try to fix references on relocated packages. */
	FName RelocatePackage(const FName& PackageName) const
	{
		if (const FName* RemappedName = RelocatedPackageMapping.Find(PackageName))
		{
			return *RemappedName;
		}
		return PackageName;
	}

	friend class FLinkerLoad;

	/** Map of original package name to their instance counterpart. */
	TMap<FName, FName> PackageMapping;
	/** Map of original top level asset path to their instance counterpart. */
	TMap<FTopLevelAssetPath, FTopLevelAssetPath> PathMapping;
	/** Map of original package name to their potential relocated counterpart. */
	TMap<FName, FName> RelocatedPackageMapping;

	/** Tags can be used to determine some loading behavior. */
	TSet<FName> Tags;
	/** Remap soft object paths */
	bool bSoftObjectPathRemappingEnabled = true;
	/** When true we will generate new unique identifiers for editor bulkdata objects */
	bool bRegenerateUniqueBulkDataGuids = false;
};
