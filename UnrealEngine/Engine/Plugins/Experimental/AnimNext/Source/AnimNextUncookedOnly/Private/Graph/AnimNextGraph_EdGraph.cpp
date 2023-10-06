﻿// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/AnimNextGraph_EdGraph.h"
#include "Graph/AnimNextGraph_EditorData.h"

void UAnimNextGraph_EdGraph::Initialize(UAnimNextGraph_EditorData* InEditorData)
{
	InEditorData->ModifiedEvent.RemoveAll(this);
	InEditorData->ModifiedEvent.AddUObject(this, &UAnimNextGraph_EdGraph::HandleModifiedEvent);
	InEditorData->VMCompiledEvent.RemoveAll(this);
	InEditorData->VMCompiledEvent.AddUObject(this, &UAnimNextGraph_EdGraph::HandleVMCompiledEvent);
}

FRigVMClient* UAnimNextGraph_EdGraph::GetRigVMClient() const
{
	if (const UAnimNextGraph_EditorData* EditorData = GetTypedOuter<UAnimNextGraph_EditorData>())
	{
		return (FRigVMClient*)EditorData->GetRigVMClient();
	}
	return nullptr;
}
