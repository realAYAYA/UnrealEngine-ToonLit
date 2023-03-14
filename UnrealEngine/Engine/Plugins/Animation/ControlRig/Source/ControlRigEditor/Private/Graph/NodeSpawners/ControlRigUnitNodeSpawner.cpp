// Copyright Epic Games, Inc. All Rights Reserved.

#include "ControlRigUnitNodeSpawner.h"
#include "Graph/ControlRigGraph.h"
#include "Graph/ControlRigGraphNode.h"
#include "Graph/ControlRigGraphSchema.h"
#include "EdGraphSchema_K2.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Settings/EditorStyleSettings.h"
#include "Editor/EditorEngine.h"
#include "ObjectEditorUtils.h"
#include "EditorCategoryUtils.h"
#include "K2Node_Variable.h"
#include "BlueprintNodeTemplateCache.h"
#include "ControlRigBlueprintUtils.h"
#include "ScopedTransaction.h"
#include "Units/Execution/RigUnit_BeginExecution.h"
#include "ControlRig.h"
#include "Settings/ControlRigSettings.h"
#include "Units/Execution/RigUnit_BeginExecution.h"
#include "Units/Execution/RigUnit_InverseExecution.h"
#include "Units/Execution/RigUnit_PrepareForExecution.h"
#include "Units/Execution/RigUnit_InteractionExecution.h"
#include "Units/Execution/RigUnit_UserDefinedEvent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ControlRigUnitNodeSpawner)

#if WITH_EDITOR
#include "Editor.h"
#include "SGraphActionMenu.h"
#endif

#define LOCTEXT_NAMESPACE "ControlRigUnitNodeSpawner"

UControlRigUnitNodeSpawner* UControlRigUnitNodeSpawner::CreateFromStruct(UScriptStruct* InStruct, const FText& InMenuDesc, const FText& InCategory, const FText& InTooltip)
{
	UControlRigUnitNodeSpawner* NodeSpawner = NewObject<UControlRigUnitNodeSpawner>(GetTransientPackage());
	NodeSpawner->StructTemplate = InStruct;
	NodeSpawner->NodeClass = UControlRigGraphNode::StaticClass();

	FBlueprintActionUiSpec& MenuSignature = NodeSpawner->DefaultMenuSignature;
	
	MenuSignature.MenuName = InMenuDesc;
	MenuSignature.Tooltip  = InTooltip;
	MenuSignature.Category = InCategory;

	FString KeywordsMetadata, TemplateNameMetadata;
	InStruct->GetStringMetaDataHierarchical(FRigVMStruct::KeywordsMetaName, &KeywordsMetadata);
	if(!TemplateNameMetadata.IsEmpty())
	{
		if(KeywordsMetadata.IsEmpty())
		{
			KeywordsMetadata = TemplateNameMetadata;
		}
		else
		{
			KeywordsMetadata = KeywordsMetadata + TEXT(",") + TemplateNameMetadata;
		}
	}
	MenuSignature.Keywords = FText::FromString(KeywordsMetadata);

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
	FStructOnScope StructScope(InStruct);
	const FRigVMStruct* StructInstance = (const FRigVMStruct*)StructScope.GetStructMemory();
	if(StructInstance->GetEventName().IsNone())
	{
		if(InStruct->HasMetaDataHierarchical(FRigVMStruct::IconMetaName))
		{
			FString IconPath;
			const int32 NumOfIconPathNames = 4;
			
			FName IconPathNames[NumOfIconPathNames] = {
				NAME_None, // StyleSetName
				NAME_None, // StyleName
				NAME_None, // SmallStyleName
				NAME_None  // StatusOverlayStyleName
			};

			InStruct->GetStringMetaDataHierarchical(FRigVMStruct::IconMetaName, &IconPath);

			int32 NameIndex = 0;

			while (!IconPath.IsEmpty() && NameIndex < NumOfIconPathNames)
			{
				FString Left;
				FString Right;

				if (!IconPath.Split(TEXT("|"), &Left, &Right))
				{
					Left = IconPath;
				}

				IconPathNames[NameIndex] = FName(*Left);

				NameIndex++;
				IconPath = Right;
			}
			MenuSignature.Icon = FSlateIcon(IconPathNames[0], IconPathNames[1], IconPathNames[2], IconPathNames[3]);
		}
		else
		{
			MenuSignature.Icon = FSlateIcon(TEXT("ControlRigEditorStyle"), TEXT("ControlRig.RigUnit"));
		}
	}
	else
	{
		MenuSignature.Icon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "GraphEditor.Event_16x");
	}

	return NodeSpawner;
}

void UControlRigUnitNodeSpawner::Prime()
{
	// we expect that you don't need a node template to construct menu entries
	// from this, so we choose not to pre-cache one here
}

FBlueprintNodeSignature UControlRigUnitNodeSpawner::GetSpawnerSignature() const
{
	return FBlueprintNodeSignature(FString("RigUnit=" + StructTemplate->GetFName().ToString()));
}

FBlueprintActionUiSpec UControlRigUnitNodeSpawner::GetUiSpec(FBlueprintActionContext const& Context, FBindingSet const& Bindings) const
{
	UEdGraph* TargetGraph = (Context.Graphs.Num() > 0) ? Context.Graphs[0] : nullptr;
	FBlueprintActionUiSpec MenuSignature = PrimeDefaultUiSpec(TargetGraph);

	DynamicUiSignatureGetter.ExecuteIfBound(Context, Bindings, &MenuSignature);
	return MenuSignature;
}

UEdGraphNode* UControlRigUnitNodeSpawner::Invoke(UEdGraph* ParentGraph, FBindingSet const& Bindings, FVector2D const Location) const
{
	UControlRigGraphNode* NewNode = nullptr;

	if(StructTemplate)
	{
#if WITH_EDITOR
		if (GEditor)
		{
			GEditor->CancelTransaction(0);
		}
#endif

		UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(ParentGraph);
		NewNode = SpawnNode(ParentGraph, Blueprint, StructTemplate, Location);
	}

	return NewNode;
}

UControlRigGraphNode* UControlRigUnitNodeSpawner::SpawnNode(UEdGraph* ParentGraph, UBlueprint* Blueprint, UScriptStruct* StructTemplate, FVector2D const Location)
{
	UControlRigGraphNode* NewNode = nullptr;
	UControlRigBlueprint* RigBlueprint = Cast<UControlRigBlueprint>(Blueprint);
	UControlRigGraph* RigGraph = Cast<UControlRigGraph>(ParentGraph);

	if (RigBlueprint != nullptr && RigGraph != nullptr)
	{
		bool const bIsTemplateNode = FBlueprintNodeTemplateCache::IsTemplateOuter(ParentGraph);
		bool const bIsUserFacingNode = !bIsTemplateNode;

		FName Name = bIsTemplateNode ? *StructTemplate->GetStructCPPName() : FControlRigBlueprintUtils::ValidateName(RigBlueprint, StructTemplate->GetFName().ToString());

		FStructOnScope StructScope(StructTemplate);
		const FRigVMStruct* StructInstance = (const FRigVMStruct*)StructScope.GetStructMemory();
		const FName EventName = StructInstance->GetEventName();

		// events can only exist once across all uber graphs
		if(bIsUserFacingNode)
		{
			if(!EventName.IsNone())
			{
				if(StructInstance->CanOnlyExistOnce() &&
					(StructTemplate != FRigUnit_UserDefinedEvent::StaticStruct()))
				{
					const TArray<URigVMGraph*> Models = RigBlueprint->GetAllModels();
					for(URigVMGraph* Model : Models)
					{
						if(Model->IsRootGraph() && !Model->IsA<URigVMFunctionLibrary>())
						{
							for(const URigVMNode* Node : Model->GetNodes())
							{
								if(Node->GetEventName() == EventName)
								{
									RigBlueprint->OnRequestJumpToHyperlink().Execute(Node);
									return nullptr;
								}
							}
						}
					}
				}
			}
		}
		
		URigVMController* Controller = bIsTemplateNode ? RigGraph->GetTemplateController() : RigBlueprint->GetController(ParentGraph);
		if(Controller == nullptr)
		{
			return nullptr;
		}

		if (!bIsTemplateNode)
		{
			Controller->OpenUndoBracket(FString::Printf(TEXT("Add '%s' Node"), *Name.ToString()));
		}

		FRigVMUnitNodeCreatedContext& UnitNodeCreatedContext = Controller->GetUnitNodeCreatedContext();
		FRigVMUnitNodeCreatedContext::FScope ReasonScope(UnitNodeCreatedContext, ERigVMNodeCreatedReason::NodeSpawner);

		if (URigVMUnitNode* ModelNode = Controller->AddUnitNode(StructTemplate, FRigUnit::GetMethodName(), Location, Name.ToString(), bIsUserFacingNode, !bIsTemplateNode))
		{
			NewNode = Cast<UControlRigGraphNode>(RigGraph->FindNodeForModelNodeName(ModelNode->GetFName()));
			check(NewNode);

			if (NewNode && bIsUserFacingNode)
			{
				if(StructTemplate == FRigUnit_UserDefinedEvent::StaticStruct())
				{
					// ensure uniqueness for the event name
					TArray<FName> ExistingEventNames = {
						FRigUnit_BeginExecution::EventName,
						FRigUnit_InverseExecution::EventName,
						FRigUnit_PrepareForExecution::EventName,
						FRigUnit_InteractionExecution::EventName
					};

					const TArray<URigVMGraph*> Models = RigBlueprint->GetAllModels();
					for(URigVMGraph* Model : Models)
					{
						for(const URigVMNode* Node : Model->GetNodes())
						{
							if(Node != ModelNode)
							{
								const FName ExistingEventName = Node->GetEventName();
								if(!ExistingEventName.IsNone())
								{
									ExistingEventNames.AddUnique(ExistingEventName);
								}
							}
						}
					}

					FName SafeEventName = URigVMController::GetUniqueName(EventName, [ExistingEventNames](const FName& NameToCheck) -> bool
					{
						return !ExistingEventNames.Contains(NameToCheck);
					}, false, true);

					Controller->SetPinDefaultValue(ModelNode->FindPin(TEXT("EventName"))->GetPinPath(), SafeEventName.ToString(), false, true, false, true);
				}

				Controller->ClearNodeSelection(true);
				Controller->SelectNode(ModelNode, true, true);

				HookupMutableNode(ModelNode, RigBlueprint);
			}

#if WITH_EDITORONLY_DATA
			if (!bIsTemplateNode)
			{
				const FControlRigSettingsPerPinBool* ExpansionMapPtr = UControlRigEditorSettings::Get()->RigUnitPinExpansion.Find(ModelNode->GetScriptStruct()->GetName());
				if (ExpansionMapPtr)
				{
					const FControlRigSettingsPerPinBool& ExpansionMap = *ExpansionMapPtr;

					for (const TPair<FString, bool>& Pair : ExpansionMap.Values)
					{
						FString PinPath = FString::Printf(TEXT("%s.%s"), *ModelNode->GetName(), *Pair.Key);
						Controller->SetPinExpansion(PinPath, Pair.Value, bIsUserFacingNode);
					}
				}

				FString UsedFilterString = SGraphActionMenu::LastUsedFilterText;
				if (!UsedFilterString.IsEmpty())
				{
					UsedFilterString = UsedFilterString.ToLower();

					if (UEnum* RigElementTypeEnum = StaticEnum<ERigElementType>())
					{
						ERigElementType UsedElementType = ERigElementType::None;
						int64 MaxEnumValue = RigElementTypeEnum->GetMaxEnumValue();

						for (int64 EnumValue = 0; EnumValue < MaxEnumValue; EnumValue++)
						{
							FString EnumText = RigElementTypeEnum->GetDisplayNameTextByValue(EnumValue).ToString().ToLower();
							if (UsedFilterString.Contains(EnumText))
							{
								UsedElementType = (ERigElementType)EnumValue;
								break;
							}
						}

						if (UsedElementType != ERigElementType::None)
						{
							TArray<URigVMPin*> ModelPins = ModelNode->GetAllPinsRecursively();
							for (URigVMPin* ModelPin : ModelPins)
							{
								if (ModelPin->GetCPPTypeObject() == FRigElementKey::StaticStruct())
								{
									if (URigVMPin* TypePin = ModelPin->FindSubPin(TEXT("Type")))
									{
										FString DefaultValue = RigElementTypeEnum->GetDisplayNameTextByValue((int64)UsedElementType).ToString();
										Controller->SetPinDefaultValue(TypePin->GetPinPath(), DefaultValue);
										break;
									}
								}
							}
						}
					}
				}
			}
#endif

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
	}
	return NewNode;
}

void UControlRigUnitNodeSpawner::HookupMutableNode(URigVMNode* InModelNode, UControlRigBlueprint* InRigBlueprint)
{
	if(!UControlRigEditorSettings::Get()->bAutoLinkMutableNodes)
	{
		return;
	}
	
	URigVMController* Controller = InRigBlueprint->GetController(InModelNode->GetGraph());

	Controller->ClearNodeSelection(true);
	Controller->SelectNode(InModelNode, true, true);

	// see if the node has an execute pin
	URigVMPin* ModelNodeExecutePin = nullptr;
	for (URigVMPin* Pin : InModelNode->GetPins())
	{
		if (Pin->GetScriptStruct())
		{
			if (Pin->GetScriptStruct()->IsChildOf(FRigVMExecuteContext::StaticStruct()))
			{
				if (Pin->GetDirection() == ERigVMPinDirection::IO || Pin->GetDirection() == ERigVMPinDirection::Input)
				{
					ModelNodeExecutePin = Pin;
					break;
				}
			}
		}
	}

	// we have an execute pin - so we have to hook it up
	if (ModelNodeExecutePin)
	{
		URigVMPin* ClosestOtherModelNodeExecutePin = nullptr;
		float ClosestDistance = FLT_MAX;

		const UControlRigGraphSchema* Schema = GetDefault<UControlRigGraphSchema>();
		if (Schema->LastPinForCompatibleCheck)
		{
			URigVMPin* FromPin = Controller->GetGraph()->FindPin(Schema->LastPinForCompatibleCheck->GetName());
			if (FromPin)
			{
				if (FromPin->IsExecuteContext() &&
					(FromPin->GetDirection() == ERigVMPinDirection::IO || FromPin->GetDirection() == ERigVMPinDirection::Output))
				{
					ClosestOtherModelNodeExecutePin = FromPin;
				}
			}

		}

		if (ClosestOtherModelNodeExecutePin == nullptr)
		{
			for (URigVMNode* OtherModelNode : Controller->GetGraph()->GetNodes())
			{
				if (OtherModelNode == InModelNode)
				{
					continue;
				}

				for (URigVMPin* Pin : OtherModelNode->GetPins())
				{
					if (Pin->GetScriptStruct())
					{
						if (Pin->GetScriptStruct()->IsChildOf(FRigVMExecuteContext::StaticStruct()))
						{
							if (Pin->GetDirection() == ERigVMPinDirection::IO || Pin->GetDirection() == ERigVMPinDirection::Output)
							{
								if (Pin->GetLinkedTargetPins().Num() == 0)
								{
									float Distance = (OtherModelNode->GetPosition() - InModelNode->GetPosition()).Size();
									if (Distance < ClosestDistance)
									{
										ClosestOtherModelNodeExecutePin = Pin;
										ClosestDistance = Distance;
										break;
									}
								}
							}
						}
					}
				}
			}
		}

		if (ClosestOtherModelNodeExecutePin)
		{
			Controller->AddLink(ClosestOtherModelNodeExecutePin->GetPinPath(), ModelNodeExecutePin->GetPinPath(), true);
		}
	}
}

#undef LOCTEXT_NAMESPACE

