// Copyright Epic Games, Inc. All Rights Reserved.

/**
 *
 * This thumbnail renderer generates a render of a waveform
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "ThumbnailRendering/DefaultSizedThumbnailRenderer.h"
#include "SoundWaveThumbnailRenderer.generated.h"

class FCanvas;
class FRenderTarget;

UCLASS()
class USoundWaveThumbnailRenderer : public UDefaultSizedThumbnailRenderer
{
	GENERATED_UCLASS_BODY()

	// Begin UThumbnailRenderer Object
	virtual bool CanVisualizeAsset(UObject* Object);
	virtual void Draw(UObject* Object, int32 X, int32 Y, uint32 Width, uint32 Height, FRenderTarget*, FCanvas* Canvas, bool bAdditionalViewFamily) override;
	virtual bool AllowsRealtimeThumbnails(UObject* Object) const override;
	// End UThumbnailRenderer Object
};

