// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"
#include "Dataflow/DataflowEdNode.h"
#include "Dataflow/SelectionViewWidget.h"
#include "Dataflow/DataflowSelection.h"


/**
*
* Base listener class to interface between the DataflowToolkit and Dataflow views
*
*/
class IDataflowViewListener
{
public:
	virtual void OnSelectedNodeChanged(UDataflowEdNode* InNode) = 0;  // nullptr is valid
	virtual void OnNodeInvalidated(FDataflowNode* InvalidatedNode) = 0;
};


/**
*
* FDataflowNodeView class implements common functions for single node based Dataflow views
*
*/
class FDataflowNodeView : public IDataflowViewListener, public FGCObject
{
public:
	virtual ~FDataflowNodeView();

	UDataflowEdNode* GetSelectedNode() const { return SelectedNode; }
	bool SelectedNodeHaveSupportedOutputTypes(UDataflowEdNode* InNode);

	TArray<FString>& GetSupportedOutputTypes() { return SupportedOutputTypes; }

	TSharedPtr<Dataflow::FContext> GetContext() { return Context; }
	void SetContext(TSharedPtr<Dataflow::FContext>& InContext);

	/**
	* Virtual functions to overwrite in view widget classes
	*/
	virtual void UpdateViewData() = 0;
	virtual void SetSupportedOutputTypes() = 0;

	/**
	* Callback for PinnedDown change
	*/
	void OnPinnedDownChanged(const bool State) { bIsPinnedDown = State; }

	/**
	* Callback for RefreshLock change
	*/
	void OnRefreshLockedChanged(const bool State) { bIsRefreshLocked = State; }

	/**
	* Virtual function overrides from IDataflowViewListener base class
	*/
	virtual void OnSelectedNodeChanged(UDataflowEdNode* InNode) override;  // nullptr is valid
	virtual void OnNodeInvalidated(FDataflowNode* InvalidatedNode) override;


	/** FGCObject interface */
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override { return TEXT("FDataflowNodeView"); }

private:
	TObjectPtr<UDataflowEdNode> SelectedNode = nullptr;
	TSharedPtr<Dataflow::FContext> Context;
	bool bIsPinnedDown = false;
	bool bIsRefreshLocked = false;

	TArray<FString> SupportedOutputTypes;

	FDelegateHandle OnNodeInvalidatedDelegateHandle;
};


