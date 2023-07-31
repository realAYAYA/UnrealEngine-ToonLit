// Copyright Epic Games, Inc. All Rights Reserved.

#include "LevelAssetEditorToolkit.h"

#include "EditorModeManager.h"
#include "LevelAssetEditorViewportClient.h"
#include "LevelAssetEditorViewport.h"

FLevelEditorAssetToolkit::FLevelEditorAssetToolkit(UAssetEditor* InOwningAssetEditor)
    : FBaseAssetToolkit(InOwningAssetEditor)
{
}

FLevelEditorAssetToolkit::~FLevelEditorAssetToolkit()
{
}

AssetEditorViewportFactoryFunction FLevelEditorAssetToolkit::GetViewportDelegate()
{
	AssetEditorViewportFactoryFunction TempViewportDelegate = [this](const FAssetEditorViewportConstructionArgs InArgs)
	{
		return SNew(SLevelAssetEditorViewport, InArgs)
			.EditorViewportClient(ViewportClient);
	};

	return TempViewportDelegate;
}

TSharedPtr<FEditorViewportClient> FLevelEditorAssetToolkit::CreateEditorViewportClient() const
{
	// Leaving the preview scene to nullptr default creates us a viewport that mirrors the main level editor viewport
	TSharedPtr<FEditorViewportClient> WrappedViewportClient = MakeShared<FLevelAssetEditorViewportClient>(EditorModeManager.Get(), nullptr);
	WrappedViewportClient->SetViewLocation(EditorViewportDefs::DefaultPerspectiveViewLocation);
	WrappedViewportClient->SetViewRotation(EditorViewportDefs::DefaultPerspectiveViewRotation);
	return WrappedViewportClient;
}

void FLevelEditorAssetToolkit::PostInitAssetEditor()
{
}
