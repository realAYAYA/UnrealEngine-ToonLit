// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Tools/BaseAssetToolkit.h"

class FAdvancedPreviewScene;
class FUVEditorModeUILayer;
class FUVEditorPreviewModeManager;
class IDetailsView;
class SDockTab;
class SBorder;
class SWidget;
class UInteractiveToolsContext;
class UInputRouter;
class UUVToolViewportButtonsAPI;
class UUVTool2DViewportAPI;

/**
 * The toolkit is supposed to act as the UI manager for the asset editor. It's responsible 
 * for setting up viewports and most toolbars, except for the internals of the mode panel.
 * However, because the toolkit also sets up the mode manager, and much of the important
 * state is held in the UUVEditorMode managed by the mode manager, the toolkit also ends up
 * initializing the UV mode.
 * Thus, the FUVEdiotrToolkit ends up being the central place for the UV Asset editor setup.
 */
class UVEDITOR_API FUVEditorToolkit : public FBaseAssetToolkit
{
public:
	FUVEditorToolkit(UAssetEditor* InOwningAssetEditor);
	virtual ~FUVEditorToolkit();

	static const FName InteractiveToolsPanelTabID;
	static const FName LivePreviewTabID;

	FPreviewScene* GetPreviewScene() { return UnwrapScene.Get(); }

	// FBaseAssetToolkit
	virtual void CreateWidgets() override;

	// FAssetEditorToolkit
	virtual void AddViewportOverlayWidget(TSharedRef<SWidget> InViewportOverlayWidget) override;
	virtual void RemoveViewportOverlayWidget(TSharedRef<SWidget> InViewportOverlayWidget) override;
	virtual void CreateEditorModeManager() override;
	virtual FText GetToolkitName() const override;
	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;
	virtual FText GetToolkitToolTipText() const override;
	virtual void RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override;
	virtual bool OnRequestClose() override;
	virtual void OnClose() override;
	virtual void SaveAsset_Execute() override;
	virtual bool CanSaveAsset() const override;
	virtual bool CanSaveAssetAs() const override;
	void OnToolkitHostingStarted(const TSharedRef<IToolkit>& Toolkit) override;
	void OnToolkitHostingFinished(const TSharedRef<IToolkit>& Toolkit) override;

	// IAssetEditorInstance
	// This is important because if this returns true, attempting to edit a static mesh that is
	// open in the UV editor may open the UV editor instead of opening the static mesh editor.
	virtual bool IsPrimaryEditor() const override { return false; };

protected:

	TSharedRef<SDockTab> SpawnTab_InteractiveToolsPanel(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_LivePreview(const FSpawnTabArgs& Args);

	// FBaseAssetToolkit
	virtual AssetEditorViewportFactoryFunction GetViewportDelegate() override;
	virtual TSharedPtr<FEditorViewportClient> CreateEditorViewportClient() const override;

	// FAssetEditorToolkit
	virtual void PostInitAssetEditor() override;
	virtual const FSlateBrush* GetDefaultTabIcon() const override;
	virtual FLinearColor GetDefaultTabColor() const override;

	/** Scene in which the 2D unwrapped uv meshes live. */
	TUniquePtr<FPreviewScene> UnwrapScene;

	/** Scene in which the 3D preview meshes of the assets live. */
	TUniquePtr<FAdvancedPreviewScene> LivePreviewScene;

	// These are related to the 3D "live preview" viewport. The 2d unwrap viewport things are
	// stored in FBaseAssetToolkit::ViewportTabContent, ViewportDelegate, ViewportClient
	TSharedPtr<class FEditorViewportTabContent> LivePreviewTabContent;
	AssetEditorViewportFactoryFunction LivePreviewViewportDelegate;
	TSharedPtr<FEditorViewportClient> LivePreviewViewportClient;
	
	TSharedPtr<FAssetEditorModeManager> LivePreviewEditorModeManager;
	TObjectPtr<UInputRouter> LivePreviewInputRouter = nullptr;

	TWeakPtr<SEditorViewport> UVEditor2DViewport;
	UUVToolViewportButtonsAPI* ViewportButtonsAPI = nullptr;

	UUVTool2DViewportAPI* UVTool2DViewportAPI = nullptr;

	TSharedPtr<FUVEditorModeUILayer> ModeUILayer;
	TSharedPtr<FWorkspaceItem> UVEditorMenuCategory;
};