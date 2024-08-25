// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once


#include "ThumbnailRendering/DefaultSizedThumbnailRenderer.h"
#include "WidgetBlueprintThumbnailRenderer.generated.h"

class FCanvas;
class FRenderTarget;
class UTextureRenderTarget2D;
class FWidgetBlueprintThumbnailPool;

UCLASS()
class UMGEDITOR_API UWidgetBlueprintThumbnailRenderer : public UDefaultSizedThumbnailRenderer
{
	GENERATED_BODY()

	UWidgetBlueprintThumbnailRenderer();
	virtual ~UWidgetBlueprintThumbnailRenderer();

	//~ Begin UThumbnailRenderer Object
	virtual bool CanVisualizeAsset(UObject* Object) override;
	virtual void Draw(UObject* Object, int32 X, int32 Y, uint32 Width, uint32 Height, FRenderTarget* RenderTarget, FCanvas* Canvas, bool bAdditionalViewFamily) override;
	virtual EThumbnailRenderFrequency GetThumbnailRenderFrequency(UObject* Object) const override;
	//~ End UThumbnailRenderer Object

private:
	void OnBlueprintUnloaded(UBlueprint* Blueprint);

private:
	struct FWidgetBlueprintThumbnailPoolDeleter
	{
		void operator()(FWidgetBlueprintThumbnailPool* Pointer);
	};

	TUniquePtr<FWidgetBlueprintThumbnailPool, FWidgetBlueprintThumbnailPoolDeleter> ThumbnailPool;
};
