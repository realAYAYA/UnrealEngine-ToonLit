// Copyright Epic Games, Inc. All Rights Reserved.

#include "Viewports/DMXPixelMappingPreviewViewportClient.h"

#include "Toolkits/DMXPixelMappingToolkit.h"
#include "Components/DMXPixelMappingRendererComponent.h"
#include "IDMXPixelMappingRenderer.h"

#include "CanvasTypes.h"
#include "Engine/TextureRenderTarget2D.h"
#include "UnrealClient.h"

FDMXPixelMappingPreviewViewportClient::FDMXPixelMappingPreviewViewportClient(const TSharedPtr<FDMXPixelMappingToolkit>& InToolkit, TWeakPtr<SDMXPixelMappingPreviewViewport> InViewport)
	: ToolkitWeakPtr(InToolkit)
	, WeakViewport(InViewport)
{
	check(InToolkit.IsValid() && WeakViewport.IsValid())
}

void FDMXPixelMappingPreviewViewportClient::Draw(FViewport* InViewport, FCanvas* Canvas)
{
	Canvas->Clear(FColor::Transparent);

	TSharedPtr<FDMXPixelMappingToolkit> Toolkit = ToolkitWeakPtr.Pin();
	check(Toolkit.IsValid());

	UDMXPixelMappingRendererComponent* RendererComponent = Toolkit->GetActiveRendererComponent();
	if (RendererComponent)
	{
		for (UDMXPixelMappingOutputComponent* OutputComponent : Toolkit->GetActiveOutputComponents())
		{
			UTextureRenderTarget2D* InputTexture = RendererComponent->GetPreviewRenderTarget();
			const TSharedPtr<IDMXPixelMappingRenderer>& Renderer = RendererComponent->GetRenderer();

			if (Renderer.IsValid() && InputTexture != nullptr)
			{
				Renderer->RenderTextureToRectangle(InputTexture->GetResource(), InViewport->GetRenderTargetTexture(), InViewport->GetSizeXY(), InputTexture->SRGB);
			}
		}
	}
}
