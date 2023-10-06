// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Curves/CurveLinearColorAtlas.h"
#include "Curves/CurveFloat.h"
#include "ToonRenderingSettings.generated.h"

USTRUCT(BlueprintType)
struct ENGINE_API FRuntimeAtlasTexture
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere,Category=RuntimeFloatCurve)
	TObjectPtr<UCurveLinearColorAtlas> CurveLinearColorAtlas;

	FRuntimeAtlasTexture();

	/** Get the current texture struct */
	UTexture2D* GetAtlasTexture2D() const;
	const UTexture2D* GetAtlasTexture2DConst() const;
};

UCLASS(config=Engine, defaultconfig, meta=(DisplayName="Toon Rendering"))
class ENGINE_API UToonRenderingSettings : public UDeveloperSettings
{
	GENERATED_UCLASS_BODY()

public:
	
	UPROPERTY(config, EditAnywhere, Category = Shading, meta = (
		DisplayName = "PreIntegrated-RampTexture",
		ToolTip = "Control all Things' ramp of the Projects."))
	FRuntimeAtlasTexture RampTexture;

	UPROPERTY(config, EditAnywhere, Category = Outline, meta = (
		DisplayName = "Outline ZOffset Curve",
		ToolTip = ""))
	FRuntimeFloatCurve OutlineZOffsetCurve;

	UPROPERTY(config, EditAnywhere, Category = Outline, meta = (
		DisplayName = "Enable Backface ToonOutline",
		ToolTip = ""))
	bool bToonOutlinePass_Backface = true;

	UPROPERTY(config, EditAnywhere, Category = Outline, meta = (
		DisplayName = "Enable Postprocess ToonOutlinePass",
		ToolTip = ""))
	bool bToonOutlinePass_PostProcess = false;

	void InitToonData() const;
	
public:

	//~ Begin UObject Interface

	virtual void PostInitProperties() override;

#if WITH_EDITOR
	virtual void PreEditChange(FProperty* PropertyAboutToChange) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual bool CanEditChange(const FProperty* InProperty) const override;
#endif

	//~ End UObject Interface
};
