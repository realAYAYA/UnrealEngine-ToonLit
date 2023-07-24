// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Layout/Visibility.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
class FSceneViewport;
class SViewport;
class SSimulcamViewport;
class FSimulcamEditorViewportClient;
struct FPointerEvent;

/**
 * Implements the texture editor's view port.
 */
class SSimulcamEditorViewport
	: public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SSimulcamEditorViewport) { }
	SLATE_END_ARGS()
public:

	/**
	 * Constructs the widget.
	 *
	 * @param InArgs The construction arguments.
	 */
	void Construct( const FArguments& InArgs, const TSharedRef<SSimulcamViewport>& InSimulcamViewport, const bool bWithZoom, const bool bWithPan);

	/** Enable viewport rendering */
	void EnableRendering();
	/** Disable viewport rendering */
	void DisableRendering();

	/** Get the current SceneViewport */
	TSharedPtr<FSceneViewport> GetViewport( ) const;

	/** Get The SViewport Widget using this editor */
	TSharedPtr<SViewport> GetViewportWidget( ) const;

	/** Returns the current texture dimensions (in pixels) */
	FVector2D CalculateTextureDimensions() const;

	/** The current zoom level */
	double GetCustomZoomLevel() const;

	/** Directly set a zoom level */
	void SetCustomZoomLevel(double ZoomValue);

	/** Zoom with offset */
	void OffsetZoom(double OffsetValue, bool bSnapToStepSize = true);
	
	/** Increase zooming */
	void ZoomIn();

	/** Decrease zooming */
	void ZoomOut();

	//~ Begin SWidget interface
	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime ) override;
	//~ End SWidget interface

	/** Quick compute and cache the texture size before each frame */
	void CacheEffectiveTextureSize();

	/** Returns the minimal level of Zoom */
	double GetMinZoomLevel() const;

	/** Compute the best position for fitting the texture in the viewport */
	FVector2D GetFitPosition() const;

	/** Triggered whenever the viewport is clicked */
	void OnViewportClicked(const FGeometry& MyGeometry, const FPointerEvent& PointerEvent);
protected:
	/**
	 * Gets the displayed textures resolution as a string.
	 *
	 * @return Texture resolution string.
	 */
	FText GetDisplayedResolution() const;

private:

	// Callback for getting the zoom percentage text.
	FText HandleZoomPercentageText( ) const;

	// Checks if the texture being edited has a valid texture resource
	bool HasValidTextureResource( ) const;
private:
	TWeakPtr<SSimulcamViewport> SimulcamViewportWeakPtr;
	
	// Level viewport client.
	TSharedPtr<FSimulcamEditorViewportClient> ViewportClient;
	// Slate viewport for rendering and IO.
	TSharedPtr<FSceneViewport> Viewport;
	// Viewport widget.
	TSharedPtr<SViewport> ViewportWidget;

	// Is rendering currently enabled? (disabled when reimporting a texture)
	bool bIsRenderingEnabled;

	/** The maximum width/height at which the texture will render in the preview window */
	FVector2D CachedEffectiveTextureSize;

	/** The texture's zoom factor. forced to 'fit' when 0 */
	double Zoom = 0;

public:
	static constexpr double MaxZoom = 16.0;
	static constexpr double ZoomStep = 0.1;
};
