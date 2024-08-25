// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Dataflow/DataflowEditorCommands.h"
#include "EdGraphUtilities.h"
#include "Framework/Commands/GenericCommands.h"
#include "Framework/Commands/UICommandList.h"
#include "GraphEditor.h"
#include "GraphEditorActions.h"
#include "Layout/SlateRect.h"
#include "NodeFactory.h"
#include "SGraphNode.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Layout/SConstraintCanvas.h"
#include "Widgets/SWidget.h"


class FDataflowEditorToolkit;
class FDataflowSNodeFactory;
class UDataflow;
struct FDataflowConnection;
namespace Dataflow {
	class FContext;
}
/**
 * The SDataflowGraphEditpr class is a specialization of SGraphEditor
 * to display and manipulate the actions of a Dataflow asset
 * 
 * see(SDataprepGraphEditor for reference)
 */
class DATAFLOWEDITOR_API SDataflowGraphEditor : public SGraphEditor, public FGCObject
{
public:

	SLATE_BEGIN_ARGS(SDataflowGraphEditor)
		: _AdditionalCommands(static_cast<FUICommandList*>(nullptr))
		, _GraphToEdit(nullptr)
	{}

	SLATE_ARGUMENT(TSharedPtr<FUICommandList>, AdditionalCommands)
	SLATE_ATTRIBUTE(FGraphAppearanceInfo, Appearance)
	SLATE_ARGUMENT(UEdGraph*, GraphToEdit)
	SLATE_ARGUMENT(FGraphEditorEvents, GraphEvents)
	SLATE_ARGUMENT(TSharedPtr<IStructureDetailsView>, DetailsView)
	SLATE_ARGUMENT(FDataflowEditorCommands::FGraphEvaluationCallback, EvaluateGraph)
	SLATE_ARGUMENT(FDataflowEditorCommands::FOnDragDropEventCallback, OnDragDropEvent)
	SLATE_END_ARGS()

	// This delegate exists in SGraphEditor but it is not multicast, and we are going to bind it to OnSelectedNodesChanged().
	// This new multicast delegate will be broadcast from the OnSelectedNodesChanged handler in case another class wants to be notified.
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnSelectionChangedMulticast, const FGraphPanelSelectionSet&)
	FOnSelectionChangedMulticast OnSelectionChangedMulticast;

	DECLARE_MULTICAST_DELEGATE_OneParam(FOnNodeDeletedMulticast, const FGraphPanelSelectionSet&)
	FOnNodeDeletedMulticast OnNodeDeletedMulticast;

	// SWidget overrides
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply OnKeyUp(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply OnDragOver(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;
	virtual FReply OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;
	//virtual void OnDragEnter(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;
	//virtual void OnDragLeave(const FDragDropEvent& DragDropEvent) override;
	// end SWidget

	/** */
	void Construct(const FArguments& InArgs, UObject* AssetOwner);

	/** */
	void EvaluateNode();

	/** */
	void DeleteNode();

	/** */
	void OnSelectedNodesChanged(const TSet<UObject*>& NewSelection);

	/** */
	void CreateComment();

	/** */
	void CreateVertexSelectionNode(const FString& InArray);

	/** */
	void AlignTop();

	/** */
	void AlignMiddle();

	/** */
	void AlignBottom();

	/** */
	void AlignLeft();

	/** */
	void AlignCenter();

	/** */
	void AlignRight();

	/** */
	void StraightenConnections();

	/** */
	void DistributeHorizontally();

	/** */
	void DistributeVertically();

	/** */
	void ToggleEnabledState();

	/** */
	void DuplicateSelectedNodes();

	/** */
	void ZoomToFitGraph();

	/** */
	void CopySelectedNodes();

	/** */
	void CutSelectedNodes();

	/** */
	void PasteSelectedNodes();

	SGraphEditor* GetGraphEditor() { return (SGraphEditor*)this; }

	/** FGCObject interface */
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override { return TEXT("SDataflowGraphEditor"); }

private:
	/** Add an additional option pin to all selected Dataflow nodes for those that overrides the AddPin function. */
	void OnAddOptionPin();
	/** Return whether all currently selected Dataflow nodes can execute the AddPin function. */
	bool CanAddOptionPin() const;

	/** Remove an option pin from all selected Dataflow nodes for those that overrides the RemovePin function. */
	void OnRemoveOptionPin();
	/** Return whether all currently selected Dataflow nodes can execute the RemovePin function. */
	bool CanRemoveOptionPin() const;

	bool GetPinVisibility(SGraphEditor::EPinVisibility InVisibility) const;

	FDataflowEditorCommands::FOnDragDropEventCallback OnDragDropEventCallback;
	FDataflowEditorCommands::FGraphEvaluationCallback EvaluateGraphCallback;

	/** The asset that ownes this dataflow graph */
	TWeakObjectPtr<UObject> AssetOwner;

	/** The dataflow asset associated with this graph */
	TWeakObjectPtr<UDataflow> DataflowAsset;

	/** Command list associated with this graph editor */
	TSharedPtr<FUICommandList> GraphEditorCommands;

	/** Factory to create the associated SGraphNode classes for Dataflow graph's UEdGraph classes */
	static TSharedPtr<FDataflowSNodeFactory> NodeFactory;

	/** The details view that responds to this widget. */
	TSharedPtr<IStructureDetailsView> DetailsView;

	bool VKeyDown = false;
	bool LeftControlKeyDown = false;
	bool RightControlKeyDown = false;
};
