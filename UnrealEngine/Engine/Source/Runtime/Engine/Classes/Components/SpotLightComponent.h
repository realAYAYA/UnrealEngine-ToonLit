// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Components/PointLightComponent.h"
#include "SpotLightComponent.generated.h"

class FLightSceneProxy;

/**
 * A spot light component emits a directional cone shaped light (Eg a Torch).
 */
UCLASS(Blueprintable, ClassGroup=Lights, hidecategories=Object, editinlinenew, meta=(BlueprintSpawnableComponent))
class ENGINE_API USpotLightComponent : public UPointLightComponent
{
	GENERATED_UCLASS_BODY()

	/** Degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Light, meta=(UIMin = "1.0", UIMax = "80.0", ShouldShowInViewport = true))
	float InnerConeAngle;

	/** Degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Light, meta=(UIMin = "1.0", UIMax = "80.0", ShouldShowInViewport = true))
	float OuterConeAngle;

	UFUNCTION(BlueprintCallable, Category="Rendering|Lighting")
	void SetInnerConeAngle(float NewInnerConeAngle);

	UFUNCTION(BlueprintCallable, Category="Rendering|Lighting")
	void SetOuterConeAngle(float NewOuterConeAngle);

	// ULightComponent interface.
	virtual FSphere GetBoundingSphere() const override;
	virtual bool AffectsBounds(const FBoxSphereBounds& InBounds) const override;
	virtual ELightComponentType GetLightType() const override;
	virtual FLightSceneProxy* CreateSceneProxy() const override;

	virtual float ComputeLightBrightness() const override;
#if WITH_EDITOR
	virtual void SetLightBrightness(float InBrightness) override;
	virtual FBox GetStreamingBounds() const override;
#endif

	float GetHalfConeAngle() const;
	float GetCosHalfConeAngle() const;

#if WITH_EDITOR
	virtual void PostEditChangeProperty( struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};



