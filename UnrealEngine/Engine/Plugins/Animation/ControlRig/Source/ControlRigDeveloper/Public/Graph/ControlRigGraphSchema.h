// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ControlRigGraphNode.h"
#include "EdGraph/EdGraphSchema.h"
#include "GraphEditorDragDropAction.h"
#include "RigVMModel/RigVMGraph.h"
#include "EdGraphSchema_K2_Actions.h"
#include "Kismet2/Kismet2NameValidators.h"

#include "ControlRigGraphSchema.generated.h"

class UControlRigBlueprint;
class UControlRigGraph;
class UControlRigGraphNode;
class UControlRigGraphNode_Unit;
class UControlRigGraphNode_Property;

/** Extra operations that can be performed on pin connection */
enum class ECanCreateConnectionResponse_Extended
{
	None,

	BreakChildren,

	BreakParent,
};

/** Struct used to extend the response to a conneciton request to include breaking parents/children */
struct FControlRigPinConnectionResponse
{
	FControlRigPinConnectionResponse(const ECanCreateConnectionResponse InResponse, FText InMessage, ECanCreateConnectionResponse_Extended InExtendedResponse = ECanCreateConnectionResponse_Extended::None)
		: Response(InResponse, MoveTemp(InMessage))
		, ExtendedResponse(InExtendedResponse)
	{
	}

	friend bool operator==(const FControlRigPinConnectionResponse& A, const FControlRigPinConnectionResponse& B)
	{
		return (A.Response == B.Response) && (A.ExtendedResponse == B.ExtendedResponse);
	}	

	FPinConnectionResponse Response;
	ECanCreateConnectionResponse_Extended ExtendedResponse;
};

/////////////////////////////////////////////////////
// FControlRigLocalVariableNameValidator
class CONTROLRIGDEVELOPER_API FControlRigLocalVariableNameValidator : public FStringSetNameValidator
{

public:
	FControlRigLocalVariableNameValidator(const class UBlueprint* Blueprint, const URigVMGraph* Graph, FName InExistingName = NAME_None);

	// Begin FNameValidatorInterface
	virtual EValidatorResult IsValid(const FString& Name, bool bOriginal) override;
	virtual EValidatorResult IsValid(const FName& Name, bool bOriginal) override;
	// End FNameValidatorInterface
};

/////////////////////////////////////////////////////
// FControlRigNameValidator
class CONTROLRIGDEVELOPER_API FControlRigNameValidator : public FStringSetNameValidator
{

public:
	FControlRigNameValidator(const class UBlueprint* Blueprint, const UStruct* ValidationScope, FName InExistingName = NAME_None);

	// Begin FNameValidatorInterface
	virtual EValidatorResult IsValid(const FString& Name, bool bOriginal) override;
	virtual EValidatorResult IsValid(const FName& Name, bool bOriginal) override;
	// End FNameValidatorInterface
};

USTRUCT()
struct CONTROLRIGDEVELOPER_API FControlRigGraphSchemaAction_LocalVar : public FEdGraphSchemaAction_BlueprintVariableBase
{
	GENERATED_BODY()

	
public:

	// Simple type info
	static FName StaticGetTypeId() {static FName Type("FControlRigGraphSchemaAction_LocalVar"); return Type;}
	virtual FName GetTypeId() const override { return StaticGetTypeId(); } 

	FControlRigGraphSchemaAction_LocalVar()
		: FEdGraphSchemaAction_BlueprintVariableBase()
	{}

	FControlRigGraphSchemaAction_LocalVar(FText InNodeCategory, FText InMenuDesc, FText InToolTip, const int32 InGrouping, const int32 InSectionID)
		: FEdGraphSchemaAction_BlueprintVariableBase(MoveTemp(InNodeCategory), MoveTemp(InMenuDesc), MoveTemp(InToolTip), InGrouping, InSectionID)
	{}

	virtual bool IsA(const FName& InType) const override
	{
		return InType == GetTypeId() || InType == FEdGraphSchemaAction_BlueprintVariableBase::StaticGetTypeId();
	}

	virtual FEdGraphPinType GetPinType() const override;

	virtual void ChangeVariableType(const FEdGraphPinType& NewPinType) override;

	virtual void RenameVariable(const FName& NewName) override;

	virtual bool IsValidName(const FName& NewName, FText& OutErrorMessage) const override;

	virtual void DeleteVariable() override;

	virtual bool IsVariableUsed() override;
};

USTRUCT()
struct CONTROLRIGDEVELOPER_API FControlRigGraphSchemaAction_PromoteToVariable : public FEdGraphSchemaAction
{
	GENERATED_BODY()

public:

	// Simple type info
	static FName StaticGetTypeId() {static FName Type("FControlRigGraphSchemaAction_PromoteToVariable"); return Type;}
	virtual FName GetTypeId() const override { return StaticGetTypeId(); } 

	FControlRigGraphSchemaAction_PromoteToVariable()
		: FEdGraphSchemaAction()
	{}

	FControlRigGraphSchemaAction_PromoteToVariable(UEdGraphPin* InEdGraphPin, bool InLocalVariable);

	virtual bool IsA(const FName& InType) const override
	{
		return InType == GetTypeId();
	}

	virtual UEdGraphNode* PerformAction(UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode) override;

private:

	UEdGraphPin* EdGraphPin;
	bool bLocalVariable;
};

USTRUCT()
struct CONTROLRIGDEVELOPER_API FControlRigGraphSchemaAction_Event : public FEdGraphSchemaAction
{
	GENERATED_BODY()

public:

	// Simple type info
	static FName StaticGetTypeId() {static FName Type("FControlRigGraphSchemaAction_Event"); return Type;}
	virtual FName GetTypeId() const override { return StaticGetTypeId(); } 

	FControlRigGraphSchemaAction_Event()
		: FEdGraphSchemaAction()
	{}

	FControlRigGraphSchemaAction_Event(const FName& InEventName, const FString& InNodePath, const FText& InNodeCategory);

	virtual bool IsA(const FName& InType) const override
	{
		return InType == GetTypeId() || Super::IsA(InType);
	}
	virtual bool IsParentable() const override { return true; }
	virtual FReply OnDoubleClick(UBlueprint* InBlueprint) override;
	virtual bool CanBeRenamed() const override { return false; }
	virtual FSlateBrush const* GetPaletteIcon() const override;

private:

	FString NodePath;
};

/** DragDropAction class for drag and dropping an item from the My Blueprints tree (e.g., variable or function) */
class CONTROLRIGDEVELOPER_API FControlRigFunctionDragDropAction : public FGraphSchemaActionDragDropAction
{
public:

	DRAG_DROP_OPERATOR_TYPE(FControlRigFunctionDragDropAction, FGraphSchemaActionDragDropAction)

	// FGraphEditorDragDropAction interface
	virtual FReply DroppedOnPanel(const TSharedRef< class SWidget >& Panel, FVector2D ScreenPosition, FVector2D GraphPosition, UEdGraph& Graph) override;
	virtual FReply DroppedOnPin(FVector2D ScreenPosition, FVector2D GraphPosition) override;
	virtual FReply DroppedOnAction(TSharedRef<FEdGraphSchemaAction> Action) override;
	virtual FReply DroppedOnCategory(FText Category) override;
	virtual void HoverTargetChanged() override;
	// End of FGraphEditorDragDropAction

	/** Set if operation is modified by alt */
	void SetAltDrag(bool InIsAltDrag) { bAltDrag = InIsAltDrag; }

	/** Set if operation is modified by the ctrl key */
	void SetCtrlDrag(bool InIsCtrlDrag) { bControlDrag = InIsCtrlDrag; }

protected:

	/** Constructor */
	FControlRigFunctionDragDropAction();

	static TSharedRef<FControlRigFunctionDragDropAction> New(TSharedPtr<FEdGraphSchemaAction> InAction, UControlRigBlueprint* InRigBlueprint, UControlRigGraph* InRigGraph);

protected:

	UControlRigBlueprint* SourceRigBlueprint;
	UControlRigGraph* SourceRigGraph;
	bool bControlDrag;
	bool bAltDrag;

	friend class UControlRigGraphSchema;
};

UCLASS()
class CONTROLRIGDEVELOPER_API UControlRigGraphSchema : public UEdGraphSchema
{
	GENERATED_BODY()

public:
	/** Name constants */
	static const FName GraphName_ControlRig;

public:
	UControlRigGraphSchema();

	// UEdGraphSchema interface
	virtual void GetGraphContextActions(FGraphContextMenuBuilder& ContextMenuBuilder) const override;
	virtual void GetContextMenuActions(class UToolMenu* Menu, class UGraphNodeContextMenuContext* Context) const override;
	virtual bool TryCreateConnection(UEdGraphPin* PinA, UEdGraphPin* PinB) const override;
	virtual const FPinConnectionResponse CanCreateConnection(const UEdGraphPin* A, const UEdGraphPin* B) const override;
	virtual FLinearColor GetPinTypeColor(const FEdGraphPinType& PinType) const override;
	virtual bool SupportsPinType(TWeakPtr<const FEdGraphSchemaAction> SchemaAction, const FEdGraphPinType& PinType) const override;
	virtual bool SupportsPinTypeContainer(TWeakPtr<const FEdGraphSchemaAction> SchemaAction, const FEdGraphPinType& PinType, const EPinContainerType& ContainerType) const override;
	virtual void BreakPinLinks(UEdGraphPin& TargetPin, bool bSendsNodeNotifcation) const override;
	virtual void BreakSinglePinLink(UEdGraphPin* SourcePin, UEdGraphPin* TargetPin) const override;
	virtual bool CanGraphBeDropped(TSharedPtr<FEdGraphSchemaAction> InAction) const override;
	virtual FString GetFindReferenceSearchTerm(const FEdGraphSchemaAction* InGraphAction) const override;
	virtual FReply BeginGraphDragAction(TSharedPtr<FEdGraphSchemaAction> InAction, const FPointerEvent& MouseEvent = FPointerEvent() ) const override;
	virtual class FConnectionDrawingPolicy* CreateConnectionDrawingPolicy(int32 InBackLayerID, int32 InFrontLayerID, float InZoomFactor, const FSlateRect& InClippingRect, class FSlateWindowElementList& InDrawElements, class UEdGraph* InGraphObj) const override;
	virtual bool ShouldHidePinDefaultValue(UEdGraphPin* Pin) const override;
	virtual void TrySetDefaultValue(UEdGraphPin& InPin, const FString& InNewDefaultValue, bool bMarkAsModified = true) const override;
	virtual void TrySetDefaultObject(UEdGraphPin& InPin, UObject* InNewDefaultObject, bool bMarkAsModified) const override;
	virtual void TrySetDefaultText(UEdGraphPin& InPin, const FText& InNewDefaultText, bool bMarkAsModified) const override;
	virtual bool ShouldAlwaysPurgeOnModification() const override { return false; }
	virtual bool ArePinsCompatible(const UEdGraphPin* PinA, const UEdGraphPin* PinB, const UClass* CallingContext, bool bIgnoreArray /*= false*/) const override;
	virtual bool DoesSupportPinWatching() const	override { return true; }
	virtual bool IsPinBeingWatched(UEdGraphPin const* Pin) const override;
	virtual void ClearPinWatch(UEdGraphPin const* Pin) const override;
	virtual void OnPinConnectionDoubleCicked(UEdGraphPin* PinA, UEdGraphPin* PinB, const FVector2D& GraphPosition) const override;
	virtual bool MarkBlueprintDirtyFromNewNode(UBlueprint* InBlueprint, UEdGraphNode* InEdGraphNode) const override;
	virtual bool SafeDeleteNodeFromGraph(UEdGraph* Graph, UEdGraphNode* Node) const override;
	virtual bool CanVariableBeDropped(UEdGraph* InGraph, FProperty* InVariableToDrop) const override;
	virtual bool RequestVariableDropOnPanel(UEdGraph* InGraph, FProperty* InVariableToDrop, const FVector2D& InDropPosition, const FVector2D& InScreenPosition) override;
	virtual bool RequestVariableDropOnPin(UEdGraph* InGraph, FProperty* InVariableToDrop, UEdGraphPin* InPin, const FVector2D& InDropPosition, const FVector2D& InScreenPosition) override;
	virtual bool IsStructEditable(UStruct* InStruct) const;
	virtual void SetNodePosition(UEdGraphNode* Node, const FVector2D& Position) const override;
	void SetNodePosition(UEdGraphNode* Node, const FVector2D& Position, bool bSetupUndo) const;
	virtual void GetGraphDisplayInformation(const UEdGraph& Graph, /*out*/ FGraphDisplayInfo& DisplayInfo) const override;
	virtual bool GetLocalVariables(const UEdGraph* InGraph, TArray<FBPVariableDescription>& OutLocalVariables) const override;
	virtual TSharedPtr<FEdGraphSchemaAction> MakeActionFromVariableDescription(const UEdGraph* InEdGraph, const FBPVariableDescription& Variable) const override;
	virtual FText GetGraphCategory(const UEdGraph* InGraph) const override;
	virtual EGraphType GetGraphType(const UEdGraph* TestEdGraph) const override;
	virtual FReply TrySetGraphCategory(const UEdGraph* InGraph, const FText& InCategory) override;
	virtual bool TryDeleteGraph(UEdGraph* GraphToDelete) const override;
	virtual bool TryRenameGraph(UEdGraph* GraphToRename, const FName& InNewName) const override;
	virtual bool TryToGetChildEvents(const UEdGraph* Graph, const int32 SectionId, TArray<TSharedPtr<FEdGraphSchemaAction>>& Actions, const FText& ParentCategory) const override;
	virtual bool CanDuplicateGraph(UEdGraph* InSourceGraph) const { return false; }
	virtual UEdGraphPin* DropPinOnNode(UEdGraphNode* InTargetNode, const FName& InSourcePinName, const FEdGraphPinType& InSourcePinType, EEdGraphPinDirection InSourcePinDirection) const override;
	virtual bool SupportsDropPinOnNode(UEdGraphNode* InTargetNode, const FEdGraphPinType& InSourcePinType, EEdGraphPinDirection InSourcePinDirection, FText& OutErrorMessage) const override;
	virtual void SetPinBeingDroppedOnNode(UEdGraphPin* InSourcePin) const override { PinBeingDropped = InSourcePin; }
	virtual void InsertAdditionalActions(TArray<UBlueprint*> InBlueprints, TArray<UEdGraph*> InGraphs, TArray<UEdGraphPin*> InPins, FGraphActionListBuilderBase& OutAllActions) const override;
	virtual TSharedPtr<INameValidatorInterface> GetNameValidator(const UBlueprint* BlueprintObj, const FName& OriginalName, const UStruct* ValidationScope, const FName& ActionTypeId) const override;

	/** Returns true if the schema supports the script type */
	bool SupportsPinType(UScriptStruct* ScriptStruct) const;

	/** Create a graph node for a rig */
	UControlRigGraphNode* CreateGraphNode(UControlRigGraph* InGraph, const FName& InPropertyName) const;

	/** Automatically layout the passed-in nodes */
	void LayoutNodes(UControlRigGraph* InGraph, const TArray<UControlRigGraphNode*>& InNodes) const;

	/** Helper function to rename a node */
	void RenameNode(UControlRigGraphNode* Node, const FName& InNewNodeName) const;

	/** Helper function to recursively reset the pin defaults */
	virtual void ResetPinDefaultsRecursive(UEdGraphPin* InPin) const;

	/** Returns all of the applicable pin types for variables within a control rig */
	virtual void GetVariablePinTypes(TArray<FEdGraphPinType>& PinTypes) const;

	void StartGraphNodeInteraction(UEdGraphNode* InNode) const;
	void EndGraphNodeInteraction(UEdGraphNode* InNode) const;
	static TArray<UEdGraphNode*> GetNodesToMoveForNode(UEdGraphNode* InNode);
	FVector2D GetNodePositionAtStartOfInteraction(UEdGraphNode* InNode) const;

	void HandleModifiedEvent(ERigVMGraphNotifType InNotifType, URigVMGraph* InGraph, UObject* InSubject);

	// Allow derived classes to spawn derived node classes
	virtual TSubclassOf<UControlRigGraphNode> GetGraphNodeClass() const { return UControlRigGraphNode::StaticClass(); }

	static bool IsControlRigDefaultEvent(const FName& InEventName);

private:

	const UEdGraphPin* LastPinForCompatibleCheck = nullptr;
	bool bLastPinWasInput;
	mutable UEdGraphPin* PinBeingDropped = nullptr;

#if WITH_EDITOR
	mutable TArray<UEdGraphNode*> NodesBeingInteracted;
	mutable TMap<FName, FVector2D> NodePositionsDuringStart;
#endif

	friend class UControlRigRerouteNodeSpawner;
	friend class UControlRigIfNodeSpawner;
	friend class UControlRigSelectNodeSpawner;
	friend class UControlRigUnitNodeSpawner;
	friend class UControlRigArrayNodeSpawner;
};
