// Copyright Epic Games, Inc. All Rights Reserved.

#include "EdGraph/NodeSpawners/RigVMEdGraphTemplateNodeSpawner.h"
#include "EdGraph/NodeSpawners/RigVMEdGraphUnitNodeSpawner.h"
#include "EdGraph/RigVMEdGraph.h"
#include "EdGraph/RigVMEdGraphNode.h"
#include "EdGraph/RigVMEdGraphSchema.h"
#include "EdGraphSchema_K2.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Settings/EditorStyleSettings.h"
#include "Editor/EditorEngine.h"
#include "ObjectEditorUtils.h"
#include "EditorCategoryUtils.h"
#include "K2Node_Variable.h"
#include "BlueprintNodeTemplateCache.h"
#include "RigVMBlueprintUtils.h"
#include "ScopedTransaction.h"
#include "RigVMModel/Nodes/RigVMDispatchNode.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigVMEdGraphTemplateNodeSpawner)

#if WITH_EDITOR
#include "Editor.h"
#endif

#define LOCTEXT_NAMESPACE "RigVMEdGraphTemplateNodeSpawner"

URigVMEdGraphTemplateNodeSpawner* URigVMEdGraphTemplateNodeSpawner::CreateFromNotation(const FName& InNotation, const FText& InMenuDesc, const FText& InCategory, const FText& InTooltip)
{
	URigVMEdGraphTemplateNodeSpawner* NodeSpawner = NewObject<URigVMEdGraphTemplateNodeSpawner>(GetTransientPackage());
	NodeSpawner->TemplateNotation = InNotation;
	NodeSpawner->NodeClass = URigVMEdGraphNode::StaticClass();

	FBlueprintActionUiSpec& MenuSignature = NodeSpawner->DefaultMenuSignature;
	
	MenuSignature.MenuName = InMenuDesc;
	MenuSignature.Tooltip  = InTooltip;
	MenuSignature.Category = InCategory;

	if (const FRigVMTemplate* Template = FRigVMRegistry::Get().FindTemplate(InNotation))
	{
#if WITH_EDITOR
		FString KeywordsMetadata = Template->GetKeywords();
		MenuSignature.Keywords = FText::FromString(KeywordsMetadata);
#endif
	}

	// add at least one character, so that PrimeDefaultUiSpec() doesn't 
	// attempt to query the template node
	//
	// @TODO: maybe UPROPERTY() fields should have keyword metadata like functions
	if (MenuSignature.Keywords.IsEmpty())
	{
		// want to set it to something so we won't end up back in this condition
		MenuSignature.Keywords = FText::FromString(TEXT(" "));
	}

	// @TODO: should use details customization-like extensibility system to provide editor only data like this
	MenuSignature.Icon = FSlateIcon(TEXT("RigVMEditorStyle"), TEXT("RigVM.Unit"));

	return NodeSpawner;
}

void URigVMEdGraphTemplateNodeSpawner::Prime()
{
	// we expect that you don't need a node template to construct menu entries
	// from this, so we choose not to pre-cache one here
}

FBlueprintNodeSignature URigVMEdGraphTemplateNodeSpawner::GetSpawnerSignature() const
{
	const int32 NotationHash = (int32)GetTypeHash(TemplateNotation);
	return FBlueprintNodeSignature(FString("RigVMTemplate_") + FString::FromInt(NotationHash));
}

bool URigVMEdGraphTemplateNodeSpawner::IsTemplateNodeFilteredOut(FBlueprintActionFilter const& Filter) const
{
	if(URigVMEdGraphNodeSpawner::IsTemplateNodeFilteredOut(Filter))
	{
		return true;
	}

	bool bFilteredOut = true;
	for (UEdGraphPin* Pin : Filter.Context.Pins)
	{
		for (UEdGraph* Graph : Filter.Context.Graphs)
		{
			if (!Graph->Nodes.Contains(Pin->GetOwningNode()))
			{
				continue;
			}

			if (URigVMEdGraph* EdGraph = Cast<URigVMEdGraph>(Graph))
			{
				if (URigVMGraph* RigGraph = EdGraph->GetModel())
				{
					if (URigVMPin* ModelPin = RigGraph->FindPin(Pin->GetName()))
					{
						if (URigVMTemplateNode* TemplateNode = Cast<URigVMTemplateNode>(ModelPin->GetNode()))
						{
							// Only filter when the sourece pin is not a wildcard
							if (ModelPin->GetCPPType() != RigVMTypeUtils::GetWildCardCPPType())
							{
								if (const FRigVMTemplate* Template = FRigVMRegistry::Get().FindTemplate(TemplateNotation))
								{
									if(ModelPin->IsExecuteContext())
									{
										FRigVMDispatchContext DispatchContext;
										if(const URigVMDispatchNode* DispatchNode = Cast<URigVMDispatchNode>(ModelPin->GetNode()))
										{
											DispatchContext = DispatchNode->GetDispatchContext();
										}

										if(Template->NumExecuteArguments(DispatchContext) > 0)
										{
											return false;
										}
									}
									
									for (int32 i=0; i<Template->NumArguments(); ++i)
									{
										const FRigVMTemplateArgument* Argument = Template->GetArgument(i);
										if (Template->ArgumentSupportsTypeIndex(Argument->GetName(), ModelPin->GetTypeIndex()))
										{
											return false;
										}
									}
									return true;
								}									
							}
							else
							{
								// TODO: given the filtered types of the source pin, provide only the nodes that will not break any connections
							}
						}
					}					
				}
			}
		}
	}

	return false;
}

FBlueprintActionUiSpec URigVMEdGraphTemplateNodeSpawner::GetUiSpec(FBlueprintActionContext const& Context, FBindingSet const& Bindings) const
{
	UEdGraph* TargetGraph = (Context.Graphs.Num() > 0) ? Context.Graphs[0] : nullptr;
	FBlueprintActionUiSpec MenuSignature = PrimeDefaultUiSpec(TargetGraph);

	DynamicUiSignatureGetter.ExecuteIfBound(Context, Bindings, &MenuSignature);
	return MenuSignature;
}

UEdGraphNode* URigVMEdGraphTemplateNodeSpawner::Invoke(UEdGraph* ParentGraph, FBindingSet const& Bindings, FVector2D const Location) const
{
	URigVMEdGraphNode* NewNode = nullptr;

	if(!TemplateNotation.IsNone())
	{
#if WITH_EDITOR
		if (GEditor)
		{
			GEditor->CancelTransaction(0);
		}
#endif

		UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(ParentGraph);
		NewNode = SpawnNode(ParentGraph, Blueprint, TemplateNotation, Location);
	}

	return NewNode;
}

URigVMEdGraphNode* URigVMEdGraphTemplateNodeSpawner::SpawnNode(UEdGraph* ParentGraph, UBlueprint* Blueprint, const FName& InNotation, FVector2D const Location)
{
	URigVMEdGraphNode* NewNode = nullptr;
	URigVMBlueprint* RigBlueprint = Cast<URigVMBlueprint>(Blueprint);
	URigVMEdGraph* RigGraph = Cast<URigVMEdGraph>(ParentGraph);

	if (RigBlueprint != nullptr && RigGraph != nullptr)
	{
		bool const bIsTemplateNode = FBlueprintNodeTemplateCache::IsTemplateOuter(ParentGraph);
		bool const bIsUserFacingNode = !bIsTemplateNode;

		const FRigVMTemplate* Template = FRigVMRegistry::Get().FindTemplate(InNotation);
		if (Template == nullptr)
		{
			return nullptr;
		}

		const int32 NotationHash = (int32)GetTypeHash(InNotation);
		const FString TemplateName = TEXT("RigVMTemplate_") + FString::FromInt(NotationHash);

		FName Name = bIsTemplateNode ? *TemplateName : FRigVMBlueprintUtils::ValidateName(RigBlueprint, Template->GetName().ToString());
		URigVMController* Controller = bIsTemplateNode ? RigGraph->GetTemplateController() : RigBlueprint->GetController(ParentGraph);

		if (!bIsTemplateNode)
		{
			Controller->OpenUndoBracket(FString::Printf(TEXT("Add '%s' Node"), *Name.ToString()));
		}

		if (URigVMTemplateNode* ModelNode = Controller->AddTemplateNode(InNotation, Location, Name.ToString(), bIsUserFacingNode, !bIsTemplateNode))
		{
			NewNode = Cast<URigVMEdGraphNode>(RigGraph->FindNodeForModelNodeName(ModelNode->GetFName()));

			if (NewNode && bIsUserFacingNode)
			{
				Controller->ClearNodeSelection(true);
				Controller->SelectNode(ModelNode, true, true);

				URigVMEdGraphUnitNodeSpawner::HookupMutableNode(ModelNode, RigBlueprint);
			}

			if (bIsUserFacingNode)
			{
				Controller->CloseUndoBracket();
			}
			else
			{
				if(NewNode)
				{
					NewNode->ModelNodePath = Template->GetNotation().ToString();
				}
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
	}
	return NewNode;
}

#undef LOCTEXT_NAMESPACE

