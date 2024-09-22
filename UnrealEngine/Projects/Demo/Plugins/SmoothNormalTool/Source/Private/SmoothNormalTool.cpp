// Copyright Epic Games, Inc. All Rights Reserved.

#include "SmoothNormalTool.h"
#include "SmoothNormalCommand.h"
#include "Misc/MessageDialog.h"
#include "ToolMenus.h"
#include "ContentBrowserModule.h"

#define LOCTEXT_NAMESPACE "FSmoothNormalToolModule"

//static FDelegateHandle HookEditorUIContentBrowserEmptyExtenderDelegateHandle;
static FContentBrowserMenuExtender_SelectedAssets HookEditorUIContentBrowserExtenderDelegate;
static FDelegateHandle HookEditorUIContentBrowserExtenderDelegateHandle;

static void CreateActionsMenuForAsset(FMenuBuilder& MenuBuilder, TArray<FAssetData> SelectedAssets)
{
	MenuBuilder.AddMenuEntry(
		FText::FromName(TEXT("Smooth normal")),
		FText::FromName(TEXT("Smooth normal")),
		FSlateIcon(),
		FExecuteAction::CreateStatic(&FSmoothNormalCommand::SmoothNormal, SelectedAssets)
	);
}

static void CreateSubMenuForAsset(FMenuBuilder& MenuBuilder, TArray<FAssetData> SelectedAssets)
{
	MenuBuilder.AddSubMenu(
		FText::FromName(TEXT("Toon Tool")),
		FText::FromName(TEXT("ToonTool")),
		FNewMenuDelegate::CreateStatic(&CreateActionsMenuForAsset, SelectedAssets)
	);
}

static TSharedRef<FExtender> OnExtendContentBrowserAssetSelectionMenu(const TArray<FAssetData>& SelectedAssets)
{
	TSharedRef<FExtender> Extender(new FExtender());
	
	// 过滤资产类型
	bool NeedShow = false;

	for (const FAssetData& Asset : SelectedAssets)
	{
		const FString& ClassName = Asset.AssetClassPath.ToString();
		if (ClassName.Contains("StaticMesh") || ClassName.Contains("SkeletalMesh"))
		{
			NeedShow = true;
			break;
		}
	}

	if (!NeedShow)
		return Extender;
	
	Extender->AddMenuExtension(
		"CommonAssetActions",
		EExtensionHook::First,
		nullptr,
		FMenuExtensionDelegate::CreateStatic(&CreateSubMenuForAsset, SelectedAssets));
	
	return Extender;
}

void FSmoothNormalToolModule::StartupModule()
{
	{
		// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
		FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
		HookEditorUIContentBrowserExtenderDelegate = FContentBrowserMenuExtender_SelectedAssets::CreateStatic(&OnExtendContentBrowserAssetSelectionMenu);
		TArray<FContentBrowserMenuExtender_SelectedAssets>& CBMenuExtenderDelegates = ContentBrowserModule.GetAllAssetViewContextMenuExtenders();
		CBMenuExtenderDelegates.Add(HookEditorUIContentBrowserExtenderDelegate);
		HookEditorUIContentBrowserExtenderDelegateHandle = CBMenuExtenderDelegates.Last().GetHandle();
	}
}

void FSmoothNormalToolModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.

	UToolMenus::UnRegisterStartupCallback(this);

	UToolMenus::UnregisterOwner(this);
	
}

void FSmoothNormalToolModule::PluginButtonClicked()
{
	// Put your "OnButtonClicked" stuff here
	const FText DialogText = FText::Format(
		LOCTEXT("PluginButtonDialogText", "Add code to {0} in {1} to override this button's actions"),
		FText::FromString(TEXT("FSmoothNormalToolModule::PluginButtonClicked()")),
		FText::FromString(TEXT("SmoothNormalTool.cpp"))
		);
	FMessageDialog::Open(EAppMsgType::Ok, DialogText);
}

void FSmoothNormalToolModule::RegisterMenus()
{
	// Owner will be used for cleanup in call to UToolMenus::UnregisterOwner
	
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FSmoothNormalToolModule, SmoothNormalTool)