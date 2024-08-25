// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Layout/Visibility.h"
#include "TG_TexturePreviewViewportClient.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "STG_SelectionPreview.h"

class FSceneViewport;
class SScrollBar;
class SViewport;

/**
 * Implements the texture editor's view port.
 */
class STG_TexturePreviewViewport
	: public SCompoundWidget
{
public:
	DECLARE_DELEGATE(FOnMouseHover);

	SLATE_BEGIN_ARGS(STG_TexturePreviewViewport) { }
	SLATE_EVENT(FOnMouseHover, OnMouseHover)
	SLATE_END_ARGS()

	/**
	 */
	void AddReferencedObjects( FReferenceCollector& Collector );

	/**
	 * Constructs the widget.
	 *
	 * @param InArgs The construction arguments.
	 */
	void Construct( const FArguments& InArgs, const TSharedRef<STG_SelectionPreview>& SelectionPreview );
	/**
	 * Modifies the checkerboard texture's data.
	 */
	void ModifyCheckerboardTextureColors( );


	/** Enable viewport rendering */
	void EnableRendering();

	/** Disable viewport rendering */
	void DisableRendering();

	void OnViewportMouseMove();

	TSharedPtr<FSceneViewport> GetViewport( ) const;
	TSharedPtr<SViewport> GetViewportWidget( ) const;
	TSharedPtr<SScrollBar> GetVerticalScrollBar( ) const;
	TSharedPtr<SScrollBar> GetHorizontalScrollBar( ) const;

	void SetViewportClientClearColor(const FLinearColor InColor) const { ViewportClient->SetClearColor(InColor); }
public:

	// SWidget overrides

	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime ) override;
	virtual bool GetMousePosition(FVector2D& MousePosition);
protected:

	/**
	 * Gets the displayed textures resolution as a string.
	 *
	 * @return Texture resolution string.
	 */
	FText GetDisplayedResolution() const;

private:

	// Callback for the horizontal scroll bar.
	void HandleHorizontalScrollBarScrolled( float InScrollOffsetFraction );

	// Callback for getting the visibility of the horizontal scroll bar.
	EVisibility HandleHorizontalScrollBarVisibility( ) const;

	// Callback for the vertical scroll bar.
	void HandleVerticalScrollBarScrolled( float InScrollOffsetFraction );

	// Callback for getting the visibility of the horizontal scroll bar.
	EVisibility HandleVerticalScrollBarVisibility( ) const;

	// Pointer back to the Texture editor tool that owns us.
	TWeakPtr<STG_SelectionPreview> SelectionPreview;
	
	// Level viewport client.
	TSharedPtr<class FTG_TexturePreviewViewportClient> ViewportClient;

	// Slate viewport for rendering and IO.
	TSharedPtr<FSceneViewport> Viewport;

	// Viewport widget.
	TSharedPtr<SViewport> ViewportWidget;

	// Vertical scrollbar.
	TSharedPtr<SScrollBar> TextureViewportVerticalScrollBar;

	// Horizontal scrollbar.
	TSharedPtr<SScrollBar> TextureViewportHorizontalScrollBar;

	// Is rendering currently enabled? (disabled when reimporting a texture)
	bool bIsRenderingEnabled = true;

	FOnMouseHover OnMouseHover;
};
