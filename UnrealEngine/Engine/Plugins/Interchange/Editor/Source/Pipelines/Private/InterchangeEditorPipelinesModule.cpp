// Copyright Epic Games, Inc. All Rights Reserved.

#include "InterchangeEditorPipelinesModule.h"

#include "AssetToolsModule.h"
#include "CoreMinimal.h"
#include "Engine/Engine.h"
#include "InterchangeEditorPipelineDetails.h"
#include "InterchangeEditorPipelineStyle.h"
#include "InterchangeManager.h"
#include "InterchangePipelineBase.h"
#include "InterchangePipelineFactories.h"
#include "InterchangePythonPipelineBase.h"
#include "Misc/CoreDelegates.h"
#include "Modules/ModuleManager.h"
#include "Nodes/InterchangeBaseNode.h"
#include "PropertyEditorModule.h"



#define LOCTEXT_NAMESPACE "InterchangeEditorPipelines"

class FInterchangeEditorPipelinesModule : public IInterchangeEditorPipelinesModule
{
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	//Called when we start the module
	void AcquireResources();
	
	//Can be called multiple time
	void ReleaseResources();

	TSharedRef<FPropertySection> RegisterPropertySection(FPropertyEditorModule& PropertyModule, FName ClassName, FName SectionName, FText DisplayName);
	void RegisterPropertySectionMappings();
	void UnregisterPropertySectionMappings();

private:
	/** Pointer to the style set to use for the UI. */
	TSharedPtr<ISlateStyle> InterchangeEditorPipelineStyle = nullptr;

	TMultiMap<FName, FName> RegisteredPropertySections;
};

IMPLEMENT_MODULE(FInterchangeEditorPipelinesModule, InterchangeEditorPipelines)

void FInterchangeEditorPipelinesModule::StartupModule()
{
	AcquireResources();
}

void FInterchangeEditorPipelinesModule::ShutdownModule()
{
	ReleaseResources();
}

void FInterchangeEditorPipelinesModule::AcquireResources()
{
	auto RegisterItems = [this]()
	{
		UInterchangeManager& InterchangeManager = UInterchangeManager::GetInterchangeManager();

		RegisterPropertySectionMappings();
	};

	if (GEngine)
	{
		RegisterItems();
	}
	else
	{
		FCoreDelegates::OnPostEngineInit.AddLambda(RegisterItems);
	}

	ClassesToUnregisterOnShutdown.Reset();
	// Register details customizations for animation controller nodes
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

	ClassesToUnregisterOnShutdown.Add(UInterchangeBaseNode::StaticClass()->GetFName());
	PropertyEditorModule.RegisterCustomClassLayout(ClassesToUnregisterOnShutdown.Last(), FOnGetDetailCustomizationInstance::CreateStatic(&FInterchangeBaseNodeDetailsCustomization::MakeInstance));
	
	ClassesToUnregisterOnShutdown.Add(UInterchangePipelineBase::StaticClass()->GetFName());
	PropertyEditorModule.RegisterCustomClassLayout(ClassesToUnregisterOnShutdown.Last(), FOnGetDetailCustomizationInstance::CreateStatic(&FInterchangePipelineBaseDetailsCustomization::MakeInstance));


	if (!InterchangeEditorPipelineStyle.IsValid())
	{
		InterchangeEditorPipelineStyle = MakeShared<FInterchangeEditorPipelineStyle>();
	}

	// Register the InterchangeImportTestPlan asset
	FAssetToolsModule& AssetToolsModule = FAssetToolsModule::GetModule();
	IAssetTools& AssetTools = AssetToolsModule.Get();
	
	BlueprintPipelineBase_TypeActions = MakeShared<FAssetTypeActions_InterchangeBlueprintPipelineBase>();
	AssetTools.RegisterAssetTypeActions(BlueprintPipelineBase_TypeActions.ToSharedRef());

	PipelineBase_TypeActions = MakeShared<FAssetTypeActions_InterchangePipelineBase>();
	AssetTools.RegisterAssetTypeActions(PipelineBase_TypeActions.ToSharedRef());

	PythonPipelineBase_TypeActions = MakeShared<FAssetTypeActions_InterchangePythonPipelineBase>();
	AssetTools.RegisterAssetTypeActions(PythonPipelineBase_TypeActions.ToSharedRef());

	FCoreDelegates::OnPreExit.AddLambda([this]()
		{
			//We must release the resources before the application start to unload modules
			ReleaseResources();
		});
}

void FInterchangeEditorPipelinesModule::ReleaseResources()
{
	FPropertyEditorModule* PropertyEditorModule = FModuleManager::GetModulePtr<FPropertyEditorModule>("PropertyEditor");
	if (PropertyEditorModule)
	{
		for (FName ClassName : ClassesToUnregisterOnShutdown)
		{
			PropertyEditorModule->UnregisterCustomClassLayout(ClassName);
		}
	}
	ClassesToUnregisterOnShutdown.Empty();

	UnregisterPropertySectionMappings();

	InterchangeEditorPipelineStyle = nullptr;

	FAssetToolsModule* AssetToolsModule = FModuleManager::GetModulePtr<FAssetToolsModule>("AssetTools");
	if (AssetToolsModule)
	{
		IAssetTools& AssetTools = AssetToolsModule->Get();
		AssetTools.UnregisterAssetTypeActions(BlueprintPipelineBase_TypeActions.ToSharedRef());
		AssetTools.UnregisterAssetTypeActions(PipelineBase_TypeActions.ToSharedRef());
		AssetTools.UnregisterAssetTypeActions(PythonPipelineBase_TypeActions.ToSharedRef());
	}
}

TSharedRef<FPropertySection> FInterchangeEditorPipelinesModule::RegisterPropertySection(FPropertyEditorModule& PropertyModule, FName ClassName, FName SectionName, FText DisplayName)
{
	TSharedRef<FPropertySection> PropertySection = PropertyModule.FindOrCreateSection(ClassName, SectionName, DisplayName);
	RegisteredPropertySections.Add(ClassName, SectionName);

	return PropertySection;
}

void FInterchangeEditorPipelinesModule::RegisterPropertySectionMappings()
{
	const FName PropertyEditorModuleName("PropertyEditor");
	FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>(PropertyEditorModuleName);

	// Assets
	{
		TSharedRef<FPropertySection> Section = RegisterPropertySection(PropertyModule, "InterchangeGenericAssetsPipeline", "General", LOCTEXT("General", "General"));
		Section->AddCategory("Common");
	}

	// Materials
	{
		TSharedRef<FPropertySection> Section = RegisterPropertySection(PropertyModule, "InterchangeGenericMaterialPipeline", "Materials", LOCTEXT("Materials", "Materials"));
		Section->AddCategory("Materials");
	}

	// Skeletal Meshes
	{
		TSharedRef<FPropertySection> Section = RegisterPropertySection(PropertyModule, "InterchangeGenericMeshPipeline", "SkeletalMeshes", LOCTEXT("Skeletal Meshes", "Skeletal Meshes"));
		Section->AddCategory("Common Meshes");
		Section->AddCategory("Common Skeletal Meshes and Animations");
		Section->AddCategory("Skeletal Meshes");
	}

	// Static Meshes
	{
		TSharedRef<FPropertySection> Section = RegisterPropertySection(PropertyModule, "InterchangeGenericMeshPipeline", "StaticMeshes", LOCTEXT("Static Meshes", "Static Meshes"));
		Section->AddCategory("Common Meshes");
		Section->AddCategory("Static Meshes");
	}

	// Animation
	{
		TSharedRef<FPropertySection> Section = RegisterPropertySection(PropertyModule, "InterchangeGenericAnimationPipeline", "AnimationSequences", LOCTEXT("Animation Sequences", "Animation Sequences"));
		Section->AddCategory("Common Skeletal Meshes and Animations");
		Section->AddCategory("Animations");
	}

	// Textures
	{
		TSharedRef<FPropertySection> Section = RegisterPropertySection(PropertyModule, "InterchangeGenericTexturePipeline", "Textures", LOCTEXT("Textures", "Textures"));
		Section->AddCategory("Textures");
	}
}

void FInterchangeEditorPipelinesModule::UnregisterPropertySectionMappings()
{
	const FName PropertyEditorModuleName("PropertyEditor");
	FPropertyEditorModule* PropertyModule = FModuleManager::GetModulePtr<FPropertyEditorModule>(PropertyEditorModuleName);

	if (!PropertyModule)
	{
		return;
	}

	for (TMultiMap<FName, FName>::TIterator PropertySectionIterator = RegisteredPropertySections.CreateIterator(); PropertySectionIterator; ++PropertySectionIterator)
	{
		PropertyModule->RemoveSection(PropertySectionIterator->Key, PropertySectionIterator->Value);
		PropertySectionIterator.RemoveCurrent();
	}

	RegisteredPropertySections.Empty();
}

#undef LOCTEXT_NAMESPACE

