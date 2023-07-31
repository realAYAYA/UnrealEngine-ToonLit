// Copyright Epic Games, Inc. All Rights Reserved.

#include "Sections/MovieSceneAudioSection.h"
#include "Tracks/MovieSceneAudioTrack.h"
#include "Sound/SoundBase.h"
#include "UObject/SequencerObjectVersion.h"
#include "Channels/MovieSceneChannelProxy.h"
#include "MovieScene.h"
#include "MovieSceneCommonHelpers.h"
#include "Misc/FrameRate.h"
#include "Misc/GeneratedTypeName.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MovieSceneAudioSection)

#if WITH_EDITOR

struct FAudioChannelEditorData
{
	FAudioChannelEditorData()
	{
		Data[0].SetIdentifiers("Volume", NSLOCTEXT("MovieSceneAudioSection", "SoundVolumeText", "Volume"));
		Data[1].SetIdentifiers("Pitch", NSLOCTEXT("MovieSceneAudioSection", "PitchText", "Pitch"));
		Data[2].SetIdentifiers("AttachActor", NSLOCTEXT("MovieSceneAudioSection", "AttachActorText", "Attach"));
	}

	FMovieSceneChannelMetaData Data[3];
};

#endif // WITH_EDITOR

namespace
{
	float AudioDeprecatedMagicNumber = TNumericLimits<float>::Lowest();

	FFrameNumber GetStartOffsetAtTrimTime(FQualifiedFrameTime TrimTime, FFrameNumber StartOffset, FFrameNumber StartFrame)
	{
		return StartOffset + TrimTime.Time.FrameNumber - StartFrame;
	}
}

UMovieSceneAudioSection::UMovieSceneAudioSection( const FObjectInitializer& ObjectInitializer )
	: Super( ObjectInitializer )
{
	Sound = nullptr;
	StartOffset_DEPRECATED = AudioDeprecatedMagicNumber;
	AudioStartTime_DEPRECATED = AudioDeprecatedMagicNumber;
	AudioDilationFactor_DEPRECATED = AudioDeprecatedMagicNumber;
	AudioVolume_DEPRECATED = AudioDeprecatedMagicNumber;
	bLooping = true;
	bSuppressSubtitles = false;
	bOverrideAttenuation = false;
	BlendType = EMovieSceneBlendType::Absolute;

	EvalOptions.EnableAndSetCompletionMode
		(GetLinkerCustomVersion(FSequencerObjectVersion::GUID) < FSequencerObjectVersion::WhenFinishedDefaultsToProjectDefault ? 
			EMovieSceneCompletionMode::RestoreState : 
			EMovieSceneCompletionMode::ProjectDefault);

	SoundVolume.SetDefault(1.f);
	PitchMultiplier.SetDefault(1.f);
}

namespace MovieSceneAudioSectionPrivate
{
	template<typename ChannelType, typename ValueType>
	void AddInputChannels(UMovieSceneAudioSection* InSection, FMovieSceneChannelProxyData& InChannelProxyData)
	{
		InSection->ForEachInput([&InChannelProxyData](FName InName, const ChannelType& InChannel)
		{
#if WITH_EDITOR
			FMovieSceneChannelMetaData Data;
			FText TextName = FText::FromName(InName);	
			Data.SetIdentifiers(FName(InName.ToString() + GetGeneratedTypeName<ChannelType>()), TextName, TextName);
			InChannelProxyData.Add(const_cast<ChannelType&>(InChannel), Data, TMovieSceneExternalValue<ValueType>::Make());
#else //WITH_EDITOR
			InChannelProxyData.Add(const_cast<ChannelType&>(InChannel));
#endif //WITH_EDITOR

		});
	}
}

void UMovieSceneAudioSection::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	if (Ar.IsLoading())
	{
		CacheChannelProxy();
	}
}

void UMovieSceneAudioSection::PostEditImport()
{
	Super::PostEditImport();

	CacheChannelProxy();
}

EMovieSceneChannelProxyType  UMovieSceneAudioSection::CacheChannelProxy()
{
	// Set up the channel proxy
	FMovieSceneChannelProxyData Channels;

	UMovieSceneAudioTrack* AudioTrack = Cast<UMovieSceneAudioTrack>(GetOuter());

#if WITH_EDITOR

	FAudioChannelEditorData EditorData;
	Channels.Add(SoundVolume,     EditorData.Data[0], TMovieSceneExternalValue<float>());
	Channels.Add(PitchMultiplier, EditorData.Data[1], TMovieSceneExternalValue<float>());

	if (AudioTrack && AudioTrack->IsAMasterTrack())
	{
		Channels.Add(AttachActorData, EditorData.Data[2]);
	}

#else

	Channels.Add(SoundVolume);
	Channels.Add(PitchMultiplier);
	if (AudioTrack && AudioTrack->IsAMasterTrack())
	{
		Channels.Add(AttachActorData);
	}

#endif

	using namespace MovieSceneAudioSectionPrivate;
	SetupSoundInputParameters(GetSound());
	AddInputChannels<FMovieSceneFloatChannel, float>(this, Channels);
	AddInputChannels<FMovieSceneBoolChannel, bool>(this, Channels);
	AddInputChannels<FMovieSceneIntegerChannel, int32>(this, Channels);
	AddInputChannels<FMovieSceneStringChannel, FString>(this, Channels);
	AddInputChannels<FMovieSceneAudioTriggerChannel, bool>(this, Channels);

	ChannelProxy = MakeShared<FMovieSceneChannelProxy>(MoveTemp(Channels));

	return EMovieSceneChannelProxyType::Dynamic;
}

void UMovieSceneAudioSection::SetupSoundInputParameters(const USoundBase* InSoundBase)
{
	// Populate with defaults.
	if (InSoundBase)
	{
		TArray<FAudioParameter> DefaultParams;
		InSoundBase->GetAllDefaultParameters(DefaultParams);

		for (const FAudioParameter& Param : DefaultParams)
		{
			switch(Param.ParamType)
			{
			case EAudioParameterType::Float:
			{
				Inputs_Float.FindOrAdd(Param.ParamName, FMovieSceneFloatChannel{}).SetDefault(Param.FloatParam);
				break;
			}
			case EAudioParameterType::Boolean:
			{
				// Triggers are fundamentally just booleans outside of Metasound.
				static const FName TriggerName = FName(TEXT("Trigger")); // MOVE ME.
				if (Param.TypeName == TriggerName)
				{
					Inputs_Trigger.FindOrAdd(Param.ParamName, FMovieSceneAudioTriggerChannel{});
				}
				else
				{
					Inputs_Bool.FindOrAdd(Param.ParamName, FMovieSceneBoolChannel{}).SetDefault(Param.BoolParam);
				}
				break;
			}
			case EAudioParameterType::Integer:
			{
				Inputs_Int.FindOrAdd(Param.ParamName, FMovieSceneIntegerChannel{}).SetDefault(Param.IntParam);
				break;
			}
			case EAudioParameterType::String:
			{
				Inputs_String.FindOrAdd(Param.ParamName, FMovieSceneStringChannel{}).SetDefault(Param.StringParam);
				break;
			}
			default:
				// Not supported yet.
				break;
			}
		}
	}
}

TOptional<FFrameTime> UMovieSceneAudioSection::GetOffsetTime() const
{
	return TOptional<FFrameTime>(StartFrameOffset);
}

void UMovieSceneAudioSection::MigrateFrameTimes(FFrameRate SourceRate, FFrameRate DestinationRate)
{
	if (StartFrameOffset.Value > 0)
	{
		FFrameNumber NewStartFrameOffset = ConvertFrameTime(FFrameTime(StartFrameOffset), SourceRate, DestinationRate).FloorToFrame();
		StartFrameOffset = NewStartFrameOffset;
	}
}

void UMovieSceneAudioSection::PostLoad()
{
	Super::PostLoad();

	if (AudioDilationFactor_DEPRECATED != AudioDeprecatedMagicNumber)
	{
		PitchMultiplier.SetDefault(AudioDilationFactor_DEPRECATED);

		AudioDilationFactor_DEPRECATED = AudioDeprecatedMagicNumber;
	}

	if (AudioVolume_DEPRECATED != AudioDeprecatedMagicNumber)
	{
		SoundVolume.SetDefault(AudioVolume_DEPRECATED);

		AudioVolume_DEPRECATED = AudioDeprecatedMagicNumber;
	}

	TOptional<double> StartOffsetToUpgrade;
	if (AudioStartTime_DEPRECATED != AudioDeprecatedMagicNumber)
	{
		// Previously, start time in relation to the sequence. Start time was used to calculate the offset into the 
		// clip at the start of the section evaluation as such: Section Start Time - Start Time. 
		if (AudioStartTime_DEPRECATED != 0.f && HasStartFrame())
		{
			StartOffsetToUpgrade = GetInclusiveStartFrame() / GetTypedOuter<UMovieScene>()->GetTickResolution() - AudioStartTime_DEPRECATED;
		}
		AudioStartTime_DEPRECATED = AudioDeprecatedMagicNumber;
	}

	if (StartOffset_DEPRECATED != AudioDeprecatedMagicNumber)
	{
		StartOffsetToUpgrade = StartOffset_DEPRECATED;

		StartOffset_DEPRECATED = AudioDeprecatedMagicNumber;
	}

	FFrameRate LegacyFrameRate = GetLegacyConversionFrameRate();

	if (StartOffsetToUpgrade.IsSet())
	{
		FFrameRate DisplayRate = GetTypedOuter<UMovieScene>()->GetDisplayRate();
		FFrameRate TickResolution = GetTypedOuter<UMovieScene>()->GetTickResolution();

		StartFrameOffset = ConvertFrameTime(FFrameTime::FromDecimal(DisplayRate.AsDecimal() * StartOffsetToUpgrade.GetValue()), DisplayRate, TickResolution).FrameNumber;
	}
}
	
TOptional<TRange<FFrameNumber> > UMovieSceneAudioSection::GetAutoSizeRange() const
{
	if (!Sound)
	{
		return TRange<FFrameNumber>();
	}

	float SoundDuration = MovieSceneHelpers::GetSoundDuration(Sound);

	FFrameRate FrameRate = GetTypedOuter<UMovieScene>()->GetTickResolution();

	// determine initial duration
	// @todo Once we have infinite sections, we can remove this
	// @todo ^^ Why? Infinte sections would mean there's no starting time?
	FFrameTime DurationToUse = 1.f * FrameRate; // if all else fails, use 1 second duration

	if (SoundDuration != INDEFINITELY_LOOPING_DURATION)
	{
		DurationToUse = SoundDuration * FrameRate;
	}

	return TRange<FFrameNumber>(GetInclusiveStartFrame(), GetInclusiveStartFrame() + DurationToUse.FrameNumber);
}

	
void UMovieSceneAudioSection::TrimSection(FQualifiedFrameTime TrimTime, bool bTrimLeft, bool bDeleteKeys)
{
	SetFlags(RF_Transactional);

	if (TryModify())
	{
		if (bTrimLeft)
		{
			StartFrameOffset = HasStartFrame() ? GetStartOffsetAtTrimTime(TrimTime, StartFrameOffset, GetInclusiveStartFrame()) : 0;
		}

		Super::TrimSection(TrimTime, bTrimLeft, bDeleteKeys);
	}
}

UMovieSceneSection* UMovieSceneAudioSection::SplitSection(FQualifiedFrameTime SplitTime, bool bDeleteKeys)
{
	const FFrameNumber InitialStartFrameOffset = StartFrameOffset;

	const FFrameNumber NewOffset = HasStartFrame() ? GetStartOffsetAtTrimTime(SplitTime, StartFrameOffset, GetInclusiveStartFrame()) : 0;

	UMovieSceneSection* NewSection = Super::SplitSection(SplitTime, bDeleteKeys);
	if (NewSection != nullptr)
	{
		UMovieSceneAudioSection* NewAudioSection = Cast<UMovieSceneAudioSection>(NewSection);
		NewAudioSection->StartFrameOffset = NewOffset;
	}

	// Restore original offset modified by splitting
	StartFrameOffset = InitialStartFrameOffset;

	return NewSection;
}

void UMovieSceneAudioSection::SetSound(USoundBase* InSound)
{
	Sound = InSound;
	CacheChannelProxy();
}

USceneComponent* UMovieSceneAudioSection::GetAttachComponent(const AActor* InParentActor, const FMovieSceneActorReferenceKey& Key) const
{
	FName AttachComponentName = Key.ComponentName;
	FName AttachSocketName = Key.SocketName;

	if (AttachSocketName != NAME_None)
	{
		if (AttachComponentName != NAME_None)
		{
			TInlineComponentArray<USceneComponent*> PotentialAttachComponents(InParentActor);
			for (USceneComponent* PotentialAttachComponent : PotentialAttachComponents)
			{
				if (PotentialAttachComponent->GetFName() == AttachComponentName && PotentialAttachComponent->DoesSocketExist(AttachSocketName))
				{
					return PotentialAttachComponent;
				}
			}
		}
		else if (InParentActor->GetRootComponent()->DoesSocketExist(AttachSocketName))
		{
			return InParentActor->GetRootComponent();
		}
	}
	else if (AttachComponentName != NAME_None)
	{
		TInlineComponentArray<USceneComponent*> PotentialAttachComponents(InParentActor);
		for (USceneComponent* PotentialAttachComponent : PotentialAttachComponents)
		{
			if (PotentialAttachComponent->GetFName() == AttachComponentName)
			{
				return PotentialAttachComponent;
			}
		}
	}

	if (InParentActor->GetDefaultAttachComponent())
	{
		return InParentActor->GetDefaultAttachComponent();
	}
	else
	{
		return InParentActor->GetRootComponent();
	}
}


