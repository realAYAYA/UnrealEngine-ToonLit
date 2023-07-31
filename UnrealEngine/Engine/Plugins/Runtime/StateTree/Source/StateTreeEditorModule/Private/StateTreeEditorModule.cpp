// Copyright Epic Games, Inc. All Rights Reserved.

#include "StateTreeEditorModule.h"
#include "AssetToolsModule.h"
#include "AssetTypeActions_StateTree.h"
#include "Blueprint/StateTreeConditionBlueprintBase.h"
#include "Blueprint/StateTreeEvaluatorBlueprintBase.h"
#include "Blueprint/StateTreeTaskBlueprintBase.h"
#include "Customizations/StateTreeAnyEnumDetails.h"
#include "Customizations/StateTreeEditorDataDetails.h"
#include "Customizations/StateTreeEditorNodeDetails.h"
#include "Customizations/StateTreeReferenceDetails.h"
#include "Customizations/StateTreeStateDetails.h"
#include "Customizations/StateTreeStateLinkDetails.h"
#include "Customizations/StateTreeStateParametersDetails.h"
#include "Customizations/StateTreeTransitionDetails.h"
#include "IAssetTools.h"
#include "Modules/ModuleManager.h"
#include "StateTree.h"
#include "StateTreeCompiler.h"
#include "StateTreeCompilerLog.h"
#include "StateTreeConditionBase.h"
#include "StateTreeDelegates.h"
#include "StateTreeEditor.h"
#include "StateTreeEditorCommands.h"
#include "StateTreeEditorStyle.h"
#include "StateTreeEvaluatorBase.h"
#include "StateTreeNodeClassCache.h"
#include "StateTreeTaskBase.h"

#define LOCTEXT_NAMESPACE "StateTreeEditor"

DEFINE_LOG_CATEGORY(LogStateTreeEditor);

IMPLEMENT_MODULE(FStateTreeEditorModule, StateTreeEditorModule)

namespace UE::StateTree::Editor
{
	// @todo Could we make this a IModularFeature?
	static bool CompileStateTree(UStateTree& StateTree)
	{
		// Compile the StateTree asset.
		UE::StateTree::Editor::ValidateAsset(StateTree);
		const uint32 EditorDataHash = UE::StateTree::Editor::CalcAssetHash(StateTree);

		FStateTreeCompilerLog Log;
		FStateTreeCompiler Compiler(Log);

		const bool bSuccess = Compiler.Compile(StateTree);

		if (bSuccess)
		{
			// Success
			StateTree.LastCompiledEditorDataHash = EditorDataHash;
			UE::StateTree::Delegates::OnPostCompile.Broadcast(StateTree);
			UE_LOG(LogStateTreeEditor, Log, TEXT("Compile StateTree '%s' succeeded."), *StateTree.GetFullName());
		}
		else
		{
			// Make sure not to leave stale data on failed compile.
			StateTree.ResetCompiled();
			StateTree.LastCompiledEditorDataHash = 0;

			UE_LOG(LogStateTreeEditor, Error, TEXT("Failed to compile '%s', errors follow."), *StateTree.GetFullName());
			Log.DumpToLog(LogStateTreeEditor);
		}

		return bSuccess;
	}

}; // UE::StateTree::Editor

void FStateTreeEditorModule::StartupModule()
{
	UE::StateTree::Delegates::OnRequestCompile.BindStatic(&UE::StateTree::Editor::CompileStateTree);

	MenuExtensibilityManager = MakeShareable(new FExtensibilityManager);
	ToolBarExtensibilityManager = MakeShareable(new FExtensibilityManager);

	FStateTreeEditorStyle::Initialize();
	FStateTreeEditorCommands::Register();

	// Register asset types
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

	// Create or find existing category
	const EAssetTypeCategories::Type AICategory = AssetTools.RegisterAdvancedAssetCategory(FName(TEXT("AI")), LOCTEXT("AIAssetCategory", "Artificial Intelligence"));

	TSharedPtr<FAssetTypeActions_StateTree> StateTreeAssetTypeAction = MakeShareable(new FAssetTypeActions_StateTree(AICategory | EAssetTypeCategories::Gameplay));
	ItemDataAssetTypeActions.Add(StateTreeAssetTypeAction);
	AssetTools.RegisterAssetTypeActions(StateTreeAssetTypeAction.ToSharedRef());

	// Register the details customizer
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.RegisterCustomPropertyTypeLayout("StateTreeTransition", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FStateTreeTransitionDetails::MakeInstance));
	PropertyModule.RegisterCustomPropertyTypeLayout("StateTreeStateLink", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FStateTreeStateLinkDetails::MakeInstance));
	PropertyModule.RegisterCustomPropertyTypeLayout("StateTreeEditorNode", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FStateTreeEditorNodeDetails::MakeInstance));
	PropertyModule.RegisterCustomPropertyTypeLayout("StateTreeStateParameters", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FStateTreeStateParametersDetails::MakeInstance));
	PropertyModule.RegisterCustomPropertyTypeLayout("StateTreeAnyEnum", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FStateTreeAnyEnumDetails::MakeInstance));
	PropertyModule.RegisterCustomPropertyTypeLayout("StateTreeReference", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FStateTreeReferenceDetails::MakeInstance));
	PropertyModule.RegisterCustomClassLayout("StateTreeState", FOnGetDetailCustomizationInstance::CreateStatic(&FStateTreeStateDetails::MakeInstance));
	PropertyModule.RegisterCustomClassLayout("StateTreeEditorData", FOnGetDetailCustomizationInstance::CreateStatic(&FStateTreeEditorDataDetails::MakeInstance));

	PropertyModule.NotifyCustomizationModuleChanged();
}

void FStateTreeEditorModule::ShutdownModule()
{
	UE::StateTree::Delegates::OnRequestCompile.Unbind();
	
	MenuExtensibilityManager.Reset();
	ToolBarExtensibilityManager.Reset();

	FStateTreeEditorStyle::Shutdown();
	FStateTreeEditorCommands::Unregister();

	// Unregister the data asset type actions
	if (FModuleManager::Get().IsModuleLoaded("AssetTools"))
	{
		IAssetTools& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get();
		for (int i = 0; i < ItemDataAssetTypeActions.Num(); i++)
		{
			if (ItemDataAssetTypeActions[i].IsValid())
			{
				AssetToolsModule.UnregisterAssetTypeActions(ItemDataAssetTypeActions[i].ToSharedRef());
			}
		}
	}
	ItemDataAssetTypeActions.Empty();

	// Unregister the details customization
	if (FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.UnregisterCustomPropertyTypeLayout("StateTreeTransition");
		PropertyModule.UnregisterCustomPropertyTypeLayout("StateTreeStateLink");
		PropertyModule.UnregisterCustomPropertyTypeLayout("StateTreeEditorNode");
		PropertyModule.UnregisterCustomPropertyTypeLayout("StateTreeStateParameters");
		PropertyModule.UnregisterCustomPropertyTypeLayout("StateTreeAnyEnum");
		PropertyModule.NotifyCustomizationModuleChanged();
	}

}

TSharedRef<IStateTreeEditor> FStateTreeEditorModule::CreateStateTreeEditor(const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, UStateTree* StateTree)
{
	TSharedRef<FStateTreeEditor> NewEditor(new FStateTreeEditor());
	NewEditor->InitEditor(Mode, InitToolkitHost, StateTree);
	return NewEditor;
}

TSharedPtr<FStateTreeNodeClassCache> FStateTreeEditorModule::GetNodeClassCache()
{
	if (!NodeClassCache.IsValid())
	{
		NodeClassCache = MakeShareable(new FStateTreeNodeClassCache());
		NodeClassCache->AddRootScriptStruct(FStateTreeEvaluatorBase::StaticStruct());
		NodeClassCache->AddRootScriptStruct(FStateTreeTaskBase::StaticStruct());
		NodeClassCache->AddRootScriptStruct(FStateTreeConditionBase::StaticStruct());
		NodeClassCache->AddRootClass(UStateTreeEvaluatorBlueprintBase::StaticClass());
		NodeClassCache->AddRootClass(UStateTreeTaskBlueprintBase::StaticClass());
		NodeClassCache->AddRootClass(UStateTreeConditionBlueprintBase::StaticClass());
		NodeClassCache->AddRootClass(UStateTreeSchema::StaticClass());
	}

	return NodeClassCache;
}


#undef LOCTEXT_NAMESPACE
