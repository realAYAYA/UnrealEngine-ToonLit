// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SEditorViewport.h"
#include "SCommonEditorViewportToolbarBase.h"

class FSmartObjectAssetEditorViewportClient;
class FSmartObjectAssetToolkit;
class SSmartObjectViewportToolBar;
class FAdvancedPreviewScene;


class SSmartObjectViewport : public SEditorViewport, public ICommonEditorViewportToolbarInfoProvider
{
public:
	
	SLATE_BEGIN_ARGS(SSmartObjectViewport) {}
		SLATE_ARGUMENT(TSharedPtr<FSmartObjectAssetEditorViewportClient>, EditorViewportClient)
		SLATE_ARGUMENT(TSharedPtr<FSmartObjectAssetToolkit>, AssetEditorToolkit)
		SLATE_ARGUMENT(FAdvancedPreviewScene*, PreviewScene)
	SLATE_END_ARGS();

	void Construct(const FArguments& InArgs);
	virtual ~SSmartObjectViewport() override {}

	// ~ICommonEditorViewportToolbarInfoProvider interface
	virtual TSharedRef<class SEditorViewport> GetViewportWidget() override;
	virtual TSharedPtr<FExtender> GetExtenders() const override;
	virtual void OnFloatingButtonClicked() override;
	// ~End of ICommonEditorViewportToolbarInfoProvider interface

protected:

	// ~SEditorViewport interface
	virtual void BindCommands() override;
	virtual TSharedRef<FEditorViewportClient> MakeEditorViewportClient() override;
	virtual TSharedPtr<SWidget> MakeViewportToolbar() override;
	// ~End of SEditorViewport interface

	/** The viewport toolbar */
	TSharedPtr<SSmartObjectViewportToolBar> ViewportToolbar;

	/** Viewport client */
	TSharedPtr<FSmartObjectAssetEditorViewportClient> ViewportClient;

	/** The preview scene that we are viewing */
	FAdvancedPreviewScene* PreviewScene = nullptr;

	/** Asset editor toolkit we are embedded in */
	TWeakPtr<FSmartObjectAssetToolkit> AssetEditorToolkitPtr;
};
