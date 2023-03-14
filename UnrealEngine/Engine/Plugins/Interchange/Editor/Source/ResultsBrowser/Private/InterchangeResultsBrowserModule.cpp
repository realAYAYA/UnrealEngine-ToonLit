// Copyright Epic Games, Inc. All Rights Reserved.

#include "InterchangeResultsBrowserModule.h"

#include "CoreMinimal.h"
#include "Engine/Engine.h"
#include "Framework/Docking/TabManager.h"
#include "InterchangeResultsBrowserStyle.h"
#include "InterchangeManager.h"
#include "SInterchangeResultsBrowserWindow.h"
#include "Styling/SlateIconFinder.h"
#include "Widgets/Docking/SDockTab.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"


#define LOCTEXT_NAMESPACE "InterchangeResultsBrowser"

class FInterchangeResultsBrowserModule : public IInterchangeResultsBrowserModule
{
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:

	TSharedRef<SDockTab> SpawnInterchangeResults(const FSpawnTabArgs& Args);
	void OpenErrorBrowser(TStrongObjectPtr<UInterchangeResultsContainer> InResultsContainer);

	/** Pointer to the style set to use for the UI. */
	TSharedPtr<ISlateStyle> InterchangeResultsBrowserStyle = nullptr;

	/** Owning pointer to the InterchangeResultsContainer being displayed */
	TStrongObjectPtr<UInterchangeResultsContainer> ResultsContainer;

	/** Pointer to the Slate widget */
	TSharedPtr<SInterchangeResultsBrowserWindow> InterchangeResultsBrowserWindow;

	/** Whether the results list is filtered */
	// @TODO: load this from config
	bool bIsFiltered = false;
};


IMPLEMENT_MODULE(FInterchangeResultsBrowserModule, InterchangeResultsBrowser)


void FInterchangeResultsBrowserModule::StartupModule()
{
	auto RegisterItems = [this]()
	{
		UInterchangeManager& InterchangeManager = UInterchangeManager::GetInterchangeManager();
		InterchangeManager.OnBatchImportComplete.AddRaw(this, &FInterchangeResultsBrowserModule::OpenErrorBrowser);

		auto UnregisterItems = [this]()
		{
			UInterchangeManager& InterchangeManager = UInterchangeManager::GetInterchangeManager();
			InterchangeManager.OnBatchImportComplete.RemoveAll(this);
		};

		InterchangeManager.OnPreDestroyInterchangeManager.AddLambda(UnregisterItems);
	};

	if (GEngine)
	{
		RegisterItems();
	}
	else
	{
		FCoreDelegates::OnPostEngineInit.AddLambda(RegisterItems);
	}
	
	if (!InterchangeResultsBrowserStyle.IsValid())
	{
		InterchangeResultsBrowserStyle = MakeShared<FInterchangeResultsBrowserStyle>();
	}

	FGlobalTabmanager::Get()->RegisterNomadTabSpawner("InterchangeResults", FOnSpawnTab::CreateRaw(this, &FInterchangeResultsBrowserModule::SpawnInterchangeResults))
		.SetDisplayName(LOCTEXT("InterchangeResultsBrowser", "Interchange Results Browser"))
		.SetTooltipText(LOCTEXT("InterchangeResultsBrowserTooltipText", "Open the Interchange Results Browser tab."))
		.SetGroup(WorkspaceMenu::GetMenuStructure().GetDeveloperToolsLogCategory())
		.SetIcon(FSlateIconFinder::FindIcon("InterchangeResultsBrowser.TabIcon"));
}


void FInterchangeResultsBrowserModule::ShutdownModule()
{
	InterchangeResultsBrowserStyle = nullptr;
}


TSharedRef<SDockTab> FInterchangeResultsBrowserModule::SpawnInterchangeResults(const FSpawnTabArgs& Args)
{
	TSharedRef<SDockTab> DockTab = SNew(SDockTab)
		.TabRole(ETabRole::NomadTab);

	DockTab->SetContent(
		SAssignNew(InterchangeResultsBrowserWindow, SInterchangeResultsBrowserWindow)
		.OwnerTab(DockTab)
		.InterchangeResultsContainer(ResultsContainer.Get())
		.IsFiltered(bIsFiltered)
		.OnFilterChangedState_Lambda([this](bool bNewState) { this->bIsFiltered = bNewState; })
	);

	return DockTab;
}


void FInterchangeResultsBrowserModule::OpenErrorBrowser(TStrongObjectPtr<UInterchangeResultsContainer> InResultsContainer)
{
	// Only showing when we have errors or warnings for now
	const bool bShouldShow = [&InResultsContainer]()
	{
		for (UInterchangeResult* Result : InResultsContainer->GetResults())
		{
			if (Result->GetResultType() != EInterchangeResultType::Success)
			{
				return true;
			}
		}

		return false;
	}();

	if (GIsAutomationTesting || !bShouldShow)
	{
		return;
	}

	ResultsContainer = InResultsContainer;
	ResultsContainer->Finalize();
	FGlobalTabmanager::Get()->TryInvokeTab(FName("InterchangeResults"));
	InterchangeResultsBrowserWindow->Set(ResultsContainer.Get());
}

#undef LOCTEXT_NAMESPACE
