// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "IAssetSearchModule.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "WorkspaceMenuStructure.h"
#include "Styling/AppStyle.h"
#include "WorkspaceMenuStructureModule.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/SSearchBrowser.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetSearchManager.h"

#define LOCTEXT_NAMESPACE "FAssetSearchModule"

static const FName SearchTabName("Search");

class FAssetSearchModule : public IAssetSearchModule
{
public:

	// IAssetSearchModule interface


public:

	// IModuleInterface interface
	
	virtual void StartupModule() override
	{
		SearchManager = MakeUnique<FAssetSearchManager>();
		SearchManager->Start();

		FGlobalTabmanager::Get()->RegisterNomadTabSpawner(SearchTabName, FOnSpawnTab::CreateRaw(this, &FAssetSearchModule::HandleSpawnSettingsTab))
			.SetDisplayName(LOCTEXT("Search", "Search"))
			.SetTooltipText(LOCTEXT("SearchTab", "Search Tab"))
			.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "Symbols.SearchGlass"))
			.SetGroup(WorkspaceMenu::GetMenuStructure().GetToolsCategory());
	}

	virtual void ShutdownModule() override
	{
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(SearchTabName);
	}

	void ExecuteOpenObjectBrowser()
	{
		FGlobalTabmanager::Get()->TryInvokeTab(SearchTabName);
	}

	TSharedRef<SDockTab> HandleSpawnSettingsTab(const FSpawnTabArgs& SpawnTabArgs)
	{
		const TSharedRef<SDockTab> DockTab = SNew(SDockTab)
			.TabRole(ETabRole::NomadTab);

		DockTab->SetContent(SNew(SSearchBrowser));

		return DockTab;
	}

	virtual FSearchStats GetStats() const override
	{
		return SearchManager->GetStats();
	}

	virtual void Search(FSearchQueryPtr SearchQuery) override
	{
		SearchManager->Search(SearchQuery);
	}

	virtual void ForceIndexOnAssetsMissingIndex() override
	{
		SearchManager->ForceIndexOnAssetsMissingIndex();
	}

	virtual void RegisterAssetIndexer(const UClass* InAssetClass, TUniquePtr<IAssetIndexer>&& Indexer) override
	{
		SearchManager->RegisterAssetIndexer(InAssetClass, MoveTemp(Indexer));
	}

	virtual void RegisterSearchProvider(FName SearchProviderName, TUniquePtr<ISearchProvider>&& InSearchProvider) override
	{
		SearchManager->RegisterSearchProvider(SearchProviderName, MoveTemp(InSearchProvider));
	}

private:
	TUniquePtr<FAssetSearchManager> SearchManager;
};

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FAssetSearchModule, AssetSearch);
