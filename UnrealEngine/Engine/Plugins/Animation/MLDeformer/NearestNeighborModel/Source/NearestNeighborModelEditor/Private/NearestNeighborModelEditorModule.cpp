// Copyright Epic Games, Inc. All Rights Reserved.

#include "NearestNeighborModelVizSettingsDetails.h"
#include "NearestNeighborModelDetails.h"
#include "NearestNeighborEditorModel.h"
#include "NearestNeighborModel.h"
#include "MLDeformerEditorModule.h"
#include "Modules/ModuleManager.h"
#include "EditorModeRegistry.h"

#define LOCTEXT_NAMESPACE "NearestNeighborModelEditorModule"

namespace UE::NearestNeighborModel
{
	class FNearestNeighborModelEditorModule
		: public IModuleInterface
	{
	public:
		// IModuleInterface overrides.
		void StartupModule() override;
		void ShutdownModule() override;
		// ~END IModuleInterface overrides.
	};
}
IMPLEMENT_MODULE(UE::NearestNeighborModel::FNearestNeighborModelEditorModule, NearestNeighborModelEditor)

namespace UE::NearestNeighborModel
{
	using namespace UE::MLDeformer;

	void FNearestNeighborModelEditorModule::StartupModule()
	{
		// Register object detail customizations.
		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.RegisterCustomClassLayout("NearestNeighborModelVizSettings", FOnGetDetailCustomizationInstance::CreateStatic(&FNearestNeighborModelVizSettingsDetails::MakeInstance));
		PropertyModule.RegisterCustomClassLayout("NearestNeighborModel", FOnGetDetailCustomizationInstance::CreateStatic(&FNearestNeighborModelDetails::MakeInstance));
		PropertyModule.NotifyCustomizationModuleChanged();

		// Register our custom ML deformer model to the model registry in the ML Deformer Framework.
		FMLDeformerEditorModule& EditorModule = FModuleManager::LoadModuleChecked<FMLDeformerEditorModule>("MLDeformerFrameworkEditor");
		FMLDeformerEditorModelRegistry& ModelRegistry = EditorModule.GetModelRegistry();
		ModelRegistry.RegisterEditorModel(UNearestNeighborModel::StaticClass(), FOnGetEditorModelInstance::CreateStatic(&FNearestNeighborEditorModel::MakeInstance));
	}

	void FNearestNeighborModelEditorModule::ShutdownModule()
	{
		// Unregister our ML Deformer model.
		if (FModuleManager::Get().IsModuleLoaded(TEXT("MLDeformerFrameworkEditor")))
		{
			FMLDeformerEditorModule& EditorModule = FModuleManager::GetModuleChecked<FMLDeformerEditorModule>("MLDeformerFrameworkEditor");
			FMLDeformerEditorModelRegistry& ModelRegistry = EditorModule.GetModelRegistry();
			ModelRegistry.UnregisterEditorModel(UNearestNeighborModel::StaticClass());
		}

		// Unregister object detail customizations for this model.
		if (FModuleManager::Get().IsModuleLoaded(TEXT("PropertyEditor")))
		{
			FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
			PropertyModule.UnregisterCustomClassLayout(TEXT("NearestNeighborModelVizSettings"));
			PropertyModule.UnregisterCustomClassLayout(TEXT("NearestNeighborModel"));
			PropertyModule.NotifyCustomizationModuleChanged();
		}
	}

}	// namespace UE::NearestNeighborModel

#undef LOCTEXT_NAMESPACE
