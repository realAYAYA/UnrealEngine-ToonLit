// Copyright Epic Games, Inc. All Rights Reserved.

#include "ControlRigVariableNodeSpawner.h"
#include "Graph/ControlRigGraph.h"
#include "Graph/ControlRigGraphNode.h"
#include "EdGraphSchema_K2.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Settings/EditorStyleSettings.h"
#include "Editor/EditorEngine.h"
#include "ObjectEditorUtils.h"
#include "EditorCategoryUtils.h"
#include "K2Node_Variable.h"
#include "BlueprintNodeTemplateCache.h"
#include "RigVMModel/RigVMController.h"
#include "ControlRigBlueprint.h"
#include "ControlRigBlueprintUtils.h"
#include "RigVMModel/Nodes/RigVMVariableNode.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ControlRigVariableNodeSpawner)

#if WITH_EDITOR
#include "Editor.h"
#endif

#define LOCTEXT_NAMESPACE "ControlRigVariableNodeSpawner"

UControlRigVariableNodeSpawner* UControlRigVariableNodeSpawner::CreateFromExternalVariable(UControlRigBlueprint* InBlueprint, const FRigVMExternalVariable& InExternalVariable, bool bInIsGetter, const FText& InMenuDesc, const FText& InCategory, const FText& InTooltip)
{
	UControlRigVariableNodeSpawner* NodeSpawner = NewObject<UControlRigVariableNodeSpawner>(GetTransientPackage());
	NodeSpawner->Blueprint = InBlueprint;
	NodeSpawner->ExternalVariable = InExternalVariable;
	NodeSpawner->bIsGetter = bInIsGetter;
	NodeSpawner->NodeClass = UControlRigGraphNode::StaticClass();

	FBlueprintActionUiSpec& MenuSignature = NodeSpawner->DefaultMenuSignature;
	
	MenuSignature.MenuName = FText::FromString(FString::Printf(TEXT("%s %s"), bInIsGetter ? TEXT("Get") : TEXT("Set"), *InMenuDesc.ToString()));
	MenuSignature.Tooltip  = InTooltip;
	MenuSignature.Category = InCategory;
	MenuSignature.Keywords = FText::FromString(TEXT("Variable"));

	FEdGraphPinType PinType = RigVMTypeUtils::PinTypeFromExternalVariable(InExternalVariable);
	MenuSignature.Icon = UK2Node_Variable::GetVarIconFromPinType(PinType, MenuSignature.IconTint);

	return NodeSpawner;
}

UControlRigVariableNodeSpawner* UControlRigVariableNodeSpawner::CreateFromLocalVariable(
	UControlRigBlueprint* InBlueprint, URigVMGraph* InGraphOwner, const FRigVMGraphVariableDescription& InLocalVariable,
	bool bInIsGetter, const FText& InMenuDesc, const FText& InCategory, const FText& InTooltip)
{
	UControlRigVariableNodeSpawner* Spawner = CreateFromExternalVariable(InBlueprint, InLocalVariable.ToExternalVariable(), bInIsGetter, InMenuDesc, InCategory, InTooltip);
	Spawner->bIsLocalVariable = true;
	Spawner->GraphOwner = InGraphOwner;
	return Spawner;
}

void UControlRigVariableNodeSpawner::Prime()
{
	// we expect that you don't need a node template to construct menu entries
	// from this, so we choose not to pre-cache one here
}

bool UControlRigVariableNodeSpawner::IsTemplateNodeFilteredOut(FBlueprintActionFilter const& Filter) const
{
	if (Blueprint.IsValid())
	{
		if (!Filter.Context.Blueprints.Contains(Blueprint.Get()))
		{
			return true;
		}

		if (bIsLocalVariable)
		{
			bool bIsFiltered = true;
			if (Filter.Context.Graphs.Num() == 1 && GraphOwner.IsValid())
			{
				if (UControlRigGraph* Graph = Cast<UControlRigGraph>(Filter.Context.Graphs[0]))
				{
					if (GraphOwner.Get() == Graph->GetModel())
					{
						bIsFiltered = false;
					}
				}
			}

			if (bIsFiltered)
			{
				return true;
			}
		}
	}
	return Super::IsTemplateNodeFilteredOut(Filter);
}

FBlueprintNodeSignature UControlRigVariableNodeSpawner::GetSpawnerSignature() const
{
	return FBlueprintNodeSignature(FString("RigUnit=" + ExternalVariable.Name.ToString()));
}

FBlueprintActionUiSpec UControlRigVariableNodeSpawner::GetUiSpec(FBlueprintActionContext const& Context, FBindingSet const& Bindings) const
{
	UEdGraph* TargetGraph = (Context.Graphs.Num() > 0) ? Context.Graphs[0] : nullptr;
	FBlueprintActionUiSpec MenuSignature = PrimeDefaultUiSpec(TargetGraph);

	DynamicUiSignatureGetter.ExecuteIfBound(Context, Bindings, &MenuSignature);
	return MenuSignature;
}

UEdGraphNode* UControlRigVariableNodeSpawner::Invoke(UEdGraph* ParentGraph, FBindingSet const& Bindings, FVector2D const Location) const
{
	UControlRigGraphNode* NewNode = nullptr;

	bool const bIsTemplateNode = FBlueprintNodeTemplateCache::IsTemplateOuter(ParentGraph);
	bool const bIsUserFacingNode = !bIsTemplateNode;

	// First create a backing member for our node
	UControlRigGraph* RigGraph = Cast<UControlRigGraph>(ParentGraph);
	if(RigGraph == nullptr) return nullptr;
	UControlRigBlueprint* RigBlueprint = Cast<UControlRigBlueprint>(FBlueprintEditorUtils::FindBlueprintForGraph(ParentGraph));
	check(RigBlueprint);

	FName MemberName = NAME_None;

#if WITH_EDITOR
	if (GEditor && !bIsTemplateNode)
	{
		GEditor->CancelTransaction(0);
	}
#endif

	URigVMController* Controller = bIsTemplateNode ? RigGraph->GetTemplateController() : RigBlueprint->GetController(ParentGraph);

	if (bIsUserFacingNode)
	{
		Controller->OpenUndoBracket(TEXT("Add Variable"));
	}

	FString ObjectPath;
	if (ExternalVariable.TypeObject)
	{
		ObjectPath = ExternalVariable.TypeObject->GetPathName();
	}

	FString TypeName = ExternalVariable.TypeName.ToString();
	if (ExternalVariable.bIsArray)
	{
		TypeName = FString::Printf(TEXT("TArray<%s>"), *TypeName);
	}

	FString NodeName;
	if(bIsTemplateNode)
	{
		// since we are removing the node at the end of this function
		// we need to create a unique here.
		static constexpr TCHAR VariableNodeNameFormat[] = TEXT("VariableNode_%s_%s");
		static const FString GetterPrefix = TEXT("Getter");
		static const FString SetterPrefix = TEXT("Setter");
		NodeName = FString::Printf(VariableNodeNameFormat, bIsGetter ? *GetterPrefix : *SetterPrefix, *ExternalVariable.Name.ToString());
	}

	if (URigVMNode* ModelNode = Controller->AddVariableNodeFromObjectPath(ExternalVariable.Name, TypeName, ObjectPath, bIsGetter, FString(), Location, NodeName, !bIsTemplateNode, !bIsTemplateNode))
	{
		for (UEdGraphNode* Node : ParentGraph->Nodes)
		{
			if (UControlRigGraphNode* RigNode = Cast<UControlRigGraphNode>(Node))
			{
				if (RigNode->GetModelNodeName() == ModelNode->GetFName())
				{
					NewNode = RigNode;
					break;
				}
			}
		}

		if (bIsUserFacingNode)
		{
			if (NewNode)
			{
				Controller->ClearNodeSelection(true);
				Controller->SelectNode(ModelNode, true, true);
			}
			Controller->CloseUndoBracket();
		}
		else
		{
			// similar to UBlueprintNodeSpawner::Invoke -> UBlueprintNodeSpawner::SpawnEdGraphNode
			// we simply want the node, but not actually adding it to a graph
			Controller->RemoveNode(ModelNode, false);
		}
	}
	else
	{
		if (!bIsTemplateNode)
		{
			Controller->CancelUndoBracket();
		}
	}


	return NewNode;
}

#undef LOCTEXT_NAMESPACE

