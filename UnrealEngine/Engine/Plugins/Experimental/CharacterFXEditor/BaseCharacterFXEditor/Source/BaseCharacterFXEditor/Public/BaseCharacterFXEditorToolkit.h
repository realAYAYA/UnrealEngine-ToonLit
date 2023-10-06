// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Tools/BaseAssetToolkit.h"
#include "BaseCharacterFXEditorModeUILayer.h"
#include "PreviewScene.h"

class UBaseCharacterFXEditorMode;

/**
 * The toolkit is supposed to act as the UI manager for the asset editor. It's responsible 
 * for setting up viewports and most toolbars, except for the internals of the mode panel.
 * However, because the toolkit also sets up the mode manager, and much of the important
 * state is held in the mode managed by the mode manager, the toolkit also ends up
 * initializing the mode.
 * Thus, the FBaseCharacterFXEditorToolkit ends up being the central place for the CharacterFX Asset Editor setup.
 */

class BASECHARACTERFXEDITOR_API FBaseCharacterFXEditorToolkit : public FBaseAssetToolkit
{
public:
	    
	FBaseCharacterFXEditorToolkit(UAssetEditor* InOwningAssetEditor, const FName& ModuleName);

	virtual void OnToolkitHostingStarted(const TSharedRef<IToolkit>& Toolkit) override;
	virtual void OnToolkitHostingFinished(const TSharedRef<IToolkit>& Toolkit) override;
	virtual bool OnRequestClose(EAssetEditorCloseReason InCloseReason) override;
	virtual void RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override;

	virtual AssetEditorViewportFactoryFunction GetViewportDelegate() override;
	virtual void AddViewportOverlayWidget(TSharedRef<SWidget> InViewportOverlayWidget) override;
	virtual void RemoveViewportOverlayWidget(TSharedRef<SWidget> InViewportOverlayWidget) override;

protected:

	// Override this to get the Mode ID of the concrete CharacterFXEditorMode
	virtual FEditorModeID GetEditorModeId() const PURE_VIRTUAL(FBaseCharacterFXEditorToolkit::GetEditorModeId, return FEditorModeID("EM_ERROR"););

	// Helpers for PostInitAssetEditor
	
	// Override this to create the concrete CharacterFXEditorModeUILayer
	virtual void CreateEditorModeUILayer();

	// Override this to provide the EdMode with any custom data it needs
	virtual void InitializeEdMode(UBaseCharacterFXEditorMode* EdMode);

	virtual void PostInitAssetEditor() override;

	// Called from FBaseAssetToolkit::CreateWidgets.
	virtual void CreateEditorModeManager() override;
	
	// Called from FBaseAssetToolkit::CreateWidgets to populate ViewportClient, but otherwise only used 
	// in our own viewport delegate
	virtual TSharedPtr<FEditorViewportClient> CreateEditorViewportClient() const override;

	// Handles the hosting of additional toolkits, such as the mode toolkit
	TSharedPtr<FBaseCharacterFXEditorModeUILayer> ModeUILayer;

	// PreviewScene showing the objects being edited
	TUniquePtr<FPreviewScene> ObjectScene;

	// A default "CharacterFXEditor" category is created in RegisterTabSpawners. Override that function to set this to a concrete class type
	TSharedPtr<FWorkspaceItem> EditorMenuCategory;
};

