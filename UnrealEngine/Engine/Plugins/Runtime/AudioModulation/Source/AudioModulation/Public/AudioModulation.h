// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "AudioDeviceManager.h"
#include "CoreMinimal.h"
#include "HAL/ThreadSafeBool.h"
#include "IAudioExtensionPlugin.h"
#include "IAudioModulation.h"
#include "Modules/ModuleInterface.h"
#include "SoundControlBus.h"
#include "SoundControlBusMix.h"
#include "SoundModulationGenerator.h"
#include "SoundModulationParameter.h"
#include "Stats/Stats.h"


// Cycle stats for audio mixer
DECLARE_STATS_GROUP(TEXT("AudioModulation"), STATGROUP_AudioModulation, STATCAT_Advanced);

// Tracks the time for the full render block 
DECLARE_CYCLE_STAT_EXTERN(TEXT("Process Modulators"), STAT_AudioModulationProcessModulators, STATGROUP_AudioModulation, AUDIOMODULATION_API);

namespace AudioModulation
{
	// Forward Declarations
	class FAudioModulationSystem;

	class AUDIOMODULATION_API FAudioModulationManager : public IAudioModulationManager
	{
	public:
		FAudioModulationManager();
		virtual ~FAudioModulationManager();

		//~ Begin IAudioModulationManager implementation
		virtual void Initialize(const FAudioPluginInitializationParams& InitializationParams) override;
		virtual void OnAuditionEnd() override;

#if !UE_BUILD_SHIPPING
		virtual void SetDebugBusFilter(const FString* InFilter);
		virtual void SetDebugMixFilter(const FString* InFilter);
		virtual void SetDebugMatrixEnabled(bool bInIsEnabled);
		virtual void SetDebugGeneratorsEnabled(bool bInIsEnabled);
		virtual void SetDebugGeneratorFilter(const FString* InFilter);
		virtual void SetDebugGeneratorTypeFilter(const FString* InFilter, bool bInIsEnabled);

		virtual bool OnPostHelp(FCommonViewportClient* ViewportClient, const TCHAR* Stream) override;
		virtual int32 OnRenderStat(FViewport* Viewport, FCanvas* Canvas, int32 X, int32 Y, const UFont& Font, const FVector* ViewLocation, const FRotator* ViewRotation) override;
		virtual bool OnToggleStat(FCommonViewportClient* ViewportClient, const TCHAR* Stream) override;
#endif // !UE_BUILD_SHIPPING

		virtual void ProcessModulators(const double InElapsed) override;
		virtual void UpdateModulator(const USoundModulatorBase& InModulator) override;

	protected:
		virtual void RegisterModulator(Audio::FModulatorHandleId InHandleId, Audio::FModulatorId InModulatorId) override;
		virtual bool GetModulatorValue(const Audio::FModulatorHandle& ModulatorHandle, float& OutValue) const override;
		virtual bool GetModulatorValueThreadSafe(const Audio::FModulatorHandle& ModulatorHandle, float& OutValue) const override;
		virtual void UnregisterModulator(const Audio::FModulatorHandle& InHandle) override;
	//~ End IAudioModulationManager implementation

	public:
		void ActivateBus(const USoundControlBus& InBus);
		void ActivateBusMix(const USoundControlBusMix& InBusMix);
		void ActivateGenerator(const USoundModulationGenerator& InGenerator);

		void DeactivateBus(const USoundControlBus& InBus);
		void DeactivateBusMix(const USoundControlBusMix& InBusMix);
		void DeactivateAllBusMixes();
		void DeactivateGenerator(const USoundModulationGenerator& InGenerator);

		void SaveMixToProfile(const USoundControlBusMix& InBusMix, const int32 InProfileIndex);
		TArray<FSoundControlBusMixStage> LoadMixFromProfile(const int32 InProfileIndex, USoundControlBusMix& OutBusMix);

		void UpdateMix(const TArray<FSoundControlBusMixStage>& InStages, USoundControlBusMix& InOutMix, bool bInUpdateObject = false, float InFadeTime = -1.0f);
		void UpdateMix(const USoundControlBusMix& InMix, float InFadeTime = -1.0f);
		void UpdateMixByFilter(const FString& InAddressFilter, const TSubclassOf<USoundModulationParameter>& InParamClassFilter, USoundModulationParameter* InParamFilter, float Value, float FadeTime, USoundControlBusMix& InOutMix, bool bInUpdateObject = false);

		void SoloBusMix(const USoundControlBusMix& InBusMix);

		void SetGlobalBusMixValue(USoundControlBus& InBus, float InValue, float InFadeTime);
		void ClearGlobalBusMixValue(const USoundControlBus& InBus, float InFadeTime);
		void ClearAllGlobalBusMixValues(float InFadeTime);

		FAudioModulationSystem& GetSystem();

	private:
		FAudioModulationSystem* ModSystem = nullptr;
	};

	AUDIOMODULATION_API FAudioModulationManager* GetDeviceModulationManager(Audio::FDeviceId InDeviceId);

	AUDIOMODULATION_API void IterateModulationManagers(TFunctionRef<void(FAudioModulationManager&)> InFunction);
} // namespace AudioModulation

class AUDIOMODULATION_API FAudioModulationPluginFactory : public IAudioModulationFactory
{
public:
	virtual const FName& GetDisplayName() const override
	{
		static FName DisplayName = FName(TEXT("DefaultModulationPlugin"));
		return DisplayName;
	}

	virtual TAudioModulationPtr CreateNewModulationPlugin(FAudioDevice* OwningDevice) override;
};

class AUDIOMODULATION_API FAudioModulationModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	FAudioModulationPluginFactory ModulationPluginFactory;
};