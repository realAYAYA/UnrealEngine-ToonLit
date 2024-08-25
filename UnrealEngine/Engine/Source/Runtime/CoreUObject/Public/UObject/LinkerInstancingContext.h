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
		, bIsInstanced(false)
#if WITH_EDITOR
		, bEnableNonEditorPath(false)
#else
		, bEnableNonEditorPath(true)
#endif
	{
	}

	bool IsInstanced() const
	{
		return bIsInstanced;
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

	/** Whether InstancedPackageMapping contains remapping data other that none */
	bool bIsInstanced;

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
	COREUOBJECT_API FLinkerInstancingContext();
	COREUOBJECT_API explicit FLinkerInstancingContext(TSet<FName> InTags);
	COREUOBJECT_API explicit FLinkerInstancingContext(bool bInSoftObjectPathRemappingEnabled);

	COREUOBJECT_API static FLinkerInstancingContext DuplicateContext(const FLinkerInstancingContext& InLinkerInstancingContext);

	COREUOBJECT_API bool IsInstanced() const;

	/** Remap the package name from the import table to its instanced counterpart, otherwise return the name unmodified. */
	COREUOBJECT_API FName RemapPackage(const FName& PackageName) const;

	/**
	 * Remap the top level asset part of the path name to its instanced counterpart, otherwise return the name unmodified. 
	 * i.e. remaps /Path/To/Package.AssetName:Inner to /NewPath/To/NewPackage.NewAssetName:Inner 
	 */
	COREUOBJECT_API FSoftObjectPath RemapPath(const FSoftObjectPath& Path) const;

	/** Add a mapping from a package name to a new package name. There should be no separators (. or :) in these strings. */
	COREUOBJECT_API void AddPackageMapping(FName Original, FName Instanced);

	/** Add a mapping function from a package name to a new package name. This function should be thread-safe, as it can be invoked from ALT. */
	COREUOBJECT_API void AddPackageMappingFunc(TFunction<FName(FName)> InInstancedPackageMapFunc);
	
	/** Add a mapping from a top level asset path (/Path/To/Package.AssetName) to another. */
	COREUOBJECT_API void AddPathMapping(FSoftObjectPath Original, FSoftObjectPath Instanced);

	COREUOBJECT_API void AddTag(FName NewTag);
	COREUOBJECT_API void AppendTags(const TSet<FName>& NewTags);
	COREUOBJECT_API bool HasTag(FName Tag) const;
	COREUOBJECT_API void SetSoftObjectPathRemappingEnabled(bool bInSoftObjectPathRemappingEnabled);
	COREUOBJECT_API bool GetSoftObjectPathRemappingEnabled() const;

	UE_DEPRECATED(5.2, "No longer used, pass ELoadFlags::LOAD_RegenerateBulkDataGuids to LoadPackage instead")
	void SetRegenerateUniqueBulkDataGuids(bool bFlag) { }

	UE_DEPRECATED(5.2, "No longer used, check ELoadFlags::LOAD_RegenerateBulkDataGuids in the LoadFlags instead")
	bool ShouldRegenerateUniqueBulkDataGuids() const { return false; }

	/** Return the instanced package name for a given instanced outer package and an object package name */
	static FString GetInstancedPackageName(const FString& InOuterPackageName, const FString& InPackageName)
	{
		return FString::Printf(TEXT("%s_InstanceOf_%s"), *InOuterPackageName, *InPackageName);
	}

	COREUOBJECT_API void FixupSoftObjectPath(FSoftObjectPath& InOutSoftObjectPath) const;

private:
	void EnableAutomationTest();
	void BuildPackageMapping(FName Original, FName Instanced);
	bool FindPackageMapping(FName Original, FName& Instanced) const;

	friend class FLinkerLoad;
	friend struct FAsyncPackage2;
	friend class FLinkerInstancingContextTests;
	class FSharedLinkerInstancingContextData;

	TSharedPtr<FSharedLinkerInstancingContextData> SharedData;
};