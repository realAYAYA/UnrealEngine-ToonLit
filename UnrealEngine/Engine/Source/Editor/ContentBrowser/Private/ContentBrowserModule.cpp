// Copyright Epic Games, Inc. All Rights Reserved.


#include "ContentBrowserModule.h"

#include "ContentBrowserDataSubsystem.h"
#include "ContentBrowserLog.h"
#include "ContentBrowserSingleton.h"
#include "HAL/PlatformMath.h"
#include "IContentBrowserDataModule.h"
#include "IContentBrowserSingleton.h"
#include "Logging/LogMacros.h"
#include "MRUFavoritesList.h"
#include "Misc/AssertionMacros.h"
#include "Modules/ModuleManager.h"
#include "Settings/ContentBrowserSettings.h"
#include "UObject/UObjectGlobals.h"
#include "Editor.h"
#include "Subsystems/AssetEditorSubsystem.h"

IMPLEMENT_MODULE( FContentBrowserModule, ContentBrowser );
DEFINE_LOG_CATEGORY(LogContentBrowser);
const FName FContentBrowserModule::NumberOfRecentAssetsName(TEXT("NumObjectsInRecentList"));

void FContentBrowserModule::StartupModule()
{
	// Ensure the data module is loaded
	IContentBrowserDataModule::Get();

	ContentBrowserSingleton = new FContentBrowserSingleton();
	
	UContentBrowserSettings::OnSettingChanged().AddRaw(this, &FContentBrowserModule::ContentBrowserSettingChanged);
}

void FContentBrowserModule::ShutdownModule()
{	
	if ( ContentBrowserSingleton )
	{
		delete ContentBrowserSingleton;
		ContentBrowserSingleton = NULL;
	}
	UContentBrowserSettings::OnSettingChanged().RemoveAll(this);
}

IContentBrowserSingleton& FContentBrowserModule::Get() const
{
	check(ContentBrowserSingleton);
	return *ContentBrowserSingleton;
}

FMainMRUFavoritesList* FContentBrowserModule::GetRecentlyOpenedAssets() const
{
	return GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->GetRecentlyOpenedAssets();
}

FDelegateHandle FContentBrowserModule::AddAssetViewExtraStateGenerator(const FAssetViewExtraStateGenerator& Generator)
{
	AssetViewExtraStateGenerators.Add(Generator);
	return Generator.Handle;
}

void FContentBrowserModule::RemoveAssetViewExtraStateGenerator(const FDelegateHandle& GeneratorHandle)
{
	AssetViewExtraStateGenerators.RemoveAll([&GeneratorHandle](const FAssetViewExtraStateGenerator& Generator) { return Generator.Handle == GeneratorHandle; });
}

void FContentBrowserModule::ContentBrowserSettingChanged(FName InName)
{
	if (UContentBrowserDataSubsystem* ContentBrowserData = IContentBrowserDataModule::Get().GetSubsystem())
	{
		ContentBrowserData->RefreshVirtualPathTreeIfNeeded();
	}

	ContentBrowserSingleton->SetPrivateContentPermissionListDirty();

	OnContentBrowserSettingChanged.Broadcast(InName);
}
