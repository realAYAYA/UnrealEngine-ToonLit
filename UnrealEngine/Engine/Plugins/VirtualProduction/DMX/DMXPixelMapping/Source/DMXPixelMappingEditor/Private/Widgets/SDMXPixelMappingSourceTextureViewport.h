// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Layout/Visibility.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class FSceneViewport;
class SViewport;
class FDMXPixelMappingToolkit;
class FDMXPixelMappingSourceTextureViewportClient;
class UTexture;
class FTextureResource;
struct FOptionalSize;

class SDMXPixelMappingSourceTextureViewport
	: public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SDMXPixelMappingSourceTextureViewport) { }
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

	// Begin SWidget interface
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	// End of SWidget interface

	FOptionalSize GetPreviewAreaWidth() const;

	FOptionalSize GetPreviewAreaHeight() const;

	UTexture* GetInputTexture() const;

	const FTextureResource* GetInputTextureResource() const;

private:
	/** Weak pointer to the editor toolkit */
	TWeakPtr<FDMXPixelMappingToolkit> ToolkitWeakPtr;

	/** viewport client. */
	TSharedPtr<FDMXPixelMappingSourceTextureViewportClient> ViewportClient;

	/** Slate viewport for rendering and IO. */
	TSharedPtr<FSceneViewport> Viewport;

	/** Viewport widget. */
	TSharedPtr<SViewport> ViewportWidget;

	/** Is rendering currently enabled? (disabled when reimporting a texture) */
	bool bIsRenderingEnabled;
};
