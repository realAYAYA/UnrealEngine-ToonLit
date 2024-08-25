// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/AnimNextGraph_EdGraph.h"
#include "Graph/AnimNextGraph_EditorData.h"

void UAnimNextGraph_EdGraph::PostLoad()
{
	Super::PostLoad();

	UAnimNextRigVMAssetEditorData* EditorData = GetTypedOuter<UAnimNextRigVMAssetEditorData>();
	check(EditorData);
	Initialize(EditorData);
}

void UAnimNextGraph_EdGraph::Initialize(UAnimNextRigVMAssetEditorData* InEditorData)
{
	InEditorData->RigVMGraphModifiedEvent.RemoveAll(this);
	InEditorData->RigVMGraphModifiedEvent.AddUObject(this, &UAnimNextGraph_EdGraph::HandleModifiedEvent);
	InEditorData->RigVMCompiledEvent.RemoveAll(this);
	InEditorData->RigVMCompiledEvent.AddUObject(this, &UAnimNextGraph_EdGraph::HandleVMCompiledEvent);
}

FRigVMClient* UAnimNextGraph_EdGraph::GetRigVMClient() const
{
	if (const UAnimNextGraph_EditorData* EditorData = GetTypedOuter<UAnimNextGraph_EditorData>())
	{
		return (FRigVMClient*)EditorData->GetRigVMClient();
	}
	return nullptr;
}
