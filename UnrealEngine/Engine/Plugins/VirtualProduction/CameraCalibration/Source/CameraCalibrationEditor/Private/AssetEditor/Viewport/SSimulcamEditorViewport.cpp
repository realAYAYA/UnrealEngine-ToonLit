// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetEditor/Viewport/SSimulcamEditorViewport.h"

#include "AssetEditor/SSimulcamViewport.h"
#include "AssetEditor/Viewport/SimulcamEditorViewportClient.h"
#include "Engine/Texture.h"
#include "Engine/TextureRenderTargetVolume.h"
#include "Engine/VolumeTexture.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Slate/SceneViewport.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/SToolTip.h"
#include "Widgets/SViewport.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "STextureEditorViewport"

void SSimulcamEditorViewport::Construct(const FArguments& InArgs, const TSharedRef<SSimulcamViewport>& InSimulcamViewport, const bool bWithZoom, const bool bWithPan)
{
	bIsRenderingEnabled = true;
	SimulcamViewportWeakPtr = InSimulcamViewport;

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.FillHeight(1)
				[
					SNew(SOverlay)
					// viewport canvas
					+ SOverlay::Slot()
					[
						SAssignNew(ViewportWidget, SViewport)
						.EnableGammaCorrection(false)
						.IsEnabled(FSlateApplication::Get().GetNormalExecutionAttribute())
						.ShowEffectWhenDisabled(false)
						.EnableBlending(true)
						.ToolTip(SNew(SToolTip).Text(this, &SSimulcamEditorViewport::GetDisplayedResolution))
					]

					// tool bar
					+ SOverlay::Slot()
					.Padding(2.0f)
					.VAlign(VAlign_Top)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.FillWidth(1.0f)
						.Padding(4.0f, 0.0f)
						.HAlign(HAlign_Right)
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Text(this, &SSimulcamEditorViewport::HandleZoomPercentageText)
							.Visibility(bWithZoom ? EVisibility::Visible : EVisibility::Hidden)
						]
					]
				]
			]
		]

	];

	ViewportClient = MakeShared<FSimulcamEditorViewportClient>(InSimulcamViewport, SharedThis(this), bWithZoom, bWithPan);
	Viewport = MakeShared<FSceneViewport>(ViewportClient.Get(), ViewportWidget);
	// The viewport widget needs an interface so it knows what should render
	ViewportWidget->SetViewportInterface(Viewport.ToSharedRef());

	// required for having the first 'zoom to fit' behaviour
	CacheEffectiveTextureSize();

	Viewport->ViewportResizedEvent.AddSP(ViewportClient.ToSharedRef(), &FSimulcamEditorViewportClient::OnViewportResized);
}

void SSimulcamEditorViewport::EnableRendering()
{
	bIsRenderingEnabled = true;
}

void SSimulcamEditorViewport::DisableRendering()
{
	bIsRenderingEnabled = false;
}

TSharedPtr<FSceneViewport> SSimulcamEditorViewport::GetViewport() const
{
	return Viewport;
}

TSharedPtr<SViewport> SSimulcamEditorViewport::GetViewportWidget() const
{
	return ViewportWidget;
}

void SSimulcamEditorViewport::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	if (bIsRenderingEnabled)
	{
		Viewport->Invalidate();
	}
}

FText SSimulcamEditorViewport::GetDisplayedResolution() const
{
	return ViewportClient->GetDisplayedResolution();
}

bool SSimulcamEditorViewport::HasValidTextureResource() const
{
	return (SimulcamViewportWeakPtr.IsValid() && SimulcamViewportWeakPtr.Pin()->HasValidTextureResource());
}

FText SSimulcamEditorViewport::HandleZoomPercentageText() const
{
	if (SimulcamViewportWeakPtr.IsValid() && HasValidTextureResource())
	{
		FText FormattedText = FText::FromString(TEXT("{0}: {1}"));
		double DisplayedZoomLevel = GetCustomZoomLevel();
		return FText::Format(FormattedText, FText::FromName(SimulcamViewportWeakPtr.Pin()->GetTexture()->GetFName()), FText::AsPercent(DisplayedZoomLevel));
	}
	else
	{
		return LOCTEXT("InvalidTexture", "Invalid Texture");
	}
}

void SSimulcamEditorViewport::CacheEffectiveTextureSize()
{
	if (!SimulcamViewportWeakPtr.IsValid())
	{
		return;
	}

	UTexture* Texture = SimulcamViewportWeakPtr.Pin()->GetTexture();
	if (!Texture)
	{
		return;
	}

	// If the cached texture dimensions do not match the current texture dimensions (because the texture was resized), update the cached value and zoom all the way out
	if (CachedEffectiveTextureSize.X != Texture->GetSurfaceWidth() || CachedEffectiveTextureSize.Y != Texture->GetSurfaceHeight())
	{
		CachedEffectiveTextureSize = FVector2D(Texture->GetSurfaceWidth(), Texture->GetSurfaceHeight());
		ViewportClient->OnTextureResized();
	}
}

FVector2D SSimulcamEditorViewport::CalculateTextureDimensions() const
{
	if (!SimulcamViewportWeakPtr.IsValid())
	{
		return FVector2D::ZeroVector;
	}

	UTexture* Texture = SimulcamViewportWeakPtr.Pin()->GetTexture();
	if (!Texture)
	{
		return FVector2D::ZeroVector;
	}

	FVector2D Size = FVector2D(CachedEffectiveTextureSize.X * Zoom, CachedEffectiveTextureSize.Y * Zoom);

	/** catch if the Width and Height are still zero for some reason */
	if ((Size.X == 0) || (Size.Y == 0))
	{
		Size = FVector2D::ZeroVector;
	}

	return Size;
}

double SSimulcamEditorViewport::GetCustomZoomLevel() const
{
	return Zoom;
}

double SSimulcamEditorViewport::GetMinZoomLevel() const
{
	FIntPoint ViewportSize = GetViewport()->GetSizeXY();
	return FMath::Min(ViewportSize.X / CachedEffectiveTextureSize.X, ViewportSize.Y / CachedEffectiveTextureSize.Y);
}

FVector2D SSimulcamEditorViewport::GetFitPosition() const
{
	FIntPoint ViewportSize = GetViewport()->GetSizeXY();
	double MinZoom = GetMinZoomLevel();
	return FVector2D((ViewportSize.X - (CachedEffectiveTextureSize.X * MinZoom)) / 2, (ViewportSize.Y - (CachedEffectiveTextureSize.Y * MinZoom)) / 2);
}

void SSimulcamEditorViewport::SetCustomZoomLevel(double ZoomValue)
{
	Zoom = FMath::Clamp(ZoomValue, GetMinZoomLevel(), MaxZoom);
}

void SSimulcamEditorViewport::OffsetZoom(double OffsetValue, bool bSnapToStepSize)
{
	/** Offset from our current "visual" zoom level so that you can
		smoothly transition from Fit/Fill mode into a custom zoom level */
	const double CurrentZoom = GetCustomZoomLevel();
	if (bSnapToStepSize)
	{
		/** Snap to the zoom step when offsetting to avoid zooming all the way to the min(0.01)
			then back up (+0.1) causing your zoom level to be off by 0.01 (eg. 11%)
			If we were in a fit view mode then our current zoom level could also be off the grid */
		const double FinalZoom = FMath::GridSnap(CurrentZoom + OffsetValue, ZoomStep);
		SetCustomZoomLevel(FinalZoom);
	}
	else
	{
		SetCustomZoomLevel(CurrentZoom + OffsetValue);
	}
}

void SSimulcamEditorViewport::ZoomIn()
{
	OffsetZoom(ZoomStep);
}

void SSimulcamEditorViewport::ZoomOut()
{
	OffsetZoom(-ZoomStep);
}

void SSimulcamEditorViewport::OnViewportClicked(const FGeometry& MyGeometry, const FPointerEvent& PointerEvent)
{
	if (!SimulcamViewportWeakPtr.IsValid())
	{
		return;
	}

	SimulcamViewportWeakPtr.Pin()->OnViewportClicked(MyGeometry, PointerEvent);
}

#undef LOCTEXT_NAMESPACE
