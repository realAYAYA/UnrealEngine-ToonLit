// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/SoftObjectPath.h"

/**
 * Helper class to map between an original package and an instance of it (including world partition cells).
 */
class FLinkerInstancedPackageMap
{
public:
	enum class EInstanceMappingDirection : uint8
	{
		OriginalToInstanced,
		InstancedToOriginal,
	};

	FLinkerInstancedPackageMap() :
		FLinkerInstancedPackageMap(EInstanceMappingDirection::OriginalToInstanced) {}

	explicit FLinkerInstancedPackageMap(EInstanceMappingDirection MappingDirection)
		: InstanceMappingDirection(MappingDirection)
#if WITH_EDITOR
		, bEnableNonEditorPath(false)
#else
		, bEnableNonEditorPath(true)
#endif
	{
	}

	bool IsInstanced() const
	{
		return InstancedPackageMapping.Num() > 0;
	}

	/** Remap the package name from the import table to its instanced counterpart, otherwise return the name unmodified. */
	FName RemapPackage(const FName& PackageName) const
	{
		if (const FName* RemappedName = InstancedPackageMapping.Find(PackageName))
		{
			return *RemappedName;
		}
		return PackageName;
	}

	/** Add a mapping from a package name to a new package name. There should be no separators (. or :) in these strings. */
	COREUOBJECT_API void AddPackageMapping(FName Original, FName Instanced);

	COREUOBJECT_API void BuildPackageMapping(FName Original, FName Instanced, const bool bBuildWorldPartitionCellMapping = true);

	COREUOBJECT_API bool FixupSoftObjectPath(FSoftObjectPath& InOutSoftObjectPath) const;

private:
	friend class FLinkerInstancingContext;
	friend class FLinkerInstancingContextTests;
	void EnableAutomationTest() { bEnableNonEditorPath = true; }

	/**
	 * Map between the original package name and its instance counterpart.
	 * Key=Original and Value=Instanced when InstanceMappingDirection==OriginalToInstanced
	 * Key=Instanced and Value=Original when InstanceMappingDirection==InstancedToOriginal
	 */
	TMap<FName, FName> InstancedPackageMapping;

	/**
	 * In which direction has this mapping been built?
	 */
	EInstanceMappingDirection InstanceMappingDirection;

	/** Data needed to re-map world partition cells */
	FString GeneratedPackagesFolder;
	FString InstancedPackagePrefix;
	FString InstancedPackageSuffix;

	/** Allows tests to run non editor path from editor build. */
	bool bEnableNonEditorPath;
};

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
		return InstancedPackageMap.IsInstanced() || PathMapping.Num() > 0;
	}

	/** Remap the package name from the import table to its instanced counterpart, otherwise return the name unmodified. */
	FName RemapPackage(const FName& PackageName) const
	{
		return InstancedPackageMap.RemapPackage(PackageName);
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
		InstancedPackageMap.AddPackageMapping(Original, Instanced);
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

	UE_DEPRECATED(5.2, "No longer used, pass ELoadFlags::LOAD_RegenerateBulkDataGuids to LoadPackage instead")
	void SetRegenerateUniqueBulkDataGuids(bool bFlag) { }

	UE_DEPRECATED(5.2, "No longer used, check ELoadFlags::LOAD_RegenerateBulkDataGuids in the LoadFlags instead")
	bool ShouldRegenerateUniqueBulkDataGuids() const { return false; }

	/** Return the instanced package name for a given instanced outer package and an object package name */
	static FString GetInstancedPackageName(const FString& InOuterPackageName, const FString& InPackageName)
	{
		return FString::Printf(TEXT("%s_InstanceOf_%s"), *InOuterPackageName, *InPackageName);
	}

	void FixupSoftObjectPath(FSoftObjectPath& InOutSoftObjectPath) const;

private:
	void EnableAutomationTest() { InstancedPackageMap.EnableAutomationTest(); }

	void BuildPackageMapping(FName Original, FName Instanced)
	{
		InstancedPackageMap.BuildPackageMapping(Original, Instanced, GetSoftObjectPathRemappingEnabled());
	}

	FName& FindOrAddPackageMapping(FName Original)
	{
		return InstancedPackageMap.InstancedPackageMapping.FindOrAdd(Original);
	}

	friend class FLinkerLoad;
	friend struct FAsyncPackage2;
	friend class FLinkerInstancingContextTests;

	/** Map of original package name to their instance counterpart. */
	FLinkerInstancedPackageMap InstancedPackageMap;
	/** Map of original top level asset path to their instance counterpart. */
	TMap<FTopLevelAssetPath, FTopLevelAssetPath> PathMapping;
	/** Tags can be used to determine some loading behavior. */
	TSet<FName> Tags;
	/** Remap soft object paths */
	bool bSoftObjectPathRemappingEnabled = true;
};
