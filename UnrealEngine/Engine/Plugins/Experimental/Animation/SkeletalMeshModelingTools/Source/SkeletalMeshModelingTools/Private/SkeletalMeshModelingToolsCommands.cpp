// Copyright Epic Games, Inc. All Rights Reserved.

#include "SkeletalMeshModelingToolsCommands.h"

#include "SkeletalMesh/SkeletonEditingTool.h"

#define LOCTEXT_NAMESPACE "SkeletalMeshModelingToolsCommands"


void FSkeletalMeshModelingToolsCommands::RegisterCommands()
{
	UI_COMMAND(ToggleEditingToolsMode, "Enable Editing Tools", "Toggles editing tools on or off.", EUserInterfaceActionType::ToggleButton, FInputChord());
	
	UI_COMMAND(NewBone, "New Bone", "Add new Bone.", EUserInterfaceActionType::Button, FInputChord(EKeys::N, EModifierKey::Control));
	UI_COMMAND(RemoveBone, "Remove Bone", "Remove selected bone(s).", EUserInterfaceActionType::Button, FInputChord(EKeys::Delete), FInputChord(EKeys::BackSpace));
	UI_COMMAND(UnParentBone, "Unparent Bone", "Unparent selected bone(s).", EUserInterfaceActionType::Button, FInputChord(EKeys::P, EModifierKey::Shift));
	UI_COMMAND(RenameBone, "Rename Bone", "Rename the selected bone.", EUserInterfaceActionType::Button, FInputChord(EKeys::F2));

	UI_COMMAND(CopyBones, "Copy Bone(s)", "Copy selected bone(s).", EUserInterfaceActionType::Button, FInputChord(EKeys::C, EModifierKey::Control));
	UI_COMMAND(PasteBones, "Paste Bone(s)", "Paste selected bone(s).", EUserInterfaceActionType::Button, FInputChord(EKeys::V, EModifierKey::Control));
	UI_COMMAND(DuplicateBones, "Duplicate Bone(s)", "Duplicate selected bone(s).", EUserInterfaceActionType::Button, FInputChord(EKeys::D, EModifierKey::Control));
}

const FSkeletalMeshModelingToolsCommands& FSkeletalMeshModelingToolsCommands::Get()
{
	return TCommands<FSkeletalMeshModelingToolsCommands>::Get();
}

FSkeletalMeshModelingToolsActionCommands::FSkeletalMeshModelingToolsActionCommands() : 
	TInteractiveToolCommands<FSkeletalMeshModelingToolsActionCommands>(
		"SeletalMeshModelingToolsEditMode", // Context name for fast lookup
		NSLOCTEXT("Contexts", "SeletalMeshModelingToolsEditMode", "Skeletal Mesh Modeling Tools - Shared Shortcuts"), // Localized context name for displaying
		NAME_None, // Parent
		FAppStyle::GetAppStyleSetName() // Icon Style Set
	)
{
}

void FSkeletalMeshModelingToolsActionCommands::GetToolDefaultObjectList(TArray<UInteractiveTool*>& ToolCDOs)
{}

void FSkeletalMeshModelingToolsActionCommands::RegisterAllToolActions()
{
	FSkeletonEditingToolActionCommands::Register();
}

void FSkeletalMeshModelingToolsActionCommands::UnregisterAllToolActions()
{
	FSkeletonEditingToolActionCommands::Unregister();
}

void FSkeletalMeshModelingToolsActionCommands::UpdateToolCommandBinding(UInteractiveTool* Tool, TSharedPtr<FUICommandList> UICommandList, bool bUnbind)
{
#define UPDATE_BINDING(CommandsType)  if (!bUnbind) CommandsType::Get().BindCommandsForCurrentTool(UICommandList, Tool); else CommandsType::Get().UnbindActiveCommands(UICommandList);

	if (ExactCast<USkeletonEditingTool>(Tool) != nullptr)
	{
		UPDATE_BINDING(FSkeletonEditingToolActionCommands);
	}
}

#define DEFINE_TOOL_ACTION_COMMANDS(CommandsClassName, ContextNameString, SettingsDialogString, ToolClassName ) \
CommandsClassName::CommandsClassName() : TInteractiveToolCommands<CommandsClassName>( \
ContextNameString, NSLOCTEXT("Contexts", ContextNameString, SettingsDialogString), NAME_None, FAppStyle::GetAppStyleSetName()) {} \
void CommandsClassName::GetToolDefaultObjectList(TArray<UInteractiveTool*>& ToolCDOs) \
{\
ToolCDOs.Add(GetMutableDefault<ToolClassName>()); \
}

DEFINE_TOOL_ACTION_COMMANDS(FSkeletonEditingToolActionCommands, "SkeletalMeshModelingToolsSkeletonEditing", "Skeletal Mesh Modeling Tools - Skeleton Editing Tool", USkeletonEditingTool);

#undef LOCTEXT_NAMESPACE
