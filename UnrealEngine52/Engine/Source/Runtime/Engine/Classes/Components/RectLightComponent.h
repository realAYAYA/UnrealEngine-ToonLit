// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/EngineTypes.h"
#include "Engine/Scene.h"
#include "Components/LocalLightComponent.h"
#include "RectLightComponent.generated.h"

float ENGINE_API GetRectLightBarnDoorMaxAngle();

class FLightSceneProxy;

/**
 * A light component which emits light from a rectangle.
 */
UCLASS(Blueprintable, ClassGroup=(Lights), hidecategories=(Object, LightShafts), editinlinenew, meta=(BlueprintSpawnableComponent))
class ENGINE_API URectLightComponent : public ULocalLightComponent
{
	GENERATED_UCLASS_BODY()

	/** 
	 * Width of light source rect.
	 * Note that light sources shapes which intersect shadow casting geometry can cause shadowing artifacts.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, interp, Category=Light)
	float SourceWidth;

	/** 
	 * Height of light source rect.
	 * Note that light sources shapes which intersect shadow casting geometry can cause shadowing artifacts.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, interp, Category=Light)
	float SourceHeight;

	/**
	 * Angle of barn door attached to the light source rect.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, interp, Category = Light, meta = (UIMin = "0.0", UIMax = "90.0"))
	float BarnDoorAngle;
	
	/**
	 * Length of barn door attached to the light source rect.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, interp, Category = Light, meta = (UIMin = "0.0"))
	float BarnDoorLength;

	/** Texture mapped to the light source rectangle */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Light)
	TObjectPtr<class UTexture> SourceTexture;

	UFUNCTION(BlueprintCallable, Category = "Rendering|Lighting")
	void SetSourceTexture(UTexture* NewValue);

	UFUNCTION(BlueprintCallable, Category="Rendering|Lighting")
	void SetSourceWidth(float NewValue);

	UFUNCTION(BlueprintCallable, Category="Rendering|Lighting")
	void SetSourceHeight(float NewValue);

	UFUNCTION(BlueprintCallable, Category = "Rendering|Lighting")
	void SetBarnDoorAngle(float NewValue);
	
	UFUNCTION(BlueprintCallable, Category = "Rendering|Lighting")
	void SetBarnDoorLength(float NewValue);

public:

	virtual float ComputeLightBrightness() const override;
#if WITH_EDITOR
	virtual void SetLightBrightness(float InBrightness) override;
#endif

	//~ Begin ULightComponent Interface.
	virtual ELightComponentType GetLightType() const override;
	virtual float GetUniformPenumbraSize() const override;
	virtual FLightSceneProxy* CreateSceneProxy() const override;

	virtual void BeginDestroy() override;
	//~ Begin UObject Interface
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR
	//~ End UObject Interface

private:
	void UpdateRayTracingData();
	friend class FRectLightSceneProxy;
	struct FRectLightRayTracingData* RayTracingData;
};