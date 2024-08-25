// Copyright Epic Games, Inc. All Rights Reserved.

#include "USDClassesEditorModule.h"

#include "USDAssetCache2.h"
#include "USDAssetCacheAssetActions.h"
#include "USDDefaultAssetCacheDialog.h"

#include "AssetToolsModule.h"
#include "Framework/Application/SlateApplication.h"
#include "Interfaces/IMainFrameModule.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "USDClassesEditorModule"

UUsdAssetCache2* IUsdClassesEditorModule::ShowMissingDefaultAssetCacheDialog()
{
	UUsdAssetCache2* Result = nullptr;
	EDefaultAssetCacheDialogOption Unused = ShowMissingDefaultAssetCacheDialog(Result);
	return Result;
}

void IUsdClassesEditorModule::ShowMissingDefaultAssetCacheDialog(UUsdAssetCache2*& OutCreatedCache, bool& bOutUserAccepted)
{
	EDefaultAssetCacheDialogOption Result = ShowMissingDefaultAssetCacheDialog(OutCreatedCache);
	bOutUserAccepted = Result != EDefaultAssetCacheDialogOption::Cancel;
}

EDefaultAssetCacheDialogOption IUsdClassesEditorModule::ShowMissingDefaultAssetCacheDialog(UUsdAssetCache2*& OutCreatedCache)
{
	TSharedPtr<SWindow> ParentWindow;

	if (FModuleManager::Get().IsModuleLoaded("MainFrame"))
	{
		IMainFrameModule& MainFrame = FModuleManager::LoadModuleChecked<IMainFrameModule>("MainFrame");
		ParentWindow = MainFrame.GetParentWindow();
	}

	FText WindowTitle = LOCTEXT("WindowTitle", "Set the default USD Asset Cache");

	TSharedRef<SWindow> Window = SNew(SWindow).Title(WindowTitle).SizingRule(ESizingRule::Autosized).AdjustInitialSizeAndPositionForDPIScale(false);

	TSharedPtr<SUsdDefaultAssetCacheDialog> OptionsWindow;
	Window->SetContent(SAssignNew(OptionsWindow, SUsdDefaultAssetCacheDialog).WidgetWindow(Window));

	const bool bSlowTaskWindow = false;
	FSlateApplication::Get().AddModalWindow(Window, ParentWindow, bSlowTaskWindow);

	OutCreatedCache = OptionsWindow->GetCreatedCache();
	return OptionsWindow->GetDialogOutcome();
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

IMPLEMENT_MODULE(FUsdClassesEditorModule, USDClassesEditor);

#undef LOCTEXT_NAMESPACE
