// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions/AssetTypeActions_ModifierHierarchyAsset.h"
#include "AssetTypeActions/AssetTypeActions_ModifierBoundWidgetStylesAsset.h"

#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

namespace UE::VCamExtensionsEditor::Private
{
	class FVCamExtensionsEditorModule : public IModuleInterface
	{
	public:
		
		virtual void StartupModule() override
		{
			SetupAssetTypeActions();
		}
		
		virtual void ShutdownModule() override
		{
			CleanUpAssetTypeActions();
		}

	private:

		TSharedPtr<FAssetTypeActions_ModifierBoundWidgetStylesAsset> MetaDataActions;
		TSharedPtr<FAssetTypeActions_ModifierHierarchyAsset> HierarchyActions;
		
		void SetupAssetTypeActions()
		{
			MetaDataActions = MakeShared<FAssetTypeActions_ModifierBoundWidgetStylesAsset>();
			HierarchyActions = MakeShared<FAssetTypeActions_ModifierHierarchyAsset>();
			
			IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
			AssetTools.RegisterAssetTypeActions(MetaDataActions.ToSharedRef());
			AssetTools.RegisterAssetTypeActions(HierarchyActions.ToSharedRef());
		}
		
		void CleanUpAssetTypeActions()
		{
			if (IModuleInterface* AssetToolsPtr = FModuleManager::Get().GetModule("AssetTools"))
			{
				IAssetTools& AssetTools = static_cast<FAssetToolsModule*>(AssetToolsPtr)->Get();
				AssetTools.UnregisterAssetTypeActions(MetaDataActions.ToSharedRef());
				AssetTools.UnregisterAssetTypeActions(HierarchyActions.ToSharedRef());
			}

			MetaDataActions.Reset();
			HierarchyActions.Reset();
		}
	};
}

IMPLEMENT_MODULE(UE::VCamExtensionsEditor::Private::FVCamExtensionsEditorModule, VCamExtensionsEditor);