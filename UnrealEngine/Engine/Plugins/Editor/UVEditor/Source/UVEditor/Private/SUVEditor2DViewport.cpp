// Copyright Epic Games, Inc. All Rights Reserved.

#include "SUVEditor2DViewport.h"

#include "SUVEditor2DViewportToolBar.h"
#include "UVEditor2DViewportClient.h"
#include "UVEditorCommands.h"
#include "ContextObjects/UVToolContextObjects.h" // UUVToolViewportButtonsAPI::ESelectionMode

#define LOCTEXT_NAMESPACE "SUVEditor2DViewport"

void SUVEditor2DViewport::BindCommands()
{
	SAssetEditorViewport::BindCommands();

	const FUVEditorCommands& CommandInfos = FUVEditorCommands::Get();
	CommandList->MapAction(
		CommandInfos.VertexSelection,
		FExecuteAction::CreateLambda([this]() {
			StaticCastSharedPtr<FUVEditor2DViewportClient>(Client)->SetSelectionMode(UUVToolViewportButtonsAPI::ESelectionMode::Vertex); 
		}),
		FCanExecuteAction::CreateLambda([this]() { 
			return StaticCastSharedPtr<FUVEditor2DViewportClient>(Client)->AreSelectionButtonsEnabled(); 
		}),
		FIsActionChecked::CreateLambda([this]() { 
			return StaticCastSharedPtr<FUVEditor2DViewportClient>(Client)->GetSelectionMode() == UUVToolViewportButtonsAPI::ESelectionMode::Vertex;
		}));

	CommandList->MapAction(
		CommandInfos.EdgeSelection,
		FExecuteAction::CreateLambda([this]() {
			StaticCastSharedPtr<FUVEditor2DViewportClient>(Client)->SetSelectionMode(UUVToolViewportButtonsAPI::ESelectionMode::Edge); 
		}),
		FCanExecuteAction::CreateLambda([this]() { 
			return StaticCastSharedPtr<FUVEditor2DViewportClient>(Client)->AreSelectionButtonsEnabled(); 
		}),
		FIsActionChecked::CreateLambda([this]() { 
			return StaticCastSharedPtr<FUVEditor2DViewportClient>(Client)->GetSelectionMode() == UUVToolViewportButtonsAPI::ESelectionMode::Edge;
		}));

	CommandList->MapAction(
		CommandInfos.TriangleSelection,
		FExecuteAction::CreateLambda([this]() {
			StaticCastSharedPtr<FUVEditor2DViewportClient>(Client)->SetSelectionMode(UUVToolViewportButtonsAPI::ESelectionMode::Triangle); 
		}),
		FCanExecuteAction::CreateLambda([this]() { 
			return StaticCastSharedPtr<FUVEditor2DViewportClient>(Client)->AreSelectionButtonsEnabled(); 
		}),
		FIsActionChecked::CreateLambda([this]() { 
			return StaticCastSharedPtr<FUVEditor2DViewportClient>(Client)->GetSelectionMode() == UUVToolViewportButtonsAPI::ESelectionMode::Triangle;
		}));

	CommandList->MapAction(
		CommandInfos.IslandSelection,
		FExecuteAction::CreateLambda([this]() {
			StaticCastSharedPtr<FUVEditor2DViewportClient>(Client)->SetSelectionMode(UUVToolViewportButtonsAPI::ESelectionMode::Island); 
		}),
		FCanExecuteAction::CreateLambda([this]() { 
			return StaticCastSharedPtr<FUVEditor2DViewportClient>(Client)->AreSelectionButtonsEnabled(); 
		}),
		FIsActionChecked::CreateLambda([this]() { 
			return StaticCastSharedPtr<FUVEditor2DViewportClient>(Client)->GetSelectionMode() == UUVToolViewportButtonsAPI::ESelectionMode::Island;
		}));

	CommandList->MapAction(
		CommandInfos.FullMeshSelection,
		FExecuteAction::CreateLambda([this]() {
			StaticCastSharedPtr<FUVEditor2DViewportClient>(Client)->SetSelectionMode(UUVToolViewportButtonsAPI::ESelectionMode::Mesh); 
		}),
		FCanExecuteAction::CreateLambda([this]() { 
			return StaticCastSharedPtr<FUVEditor2DViewportClient>(Client)->AreSelectionButtonsEnabled(); 
		}),
		FIsActionChecked::CreateLambda([this]() { 
			return StaticCastSharedPtr<FUVEditor2DViewportClient>(Client)->GetSelectionMode() == UUVToolViewportButtonsAPI::ESelectionMode::Mesh;
		}));
}

void SUVEditor2DViewport::AddOverlayWidget(TSharedRef<SWidget> OverlaidWidget)
{
	ViewportOverlay->AddSlot()
	[
		OverlaidWidget
	];
}

void SUVEditor2DViewport::RemoveOverlayWidget(TSharedRef<SWidget> OverlaidWidget)
{
	ViewportOverlay->RemoveSlot(OverlaidWidget);
}

TSharedPtr<SWidget> SUVEditor2DViewport::MakeViewportToolbar()
{
	return SNew(SUVEditor2DViewportToolBar)
		.CommandList(CommandList)
		.Viewport2DClient(StaticCastSharedPtr<FUVEditor2DViewportClient>(Client));
}

bool SUVEditor2DViewport::IsWidgetModeActive(UE::Widget::EWidgetMode Mode) const
{
	return static_cast<FUVEditor2DViewportClient*>(Client.Get())->AreWidgetButtonsEnabled() 
		&& Client->GetWidgetMode() == Mode;
}

#undef LOCTEXT_NAMESPACE
