// Copyright Epic Games, Inc. All Rights Reserved.

#include "DataInterfaceGraph_EdGraph.h"
#include "DataInterfaceGraph_EditorData.h"
#include "DataInterfaceUncookedOnlyUtils.h"

void UDataInterfaceGraph_EdGraph::Initialize(UDataInterfaceGraph_EditorData* InEditorData)
{
	InEditorData->ModifiedEvent.RemoveAll(this);
	InEditorData->ModifiedEvent.AddUObject(this, &UDataInterfaceGraph_EdGraph::HandleModifiedEvent);
	InEditorData->VMCompiledEvent.RemoveAll(this);
	InEditorData->VMCompiledEvent.AddUObject(this, &UDataInterfaceGraph_EdGraph::HandleVMCompiledEvent);
}

FRigVMClient* UDataInterfaceGraph_EdGraph::GetRigVMClient() const
{
	if (const UDataInterfaceGraph_EditorData* EditorData = GetTypedOuter<UDataInterfaceGraph_EditorData>())
	{
		return (FRigVMClient*)EditorData->GetRigVMClient();
	}
	return nullptr;
}
