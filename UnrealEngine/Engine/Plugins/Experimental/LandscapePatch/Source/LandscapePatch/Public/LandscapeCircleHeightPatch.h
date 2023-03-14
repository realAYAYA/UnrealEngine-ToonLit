// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "LandscapePatchComponent.h"

#include "LandscapeCircleHeightPatch.generated.h"

/**
 * The simplest height patch: a circle of flat ground with a falloff past the initial radius across which the
 * alpha decreases linearly. When added to an actor, initializes itself to the bottom of the bounding box.
 */
UCLASS(Blueprintable, BlueprintType, ClassGroup = Landscape, meta = (BlueprintSpawnableComponent))
class LANDSCAPEPATCH_API ULandscapeCircleHeightPatch : public ULandscapePatchComponent
{
	GENERATED_BODY()

public:

	//TODO: For now the height patches have a similar interface to blueprint brushes, but this is likely to
	// change. We are likely to pass a graph builder to the patches instead, along with an area that
	// we want affected.
	virtual void Initialize_Native(const FTransform& InLandscapeTransform, 
		const FIntPoint& InLandscapeSize, 
		const FIntPoint& InLandscapeRenderTargetSize) override;
	virtual UTextureRenderTarget2D* Render_Native(bool InIsHeightmap,
		UTextureRenderTarget2D* InCombinedResult,
		const FName& InWeightmapLayerName) override;

	// UActorComponent
	virtual void OnComponentCreated() override;

protected:

	UPROPERTY(EditAnywhere, Category = Settings)
	float Radius = 500;

	/** Distance across which the alpha will go from 1 down to 0 outside of circle. */
	UPROPERTY(EditAnywhere, Category = Settings)
	float Falloff = 500;

	/** When true, only the vertices in the circle have alpha 1. If false, the radius is expanded slightly so that neighboring 
	  vertices are also included and the whole circle is able to lie flat. */
	UPROPERTY(EditAnywhere, Category = Settings, AdvancedDisplay)
	bool bExclusiveRadius = false;
};