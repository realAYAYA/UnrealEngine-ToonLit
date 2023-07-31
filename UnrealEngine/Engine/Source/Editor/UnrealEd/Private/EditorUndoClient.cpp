// Copyright Epic Games, Inc. All Rights Reserved.

#include "EditorUndoClient.h"
#include "Editor.h"

FEditorUndoClient::~FEditorUndoClient()
{
	if (GEditor)
	{
		GEditor->UnregisterForUndo(this);
	}
}

FSelfRegisteringEditorUndoClient::FSelfRegisteringEditorUndoClient()
{
	if (GEditor)
	{
		GEditor->RegisterForUndo(this);
	}
}