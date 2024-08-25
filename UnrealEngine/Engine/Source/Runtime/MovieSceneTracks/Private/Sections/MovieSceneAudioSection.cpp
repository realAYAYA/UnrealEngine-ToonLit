// Copyright Epic Games, Inc. All Rights Reserved.

#include "Sections/MovieSceneAudioSection.h"
#include "Tracks/MovieSceneAudioTrack.h"
#include "Sound/SoundBase.h"
#include "UObject/SequencerObjectVersion.h"
#include "Channels/MovieSceneChannelProxy.h"
#include "GameFramework/Actor.h"
#include "MovieScene.h"
#include "MovieSceneCommonHelpers.h"
#include "Misc/FrameRate.h"
#include "Misc/GeneratedTypeName.h"
#include "EntitySystem/BuiltInComponentTypes.h"
#include "MovieSceneTracksComponentTypes.h"

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

namespace UE::MovieScene
{

/* 
 * Entity IDs are an encoded type and index, with the upper bit being the type (scalar iputs vs audio trigger),
 * and the lower 31 bits as the entity index
 */
enum class EAudioSectionEntityType : uint8 { MainEntity, InputsEntity, TriggerEntity };

uint32 EncodeEntityID(int32 InIndex, EAudioSectionEntityType InEntityType)
{
	check(InIndex >= 0 && InIndex < int32(0x00FFFFFF));
	return static_cast<uint32>(InIndex) | ((uint8)InEntityType << 24);
}
void DecodeEntityID(uint32 InEntityID, int32& OutIndex, EAudioSectionEntityType& OutEntityType)
{
	OutIndex = static_cast<int32>(InEntityID & 0x00FFFFFF);
	OutEntityType = (EAudioSectionEntityType)(InEntityID >> 24);
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
	UMovieScene* MovieScene = AudioTrack ? Cast<UMovieScene>(AudioTrack->GetOuter()) : nullptr;
	const bool bHasAttachData = MovieScene && MovieScene->ContainsTrack(*AudioTrack);

#if WITH_EDITOR

	FAudioChannelEditorData EditorData;
	Channels.Add(SoundVolume,     EditorData.Data[0], TMovieSceneExternalValue<float>());
	Channels.Add(PitchMultiplier, EditorData.Data[1], TMovieSceneExternalValue<float>());

	if (bHasAttachData)
	{
		Channels.Add(AttachActorData, EditorData.Data[2]);
	}

#else

	Channels.Add(SoundVolume);
	Channels.Add(PitchMultiplier);
	if (bHasAttachData)
	{
		Channels.Add(AttachActorData);
	}

#endif

	using namespace MovieSceneAudioSectionPrivate;
	SetupSoundInputParameters(Sound);
	AddInputChannels<FMovieSceneFloatChannel, float>(this, Channels);
	AddInputChannels<FMovieSceneBoolChannel, bool>(this, Channels);
	AddInputChannels<FMovieSceneIntegerChannel, int32>(this, Channels);
	AddInputChannels<FMovieSceneStringChannel, FString>(this, Channels);
	AddInputChannels<FMovieSceneAudioTriggerChannel, bool>(this, Channels);

	ChannelProxy = MakeShared<FMovieSceneChannelProxy>(MoveTemp(Channels));

	return EMovieSceneChannelProxyType::Dynamic;
}

void UMovieSceneAudioSection::SetupSoundInputParameters(USoundBase* InSoundBase)
{
	// Populate with defaults.

	// Don't init resources when running cook, as this can trigger 
	// registration of a MetaSound and its dependent graphs.
	// Those will instead be registered when the MetaSound itself is cooked (FMetasoundAssetBase::CookMetaSound)
	// in a way that does not deal with runtime data like this function does
	// Getting the default parameters and the rest of the function are 
	// dependent on that runtime data and don't need to be cooked
	if (InSoundBase && !IsRunningCookCommandlet())
	{
		InSoundBase->InitResources();

		TArray<FAudioParameter> DefaultParams;
		InSoundBase->GetAllDefaultParameters(DefaultParams);

		TSet<FName> OrphanedFloatInputs;
		Inputs_Float.GetKeys(OrphanedFloatInputs);
		TSet<FName> OrphanedTriggerInputs;
		Inputs_Trigger.GetKeys(OrphanedTriggerInputs);
		TSet<FName> OrphanedBoolInputs;
		Inputs_Bool.GetKeys(OrphanedBoolInputs);
		TSet<FName> OrphanedIntInputs;
		Inputs_Int.GetKeys(OrphanedIntInputs);
		TSet<FName> OrphanedStringInputs;
		Inputs_String.GetKeys(OrphanedStringInputs);

		for (const FAudioParameter& Param : DefaultParams)
		{
			switch (Param.ParamType)
			{
			case EAudioParameterType::Float:
			{
				Inputs_Float.FindOrAdd(Param.ParamName, FMovieSceneFloatChannel{}).SetDefault(Param.FloatParam);
				OrphanedFloatInputs.Remove(Param.ParamName);
				break;
			}
			case EAudioParameterType::Trigger:
			{
				Inputs_Trigger.FindOrAdd(Param.ParamName, FMovieSceneAudioTriggerChannel{});
				OrphanedTriggerInputs.Remove(Param.ParamName);
				break;
			}
			case EAudioParameterType::Boolean:
			{
				Inputs_Bool.FindOrAdd(Param.ParamName, FMovieSceneBoolChannel{}).SetDefault(Param.BoolParam);
				OrphanedBoolInputs.Remove(Param.ParamName);
				break;
			}
			case EAudioParameterType::Integer:
			{
				Inputs_Int.FindOrAdd(Param.ParamName, FMovieSceneIntegerChannel{}).SetDefault(Param.IntParam);
				OrphanedIntInputs.Remove(Param.ParamName);
				break;
			}
			case EAudioParameterType::String:
			{
				Inputs_String.FindOrAdd(Param.ParamName, FMovieSceneStringChannel{}).SetDefault(Param.StringParam);
				OrphanedStringInputs.Remove(Param.ParamName);
				break;
			}
			default:
				// Not supported yet.
				break;
			}
		}

		for (const FName& Name : OrphanedFloatInputs)
		{
			Inputs_Float.Remove(Name);
		}
		for (const FName& Name : OrphanedTriggerInputs)
		{
			Inputs_Trigger.Remove(Name);
		}
		for (const FName& Name : OrphanedBoolInputs)
		{
			Inputs_Bool.Remove(Name);
		}
		for (const FName& Name : OrphanedIntInputs)
		{
			Inputs_Int.Remove(Name);
		}
		for (const FName& Name : OrphanedStringInputs)
		{
			Inputs_String.Remove(Name);
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

	CacheChannelProxy();

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

	const float SoundDuration = MovieSceneHelpers::GetSoundDuration(Sound);

	const FFrameRate FrameRate = GetTypedOuter<UMovieScene>()->GetTickResolution();

	// determine initial duration
	// @todo Once we have infinite sections, we can remove this
	// @todo ^^ Why? Infinte sections would mean there's no starting time?
	FFrameTime DurationToUse = 1.f * FrameRate; // if all else fails, use 1 second duration

	if (SoundDuration != INDEFINITELY_LOOPING_DURATION && SoundDuration > 0)
	{
		DurationToUse = FMath::Max(SoundDuration * FrameRate - StartFrameOffset, FFrameTime(1));
	}

	const int32 IFrameNumber = DurationToUse.FrameNumber.Value + (int)(DurationToUse.GetSubFrame() + 0.5f);
	return TRange<FFrameNumber>(GetInclusiveStartFrame(), GetInclusiveStartFrame() + IFrameNumber);
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

bool UMovieSceneAudioSection::PopulateEvaluationFieldImpl(const TRange<FFrameNumber>& EffectiveRange, const FMovieSceneEvaluationFieldEntityMetaData& InMetaData, FMovieSceneEntityComponentFieldBuilder* OutFieldBuilder)
{
	using namespace UE::MovieScene;

	int32 MetaDataIndex = OutFieldBuilder->AddMetaData(InMetaData);

	// Add the default entity first.
	int32 MainEntityIndex = OutFieldBuilder->FindOrAddEntity(this, EncodeEntityID(0, EAudioSectionEntityType::MainEntity));
	OutFieldBuilder->AddPersistentEntity(EffectiveRange, MainEntityIndex, MetaDataIndex);

	// See how many additional entities we need to store the audio input data.
	// We can pack 9 float channels per entity, but we can only have one string/bool/int/audio-trigger
	// channel per entity.
	int32 NumInputDataEntities = Inputs_Float.Num() % 9;
	NumInputDataEntities = FMath::Max(NumInputDataEntities, Inputs_String.Num());
	NumInputDataEntities = FMath::Max(NumInputDataEntities, Inputs_Bool.Num());
	NumInputDataEntities = FMath::Max(NumInputDataEntities, Inputs_Int.Num());

	// Add these extra entities to the evaluation field.
	for (int32 InputDataEntity = 0; InputDataEntity < NumInputDataEntities; ++InputDataEntity)
	{
		int32 EntityIndex = OutFieldBuilder->FindOrAddEntity(this, EncodeEntityID(InputDataEntity, EAudioSectionEntityType::InputsEntity));
		OutFieldBuilder->AddPersistentEntity(EffectiveRange, EntityIndex, MetaDataIndex);
	}

	// Audio triggers are added differently, as one-shot entities.
	TArray<FName> AudioTriggerNames;
	Inputs_Trigger.GetKeys(AudioTriggerNames);
	AudioTriggerNames.Sort(FNameLexicalLess());
	for (int32 TriggerIndex = 0; TriggerIndex < AudioTriggerNames.Num(); ++TriggerIndex)
	{
		FMovieSceneAudioTriggerChannel& TriggerChannel = Inputs_Trigger[AudioTriggerNames[TriggerIndex]];
		TArrayView<const FFrameNumber> Times = TriggerChannel.GetTimes();
		for (int32 Index = 0; Index < Times.Num(); ++Index)
		{
			if (EffectiveRange.Contains(Times[Index]))
			{
				TRange<FFrameNumber> TriggerRange(Times[Index]);
				int32 EntityIndex = OutFieldBuilder->FindOrAddEntity(this, EncodeEntityID(TriggerIndex, EAudioSectionEntityType::TriggerEntity));
				OutFieldBuilder->AddOneShotEntity(TriggerRange, EntityIndex, MetaDataIndex);
			}
		}
	}

	// Return true to indicate we've done everything ourselves.
	return true;
}

void UMovieSceneAudioSection::ImportEntityImpl(UMovieSceneEntitySystemLinker* EntityLinker, const FEntityImportParams& Params, FImportedEntity* OutImportedEntity)
{
	using namespace UE::MovieScene;

	if (!Sound)
	{
		return;
	}

	const FBuiltInComponentTypes* BuiltInComponents = FBuiltInComponentTypes::Get();
	const FMovieSceneTracksComponentTypes* TrackComponents = FMovieSceneTracksComponentTypes::Get();

	const FGuid ObjectBindingID = Params.GetObjectBindingID();

	int32 EntityIndex;
	EAudioSectionEntityType EntityType;
	DecodeEntityID(Params.EntityID, EntityIndex, EntityType);

	if (EntityType == EAudioSectionEntityType::MainEntity)
	{
		// Default entity... we add the main audio component data, plus the volume and pitch channels.
		OutImportedEntity->AddBuilder(
			FEntityBuilder()
			.AddConditional(BuiltInComponents->GenericObjectBinding, ObjectBindingID, ObjectBindingID.IsValid())
			.AddTagConditional(BuiltInComponents->Tags.Root, !ObjectBindingID.IsValid())
			.Add(TrackComponents->Audio, FMovieSceneAudioComponentData{ this })
			.Add(BuiltInComponents->FloatChannel[0], &SoundVolume)
			.Add(BuiltInComponents->FloatChannel[1], &PitchMultiplier)
		);
	}
	else if (EntityType == EAudioSectionEntityType::InputsEntity)
	{
		// Additional entities for custom audio input values.
		TArray<FName> InputNames;
		FMovieSceneAudioInputData InputData;

		// There are up to 9 float channels per entity, and we know that all entities are fully packed
		// until the last one. So add as many as we can for the given entity we're building.
		int32 FloatInputStartIndex = EntityIndex * 9;
		if (FloatInputStartIndex < Inputs_Float.Num())
		{
			int32 FloatInputNum = FMath::Min(9, Inputs_Float.Num() - FloatInputStartIndex);
			Inputs_Float.GetKeys(InputNames);
			for (int32 Offset = 0; Offset < FloatInputNum; ++Offset)
			{
				InputData.FloatInputs[Offset] = InputNames[FloatInputStartIndex + Offset];
			}
		}

		// Other inputs can only be added once per entity, so add one of each type that exists.
		int32 OtherInputStartIndex = EntityIndex;
		if (OtherInputStartIndex < Inputs_String.Num())
		{
			InputNames.Reset();
			Inputs_String.GetKeys(InputNames);
			InputData.StringInput = InputNames[OtherInputStartIndex];
		}
		if (OtherInputStartIndex < Inputs_Bool.Num())
		{
			InputNames.Reset();
			Inputs_Bool.GetKeys(InputNames);
			InputData.BoolInput = InputNames[OtherInputStartIndex];
		}
		if (OtherInputStartIndex < Inputs_Int.Num())
		{
			InputNames.Reset();
			Inputs_Int.GetKeys(InputNames);
			InputData.IntInput = InputNames[OtherInputStartIndex];
		}

		// Make this additional entity by adding the component that specifies what audio input channels
		// are present, plus all of these channels.
		OutImportedEntity->AddBuilder(
			FEntityBuilder()
			.AddConditional(BuiltInComponents->GenericObjectBinding, ObjectBindingID, ObjectBindingID.IsValid())
			.AddTagConditional(BuiltInComponents->Tags.Root, !ObjectBindingID.IsValid())
			.Add(TrackComponents->Audio, FMovieSceneAudioComponentData{ this })
			.Add(TrackComponents->AudioInputs, InputData)
		);
		for (int32 Index = 0; Index < 9; ++Index)
		{
			FName InputName = InputData.FloatInputs[Index];
			if (!InputName.IsNone())
			{
				OutImportedEntity->AddBuilder(
					FEntityBuilder()
					.Add(BuiltInComponents->FloatChannel[Index], &Inputs_Float[InputName])
				);
			}
		}
		if (!InputData.StringInput.IsNone())
		{
			OutImportedEntity->AddBuilder(
				FEntityBuilder()
				.Add(BuiltInComponents->StringChannel, &Inputs_String[InputData.StringInput])
			);
		}
		if (!InputData.BoolInput.IsNone())
		{
			OutImportedEntity->AddBuilder(
				FEntityBuilder()
				.Add(BuiltInComponents->BoolChannel, &Inputs_Bool[InputData.BoolInput])
			);
		}
		if (!InputData.IntInput.IsNone())
		{
			OutImportedEntity->AddBuilder(
				FEntityBuilder()
				.Add(BuiltInComponents->IntegerChannel, &Inputs_Int[InputData.IntInput])
			);
		}
	}
	else if (EntityType == EAudioSectionEntityType::TriggerEntity)
	{
		// Additional one-shot entities for audio triggers.
		// The decoded index is the index of the name in the triggers map.
		TArray<FName> AudioTriggerNames;
		Inputs_Trigger.GetKeys(AudioTriggerNames);
		AudioTriggerNames.Sort(FNameLexicalLess());
		OutImportedEntity->AddBuilder(
			FEntityBuilder()
			.AddConditional(BuiltInComponents->GenericObjectBinding, ObjectBindingID, ObjectBindingID.IsValid())
			.AddTagConditional(BuiltInComponents->Tags.Root, !ObjectBindingID.IsValid())
			.Add(TrackComponents->Audio, FMovieSceneAudioComponentData{ this })
			.Add(TrackComponents->AudioTriggerName, AudioTriggerNames[EntityIndex])
		);
	}
}

#if WITH_EDITOR

void UMovieSceneAudioSection::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	CacheChannelProxy();
}

#endif
