// Copyright Epic Games, Inc. All Rights Reserved.

#include "SpatializationEditorModule.h"
#include "Features/IModularFeatures.h"
#include "ITDSpatializationSourceSettingsFactory.h"


namespace
{
	static const FName AssetToolsName = TEXT("AssetTools");

	template <typename T>
	void AddAssetAction(IAssetTools& AssetTools, TArray<TSharedPtr<FAssetTypeActions_Base>>& AssetActions)
	{
		TSharedPtr<T> AssetAction = MakeShared<T>();
		TSharedPtr<FAssetTypeActions_Base> AssetActionBase = StaticCastSharedPtr<FAssetTypeActions_Base>(AssetAction);
		AssetTools.RegisterAssetTypeActions(AssetAction.ToSharedRef());
		AssetActions.Add(AssetActionBase);
	}
} // namespace <>


void FSpatializationEditorModule::StartupModule()
{
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(AssetToolsName).Get();
	AddAssetAction<FAssetTypeActions_ITDSpatializationSettings>(AssetTools, AssetActions);
}

void FSpatializationEditorModule::ShutdownModule()
{
	AssetActions.Reset();
}

IMPLEMENT_MODULE(FSpatializationEditorModule, SpatializationEditor)