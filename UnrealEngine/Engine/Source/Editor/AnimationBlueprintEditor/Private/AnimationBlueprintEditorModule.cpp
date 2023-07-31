// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimationBlueprintEditorModule.h"

#include "Animation/AnimInstance.h"
#include "AnimationBlueprintEditor.h"
#include "AnimationBlueprintEditorSettings.h"
#include "AnimationGraphFactory.h"
#include "BlueprintEditor.h"
#include "Delegates/Delegate.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraphSchema_K2.h"
#include "EdGraphUtilities.h"
#include "Engine/Blueprint.h"
#include "HAL/Platform.h"
#include "ISettingsModule.h"
#include "Internationalization/Internationalization.h"
#include "K2Node_CallFunction.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "MessageLogInitializationOptions.h"
#include "MessageLogModule.h"
#include "Misc/AssertionMacros.h"
#include "Modules/ModuleManager.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "UObject/Class.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ObjectPtr.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/WeakObjectPtr.h"

class IAnimationBlueprintEditor;
class IToolkitHost;

IMPLEMENT_MODULE( FAnimationBlueprintEditorModule, AnimationBlueprintEditor);

#define LOCTEXT_NAMESPACE "AnimationBlueprintEditorModule"

void FAnimationBlueprintEditorModule::StartupModule()
{
	MenuExtensibilityManager = MakeShareable(new FExtensibilityManager);
	ToolBarExtensibilityManager = MakeShareable(new FExtensibilityManager);

	AnimGraphNodeFactory = MakeShareable(new FAnimationGraphNodeFactory());
	FEdGraphUtilities::RegisterVisualNodeFactory(AnimGraphNodeFactory);

	AnimGraphPinFactory = MakeShareable(new FAnimationGraphPinFactory());
	FEdGraphUtilities::RegisterVisualPinFactory(AnimGraphPinFactory);

	AnimGraphPinConnectionFactory = MakeShareable(new FAnimationGraphPinConnectionFactory());
	FEdGraphUtilities::RegisterVisualPinConnectionFactory(AnimGraphPinConnectionFactory);

	FKismetEditorUtilities::RegisterOnBlueprintCreatedCallback(this, UAnimInstance::StaticClass(), FKismetEditorUtilities::FOnBlueprintCreated::CreateRaw(this, &FAnimationBlueprintEditorModule::OnNewBlueprintCreated));
	
	FMessageLogModule& MessageLogModule = FModuleManager::LoadModuleChecked<FMessageLogModule>("MessageLog");
	FMessageLogInitializationOptions InitOptions;
	InitOptions.bShowFilters = true;
	InitOptions.bShowPages = true;
	MessageLogModule.RegisterLogListing("AnimBlueprintLog", LOCTEXT("AnimBlueprintLog", "Anim Blueprint Log"), InitOptions);

	ISettingsModule& SettingsModule = FModuleManager::LoadModuleChecked<ISettingsModule>("Settings");
	SettingsModule.RegisterSettings("Editor", "ContentEditors", "AnimationBlueprintEditor",
		LOCTEXT("AnimationBlueprintEditorSettingsName", "Animation Blueprint Editor"),
		LOCTEXT("AnimationBlueprintEditorSettingsDescription", "Configure the look and feel of the Animation Blueprint Editor."),
		GetMutableDefault<UAnimationBlueprintEditorSettings>());

}

void FAnimationBlueprintEditorModule::ShutdownModule()
{
	FKismetEditorUtilities::UnregisterAutoBlueprintNodeCreation(this);

	FEdGraphUtilities::UnregisterVisualNodeFactory(AnimGraphNodeFactory);
	FEdGraphUtilities::UnregisterVisualPinFactory(AnimGraphPinFactory);
	FEdGraphUtilities::UnregisterVisualPinConnectionFactory(AnimGraphPinConnectionFactory);

	ISettingsModule& SettingsModule = FModuleManager::LoadModuleChecked<ISettingsModule>("Settings");
	SettingsModule.UnregisterSettings("Editor", "ContentEditors", "AnimationBlueprintEditor");

	MenuExtensibilityManager.Reset();
	ToolBarExtensibilityManager.Reset();
}

TSharedRef<IAnimationBlueprintEditor> FAnimationBlueprintEditorModule::CreateAnimationBlueprintEditor( const EToolkitMode::Type Mode, const TSharedPtr< IToolkitHost >& InitToolkitHost, class UAnimBlueprint* InAnimBlueprint)
{
	TSharedRef< FAnimationBlueprintEditor > NewAnimationBlueprintEditor( new FAnimationBlueprintEditor() );
	NewAnimationBlueprintEditor->InitAnimationBlueprintEditor( Mode, InitToolkitHost, InAnimBlueprint);
	return NewAnimationBlueprintEditor;
}

void FAnimationBlueprintEditorModule::OnNewBlueprintCreated(UBlueprint* InBlueprint)
{
	if (InBlueprint->UbergraphPages.Num() > 0)
	{
		UEdGraph* EventGraph = InBlueprint->UbergraphPages[0];

		int32 SafeXPosition = 0;
		int32 SafeYPosition = 0;

		if (EventGraph->Nodes.Num() != 0)
		{
			SafeXPosition = EventGraph->Nodes[0]->NodePosX;
			SafeYPosition = EventGraph->Nodes[EventGraph->Nodes.Num() - 1]->NodePosY + EventGraph->Nodes[EventGraph->Nodes.Num() - 1]->NodeHeight + 100;
		}

		// add try get owner node
		UK2Node_CallFunction* GetOwnerNode = NewObject<UK2Node_CallFunction>(EventGraph);
		UFunction* MakeNodeFunction = UAnimInstance::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UAnimInstance, TryGetPawnOwner));
		GetOwnerNode->CreateNewGuid();
		GetOwnerNode->PostPlacedNewNode();
		GetOwnerNode->SetFromFunction(MakeNodeFunction);
		GetOwnerNode->SetFlags(RF_Transactional);
		GetOwnerNode->AllocateDefaultPins();
		GetOwnerNode->NodePosX = SafeXPosition;
		GetOwnerNode->NodePosY = SafeYPosition;
		UEdGraphSchema_K2::SetNodeMetaData(GetOwnerNode, FNodeMetadata::DefaultGraphNode);
		GetOwnerNode->MakeAutomaticallyPlacedGhostNode();

		EventGraph->AddNode(GetOwnerNode);
	}
}

#undef LOCTEXT_NAMESPACE
