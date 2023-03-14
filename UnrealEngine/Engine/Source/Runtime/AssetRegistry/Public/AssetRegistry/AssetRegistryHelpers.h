// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Containers/Array.h"
#include "Containers/Set.h"
#include "Containers/UnrealString.h"
#include "CoreMinimal.h"
#include "UObject/NameTypes.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ScriptInterface.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/UObjectGlobals.h"

#include "AssetRegistryHelpers.generated.h"

class IAssetRegistry;
class UClass;
struct FFrame;

USTRUCT(BlueprintType)
struct FTagAndValue
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Transient, Category = AssetData)
	FName Tag;

	UPROPERTY(BlueprintReadWrite, Transient, Category = AssetData)
	FString Value;
};

UCLASS(transient)
class UAssetRegistryHelpers : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Asset Registry")
	static TScriptInterface<IAssetRegistry> GetAssetRegistry();

	/** 
	 * Creates asset data from a UObject. 
	 *
	 * @param InAsset	The asset to create asset data for
	 * @param bAllowBlueprintClass	By default trying to create asset data for a blueprint class will create one for the UBlueprint instead
	 */
	UFUNCTION(BlueprintPure, Category = "Asset Data")
	static FAssetData CreateAssetData(const UObject* InAsset, bool bAllowBlueprintClass = false);

	/** Checks to see if this AssetData refers to an asset or is NULL */
	UFUNCTION(BlueprintPure, Category = "Asset Data", meta=(DisplayName = "Is Valid Asset Data", ScriptMethod))
	static bool IsValid(const FAssetData& InAssetData);

	/** Returns true if this is the primary asset in a package, true for maps and assets but false for secondary objects like class redirectors */
	UFUNCTION(BlueprintPure, Category = "Asset Data", meta=(ScriptMethod))
	static bool IsUAsset(const FAssetData& InAssetData);

	/** Returns true if the this asset is a redirector. */
	UFUNCTION(BlueprintPure, Category = "Asset Data", meta=(ScriptMethod))
	static bool IsRedirector(const FAssetData& InAssetData);

	/** Returns the full name for the asset in the form: Class ObjectPath */
	UFUNCTION(BlueprintPure, Category = "Asset Data", meta=(ScriptMethod))
	static FString GetFullName(const FAssetData& InAssetData);

	/** Convert to a SoftObjectPath for loading */
	UFUNCTION(BlueprintPure, Category = "Asset Data", meta=(ScriptMethod))
	static FSoftObjectPath ToSoftObjectPath(const FAssetData& InAssetData);

	UFUNCTION(BlueprintPure, Category = "Asset Data", meta=(ScriptMethod))
	static UClass* GetClass(const FAssetData& InAssetData);

	/** Returns the asset UObject if it is loaded or loads the asset if it is unloaded then returns the result */
	UFUNCTION(BlueprintPure, Category = "Asset Data", meta=(ScriptMethod))
	static UObject* GetAsset(const FAssetData& InAssetData);

	/** Returns true if the asset is loaded */
	UFUNCTION(BlueprintPure, Category = "Asset Data", meta=(ScriptMethod))
	static bool IsAssetLoaded(const FAssetData& InAssetData);

	/** Returns the name for the asset in the form: Class'ObjectPath' */
	UFUNCTION(BlueprintPure, Category = "Asset Data", meta=(ScriptMethod))
	static FString GetExportTextName(const FAssetData& InAssetData);

	/** Gets the value associated with the given tag as a string */
	UFUNCTION(BlueprintPure, Category = "Asset Data", meta=(ScriptMethod))
	static bool GetTagValue(const FAssetData& InAssetData, const FName& InTagName, FString& OutTagValue);

	/**
	 * Populates the FARFilters tags and values map with the passed in tags and values
	 */
	UFUNCTION(BlueprintPure, Category = "Asset Registry")
	static FARFilter SetFilterTagsAndValues(const FARFilter& InFilter, const TArray<FTagAndValue>& InTagsAndValues);

	/** Gets asset data for all blueprint assets that match the filter. ClassPaths in the filter specify the blueprint's parent class. */
	UFUNCTION(BlueprintPure, Category = "Asset Registry", meta=(ScriptMethod))
	static void GetBlueprintAssets(const FARFilter& InFilter, TArray<FAssetData>& OutAssetData);

	/** Enable/disable asset registry caching mode for the duration of the scope */
	struct ASSETREGISTRY_API FTemporaryCachingModeScope
	{
		FTemporaryCachingModeScope(bool InTempCachingMode);
		~FTemporaryCachingModeScope();

	private:
		bool PreviousCachingMode;
	};

	/** Checks to see if the given asset data is a blueprint with a base class in the ClassNameSet. This checks the parent asset tag */
	static bool ASSETREGISTRY_API IsAssetDataBlueprintOfClassSet(const FAssetData& AssetData, const TSet<FTopLevelAssetPath>& ClassNameSet);

};
