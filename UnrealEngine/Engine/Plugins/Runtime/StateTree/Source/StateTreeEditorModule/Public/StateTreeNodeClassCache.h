// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/WeakObjectPtr.h"
#include "StateTreeNodeClassCache.generated.h"

struct FAssetData;

/**
 * Describes a class or struct.
 * If the class or struct is from a package that is not yet loaded, the data will update on GetStruct/Class/Scripstruct()
 */
USTRUCT()
struct STATETREEEDITORMODULE_API FStateTreeNodeClassData
{
	GENERATED_BODY()

	FStateTreeNodeClassData() {}
	FStateTreeNodeClassData(UStruct* InStruct);
	FStateTreeNodeClassData(const FString& InClassAssetName, const FString& InClassPackage, const FName InStructName, UStruct* InStruct);

	FName GetStructName() const { return StructName; }

	UStruct* GetStruct(bool bSilent = false);

	UClass* GetClass(bool bSilent = false)
	{
		return Cast<UClass>(GetStruct(bSilent));
	}
	UScriptStruct* GetScriptStruct(bool bSilent = false)
	{
		return Cast<UScriptStruct>(GetStruct(bSilent));
	}

private:

	/** Pointer to described struct or class. */
	TWeakObjectPtr<UStruct> Struct;

	/** Resolved name of struct or class. */
	FName StructName;

	/** Path to class if it's not loaded yet. */
	FString ClassAssetName;

	/** Package of the asset if it's not loaded yet. */
	FString ClassPackageName;
};

/**
 * Caches specified classes or structs and reacts to engine events to keep the lists always up to date.
 * All the derived classes or structs are kept in the cache.
 */
struct STATETREEEDITORMODULE_API FStateTreeNodeClassCache
{
	FStateTreeNodeClassCache();
	~FStateTreeNodeClassCache();

	/** Adds a Struct to keep track of */
	void AddRootStruct(UStruct* RootStruct);
	
	/** Adds a Class to keep track of */
	void AddRootClass(UClass* RootClass)
	{
		AddRootStruct(RootClass);
	}
	
	/** Adds a ScriptStruct to keep track of */
	void AddRootScriptStruct(UScriptStruct* RootStruct)
	{
		AddRootStruct(RootStruct);
	}

	/** Returns know derived Structs based on provided base. If the base Struct is not added as root Struct, nothing is returned. */
	void GetStructs(UStruct* BaseStruct, TArray<TSharedPtr<FStateTreeNodeClassData>>& AvailableClasses);
	
	/** Returns know derived Classes based on provided base. If the base Class is not added as root Class, nothing is returned. */
	void GetClasses(UStruct* BaseClass, TArray<TSharedPtr<FStateTreeNodeClassData>>& AvailableClasses)
	{
		GetStructs(BaseClass, AvailableClasses);
	}
	
	/** Returns know derived ScriptStructs based on provided base. If the base struct is not added as root ScriptStruct, nothing is returned. */
	void GetScripStructs(UScriptStruct* BaseStruct, TArray<TSharedPtr<FStateTreeNodeClassData>>& AvailableClasses)
	{
		GetStructs(BaseStruct, AvailableClasses);
	}

	/** Invalidates the cache, it will be rebuild on next access. */
	void InvalidateCache();

protected:
	void OnAssetAdded(const FAssetData& AssetData);
	void OnAssetRemoved(const FAssetData& AssetData);
	void OnReloadComplete(EReloadCompleteReason Reason);
	
private:
	void UpdateBlueprintClass(const FAssetData& AssetData);
	void CacheClasses();

	struct FRootClassContainer
	{
		FRootClassContainer() = default;
		FRootClassContainer(UStruct* InBaseStruct)
		{
			BaseStruct = InBaseStruct;
		} 
		TWeakObjectPtr<UStruct> BaseStruct;
		TArray<TSharedPtr<FStateTreeNodeClassData>> ClassData;
		bool bUpdated = false;
	};

	TArray<FRootClassContainer> RootClasses;
	TMap<FString, int32> RootClassNameToIndex;
};
