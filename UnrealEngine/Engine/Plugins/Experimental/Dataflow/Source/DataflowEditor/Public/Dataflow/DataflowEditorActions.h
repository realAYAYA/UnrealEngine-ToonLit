// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "Styling/AppStyle.h"

struct FDataflowOutput;
class IStructureDetailsView;
class UDataflow;
class UDataflowEdNode;
struct FDataflowNode;

typedef TSet<class UObject*> FGraphPanelSelectionSet;

/*
* FDataflowEditorCommandsImpl
* 
*/
class FDataflowEditorCommandsImpl : public TCommands<FDataflowEditorCommandsImpl>
{
public:

	FDataflowEditorCommandsImpl()
		: TCommands<FDataflowEditorCommandsImpl>( TEXT("DataflowEditor"), NSLOCTEXT("Contexts", "DataflowEditor", "Scene Graph Editor"), NAME_None, FAppStyle::GetAppStyleSetName() )
	{
	}	

	virtual ~FDataflowEditorCommandsImpl()
	{
	}

	DATAFLOWEDITOR_API virtual void RegisterCommands() override;

	TSharedPtr< FUICommandInfo > EvaluateNode;
	TSharedPtr< FUICommandInfo > CreateComment;
	TSharedPtr< FUICommandInfo > ToggleEnabledState;

	TMap< FName, TSharedPtr<FUICommandInfo> > CreateNodesMap;
};

class DATAFLOWEDITOR_API FDataflowEditorCommands
{
public:
	typedef TFunction<void(FDataflowNode*, FDataflowOutput*)> FGraphEvaluationCallback;
	typedef TFunction<void(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)> FOnDragDropEventCallback;

	static void Register();
	static void Unregister();

	static const FDataflowEditorCommandsImpl& Get();

	/*
	*  EvaluateNodes
	*/
	static void EvaluateNodes(const FGraphPanelSelectionSet& SelectedNodes, FGraphEvaluationCallback);
	
	/*
	*  DeleteNodes
	*/
	static void DeleteNodes(UDataflow* Graph, const FGraphPanelSelectionSet& SelectedNodes);
	
	/*
	*  OnSelectedNodesChanged
	*/
	static void OnSelectedNodesChanged(TSharedPtr<IStructureDetailsView> PropertiesEditor, UObject* Asset, UDataflow* Graph, const TSet<UObject*>& NewSelection);

	/*
	*  ToggleEnabledState
	*/
	static void ToggleEnabledState(UDataflow* Graph);

};
