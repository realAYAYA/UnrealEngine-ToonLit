// Copyright Epic Games, Inc. All Rights Reserved.

#include "STG_TexturePreviewViewport.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Layout/SScrollBar.h"
#include "Widgets/SToolTip.h"
#include "Widgets/SViewport.h"
#include "Slate/SceneViewport.h"
#include "Widgets/Input/SNumericEntryBox.h"

#define LOCTEXT_NAMESPACE "STG_TexturePreviewViewport"


/* STG_TexturePreviewViewport interface
 *****************************************************************************/

void STG_TexturePreviewViewport::AddReferencedObjects( FReferenceCollector& Collector )
{
	ViewportClient->AddReferencedObjects(Collector);
}

void STG_TexturePreviewViewport::Construct( const FArguments& InArgs, const TSharedRef<STG_SelectionPreview>& InSelectionPreview)
{
	bIsRenderingEnabled = true;
	SelectionPreview = InSelectionPreview;
	OnMouseHover = InArgs._OnMouseHover;

	this->ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			SNew(SOverlay)
			// viewport canvas
			+ SOverlay::Slot()
			.Padding(5.0f, 0.0f)
			[
				SAssignNew(ViewportWidget, SViewport)
				.EnableGammaCorrection(false)
				.IsEnabled(FSlateApplication::Get().GetNormalExecutionAttribute())
				.ShowEffectWhenDisabled(false)
				.EnableBlending(true)
			]
			+ SOverlay::Slot()
			.HAlign(HAlign_Right)
			[
				// vertical scroll bar
				SAssignNew(TextureViewportVerticalScrollBar, SScrollBar)
				.Visibility(this, &STG_TexturePreviewViewport::HandleVerticalScrollBarVisibility)
				.OnUserScrolled(this, &STG_TexturePreviewViewport::HandleVerticalScrollBarScrolled)
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			// horizontal scrollbar
			SAssignNew(TextureViewportHorizontalScrollBar, SScrollBar)
				.Orientation( Orient_Horizontal )
				.Visibility(this, &STG_TexturePreviewViewport::HandleHorizontalScrollBarVisibility)
				.OnUserScrolled(this, &STG_TexturePreviewViewport::HandleHorizontalScrollBarScrolled)
		]

	];
	
	ViewportClient = MakeShareable(new FTG_TexturePreviewViewportClient(SelectionPreview, SharedThis(this)));

	Viewport = MakeShareable(new FSceneViewport(ViewportClient.Get(), ViewportWidget));

	// The viewport widget needs an interface so it knows what should render
	ViewportWidget->SetViewportInterface( Viewport.ToSharedRef() );
}

void STG_TexturePreviewViewport::ModifyCheckerboardTextureColors( )
{
	if (ViewportClient.IsValid())
	{
		ViewportClient->ModifyCheckerboardTextureColors();
	}
}

void STG_TexturePreviewViewport::EnableRendering()
{
	bIsRenderingEnabled = true;
}

void STG_TexturePreviewViewport::DisableRendering()
{
	bIsRenderingEnabled = false;
}

TSharedPtr<FSceneViewport> STG_TexturePreviewViewport::GetViewport( ) const
{
	return Viewport;
}

TSharedPtr<SViewport> STG_TexturePreviewViewport::GetViewportWidget( ) const
{
	return ViewportWidget;
}

TSharedPtr<SScrollBar> STG_TexturePreviewViewport::GetVerticalScrollBar( ) const
{
	return TextureViewportVerticalScrollBar;
}

TSharedPtr<SScrollBar> STG_TexturePreviewViewport::GetHorizontalScrollBar( ) const
{
	return TextureViewportHorizontalScrollBar;
}


/* SWidget overrides
 *****************************************************************************/

void STG_TexturePreviewViewport::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	if (bIsRenderingEnabled)
	{
		Viewport->Invalidate();
	}
}


/* STG_TexturePreviewViewport implementation
 *****************************************************************************/

FText STG_TexturePreviewViewport::GetDisplayedResolution( ) const
{
	return ViewportClient->GetDisplayedResolution();
}


/* STG_TexturePreviewViewport event handlers
 *****************************************************************************/

void STG_TexturePreviewViewport::HandleHorizontalScrollBarScrolled( float InScrollOffsetFraction )
{
	const float Ratio = ViewportClient->GetViewportHorizontalScrollBarRatio();
	const float MaxOffset = (Ratio < 1.0f) ? 1.0f - Ratio : 0.0f;
	InScrollOffsetFraction = FMath::Clamp(InScrollOffsetFraction, 0.0f, MaxOffset);

	TextureViewportHorizontalScrollBar->SetState(InScrollOffsetFraction, Ratio);
}

EVisibility STG_TexturePreviewViewport::HandleHorizontalScrollBarVisibility( ) const
{
	if (ViewportClient->GetViewportHorizontalScrollBarRatio() < 1.0f)
	{
		return EVisibility::Visible;
	}
	
	return EVisibility::Collapsed;
}

void STG_TexturePreviewViewport::HandleVerticalScrollBarScrolled( float InScrollOffsetFraction )
{
	const float Ratio = ViewportClient->GetViewportVerticalScrollBarRatio();
	const float MaxOffset = (Ratio < 1.0f) ? 1.0f - Ratio : 0.0f;
	InScrollOffsetFraction = FMath::Clamp(InScrollOffsetFraction, 0.0f, MaxOffset);

	TextureViewportVerticalScrollBar->SetState(InScrollOffsetFraction, Ratio);
}

EVisibility STG_TexturePreviewViewport::HandleVerticalScrollBarVisibility( ) const
{
	if (ViewportClient->GetViewportVerticalScrollBarRatio() < 1.0f)
	{
		return EVisibility::Visible;
	}
	
	return EVisibility::Collapsed;
}

void STG_TexturePreviewViewport::OnViewportMouseMove()
{
	OnMouseHover.ExecuteIfBound();
}

bool STG_TexturePreviewViewport::GetMousePosition(FVector2D& MousePosition)
{
	bool bGotMousePosition = false;

	if (Viewport && FSlateApplication::Get().IsMouseAttached())
	{
		FIntPoint MousePos;
		Viewport->GetMousePos(MousePos);
		if (MousePos.X >= 0 && MousePos.Y >= 0)
		{
			MousePosition = FVector2D(MousePos);
			bGotMousePosition = true;
		}
	}
	return bGotMousePosition;
}

#undef LOCTEXT_NAMESPACE
