// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Dataflow/DataflowEditorActions.h"
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
class DATAFLOWEDITOR_API SDataflowGraphEditor : public SGraphEditor
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

	// SWidget overrides
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

	SGraphEditor* GetGraphEditor() { return (SGraphEditor*)this; }

private:
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
};
