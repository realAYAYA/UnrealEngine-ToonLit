// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/DeveloperSettings.h"
#include "InputCoreTypes.h"
#include "VCamInputSettings.generated.h"

USTRUCT(BlueprintType)
struct VCAMCORE_API FVCamInputProfile
{
	GENERATED_BODY()

	/** Remaps inputs to new keys. This remaps the input action mappings found in the modifiers' InputMappingContext. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Input")
	TMap<FName, FKey> MappableKeyOverrides;

	bool operator==(const FVCamInputProfile& OtherProfile) const;
};

/**
 * These settings store input profiles.
 *
 * When a modifier is activated, it registers its UVCamModifier::InputMappingContext (IMC). The mapping context maps enhanced input actions to default keys.
 * An input profile rebinds the default keys to new values. The name of the profile key is the name that was used for the binding in the IMC.
 * 
 * You can switch between configured profiles by calling UVCamComponent::SetInputProfileFromName or editing UVCamComponent::InputProfile in the details panel.
 */
UCLASS(Config=Game, DefaultConfig)
class VCAMCORE_API UVCamInputSettings : public UDeveloperSettings
{
	GENERATED_BODY()
public:

	/** The profile that VCam components should default to. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadWrite, BlueprintSetter=SetDefaultInputProfile, Category="Input", meta=(GetOptions=GetInputProfileNames))
	FName DefaultInputProfile;

	/** A bunch of profiles that components can switch between */
	UPROPERTY(EditAnywhere, Config, BlueprintReadWrite, BlueprintSetter=SetInputProfiles, Category="Input")
	TMap<FName, FVCamInputProfile> InputProfiles;

	/** Gets the settings object. */
	UFUNCTION(BlueprintCallable, Category="VCam")
	static UVCamInputSettings* GetVCamInputSettings();

	/** Sets DefaultInputProfile and saves it out to the config.  */
	UFUNCTION(BlueprintCallable, Category="VCam Input")
	void SetDefaultInputProfile(const FName NewDefaultInputProfile);
	/** Updates InputProfiles and save it out to the config. */
	UFUNCTION(BlueprintCallable, Category="VCam Input")
	void SetInputProfiles(const TMap<FName, FVCamInputProfile>& NewInputProfiles);

	/** @return All configured profiles */
	UFUNCTION(BlueprintCallable, Category="VCam Input")
	TArray<FName> GetInputProfileNames() const;

	//~ Begin UObject Interface
	virtual void PostInitProperties() override;
	//~ End UObject Interface
};