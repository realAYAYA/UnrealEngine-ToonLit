// Copyright Epic Games, Inc. All Rights Reserved.

#include "Viewports/DMXPixelMappingSourceTextureViewportClient.h"
#include "Widgets/SDMXPixelMappingSourceTextureViewport.h"
#include "Components/DMXPixelMappingRendererComponent.h"
#include "Toolkits/DMXPixelMappingToolkit.h"

#include "CanvasItem.h"
#include "CanvasTypes.h"
#include "Slate/SceneViewport.h"

FDMXPixelMappingSourceTextureViewportClient::FDMXPixelMappingSourceTextureViewportClient(const TSharedPtr<FDMXPixelMappingToolkit>& InToolkit, TWeakPtr<SDMXPixelMappingSourceTextureViewport> InViewport)
	: ToolkitWeakPtr(InToolkit)
	, WeakViewport(InViewport)
{
	check(ToolkitWeakPtr.IsValid() && WeakViewport.IsValid());
}

void FDMXPixelMappingSourceTextureViewportClient::Draw(FViewport* InViewport, FCanvas* Canvas)
{
	Canvas->Clear(FColor::Transparent);

	TSharedPtr<SDMXPixelMappingSourceTextureViewport> Viewport = WeakViewport.Pin();
	check(Viewport.IsValid());
	TSharedPtr<FDMXPixelMappingToolkit> Toolkit = ToolkitWeakPtr.Pin();
	check(Toolkit.IsValid());
	const TSharedPtr<FSceneViewport> SceneViewport = Viewport->GetViewport();
	check(SceneViewport.IsValid());

	UTexture* InputTexture = nullptr;
	if (UDMXPixelMappingRendererComponent* RendererComponent = Toolkit->GetActiveRendererComponent())
	{
		InputTexture = RendererComponent->GetRendererInputTexture();
	}

	if (InputTexture && InputTexture->GetResource())
	{
		const uint32 Width = SceneViewport->GetSizeXY().X;
		const uint32 Height = SceneViewport->GetSizeXY().Y;

		FCanvasTileItem TileItem(FVector2D(0, 0), InputTexture->GetResource(), FVector2D(Width, Height), FLinearColor::White);
		TileItem.BlendMode = ESimpleElementBlendMode::SE_BLEND_MAX;
		Canvas->DrawItem(TileItem);
	}
}
