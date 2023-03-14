// Copyright Epic Games, Inc. All Rights Reserved.

#include "ControlRigArrayNodeSpawner.h"
#include "ControlRigUnitNodeSpawner.h"
#include "Graph/ControlRigGraph.h"
#include "Graph/ControlRigGraphNode.h"
#include "Graph/ControlRigGraphSchema.h"
#include "BlueprintNodeTemplateCache.h"
#include "RigVMModel/RigVMController.h"
#include "RigVMCore/RigVMUnknownType.h"
#include "ControlRigBlueprint.h"
#include "ControlRigBlueprintUtils.h"
#include "RigVMModel/Nodes/RigVMArrayNode.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ControlRigArrayNodeSpawner)

#if WITH_EDITOR
#include "Editor.h"
#include "Kismet2/BlueprintEditorUtils.h"
#endif

#define LOCTEXT_NAMESPACE "ControlRigArrayNodeSpawner"

UControlRigArrayNodeSpawner* UControlRigArrayNodeSpawner::CreateGeneric(ERigVMOpCode InOpCode, const FText& InMenuDesc, const FText& InCategory, const FText& InTooltip)
{
	UControlRigArrayNodeSpawner* NodeSpawner = NewObject<UControlRigArrayNodeSpawner>(GetTransientPackage());
	NodeSpawner->NodeClass = UControlRigGraphNode::StaticClass();
	NodeSpawner->OpCode = InOpCode;

	FBlueprintActionUiSpec& MenuSignature = NodeSpawner->DefaultMenuSignature;
	
	MenuSignature.MenuName = InMenuDesc;
	MenuSignature.Tooltip  = InTooltip;
	MenuSignature.Category = InCategory;

	switch(InOpCode)
	{
		case ERigVMOpCode::ArrayReset:
		{
			MenuSignature.Keywords = FText::FromString(TEXT("Clear,Empty,RemoveAll"));
			break;
		}
		case ERigVMOpCode::ArrayGetNum:
		{
			MenuSignature.Keywords = FText::FromString(TEXT("Size,Length,Count"));
			break;
		} 
		case ERigVMOpCode::ArraySetNum:
		{
			MenuSignature.Keywords = FText::FromString(TEXT("Size,Length,Count"));
			break;
		}
		case ERigVMOpCode::ArrayGetAtIndex:
		{
			MenuSignature.Keywords = FText::FromString(TEXT("Get,Element,At,Entry,[]"));
			break;
		}  
		case ERigVMOpCode::ArraySetAtIndex:
		{
			MenuSignature.Keywords = FText::FromString(TEXT("Set,Element,At,Entry,[]"));
			break;
		}
		case ERigVMOpCode::ArrayAdd:
		{
			MenuSignature.Keywords = FText::FromString(TEXT("Push"));
			break;
		}
		case ERigVMOpCode::ArrayRemove:
		{
			MenuSignature.Keywords = FText::FromString(TEXT("Pop"));
			break;
		}
		case ERigVMOpCode::ArrayFind:
		{
			MenuSignature.Keywords = FText::FromString(TEXT("Search,Contains"));
			break;
		}
		case ERigVMOpCode::ArrayAppend:
		{
			MenuSignature.Keywords = FText::FromString(TEXT("Concatenate,Join,Merge"));
			break;
		}
		case ERigVMOpCode::ArrayClone:
		{
			MenuSignature.Keywords = FText::FromString(TEXT("Make,Copy,Duplicate"));
			break;
		}
		case ERigVMOpCode::ArrayIterator:
		{
			MenuSignature.Keywords = FText::FromString(TEXT("ForEach,ForLoop,Iterate"));
			break;
		}
		case ERigVMOpCode::ArrayUnion:
		{
			MenuSignature.Keywords = FText::FromString(TEXT("Join,Merge,Concatenate"));
			break;
		}
		case ERigVMOpCode::ArrayDifference:
		case ERigVMOpCode::ArrayIntersection:
		case ERigVMOpCode::ArrayReverse:
		case ERigVMOpCode::ArrayInsert:
		default:
		{
			MenuSignature.Keywords = FText::FromString(TEXT(" "));
			break;
		}
	}
	MenuSignature.Icon = FSlateIcon(TEXT("ControlRigEditorStyle"), TEXT("ControlRig.RigUnit"));

	return NodeSpawner;
}

FBlueprintNodeSignature UControlRigArrayNodeSpawner::GetSpawnerSignature() const
{
	return FBlueprintNodeSignature(NodeClass);
}

FBlueprintActionUiSpec UControlRigArrayNodeSpawner::GetUiSpec(FBlueprintActionContext const& Context, FBindingSet const& Bindings) const
{
	UEdGraph* TargetGraph = (Context.Graphs.Num() > 0) ? Context.Graphs[0] : nullptr;
	FBlueprintActionUiSpec MenuSignature = PrimeDefaultUiSpec(TargetGraph);

	DynamicUiSignatureGetter.ExecuteIfBound(Context, Bindings, &MenuSignature);
	return MenuSignature;
}

UEdGraphNode* UControlRigArrayNodeSpawner::Invoke(UEdGraph* ParentGraph, FBindingSet const& Bindings, FVector2D const Location) const
{
	UControlRigGraphNode* NewNode = nullptr;

	bool const bIsTemplateNode = FBlueprintNodeTemplateCache::IsTemplateOuter(ParentGraph);
	bool const bIsUserFacingNode = !bIsTemplateNode;

	// First create a backing member for our node
	UControlRigGraph* RigGraph = Cast<UControlRigGraph>(ParentGraph);
	if(RigGraph == nullptr) return nullptr;
	UControlRigBlueprint* RigBlueprint = Cast<UControlRigBlueprint>(FBlueprintEditorUtils::FindBlueprintForGraph(ParentGraph));
	check(RigBlueprint);

	FString CPPType = RigVMTypeUtils::GetWildCardCPPType();
	FString CPPTypeObjectPath = RigVMTypeUtils::GetWildCardCPPTypeObject()->GetPathName();

	if(bIsUserFacingNode)
	{
		if (UControlRigGraphSchema* RigSchema = Cast<UControlRigGraphSchema>((UEdGraphSchema*)ParentGraph->GetSchema()))
		{
			if (const UEdGraphPin* LastPin = RigSchema->LastPinForCompatibleCheck)
			{
				if (URigVMPin* ModelPin = RigBlueprint->GetModel(ParentGraph)->FindPin(LastPin->GetName()))
				{
					RigVMTypeUtils::CPPTypeFromPin(ModelPin, CPPType, CPPTypeObjectPath, true);
				}
			}
		}
	}

	FName MemberName = NAME_None;

#if WITH_EDITOR
	if (GEditor && !bIsTemplateNode)
	{
		GEditor->CancelTransaction(0);
	}
#endif

	URigVMController* Controller = bIsTemplateNode ? RigGraph->GetTemplateController() : RigBlueprint->GetController(ParentGraph);

	const FString OpCodeString = StaticEnum<ERigVMOpCode>()->GetNameStringByValue((int64)OpCode);
	const FName Name = *OpCodeString;

	if (!bIsTemplateNode)
	{
		Controller->OpenUndoBracket(FString::Printf(TEXT("Add '%s' Node"), *Name.ToString()));
	}

	if (URigVMArrayNode* ModelNode = Controller->AddArrayNodeFromObjectPath(OpCode, CPPType, CPPTypeObjectPath, Location, Name.ToString(), bIsUserFacingNode, !bIsTemplateNode))
	{
		NewNode = Cast<UControlRigGraphNode>(RigGraph->FindNodeForModelNodeName(ModelNode->GetFName()));

		if (NewNode && bIsUserFacingNode)
		{
			Controller->ClearNodeSelection(true);
			Controller->SelectNode(ModelNode, true, true);
		}

		if (bIsUserFacingNode)
		{
			Controller->CloseUndoBracket();
		}
		else
		{
			Controller->RemoveNode(ModelNode, false);
		}
	}
	else
	{
		if (bIsUserFacingNode)
		{
			Controller->CancelUndoBracket();
		}
	}


	return NewNode;
}

bool UControlRigArrayNodeSpawner::IsTemplateNodeFilteredOut(FBlueprintActionFilter const& Filter) const
{
	for (UBlueprint* Blueprint : Filter.Context.Blueprints)
	{
		if (UControlRigBlueprint* RigBlueprint = Cast<UControlRigBlueprint>(Blueprint))
		{
			if (Filter.Context.Pins.Num() == 0)
			{
				return false;
			}

			FString PinPath = Filter.Context.Pins[0]->GetName();
			UEdGraph* EdGraph = Filter.Context.Pins[0]->GetOwningNode()->GetGraph();
			if (URigVMPin* ModelPin = RigBlueprint->GetModel(EdGraph)->FindPin(PinPath))
			{
				return ModelPin->IsExecuteContext();
			}
		}
	}

	return false;
}

#undef LOCTEXT_NAMESPACE

