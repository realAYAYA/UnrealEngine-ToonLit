// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animators/PropertyAnimatorSoundWave.h"

#include "LoudnessNRT.h"
#include "Properties/Handlers/PropertyAnimatorCoreHandlerBase.h"
#include "Properties/PropertyAnimatorFloatContext.h"
#include "Sound/SoundWave.h"

UPropertyAnimatorSoundWave::UPropertyAnimatorSoundWave()
{
	SetAnimatorDisplayName(DefaultControllerName);
}

void UPropertyAnimatorSoundWave::SetSampledSoundWave(USoundWave* InSoundWave)
{
	if (SampledSoundWave == InSoundWave)
	{
		return;
	}

	SampledSoundWave = InSoundWave;
	OnSampledSoundWaveChanged();
}

void UPropertyAnimatorSoundWave::SetLoop(bool bInLoop)
{
	bLoop = bInLoop;
}

#if WITH_EDITOR
void UPropertyAnimatorSoundWave::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName MemberName = PropertyChangedEvent.GetMemberPropertyName();

	if (MemberName == GET_MEMBER_NAME_CHECKED(UPropertyAnimatorSoundWave, SampledSoundWave))
	{
		OnSampledSoundWaveChanged();
	}
}
#endif // WITH_EDITOR

void UPropertyAnimatorSoundWave::OnSampledSoundWaveChanged()
{
	if (!AudioAnalyzer)
	{
		AudioAnalyzer = NewObject<ULoudnessNRT>(this);
	}

	AudioAnalyzer->Sound = SampledSoundWave;

#if WITH_EDITOR
	// Needed to analyse new audio sample
	FProperty* SoundProperty = FindFProperty<FProperty>(ULoudnessNRT::StaticClass(), GET_MEMBER_NAME_CHECKED(ULoudnessNRT, Sound));
	FPropertyChangedEvent PropertyChangedEvent(SoundProperty, EPropertyChangeType::ValueSet);
	AudioAnalyzer->PostEditChangeProperty(PropertyChangedEvent);
#endif
}

float UPropertyAnimatorSoundWave::Evaluate(double InTimeElapsed, const FPropertyAnimatorCoreData& InPropertyData, UPropertyAnimatorFloatContext* InOptions) const
{
	float Loudness = 0.f;

	if (AudioAnalyzer
		&& AudioAnalyzer->DurationInSeconds > 0)
	{
		float SampleTime = InTimeElapsed + InOptions->GetTimeOffset();

		if ((SampleTime >= 0 && SampleTime <= AudioAnalyzer->DurationInSeconds) || bLoop)
		{
			SampleTime = FMath::Fmod(SampleTime, AudioAnalyzer->DurationInSeconds);

			if (SampleTime < 0)
			{
				SampleTime = AudioAnalyzer->DurationInSeconds + SampleTime;
			}

			AudioAnalyzer->GetNormalizedLoudnessAtTime(SampleTime, Loudness);
		}
	}

	// Remap from [0, 1] to user amplitude from [Min, Max]
	return FMath::GetMappedRangeValueClamped(FVector2D(0, 1), FVector2D(InOptions->GetAmplitudeMin(), InOptions->GetAmplitudeMax()), Loudness);
}
