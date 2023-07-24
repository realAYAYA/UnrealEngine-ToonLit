// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNextInterfaceGraph_EdGraph.h"
#include "AnimNextInterfaceGraph_EditorData.h"
#include "AnimNextInterfaceUncookedOnlyUtils.h"

void UAnimNextInterfaceGraph_EdGraph::Initialize(UAnimNextInterfaceGraph_EditorData* InEditorData)
{
	InEditorData->ModifiedEvent.RemoveAll(this);
	InEditorData->ModifiedEvent.AddUObject(this, &UAnimNextInterfaceGraph_EdGraph::HandleModifiedEvent);
	InEditorData->VMCompiledEvent.RemoveAll(this);
	InEditorData->VMCompiledEvent.AddUObject(this, &UAnimNextInterfaceGraph_EdGraph::HandleVMCompiledEvent);
}

FRigVMClient* UAnimNextInterfaceGraph_EdGraph::GetRigVMClient() const
{
	if (const UAnimNextInterfaceGraph_EditorData* EditorData = GetTypedOuter<UAnimNextInterfaceGraph_EditorData>())
	{
		return (FRigVMClient*)EditorData->GetRigVMClient();
	}
	return nullptr;
}
