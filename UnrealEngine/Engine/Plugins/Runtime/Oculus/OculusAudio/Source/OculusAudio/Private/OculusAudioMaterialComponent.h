// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Components/ActorComponent.h"
#include "OVR_Audio.h"
#include "OculusAudioMaterialComponent.generated.h"

UENUM()
enum class EOculusAudioMaterial : uint8
{
	ACOUSTICTILE = 0			UMETA(DisplayName = "AcousticTile"),
	BRICK = 1					UMETA(DisplayName = "Brick"),
	BRICKPAINTED = 2			UMETA(DisplayName = "BrickPainted"),
	CARPET = 3					UMETA(DisplayName = "Carpet"),
	CARPETHEAVY = 4				UMETA(DisplayName = "CarpetHeavy"),
	CARPETHEAVYPADDED = 5		UMETA(DisplayName = "CarpetHeavyPadded"),
	CERAMICTILE = 6				UMETA(DisplayName = "CeramicTile"),
	CONCRETE = 7				UMETA(DisplayName = "Concrete"),
	CONCRETEROUGH = 8			UMETA(DisplayName = "ConcreteRough"),
	CONCRETEBLOCK = 9			UMETA(DisplayName = "ConcreteBlock"),
	CONCRETEBLOCKPAINTED = 10	UMETA(DisplayName = "ConcreteBlockPainted"),
	CURTAIN = 11				UMETA(DisplayName = "Curtain"),
	FOLIAGE = 12				UMETA(DisplayName = "Foliage"),
	GLASS = 13					UMETA(DisplayName = "Glass"),
	GLASSHEAVY = 14				UMETA(DisplayName = "GlassHeavy"),
	GRASS = 15					UMETA(DisplayName = "Grass"),
	GRAVEL = 16					UMETA(DisplayName = "Gravel"),
	GYPSUMBOARD = 17			UMETA(DisplayName = "GypsumBoard"),
	PLASTERONBRICK = 18			UMETA(DisplayName = "PlasterOnBrick"),
	PLASTERONCONCRETEBLOCK = 19 UMETA(DisplayName = "PlasterOnConcreteBlock"),
	SOIL = 20					UMETA(DisplayName = "Soil"),
	SOUNDPROOF = 21				UMETA(DisplayName = "SoundProof"),
	SNOW = 22					UMETA(DisplayName = "Snow"),
	STEEL = 23					UMETA(DisplayName = "Steel"),
	WATER = 24					UMETA(DisplayName = "Water"),
	WOODTHIN = 25				UMETA(DisplayName = "WoodThin"),
	WOODTHICK = 26				UMETA(DisplayName = "WoodThick"),
	WOODFLOOR = 27				UMETA(DisplayName = "WoodFloor"),
	WOODONCONCRETE = 28			UMETA(DisplayName = "WoodOnConcrete"),
	MATERIAL_MAX				UMETA(Hidden),
	NOMATERIAL = 255			UMETA(DisplayName = "NoMaterial") // default
};

/*
 * OculusAudio material components are used to set the acoustic properties of the geometry.
 */
// PAS: TODO check these UCLASS parameters
UCLASS(ClassGroup = (Audio), HideCategories = (Activation, Collision, Cooking), meta = (BlueprintSpawnableComponent))
class UOculusAudioMaterialComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UOculusAudioMaterialComponent();
	void ConstructMaterial(ovrAudioMaterial ovrMaterial);

#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual bool CanEditChange(const FProperty* InProperty) const override;
	void AssignCoefficients(TArray<float> Absorption, TArray<float> Transmission, TArray<float> Scattering);
#endif

private:
	EOculusAudioMaterial GetMaterialPreset() const { return MaterialPreset; }
	bool IsValidMaterialPreset() const { return (MaterialPreset < EOculusAudioMaterial::MATERIAL_MAX); }
	void ResetAcousticMaterialPreset();

	// Choose from a variety of preset physical materials, or choose Custom to specify values manually.
	UPROPERTY(EditAnywhere, Category = Settings)
	EOculusAudioMaterial MaterialPreset;

	// ------------------------------------------		Absorption			-----------------------------------------------

	// How much this material absorbs @ 63Hz
	UPROPERTY(VisibleAnywhere, Category = Settings, meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float Absorption63Hz;

	// How much this material absorbs @ 125Hz
	UPROPERTY(VisibleAnywhere, Category = Settings, meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float Absorption125Hz;

	// How much this material absorbs @ 250Hz
	UPROPERTY(VisibleAnywhere, Category = Settings, meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float Absorption250Hz;

	// How much this material absorbs @ 500Hz
	UPROPERTY(VisibleAnywhere, Category = Settings, meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float Absorption500Hz;

	// How much this material absorbs @ 1000Hz
	UPROPERTY(VisibleAnywhere, Category = Settings, meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float Absorption1000Hz;

	// How much this material absorbs @ 2000Hz
	UPROPERTY(VisibleAnywhere, Category = Settings, meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float Absorption2000Hz;

	// How much this material absorbs @ 4000Hz
	UPROPERTY(VisibleAnywhere, Category = Settings, meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float Absorption4000Hz;

	// How much this material absorbs @ 8000Hz
	UPROPERTY(VisibleAnywhere, Category = Settings, meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float Absorption8000Hz;

	// ------------------------------------------		Transmission		-----------------------------------------------

	// How much this material transmits @ 63Hz
	UPROPERTY(VisibleAnywhere, Category = Settings, meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float Transmission63Hz;

	// How much this material transmits @ 125Hz
	UPROPERTY(VisibleAnywhere, Category = Settings, meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float Transmission125Hz;

	// How much this material transmits @ 250Hz
	UPROPERTY(VisibleAnywhere, Category = Settings, meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float Transmission250Hz;

	// How much this material transmits @ 500Hz
	UPROPERTY(VisibleAnywhere, Category = Settings, meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float Transmission500Hz;

	// How much this material transmits @ 1000Hz
	UPROPERTY(VisibleAnywhere, Category = Settings, meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float Transmission1000Hz;

	// How much this material transmits @ 2000Hz
	UPROPERTY(VisibleAnywhere, Category = Settings, meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float Transmission2000Hz;

	// How much this material transmits @ 4000Hz
	UPROPERTY(VisibleAnywhere, Category = Settings, meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float Transmission4000Hz;

	// How much this material transmits @ 8000Hz
	UPROPERTY(VisibleAnywhere, Category = Settings, meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float Transmission8000Hz;

	// ------------------------------------------		Scattering			-----------------------------------------------

	// How much this material scatters @ 63Hz
	UPROPERTY(VisibleAnywhere, Category = Settings, meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float Scattering63Hz;

	// How much this material scatters @ 125Hz
	UPROPERTY(VisibleAnywhere, Category = Settings, meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float Scattering125Hz;

	// How much this material scatters @ 250Hz
	UPROPERTY(VisibleAnywhere, Category = Settings, meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float Scattering250Hz;

	// How much this material scatters @ 500Hz
	UPROPERTY(VisibleAnywhere, Category = Settings, meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float Scattering500Hz;

	// How much this material scatters @ 1000Hz
	UPROPERTY(VisibleAnywhere, Category = Settings, meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float Scattering1000Hz;

	// How much this material scatters @ 2000Hz
	UPROPERTY(VisibleAnywhere, Category = Settings, meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float Scattering2000Hz;

	// How much this material scatters @ 4000Hz
	UPROPERTY(VisibleAnywhere, Category = Settings, meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float Scattering4000Hz;

	// How much this material scatters @ 8000Hz
	UPROPERTY(VisibleAnywhere, Category = Settings, meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float Scattering8000Hz;
};