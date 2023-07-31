// Copyright Epic Games, Inc. All Rights Reserved.

#include "Sound/SoundModulationDestination.h"

#include "Algo/NoneOf.h"
#include "Algo/Transform.h"
#include "Async/Async.h"
#include "AudioDevice.h"
#include "DSP/FloatArrayMath.h"
#include "IAudioModulation.h"
#include "Math/TransformCalculus.h"
#include "UObject/Object.h"
#include "GenericPlatform/GenericPlatformCompilerPreSetup.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SoundModulationDestination)


FSoundModulationDefaultSettings::FSoundModulationDefaultSettings()
{
	VolumeModulationDestination.Value = 0.0f;
	PitchModulationDestination.Value = 0.0f;
	HighpassModulationDestination.Value = MIN_FILTER_FREQUENCY;
	LowpassModulationDestination.Value = MAX_FILTER_FREQUENCY;
}

#if WITH_EDITORONLY_DATA
void FSoundModulationDefaultSettings::VersionModulators()
{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	VolumeModulationDestination.VersionModulators();
	PitchModulationDestination.VersionModulators();
	HighpassModulationDestination.VersionModulators();
	LowpassModulationDestination.VersionModulators();
PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

void FSoundModulationDestinationSettings::VersionModulators()
{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	if (Modulator)
	{
		Modulators.Add(Modulator);
		Modulator = nullptr;
	}
PRAGMA_ENABLE_DEPRECATION_WARNINGS
}
#endif // WITH_EDITORONLY_DATA


FSoundModulationDefaultRoutingSettings::FSoundModulationDefaultRoutingSettings()
	: FSoundModulationDefaultSettings()
{
}


namespace Audio
{
	FModulationDestination::FModulationDestination(const FModulationDestination& InModulationDestination)
		: DeviceId(InModulationDestination.DeviceId)
		, ValueTarget(InModulationDestination.ValueTarget)
		, bIsBuffered(InModulationDestination.bIsBuffered)
		, bValueNormalized(InModulationDestination.bValueNormalized)
		, OutputBuffer(InModulationDestination.OutputBuffer)
		, Parameter(InModulationDestination.Parameter)
	{
		FScopeLock OtherLock(&InModulationDestination.HandleCritSection);
		Handles = InModulationDestination.Handles;
	}

	FModulationDestination::FModulationDestination(FModulationDestination&& InModulationDestination)
		: DeviceId(MoveTemp(InModulationDestination.DeviceId))
		, ValueTarget(MoveTemp(InModulationDestination.ValueTarget))
		, bIsBuffered(MoveTemp(InModulationDestination.bIsBuffered))
		, bValueNormalized(MoveTemp(InModulationDestination.bValueNormalized))
		, OutputBuffer(MoveTemp(InModulationDestination.OutputBuffer))
		, Parameter(MoveTemp(InModulationDestination.Parameter))
	{
		FScopeLock OtherLock(&InModulationDestination.HandleCritSection);
		Handles = MoveTemp(InModulationDestination.Handles);
	}

	FModulationDestination& FModulationDestination::operator=(const FModulationDestination& InModulationDestination)
	{
		DeviceId = InModulationDestination.DeviceId;
		ValueTarget = InModulationDestination.ValueTarget;
		bIsBuffered = InModulationDestination.bIsBuffered;
		bValueNormalized = InModulationDestination.bValueNormalized;
		OutputBuffer = InModulationDestination.OutputBuffer;

		{
			FScopeLock OtherLock(&InModulationDestination.HandleCritSection);
			FScopeLock Lock(&HandleCritSection);
			Handles = InModulationDestination.Handles;
		}

		Parameter = InModulationDestination.Parameter;

		return *this;
	}

	FModulationDestination& FModulationDestination::operator=(FModulationDestination&& InModulationDestination)
	{
		DeviceId = MoveTemp(InModulationDestination.DeviceId);
		ValueTarget = MoveTemp(InModulationDestination.ValueTarget);
		bIsBuffered = MoveTemp(InModulationDestination.bIsBuffered);
		bValueNormalized = MoveTemp(InModulationDestination.bValueNormalized);
		bHasProcessed = MoveTemp(InModulationDestination.bHasProcessed);
		OutputBuffer = MoveTemp(InModulationDestination.OutputBuffer);
		{
			FScopeLock OtherLock(&InModulationDestination.HandleCritSection);
			FScopeLock Lock(&HandleCritSection);
			Handles = MoveTemp(InModulationDestination.Handles);
		}

		Parameter = MoveTemp(InModulationDestination.Parameter);

		return *this;
	}

	void FModulationDestination::ResetHandles()
	{
		Audio::FModulationParameter ParameterCopy = Parameter;

		FScopeLock Lock(&HandleCritSection);
		Handles.Reset();
		Handles.Add(FModulatorHandle { MoveTemp(ParameterCopy) });
	}

	void FModulationDestination::Init(FDeviceId InDeviceId, bool bInIsBuffered, bool bInValueNormalized)
	{
		Init(InDeviceId, FName(), bInIsBuffered, bInValueNormalized);
	}

	void FModulationDestination::Init(FDeviceId InDeviceId, FName InParameterName, bool bInIsBuffered, bool bInValueNormalized)
	{
		DeviceId = InDeviceId;
		bIsBuffered = bInIsBuffered;
		bValueNormalized = bInValueNormalized;

		OutputBuffer.Reset();
		Parameter = Audio::GetModulationParameter(InParameterName);

		ResetHandles();
	}

	bool FModulationDestination::IsActive()
	{
		FScopeLock Lock(&HandleCritSection);
		return Algo::NoneOf(Handles, [](const FModulatorHandle& Handle) { return Handle.IsValid(); });
	}

	bool FModulationDestination::ProcessControl(float InValueUnitBase, int32 InNumSamples)
	{
		bHasProcessed = true;
		float LastTarget = ValueTarget;

		float NewTargetNormalized = Parameter.DefaultValue;
		if (Parameter.bRequiresConversion)
		{
			Parameter.NormalizedFunction(NewTargetNormalized);
		}

		FScopeLock Lock(&HandleCritSection);
		{
			for (const FModulatorHandle& Handle : Handles)
			{
				if (Handle.IsValid())
				{
					float NewHandleValue = 1.0f;
					Handle.GetValue(NewHandleValue);
					Parameter.MixFunction(NewTargetNormalized, NewHandleValue);
				}
			}
		}

		// Convert base to linear space
		float InValueBaseNormalized = InValueUnitBase;
		if (Parameter.bRequiresConversion)
		{
			Parameter.NormalizedFunction(InValueBaseNormalized);
		}

		// Mix in base value
		Parameter.MixFunction(NewTargetNormalized, InValueBaseNormalized);
		ValueTarget = NewTargetNormalized;

		// Convert target to unit space if required
		if (Parameter.bRequiresConversion && !bValueNormalized)
		{
			Parameter.UnitFunction(ValueTarget);
		}

		if (bIsBuffered)
		{
			if (OutputBuffer.Num() != InNumSamples)
			{
				OutputBuffer.Reset();
				OutputBuffer.AddZeroed(InNumSamples);
			}
		}

		// Fade from last target to new if output buffer is active
		if (!OutputBuffer.IsEmpty())
		{
			if (OutputBuffer.Num() % AUDIO_NUM_FLOATS_PER_VECTOR_REGISTER == 0)
			{
				if (FMath::IsNearlyEqual(LastTarget, ValueTarget))
				{
					ArraySetToConstantInplace(OutputBuffer, ValueTarget);
				}
				else
				{
					ArraySetToConstantInplace(OutputBuffer, 1.0f);
					ArrayFade(OutputBuffer, LastTarget, ValueTarget);
				}
			}
			else
			{
				if (FMath::IsNearlyEqual(LastTarget, ValueTarget))
				{
					OutputBuffer.Init(ValueTarget, InNumSamples);
				}
				else
				{
					float SampleValue = LastTarget;
					const float DeltaValue = (ValueTarget - LastTarget) / OutputBuffer.Num();
					for (int32 i = 0; i < OutputBuffer.Num(); ++i)
					{
						OutputBuffer[i] = SampleValue;
						SampleValue += DeltaValue;
					}
				}
			}
		}

		return !FMath::IsNearlyEqual(LastTarget, ValueTarget);
	}

	void FModulationDestination::UpdateModulator(const USoundModulatorBase* InModulator)
	{
		UpdateModulators({ InModulator });
	}

	void FModulationDestination::UpdateModulators(const TSet<TObjectPtr<USoundModulatorBase>>& InModulators)
	{
		TArray<TUniquePtr<Audio::IModulatorSettings>> ProxySettings;
		Algo::TransformIf(
			InModulators,
			ProxySettings,
			[](const USoundModulatorBase* Mod) { return Mod != nullptr; },
			[](const USoundModulatorBase* Mod) { return Mod->CreateProxySettings(); }
		);

		UpdateModulatorsInternal(MoveTemp(ProxySettings));
	}

	void FModulationDestination::UpdateModulators(const TSet<USoundModulatorBase*>& InModulators)
	{
		TArray<TUniquePtr<Audio::IModulatorSettings>> ProxySettings;
		Algo::TransformIf(
			InModulators,
			ProxySettings,
			[](const USoundModulatorBase* Mod) { return Mod != nullptr; },
			[](const USoundModulatorBase* Mod) { return Mod->CreateProxySettings(); }
		);

		UpdateModulatorsInternal(MoveTemp(ProxySettings));
	}

	void FModulationDestination::UpdateModulators(const TSet<const USoundModulatorBase*>& InModulators)
	{
		TArray<TUniquePtr<Audio::IModulatorSettings>> ProxySettings;
		Algo::TransformIf(
			InModulators,
			ProxySettings,
			[](const USoundModulatorBase* Mod) { return Mod != nullptr; },
			[](const USoundModulatorBase* Mod) { return Mod->CreateProxySettings(); }
		);

		UpdateModulatorsInternal(MoveTemp(ProxySettings));
	}

	void FModulationDestination::UpdateModulatorsInternal(TArray<TUniquePtr<Audio::IModulatorSettings>>&& ProxySettings)
	{
		auto UpdateHandleLambda = [this, ModSettings = MoveTemp(ProxySettings)]() mutable
		{
			if (FAudioDevice* AudioDevice = FAudioDeviceManager::Get()->GetAudioDeviceRaw(DeviceId))
			{
				if (AudioDevice->IsModulationPluginEnabled() && AudioDevice->ModulationInterface.IsValid())
				{
					if (IAudioModulationManager* Modulation = AudioDevice->ModulationInterface.Get())
					{
						TSet<FModulatorHandle> NewHandles;
						for (TUniquePtr<Audio::IModulatorSettings>& ModSetting : ModSettings)
						{
							Audio::FModulationParameter HandleParam = Parameter;
							NewHandles.Add(FModulatorHandle{ *Modulation, *ModSetting.Get(), MoveTemp(HandleParam) });
						}

						FScopeLock Lock(&HandleCritSection);
						Handles = MoveTemp(NewHandles);
					}
					return;
				}
			}

			ResetHandles();
		};

		FAudioThread::RunCommandOnAudioThread(MoveTemp(UpdateHandleLambda));
	}
} // namespace Audio

