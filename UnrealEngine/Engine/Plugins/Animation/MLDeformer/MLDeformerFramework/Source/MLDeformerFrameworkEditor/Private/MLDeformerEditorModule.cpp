// Copyright Epic Games, Inc. All Rights Reserved.

#include "MLDeformerEditorModule.h"
#include "MLDeformerEditorMode.h"
#include "MLDeformerModule.h"
#include "MLDeformerEditorModel.h"
#include "MLDeformerModel.h"
#include "MLDeformerCurveReferenceCustomization.h"
#include "MLDeformerGeomCacheTrainingInputAnimCustomize.h"
#include "SMLDeformerInputWidget.h"
#include "Modules/ModuleManager.h"
#include "EditorModeRegistry.h"
#include "PropertyEditorDelegates.h"

#define LOCTEXT_NAMESPACE "MLDeformerEditorModule"

IMPLEMENT_MODULE(UE::MLDeformer::FMLDeformerEditorModule, MLDeformerFrameworkEditor)

namespace UE::MLDeformer
{
	void FMLDeformerEditorModule::StartupModule()
	{
		FEditorModeRegistry::Get().RegisterMode<FMLDeformerEditorMode>(FMLDeformerEditorMode::ModeName, LOCTEXT("MLDeformerEditorMode", "MLDeformer"), FSlateIcon(), false);

		// Register object detail customizations.
		FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.RegisterCustomPropertyTypeLayout("MLDeformerCurveReference", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FMLDeformerCurveReferenceCustomization::MakeInstance) );
		PropertyModule.RegisterCustomPropertyTypeLayout("MLDeformerGeomCacheTrainingInputAnim", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FMLDeformerGeomCacheTrainingInputAnimCustomization::MakeInstance) );
		PropertyModule.NotifyCustomizationModuleChanged();

		SMLDeformerInputWidget::RegisterCommands();
	}

	void FMLDeformerEditorModule::ShutdownModule()
	{
		FEditorModeRegistry::Get().UnregisterMode(FMLDeformerEditorMode::ModeName);
		
		// Unregister object detail customizations.
		if (FModuleManager::Get().IsModuleLoaded(TEXT("PropertyEditor")))
		{
			FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
			PropertyModule.UnregisterCustomPropertyTypeLayout(TEXT("MLDeformerCurveReference"));
			PropertyModule.UnregisterCustomPropertyTypeLayout(TEXT("MLDeformerGeomCacheTrainingInputAnim"));
			PropertyModule.NotifyCustomizationModuleChanged();
		}
	}
}	// namespace UE::MLDeformer

#undef LOCTEXT_NAMESPACE
