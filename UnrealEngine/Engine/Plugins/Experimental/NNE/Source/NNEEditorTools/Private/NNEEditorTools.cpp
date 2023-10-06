// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modules/ModuleManager.h"
#include "NNEEditorToolsModelDataActions.h"

class FNNEEditorToolsModule : public IModuleInterface
{
public:

	virtual void StartupModule() override
	{
		ModelDataAssetTypeActions = MakeShared<UE::NNEEditorTools::Private::FModelDataAssetTypeActions>();
		FAssetToolsModule::GetModule().Get().RegisterAssetTypeActions(ModelDataAssetTypeActions.ToSharedRef());
	}

	virtual void ShutdownModule() override
	{
		if (FModuleManager::Get().IsModuleLoaded("AssetTools"))
		{
			FAssetToolsModule::GetModule().Get().UnregisterAssetTypeActions(ModelDataAssetTypeActions.ToSharedRef());
		}
	}

	TSharedPtr<UE::NNEEditorTools::Private::FModelDataAssetTypeActions> ModelDataAssetTypeActions;
};

IMPLEMENT_MODULE(FNNEEditorToolsModule, NNEEditorTools)