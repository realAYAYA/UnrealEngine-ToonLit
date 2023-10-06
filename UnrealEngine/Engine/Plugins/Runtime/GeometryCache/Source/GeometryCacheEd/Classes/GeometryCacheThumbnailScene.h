// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ThumbnailHelpers.h"

class AGeometryCacheActor;
class UGeometryCache;

class FGeometryCacheThumbnailScene : public FThumbnailPreviewScene
{
public:
	/** Constructor */
	FGeometryCacheThumbnailScene();

	/** Sets the static mesh to use in the next CreateView() */
	void SetGeometryCache(UGeometryCache* GeometryCache);

protected:
	// FThumbnailPreviewScene implementation
	virtual void GetViewMatrixParameters(const float InFOVDegrees, FVector& OutOrigin, float& OutOrbitPitch, float& OutOrbitYaw, float& OutOrbitZoom) const override;

private:
	/** The static mesh actor used to display all static mesh thumbnails */
	AGeometryCacheActor* PreviewActor;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif
