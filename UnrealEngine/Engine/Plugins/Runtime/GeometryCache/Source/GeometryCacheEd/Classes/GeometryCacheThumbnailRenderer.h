// Copyright Epic Games, Inc. All Rights Reserved.

/**
*
* This thumbnail renderer displays a given GeometryCache
*/

#pragma once

#include "ThumbnailRendering/DefaultSizedThumbnailRenderer.h"
#include "GeometryCacheThumbnailRenderer.generated.h"

enum class EThumbnailRenderFrequency : uint8;

class FCanvas;
class FGeometryCacheThumbnailScene;
class FRenderTarget;

UCLASS(config = Editor, MinimalAPI)
class UGeometryCacheThumbnailRenderer : public UDefaultSizedThumbnailRenderer
{
	GENERATED_UCLASS_BODY()

	// Begin UThumbnailRenderer Object
	virtual void Draw(UObject* Object, int32 X, int32 Y, uint32 Width, uint32 Height, FRenderTarget* RenderTarget, FCanvas* Canvas, bool bAdditionalViewFamily) override;
	// End UThumbnailRenderer Object

	// Begin UObject implementation
	virtual void BeginDestroy() override;
	// End UObject implementation
private:
	FGeometryCacheThumbnailScene* ThumbnailScene;
public:
	virtual EThumbnailRenderFrequency GetThumbnailRenderFrequency(UObject* Object) const override;

};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif
