// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/DataflowView.h"
#include "Templates/EnableIf.h"
#include "Dataflow/DataflowSelection.h"

#define LOCTEXT_NAMESPACE "DataflowView"

FDataflowNodeView::~FDataflowNodeView()
{
	if (SelectedNode)
	{
		if (TSharedPtr<FDataflowNode> DataflowNode = SelectedNode->GetDataflowNode())
		{
			if (DataflowNode->GetOnNodeInvalidatedDelegate().IsBound() && OnNodeInvalidatedDelegateHandle.IsValid())
			{
				DataflowNode->GetOnNodeInvalidatedDelegate().Remove(OnNodeInvalidatedDelegateHandle);
			}
		}
	}
}


bool FDataflowNodeView::SelectedNodeHaveSupportedOutputTypes(UDataflowEdNode* InNode)
{
	SetSupportedOutputTypes();

	if (InNode->IsBound())
	{
		if (TSharedPtr<FDataflowNode> DataflowNode = InNode->DataflowGraph->FindBaseNode(InNode->DataflowNodeGuid))
		{
			TArray<FDataflowOutput*> Outputs = DataflowNode->GetOutputs();

			for (FDataflowOutput* Output : Outputs)
			{
				for (const FString& OutputType : SupportedOutputTypes)
				{
					if (Output->GetType() == FName(*OutputType))
					{
						return true;
					}
				}
			}
		}
	}

	return false;
}


void FDataflowNodeView::SetContext(TSharedPtr<Dataflow::FContext>& InContext)
{
	Context = InContext;
}


void FDataflowNodeView::OnSelectedNodeChanged(UDataflowEdNode* InNode)
{
	if (!bIsPinnedDown)
	{
		//
		// Remove from broadcast
		//
		if (SelectedNode)
		{
			if (TSharedPtr<FDataflowNode> DataflowNode = SelectedNode->GetDataflowNode())
			{
				if (DataflowNode->GetOnNodeInvalidatedDelegate().IsBound() && OnNodeInvalidatedDelegateHandle.IsValid())
				{
					DataflowNode->GetOnNodeInvalidatedDelegate().Remove(OnNodeInvalidatedDelegateHandle);
				}
			}
		}

		SelectedNode = nullptr;

		if (InNode)  // nullptr is valid
		{
			if (SelectedNodeHaveSupportedOutputTypes(InNode))
			{
				SelectedNode = InNode;
			}

			// 
			// Bind OnNodeInvalidated() to new SelectedNode
			// 
			if (SelectedNode)
			{
				if (TSharedPtr<FDataflowNode> DataflowNode = SelectedNode->GetDataflowNode())
				{
					OnNodeInvalidatedDelegateHandle = DataflowNode->GetOnNodeInvalidatedDelegate().AddRaw(this, &FDataflowNodeView::OnNodeInvalidated);
				}
			}
		}

		UpdateViewData();
	}
}

void FDataflowNodeView::OnNodeInvalidated(FDataflowNode* InvalidatedNode)
{
	if (!bIsRefreshLocked)
	{
		if (TSharedPtr<FDataflowNode> DataflowNode = SelectedNode->GetDataflowNode())
		{
			if (InvalidatedNode == DataflowNode.Get())
			{
				UpdateViewData();
			}
		}
	}
}

void FDataflowNodeView::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(SelectedNode);
}


#undef LOCTEXT_NAMESPACE
