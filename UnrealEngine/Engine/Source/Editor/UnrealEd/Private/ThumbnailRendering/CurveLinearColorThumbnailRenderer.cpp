// Copyright Epic Games, Inc. All Rights Reserved.

#include "ThumbnailRendering/CurveLinearColorThumbnailRenderer.h"
#include "Misc/App.h"
#include "ShowFlags.h"
#include "SceneView.h"
#include "ThumbnailHelpers.h"

#include "Curves/CurveLinearColor.h"
#include "CanvasTypes.h"

UCurveLinearColorThumbnailRenderer::UCurveLinearColorThumbnailRenderer(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UCurveLinearColorThumbnailRenderer::GetThumbnailSize(UObject* Object, float Zoom, uint32& OutWidth, uint32& OutHeight) const
{
	UCurveLinearColor* GradientCurve = Cast<UCurveLinearColor>(Object);
	if (GradientCurve)
	{
		OutWidth = 255;
		OutHeight = 255;
	}
	else
	{
		OutWidth = 0;
		OutHeight = 0;
	}
}

void UCurveLinearColorThumbnailRenderer::Draw(UObject* Object, int32 X, int32 Y, uint32 Width, uint32 Height, FRenderTarget* Viewport, FCanvas* Canvas, bool bAdditionalViewFamily)
{
	UCurveLinearColor* GradientCurve = Cast<UCurveLinearColor>(Object);
	if (GradientCurve)
	{
		FVector2D TextureSize = Canvas->GetRenderTarget()->GetSizeXY();
		GradientCurve->DrawThumbnail(Canvas, FVector2D(0.f, 0.f), TextureSize);
	}
}
