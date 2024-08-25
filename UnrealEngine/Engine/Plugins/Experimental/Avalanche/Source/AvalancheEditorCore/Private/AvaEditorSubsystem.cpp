// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaEditorSubsystem.h"

bool UAvaEditorSubsystem::DoesSupportWorldType(const EWorldType::Type InWorldType) const
{
	return InWorldType == EWorldType::Editor;
}

void UAvaEditorSubsystem::OnEditorActivated(const TSharedRef<IAvaEditor>& InAvaEditor)
{
	ActiveEditorWeak = InAvaEditor;
}

void UAvaEditorSubsystem::OnEditorDeactivated()
{
	ActiveEditorWeak.Reset();
}
