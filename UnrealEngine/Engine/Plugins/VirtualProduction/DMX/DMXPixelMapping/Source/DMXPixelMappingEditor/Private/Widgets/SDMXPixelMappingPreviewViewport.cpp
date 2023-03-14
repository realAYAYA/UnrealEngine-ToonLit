// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/SDMXPixelMappingPreviewViewport.h"

#include "Components/DMXPixelMappingRendererComponent.h"
#include "Toolkits/DMXPixelMappingToolkit.h"
#include "Viewports/DMXPixelMappingSceneViewport.h"
#include "Viewports/DMXPixelMappingPreviewViewportClient.h"

#include "Framework/Application/SlateApplication.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Slate/SceneViewport.h"
#include "Widgets/SViewport.h"
#include "Widgets/Layout/SBox.h"

void SDMXPixelMappingPreviewViewport::Construct(const FArguments& InArgs, const TSharedPtr<FDMXPixelMappingToolkit>& InToolkit)
{
	bIsRenderingEnabled = true;
	ToolkitWeakPtr = InToolkit;

	ChildSlot
		[
			SNew(SBox)
			.Visibility(EVisibility::HitTestInvisible)
			.WidthOverride(this, &SDMXPixelMappingPreviewViewport::GetPreviewAreaWidth)
			.HeightOverride(this, &SDMXPixelMappingPreviewViewport::GetPreviewAreaHeight)
			[
				SAssignNew(ViewportWidget, SViewport)
					.IsEnabled(FSlateApplication::Get().GetNormalExecutionAttribute())
					.EnableGammaCorrection(false)
					.ShowEffectWhenDisabled(false)
			]
		];

	ViewportClient = MakeShared<FDMXPixelMappingPreviewViewportClient>(InToolkit, SharedThis(this));

	Viewport = MakeShared<FDMXPixelMappingSceneViewport>(ViewportClient.Get(), ViewportWidget);

	// The viewport widget needs an interface so it knows what should render
	ViewportWidget->SetViewportInterface(Viewport.ToSharedRef());
}

void SDMXPixelMappingPreviewViewport::EnableRendering()
{
	bIsRenderingEnabled = true;
}

void SDMXPixelMappingPreviewViewport::DisableRendering()
{
	bIsRenderingEnabled = false;
}

void SDMXPixelMappingPreviewViewport::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	if (bIsRenderingEnabled)
	{
		Viewport->Invalidate();
	}
}

FOptionalSize SDMXPixelMappingPreviewViewport::GetPreviewAreaWidth() const
{
	const TSharedPtr<FDMXPixelMappingToolkit> Toolkit = ToolkitWeakPtr.Pin();
	check(Toolkit.IsValid());

	if (UDMXPixelMappingRendererComponent* RendererComponent = Toolkit->GetActiveRendererComponent())
	{
		UTextureRenderTarget2D* OutputTexture = RendererComponent->GetPreviewRenderTarget();
		check(OutputTexture) // Preview Texture should be valid in any case
		
		return OutputTexture->SizeX;
	}

	return 1.f;
}

FOptionalSize SDMXPixelMappingPreviewViewport::GetPreviewAreaHeight() const
{
	const TSharedPtr<FDMXPixelMappingToolkit> Toolkit = ToolkitWeakPtr.Pin();
	check(Toolkit.IsValid());

	if (UDMXPixelMappingRendererComponent* RendererComponent = Toolkit->GetActiveRendererComponent())
	{
		UTextureRenderTarget2D* OutputTexture = RendererComponent->GetPreviewRenderTarget();
		check(OutputTexture) // Preview Texture should be valid in any case
		
		return OutputTexture->SizeY;
	}

	return 1.f;
}

