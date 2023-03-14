// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/DataflowSchema.h"

#include "Dataflow/DataflowEdNode.h"
#include "Dataflow/DataflowEditorActions.h"
#include "Dataflow/DataflowEdNode.h"
#include "Dataflow/DataflowSNode.h"
#include "Dataflow/DataflowNodeFactory.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraph.h"
#include "Framework/Commands/GenericCommands.h"
#include "Logging/LogMacros.h"
#include "ToolMenuSection.h"
#include "ToolMenu.h"
#include "GraphEditorActions.h"
#include "Dataflow/DataflowSettings.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DataflowSchema)

#define LOCTEXT_NAMESPACE "DataflowNode"

UDataflowSchema::UDataflowSchema()
{
}

void UDataflowSchema::GetContextMenuActions(class UToolMenu* Menu, class UGraphNodeContextMenuContext* Context) const
{
	if (Context->Node)
	{
		{
			FToolMenuSection& Section = Menu->AddSection("TestGraphSchemaNodeActions", LOCTEXT("GraphSchemaNodeActions_MenuHeader", "Node Actions"));
			{
				Section.AddMenuEntry(FGenericCommands::Get().Rename);
				Section.AddMenuEntry(FGenericCommands::Get().Delete);
				Section.AddMenuEntry(FGenericCommands::Get().Cut);
				Section.AddMenuEntry(FGenericCommands::Get().Copy);
				Section.AddMenuEntry(FGenericCommands::Get().Duplicate);
				Section.AddMenuEntry(FDataflowEditorCommands::Get().ToggleEnabledState, FText::FromString("Toggle Enabled State"));
				Section.AddMenuEntry(FGraphEditorCommands::Get().BreakNodeLinks);
				Section.AddMenuEntry(FDataflowEditorCommands::Get().EvaluateNode);
			}
		}

		{
			FToolMenuSection& Section = Menu->AddSection("TestGraphSchemaOrganization", LOCTEXT("GraphSchemaOrganization_MenuHeader", "Organization"));
			{
				Section.AddSubMenu("Alignment", LOCTEXT("AlignmentHeader", "Alignment"), FText(), FNewToolMenuDelegate::CreateLambda([](UToolMenu* AlignmentMenu)
				{
					{
						FToolMenuSection& InSection = AlignmentMenu->AddSection("TestGraphSchemaAlignment", LOCTEXT("GraphSchemaAlignment_MenuHeader", "Align"));

						InSection.AddMenuEntry(FGraphEditorCommands::Get().AlignNodesTop);
						InSection.AddMenuEntry(FGraphEditorCommands::Get().AlignNodesMiddle);
						InSection.AddMenuEntry(FGraphEditorCommands::Get().AlignNodesBottom);
						InSection.AddMenuEntry(FGraphEditorCommands::Get().AlignNodesLeft);
						InSection.AddMenuEntry(FGraphEditorCommands::Get().AlignNodesCenter);
						InSection.AddMenuEntry(FGraphEditorCommands::Get().AlignNodesRight);
						InSection.AddMenuEntry(FGraphEditorCommands::Get().StraightenConnections);
					}

					{
						FToolMenuSection& InSection = AlignmentMenu->AddSection("TestGraphSchemaDistribution", LOCTEXT("GraphSchemaDistribution_MenuHeader", "Distribution"));
						InSection.AddMenuEntry(FGraphEditorCommands::Get().DistributeNodesHorizontally);
						InSection.AddMenuEntry(FGraphEditorCommands::Get().DistributeNodesVertically);
					}
				}));
			}
		}
	}

	Super::GetContextMenuActions(Menu, Context);
}

void UDataflowSchema::GetGraphContextActions(FGraphContextMenuBuilder& ContextMenuBuilder) const
{
	if (Dataflow::FNodeFactory* Factory = Dataflow::FNodeFactory::GetInstance())
	{
		for (Dataflow::FFactoryParameters NodeParameters : Factory->RegisteredParameters())
		{
			if (FDataflowEditorCommands::Get().CreateNodesMap.Contains(NodeParameters.TypeName))
			{
				if (TSharedPtr<FAssetSchemaAction_Dataflow_CreateNode_DataflowEdNode> Action =
					FAssetSchemaAction_Dataflow_CreateNode_DataflowEdNode::CreateAction(ContextMenuBuilder.OwnerOfTemporaries, NodeParameters.TypeName))
				{
					ContextMenuBuilder.AddAction(Action);
				}
			}
		}
	}
}

const FPinConnectionResponse UDataflowSchema::CanCreateConnection(const UEdGraphPin* InPinA, const UEdGraphPin* InPinB) const
{
	bool bSwapped = false;
	const UEdGraphPin* PinA = InPinA;
	const UEdGraphPin* PinB = InPinB;
	if (PinA->Direction == EEdGraphPinDirection::EGPD_Input &&
		PinB->Direction == EEdGraphPinDirection::EGPD_Output)
	{
		bSwapped = true;
		PinA = InPinB; PinB = InPinA;
	}


	if (PinA->Direction == EEdGraphPinDirection::EGPD_Output)
	{
		if (PinB->Direction == EEdGraphPinDirection::EGPD_Input)
		{
			// Make sure the pins are not on the same node
			if (PinA->GetOwningNode() != PinB->GetOwningNode())
			{
				// Make sure types match. 
				if (PinA->PinType == PinB->PinType)
				{
					if (PinB->LinkedTo.Num())
					{
						return (bSwapped) ?
							FPinConnectionResponse(CONNECT_RESPONSE_BREAK_OTHERS_A, LOCTEXT("PinSteal", "Disconnect existing input and connect new input."))
							:
							FPinConnectionResponse(CONNECT_RESPONSE_BREAK_OTHERS_B, LOCTEXT("PinSteal", "Disconnect existing input and connect new input."));

					}
					return FPinConnectionResponse(CONNECT_RESPONSE_MAKE, LOCTEXT("PinConnect", "Connect input to output."));
				}
			}
		}
	}
	TArray<FText> NoConnectionResponse = {
		LOCTEXT("PinErrorSameNode_Nope", "Nope"),
		LOCTEXT("PinErrorSameNode_Sorry", "Sorry :("),
		LOCTEXT("PinErrorSameNode_NotGonnaWork", "Not gonna work."),
		LOCTEXT("PinErrorSameNode_StillNo", "Still no!"),
		LOCTEXT("PinErrorSameNode_TryAgain", "Try again?"),
	};
	return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, NoConnectionResponse[FMath::RandRange(0, NoConnectionResponse.Num()-1)]);
}

FLinearColor UDataflowSchema::GetPinTypeColor(const FEdGraphPinType& PinType) const
{
	return GetTypeColor(PinType.PinCategory);
}

FLinearColor UDataflowSchema::GetTypeColor(const FName& Type)
{
	const UGraphEditorSettings* Settings = GetDefault<UGraphEditorSettings>();
	const UDataflowSettings* DataflowSettings = GetDefault<UDataflowSettings>();

	if (Type == FName("FManagedArrayCollection"))
	{
		return DataflowSettings->ManagedArrayCollectionPinTypeColor;
	}
	else if (Type == FName("float"))
	{
		return Settings->FloatPinTypeColor;
	}
	else if (Type == FName("int32"))
	{
		return Settings->IntPinTypeColor;
	}
	else if (Type == FName("bool"))
	{
		return Settings->BooleanPinTypeColor;
	}
	else if (Type == FName("FString"))
	{
		return Settings->StringPinTypeColor;
	}
	else if (Type == FName("FVector"))
	{
		return Settings->VectorPinTypeColor;
	}
	else if (Type == FName("TArray"))
	{
		return DataflowSettings->ArrayPinTypeColor;
	}
	else if (Type == FName("FBox"))
	{
		return DataflowSettings->BoxPinTypeColor;
	}

	return Settings->DefaultPinTypeColor;
}

//void UDataflowSchema::OnPinConnectionDoubleCicked(UEdGraphPin* PinA, UEdGraphPin* PinB, const FVector2D& GraphPosition) const
//{
//}

FConnectionDrawingPolicy* UDataflowSchema::CreateConnectionDrawingPolicy(int32 InBackLayerID, int32 InFrontLayerID, float InZoomFactor, const FSlateRect& InClippingRect, class FSlateWindowElementList& InDrawElements, class UEdGraph* InGraphObj) const
{
	return new FDataflowConnectionDrawingPolicy(InBackLayerID, InFrontLayerID, InZoomFactor, InClippingRect, InDrawElements, InGraphObj);
}

FDataflowConnectionDrawingPolicy::FDataflowConnectionDrawingPolicy(int32 InBackLayerID, int32 InFrontLayerID, float InZoomFactor, const FSlateRect& InClippingRect, FSlateWindowElementList& InDrawElements, UEdGraph* InGraph)
	: FConnectionDrawingPolicy(InBackLayerID, InFrontLayerID, InZoomFactor, InClippingRect, InDrawElements)
	, Schema((UDataflowSchema*)(InGraph->GetSchema()))
{
	ArrowImage = nullptr;
	ArrowRadius = FVector2D::ZeroVector;
}

void FDataflowConnectionDrawingPolicy::DetermineWiringStyle(UEdGraphPin* OutputPin, UEdGraphPin* InputPin, /*inout*/ FConnectionParams& Params)
{
	FConnectionDrawingPolicy::DetermineWiringStyle(OutputPin, InputPin, Params);
	if (HoveredPins.Contains(InputPin) && HoveredPins.Contains(OutputPin))
	{
		Params.WireThickness = Params.WireThickness * 5;
	}

	const UDataflowSchema* DataflowSchema = GetSchema();
	if (DataflowSchema && OutputPin)
	{
		Params.WireColor = DataflowSchema->GetPinTypeColor(OutputPin->PinType);
	}

	if (OutputPin && InputPin)
	{
		if (OutputPin->bOrphanedPin || InputPin->bOrphanedPin)
		{
			Params.WireColor = FLinearColor::Red;
		}
	}
}

#undef LOCTEXT_NAMESPACE

