// Copyright Epic Games, Inc. All Rights Reserved.

#include "PreprocessRenderInputUserWidgetProxy.h"

#include "Blueprint/UserWidget.h"
#include "DMXStats.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Slate/WidgetRenderer.h"


namespace UE::DMXPixelMapping::Rendering::Preprocess::Private
{
	DECLARE_CYCLE_STAT(TEXT("PixelMapping RenderInputUserWidget"), STAT_PreprocessRenderInputUserWidget, STATGROUP_DMX);

	FPreprocessRenderInputUserWidgetProxy::FPreprocessRenderInputUserWidgetProxy(UUserWidget* InUserWidget, const FVector2D& InInputSize)
		: WeakUserWidget(InUserWidget)
	{
		IntermediateRenderTarget = NewObject<UTextureRenderTarget2D>();
		IntermediateRenderTarget->ClearColor = FLinearColor::Black;
		IntermediateRenderTarget->InitAutoFormat(InInputSize.X, InInputSize.Y);
		IntermediateRenderTarget->UpdateResourceImmediate();

		const bool bUseGammaCorrection = false;
		UMGRenderer = MakeShared<FWidgetRenderer>(bUseGammaCorrection);
	}

	void FPreprocessRenderInputUserWidgetProxy::Render()
	{
		SCOPE_CYCLE_COUNTER(STAT_PreprocessRenderInputUserWidget);

		if (UUserWidget* UserWidget = WeakUserWidget.Get())
		{
			check(IntermediateRenderTarget);
			FVector2D TextureSize = FVector2D(IntermediateRenderTarget->SizeX, IntermediateRenderTarget->SizeY);

			static const float DeltaTime = 0.f;
			UMGRenderer->DrawWidget(IntermediateRenderTarget, UserWidget->TakeWidget(), TextureSize, DeltaTime);
		}
	}

	UTexture* FPreprocessRenderInputUserWidgetProxy::GetRenderedTexture() const
	{
		return IntermediateRenderTarget;
	}

	FVector2D FPreprocessRenderInputUserWidgetProxy::GetSize2D() const
	{
		check(IntermediateRenderTarget);
		return FVector2D(IntermediateRenderTarget->GetSurfaceWidth(), IntermediateRenderTarget->GetSurfaceHeight());
	}

	void FPreprocessRenderInputUserWidgetProxy::AddReferencedObjects(FReferenceCollector& Collector)
	{
		Collector.AddReferencedObject(IntermediateRenderTarget);
	}
}
