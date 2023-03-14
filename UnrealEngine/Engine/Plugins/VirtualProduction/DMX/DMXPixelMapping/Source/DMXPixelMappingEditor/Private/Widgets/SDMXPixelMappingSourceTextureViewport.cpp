// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/SDMXPixelMappingSourceTextureViewport.h"
#include "Viewports/DMXPixelMappingSourceTextureViewportClient.h"
#include "DMXPixelMapping.h"
#include "Toolkits/DMXPixelMappingToolkit.h"
#include "Components/DMXPixelMappingRendererComponent.h"
#include "Viewports/DMXPixelMappingSceneViewport.h"

#include "Engine/Texture.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SViewport.h"
#include "Widgets/Layout/SBox.h"

void SDMXPixelMappingSourceTextureViewport::Construct(const FArguments& InArgs, const TSharedPtr<FDMXPixelMappingToolkit>& InToolkit)
{
	bIsRenderingEnabled = true;
	ToolkitWeakPtr = InToolkit;

	TSharedPtr<SHorizontalBox> HorizontalBox;

	ChildSlot
	[
		SNew(SBox)
		.WidthOverride(this, &SDMXPixelMappingSourceTextureViewport::GetPreviewAreaWidth)
		.HeightOverride(this, &SDMXPixelMappingSourceTextureViewport::GetPreviewAreaHeight)
			[
				SAssignNew(ViewportWidget, SViewport)
					.EnableGammaCorrection(false)
					.IsEnabled(FSlateApplication::Get().GetNormalExecutionAttribute())
					.ShowEffectWhenDisabled(false)
					.EnableBlending(true)
			]
	];

	ViewportClient = MakeShared<FDMXPixelMappingSourceTextureViewportClient>(InToolkit, SharedThis(this));

	Viewport = MakeShared<FDMXPixelMappingSceneViewport>(ViewportClient.Get(), ViewportWidget);

	// The viewport widget needs an interface so it knows what should render
	ViewportWidget->SetViewportInterface(Viewport.ToSharedRef());
}

void SDMXPixelMappingSourceTextureViewport::EnableRendering()
{
	bIsRenderingEnabled = true;
}

void SDMXPixelMappingSourceTextureViewport::DisableRendering()
{
	bIsRenderingEnabled = false;
}

void SDMXPixelMappingSourceTextureViewport::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	if (bIsRenderingEnabled)
	{
		Viewport->Invalidate();
	}
}

FOptionalSize SDMXPixelMappingSourceTextureViewport::GetPreviewAreaWidth() const
{
	float Size = 1.f;

	if (const FTextureResource* Resource = GetInputTextureResource())
	{
		Size = Resource->GetSizeX();
	}

	return Size;
}

FOptionalSize SDMXPixelMappingSourceTextureViewport::GetPreviewAreaHeight() const
{
	float Size = 1.f;

	if (const FTextureResource* Resource = GetInputTextureResource())
	{
		Size = Resource->GetSizeY();
	}

	return Size;
}

UTexture* SDMXPixelMappingSourceTextureViewport::GetInputTexture() const
{
	if (TSharedPtr<FDMXPixelMappingToolkit> Toolkit = ToolkitWeakPtr.Pin())
	{
		if (UDMXPixelMappingRendererComponent* RendererComponent = Toolkit->GetActiveRendererComponent())
		{
			return RendererComponent->GetRendererInputTexture();
		}
	}

	return nullptr;
}

const FTextureResource* SDMXPixelMappingSourceTextureViewport::GetInputTextureResource() const
{
	if (const UTexture* Texture = GetInputTexture())
	{
		return Texture->GetResource();
	}

	return nullptr;
}
