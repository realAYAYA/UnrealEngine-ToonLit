// Copyright Epic Games, Inc. All Rights Reserved.

#include "AudioModulation.h"

#include "AudioModulationLogging.h"
#include "AudioModulationSettings.h"
#include "AudioModulationSystem.h"
#include "CanvasTypes.h"
#include "Features/IModularFeatures.h"
#include "HAL/LowLevelMemTracker.h"
#include "IAudioModulation.h"
#include "Modules/ModuleManager.h"
#include "SoundControlBusMix.h"
#include "SoundModulationParameter.h"
#include "SoundModulationPatch.h"
#include "SoundModulatorAsset.h"
#include "UObject/NoExportTypes.h"

#if WITH_AUDIOMODULATION_METASOUND_SUPPORT
#include "MetasoundDataTypeRegistrationMacro.h"
#include "MetasoundFrontendRegistries.h"

REGISTER_METASOUND_DATATYPE(AudioModulation::FSoundModulatorAsset, "Modulator", Metasound::ELiteralType::UObjectProxy, USoundModulatorBase);
REGISTER_METASOUND_DATATYPE(AudioModulation::FSoundModulationParameterAsset, "ModulationParameter", Metasound::ELiteralType::UObjectProxy, USoundModulationParameter);
#endif // WITH_AUDIOMODULATION_METASOUND_SUPPORT

namespace AudioModulation
{
	FAudioModulationManager::FAudioModulationManager()
		: ModSystem(new FAudioModulationSystem())
	{
	}

	FAudioModulationManager::~FAudioModulationManager()
	{
		delete ModSystem;
	}

	void FAudioModulationManager::Initialize(const FAudioPluginInitializationParams& InitializationParams)
	{
		ModSystem->Initialize(InitializationParams);
	}

	void FAudioModulationManager::OnAuditionEnd()
	{
		ModSystem->OnAuditionEnd();
	}

	void FAudioModulationManager::ActivateBus(const USoundControlBus& InBus)
	{
		ModSystem->ActivateBus(InBus);
	}

	void FAudioModulationManager::ActivateBusMix(const USoundControlBusMix& InBusMix)
	{
		ModSystem->ActivateBusMix(InBusMix);
	}

	void FAudioModulationManager::ActivateGenerator(const USoundModulationGenerator& InGenerator)
	{
		ModSystem->ActivateGenerator(InGenerator);
	}

	void FAudioModulationManager::DeactivateBus(const USoundControlBus& InBus)
	{
		ModSystem->DeactivateBus(InBus);
	}

	void FAudioModulationManager::DeactivateBusMix(const USoundControlBusMix& InBusMix)
	{
		ModSystem->DeactivateBusMix(InBusMix);
	}

	void FAudioModulationManager::DeactivateAllBusMixes()
	{
		ModSystem->DeactivateAllBusMixes();
	}

	void FAudioModulationManager::DeactivateGenerator(const USoundModulationGenerator& InGenerator)
	{
		ModSystem->DeactivateGenerator(InGenerator);
	}

#if !UE_BUILD_SHIPPING
	void FAudioModulationManager::SetDebugBusFilter(const FString* InNameFilter)
	{
		ModSystem->SetDebugBusFilter(InNameFilter);
	}

	void FAudioModulationManager::SetDebugGeneratorFilter(const FString* InFilter)
	{
		ModSystem->SetDebugGeneratorFilter(InFilter);
	}

	void FAudioModulationManager::SetDebugGeneratorTypeFilter(const FString* InFilter, bool bInIsEnabled)
	{
		ModSystem->SetDebugGeneratorTypeFilter(InFilter, bInIsEnabled);
	}

	void FAudioModulationManager::SetDebugGeneratorsEnabled(bool bInIsEnabled)
	{
		ModSystem->SetDebugGeneratorsEnabled(bInIsEnabled);
	}

	void FAudioModulationManager::SetDebugMatrixEnabled(bool bInIsEnabled)
	{
		ModSystem->SetDebugMatrixEnabled(bInIsEnabled);
	}

	void FAudioModulationManager::SetDebugMixFilter(const FString* InNameFilter)
	{
		ModSystem->SetDebugMixFilter(InNameFilter);
	}

#endif // !UE_BUILD_SHIPPING

	void FAudioModulationManager::SaveMixToProfile(const USoundControlBusMix& InBusMix, const int32 InProfileIndex)
	{
		ModSystem->SaveMixToProfile(InBusMix, InProfileIndex);
	}

	TArray<FSoundControlBusMixStage> FAudioModulationManager::LoadMixFromProfile(const int32 InProfileIndex, USoundControlBusMix& OutBusMix)
	{
		return ModSystem->LoadMixFromProfile(InProfileIndex, OutBusMix);
	}

	void FAudioModulationManager::UpdateMix(const TArray<FSoundControlBusMixStage>& InStages, USoundControlBusMix& InOutMix, bool bInUpdateObject, float InFadeTime)
	{
		ModSystem->UpdateMix(InStages, InOutMix, bInUpdateObject, InFadeTime);
	}

	void FAudioModulationManager::UpdateMix(const USoundControlBusMix& InMix, float InFadeTime)
	{
		ModSystem->UpdateMix(InMix, InFadeTime);
	}

	void FAudioModulationManager::UpdateMixByFilter(const FString& InAddressFilter, const TSubclassOf<USoundModulationParameter>& InParamClassFilter, USoundModulationParameter* InParamFilter, float Value, float FadeTime, USoundControlBusMix& InOutMix, bool bInUpdateObject)
	{
		ModSystem->UpdateMixByFilter(InAddressFilter, InParamClassFilter, InParamFilter, Value, FadeTime, InOutMix, bInUpdateObject);
	}

	void FAudioModulationManager::SoloBusMix(const USoundControlBusMix& InBusMix)
	{
		ModSystem->SoloBusMix(InBusMix);
	}

	void FAudioModulationManager::SetGlobalBusMixValue(USoundControlBus& InBus, float InValue, float InFadeTime)
	{
		ModSystem->SetGlobalBusMixValue(InBus, InValue, InFadeTime);
	}

	void FAudioModulationManager::ClearGlobalBusMixValue(const USoundControlBus& InBus, float InFadeTime)
	{
		ModSystem->ClearGlobalBusMixValue(InBus, InFadeTime);
	}

	void FAudioModulationManager::ClearAllGlobalBusMixValues(float InFadeTime)
	{
		ModSystem->ClearAllGlobalBusMixValues(InFadeTime);
	}

#if !UE_BUILD_SHIPPING
	bool FAudioModulationManager::OnPostHelp(FCommonViewportClient* ViewportClient, const TCHAR* Stream)
	{
		return ModSystem->OnPostHelp(ViewportClient, Stream);
	}

	int32 FAudioModulationManager::OnRenderStat(FViewport* Viewport, FCanvas* Canvas, int32 X, int32 Y, const UFont& Font, const FVector* ViewLocation, const FRotator* ViewRotation)
	{
		return ModSystem->OnRenderStat(Viewport, Canvas, X, Y, Font, ViewLocation, ViewRotation);
	}

	bool FAudioModulationManager::OnToggleStat(FCommonViewportClient* ViewportClient, const TCHAR* Stream)
	{
		return ModSystem->OnToggleStat(ViewportClient, Stream);
	}
#endif // !UE_BUILD_SHIPPING

	void FAudioModulationManager::ProcessModulators(const double InElapsed)
	{
		ModSystem->ProcessModulators(InElapsed);
	}

	void FAudioModulationManager::RegisterModulator(Audio::FModulatorHandleId InHandleId, Audio::FModulatorId InModulatorId)
	{
		ModSystem->RegisterModulator(InHandleId, InModulatorId);
	}

	bool FAudioModulationManager::GetModulatorValue(const Audio::FModulatorHandle& ModulatorHandle, float& OutValue) const
	{
		return ModSystem->GetModulatorValue(ModulatorHandle, OutValue);
	}

	bool FAudioModulationManager::GetModulatorValueThreadSafe(const Audio::FModulatorHandle& ModulatorHandle, float& OutValue) const
	{
		return ModSystem->GetModulatorValueThreadSafe(ModulatorHandle, OutValue);
	}

	FAudioModulationSystem& FAudioModulationManager::GetSystem()
	{
		check(ModSystem);
		return *ModSystem;
	}

	void FAudioModulationManager::UnregisterModulator(const Audio::FModulatorHandle& InHandle)
	{
		ModSystem->UnregisterModulator(InHandle);
	}

	void FAudioModulationManager::UpdateModulator(const USoundModulatorBase& InModulator)
	{
		ModSystem->UpdateModulator(InModulator);
	}

	FAudioModulationManager* GetDeviceModulationManager(Audio::FDeviceId InDeviceId)
	{
		FAudioDeviceManager* DeviceManager = FAudioDeviceManager::Get();
		if (ensure(DeviceManager))
		{
			FAudioDevice* AudioDevice = DeviceManager->GetAudioDeviceRaw(InDeviceId);
			if (ensure(AudioDevice))
			{
				if (AudioDevice->IsModulationPluginEnabled())
				{
					if (IAudioModulationManager* Modulation = AudioDevice->ModulationInterface.Get())
					{
						return static_cast<FAudioModulationManager*>(Modulation);
					}
				}
			}
		}

		return nullptr;
	}

	void IterateModulationManagers(TFunctionRef<void(FAudioModulationManager&)> InFunction)
	{
		if (FAudioDeviceManager* DeviceManager = FAudioDeviceManager::Get())
		{
			TArray<FAudioDevice*> Devices = DeviceManager->GetAudioDevices();
			DeviceManager->IterateOverAllDevices([ModFunction = MoveTemp(InFunction)](Audio::FDeviceId DeviceId, FAudioDevice* AudioDevice)
			{
				if (AudioDevice && AudioDevice->IsModulationPluginEnabled() && AudioDevice->ModulationInterface.IsValid())
				{
					auto ModulationInterface = static_cast<AudioModulation::FAudioModulationManager*>(AudioDevice->ModulationInterface.Get());
					ModFunction(*ModulationInterface);
				}
			});
		}
	}
} // namespace AudioModulation

TAudioModulationPtr FAudioModulationPluginFactory::CreateNewModulationPlugin(FAudioDevice* OwningDevice)
{
	return TAudioModulationPtr(new AudioModulation::FAudioModulationManager());
}

void FAudioModulationModule::StartupModule()
{
	LLM_SCOPE(ELLMTag::AudioMixerPlugins);
	IModularFeatures::Get().RegisterModularFeature(FAudioModulationPluginFactory::GetModularFeatureName(), &ModulationPluginFactory);

	if (const UAudioModulationSettings* ModulationSettings = GetDefault<UAudioModulationSettings>())
	{
		ModulationSettings->RegisterParameters();
	}

#if WITH_AUDIOMODULATION_METASOUND_SUPPORT
	UE_LOG(LogAudioModulation, Log, TEXT("Registering Modulation MetaSound Nodes..."));

	// All MetaSound interfaces are required to be loaded prior to registering & loading MetaSound assets,
	// so check that the MetaSoundEngine is loaded prior to pending Modulation defined classes
	FModuleManager::Get().LoadModuleChecked("MetasoundEngine");

	FMetasoundFrontendRegistryContainer::Get()->RegisterPendingNodes();
#endif // WITH_AUDIOMODULATION_METASOUND_SUPPORT

	UE_LOG(LogAudioModulation, Log, TEXT("Audio Modulation Initialized"));
}

void FAudioModulationModule::ShutdownModule()
{
	LLM_SCOPE(ELLMTag::AudioMixerPlugins);
	IModularFeatures::Get().UnregisterModularFeature(FAudioModulationPluginFactory::GetModularFeatureName(), &ModulationPluginFactory);
	UE_LOG(LogAudioModulation, Log, TEXT("Audio Modulation Shutdown"));
}


IMPLEMENT_MODULE(FAudioModulationModule, AudioModulation);
