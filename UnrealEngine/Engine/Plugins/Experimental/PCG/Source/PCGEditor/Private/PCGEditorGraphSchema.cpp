// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGEditorGraphSchema.h"

#include "Elements/PCGExecuteBlueprint.h"
#include "PCGEditorCommon.h"
#include "PCGEditorGraph.h"
#include "PCGEditorGraphNodeBase.h"
#include "PCGEditorGraphSchemaActions.h"
#include "PCGEditorSettings.h"
#include "PCGGraph.h"
#include "PCGSettings.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/Blueprint.h"
#include "ScopedTransaction.h"
#include "UObject/UObjectIterator.h"
#include "PCGEditorUtils.h"

#define LOCTEXT_NAMESPACE "PCGEditorGraphSchema"


void UPCGEditorGraphSchema::GetPaletteActions(FGraphActionMenuBuilder& ActionMenuBuilder, const EPCGElementType InPCGElementTypeFilter) const
{
	if (!!(InPCGElementTypeFilter & EPCGElementType::Native))
	{
		GetNativeElementActions(ActionMenuBuilder);
	}
	if (!!(InPCGElementTypeFilter & EPCGElementType::Subgraph))
	{
		GetSubgraphElementActions(ActionMenuBuilder);
	}
	if (!!(InPCGElementTypeFilter & EPCGElementType::Blueprint))
	{
		GetBlueprintElementActions(ActionMenuBuilder);
	}
	if (!!(InPCGElementTypeFilter & EPCGElementType::Other))
	{
		GetExtraElementActions(ActionMenuBuilder);
	}
}

void UPCGEditorGraphSchema::GetGraphContextActions(FGraphContextMenuBuilder& ContextMenuBuilder) const
{
	Super::GetGraphContextActions(ContextMenuBuilder);

	GetNativeElementActions(ContextMenuBuilder);
	GetSubgraphElementActions(ContextMenuBuilder);
	GetBlueprintElementActions(ContextMenuBuilder);
	GetExtraElementActions(ContextMenuBuilder);
}

FLinearColor UPCGEditorGraphSchema::GetPinTypeColor(const FEdGraphPinType& PinType) const
{
	return GetDefault<UPCGEditorSettings>()->GetPinColor(PinType);
}

FConnectionDrawingPolicy* UPCGEditorGraphSchema::CreateConnectionDrawingPolicy(int32 InBackLayerID, int32 InFrontLayerID, float InZoomFactor, const FSlateRect& InClippingRect, class FSlateWindowElementList& InDrawElements, class UEdGraph* InGraphObj) const
{
	return new FPCGEditorConnectionDrawingPolicy(InBackLayerID, InFrontLayerID, InZoomFactor, InClippingRect, InDrawElements, InGraphObj);
}

const FPinConnectionResponse UPCGEditorGraphSchema::CanCreateConnection(const UEdGraphPin* A, const UEdGraphPin* B) const
{
	check(A && B);
	const UEdGraphNode* NodeA = A->GetOwningNode();
	const UEdGraphNode* NodeB = B->GetOwningNode();

	if (NodeA == NodeB)
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("ConnectionSameNode", "Both pins are on same node"));
	}

	if (A->Direction == B->Direction)
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("ConnectionSameDirection", "Both pins are the same direction"));
	}

	const UPCGEditorGraphNodeBase* EditorNodeA = CastChecked<const UPCGEditorGraphNodeBase>(NodeA);
	const UPCGEditorGraphNodeBase* EditorNodeB = CastChecked<const UPCGEditorGraphNodeBase>(NodeB);

	// Check type compatibility & whether we can connect more pins
	const UPCGPin* InputPin = nullptr;
	const UPCGPin* OutputPin = nullptr;

	if (A->Direction == EGPD_Output)
	{
		OutputPin = EditorNodeA->GetPCGNode()->GetOutputPin(A->PinName);
		InputPin = EditorNodeB->GetPCGNode()->GetInputPin(B->PinName);
	}
	else
	{
		OutputPin = EditorNodeB->GetPCGNode()->GetOutputPin(B->PinName);
		InputPin = EditorNodeA->GetPCGNode()->GetInputPin(A->PinName);
	}

	if (!InputPin || !OutputPin)
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("ConnectionFailed", "Unable to verify pins"));
	}

	if (!InputPin->IsCompatible(OutputPin))
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("ConnectionTypesIncompatible", "Pins are incompatible"));
	}

	return FPinConnectionResponse();
}

bool UPCGEditorGraphSchema::TryCreateConnection(UEdGraphPin* InA, UEdGraphPin* InB) const
{
	// TODO: check if we need to verify connectivity first
	bool bModified = Super::TryCreateConnection(InA, InB);

	if (bModified)
	{
		check(InA && InB);
		UEdGraphPin* A = (InA->Direction == EGPD_Output) ? InA : InB;
		UEdGraphPin* B = (InA->Direction == EGPD_Input) ? InA : InB;
		
		check(A->Direction == EGPD_Output && B->Direction == EGPD_Input);

		UEdGraphNode* NodeA = A->GetOwningNode();
		UEdGraphNode* NodeB = B->GetOwningNode();

		UPCGEditorGraphNodeBase* PCGGraphNodeA = CastChecked<UPCGEditorGraphNodeBase>(NodeA);
		UPCGEditorGraphNodeBase* PCGGraphNodeB = CastChecked<UPCGEditorGraphNodeBase>(NodeB);

		UPCGNode* PCGNodeA = PCGGraphNodeA->GetPCGNode();
		UPCGNode* PCGNodeB = PCGGraphNodeB->GetPCGNode();
		check(PCGNodeA && PCGNodeB);

		UPCGGraph* PCGGraph = PCGNodeA->GetGraph();
		check(PCGGraph);

		PCGGraph->AddLabeledEdge(PCGNodeA, A->PinName, PCGNodeB, B->PinName);

		// TODO: unclear if that kind of behavior should be down the code hierarchy or not,
		// Since we really want to do cleanup only on manual interaction
		if (UPCGPin* InputPin = PCGNodeB->GetInputPin(B->PinName))
		{
			if (!InputPin->Properties.bAllowMultipleConnections)
			{
				if (InputPin->BreakAllIncompatibleEdges())
				{
					PCGGraphNodeB->ReconstructNode();
				}
			}
		}
	}

	return bModified;
}

void UPCGEditorGraphSchema::BreakPinLinks(UEdGraphPin& TargetPin, bool bSendsNodeNotification) const
{
	const FScopedTransaction Transaction(*FPCGEditorCommon::ContextIdentifier, LOCTEXT("PCGEditorBreakPinLinks", "Break Pin Links"), nullptr);

	Super::BreakPinLinks(TargetPin, bSendsNodeNotification);

	UEdGraphNode* GraphNode = TargetPin.GetOwningNode();

	UPCGEditorGraphNodeBase* PCGGraphNode = CastChecked<UPCGEditorGraphNodeBase>(GraphNode);

	UPCGNode* PCGNode = PCGGraphNode->GetPCGNode();
	check(PCGNode);

	UPCGGraph* PCGGraph = PCGNode->GetGraph();
	check(PCGGraph);

	if (TargetPin.Direction == EEdGraphPinDirection::EGPD_Input)
	{
		PCGGraph->RemoveInboundEdges(PCGNode, TargetPin.PinName);
	}
	else if (TargetPin.Direction == EEdGraphPinDirection::EGPD_Output)
	{
		PCGGraph->RemoveOutboundEdges(PCGNode, TargetPin.PinName);
	}
}

void UPCGEditorGraphSchema::BreakSinglePinLink(UEdGraphPin* SourcePin, UEdGraphPin* TargetPin) const
{
	const FScopedTransaction Transaction(*FPCGEditorCommon::ContextIdentifier, LOCTEXT("PCGEditorBreakSinglePinLink", "Break Single Pin Link"), nullptr);

	Super::BreakSinglePinLink(SourcePin, TargetPin);

	UEdGraphNode* SourceGraphNode = SourcePin->GetOwningNode();
	UEdGraphNode* TargetGraphNode = TargetPin->GetOwningNode();

	UPCGEditorGraphNodeBase* SourcePCGGraphNode = CastChecked<UPCGEditorGraphNodeBase>(SourceGraphNode);
	UPCGEditorGraphNodeBase* TargetPCGGraphNode = CastChecked<UPCGEditorGraphNodeBase>(TargetGraphNode);

	UPCGNode* SourcePCGNode = SourcePCGGraphNode->GetPCGNode();
	UPCGNode* TargetPCGNode = TargetPCGGraphNode->GetPCGNode();
	check(SourcePCGNode && TargetPCGNode);

	UPCGGraph* PCGGraph = SourcePCGNode->GetGraph();
	PCGGraph->RemoveEdge(SourcePCGNode, SourcePin->PinName, TargetPCGNode, TargetPin->PinName);
}

void UPCGEditorGraphSchema::GetNativeElementActions(FGraphActionMenuBuilder& ActionMenuBuilder) const
{
	TArray<UClass*> SettingsClasses;
	for (TObjectIterator<UClass> It; It; ++It)
	{
		UClass* Class = *It;

		if (Class->IsChildOf(UPCGSettings::StaticClass()) &&
			!Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_Hidden))
		{
			SettingsClasses.Add(Class);
		}
	}

	for (UClass* SettingsClass : SettingsClasses)
	{
		if (const UPCGSettings* PCGSettings = SettingsClass->GetDefaultObject<UPCGSettings>())
		{
			const FText MenuDesc = FText::FromName(PCGSettings->GetDefaultNodeName());
			const FText Category = StaticEnum<EPCGSettingsType>()->GetDisplayNameTextByValue(static_cast<__underlying_type(EPCGSettingsType)>(PCGSettings->GetType()));
			const FText Description = FText::GetEmpty();

			TSharedPtr<FPCGEditorGraphSchemaAction_NewNativeElement> NewAction(new FPCGEditorGraphSchemaAction_NewNativeElement(Category, MenuDesc, Description, 0));
			NewAction->SettingsClass = SettingsClass;
			ActionMenuBuilder.AddAction(NewAction);
		}
	}
}

void UPCGEditorGraphSchema::GetBlueprintElementActions(FGraphActionMenuBuilder& ActionMenuBuilder) const
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

	FARFilter Filter;
	Filter.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());
	Filter.bRecursiveClasses = true;
	Filter.TagsAndValues.Add(FBlueprintTags::NativeParentClassPath, UPCGBlueprintElement::GetParentClassName());

	TArray<FAssetData> BlueprintElementAssets;
	AssetRegistryModule.Get().GetAssets(Filter, BlueprintElementAssets);

	for (const FAssetData& AssetData : BlueprintElementAssets)
	{
		const bool bExposeToLibrary = AssetData.GetTagValueRef<bool>(TEXT("bExposeToLibrary"));
		if (bExposeToLibrary)
		{
			const FText MenuDesc = FText::FromName(AssetData.AssetName);
			const FText Category = AssetData.GetTagValueRef<FText>(TEXT("Category"));
			const FText Description = AssetData.GetTagValueRef<FText>(TEXT("Description"));

			const FString GeneratedClass = AssetData.GetTagValueRef<FString>(FBlueprintTags::GeneratedClassPath);

			TSharedPtr<FPCGEditorGraphSchemaAction_NewBlueprintElement> NewBlueprintAction(new FPCGEditorGraphSchemaAction_NewBlueprintElement(Category, MenuDesc, Description, 0));
			NewBlueprintAction->BlueprintClassPath = FSoftClassPath(GeneratedClass);
			ActionMenuBuilder.AddAction(NewBlueprintAction);
		}
	}
}

void UPCGEditorGraphSchema::GetSubgraphElementActions(FGraphActionMenuBuilder& ActionMenuBuilder) const
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

	TArray<FAssetData> AssetDataList;
	AssetRegistryModule.Get().GetAssetsByClass(UPCGGraph::StaticClass()->GetClassPathName(), AssetDataList);

	for (const FAssetData& AssetData : AssetDataList)
	{
		const bool bExposeToLibrary = AssetData.GetTagValueRef<bool>(TEXT("bExposeToLibrary"));
		if (bExposeToLibrary)
		{
			const FText MenuDesc = FText::FromName(AssetData.AssetName);
			const FText Category = AssetData.GetTagValueRef<FText>(TEXT("Category"));
			const FText Description = AssetData.GetTagValueRef<FText>(TEXT("Description"));

			TSharedPtr<FPCGEditorGraphSchemaAction_NewSubgraphElement> NewSubgraphAction(new FPCGEditorGraphSchemaAction_NewSubgraphElement(Category, MenuDesc, Description, 0));
PRAGMA_DISABLE_DEPRECATION_WARNINGS
			NewSubgraphAction->SubgraphObjectPath = AssetData.ObjectPath;
PRAGMA_ENABLE_DEPRECATION_WARNINGS
			ActionMenuBuilder.AddAction(NewSubgraphAction);
		}
	}
}

void UPCGEditorGraphSchema::GetExtraElementActions(FGraphActionMenuBuilder& ActionMenuBuilder) const
{
	// Comment action
	const FText MenuDesc = LOCTEXT("PCGAddComment", "Add Comment...");
	const FText Category;
	const FText Description = LOCTEXT("PCGAddCommentTooltip", "Create a resizable comment box.");

	TSharedPtr<FPCGEditorGraphSchemaAction_NewComment> NewCommentAction(new FPCGEditorGraphSchemaAction_NewComment(Category, MenuDesc, Description, 0));
	ActionMenuBuilder.AddAction(NewCommentAction);
}

void UPCGEditorGraphSchema::DroppedAssetsOnGraph(const TArray<FAssetData>& Assets, const FVector2D& GraphPosition, UEdGraph* Graph) const
{
	FVector2D GraphPositionOffset = GraphPosition;
	constexpr float PositionOffsetIncrementY = 50.f;
	UEdGraphPin* NullFromPin = nullptr;

	for (const FAssetData& AssetData : Assets)
	{
		if (const UObject* Asset = AssetData.GetAsset())
		{
			if (Asset->IsA<UPCGGraph>())
			{
				FPCGEditorGraphSchemaAction_NewSubgraphElement NewSubgraphAction;
PRAGMA_DISABLE_DEPRECATION_WARNINGS
				NewSubgraphAction.SubgraphObjectPath = AssetData.ObjectPath;
PRAGMA_ENABLE_DEPRECATION_WARNINGS
				NewSubgraphAction.PerformAction(Graph, NullFromPin, GraphPositionOffset);
				GraphPositionOffset.Y += PositionOffsetIncrementY;
			}
			else if (PCGEditorUtils::IsAssetPCGBlueprint(AssetData))
			{
				const FString GeneratedClass = AssetData.GetTagValueRef<FString>(FBlueprintTags::GeneratedClassPath);

				FPCGEditorGraphSchemaAction_NewBlueprintElement NewBlueprintAction;
				NewBlueprintAction.BlueprintClassPath = FSoftClassPath(GeneratedClass);
				NewBlueprintAction.PerformAction(Graph, NullFromPin, GraphPositionOffset);
				GraphPositionOffset.Y += PositionOffsetIncrementY;
			}
		}
	}
}

void UPCGEditorGraphSchema::GetAssetsGraphHoverMessage(const TArray<FAssetData>& Assets, const UEdGraph* /*HoverGraph*/, FString& OutTooltipText, bool& OutOkIcon) const
{
	for (const FAssetData& AssetData : Assets)
	{
		if (const UObject* Asset = AssetData.GetAsset())
		{
			if (Asset->IsA<UPCGGraph>() || PCGEditorUtils::IsAssetPCGBlueprint(AssetData))
			{
				OutOkIcon = true;
				return;
			}
			else if (Asset->IsA<UBlueprint>())
			{
				OutTooltipText = LOCTEXT("PCGEditorDropAssetInvalidBP", "Blueprint does not derive from UPCGBlueprintElement").ToString();
				OutOkIcon = false;
				return;
			}
		}
	}

	OutTooltipText = LOCTEXT("PCGEditorDropAssetInvalid", "Can't create a node for this asset").ToString();
	OutOkIcon = false;
}


FPCGEditorConnectionDrawingPolicy::FPCGEditorConnectionDrawingPolicy(int32 InBackLayerID, int32 InFrontLayerID, float InZoomFactor, const FSlateRect& InClippingRect, FSlateWindowElementList& InDrawElements, UEdGraph* InGraph)
	: FConnectionDrawingPolicy(InBackLayerID, InFrontLayerID, InZoomFactor, InClippingRect, InDrawElements)
	, Graph(CastChecked<UPCGEditorGraph>(InGraph))
{
	ArrowImage = nullptr;
	ArrowRadius = FVector2D::ZeroVector;
}

void FPCGEditorConnectionDrawingPolicy::DetermineWiringStyle(UEdGraphPin* OutputPin, UEdGraphPin* InputPin, /*inout*/ FConnectionParams& Params)
{
	FConnectionDrawingPolicy::DetermineWiringStyle(OutputPin, InputPin, Params);
	// Emphasize wire thickness on hovered pins
	if (HoveredPins.Contains(InputPin) && HoveredPins.Contains(OutputPin))
	{
		Params.WireThickness = Params.WireThickness * 3;
	}

	// Base the color of the wire on the color of the output pin
	if (OutputPin)
	{
		Params.WireColor = GetDefault<UPCGEditorSettings>()->GetPinColor(OutputPin->PinType);
	}
}

#undef LOCTEXT_NAMESPACE
