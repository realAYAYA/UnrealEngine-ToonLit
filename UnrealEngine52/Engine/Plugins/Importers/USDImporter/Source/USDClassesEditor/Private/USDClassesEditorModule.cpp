// Copyright Epic Games, Inc. All Rights Reserved.

#include "USDClassesEditorModule.h"

#include "USDAssetCache2.h"
#include "USDAssetCacheAssetActions.h"
#include "USDDefaultAssetCacheDialog.h"

#include "AssetToolsModule.h"
#include "Modules/ModuleManager.h"
#include "Framework/Application/SlateApplication.h"
#include "Interfaces/IMainFrameModule.h"

#define LOCTEXT_NAMESPACE "USDClassesEditorModule"

UUsdAssetCache2* IUsdClassesEditorModule::ShowMissingDefaultAssetCacheDialog()
{
	TSharedPtr<SWindow> ParentWindow;

	if (FModuleManager::Get().IsModuleLoaded("MainFrame"))
	{
		IMainFrameModule& MainFrame = FModuleManager::LoadModuleChecked<IMainFrameModule>("MainFrame");
		ParentWindow = MainFrame.GetParentWindow();
	}

	FText WindowTitle = LOCTEXT("WindowTitle", "Set the default USD Asset Cache");

	TSharedRef<SWindow> Window = SNew(SWindow)
		.Title(WindowTitle)
		.SizingRule(ESizingRule::Autosized)
		.AdjustInitialSizeAndPositionForDPIScale(false);

	TSharedPtr<SUsdDefaultAssetCacheDialog> OptionsWindow;
	Window->SetContent
	(
		SAssignNew(OptionsWindow, SUsdDefaultAssetCacheDialog)
		.WidgetWindow(Window)
	);

	const bool bSlowTaskWindow = false;
	FSlateApplication::Get().AddModalWindow(Window, ParentWindow, bSlowTaskWindow);

	UUsdAssetCache2* NewCache = nullptr;
	if (OptionsWindow->UserAccepted())
	{
		NewCache = OptionsWindow->GetCreatedCache();
	}

	return NewCache;
}

class FUsdClassesEditorModule : public IUsdClassesEditorModule
{
public:
	virtual void StartupModule() override
	{
		// Register asset actions for the AssetCache asset
		IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
		AssetCacheAssetActions = MakeShared<FUsdAssetCacheAssetActions>();
		AssetTools.RegisterAssetTypeActions(AssetCacheAssetActions.ToSharedRef());
	}

	virtual void ShutdownModule() override
	{
		// Unregister asset actions for the AssetCache asset
		if (FAssetToolsModule* AssetToolsModule = FModuleManager::GetModulePtr<FAssetToolsModule>(TEXT("AssetTools")))
		{
			IAssetTools& AssetTools = AssetToolsModule->Get();
			AssetTools.UnregisterAssetTypeActions(AssetCacheAssetActions.ToSharedRef());
		}
	}

private:
	TSharedPtr<IAssetTypeActions> AssetCacheAssetActions;
};

IMPLEMENT_MODULE( FUsdClassesEditorModule, USDClassesEditor );

#undef LOCTEXT_NAMESPACE

