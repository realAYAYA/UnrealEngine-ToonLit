// Copyright Epic Games, Inc. All Rights Reserved.

#include "SUVEditor3DViewport.h"

#include "EditorViewportClient.h"
#include "SUVEditor3DViewportToolBar.h"
#include "UVEditorCommands.h"
#include "UVEditor3DViewportClient.h"

#define LOCTEXT_NAMESPACE "SUVEditor3DViewport"

void SUVEditor3DViewport::BindCommands()
{
	SAssetEditorViewport::BindCommands();

	const FUVEditorCommands& CommandInfos = FUVEditorCommands::Get();
	CommandList->MapAction(
		CommandInfos.EnableOrbitCamera,
		FExecuteAction::CreateLambda([this]() {
			StaticCastSharedPtr<FUVEditor3DViewportClient>(Client)->SetCameraMode(EUVEditor3DViewportClientCameraMode::Orbit);
			}),
		FCanExecuteAction::CreateLambda([this]() { return true; }),
		FIsActionChecked::CreateLambda([this]() { return StaticCastSharedPtr<FUVEditor3DViewportClient>(Client)->GetCameraMode() == EUVEditor3DViewportClientCameraMode::Orbit; }));

	CommandList->MapAction(
		CommandInfos.EnableFlyCamera,
		FExecuteAction::CreateLambda([this]() {
			StaticCastSharedPtr<FUVEditor3DViewportClient>(Client)->SetCameraMode(EUVEditor3DViewportClientCameraMode::Fly);
			}),
		FCanExecuteAction::CreateLambda([this]() { return true; }),
		FIsActionChecked::CreateLambda([this]() { return StaticCastSharedPtr<FUVEditor3DViewportClient>(Client)->GetCameraMode() == EUVEditor3DViewportClientCameraMode::Fly; }));

	CommandList->MapAction(
		CommandInfos.SetFocusCamera,
		FExecuteAction::CreateLambda([this]() {
			StaticCastSharedPtr<FUVEditor3DViewportClient>(Client)->FocusCameraOnSelection();
		}),
		FCanExecuteAction::CreateLambda([this]() { 
			return true;
		}));
}

TSharedPtr<SWidget> SUVEditor3DViewport::MakeViewportToolbar()
{
	return SNew(SUVEditor3DViewportToolBar, SharedThis(this))
		.CommandList(CommandList);
}

#undef LOCTEXT_NAMESPACE
