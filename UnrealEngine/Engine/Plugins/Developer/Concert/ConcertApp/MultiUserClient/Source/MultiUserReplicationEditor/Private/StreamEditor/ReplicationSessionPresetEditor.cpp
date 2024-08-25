// Copyright Epic Games, Inc. All Rights Reserved.

#include "ReplicationSessionPresetEditor.h"

#include "ReplicationSessionPresetEditorToolkit.h"

void UReplicationSessionPresetEditor::SetObjectToEdit(UObject* InObject)
{
	ObjectToEdit = InObject;
}

void UReplicationSessionPresetEditor::GetObjectsToEdit(TArray<UObject*>& OutObjectsToEdit)
{
	OutObjectsToEdit = { ObjectToEdit };
}

TSharedPtr<FBaseAssetToolkit> UReplicationSessionPresetEditor::CreateToolkit()
{
	return MakeShared<UE::MultiUserReplicationEditor::FReplicationSessionPresetEditorToolkit>(this);
}
