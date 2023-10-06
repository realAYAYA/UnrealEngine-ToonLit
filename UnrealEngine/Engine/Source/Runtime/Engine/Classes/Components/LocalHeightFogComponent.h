// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Math/Color.h"
#include "LocalHeightFogComponent.generated.h"

class FLocalHeightFogSceneProxy;

UENUM()
enum class ELocalFogMode : uint8
{
	LocalHeightFog = 0,
	LocalSphereFog = 1,
};

UCLASS(ClassGroup = Rendering, collapsecategories, hidecategories = (Object, Mobility, Activation, "Components|Activation"), editinlinenew, meta = (BlueprintSpawnableComponent), MinimalAPI)
class ULocalHeightFogComponent : public USceneComponent
{
	GENERATED_UCLASS_BODY()

	~ULocalHeightFogComponent();

	/** Controls the softness of the transition region when the volume is fading out. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fog Mode")
	ELocalFogMode FogMode = ELocalFogMode::LocalHeightFog;

	/** Global density factor for this fog. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, interp, Category = "Fog Distribution", meta = (UIMin = "0", UIMax = "10.0", SliderExponent = 2.0, ClampMin = 0.0))
	float FogDensity = 5.0f;

	/** Controls how the density decreases as height increases. Smaller values make the visible transition larger. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, interp, Category = "Fog Distribution", meta = (UIMin = "0.001", UIMax = "5000", SliderExponent = 2.0, ClampMin = 0.001))
	float FogHeightFalloff = 1000.0f;

	/** Height offset, relative to the actor Z position. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, interp, Category = "Fog Distribution", meta = (UIMin = "-2.0", UIMax = "2.0"))
	float FogHeightOffset = 0.0f;

	/** Controls how strong the radial attenuation of this fog volume is. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, interp, Category = "Fog Distribution", meta = (UIMin = "0.0", UIMax = "4.0", SliderExponent = 3.0, ClampMin = 0.0))
	float FogRadialAttenuation = 0.0f;

	/** Controls the phase `G` parameter, describing the directionality of the scattering within this fog volume. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, interp, Category = "Fog Shading", meta = (DisplayName = "Scattering Distribution", UIMin = "0.0", UIMax = "0.999", ClampMin = 0.0, ClampMax = 0.999))
	float FogPhaseG = 0.8f;

	/** Controls the albedo of this fog volume. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, interp, Category = "Fog Shading", meta = (ClampMin = 0.0, ClampMax = 1.0))
	FLinearColor FogAlbedo = FLinearColor::White;

	/** Controls the emissive color of this fog volume. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, interp, Category = "Fog Shading", meta = (ClampMin = 0.0, ClampMax = 1.0))
	FLinearColor FogEmissive = FLinearColor::Black;

	/** The priority can be used as a way to override the sorting by distance. A lower value means the volume will be considered further away, i.e. it will draw behind the one with a higher priority value. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sorting", meta = (UIMin = "-127", UIMax = "127", ClampMin = -127, ClampMax = 127))
	int32 FogSortPriority = 0;

public:

	//~ Begin UObject Interface
#if WITH_EDITOR
	virtual bool CanEditChange(const FProperty* InProperty) const override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR
	//~ End UObject Interface

	/** Callback to create the rendering thread mirror. */
	ENGINE_API FLocalHeightFogSceneProxy* CreateSceneProxy();

protected:
	//~ Begin UActorComponent Interface.
	virtual void CreateRenderState_Concurrent(FRegisterComponentContext* Context) override;
	virtual void SendRenderTransform_Concurrent() override;
	virtual void DestroyRenderState_Concurrent() override;
	//~ End UActorComponent Interface.

	void SendRenderTransformCommand();

private:

	FLocalHeightFogSceneProxy* LocalHeightFogSceneProxy;
};

