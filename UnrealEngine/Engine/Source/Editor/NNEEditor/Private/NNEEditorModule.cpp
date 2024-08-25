// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modules/ModuleManager.h"
#include "NNEEditorModelDataActions.h"

class FNNEEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		ModelDataAssetTypeActions = MakeShared<UE::NNEEditor::Private::FModelDataAssetTypeActions>();
		FAssetToolsModule::GetModule().Get().RegisterAssetTypeActions(ModelDataAssetTypeActions.ToSharedRef());
	}

	virtual void ShutdownModule() override
	{
		if (FModuleManager::Get().IsModuleLoaded("AssetTools"))
		{
			FAssetToolsModule::GetModule().Get().UnregisterAssetTypeActions(ModelDataAssetTypeActions.ToSharedRef());
		}
	}

private:
	TSharedPtr<UE::NNEEditor::Private::FModelDataAssetTypeActions> ModelDataAssetTypeActions;
};

IMPLEMENT_MODULE(FNNEEditorModule, NNEEditor)