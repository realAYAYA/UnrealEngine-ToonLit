// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGEditorGraphSchema.h"

#include "PCGComponent.h"
#include "PCGDataAsset.h"
#include "PCGEdge.h"
#include "PCGGraph.h"
#include "PCGPin.h"
#include "Elements/PCGCollapseElement.h"
#include "Elements/PCGCreateSurfaceFromSpline.h"
#include "Elements/PCGExecuteBlueprint.h"
#include "Elements/PCGFilterByType.h"
#include "Elements/PCGMakeConcreteElement.h"
#include "Elements/PCGReroute.h"
#include "Elements/PCGUserParameterGet.h"

#include "PCGEditor.h"
#include "PCGEditorCommon.h"
#include "PCGEditorGraph.h"
#include "PCGEditorGraphNodeBase.h"
#include "PCGEditorGraphNodeReroute.h"
#include "PCGEditorGraphSchemaActions.h"
#include "PCGEditorSettings.h"
#include "PCGEditorUtils.h"

#include "AssetRegistry/AssetData.h"
#include "Blueprint/BlueprintSupport.h"
#include "Engine/Blueprint.h"
#include "Framework/Application/SlateApplication.h"
#include "ScopedTransaction.h"
#include "UObject/UObjectIterator.h"

#include "SGraphPanel.h"

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
	if (!!(InPCGElementTypeFilter & EPCGElementType::Settings))
	{
		GetSettingsElementActions(ActionMenuBuilder, /*bIsContextual=*/false);
	}
	if (!!(InPCGElementTypeFilter & EPCGElementType::Asset))
	{
		GetDataAssetActions(ActionMenuBuilder);
	}
	if (!!(InPCGElementTypeFilter & EPCGElementType::Other))
	{
		GetNamedRerouteUsageActions(ActionMenuBuilder);
		GetExtraElementActions(ActionMenuBuilder);
	}
}

void UPCGEditorGraphSchema::GetGraphContextActions(FGraphContextMenuBuilder& ContextMenuBuilder) const
{
	Super::GetGraphContextActions(ContextMenuBuilder);

	GetNativeElementActions(ContextMenuBuilder, ContextMenuBuilder.CurrentGraph);
	GetSubgraphElementActions(ContextMenuBuilder);
	GetBlueprintElementActions(ContextMenuBuilder);
	GetSettingsElementActions(ContextMenuBuilder, /*bIsContextual=*/true);
	GetDataAssetActions(ContextMenuBuilder);
	GetNamedRerouteUsageActions(ContextMenuBuilder, ContextMenuBuilder.CurrentGraph);
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
	const UPCGEditorGraphNodeBase* EditorNodeWithInput = nullptr;

	// Check type compatibility & whether we can connect more pins
	const UPCGPin* InputPin = nullptr;
	const UPCGPin* OutputPin = nullptr;

	if (A->Direction == EGPD_Output)
	{
		OutputPin = EditorNodeA->GetPCGNode()->GetOutputPin(A->PinName);
		InputPin = EditorNodeB->GetPCGNode()->GetInputPin(B->PinName);
		EditorNodeWithInput = EditorNodeB;
	}
	else
	{
		OutputPin = EditorNodeB->GetPCGNode()->GetOutputPin(B->PinName);
		InputPin = EditorNodeA->GetPCGNode()->GetInputPin(A->PinName);
		EditorNodeWithInput = EditorNodeA;
	}

	if (!InputPin || !OutputPin)
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("ConnectionFailed", "Unable to verify pins"));
	}

	if (!InputPin->IsCompatible(OutputPin))
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("ConnectionTypesIncompatible", "Pins are incompatible"));
	}

	const EPCGTypeConversion RequiredTypeConversion = InputPin->GetRequiredTypeConversion(OutputPin);
	if (RequiredTypeConversion == EPCGTypeConversion::CollapseToPoint)
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_MAKE_WITH_CONVERSION_NODE, LOCTEXT("ConnectionConversionToPoint", "Convert to Point"));
	}
	else if (RequiredTypeConversion == EPCGTypeConversion::Filter)
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_MAKE_WITH_CONVERSION_NODE, LOCTEXT("ConnectionUsingFilter", "Filter data to match type"));
	}
	else if (RequiredTypeConversion == EPCGTypeConversion::MakeConcrete)
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_MAKE_WITH_CONVERSION_NODE, LOCTEXT("ConnectionUsingMakeConcrete", "Make data concrete"));
	}

	if (!InputPin->AllowsMultipleConnections() && InputPin->EdgeCount() > 0)
	{
		return FPinConnectionResponse((A->Direction == EGPD_Output) ? CONNECT_RESPONSE_BREAK_OTHERS_B : CONNECT_RESPONSE_BREAK_OTHERS_A, LOCTEXT("ConnectionBreakExisting", "Break existing connection?"));
	}

	FText Reason;
	if (!EditorNodeWithInput->IsCompatible(InputPin, OutputPin, Reason))
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, Reason);
	}

	return FPinConnectionResponse();
}

bool UPCGEditorGraphSchema::TryCreateConnection(UEdGraphPin* InA, UEdGraphPin* InB) const
{
	return TryCreateConnectionInternal(InA, InB, /*bAddConversionNodeIfNeeded=*/true);
}

bool UPCGEditorGraphSchema::TryCreateConnectionInternal(UEdGraphPin* InA, UEdGraphPin* InB, bool bAddConversionNodeIfNeeded) const
{
	check(InA && InB);
	if (InA->Direction == InB->Direction)
	{
		// Don't connect same polarity
		return false;
	}

	UEdGraphPin* A = (InA->Direction == EGPD_Output) ? InA : InB;
	UEdGraphPin* B = (InA->Direction == EGPD_Input) ? InA : InB;
	check(A->Direction == EGPD_Output && B->Direction == EGPD_Input);

	UEdGraphNode* NodeA = A->GetOwningNodeUnchecked();
	UEdGraphNode* NodeB = B->GetOwningNodeUnchecked();
	if (!ensure(NodeA && NodeB))
	{
		// TODO: We've had crashes where one of these nodes was nullptr, we need to figure out why this can happen.
		return false;
	}

	UPCGEditorGraphNodeBase* PCGEdGraphNodeA = CastChecked<UPCGEditorGraphNodeBase>(NodeA);
	UPCGEditorGraphNodeBase* PCGEdGraphNodeB = CastChecked<UPCGEditorGraphNodeBase>(NodeB);

	UPCGNode* PCGNodeA = PCGEdGraphNodeA->GetPCGNode();
	UPCGNode* PCGNodeB = PCGEdGraphNodeB->GetPCGNode();
	check(PCGNodeA && PCGNodeB);

	const UPCGPin* PCGPinA = PCGNodeA->GetOutputPin(A->PinName);
	const UPCGPin* PCGPinB = PCGNodeB->GetInputPin(B->PinName);
	check(PCGPinA && PCGPinB);
	if (!PCGPinA->IsCompatible(PCGPinB))
	{
		return false;
	}

	UPCGGraph* PCGGraph = PCGNodeA->GetGraph();
	check(PCGGraph);

	// UPCGEditorGraphSchema::TryCreateConnectionInternal is called directly by FDragConnection::DroppedOnPin
	PCGGraph->PrimeGraphCompilationCache();

	// Creates a connection via an intermediate conversion node.
	auto ConnectViaIntermediate = [this, PCGGraph, NodeA, NodeB, A, B](UPCGNode* IntermediateNode)
	{
		UEdGraph* Graph = NodeA->GetGraph();
		check(Graph);
		Graph->Modify();

		FGraphNodeCreator<UPCGEditorGraphNode> NodeCreator(*Graph);
		UPCGEditorGraphNode* ConversionNode = NodeCreator.CreateUserInvokedNode(/*bSelectNewNode=*/false);
		ConversionNode->Construct(IntermediateNode);

		// Put the conversion node between A & B but make it stay within a radius of B to keep things tidy.
		{
			// Initially place at mid point
			ConversionNode->NodePosX = (NodeA->NodePosX + NodeB->NodePosX) / 2;
			ConversionNode->NodePosY = (NodeA->NodePosY + NodeB->NodePosY) / 2;

			// A hand tweaked distance that keeps it reasonably close.
			constexpr float MaxDistFromB = 200;
			const FVector2D OffsetFromB(ConversionNode->NodePosX - NodeB->NodePosX, ConversionNode->NodePosY - NodeB->NodePosY);
			const float Dist = OffsetFromB.Length();
			if (Dist > MaxDistFromB)
			{
				const float Scale = MaxDistFromB / Dist;
				ConversionNode->NodePosX = NodeB->NodePosX + Scale * OffsetFromB.X;
				ConversionNode->NodePosY = NodeB->NodePosY + Scale * OffsetFromB.Y;
			}
		}

		NodeCreator.Finalize();

		IntermediateNode->PositionX = ConversionNode->NodePosX;
		IntermediateNode->PositionY = ConversionNode->NodePosY;

		bool bModifiedA = false, bModifiedB = false;

		UEdGraphPin*const* ConversionInputPin = ConversionNode->GetAllPins().FindByPredicate([](const UEdGraphPin* InPin)
		{
			return InPin->Direction == EGPD_Input && InPin->GetFName() == PCGPinConstants::DefaultInputLabel;
		});

		if (ensure(ConversionInputPin && *ConversionInputPin))
		{
			// Last argument: don't allow recursively adding conversion nodes.
			bModifiedA = TryCreateConnectionInternal(A, *ConversionInputPin, /*bAddConversionNodeIfNeeded=*/false);
		}

		// Call GetAllPins() a second time. It's important that we wire up the pins one at a time. Wiring a pin can change dynamic pin types
		// which can refresh the node, so we must re-query the pins after each connection is made.
		UEdGraphPin*const* ConversionOutputPin = ConversionNode->GetAllPins().FindByPredicate([](const UEdGraphPin* InPin)
		{
			return InPin->Direction == EGPD_Output && (InPin->GetFName() == PCGPinConstants::DefaultOutputLabel || InPin->GetFName() == PCGPinConstants::DefaultInFilterLabel);
		});

		if (ensure(ConversionOutputPin && *ConversionOutputPin))
		{
			// Last argument: don't allow recursively adding conversion nodes.
			bModifiedB = TryCreateConnectionInternal(*ConversionOutputPin, B, /*bAddConversionNodeIfNeeded=*/false);
		}

		return bModifiedA || bModifiedB;
	};

	const EPCGTypeConversion Conversion = bAddConversionNodeIfNeeded ? PCGPinA->GetRequiredTypeConversion(PCGPinB) : EPCGTypeConversion::NoConversionRequired;
	if (Conversion == EPCGTypeConversion::CollapseToPoint)
	{
		UPCGSettings* NodeSettings = nullptr;
		UPCGNode* ConversionPCGNode = PCGGraph->AddNodeOfType(UPCGCollapseSettings::StaticClass(), NodeSettings);

		return ConnectViaIntermediate(ConversionPCGNode);
	}
	else if (Conversion == EPCGTypeConversion::Filter)
	{
		UPCGSettings* NodeSettings = nullptr;
		UPCGNode* ConversionPCGNode = PCGGraph->AddNodeOfType(UPCGFilterByTypeSettings::StaticClass(), NodeSettings);

		// Setup the output pin based on the conversion target type, before new node is finalized.
		UPCGFilterByTypeSettings* Settings = CastChecked<UPCGFilterByTypeSettings>(NodeSettings);
		Settings->TargetType = PCGPinB->Properties.AllowedTypes;
		ConversionPCGNode->UpdateAfterSettingsChangeDuringCreation();

		return ConnectViaIntermediate(ConversionPCGNode);
	}
	else if (Conversion == EPCGTypeConversion::MakeConcrete)
	{
		UPCGSettings* NodeSettings = nullptr;
		UPCGNode* ConversionPCGNode = PCGGraph->AddNodeOfType(UPCGMakeConcreteSettings::StaticClass(), NodeSettings);

		return ConnectViaIntermediate(ConversionPCGNode);
	}
	else if (Conversion == EPCGTypeConversion::SplineToSurface)
	{
		UPCGSettings* NodeSettings = nullptr;
		UPCGNode* ConversionPCGNode = PCGGraph->AddNodeOfType(UPCGCreateSurfaceFromSplineSettings::StaticClass(), NodeSettings);

		UPCGCreateSurfaceFromSplineSettings* Settings = CastChecked<UPCGCreateSurfaceFromSplineSettings>(NodeSettings);
		Settings->bShouldDrawNodeCompact = true;

		return ConnectViaIntermediate(ConversionPCGNode);
	}
	else
	{
		const bool bModified = Super::TryCreateConnection(InA, InB);
		if (bModified)
		{
			PCGGraph->AddLabeledEdge(PCGNodeA, A->PinName, PCGNodeB, B->PinName);
		}

		return bModified;
	}
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

void UPCGEditorGraphSchema::GetNativeElementActions(FGraphActionMenuBuilder& ActionMenuBuilder, const UEdGraph* CurrentGraph) const
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
			if (PCGSettings->bExposeToLibrary)
			{
				const FText MenuDesc = PCGSettings->GetDefaultNodeTitle();
				const FText Category = StaticEnum<EPCGSettingsType>()->GetDisplayNameTextByValue(static_cast<__underlying_type(EPCGSettingsType)>(PCGSettings->GetType()));
				const FText Description = PCGSettings->GetNodeTooltipText();

				TArray<FPCGPreConfiguredSettingsInfo> AllPreconfiguredInfo = PCGSettings->GetPreconfiguredInfo();

				if (AllPreconfiguredInfo.IsEmpty() || !PCGSettings->OnlyExposePreconfiguredSettings())
				{
					TSharedPtr<FPCGEditorGraphSchemaAction_NewNativeElement> NewAction(new FPCGEditorGraphSchemaAction_NewNativeElement(Category, MenuDesc, Description, 0));
					NewAction->SettingsClass = SettingsClass;
					ActionMenuBuilder.AddAction(NewAction);

					// Also add all aliases
					for (const FText& Alias : PCGSettings->GetNodeTitleAliases())
					{
						TSharedPtr<FPCGEditorGraphSchemaAction_NewNativeElement> NewAliasAction(new FPCGEditorGraphSchemaAction_NewNativeElement(Category, Alias, Description, 0));
						NewAliasAction->SettingsClass = SettingsClass;
						ActionMenuBuilder.AddAction(NewAliasAction);
					}
				}

				// Also add preconfigured settings
				const FText NewCategory = PCGSettings->GroupPreconfiguredSettings() ? FText::Format(LOCTEXT("PreconfiguredSettingsCategory", "{0}|{1}"), Category, MenuDesc) : Category;

				for (FPCGPreConfiguredSettingsInfo PreconfiguredInfo : AllPreconfiguredInfo)
				{
					TSharedPtr<FPCGEditorGraphSchemaAction_NewNativeElement> NewPreconfiguredAction(new FPCGEditorGraphSchemaAction_NewNativeElement(NewCategory, PreconfiguredInfo.Label, PreconfiguredInfo.Tooltip.IsEmpty() ? Description : PreconfiguredInfo.Tooltip, 0));
					NewPreconfiguredAction->SettingsClass = SettingsClass;
					NewPreconfiguredAction->PreconfiguredInfo = std::move(PreconfiguredInfo);
					ActionMenuBuilder.AddAction(NewPreconfiguredAction);
				}
			}
		}
	}

	if (const UPCGEditorGraph* Graph = Cast<UPCGEditorGraph>(CurrentGraph))
	{
		if (const UPCGGraph* PCGGraph = const_cast<UPCGEditorGraph*>(Graph)->GetPCGGraph())
		{
			if (const FInstancedPropertyBag* UserParameters = PCGGraph->GetUserParametersStruct())
			{
				if (const UPropertyBag* BagStruct = UserParameters->GetPropertyBagStruct())
				{
					const FText Category = LOCTEXT("UserParametersCategoryName", "Graph Parameters");

					for (const FPropertyBagPropertyDesc& PropertyDesc : BagStruct->GetPropertyDescs())
					{
						const FText MenuDesc = FText::Format(LOCTEXT("GetterNodeName", "Get {0}"), FText::FromName(PropertyDesc.Name));
						const FText Description = FText::Format(LOCTEXT("NodeTooltip", "Get the value from '{0}' parameter, can be overridden by the graph instance."), FText::FromName(PropertyDesc.Name));

						TSharedPtr<FPCGEditorGraphSchemaAction_NewGetParameterElement> NewAction(new FPCGEditorGraphSchemaAction_NewGetParameterElement(Category, MenuDesc, Description, 0));
						NewAction->SettingsClass = UPCGUserParameterGetSettings::StaticClass();
						NewAction->PropertyName = PropertyDesc.Name;
						NewAction->PropertyGuid = PropertyDesc.ID;
						ActionMenuBuilder.AddAction(NewAction);
					}
				}
			}
		}
	}
}

void UPCGEditorGraphSchema::GetBlueprintElementActions(FGraphActionMenuBuilder& ActionMenuBuilder) const
{
	PCGEditorUtils::ForEachPCGBlueprintAssetData([&ActionMenuBuilder](const FAssetData& AssetData)
	{
		const bool bExposeToLibrary = AssetData.GetTagValueRef<bool>(GET_MEMBER_NAME_CHECKED(UPCGBlueprintElement, bExposeToLibrary));
		const bool bOnlyExposePreconfiguredSettings = AssetData.GetTagValueRef<bool>(GET_MEMBER_NAME_CHECKED(UPCGBlueprintElement, bOnlyExposePreconfiguredSettings));

		if (bExposeToLibrary)
		{
			const FText MenuDesc = FText::FromString(FName::NameToDisplayString(AssetData.AssetName.ToString(), false));
			const FText Category = AssetData.GetTagValueRef<FText>(GET_MEMBER_NAME_CHECKED(UPCGBlueprintElement, Category));
			const FText Description = AssetData.GetTagValueRef<FText>(GET_MEMBER_NAME_CHECKED(UPCGBlueprintElement, Description));

			const FSoftClassPath GeneratedClass = FSoftClassPath(AssetData.GetTagValueRef<FString>(FBlueprintTags::GeneratedClassPath));

			// Only load the class if we have enabled preconfigured settings.
			TArray<FPCGPreConfiguredSettingsInfo> AllPreconfiguredInfo;
			if (AssetData.GetTagValueRef<bool>(GET_MEMBER_NAME_CHECKED(UPCGBlueprintElement, bEnablePreconfiguredSettings)))
			{
				TSubclassOf<UPCGBlueprintElement> BlueprintClass = GeneratedClass.TryLoadClass<UPCGBlueprintElement>();
				const UPCGBlueprintElement* BlueprintElement = BlueprintClass ? Cast<UPCGBlueprintElement>(BlueprintClass->GetDefaultObject()) : nullptr;

				AllPreconfiguredInfo = BlueprintElement ? BlueprintElement->PreconfiguredInfo : TArray<FPCGPreConfiguredSettingsInfo>{};
			}

			if (AllPreconfiguredInfo.IsEmpty() || !bOnlyExposePreconfiguredSettings)
			{
				TSharedPtr<FPCGEditorGraphSchemaAction_NewBlueprintElement> NewBlueprintAction(new FPCGEditorGraphSchemaAction_NewBlueprintElement(Category, MenuDesc, Description, 0));
				NewBlueprintAction->BlueprintClassPath = GeneratedClass;
				ActionMenuBuilder.AddAction(NewBlueprintAction);
			}

			// Also add preconfigured settings
			const FText NewCategory = FText::Format(LOCTEXT("PreconfiguredSettingsCategory", "{0}|{1}"), Category, MenuDesc);

			for (FPCGPreConfiguredSettingsInfo PreconfiguredInfo : AllPreconfiguredInfo)
			{
				TSharedPtr<FPCGEditorGraphSchemaAction_NewBlueprintElement> NewPreconfiguredAction(new FPCGEditorGraphSchemaAction_NewBlueprintElement(NewCategory, PreconfiguredInfo.Label, PreconfiguredInfo.Tooltip.IsEmpty() ? Description : PreconfiguredInfo.Tooltip, 0));
				NewPreconfiguredAction->BlueprintClassPath = GeneratedClass;
				NewPreconfiguredAction->PreconfiguredInfo = std::move(PreconfiguredInfo);
				ActionMenuBuilder.AddAction(NewPreconfiguredAction);
			}
		}

		return true;
	});
}

void UPCGEditorGraphSchema::GetSettingsElementActions(FGraphActionMenuBuilder& ActionMenuBuilder, bool bIsContextual) const
{
	PCGEditorUtils::ForEachPCGSettingsAssetData([&ActionMenuBuilder, bIsContextual](const FAssetData& AssetData)
	{
		const bool bExposeToLibrary = AssetData.GetTagValueRef<bool>(TEXT("bExposeToLibrary"));
		if (bExposeToLibrary)
		{
			const FText MenuDesc = FText::FromString(FName::NameToDisplayString(AssetData.AssetName.ToString(), false));
			const FText Category = AssetData.GetTagValueRef<FText>(TEXT("Category"));
			const FText Description = AssetData.GetTagValueRef<FText>(TEXT("Description"));

			if (!bIsContextual)
			{
				TSharedPtr<FPCGEditorGraphSchemaAction_NewSettingsElement> NewSettingsAction(new FPCGEditorGraphSchemaAction_NewSettingsElement(Category, MenuDesc, Description, 0));
				NewSettingsAction->SettingsObjectPath = AssetData.GetSoftObjectPath();
				ActionMenuBuilder.AddAction(NewSettingsAction);
			}
			else
			{
				const FText MenuAndSubCategory = FText::Join(LOCTEXT("MenuDelimiter", "|"), Category, MenuDesc);

				TSharedPtr<FPCGEditorGraphSchemaAction_NewSettingsElement> NewSettingsActionCopy(new FPCGEditorGraphSchemaAction_NewSettingsElement(MenuAndSubCategory, LOCTEXT("ContextMenuCopySettings", "Copy"), Description, 0));
				NewSettingsActionCopy->SettingsObjectPath = AssetData.GetSoftObjectPath();
				NewSettingsActionCopy->Behavior = EPCGEditorNewSettingsBehavior::ForceCopy;
				ActionMenuBuilder.AddAction(NewSettingsActionCopy);

				TSharedPtr<FPCGEditorGraphSchemaAction_NewSettingsElement> NewSettingsActionInstance(new FPCGEditorGraphSchemaAction_NewSettingsElement(MenuAndSubCategory, LOCTEXT("ContextMenuInstanceSettings", "Instance"), Description, 0));
				NewSettingsActionInstance->SettingsObjectPath = AssetData.GetSoftObjectPath();
				NewSettingsActionInstance->Behavior = EPCGEditorNewSettingsBehavior::ForceInstance;
				ActionMenuBuilder.AddAction(NewSettingsActionInstance);
			}
		}

		return true;
	});
}

void UPCGEditorGraphSchema::GetSubgraphElementActions(FGraphActionMenuBuilder& ActionMenuBuilder) const
{
	PCGEditorUtils::ForEachPCGGraphAssetData([&ActionMenuBuilder](const FAssetData& AssetData)
	{
		const bool bExposeToLibrary = AssetData.GetTagValueRef<bool>(TEXT("bExposeToLibrary"));
		if (bExposeToLibrary)
		{
			const FText MenuDesc = FText::FromString(FName::NameToDisplayString(AssetData.AssetName.ToString(), false));
			const FText Category = AssetData.GetTagValueRef<FText>(TEXT("Category"));
			const FText Description = AssetData.GetTagValueRef<FText>(TEXT("Description"));

			TSharedPtr<FPCGEditorGraphSchemaAction_NewSubgraphElement> NewSubgraphAction(new FPCGEditorGraphSchemaAction_NewSubgraphElement(Category, MenuDesc, Description, 0));
			NewSubgraphAction->SubgraphObjectPath = AssetData.GetSoftObjectPath();
			ActionMenuBuilder.AddAction(NewSubgraphAction);
		}

		return true;
	});
}

void UPCGEditorGraphSchema::GetExtraElementActions(FGraphActionMenuBuilder& ActionMenuBuilder) const
{
	const FText NoCategory;
	// Comment action
	const FText CommentMenuDesc = LOCTEXT("PCGAddComment", "Add Comment...");
	const FText CommentDescription = LOCTEXT("PCGAddCommentTooltip", "Create a resizable comment box.");

	const TSharedPtr<FPCGEditorGraphSchemaAction_NewComment> NewCommentAction(new FPCGEditorGraphSchemaAction_NewComment(NoCategory, CommentMenuDesc, CommentDescription, 0));
	ActionMenuBuilder.AddAction(NewCommentAction);

	// Reroute action
	const FText RerouteMenuDesc = LOCTEXT("PCGAddRerouteNode", "Add Reroute Node");
	const FText RerouteDescription = LOCTEXT("PCGAddRerouteNodeTooltip", "Add a reroute node, aka knot.");

	const TSharedPtr<FPCGEditorGraphSchemaAction_NewReroute> NewRerouteAction(new FPCGEditorGraphSchemaAction_NewReroute(NoCategory, RerouteMenuDesc, RerouteDescription, 0));
	ActionMenuBuilder.AddAction(NewRerouteAction);

	// Named reroute declaration action
	const FText NamedRerouteMenuDesc = LOCTEXT("PCGAddNamedRerouteDeclarationNode", "Add Named Reroute Declaration Node...");
	const FText NamedRerouteDescription = LOCTEXT("PCGAddNamedRerouteDeclarationNodeTooltip", "Creates a new Named Reroute Declaration from the input.");

	const TSharedPtr<FPCGEditorGraphSchemaAction_NewNamedRerouteDeclaration> NewNamedRerouteDeclarationAction(new FPCGEditorGraphSchemaAction_NewNamedRerouteDeclaration(NoCategory, NamedRerouteMenuDesc, NamedRerouteDescription, 0));
	ActionMenuBuilder.AddAction(NewNamedRerouteDeclarationAction);
}

void UPCGEditorGraphSchema::GetNamedRerouteUsageActions(FGraphActionMenuBuilder& ActionMenuBuilder, const UEdGraph* CurrentGraph) const
{
	const UPCGEditorGraph* Graph = Cast<UPCGEditorGraph>(CurrentGraph);

	if (!Graph)
	{
		return;
	}

	const UPCGGraph* PCGGraph = const_cast<UPCGEditorGraph*>(Graph)->GetPCGGraph();

	if (!PCGGraph)
	{
		return;
	}

	for (const UPCGNode* Node : PCGGraph->GetNodes())
	{
		if (const UPCGNamedRerouteDeclarationSettings* RerouteDeclaration = Cast<UPCGNamedRerouteDeclarationSettings>(Node->GetSettings()))
		{
			static const FText Category = LOCTEXT("NamedRerouteCategory", "Named Reroutes");
			const FText Name = FText::FromName(Node->NodeTitle);
			const FText Tooltip = FText::Format(LOCTEXT("NamedRerouteTooltip", "Add a usage of '{0}' here."), Name);
			TSharedPtr<FPCGEditorGraphSchemaAction_NewNamedRerouteUsage> NewRerouteAction(new FPCGEditorGraphSchemaAction_NewNamedRerouteUsage(Category, Name, Tooltip, 1 /* We want named reroutes to be on top */));
			NewRerouteAction->DeclarationNode = Node;
			ActionMenuBuilder.AddAction(NewRerouteAction);
		}
	}
}

void UPCGEditorGraphSchema::GetDataAssetActions(FGraphActionMenuBuilder& ActionMenuBuilder) const
{
	PCGEditorUtils::ForEachPCGAssetData([&ActionMenuBuilder](const FAssetData& AssetData)
	{
		const bool bExposeToLibrary = AssetData.GetTagValueRef<bool>(GET_MEMBER_NAME_CHECKED(UPCGDataAsset, bExposeToLibrary));
		if (bExposeToLibrary)
		{
			const FText MenuDesc = FText::FromString(FName::NameToDisplayString(AssetData.AssetName.ToString(), false));
			const FText Category = AssetData.GetTagValueRef<FText>(GET_MEMBER_NAME_CHECKED(UPCGDataAsset, Category));
			const FText Description = AssetData.GetTagValueRef<FText>(GET_MEMBER_NAME_CHECKED(UPCGDataAsset, Description));
			const FString AssetName = AssetData.GetTagValueRef<FString>(GET_MEMBER_NAME_CHECKED(UPCGDataAsset, Name));

			const FSoftClassPath SettingsClassPath = FSoftClassPath(AssetData.GetTagValueRef<FString>(GET_MEMBER_NAME_CHECKED(UPCGDataAsset, SettingsClass)));
			TSubclassOf<UPCGSettings> SettingsClass = SettingsClassPath.TryLoadClass<UPCGSettings>();

			TSharedPtr<FPCGEditorGraphSchemaAction_NewLoadAssetElement> NewLoadDataAssetAction(new FPCGEditorGraphSchemaAction_NewLoadAssetElement(Category, AssetName.IsEmpty() ? MenuDesc : FText::FromString(AssetName), Description, 0));
			NewLoadDataAssetAction->Asset = AssetData;
			NewLoadDataAssetAction->SettingsClass = SettingsClass;

			ActionMenuBuilder.AddAction(NewLoadDataAssetAction);
		}

		return true;
	});
}

void UPCGEditorGraphSchema::DroppedAssetsOnGraph(const TArray<FAssetData>& Assets, const FVector2D& GraphPosition, UEdGraph* Graph) const
{
	FVector2D GraphPositionOffset = GraphPosition;
	constexpr float PositionOffsetIncrementY = 50.f;
	UEdGraphPin* NullFromPin = nullptr;

	TArray<FSoftObjectPath> SubgraphPaths;
	TArray<FVector2D> SubgraphPositions;

	TArray<FSoftObjectPath> SettingsPaths;
	TArray<FVector2D> SettingsGraphPositions;

	for (const FAssetData& AssetData : Assets)
	{
		if(const UClass* AssetClass = AssetData.GetClass())
		{
			if(AssetClass->IsChildOf(UPCGGraphInterface::StaticClass()))
			{
				SubgraphPaths.Add(AssetData.GetSoftObjectPath());
				SubgraphPositions.Add(GraphPositionOffset);
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
			else if(AssetClass->IsChildOf(UPCGDataAsset::StaticClass()))
			{
				FPCGEditorGraphSchemaAction_NewLoadAssetElement NewLoadAssetAction;
				NewLoadAssetAction.Asset = AssetData;
				NewLoadAssetAction.SettingsClass = FSoftClassPath(AssetData.GetTagValueRef<FString>(GET_MEMBER_NAME_CHECKED(UPCGDataAsset, SettingsClass))).TryLoadClass<UPCGSettings>();
				NewLoadAssetAction.PerformAction(Graph, NullFromPin, GraphPositionOffset);
				GraphPositionOffset.Y += PositionOffsetIncrementY;
			}
			else if(AssetClass->IsChildOf(UPCGSettings::StaticClass()))
			{
				// Delay creation so we can open a menu, once, if needed.
				SettingsPaths.Add(AssetData.GetSoftObjectPath());
				SettingsGraphPositions.Add(GraphPositionOffset);
				GraphPositionOffset.Y += PositionOffsetIncrementY;
			}
		}
	}

	UPCGEditorGraph* EditorGraph = CastChecked<UPCGEditorGraph>(Graph);

	TSharedPtr<SGraphEditor> GraphEditor = SGraphEditor::FindGraphEditorForGraph(EditorGraph);
	const FVector2D MouseCursorLocation = FSlateApplication::Get().GetCursorPos();

	// If we've dragged settings assets or a graph, we might want to open a menu (ergo this call)
	if (!SettingsPaths.IsEmpty())
	{
		check(SettingsPaths.Num() == SettingsGraphPositions.Num());
		FPCGEditorGraphSchemaAction_NewSettingsElement::MakeSettingsNodesOrContextualMenu(GraphEditor->GetGraphPanel()->AsShared(), MouseCursorLocation, Graph, SettingsPaths, SettingsGraphPositions, /*bSelectNewNodes=*/true);
	}

	if (!SubgraphPaths.IsEmpty())
	{
		check(SubgraphPaths.Num() == SubgraphPositions.Num());
		FPCGEditorGraphSchemaAction_NewSubgraphElement::MakeGraphNodesOrContextualMenu(GraphEditor->GetGraphPanel()->AsShared(), MouseCursorLocation, Graph, SubgraphPaths, SubgraphPositions, /*bSelectNewNodes=*/true);
	}
}

void UPCGEditorGraphSchema::GetAssetsGraphHoverMessage(const TArray<FAssetData>& Assets, const UEdGraph* /*HoverGraph*/, FString& OutTooltipText, bool& OutOkIcon) const
{
	for (const FAssetData& AssetData : Assets)
	{
		// TODO: get class from asset data, shouldn't require loading
		if(const UClass* AssetClass = AssetData.GetClass())
		{
			if(AssetClass->IsChildOf(UPCGGraphInterface::StaticClass()) || AssetClass->IsChildOf(UPCGSettings::StaticClass()) || AssetClass->IsChildOf(UPCGDataAsset::StaticClass()) || PCGEditorUtils::IsAssetPCGBlueprint(AssetData))
			{
				OutOkIcon = true;
				return;
			}
			else if(AssetClass->IsChildOf(UBlueprint::StaticClass()))
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

void UPCGEditorGraphSchema::OnPinConnectionDoubleCicked(UEdGraphPin* PinA, UEdGraphPin* PinB, const FVector2D& GraphPosition) const
{
	const FScopedTransaction Transaction(*FPCGEditorCommon::ContextIdentifier, LOCTEXT("PCGCreateRerouteNodeOnWire", "Create Reroute Node"), nullptr);

	const FVector2D NodeSpacerSize(42.0f, 24.0f);
	const FVector2D KnotTopLeft = GraphPosition - (NodeSpacerSize * 0.5f);

	UEdGraph* EditorGraph = PinA->GetOwningNode()->GetGraph();
	EditorGraph->Modify();

	FPCGEditorGraphSchemaAction_NewReroute Action;

	if (UPCGEditorGraphNodeReroute* RerouteNode = Cast<UPCGEditorGraphNodeReroute>(Action.PerformAction(EditorGraph, nullptr, KnotTopLeft, /*bSelectNewNode=*/true)))
	{
		UEdGraphNode* SourceGraphNode = PinA->GetOwningNode();
		UEdGraphNode* TargetGraphNode = PinB->GetOwningNode();

		UPCGEditorGraphNodeBase* SourcePCGGraphNode = CastChecked<UPCGEditorGraphNodeBase>(SourceGraphNode);
		UPCGEditorGraphNodeBase* TargetPCGGraphNode = CastChecked<UPCGEditorGraphNodeBase>(TargetGraphNode);

		// We need to disable full node reconstruction to make sure the pins are valid when creating the connections.
		SourcePCGGraphNode->EnableDeferredReconstruct();
		TargetPCGGraphNode->EnableDeferredReconstruct();
		
		BreakSinglePinLink(PinA, PinB);
		TryCreateConnection(PinA, (PinA->Direction == EGPD_Output) ? RerouteNode->GetInputPin() : RerouteNode->GetOutputPin());
		TryCreateConnection(PinB, (PinB->Direction == EGPD_Output) ? RerouteNode->GetInputPin() : RerouteNode->GetOutputPin());

		SourcePCGGraphNode->DisableDeferredReconstruct();
		TargetPCGGraphNode->DisableDeferredReconstruct();
	}
}

FPCGEditorConnectionDrawingPolicy::FPCGEditorConnectionDrawingPolicy(int32 InBackLayerID, int32 InFrontLayerID, float InZoomFactor, const FSlateRect& InClippingRect, FSlateWindowElementList& InDrawElements, UEdGraph* InGraph)
	: FConnectionDrawingPolicy(InBackLayerID, InFrontLayerID, InZoomFactor, InClippingRect, InDrawElements)
	, Graph(CastChecked<UPCGEditorGraph>(InGraph))
{
	ArrowImage = nullptr;
	ArrowRadius = FVector2D::ZeroVector;
}

bool FPCGEditorConnectionDrawingPolicy::UpdateParamsIfDebugging(UEdGraphPin* OutputPin, UEdGraphPin* InputPin, FConnectionParams& Params)
{
	check(OutputPin && InputPin);

	// Early validation
	const UPCGEditorGraphNodeBase* UpstreamEditorNode = CastChecked<const UPCGEditorGraphNodeBase>(OutputPin->GetOwningNode());
	if (!UpstreamEditorNode || !UpstreamEditorNode->GetPCGNode())
	{
		return false;
	}

	const UPCGNode* NodeToInspect = nullptr;
	const UPCGPin* PinToInspect = nullptr;

	// Walk up the graph if the current node is a reroute node because there is no associated inspection data.
	PCGEditorGraphUtils::GetInspectablePin(UpstreamEditorNode->GetPCGNode(), UpstreamEditorNode->GetPCGNode()->GetOutputPin(OutputPin->GetFName()), NodeToInspect, PinToInspect);

	if (!NodeToInspect || !PinToInspect)
	{
		return false;
	}

	// Early out if we aren't in debug mode
	if (!Graph || !Graph->GetEditor().IsValid())
	{
		return false;
	}

	const FPCGEditor* Editor = Graph->GetEditor().Pin().Get();
	const FPCGStack* PCGStack = Editor ? Editor->GetStackBeingInspected() : nullptr;
	UPCGComponent* PCGComponent = Editor ? Editor->GetPCGComponentBeingInspected() : nullptr;

	if (!Editor || !PCGStack || !PCGComponent)
	{
		return false;
	}

	FPCGStack Stack = *PCGStack;
	TArray<FPCGStackFrame>& StackFrames = Stack.GetStackFramesMutable();
	StackFrames.Reserve(StackFrames.Num() + 2);
	StackFrames.Emplace(NodeToInspect);
	StackFrames.Emplace(PinToInspect);

	if (const FPCGDataCollection* DataCollection = PCGComponent->GetInspectionData(Stack))
	{
		if (DataCollection->TaggedData.Num() > 1)
		{
			Params.WireThickness *= GetDefault<UPCGEditorSettings>()->MultiDataEdgeDebugEmphasis;
		}
	}
	else
	{
		Params.WireColor = Params.WireColor.Desaturate(GetDefault<UPCGEditorSettings>()->EmptyEdgeDebugDesaturateFactor);
	}

	return true;
}

void FPCGEditorConnectionDrawingPolicy::DetermineWiringStyle(UEdGraphPin* OutputPin, UEdGraphPin* InputPin, /*inout*/ FConnectionParams& Params)
{
	FConnectionDrawingPolicy::DetermineWiringStyle(OutputPin, InputPin, Params);

	Params.WireThickness = GetDefault<UPCGEditorSettings>()->DefaultWireThickness;

	// Emphasize wire thickness on hovered pins
	if (HoveredPins.Contains(InputPin) && HoveredPins.Contains(OutputPin))
	{
		Params.WireThickness *= GetDefault<UPCGEditorSettings>()->HoverEdgeEmphasis;
	}

	// Base the color of the wire on the color of the output pin
	if (OutputPin)
	{
		Params.WireColor = GetDefault<UPCGEditorSettings>()->GetPinColor(OutputPin->PinType);
	}

	// Desaturate connection if downstream node is disabled or if the data on this wire won't be used
	if (InputPin && OutputPin)
	{
		// Try to apply debugging/dynamic visualization - if it fails, fall back to static visualization
		if (!UpdateParamsIfDebugging(OutputPin, InputPin, Params))
		{
			const UPCGEditorGraphNodeBase* EditorNode = CastChecked<const UPCGEditorGraphNodeBase>(InputPin->GetOwningNode());
			const UPCGNode* PCGNode = EditorNode ? EditorNode->GetPCGNode() : nullptr;
			const UPCGPin* PCGPin = PCGNode ? PCGNode->GetInputPin(InputPin->GetFName()) : nullptr;
			const UPCGEditorGraphNodeBase* UpstreamEditorNode = CastChecked<const UPCGEditorGraphNodeBase>(OutputPin->GetOwningNode());
			const UPCGEditorGraphNodeBase* DownstreamEditorNode = CastChecked<const UPCGEditorGraphNodeBase>(InputPin->GetOwningNode());

			if (PCGPin && UpstreamEditorNode && DownstreamEditorNode)
			{
				const bool bDownstreamNodeForceDisabled = DownstreamEditorNode->IsDisplayAsDisabledForced();

				// Look for the PCG edge that correlates with passed in (OutputPin, InputPin) edge
				const TObjectPtr<UPCGEdge>* PCGEdge = PCGPin->Edges.FindByPredicate([UpstreamEditorNode, OutputPin](const UPCGEdge* ConnectedPCGEdge)
				{
					return UpstreamEditorNode->GetPCGNode() == ConnectedPCGEdge->InputPin->Node && ConnectedPCGEdge->InputPin->Properties.Label == OutputPin->GetFName();
				});
				const bool bDownstreamNodeDoesNotUseData = PCGEdge && !PCGNode->IsEdgeUsedByNodeExecution(*PCGEdge);

				// If edge found and is not used, gray it out
				if (bDownstreamNodeForceDisabled || bDownstreamNodeDoesNotUseData)
				{
					Params.WireColor = Params.WireColor.Desaturate(GetDefault<UPCGEditorSettings>()->EmptyEdgeDebugDesaturateFactor);
				}
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
