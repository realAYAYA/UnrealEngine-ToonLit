// Copyright Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/EngineTypes.h"
#include "Components/LocalLightComponent.h"
#include "PointLightComponent.generated.h"

class FLightSceneProxy;

/**
 * A light component which emits light from a single point equally in all directions.
 */
UCLASS(Blueprintable, ClassGroup=(Lights,Common), hidecategories=(Object, LightShafts), editinlinenew, meta=(BlueprintSpawnableComponent))
class ENGINE_API UPointLightComponent : public ULocalLightComponent
{
	GENERATED_UCLASS_BODY()

	/** 
	 * Whether to use physically based inverse squared distance falloff, where AttenuationRadius is only clamping the light's contribution.  
	 * Disabling inverse squared falloff can be useful when placing fill lights (don't want a super bright spot near the light).
	 * When enabled, the light's Intensity is in units of lumens, where 1700 lumens is a 100W lightbulb.
	 * When disabled, the light's Intensity is a brightness scale.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Light, AdvancedDisplay)
	uint32 bUseInverseSquaredFalloff:1;

	/**
	 * Controls the radial falloff of the light when UseInverseSquaredFalloff is disabled. 
	 * 2 is almost linear and very unrealistic and around 8 it looks reasonable.
	 * With large exponents, the light has contribution to only a small area of its influence radius but still costs the same as low exponents.
	 */
	UPROPERTY(interp, BlueprintReadOnly, Category=Light, AdvancedDisplay, meta=(UIMin = "2.0", UIMax = "16.0"))
	float LightFalloffExponent;

	/** 
	 * Radius of light source shape.
	 * Note that light sources shapes which intersect shadow casting geometry can cause shadowing artifacts.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, interp, Category=Light, meta=(ShouldShowInViewport = true))
	float SourceRadius;

	/**
	* Soft radius of light source shape.
	* Note that light sources shapes which intersect shadow casting geometry can cause shadowing artifacts.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, interp, Category = Light)
	float SoftSourceRadius;

	/** 
	 * Length of light source shape.
	 * Note that light sources shapes which intersect shadow casting geometry can cause shadowing artifacts.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, interp, Category=Light)
	float SourceLength;

	UFUNCTION(BlueprintCallable, Category = "Rendering|Lighting")
	void SetUseInverseSquaredFalloff(bool bNewValue);

	UFUNCTION(BlueprintCallable, Category="Rendering|Lighting")
	void SetLightFalloffExponent(float NewLightFalloffExponent);

	UFUNCTION(BlueprintCallable, Category = "Rendering|Lighting")
	void SetInverseExposureBlend(float NewInverseExposureBlend);

	UFUNCTION(BlueprintCallable, Category="Rendering|Lighting")
	void SetSourceRadius(float bNewValue);

	UFUNCTION(BlueprintCallable, Category = "Rendering|Lighting")
	void SetSoftSourceRadius(float bNewValue);

	UFUNCTION(BlueprintCallable, Category="Rendering|Lighting")
	void SetSourceLength(float NewValue);

public:

	virtual float ComputeLightBrightness() const override;
#if WITH_EDITOR
	virtual void SetLightBrightness(float InBrightness) override;
#endif

	//~ Begin ULightComponent Interface.
	virtual ELightComponentType GetLightType() const override;
	virtual float GetUniformPenumbraSize() const override;
	virtual FLightSceneProxy* CreateSceneProxy() const override;

	//~ Begin UObject Interface
	virtual void Serialize(FArchive& Ar) override;
#if WITH_EDITOR
	virtual bool CanEditChange(const FProperty* InProperty) const override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR
	//~ End UObject Interface

	/** 
	 * This is called when property is modified by InterpPropertyTracks
	 *
	 * @param PropertyThatChanged	Property that changed
	 */
	virtual void PostInterpChange(FProperty* PropertyThatChanged) override;
};



