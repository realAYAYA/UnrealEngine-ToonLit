// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "AssetRegistry/AssetData.h"
#include "ConnectionDrawingPolicy.h"
#include "EdGraph/EdGraphSchema.h"
#include "EdGraphUtilities.h"
#include "MetasoundFrontend.h"
#include "MetasoundFrontendController.h"
#include "MetasoundFrontendNodesCategories.h"
#include "Templates/Function.h"
#include "UObject/ObjectMacros.h"

#include "MetasoundEditorGraphSchema.generated.h"


// Forward Declarations
class UEdGraph;
class UEdGraphNode;
class UMetaSoundPatch;
class UMetasoundEditorGraphNode;


namespace Metasound
{
	namespace Editor
	{
		// Ordered such that highest priority is highest value
		enum class EPrimaryContextGroup : uint8
		{
			Conversions = 0,
			Graphs,
			Functions,
			Outputs,
			Inputs,
			Variables,

			Common
		};

		const FText& GetContextGroupDisplayName(EPrimaryContextGroup InContextGroup);

		using FInputFilterFunction = TFunction<bool(const FMetasoundFrontendClassInput&)>;
		using FOutputFilterFunction = TFunction<bool(const FMetasoundFrontendClassOutput&)>;
		using FInterfaceNodeFilterFunction = TFunction<bool(Frontend::FConstNodeHandle)>;

		struct FActionClassFilters
		{
			FInputFilterFunction InputFilterFunction;
			FOutputFilterFunction OutputFilterFunction;
		};
	} // namespace Editor
} // namespace Metasound


USTRUCT()
struct METASOUNDEDITOR_API FMetasoundGraphSchemaAction : public FEdGraphSchemaAction
{
	GENERATED_USTRUCT_BODY()

	FMetasoundGraphSchemaAction()
		: FEdGraphSchemaAction()
	{
	}

	FMetasoundGraphSchemaAction(FText InNodeCategory, FText InMenuDesc, FText InToolTip, Metasound::Editor::EPrimaryContextGroup InGroup, FText InKeywords = FText::GetEmpty())
		: FEdGraphSchemaAction(
			MoveTemp(InNodeCategory),
			MoveTemp(InMenuDesc),
			MoveTemp(InToolTip),
			static_cast<int32>(InGroup),
			MoveTemp(InKeywords))
	{
	}

	virtual ~FMetasoundGraphSchemaAction() = default;

	virtual const FSlateBrush* GetIconBrush() const
	{
		return FAppStyle::GetBrush("NoBrush");
	}

	virtual const FLinearColor& GetIconColor() const
	{
		static const FLinearColor DefaultColor;
		return DefaultColor;
	}
};

/** This is used to combine functionality for nodes that can have multiple outputs and should never be directly instantiated. */
USTRUCT()
struct METASOUNDEDITOR_API FMetasoundGraphSchemaAction_NodeWithMultipleOutputs : public FMetasoundGraphSchemaAction
{
	GENERATED_USTRUCT_BODY();

	FMetasoundGraphSchemaAction_NodeWithMultipleOutputs()
		: FMetasoundGraphSchemaAction()
	{}

	FMetasoundGraphSchemaAction_NodeWithMultipleOutputs(FText InNodeCategory, FText InMenuDesc, FText InToolTip, Metasound::Editor::EPrimaryContextGroup InGroup, FText InKeywords = FText::GetEmpty())
		: FMetasoundGraphSchemaAction(InNodeCategory, InMenuDesc, InToolTip, InGroup, InKeywords)
	{}

	virtual ~FMetasoundGraphSchemaAction_NodeWithMultipleOutputs() = default;

	//~ Begin FEdGraphSchemaAction Interface
	virtual UEdGraphNode* PerformAction(UEdGraph* ParentGraph, TArray<UEdGraphPin*>& FromPins, const FVector2D Location, bool bSelectNewNode = true) override;
	virtual UEdGraphNode* PerformAction(UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode = true) { return nullptr; }
	//~ End FEdGraphSchemaAction Interface
};

/** Action to add an input reference to the graph */
USTRUCT()
struct METASOUNDEDITOR_API FMetasoundGraphSchemaAction_NewInput : public FMetasoundGraphSchemaAction_NodeWithMultipleOutputs
{
	GENERATED_USTRUCT_BODY();

	UPROPERTY()
	FGuid NodeID;

	FMetasoundGraphSchemaAction_NewInput()
		: FMetasoundGraphSchemaAction_NodeWithMultipleOutputs()
	{}

	FMetasoundGraphSchemaAction_NewInput(FText InNodeCategory, FText InDisplayName, FGuid InInputNodeID, FText InToolTip, Metasound::Editor::EPrimaryContextGroup InGroup);

	virtual ~FMetasoundGraphSchemaAction_NewInput() = default;

	//~ Begin FMetasoundGraphSchemaAction Interface
	virtual const FSlateBrush* GetIconBrush() const override;

	virtual const FLinearColor& GetIconColor() const override;
	//~ End FMetasoundGraphSchemaAction Interface

	//~ Begin FEdGraphSchemaAction Interface
	virtual UEdGraphNode* PerformAction(UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode = true) override;
	//~ End FEdGraphSchemaAction Interface
};

/** Promotes an input to a graph input, using its respective literal value as the default value */
USTRUCT()
struct METASOUNDEDITOR_API FMetasoundGraphSchemaAction_PromoteToInput : public FMetasoundGraphSchemaAction_NodeWithMultipleOutputs
{
	GENERATED_USTRUCT_BODY();

	FMetasoundGraphSchemaAction_PromoteToInput();

	virtual ~FMetasoundGraphSchemaAction_PromoteToInput() = default;

	//~ Begin FEdGraphSchemaAction Interface
	virtual UEdGraphNode* PerformAction(UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode = true) override;
	//~ End FEdGraphSchemaAction Interface
};

/** Adds an output to the graph */
USTRUCT()
struct METASOUNDEDITOR_API FMetasoundGraphSchemaAction_NewOutput : public FMetasoundGraphSchemaAction
{
	GENERATED_USTRUCT_BODY();

	UPROPERTY()
	FGuid NodeID;

	FMetasoundGraphSchemaAction_NewOutput()
		: FMetasoundGraphSchemaAction()
	{}

	FMetasoundGraphSchemaAction_NewOutput(FText InNodeCategory, FText InDisplayName, FGuid InOutputNodeID, FText InToolTip, Metasound::Editor::EPrimaryContextGroup InGroup);

	virtual ~FMetasoundGraphSchemaAction_NewOutput() = default;

	//~ Begin FMetasoundGraphSchemaAction Interface
	virtual const FSlateBrush* GetIconBrush() const override;

	virtual const FLinearColor& GetIconColor() const override;
	//~ End FMetasoundGraphSchemaAction Interface

	//~ Begin FEdGraphSchemaAction Interface
	virtual UEdGraphNode* PerformAction(UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode = true) override;
	//~ End FEdGraphSchemaAction Interface
};

/** Promotes a node output to a graph output */
USTRUCT()
struct METASOUNDEDITOR_API FMetasoundGraphSchemaAction_PromoteToOutput : public FMetasoundGraphSchemaAction
{
	GENERATED_USTRUCT_BODY();

	FMetasoundGraphSchemaAction_PromoteToOutput();

	virtual ~FMetasoundGraphSchemaAction_PromoteToOutput() = default;

	//~ Begin FEdGraphSchemaAction Interface
	virtual UEdGraphNode* PerformAction(UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode = true) override;
	//~ End FEdGraphSchemaAction Interface
};

/** Adds a variable node to the graph */
USTRUCT()
struct METASOUNDEDITOR_API FMetasoundGraphSchemaAction_NewVariableNode : public FMetasoundGraphSchemaAction_NodeWithMultipleOutputs
{
	GENERATED_USTRUCT_BODY();

	UPROPERTY()
	FGuid VariableID;

	FMetasoundGraphSchemaAction_NewVariableNode()
		: FMetasoundGraphSchemaAction_NodeWithMultipleOutputs()
	{}

	FMetasoundGraphSchemaAction_NewVariableNode(FText InNodeCategory, FText InDisplayName, FGuid InVariableID, FText InToolTip);

	virtual ~FMetasoundGraphSchemaAction_NewVariableNode() = default;

	//~ Begin FEdGraphSchemaAction Interface
	virtual UEdGraphNode* PerformAction(UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode = true) override;
	//~ End FEdGraphSchemaAction Interface

	//~ Begin FMetasoundGraphSchemaAction Interface
	virtual const FSlateBrush* GetIconBrush() const override;

	virtual const FLinearColor& GetIconColor() const override;
	//~ End FMetasoundGraphSchemaAction Interface
	
protected:
	// Derived classes should override this method to create the desired frontend node.
	virtual Metasound::Frontend::FNodeHandle CreateFrontendVariableNode(const Metasound::Frontend::FGraphHandle& InFrontendGraph, const FGuid& InVariableID) const
	{
		return Metasound::Frontend::INodeController::GetInvalidHandle();
	}
};

/** Adds a variable node to the graph */
USTRUCT()
struct METASOUNDEDITOR_API FMetasoundGraphSchemaAction_NewVariableAccessorNode : public FMetasoundGraphSchemaAction_NewVariableNode
{
	GENERATED_USTRUCT_BODY();

	FMetasoundGraphSchemaAction_NewVariableAccessorNode()
		: FMetasoundGraphSchemaAction_NewVariableNode()
	{}

	FMetasoundGraphSchemaAction_NewVariableAccessorNode(FText InNodeCategory, FText InDisplayName, FGuid InVariableID, FText InToolTip);

	virtual ~FMetasoundGraphSchemaAction_NewVariableAccessorNode() = default;

protected:
	virtual Metasound::Frontend::FNodeHandle CreateFrontendVariableNode(const Metasound::Frontend::FGraphHandle& InFrontendGraph, const FGuid& InVariableID) const override;

};

/** Adds a variable node to the graph */
USTRUCT()
struct METASOUNDEDITOR_API FMetasoundGraphSchemaAction_NewVariableDeferredAccessorNode : public FMetasoundGraphSchemaAction_NewVariableNode
{
	GENERATED_USTRUCT_BODY();

	FMetasoundGraphSchemaAction_NewVariableDeferredAccessorNode()
		: FMetasoundGraphSchemaAction_NewVariableNode()
	{}

	virtual ~FMetasoundGraphSchemaAction_NewVariableDeferredAccessorNode() = default;

	FMetasoundGraphSchemaAction_NewVariableDeferredAccessorNode(FText InNodeCategory, FText InDisplayName, FGuid InVariableID, FText InToolTip);
	
protected:
	virtual Metasound::Frontend::FNodeHandle CreateFrontendVariableNode(const Metasound::Frontend::FGraphHandle& InFrontendGraph, const FGuid& InVariableID) const override;
};

/** Adds a variable node to the graph */
USTRUCT()
struct METASOUNDEDITOR_API FMetasoundGraphSchemaAction_NewVariableMutatorNode : public FMetasoundGraphSchemaAction_NewVariableNode
{
	GENERATED_USTRUCT_BODY();

	FMetasoundGraphSchemaAction_NewVariableMutatorNode()
		: FMetasoundGraphSchemaAction_NewVariableNode()
	{}

	virtual ~FMetasoundGraphSchemaAction_NewVariableMutatorNode() = default;

	FMetasoundGraphSchemaAction_NewVariableMutatorNode(FText InNodeCategory, FText InDisplayName, FGuid InVariableID, FText InToolTip);
	
protected:
	virtual Metasound::Frontend::FNodeHandle CreateFrontendVariableNode(const Metasound::Frontend::FGraphHandle& InFrontendGraph, const FGuid& InVariableID) const override;
};

/** Promotes an input to a graph variable & respective getter node, using its respective literal value as the default value */
USTRUCT()
struct METASOUNDEDITOR_API FMetasoundGraphSchemaAction_PromoteToVariable_AccessorNode : public FMetasoundGraphSchemaAction_NodeWithMultipleOutputs
{
	GENERATED_USTRUCT_BODY();

	FMetasoundGraphSchemaAction_PromoteToVariable_AccessorNode();

	virtual ~FMetasoundGraphSchemaAction_PromoteToVariable_AccessorNode() = default;

	//~ Begin FEdGraphSchemaAction Interface
	virtual UEdGraphNode* PerformAction(UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode = true) override;
	//~ End FEdGraphSchemaAction Interface
};

/** Promotes an output to a graph variable & respective setter node, using its respective literal value as the default value */
USTRUCT()
struct METASOUNDEDITOR_API FMetasoundGraphSchemaAction_PromoteToVariable_MutatorNode : public FMetasoundGraphSchemaAction
{
	GENERATED_USTRUCT_BODY();

	FMetasoundGraphSchemaAction_PromoteToVariable_MutatorNode();

	virtual ~FMetasoundGraphSchemaAction_PromoteToVariable_MutatorNode() = default;

	//~ Begin FEdGraphSchemaAction Interface
	virtual UEdGraphNode* PerformAction(UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode = true) override;
	//~ End FEdGraphSchemaAction Interface
};

/** Promotes an input to a graph variable & respective deferred getter node, using its respective literal value as the default value */
USTRUCT()
struct METASOUNDEDITOR_API FMetasoundGraphSchemaAction_PromoteToVariable_DeferredAccessorNode : public FMetasoundGraphSchemaAction_NodeWithMultipleOutputs
{
	GENERATED_USTRUCT_BODY();

	FMetasoundGraphSchemaAction_PromoteToVariable_DeferredAccessorNode();

	virtual ~FMetasoundGraphSchemaAction_PromoteToVariable_DeferredAccessorNode() = default;

	//~ Begin FEdGraphSchemaAction Interface
	virtual UEdGraphNode* PerformAction(UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode = true) override;
	//~ End FEdGraphSchemaAction Interface
};


/** Action to add a node to the graph */
USTRUCT()
struct METASOUNDEDITOR_API FMetasoundGraphSchemaAction_NewNode : public FMetasoundGraphSchemaAction_NodeWithMultipleOutputs
{
	GENERATED_USTRUCT_BODY();

	/** Class Metadata of node to create */
	FMetasoundFrontendClassMetadata ClassMetadata;

	FMetasoundGraphSchemaAction_NewNode() 
		: FMetasoundGraphSchemaAction_NodeWithMultipleOutputs()
	{}

	FMetasoundGraphSchemaAction_NewNode(const FText& InNodeCategory, const FText& InMenuDesc, const FText& InToolTip, Metasound::Editor::EPrimaryContextGroup InGroup, FText InKeywords = FText::GetEmpty())
		: FMetasoundGraphSchemaAction_NodeWithMultipleOutputs(InNodeCategory, InMenuDesc, InToolTip, InGroup, InKeywords)
	{}

	virtual ~FMetasoundGraphSchemaAction_NewNode() = default;

	//~ Begin FMetasoundGraphSchemaAction Interface
	virtual const FSlateBrush* GetIconBrush() const override;

	virtual const FLinearColor& GetIconColor() const override;
	//~ End FMetasoundGraphSchemaAction Interface

	//~ Begin FEdGraphSchemaAction Interface
	virtual UEdGraphNode* PerformAction(UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode = true) override;
	//~ End FEdGraphSchemaAction Interface
};

/** Action to add nodes to the graph based on selected objects*/
USTRUCT()
struct METASOUNDEDITOR_API FMetasoundGraphSchemaAction_NewFromSelected : public FMetasoundGraphSchemaAction_NewNode
{
	GENERATED_USTRUCT_BODY();

	FMetasoundGraphSchemaAction_NewFromSelected() 
		: FMetasoundGraphSchemaAction_NewNode()
	{}

	FMetasoundGraphSchemaAction_NewFromSelected(FText InNodeCategory, FText InMenuDesc, FText InToolTip, Metasound::Editor::EPrimaryContextGroup InGroup)
		: FMetasoundGraphSchemaAction_NewNode(MoveTemp(InNodeCategory), MoveTemp(InMenuDesc), MoveTemp(InToolTip), InGroup)
	{}

	virtual ~FMetasoundGraphSchemaAction_NewFromSelected() = default;

	//~ Begin FEdGraphSchemaAction Interface
	virtual UEdGraphNode* PerformAction(UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode = true) override;
	//~ End FEdGraphSchemaAction Interface
};

/** Action to create new reroute node */
USTRUCT()
struct METASOUNDEDITOR_API FMetasoundGraphSchemaAction_NewReroute : public FMetasoundGraphSchemaAction
{
	GENERATED_USTRUCT_BODY();

	FMetasoundGraphSchemaAction_NewReroute() = default;
	FMetasoundGraphSchemaAction_NewReroute(const FLinearColor* InIconColor, bool bInShouldTransact = true);

	virtual ~FMetasoundGraphSchemaAction_NewReroute() = default;

	//~ Begin FMetasoundGraphSchemaAction Interface
	virtual const FSlateBrush* GetIconBrush() const override;

	virtual const FLinearColor& GetIconColor() const override;
	//~ End FMetasoundGraphSchemaAction Interface

	//~ Begin FEdGraphSchemaAction Interface
	virtual UEdGraphNode* PerformAction(UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode = true) override;
	//~ End FEdGraphSchemaAction Interface

private:
	FLinearColor IconColor;
	bool bShouldTransact = true;
};

/** Action to create new comment */
USTRUCT()
struct METASOUNDEDITOR_API FMetasoundGraphSchemaAction_NewComment : public FMetasoundGraphSchemaAction
{
	GENERATED_USTRUCT_BODY();

	FMetasoundGraphSchemaAction_NewComment()
		: FMetasoundGraphSchemaAction()
	{}

	FMetasoundGraphSchemaAction_NewComment(const FText& InNodeCategory, const FText& InMenuDesc, const FText& InToolTip, Metasound::Editor::EPrimaryContextGroup InGroup)
		: FMetasoundGraphSchemaAction(InNodeCategory, InMenuDesc, InToolTip, InGroup)
	{}

	//~ Begin FMetasoundGraphSchemaAction Interface
	virtual const FSlateBrush* GetIconBrush() const override;

	virtual const FLinearColor& GetIconColor() const override;
	//~ End FMetasoundGraphSchemaAction Interface

	//~ Begin FEdGraphSchemaAction Interface
	virtual UEdGraphNode* PerformAction(UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode = true) override;
	//~ End FEdGraphSchemaAction Interface
};

/** Action to paste clipboard contents into the graph */
USTRUCT()
struct METASOUNDEDITOR_API FMetasoundGraphSchemaAction_Paste : public FMetasoundGraphSchemaAction
{
	GENERATED_USTRUCT_BODY();

	FMetasoundGraphSchemaAction_Paste() 
		: FMetasoundGraphSchemaAction()
	{}

	FMetasoundGraphSchemaAction_Paste(const FText& InNodeCategory, const FText& InMenuDesc, const FText& InToolTip, Metasound::Editor::EPrimaryContextGroup InGroup)
		: FMetasoundGraphSchemaAction(InNodeCategory, InMenuDesc, InToolTip, InGroup)
	{}

	//~ Begin FEdGraphSchemaAction Interface
	virtual UEdGraphNode* PerformAction(UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode = true) override;
	//~ End FEdGraphSchemaAction Interface
};

UCLASS(MinimalAPI)
class UMetasoundEditorGraphSchema : public UEdGraphSchema
{
	GENERATED_UCLASS_BODY()

public:
	/** Check whether connecting these pins would cause a loop */
	bool ConnectionCausesLoop(const UEdGraphPin* InputPin, const UEdGraphPin* OutputPin) const;

	/** Helper method to add items valid to the palette list */
	METASOUNDEDITOR_API void GetPaletteActions(FGraphActionMenuBuilder& ActionMenuBuilder) const;

	//~ Begin EdGraphSchema Interface
	virtual void GetGraphContextActions(FGraphContextMenuBuilder& ContextMenuBuilder) const override;
	virtual void GetContextMenuActions(UToolMenu* Menu, UGraphNodeContextMenuContext* Context) const override;
	virtual FText GetPinDisplayName(const UEdGraphPin* Pin) const override;
	virtual void CreateDefaultNodesForGraph(UEdGraph& Graph) const override;
	virtual const FPinConnectionResponse CanCreateConnection(const UEdGraphPin* A, const UEdGraphPin* B) const override;
	virtual bool TryCreateConnection(UEdGraphPin* A, UEdGraphPin* B) const override;
	virtual void TrySetDefaultObject(UEdGraphPin& Pin, UObject* NewDefaultObject, bool bInMarkAsModified) const override;
	virtual void TrySetDefaultValue(UEdGraphPin& Pin, const FString& InNewDefaultValue, bool bInMarkAsModified) const override;
	virtual bool SafeDeleteNodeFromGraph(UEdGraph* Graph, UEdGraphNode* InNodeToDelete) const override;
	virtual bool ShouldHidePinDefaultValue(UEdGraphPin* Pin) const override;
	virtual FLinearColor GetPinTypeColor(const FEdGraphPinType& PinType) const override;
	virtual void BreakNodeLinks(UEdGraphNode& TargetNode) const override;
	virtual void BreakPinLinks(UEdGraphPin& TargetPin, bool bSendsNodeNotifcation) const override;
	virtual void BreakSinglePinLink(UEdGraphPin* SourcePin, UEdGraphPin* TargetPin) const override;
	virtual void GetAssetsGraphHoverMessage(const TArray<FAssetData>& Assets, const UEdGraph* HoverGraph, FString& OutTooltipText, bool& OutOkIcon) const override;
	virtual void GetAssetsPinHoverMessage(const TArray<FAssetData>& Assets, const UEdGraphPin* HoverPin, FString& OutTooltipText, bool& OutOkIcon) const override;
	virtual void DroppedAssetsOnGraph(const TArray<FAssetData>& Assets, const FVector2D& GraphPosition, UEdGraph* Graph) const override;
	virtual void DroppedAssetsOnNode(const TArray<FAssetData>& Assets, const FVector2D& GraphPosition, UEdGraphNode* Node) const override;
	virtual void DroppedAssetsOnPin(const TArray<FAssetData>& Assets, const FVector2D& GraphPosition, UEdGraphPin* Pin) const override;
	virtual int32 GetNodeSelectionCount(const UEdGraph* Graph) const override;
	virtual TSharedPtr<FEdGraphSchemaAction> GetCreateCommentAction() const override;
	virtual void OnPinConnectionDoubleCicked(UEdGraphPin* PinA, UEdGraphPin* PinB, const FVector2D& GraphPosition) const override;
	virtual void SetNodePosition(UEdGraphNode* Node, const FVector2D& Position) const;
	//~ End EdGraphSchema Interface

	void BreakNodeLinks(UEdGraphNode& TargetNode, bool bShouldActuallyTransact) const;

private:
	/** Adds actions for creating actions associated with graph DataTypes */
	void GetConversionActions(FGraphActionMenuBuilder& ActionMenuBuilder, Metasound::Editor::FActionClassFilters InFilters = Metasound::Editor::FActionClassFilters(), bool bShowSelectedActions = true) const;
	void GetFunctionActions(FGraphActionMenuBuilder& ActionMenuBuilder, Metasound::Editor::FActionClassFilters InFilters = Metasound::Editor::FActionClassFilters(), bool bShowSelectedActions = true, Metasound::Frontend::FConstGraphHandle InGraphHandle = Metasound::Frontend::IGraphController::GetInvalidHandle()) const;
	void GetVariableActions(FGraphActionMenuBuilder& ActionMenuBuilder, Metasound::Editor::FActionClassFilters InFilters = Metasound::Editor::FActionClassFilters(), bool bShowSelectedActions = true, Metasound::Frontend::FConstGraphHandle InGraphHandle = Metasound::Frontend::IGraphController::GetInvalidHandle()) const;

	void BreakSinglePinLink(UEdGraphPin* SourcePin, UEdGraphPin* TargetPin, bool bShouldTransact) const;

	void GetDataTypeInputNodeActions(FGraphContextMenuBuilder& InMenuBuilder, Metasound::Frontend::FConstGraphHandle InGraphHandle, Metasound::Editor::FInterfaceNodeFilterFunction InFilter = Metasound::Editor::FInterfaceNodeFilterFunction(), bool bShowSelectedActions = true) const;
	void GetDataTypeOutputNodeActions(FGraphContextMenuBuilder& InMenuBuilder, Metasound::Frontend::FConstGraphHandle InGraphHandle, Metasound::Editor::FInterfaceNodeFilterFunction InFilter = Metasound::Editor::FInterfaceNodeFilterFunction(), bool bShowSelectedActions = true) const;

	/** Adds action for creating a comment */
	void GetCommentAction(FGraphActionMenuBuilder& ActionMenuBuilder, const UEdGraph* CurrentGraph = nullptr) const;
};
