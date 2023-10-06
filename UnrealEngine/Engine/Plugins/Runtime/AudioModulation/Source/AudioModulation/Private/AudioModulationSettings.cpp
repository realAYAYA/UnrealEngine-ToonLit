// Copyright Epic Games, Inc. All Rights Reserved.
#include "AudioModulationSettings.h"

#include "AudioModulationLogging.h"
#include "IAudioModulation.h"
#include "SoundModulationParameter.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AudioModulationSettings)

#if WITH_EDITOR
void UAudioModulationSettings::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	if (PropertyChangedEvent.Property)
	{
		FName PropertyName = PropertyChangedEvent.Property->GetFName();

		if (PropertyName == GET_MEMBER_NAME_CHECKED(UAudioModulationSettings, Parameters))
		{
			UE_LOG(LogAudioModulation, Display, TEXT("Reloading Parameters From Modulation Developer Settings..."));
			RegisterParameters();
		}
	}
}
#endif // WITH_EDITOR

void UAudioModulationSettings::RegisterParameters() const
{
	Audio::UnregisterAllModulationParameters();

	for (const FSoftObjectPath& Path : Parameters)
	{
		if (USoundModulationParameter* Param = Cast<USoundModulationParameter>(Path.TryLoad()))
		{
			Audio::FModulationParameter NewParam = Param->CreateParameter();
			Audio::RegisterModulationParameter(NewParam.ParameterName, MoveTemp(NewParam));
			UE_LOG(LogAudioModulation, Display, TEXT("Initialized Audio Modulation Parameter '%s'"), *NewParam.ParameterName.ToString());
		}
		else
		{
			const FString& ParamName = Path.GetAssetName();
			UE_LOG(LogAudioModulation, Error, TEXT("Failed to load parameter at '%s': Missing asset or invalid type."), *ParamName);

			// Create a parameter procedurally instead. These parameters are all required by Audio Mixer Sources.
			if (ParamName == "HPFCutoffFrequency")
			{
				Audio::FModulationParameter NewParam = USoundModulationParameterHPFFrequency::CreateDefaultParameter();
				NewParam.ParameterName = "HPFCutoffFrequency";
				Audio::RegisterModulationParameter(NewParam.ParameterName, MoveTemp(NewParam));
				UE_LOG(LogAudioModulation, Error, TEXT("Created temporary HPFCutoffFrequency parameter to use instead."));
			}
			else if (ParamName == "LPFCutoffFrequency")
			{
				Audio::FModulationParameter NewParam = USoundModulationParameterLPFFrequency::CreateDefaultParameter();
				NewParam.ParameterName = "LPFCutoffFrequency";
				Audio::RegisterModulationParameter(NewParam.ParameterName, MoveTemp(NewParam));
				UE_LOG(LogAudioModulation, Error, TEXT("Created temporary LPFCutoffFrequency parameter to use instead."));
			}
			else if (ParamName == "Pitch")
			{
				// The Pitch parameter is a specialized Bipolar parameter, so we have to give it extra data and change the unit display name
				Audio::FModulationParameter NewParam = USoundModulationParameterBipolar::CreateDefaultParameter(24.0f);
				NewParam.ParameterName = "Pitch";
				Audio::RegisterModulationParameter(NewParam.ParameterName, MoveTemp(NewParam));
				UE_LOG(LogAudioModulation, Error, TEXT("Created temporary Pitch parameter to use instead."));
			}
			else if (ParamName == "Volume")
			{
				// TODO: Similar to pitch, send -60 dB to the function
				Audio::FModulationParameter NewParam = USoundModulationParameterVolume::CreateDefaultParameter(-60.0f);
				NewParam.ParameterName = "Volume";
				Audio::RegisterModulationParameter(NewParam.ParameterName, MoveTemp(NewParam));
				UE_LOG(LogAudioModulation, Error, TEXT("Created temporary Volume parameter to use instead."));
			}
		}
	}
}
