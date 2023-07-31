// Copyright Epic Games, Inc. All Rights Reserved.

#include "ExampleAssetToolkit.h"

#include "EditorViewportClientWrapper.h"
#include "ExampleAssetEditorViewport.h"
#include "EditorModeManager.h"
#include "GizmoEdMode.h"

FExampleAssetToolkit::FExampleAssetToolkit(UAssetEditor* InOwningAssetEditor)
    : FBaseAssetToolkit(InOwningAssetEditor)
{
}

FExampleAssetToolkit::~FExampleAssetToolkit()
{
}

AssetEditorViewportFactoryFunction FExampleAssetToolkit::GetViewportDelegate()
{
	AssetEditorViewportFactoryFunction TempViewportDelegate = [this](const FAssetEditorViewportConstructionArgs InArgs)
	{
		return SNew(SExampleAssetEditorViewport, InArgs)
			.EditorViewportClient(ViewportClient);
	};

	return TempViewportDelegate;
}

TSharedPtr<FEditorViewportClient> FExampleAssetToolkit::CreateEditorViewportClient() const
{
	// Leaving the preview scene to nullptr default creates us a viewport that mirrors the main level editor viewport
	TSharedPtr<FEditorViewportClient> WrappedViewportClient = MakeShared<FEditorViewportClientWrapper>(EditorModeManager.Get(), nullptr);
	WrappedViewportClient->SetViewLocation(EditorViewportDefs::DefaultPerspectiveViewLocation);
	WrappedViewportClient->SetViewRotation(EditorViewportDefs::DefaultPerspectiveViewRotation);
	return WrappedViewportClient;
}

void FExampleAssetToolkit::PostInitAssetEditor()
{
	GetEditorModeManager().ActivateMode(GetDefault<UGizmoEdMode>()->GetID());
}
