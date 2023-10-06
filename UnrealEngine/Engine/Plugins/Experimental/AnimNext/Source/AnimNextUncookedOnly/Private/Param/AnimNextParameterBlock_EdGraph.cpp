﻿// Copyright Epic Games, Inc. All Rights Reserved.

#include "Param/AnimNextParameterBlock_EdGraph.h"
#include "Param/AnimNextParameterBlock_EditorData.h"

void UAnimNextParameterBlock_EdGraph::Initialize(UAnimNextParameterBlock_EditorData* InEditorData)
{
	InEditorData->RigVMGraphModifiedEvent.RemoveAll(this);
	InEditorData->RigVMGraphModifiedEvent.AddUObject(this, &UAnimNextParameterBlock_EdGraph::HandleModifiedEvent);
	InEditorData->RigVMCompiledEvent.RemoveAll(this);
	InEditorData->RigVMCompiledEvent.AddUObject(this, &UAnimNextParameterBlock_EdGraph::HandleVMCompiledEvent);
}

FRigVMClient* UAnimNextParameterBlock_EdGraph::GetRigVMClient() const
{
	if (const UAnimNextParameterBlock_EditorData* EditorData = GetTypedOuter<UAnimNextParameterBlock_EditorData>())
	{
		return (FRigVMClient*)EditorData->GetRigVMClient();
	}
	return nullptr;
}
