// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Templates/Tuple.h"

//@TODO: Debug printing
#define UE_ASSET_DOMAIN_FILTERING_DEBUG_LOGGING 0

#if UE_ASSET_DOMAIN_FILTERING_DEBUG_LOGGING
DECLARE_LOG_CATEGORY_EXTERN(LogAssetReferenceRestrictions, Verbose, All);
#else
DECLARE_LOG_CATEGORY_EXTERN(LogAssetReferenceRestrictions, Log, Log);
#endif

class IPlugin;
struct FDomainPathNode;
struct FAssetData;

struct FDomainData final : public TSharedFromThis<FDomainData>
{
	FText UserFacingDomainName;
	FText ErrorMessageIfUsedElsewhere;

	// The list of root paths, always of the format /Mount/ or /Mount/Path/To/ with both leading and trailing /
	TArray<FString> DomainRootPaths;

	// A list of specific packages that are part of this domain
	TArray<FName> SpecificAssetPackages;

	// The domains that are visible from here (if bCanSeeEverything is true, then literally everything is visible from here)
	TSet<TSharedPtr<FDomainData>> DomainsVisibleFromHere;

	// Can we see everything?
	bool bCanSeeEverything = false;

	// Can we be seen by everything?
	bool bCanBeSeenByEverything = false;

	bool IsValid() const
	{
		return DomainRootPaths.Num() > 0 || SpecificAssetPackages.Num() > 0;
	}

	void Reset()
	{
		UserFacingDomainName = FText::GetEmpty();
		DomainRootPaths.Reset();
		DomainsVisibleFromHere.Reset();
		bCanSeeEverything = false;
	}
};

struct FDomainDatabase final
{
	FDomainDatabase();
	~FDomainDatabase();

	void Init();

	void MarkDirty();
	void UpdateIfNecessary();

	void ValidateAllDomains();
	void DebugPrintAllDomains();

	void OnPluginCreatedOrMounted(IPlugin& NewPlugin);

	TSharedPtr<FDomainData> FindDomainFromAssetData(const FAssetData& AssetData) const;

	TTuple<bool, FText> CanDomainsSeeEachOther(TSharedPtr<FDomainData> Referencee, TSharedPtr<FDomainData> Referencer) const;

	const TArray<FString>& GetDomainsDefinedByPlugins() const { return DomainsDefinedByPlugins; }

private:
	void RebuildFromScratch();
	TSharedPtr<FDomainData> FindOrAddDomainByName(const FString& Name);
	void BuildDomainFromPlugin(TSharedRef<IPlugin> Plugin);

	void AddDomainVisibilityList(TSharedPtr<FDomainData> Domain, const TArray<FString>& VisibilityList);
private:
	// Map from domain name to 
	TMap<FString, TSharedPtr<FDomainData>> DomainNameMap;

	// Map from path to domain
	TSharedPtr<FDomainPathNode> PathMap;

	// Specific packages that are forced into certain domains
	TMap<FName, TSharedPtr<FDomainData>> SpecificAssetPackageDomains;

	// The engine content domain
	TSharedPtr<FDomainData> EngineDomain;

	// The default script domain (for reflected types)
	TSharedPtr<FDomainData> ScriptDomain;

	// Used for various 'special' mount points like /Temp/, /Memory/, and /Extra/
	// Not visible as a domain for other domains to see, and can see everything
	TSharedPtr<FDomainData> TempDomain;

	// The game content domain
	TSharedPtr<FDomainData> GameDomain;

	// The never cooked content domain
	TSharedPtr<FDomainData> NeverCookDomain;

	// List of domains that came from plugins (used for domain pickers in the settings)
	TArray<FString> DomainsDefinedByPlugins;

	bool bDatabaseOutOfDate = false;
};