// Copyright Epic Games, Inc. All Rights Reserved.

#include "Param/AnimNextParameterBlockGraph.h"
#include "Param/AnimNextParameterBlock_EdGraph.h"

FText UAnimNextParameterBlockGraph::GetDisplayName() const
{
	return FText::FromName(GraphName);
}

FText UAnimNextParameterBlockGraph::GetDisplayNameTooltip() const
{
	return FText::FromName(GraphName);
}

void UAnimNextParameterBlockGraph::SetEntryName(FName InName, bool bSetupUndoRedo)
{
	if(bSetupUndoRedo)
	{
		Modify();
	}

	GraphName = InName;

	BroadcastModified();
}

URigVMGraph* UAnimNextParameterBlockGraph::GetRigVMGraph() const
{
	return Graph;
}

URigVMEdGraph* UAnimNextParameterBlockGraph::GetEdGraph() const
{
	return EdGraph;
}