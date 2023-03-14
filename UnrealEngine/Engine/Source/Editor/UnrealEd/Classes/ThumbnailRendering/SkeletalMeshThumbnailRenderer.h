// Copyright Epic Games, Inc. All Rights Reserved.

/**
 *
 * This thumbnail renderer displays a given skeletal mesh
 */

#pragma once

#include "CoreMinimal.h"
#include "ThumbnailHelpers.h"
#include "UObject/ObjectMacros.h"
#include "ThumbnailRendering/DefaultSizedThumbnailRenderer.h"
#include "SkeletalMeshThumbnailRenderer.generated.h"

class FCanvas;
class FRenderTarget;

UCLASS(config=Editor)
class UNREALED_API USkeletalMeshThumbnailRenderer : public UDefaultSizedThumbnailRenderer
{
	GENERATED_UCLASS_BODY()


	// Begin UThumbnailRenderer Object
	virtual void Draw(UObject* Object, int32 X, int32 Y, uint32 Width, uint32 Height, FRenderTarget*, FCanvas* Canvas, bool bAdditionalViewFamily) override;
	virtual bool AllowsRealtimeThumbnails(UObject* Object) const override;
	virtual bool CanVisualizeAsset(UObject* Object) override;
	// End UThumbnailRenderer Object

	// UObject implementation
	virtual void BeginDestroy() override;

	virtual void AddAdditionalPreviewSceneContent(UObject* Object, UWorld* PreviewWorld) {}

protected:
	TObjectInstanceThumbnailScene<FSkeletalMeshThumbnailScene, 128> ThumbnailSceneCache;
};

