// Copyright Epic Games, Inc. All Rights Reserved.

#include "Integrations/AdvancedRenamerContentBrowserIntegration.h"
#include "ContentBrowserDelegates.h"
#include "ContentBrowserModule.h"
#include "Delegates/IDelegateInstance.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/MultiBox/MultiBoxExtender.h"
#include "IAdvancedRenamerModule.h"
#include "Providers/AdvancedRenamerAssetProvider.h"

#define LOCTEXT_NAMESPACE "AdvancedRenamerContentBrowserIntegration"

namespace UE::AdvancedRenamer::Private
{
	FDelegateHandle ContentBrowserDelegateHandle;

	void OpenAdvancedRenamer(const TArray<FAssetData> AssetArray)
	{
		TSharedRef<FAdvancedRenamerAssetProvider> AssetProvider = MakeShared<FAdvancedRenamerAssetProvider>();
		AssetProvider->SetAssetList(AssetArray);

		TSharedPtr<SWidget> HostWidget = nullptr;

		IAdvancedRenamerModule::Get().OpenAdvancedRenamer(StaticCastSharedRef<IAdvancedRenamerProvider>(AssetProvider), HostWidget);
	}

	void ExtendAssetMenu(FMenuBuilder& MenuBuilder, const TArray<FAssetData> AssetArray)
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("AdvancedRename", "Advanced Rename"),
			LOCTEXT("AdvancedRenameTooltip", "Opens the Advanced Renamer Panel to rename all selected assets."),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "ContentBrowser.AssetActions.Rename"),
			FUIAction(FExecuteAction::CreateStatic(&OpenAdvancedRenamer, AssetArray))
		);
	}

	TSharedRef<FExtender> CreateContentMenuExtender(const TArray<FAssetData>& AssetArray)
	{
		TSharedRef<FExtender> Extender = MakeShared<FExtender>();

		if (AssetArray.Num() > 0)
		{
			Extender->AddMenuExtension(
				"CommonAssetActions",
				EExtensionHook::Position::After,
				nullptr,
				FMenuExtensionDelegate::CreateStatic(&ExtendAssetMenu, AssetArray)
			);
		}

		return Extender;
	}
}

void FAdvancedRenamerContentBrowserIntegration::Initialize()
{
	using namespace UE::AdvancedRenamer::Private;

	// Register Content Browser selection extensions
	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
	TArray<FContentBrowserMenuExtender_SelectedAssets>& AssetContextMenuDelegates = ContentBrowserModule.GetAllAssetViewContextMenuExtenders();
	AssetContextMenuDelegates.Add(FContentBrowserMenuExtender_SelectedAssets::CreateStatic(&CreateContentMenuExtender));
	ContentBrowserDelegateHandle = AssetContextMenuDelegates.Last().GetHandle();
}

void FAdvancedRenamerContentBrowserIntegration::Shutdown()
{
	using namespace UE::AdvancedRenamer::Private;

	if (ContentBrowserDelegateHandle.IsValid())
	{
		FContentBrowserModule* ContentBrowserModule = FModuleManager::GetModulePtr<FContentBrowserModule>("ContentBrowser");

		if (ContentBrowserModule)
		{
			TArray<FContentBrowserMenuExtender_SelectedAssets>& AssetContextMenuDelegates = ContentBrowserModule->GetAllAssetViewContextMenuExtenders();
			AssetContextMenuDelegates.RemoveAll([DelegateHandle = ContentBrowserDelegateHandle](const FContentBrowserMenuExtender_SelectedAssets& Delegate)
				{
					return Delegate.GetHandle() == DelegateHandle;
				});
		}

		ContentBrowserDelegateHandle.Reset();
	}
}

#undef LOCTEXT_NAMESPACE
