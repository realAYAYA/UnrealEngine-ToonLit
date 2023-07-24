// Copyright Epic Games, Inc. All Rights Reserved.

#include "MLDeformerEditorModule.h"
#include "MLDeformerAssetActions.h"
#include "MLDeformerEditorMode.h"
#include "MLDeformerModule.h"
#include "MLDeformerEditorModel.h"
#include "MLDeformerModel.h"
#include "MLDeformerCurveReferenceCustomization.h"
#include "MLDeformerMorphModelQualityLevelCustomization.h"
#include "Modules/ModuleManager.h"
#include "EditorModeRegistry.h"
#include "PropertyEditorDelegates.h"

#define LOCTEXT_NAMESPACE "MLDeformerEditorModule"

IMPLEMENT_MODULE(UE::MLDeformer::FMLDeformerEditorModule, MLDeformerFrameworkEditor)

namespace UE::MLDeformer
{
	void FMLDeformerEditorModule::StartupModule()
	{
		MLDeformerAssetActions = MakeShareable(new FMLDeformerAssetActions);
		FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get().RegisterAssetTypeActions(MLDeformerAssetActions.ToSharedRef());
		FEditorModeRegistry::Get().RegisterMode<FMLDeformerEditorMode>(FMLDeformerEditorMode::ModeName, LOCTEXT("MLDeformerEditorMode", "MLDeformer"), FSlateIcon(), false);

		// Register object detail customizations.
		FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.RegisterCustomPropertyTypeLayout("MLDeformerCurveReference", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FMLDeformerCurveReferenceCustomization::MakeInstance) );
		PropertyModule.RegisterCustomPropertyTypeLayout("MLDeformerMorphModelQualityLevel", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FMLDeformerMorphModelQualityLevelCustomization::MakeInstance) );
		PropertyModule.NotifyCustomizationModuleChanged();
	}

	void FMLDeformerEditorModule::ShutdownModule()
	{
		FEditorModeRegistry::Get().UnregisterMode(FMLDeformerEditorMode::ModeName);
		if (MLDeformerAssetActions.IsValid())
		{
			if (FModuleManager::Get().IsModuleLoaded("AssetTools"))
			{
				FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get().UnregisterAssetTypeActions(MLDeformerAssetActions.ToSharedRef());
			}
			MLDeformerAssetActions.Reset();
		}

		// Unregister object detail customizations.
		if (FModuleManager::Get().IsModuleLoaded(TEXT("PropertyEditor")))
		{
			FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
			PropertyModule.UnregisterCustomPropertyTypeLayout(TEXT("MLDeformerCurveReference"));
			PropertyModule.UnregisterCustomPropertyTypeLayout(TEXT("MLDeformerMorphModelQualityLevel"));
			PropertyModule.NotifyCustomizationModuleChanged();
		}
	}
}	// namespace UE::MLDeformer

#undef LOCTEXT_NAMESPACE
