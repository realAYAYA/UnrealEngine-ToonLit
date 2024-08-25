// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaEdMode.h"
#include "AvaEdModeToolkit.h"
#include "AvaEditorSubsystem.h"
#include "EditorModeManager.h"
#include "IAvaEditor.h"
#include "Tools/Modes.h"

#define LOCTEXT_NAMESPACE "AvaEdMode"

const FEditorModeID UAvaEdMode::ModeID(TEXT("EM_MotionDesign"));

UAvaEdMode::UAvaEdMode()
{
	Info = FEditorModeInfo(UAvaEdMode::ModeID, LOCTEXT("ModeDisplayName", "Motion Design"), FSlateIcon(), false);
}

void UAvaEdMode::Initialize()
{
	Super::Initialize();

	if (UAvaEditorSubsystem* EditorSubsystem = UAvaEditorSubsystem::Get(Owner))
	{
		EditorWeak = EditorSubsystem->GetActiveEditor();
	}
}

bool UAvaEdMode::UsesToolkits() const
{
	// Note: Uses Toolkits returns false as this is for FModeToolkit only
	return true;
}

void UAvaEdMode::CreateToolkit()
{
	Toolkit = MakeShared<FAvaEdModeToolkit>(this);
}

bool UAvaEdMode::ProcessEditCut()
{
	TSharedPtr<IAvaEditor> Editor = GetEditor();
	return Editor.IsValid() && Editor->EditCut();
}

bool UAvaEdMode::ProcessEditCopy()
{
	TSharedPtr<IAvaEditor> Editor = GetEditor();
	return Editor.IsValid() && Editor->EditCopy();
}

bool UAvaEdMode::ProcessEditPaste()
{
	TSharedPtr<IAvaEditor> Editor = GetEditor();
	return Editor.IsValid() && Editor->EditPaste();
}

bool UAvaEdMode::ProcessEditDuplicate()
{
	TSharedPtr<IAvaEditor> Editor = GetEditor();
	return Editor.IsValid() && Editor->EditDuplicate();
}

bool UAvaEdMode::ProcessEditDelete()
{
	TSharedPtr<IAvaEditor> Editor = GetEditor();
	return Editor.IsValid() && Editor->EditDelete();
}

TSharedPtr<FUICommandList> UAvaEdMode::GetToolkitCommands() const
{
	if (Toolkit.IsValid())
	{
		return Toolkit->GetToolkitCommands();
	}

	return nullptr;
}

#undef LOCTEXT_NAMESPACE
