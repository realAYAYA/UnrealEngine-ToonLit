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
			UE_LOG(LogAudioModulation, Error, TEXT("Failed to load parameter at '%s': Missing asset or invalid type."), *Path.GetAssetName());
		}
	}
}
