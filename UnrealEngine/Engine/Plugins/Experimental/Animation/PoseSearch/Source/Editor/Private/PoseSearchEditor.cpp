// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoseSearchEditor.h"
#include "PoseSearchCustomization.h"
#include "PoseSearchDebugger.h"
#include "PoseSearchTypeActions.h"
#include "PoseSearchDatabaseEdMode.h"
#include "PoseSearchDatabaseEditorCommands.h"

#include "Animation/AnimSequence.h"
#include "Modules/ModuleManager.h"
#include "AssetToolsModule.h"
#include "Editor.h"
#include "PropertyEditorModule.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "IAnimationEditor.h"
#include "IPersonaToolkit.h"
#include "IPersonaPreviewScene.h"
#include "Trace/PoseSearchTraceAnalyzer.h"
#include "Trace/PoseSearchTraceModule.h"

DEFINE_LOG_CATEGORY(LogPoseSearchEditor);

#define LOCTEXT_NAMESPACE "PoseSearchEditorModule"

//////////////////////////////////////////////////////////////////////////
// FEditorCommands

namespace UE::PoseSearch
{

namespace FEditorCommands
{

void DrawSearchIndex()
{
	FDebugDrawParams DrawParams;
	DrawParams.DefaultLifeTime = 60.0f;
	EnumAddFlags(DrawParams.Flags, EDebugDrawFlags::DrawSearchIndex);

	TArray<UObject*> EditedAssets = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->GetAllEditedAssets();
	for (UObject* EditedAsset : EditedAssets)
	{
		if (UAnimSequence* Sequence = Cast<UAnimSequence>(EditedAsset))
		{
			const UPoseSearchSequenceMetaData* MetaData = Sequence->FindMetaDataByClass<UPoseSearchSequenceMetaData>();
			if (MetaData)
			{
				DrawParams.SequenceMetaData = MetaData;
				IAssetEditorInstance* EditorInstance = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->FindEditorForAsset(Sequence, true /*bFocusIfOpen*/);
				if (EditorInstance && EditorInstance->GetEditorName() == TEXT("AnimationEditor"))
				{
					IAnimationEditor* Editor = static_cast<IAnimationEditor*>(EditorInstance);
					TSharedRef<IPersonaToolkit> Toolkit = Editor->GetPersonaToolkit();
					TSharedRef<IPersonaPreviewScene> Scene = Toolkit->GetPreviewScene();

					DrawParams.World = Scene->GetWorld();
					DrawSearchIndex(DrawParams);
				}
			}
		}
	}
}

} // namespace FEditorCommands

//////////////////////////////////////////////////////////////////////////
// FPoseSearchEditorModule

class FEditorModule : public IPoseSearchEditorModuleInterface
{
public:
	void StartupModule() override;
	void ShutdownModule() override;

private:
	TArray<class IConsoleObject*> ConsoleCommands;
	
	/** Creates the view for the Rewind Debugger */
	TSharedPtr<FDebuggerViewCreator> DebuggerViewCreator;
	/** Enables dedicated PoseSearch trace module */
	TSharedPtr<FTraceModule> TraceModule;
	
private:
	void RegisterPropertyTypeCustomizations();
	void RegisterObjectCustomizations();
	void UnregisterCustomizations();
	void RegisterCustomClassLayout(FName ClassName, FOnGetDetailCustomizationInstance DetailLayoutDelegate);
	void RegisterCustomPropertyTypeLayout(FName PropertyTypeName, FOnGetPropertyTypeCustomizationInstance PropertyTypeLayoutDelegate);

private:
	/** List of registered class that we must unregister when the module shuts down */
	TSet<FName> RegisteredClassNames;
	TSet<FName> RegisteredPropertyTypes;

	TSharedPtr<IAssetTypeActions> PoseSearchDatabaseActions;
	TSharedPtr<IAssetTypeActions> PoseSearchSchemaActions;
};

void FEditorModule::StartupModule()
{
	// Register Asset Editor Commands
	FDatabaseEditorCommands::Register();

	if (GIsEditor && !IsRunningCommandlet())
	{
		FDebugger::Initialize();
		TraceModule = MakeShared<FTraceModule>();
		DebuggerViewCreator = MakeShared<FDebuggerViewCreator>();

		IModularFeatures::Get().RegisterModularFeature("RewindDebuggerViewCreator", DebuggerViewCreator.Get());
		IModularFeatures::Get().RegisterModularFeature(TraceServices::ModuleFeatureName, TraceModule.Get());
		
		ConsoleCommands.Add(IConsoleManager::Get().RegisterConsoleCommand(
			TEXT("a.PoseSearch.DrawSearchIndex"),
			TEXT("Draw the search index for the selected asset"),
			FConsoleCommandDelegate::CreateStatic(&FEditorCommands::DrawSearchIndex),
			ECVF_Default
		));

		// Register Ed Mode used by pose search database
		FEditorModeRegistry::Get().RegisterMode<FDatabaseEdMode>(
			FDatabaseEdMode::EdModeId,
			LOCTEXT("PoseSearchDatabaseEdModeName", "PoseSearchDatabase"));

		// Register UPoseSearchDatabase Type Actions 
		IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
		PoseSearchDatabaseActions = MakeShared<FDatabaseTypeActions>();
		AssetTools.RegisterAssetTypeActions(PoseSearchDatabaseActions.ToSharedRef());

		// Register UPoseSearchSchema Type Actions 
		PoseSearchSchemaActions = MakeShared<FSchemaTypeActions>();
		AssetTools.RegisterAssetTypeActions(PoseSearchSchemaActions.ToSharedRef());
	}

	RegisterPropertyTypeCustomizations();
	RegisterObjectCustomizations();

	FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.NotifyCustomizationModuleChanged();

}

void FEditorModule::ShutdownModule()
{
	for (IConsoleObject* ConsoleCmd : ConsoleCommands)
	{
		IConsoleManager::Get().UnregisterConsoleObject(ConsoleCmd);
	}
	ConsoleCommands.Empty();

	if (FModuleManager::Get().IsModuleLoaded("AssetTools"))
	{
		IAssetTools& AssetTools = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get();
		AssetTools.UnregisterAssetTypeActions(PoseSearchDatabaseActions.ToSharedRef());
		AssetTools.UnregisterAssetTypeActions(PoseSearchSchemaActions.ToSharedRef());
	}

	// Unregister Ed Mode
	FEditorModeRegistry::Get().UnregisterMode(FDatabaseEdMode::EdModeId);
	
	UnregisterCustomizations();

	// Unregister Asset Editor Commands
	FDatabaseEditorCommands::Unregister();
	
	IModularFeatures::Get().UnregisterModularFeature(TraceServices::ModuleFeatureName, TraceModule.Get());
	FDebugger::Shutdown();
}

void FEditorModule::RegisterCustomClassLayout(FName ClassName, FOnGetDetailCustomizationInstance DetailLayoutDelegate)
{
	check(ClassName != NAME_None);

	RegisteredClassNames.Add(ClassName);

	static FName PropertyEditor("PropertyEditor");
	FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>(PropertyEditor);
	PropertyModule.RegisterCustomClassLayout(ClassName, DetailLayoutDelegate);
}


void FEditorModule::RegisterCustomPropertyTypeLayout(FName PropertyTypeName, FOnGetPropertyTypeCustomizationInstance PropertyTypeLayoutDelegate)
{
	check(PropertyTypeName != NAME_None);

	RegisteredPropertyTypes.Add(PropertyTypeName);

	static FName PropertyEditor("PropertyEditor");
	FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>(PropertyEditor);
	PropertyModule.RegisterCustomPropertyTypeLayout(PropertyTypeName, PropertyTypeLayoutDelegate);
}


void FEditorModule::RegisterPropertyTypeCustomizations()
{
	RegisterCustomPropertyTypeLayout("PoseSearchDatabaseSequence", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FPoseSearchDatabaseSequenceCustomization::MakeInstance));
}

void FEditorModule::RegisterObjectCustomizations()
{
	RegisterCustomClassLayout("PoseSearchDatabase", FOnGetDetailCustomizationInstance::CreateStatic(&FPoseSearchDatabaseDetails::MakeInstance));
}

void FEditorModule::UnregisterCustomizations()
{
	if (FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

		// Unregister all classes customized by name
		for (auto It = RegisteredClassNames.CreateConstIterator(); It; ++It)
		{
			if (It->IsValid())
			{
				PropertyModule.UnregisterCustomClassLayout(*It);
			}
		}

		// Unregister all structures
		for (auto It = RegisteredPropertyTypes.CreateConstIterator(); It; ++It)
		{
			if (It->IsValid())
			{
				PropertyModule.UnregisterCustomPropertyTypeLayout(*It);
			}
		}

		PropertyModule.NotifyCustomizationModuleChanged();
	}
}


} // namespace UE::PoseSearch

IMPLEMENT_MODULE(UE::PoseSearch::FEditorModule, PoseSearchEditor);

#undef LOCTEXT_NAMESPACE
