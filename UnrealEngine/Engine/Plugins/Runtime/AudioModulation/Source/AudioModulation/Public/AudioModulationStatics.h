// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "SoundControlBus.h"
#include "SoundControlBusMix.h"
#include "Templates/SubclassOf.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"

#include "AudioModulationStatics.generated.h"

// Forward Declarations
namespace AudioModulation
{
	class FAudioModulationManager;
	class FAudioModulationSystem;
} // namespace AudioModulation


UCLASS()
class AUDIOMODULATION_API UAudioModulationStatics : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()

public:
	/**
	 * Returns world associated with provided context object
	 */
	static UWorld* GetAudioWorld(const UObject* WorldContextObject);

	/**
	 * Returns modulation implementation associated with the provided world
	 */
	static AudioModulation::FAudioModulationManager* GetModulation(UWorld* World);

	/** Manually activates a modulation bus. If called, deactivation will only occur
	 * if bus is manually deactivated or destroyed (i.e. will not deactivate
	 * when all references become inactive).
	 */
	UFUNCTION(BlueprintCallable, Category = "Audio", DisplayName = "Activate Control Bus", meta = (
		WorldContext = "WorldContextObject", 
		Keywords = "modulation modulator")
	)
	static void ActivateBus(const UObject* WorldContextObject, USoundControlBus* Bus);

	/** Manually activates a bus modulator mix. If called, deactivation will only occur
	 * if mix is manually deactivated and not referenced or destroyed (i.e. will not deactivate
	 * when all references become inactive).
	 * @param BusMix - Mix to activate
	 */
	UFUNCTION(BlueprintCallable, Category = "Audio", DisplayName = "Activate Control Bus Mix", meta = (
		WorldContext = "WorldContextObject", 
		Keywords = "modulation modulator")
	)
	static void ActivateBusMix(const UObject* WorldContextObject, USoundControlBusMix* Mix);

	/** Manually activates a modulation generator. If called, deactivation will only occur
	 * if generator is manually deactivated and not referenced or destroyed (i.e. will not deactivate
	 * when all references become inactive).
	 * @param Modulator - Modulator to activate
	 */
	UFUNCTION(BlueprintCallable, Category = "Audio", DisplayName = "Activate Modulation Generator", meta = (
		WorldContext = "WorldContextObject", 
		Keywords = "modulator lfo envelope follower")
	)
	static void ActivateGenerator(const UObject* WorldContextObject, USoundModulationGenerator* Generator);

	/** Creates a modulation bus with the provided default value.
	 * @param Name - Name of bus
	 * @param Parameter - Default value for created bus
	 * @param Activate - Whether or not to activate bus on creation. If true, deactivation will only occur
	 * if returned bus is manually deactivated and not referenced or destroyed (i.e. will not deactivate
	 * when all references become inactive).
	 * @return Capture this in a Blueprint variable to prevent it from being automatically garbage collected. 
	 */
	UFUNCTION(BlueprintCallable, Category = "Audio", DisplayName = "Create Control Bus", meta = (
		AdvancedDisplay = "3",
		WorldContext = "WorldContextObject",
		Keywords = "make modulation LPF modulator")
	)
	static UPARAM(DisplayName = "Bus") USoundControlBus* CreateBus(UObject* WorldContextObject, FName Name, USoundModulationParameter* Parameter, bool Activate = true);

	/** Creates a stage used to mix a control bus.
	 * @param Bus - Bus stage is in charge of applying mix value to.
	 * @param Value - Value for added bus stage to target when mix is active.
	 * @param AttackTime - Time in seconds for stage to mix in.
	 * @param ReleaseTime - Time in seconds for stage to mix out.
	 */
	UFUNCTION(BlueprintCallable, Category = "Audio", DisplayName = "Create Control Bus Mix Stage", meta = (
		AdvancedDisplay = "3",
		WorldContext = "WorldContextObject",
		Keywords = "make modulation modulator stage")
	)
	static FSoundControlBusMixStage CreateBusMixStage(
		const UObject* WorldContextObject,
		USoundControlBus* Bus,
		float Value,
		float AttackTime = 0.1f,
		float ReleaseTime = 0.1f);

	/** Creates a modulation bus mix, with a bus stage set to the provided target value.
	 * @param Name - Name of mix.
	 * @param Stages - Stages mix is responsible for.
	 * @param Activate - Whether or not to activate mix on creation. If true, deactivation will only occur
	 * if returned mix is manually deactivated and not referenced or destroyed (i.e. will not deactivate
	 * when all references become inactive).
	 * @return Capture this in a Blueprint variable to prevent it from being automatically garbage collected. 
	 */
	UFUNCTION(BlueprintCallable, Category = "Audio", DisplayName = "Create Control Bus Mix", meta = (
		WorldContext = "WorldContextObject",
		Keywords = "make modulation modulator")
	)
	static UPARAM(DisplayName = "BusMix") USoundControlBusMix* CreateBusMix(
		UObject* WorldContextObject,
		FName Name, 
		TArray<FSoundControlBusMixStage> Stages,
		bool Activate);

	/** Deactivates a bus. Does nothing if the provided bus is already inactive.
	 * @param Bus - Scope of modulator
	 */
	UFUNCTION(BlueprintCallable, Category = "Audio", DisplayName = "Deactivate Control Bus", meta = (
		WorldContext = "WorldContextObject", 
		Keywords = "modulation modulator bus")
	)
	static void DeactivateBus(const UObject* WorldContextObject, USoundControlBus* Bus);

	/** Deactivates a modulation bus mix. Does nothing if an instance of the provided bus mix is already inactive.
	 * @param BusMix - Mix to deactivate
	 */
	UFUNCTION(BlueprintCallable, Category = "Audio", DisplayName = "Deactivate Control Bus Mix", meta = (
		WorldContext = "WorldContextObject",
		Keywords = "modulation modulator")
	)
	static void DeactivateBusMix(const UObject* WorldContextObject, USoundControlBusMix* Mix);

	/** Deactivates a modulation generator. Does nothing if an instance of the provided generator is already inactive.
	 * @param Generator - Generator to activate
	 * @param Scope - Scope of modulator
	 */
	UFUNCTION(BlueprintCallable, Category = "Audio", DisplayName = "Deactivate Modulation Generator", meta = (
		WorldContext = "WorldContextObject",
		Keywords = "bus modulation modulator generator")
	)
	static void DeactivateGenerator(const UObject* WorldContextObject, USoundModulationGenerator* Generator);

	/** Saves control bus mix to a profile, serialized to an ini file.  If mix is loaded, uses current proxy's state.
	 * If not, uses default UObject representation.
	 * @param BusMix - Mix object to serialize to profile .ini.
	 * @param ProfileIndex - Index of profile, allowing multiple profiles can be saved for single mix object. If 0, saves to default ini profile (no suffix).
	 */
	UFUNCTION(BlueprintCallable, Category = "Audio", DisplayName = "Save Control Bus Mix to Profile", meta = (
		WorldContext = "WorldContextObject",
		AdvancedDisplay = "2",
		Keywords = "serialize modulation modulator ini")
	)
	static void SaveMixToProfile(const UObject* WorldContextObject, USoundControlBusMix* Mix, int32 ProfileIndex = 0);

	/** Loads control bus mix from a profile into UObject mix definition, deserialized from an ini file.
	 * @param BusMix - Mix object to deserialize profile .ini to.
	 * @param bActivate - If true, activate mix upon loading from profile.
	 * @param ProfileIndex - Index of profile, allowing multiple profiles to be loaded to single mix object. If <= 0, loads from default profile (no suffix).
	 * @return Stages - Stage values loaded from profile (empty if profile did not exist or had no values serialized).
	 */
	UFUNCTION(BlueprintCallable, Category = "Audio", DisplayName = "Load Control Bus Mix From Profile", meta = (
		WorldContext = "WorldContextObject",
		AdvancedDisplay = "2",
		Keywords = "deserialize modulation modulator ini")
	)
	static UPARAM(DisplayName = "Stages") TArray<FSoundControlBusMixStage> LoadMixFromProfile(const UObject* WorldContextObject, USoundControlBusMix* Mix, bool bActivate = true, int32 ProfileIndex = 0);

	/** Sets a Control Bus Mix with the provided stage data, if the stages
	 *  are provided in an active instance proxy of the mix. 
	 *  Does not update UObject definition of the mix. 
	 * @param Mix - Mix to update
	 * @param Stages - Stages to set.  If stage's bus is not referenced by mix, stage's update request is ignored.
	 * @param FadeTime - Fade time to user when interpolating between current value and new values.
	 *					 If negative, falls back to last fade time set on stage. If fade time never set on stage,
	 *					 uses attack time set on stage in mix asset.
	 */
	UFUNCTION(BlueprintCallable, Category = "Audio", DisplayName = "Set Control Bus Mix", meta = (
		WorldContext = "WorldContextObject",
		Keywords = "modulation modulator stage")
	)
	static void UpdateMix(const UObject* WorldContextObject, USoundControlBusMix* Mix, TArray<FSoundControlBusMixStage> Stages, float FadeTime = -1.0f);

	/** Sets a Global Control Bus Mix with a single stage associated with the provided Bus to the given float value.  This call should
	 * be reserved for buses that are to be always active. It is *NOT* recommended for transient buses, as not calling clear can keep
	 * buses active indefinitely.
	 * @param Bus - Bus associated with mix to update
	 * @param Value - Value to set global stage to.
	 * @param FadeTime - Fade time to user when interpolating between current value and new value. If negative, falls back to last fade
	 * time set on stage. If fade time never set on stage, defaults to 100ms.
	 */
	UFUNCTION(BlueprintCallable, Category = "Audio", DisplayName = "Set Global Control Bus Mix Value", meta = (
		WorldContext = "WorldContextObject",
		Keywords = "modulation modulator stage")
	)
	static void SetGlobalBusMixValue(const UObject* WorldContextObject, USoundControlBus* Bus, float Value, float FadeTime = -1.0f);

	/** Clears global control bus mix if set, using the applied fade time to return to the provided bus's parameter default value.
	 * @param Bus - Bus associated with mix to update
	 * @param FadeTime - Fade time to user when interpolating between current value and new values.
	 *					 If non-positive, change is immediate.
	 */
	UFUNCTION(BlueprintCallable, Category = "Audio", DisplayName = "Clear Global Control Bus Mix Value", meta = (
		WorldContext = "WorldContextObject",
		Keywords = "modulation modulator stage")
	)
	static void ClearGlobalBusMixValue(const UObject* WorldContextObject, USoundControlBus* Bus, float FadeTime = -1.0f);

	/** Clears all global control bus mix values if set, using the applied fade time to return all to their respective bus's parameter default value.
	 * @param FadeTime - Fade time to user when interpolating between current value and new values.
	 *					 If non-positive, change is immediate.
	 */
	UFUNCTION(BlueprintCallable, Category = "Audio", DisplayName = "Clear All Global Control Bus Mix Values", meta = (
		WorldContext = "WorldContextObject",
		Keywords = "modulation modulator stage")
	)
	static void ClearAllGlobalBusMixValues(const UObject* WorldContextObject, float FadeTime = -1.0f);

	/** Sets filtered stages of a given class to a provided target value for active instance of mix.
	 * Does not update UObject definition of mix.
	 * @param Mix - Mix to modify
	 * @param AddressFilter - (Optional) Address filter to apply to provided mix's stages.
	 * @param ParamClassFilter - (Optional) Filters buses by parameter class.
	 * @param ParamFilter - (Optional) Filters buses by parameter.
	 * @param Value - Target value to mix filtered stages to.
	 * @param FadeTime - If non-negative, updates the fade time for the resulting bus stages found matching the provided filter.
	 */
	UFUNCTION(BlueprintCallable, Category = "Audio", DisplayName = "Set Control Bus Mix By Filter", meta = (
		AdvancedDisplay = "6",
		WorldContext = "WorldContextObject",
		Keywords = "control class modulation modulator stage value")
	)
	static void UpdateMixByFilter(
		const UObject* WorldContextObject,
		USoundControlBusMix* Mix,
		FString AddressFilter,
		TSubclassOf<USoundModulationParameter> ParamClassFilter,
		USoundModulationParameter* ParamFilter,
		float Value = 1.0f,
		float FadeTime = -1.0f);

	/** Commits updates from a UObject definition of a bus mix to active instance in audio thread
	 * (ignored if mix has not been activated).
	 * @param Mix - Mix to update
	 * @param FadeTime - Fade time to user when interpolating between current value and new values.
	 *					 If negative, falls back to last fade time set on stage. If fade time never set on stage,
	 *					 uses attack time set on stage in mix asset.
	 */
	UFUNCTION(BlueprintCallable, Category = "Audio", DisplayName = "Update Control Bus Mix", meta = (
		WorldContext = "WorldContextObject",
		Keywords = "set modulation modulator")
	)
	static void UpdateMixFromObject(const UObject* WorldContextObject, USoundControlBusMix* Mix, float FadeTime = -1.0f);

	/** Commits updates from a UObject definition of a modulator (e.g. Bus, Bus Mix, Generator)
	 *  to active instance in audio thread (ignored if modulator type has not been activated).
	 * @param Modulator - Modulator to update
	 */
	UFUNCTION(BlueprintCallable, Category = "Audio", DisplayName = "Update Modulator", meta = (
		WorldContext = "WorldContextObject",
		Keywords = "set control bus mix modulation modulator generator")
	)
	static void UpdateModulator(const UObject* WorldContextObject, USoundModulatorBase* Modulator);
};
