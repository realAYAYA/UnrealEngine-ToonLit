// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "VCamOutputProviderBase.h"
#include "Engine/TextureRenderTarget2D.h"
#include "CompositingElement.h"

#if WITH_EDITOR
#endif

#include "VCamOutputComposure.generated.h"

UCLASS(meta = (DisplayName = "Composure Output Provider"))
class VCAMCORE_API UVCamOutputComposure : public UVCamOutputProviderBase
{
	GENERATED_BODY()
public:
	
	/** List of Composure stack Compositing Elements to render the requested UMG into */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Output")
	TArray<TSoftObjectPtr<ACompositingElement>> LayerTargets;

	/** TextureRenderTarget2D asset that contains the final output */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Output")
	TObjectPtr<UTextureRenderTarget2D> FinalOutputRenderTarget = nullptr;

	UVCamOutputComposure();

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

protected:

	//~ Begin UVCamOutputProviderBase Interface
	virtual void CreateUMG() override;
	//~ End UVCamOutputProviderBase Interface
};
