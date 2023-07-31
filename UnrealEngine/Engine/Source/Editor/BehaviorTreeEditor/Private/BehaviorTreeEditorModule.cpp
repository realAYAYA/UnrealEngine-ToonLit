// Copyright Epic Games, Inc. All Rights Reserved.

#include "BehaviorTreeEditorModule.h"

#include "AIGraphTypes.h"
#include "AssetToolsModule.h"
#include "AssetTypeActions_BehaviorTree.h"
#include "AssetTypeActions_Blackboard.h"
#include "BehaviorTree/BTNode.h"
#include "BehaviorTree/Decorators/BTDecorator_BlueprintBase.h"
#include "BehaviorTree/Services/BTService_BlueprintBase.h"
#include "BehaviorTree/Tasks/BTTask_BlueprintBase.h"
#include "BehaviorTreeDecoratorGraphNode_Decorator.h"
#include "BehaviorTreeEditor.h"
#include "BehaviorTreeGraphNode.h"
#include "Delegates/Delegate.h"
#include "DetailCustomizations/BehaviorDecoratorDetails.h"
#include "DetailCustomizations/BlackboardDecoratorDetails.h"
#include "DetailCustomizations/BlackboardSelectorDetails.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraphUtilities.h"
#include "HAL/Platform.h"
#include "HAL/PlatformCrt.h"
#include "IAssetTools.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorDelegates.h"
#include "PropertyEditorModule.h"
#include "SGraphNode_BehaviorTree.h"
#include "SGraphNode_Decorator.h"
#include "Templates/Casts.h"
#include "UObject/NameTypes.h"
#include "UObject/UObjectBase.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

class IToolkitHost;
class UObject;


IMPLEMENT_MODULE( FBehaviorTreeEditorModule, BehaviorTreeEditor );
DEFINE_LOG_CATEGORY(LogBehaviorTreeEditor);

const FName FBehaviorTreeEditorModule::BehaviorTreeEditorAppIdentifier( TEXT( "BehaviorTreeEditorApp" ) );

class FGraphPanelNodeFactory_BehaviorTree : public FGraphPanelNodeFactory
{
	virtual TSharedPtr<class SGraphNode> CreateNode(UEdGraphNode* Node) const override
	{
		if (UBehaviorTreeGraphNode* BTNode = Cast<UBehaviorTreeGraphNode>(Node))
		{
			return SNew(SGraphNode_BehaviorTree, BTNode);
		}

		if (UBehaviorTreeDecoratorGraphNode_Decorator* InnerNode = Cast<UBehaviorTreeDecoratorGraphNode_Decorator>(Node))
		{
			return SNew(SGraphNode_Decorator, InnerNode);
		}

		return NULL;
	}
};

TSharedPtr<FGraphPanelNodeFactory> GraphPanelNodeFactory_BehaviorTree;

void FBehaviorTreeEditorModule::StartupModule()
{
	MenuExtensibilityManager = MakeShareable(new FExtensibilityManager);
	ToolBarExtensibilityManager = MakeShareable(new FExtensibilityManager);

	GraphPanelNodeFactory_BehaviorTree = MakeShareable( new FGraphPanelNodeFactory_BehaviorTree() );
	FEdGraphUtilities::RegisterVisualNodeFactory(GraphPanelNodeFactory_BehaviorTree);

	IAssetTools& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	TSharedPtr<FAssetTypeActions_BehaviorTree> BehaviorTreeAssetTypeAction = MakeShareable(new FAssetTypeActions_BehaviorTree);
	ItemDataAssetTypeActions.Add(BehaviorTreeAssetTypeAction);
	AssetToolsModule.RegisterAssetTypeActions(BehaviorTreeAssetTypeAction.ToSharedRef());

	TSharedPtr<FAssetTypeActions_Blackboard> BlackboardAssetTypeAction = MakeShareable(new FAssetTypeActions_Blackboard);
	ItemDataAssetTypeActions.Add(BlackboardAssetTypeAction);
	AssetToolsModule.RegisterAssetTypeActions(BlackboardAssetTypeAction.ToSharedRef());

	// Register the details customizer
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.RegisterCustomPropertyTypeLayout( "BlackboardKeySelector", FOnGetPropertyTypeCustomizationInstance::CreateStatic( &FBlackboardSelectorDetails::MakeInstance ) );
	PropertyModule.RegisterCustomClassLayout( "BTDecorator_Blackboard", FOnGetDetailCustomizationInstance::CreateStatic( &FBlackboardDecoratorDetails::MakeInstance ) );
	PropertyModule.RegisterCustomClassLayout( "BTDecorator", FOnGetDetailCustomizationInstance::CreateStatic( &FBehaviorDecoratorDetails::MakeInstance ) );
	PropertyModule.NotifyCustomizationModuleChanged();
}


void FBehaviorTreeEditorModule::ShutdownModule()
{
	if (!UObjectInitialized())
	{
		return;
	}

	MenuExtensibilityManager.Reset();
	ToolBarExtensibilityManager.Reset();
	ClassCache.Reset();

	if ( GraphPanelNodeFactory_BehaviorTree.IsValid() )
	{
		FEdGraphUtilities::UnregisterVisualNodeFactory(GraphPanelNodeFactory_BehaviorTree);
		GraphPanelNodeFactory_BehaviorTree.Reset();
	}

	// Unregister the BehaviorTree item data asset type actions
	if (FModuleManager::Get().IsModuleLoaded("AssetTools"))
	{
		IAssetTools& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get();
		for(auto& AssetTypeAction : ItemDataAssetTypeActions)
		{
			if (AssetTypeAction.IsValid())
			{
				AssetToolsModule.UnregisterAssetTypeActions(AssetTypeAction.ToSharedRef());
			}	
		}			
	}
	ItemDataAssetTypeActions.Empty();

	// Unregister the details customization
	if (FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.UnregisterCustomPropertyTypeLayout( "BlackboardKeySelector" );
		PropertyModule.UnregisterCustomClassLayout( "BTDecorator_Blackboard" );
		PropertyModule.UnregisterCustomClassLayout( "BTDecorator" );
		PropertyModule.NotifyCustomizationModuleChanged();
	}
}

TSharedRef<IBehaviorTreeEditor> FBehaviorTreeEditorModule::CreateBehaviorTreeEditor( const EToolkitMode::Type Mode, const TSharedPtr< IToolkitHost >& InitToolkitHost, UObject* Object )
{
	if (!ClassCache.IsValid())
	{
		ClassCache = MakeShareable(new FGraphNodeClassHelper(UBTNode::StaticClass()));
		FGraphNodeClassHelper::AddObservedBlueprintClasses(UBTTask_BlueprintBase::StaticClass());
		FGraphNodeClassHelper::AddObservedBlueprintClasses(UBTDecorator_BlueprintBase::StaticClass());
		FGraphNodeClassHelper::AddObservedBlueprintClasses(UBTService_BlueprintBase::StaticClass());
		ClassCache->UpdateAvailableBlueprintClasses();
	}

	TSharedRef< FBehaviorTreeEditor > NewBehaviorTreeEditor( new FBehaviorTreeEditor() );
	NewBehaviorTreeEditor->InitBehaviorTreeEditor( Mode, InitToolkitHost, Object );
	return NewBehaviorTreeEditor;	
}
