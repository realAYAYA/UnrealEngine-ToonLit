// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

class FSceneViewport;
class SViewport;
class FDMXPixelMappingPreviewViewportClient;
class FDMXPixelMappingToolkit;
class UTexture;
class FTextureResource;
class UDMXPixelMappingOutputComponent;
struct FOptionalSize;

class SDMXPixelMappingPreviewViewport
	: public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SDMXPixelMappingPreviewViewport) { }
	SLATE_END_ARGS()

public:
	/**
	 * Constructs the widget.
	 *
	 * @param InArgs The construction arguments.
	 */
	void Construct(const FArguments& InArgs, const TSharedPtr<FDMXPixelMappingToolkit>& InToolkit);

	/** Enable viewport rendering */
	void EnableRendering();

	/** Disable viewport rendering */
	void DisableRendering();

	TSharedPtr<FSceneViewport> GetViewport() const { return Viewport; }

	TSharedPtr<SViewport> GetViewportWidget() const { return ViewportWidget; }

	//~ Begin SWidget interface
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	//~ End of SWidget interface

	FOptionalSize GetPreviewAreaWidth() const;

	FOptionalSize GetPreviewAreaHeight() const;

private:

	/** Viewport client. */
	TSharedPtr<FDMXPixelMappingPreviewViewportClient> ViewportClient;

	/** Slate viewport for rendering and IO. */
	TSharedPtr<FSceneViewport> Viewport;

	/** Viewport widget. */
	TSharedPtr<SViewport> ViewportWidget;

	/** Weak pointer to the editor toolkit */
	TWeakPtr<FDMXPixelMappingToolkit> ToolkitWeakPtr;

	/** Is rendering currently enabled? (disabled when reimporting a texture) */
	bool bIsRenderingEnabled;
};
