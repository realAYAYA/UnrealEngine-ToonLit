// Copyright Epic Games, Inc. All Rights Reserved.

/**
 *
 * This thumbnail renderer displays a given Customizable Object
 */

#pragma once
#include "HAL/Platform.h"
#include "ThumbnailRendering/DefaultSizedThumbnailRenderer.h"
#include "UObject/ObjectPtr.h"
#include "UObject/UObjectGlobals.h"

#include "CustomizableObjectThumbnailRenderer.generated.h"

class FCanvas;
class FRenderTarget;
class UObject;


UCLASS(config=Editor)
class CUSTOMIZABLEOBJECTEDITOR_API UCustomizableObjectThumbnailRenderer : public UDefaultSizedThumbnailRenderer
{
public:
	GENERATED_BODY()

	UCustomizableObjectThumbnailRenderer();

	// Begin UThumbnailRenderer Object
	virtual void Draw(UObject* Object, int32 X, int32 Y, uint32 Width, uint32 Height, FRenderTarget*, FCanvas* Canvas, bool bAdditionalViewFamily) override;
	// End UThumbnailRenderer Object

	// UObject implementation
	virtual void BeginDestroy() override;

private:
	class FSkeletalMeshThumbnailScene* ThumbnailScene;

	UPROPERTY()
	TObjectPtr<class UCustomizableObjectInstance> CustomizableObjectInstance;

	UPROPERTY()
	TObjectPtr<class UTexture2D> NoImage;
};

