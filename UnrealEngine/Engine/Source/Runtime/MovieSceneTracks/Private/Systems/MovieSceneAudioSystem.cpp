// Copyright Epic Games, Inc. All Rights Reserved.

#include "Systems/MovieSceneAudioSystem.h"

#include "AudioDevice.h"
#include "Components/AudioComponent.h"
#include "Engine/Engine.h"
#include "Engine/EngineTypes.h"
#include "EntitySystem/BuiltInComponentTypes.h"
#include "Evaluation/PreAnimatedState/MovieScenePreAnimatedObjectStorage.h"
#include "Evaluation/PreAnimatedState/MovieScenePreAnimatedStateStorage.h"
#include "Evaluation/PreAnimatedState/MovieScenePreAnimatedStorageID.inl"
#include "GameFramework/WorldSettings.h"
#include "IMovieScenePlayer.h"
#include "MovieScene.h"
#include "MovieSceneTracksComponentTypes.h"
#include "Sections/MovieSceneAudioSection.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundCue.h"
#include "Tracks/MovieSceneAudioTrack.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MovieSceneAudioSystem)

DECLARE_CYCLE_STAT(TEXT("Audio System Evaluate"), MovieSceneEval_AudioTasks, STATGROUP_MovieSceneECS);

static float MaxSequenceAudioDesyncToleranceCVar = 0.5f;
FAutoConsoleVariableRef CVarMaxSequenceAudioDesyncTolerance(
	TEXT("Sequencer.Audio.MaxDesyncTolerance"),
	MaxSequenceAudioDesyncToleranceCVar,
	TEXT("Controls how many seconds an audio track can be out of sync in a Sequence before we attempt a time correction.\n"),
	ECVF_Default);

static bool bIgnoreAudioSyncDuringWorldTimeDilationCVar = true;
FAutoConsoleVariableRef CVarIgnoreAudioSyncDuringWorldTimeDilation(
	TEXT("Sequencer.Audio.IgnoreAudioSyncDuringWorldTimeDilation"),
	bIgnoreAudioSyncDuringWorldTimeDilationCVar,
	TEXT("Ignore correcting audio if there is world time dilation.\n"),
	ECVF_Default);

static int32 UseAudioClockForSequencerDesyncCVar = 0;
FAutoConsoleVariableRef CVaUseAudioClockForSequencerDesync(
	TEXT("Sequencer.Audio.UseAudioClockForAudioDesync"),
	UseAudioClockForSequencerDesyncCVar,
	TEXT("When set to 1, we will use the audio render thread directly to query whether audio has went out of sync with the sequence.\n"),
	ECVF_Default);

static bool bPlayAudioWhenPlaybackJumps = false;
FAutoConsoleVariableRef CVarPlayAudioWhenPlaybackJumps(
	TEXT("Sequencer.Audio.PlayAudioWhenPlaybackJumps"),
	bPlayAudioWhenPlaybackJumps,
	TEXT("Play audio when playback jumps.\n"),
	ECVF_Default);

namespace UE::MovieScene
{

enum class EPreAnimatedAudioStateType
{
	/** Pre-animated state manages the lifespan of the audio component */
	AudioComponentLifespan,
	/** Pre-animated state manages whether the audio component is playing */
	AudioPlaying
};

template<typename BaseTraits>
struct FPreAnimatedAudioStateTraits : BaseTraits
{
	using KeyType = FObjectKey;
	using StorageType = EPreAnimatedAudioStateType;

	EPreAnimatedAudioStateType CachePreAnimatedValue(FObjectKey InKey)
	{
		check(false);
		return EPreAnimatedAudioStateType::AudioComponentLifespan;
	}

	void RestorePreAnimatedValue(FObjectKey InKey, EPreAnimatedAudioStateType InStateType, const FRestoreStateParams& Params)
	{
		if (UAudioComponent* AudioComponent = Cast<UAudioComponent>(InKey.ResolveObjectPtr()))
		{
			switch (InStateType)
			{
				case EPreAnimatedAudioStateType::AudioPlaying:
					AudioComponent->Stop();
					break;
				case EPreAnimatedAudioStateType::AudioComponentLifespan:
					AudioComponent->DestroyComponent();
					break;
			}
		}
	}
};

using FPreAnimatedBoundObjectAudioStateTraits = FPreAnimatedAudioStateTraits<FBoundObjectPreAnimatedStateTraits>;

struct FPreAnimatedAudioStorage : TPreAnimatedStateStorage_ObjectTraits<FPreAnimatedBoundObjectAudioStateTraits>
{
	static TAutoRegisterPreAnimatedStorageID<FPreAnimatedAudioStorage> StorageID;
};
TAutoRegisterPreAnimatedStorageID<FPreAnimatedAudioStorage> FPreAnimatedAudioStorage::StorageID;

/**
 * Types of audio evaluation we should run for a given sequence.
 */
enum class EAudioEvaluationType
{
	Skip,
	Play,
	StopAndPlay,
	Stop
};

struct FGatherAudioInputs
{
	using FAudioInputsBySectionKey = UMovieSceneAudioSystem::FAudioInputsBySectionKey;
	using FAudioComponentInputEvaluationData = UMovieSceneAudioSystem::FAudioComponentInputEvaluationData;

	UMovieSceneAudioSystem* AudioSystem;

	FGatherAudioInputs(UMovieSceneAudioSystem* InAudioSystem)
		: AudioSystem(InAudioSystem)
	{
	}

	void ForEachAllocation(
		const FEntityAllocation* Allocation,
		TRead<FMovieSceneAudioComponentData> AudioDatas,
		TRead<FMovieSceneAudioInputData> AudioInputDatas,
		TReadOneOrMoreOf<
				double, double, double,
				double, double, double,
				double, double, double,
				FString, int32, bool>
			AudioInputResults) const
	{
		FAudioInputsBySectionKey& AudioInputsBySectionKey = AudioSystem->AudioInputsBySectionKey;

		const double* DoubleResults[9];
		{
			DoubleResults[0] = AudioInputResults.Get<0>();
			DoubleResults[1] = AudioInputResults.Get<1>();
			DoubleResults[2] = AudioInputResults.Get<2>();
			DoubleResults[3] = AudioInputResults.Get<3>();
			DoubleResults[4] = AudioInputResults.Get<4>();
			DoubleResults[5] = AudioInputResults.Get<5>();
			DoubleResults[6] = AudioInputResults.Get<6>();
			DoubleResults[7] = AudioInputResults.Get<7>();
			DoubleResults[8] = AudioInputResults.Get<8>();
		}
		const FString* StringResults = AudioInputResults.Get<9>();
		const int32* IntegerResults = AudioInputResults.Get<10>();
		const bool* BoolResults = AudioInputResults.Get<11>();

		for (int32 Index = 0; Index < Allocation->Num(); ++Index)
		{
			const FMovieSceneAudioComponentData& AudioData = AudioDatas[Index];
			const FMovieSceneAudioInputData& AudioInputNames = AudioInputDatas[Index];

			FObjectKey SectionKey(AudioData.Section);
			FAudioComponentInputEvaluationData& AudioInputValues = AudioInputsBySectionKey.FindOrAdd(SectionKey);
			
			// Gather float inputs.
			for (int32 FloatIndex = 0; FloatIndex < 9; ++FloatIndex)
			{
				if (!AudioInputNames.FloatInputs[FloatIndex].IsNone() && ensure(DoubleResults[FloatIndex]))
				{
					AudioInputValues.Inputs_Float.Add(AudioInputNames.FloatInputs[FloatIndex], DoubleResults[FloatIndex][Index]);
				}
			}

			// Gather string inputs.
			if (!AudioInputNames.StringInput.IsNone() && ensure(StringResults))
			{
				AudioInputValues.Inputs_String.Add(AudioInputNames.StringInput, StringResults[Index]);
			}

			// Gather integer inputs.
			if (!AudioInputNames.IntInput.IsNone() && ensure(IntegerResults))
			{
				AudioInputValues.Inputs_Int.Add(AudioInputNames.IntInput, IntegerResults[Index]);
			}

			// Gather boolean inputs.
			if (!AudioInputNames.BoolInput.IsNone() && ensure(BoolResults))
			{
				AudioInputValues.Inputs_Bool.Add(AudioInputNames.BoolInput, BoolResults[Index]);
			}
		}
	}
};

struct FGatherAudioTriggers
{
	using FAudioInputsBySectionKey = UMovieSceneAudioSystem::FAudioInputsBySectionKey;
	using FAudioComponentInputEvaluationData = UMovieSceneAudioSystem::FAudioComponentInputEvaluationData;

	UMovieSceneAudioSystem* AudioSystem;

	FGatherAudioTriggers(UMovieSceneAudioSystem* InAudioSystem)
		: AudioSystem(InAudioSystem)
	{
	}

	void ForEachAllocation(
		const FEntityAllocation* Allocation,
		TRead<FMovieSceneAudioComponentData> AudioDatas,
		TRead<FName> AudioTriggerNames) const
	{
		FAudioInputsBySectionKey& AudioInputsBySectionKey = AudioSystem->AudioInputsBySectionKey;

		for (int32 Index = 0; Index < Allocation->Num(); ++Index)
		{
			const FMovieSceneAudioComponentData& AudioData = AudioDatas[Index];
			const FName& AudioTriggerName = AudioTriggerNames[Index];

			FObjectKey SectionKey(AudioData.Section);
			FAudioComponentInputEvaluationData& AudioInputValues = AudioInputsBySectionKey.FindOrAdd(SectionKey);

			AudioInputValues.Inputs_Trigger.Add(AudioTriggerName);
		}
	}
};

struct FEvaluateAudio
{
	static EAudioEvaluationType GetAudioEvaluationType(const FMovieSceneContext& Context)
	{
		if (Context.GetStatus() == EMovieScenePlayerStatus::Jumping &&
				!bPlayAudioWhenPlaybackJumps)
		{
			return EAudioEvaluationType::Skip;
		}

		if (Context.HasJumped())
		{
			// If the status says we jumped, we always stop all sounds, then allow them to be played again 
			// naturally if status == Playing (for example)
			return EAudioEvaluationType::StopAndPlay;
		}

		if (
			(Context.GetStatus() != EMovieScenePlayerStatus::Playing && 
			 Context.GetStatus() != EMovieScenePlayerStatus::Scrubbing && 
			 Context.GetStatus() != EMovieScenePlayerStatus::Stepping)
			||
			Context.GetDirection() == EPlayDirection::Backwards)
		{
			// stopped, recording, etc
			return EAudioEvaluationType::Stop;
		}

		return EAudioEvaluationType::Play;
	}

	UMovieSceneAudioSystem* AudioSystem;
	const FInstanceRegistry* InstanceRegistry;

	FEvaluateAudio(UMovieSceneAudioSystem* InAudioSystem)
		: AudioSystem(InAudioSystem)
	{
		InstanceRegistry = AudioSystem->GetLinker()->GetInstanceRegistry();
	}

	void ForEachAllocation(
			const FEntityAllocation* Allocation, 
			TRead<FMovieSceneEntityID> EntityIDs, 
			TRead<FRootInstanceHandle> RootInstanceHandles,
			TRead<FInstanceHandle> InstanceHandles,
			TRead<FMovieSceneAudioComponentData> AudioDatas,
			TRead<double> VolumeMultipliers,
			TRead<double> PitchMultipliers,
			TReadOptional<UObject*> BoundObjects) const
	{
		const FBuiltInComponentTypes* BuiltInComponents = FBuiltInComponentTypes::Get();

		const int32 Num = Allocation->Num();
		const bool bWantsRestoreState = Allocation->HasComponent(BuiltInComponents->Tags.RestoreState);

		for (int32 Index = 0; Index < Num; ++Index)
		{
			const FMovieSceneEntityID& EntityID = EntityIDs[Index];
			const FRootInstanceHandle& RootInstanceHandle = RootInstanceHandles[Index];
			const FInstanceHandle& InstanceHandle = InstanceHandles[Index];
			const FMovieSceneAudioComponentData& AudioData = AudioDatas[Index];

			const FSequenceInstance& Instance = InstanceRegistry->GetInstance(InstanceHandle);

			double VolumeMultiplier = VolumeMultipliers[Index];
			double PitchMultiplier = PitchMultipliers[Index];
			UObject* BoundObject = (BoundObjects.IsValid() ? BoundObjects[Index] : nullptr);

			Evaluate(EntityID, AudioData, Instance, RootInstanceHandle, VolumeMultiplier, PitchMultiplier, BoundObject, bWantsRestoreState);
		}
	}

private:

	void Evaluate(
			const FMovieSceneEntityID& EntityID,
			const FMovieSceneAudioComponentData& AudioData,
			const FSequenceInstance& Instance,
			const FRootInstanceHandle& RootInstanceHandle,
			double VolumeMultiplier,
			double PitchMultiplier,
			UObject* BoundObject,
			bool bWantsRestoreState) const
	{
		const FMovieSceneContext& Context = Instance.GetContext();
		IMovieScenePlayer* Player = Instance.GetPlayer();

		UMovieSceneAudioSection* AudioSection = AudioData.Section;
		if (!ensureMsgf(AudioSection, TEXT("No valid audio section found in audio track component data!")))
		{
			return;
		}

		FObjectKey ActorKey(BoundObject);
		FObjectKey SectionKey(AudioSection);

		const EAudioEvaluationType EvalType = GetAudioEvaluationType(Context);
		if (EvalType == EAudioEvaluationType::StopAndPlay)
		{
			AudioSystem->StopSound(ActorKey, AudioData.Section);
		}
		else if (EvalType == EAudioEvaluationType::Stop)
		{
			AudioSystem->StopSound(ActorKey, AudioData.Section);
			return;
		}
		else if (EvalType == EAudioEvaluationType::Skip)
		{
			return;
		}

		// Root audio track
		if (BoundObject == nullptr)
		{
			UObject* PlaybackContext = Player->GetPlaybackContext();

			const FMovieSceneActorReferenceData& AttachActorData = AudioSection->GetAttachActorData();

			USceneComponent* AttachComponent = nullptr;
			FMovieSceneActorReferenceKey AttachKey;
			AttachActorData.Evaluate(Context.GetTime(), AttachKey);
			FMovieSceneObjectBindingID AttachBindingID = AttachKey.Object;
			if (AttachBindingID.IsValid())
			{
				// If the transform is set, otherwise use the bound actor's transform
				for (TWeakObjectPtr<> WeakObject : AttachBindingID.ResolveBoundObjects(Instance.GetSequenceID(), *Player))
				{
					AActor* AttachActor = Cast<AActor>(WeakObject.Get());
					if (AttachActor)
					{
						AttachComponent = AudioSection->GetAttachComponent(AttachActor, AttachKey);
					}
					if (AttachComponent)
					{
						break;
					}
				}
			}

			FAudioComponentEvaluationData* EvaluationData = AudioSystem->GetAudioComponentEvaluationData(FObjectKey(), SectionKey);
			if (!EvaluationData)
			{
				// Initialize the sound
				UWorld* World = PlaybackContext ? PlaybackContext->GetWorld() : nullptr;
				EvaluationData = AudioSystem->AddRootAudioComponent(AudioSection, World);
				UAudioComponent* AudioComponent = EvaluationData ? EvaluationData->AudioComponent.Get() : nullptr;

				if (ensure(AudioComponent))
				{
					AudioSystem->PreAnimatedStorage->BeginTrackingEntity(EntityID, bWantsRestoreState, RootInstanceHandle, AudioComponent);
					AudioSystem->PreAnimatedStorage->CachePreAnimatedValue(
						FCachePreAnimatedValueParams(), AudioComponent,
						[](FObjectKey InKey) { return EPreAnimatedAudioStateType::AudioComponentLifespan; });

					if (AudioSection->GetOnQueueSubtitles().IsBound())
					{
						AudioComponent->OnQueueSubtitles = AudioSection->GetOnQueueSubtitles();
					}
					if (AudioSection->GetOnAudioFinished().IsBound())
					{
						AudioComponent->OnAudioFinished = AudioSection->GetOnAudioFinished();
					}
					if (AudioSection->GetOnAudioPlaybackPercent().IsBound())
					{
						AudioComponent->OnAudioPlaybackPercent = AudioSection->GetOnAudioPlaybackPercent();
					}
				}
			}

			if (EvaluationData)
			{
				UAudioComponent* AudioComponent = EvaluationData->AudioComponent.Get();

				if (AudioComponent && AttachComponent && 
					(AudioComponent->GetAttachParent() != AttachComponent || AudioComponent->GetAttachSocketName() != AttachKey.SocketName))
				{
					AudioComponent->AttachToComponent(AttachComponent, FAttachmentTransformRules::KeepRelativeTransform, AttachKey.SocketName);
				}

				EvaluationData->VolumeMultiplier = VolumeMultiplier * AudioSection->EvaluateEasing(Context.GetTime());
				EvaluationData->PitchMultiplier = PitchMultiplier;

				EnsureAudioIsPlaying(nullptr, *AudioSection, *EvaluationData, Context, *Player);
			}
		}

		// Object binding audio track
		else
		{
			FAudioComponentEvaluationData* EvaluationData = AudioSystem->GetAudioComponentEvaluationData(ActorKey, SectionKey);
			if (!EvaluationData)
			{
				// Initialize the sound
				EvaluationData = AudioSystem->AddBoundObjectAudioComponent(AudioSection, BoundObject);
				UAudioComponent* AudioComponent = EvaluationData ? EvaluationData->AudioComponent.Get() : nullptr;

				if (AudioComponent)
				{
					AudioSystem->PreAnimatedStorage->BeginTrackingEntity(EntityID, bWantsRestoreState, RootInstanceHandle, AudioComponent);
					AudioSystem->PreAnimatedStorage->CachePreAnimatedValue(
						FCachePreAnimatedValueParams(), AudioComponent,
						[](FObjectKey InKey) { return EPreAnimatedAudioStateType::AudioComponentLifespan; });

					if (AudioSection->GetOnQueueSubtitles().IsBound())
					{
						AudioComponent->OnQueueSubtitles = AudioSection->GetOnQueueSubtitles();
					}
					if (AudioSection->GetOnAudioFinished().IsBound())
					{
						AudioComponent->OnAudioFinished = AudioSection->GetOnAudioFinished();
					}
					if (AudioSection->GetOnAudioPlaybackPercent().IsBound())
					{
						AudioComponent->OnAudioPlaybackPercent = AudioSection->GetOnAudioPlaybackPercent();
					}
				}
			}

			if (EvaluationData)
			{
				EvaluationData->VolumeMultiplier = VolumeMultiplier;
				EvaluationData->PitchMultiplier = PitchMultiplier;

				EnsureAudioIsPlaying(BoundObject, *AudioSection, *EvaluationData, Context, *Player);
			}
		}
	}

	void EnsureAudioIsPlaying(
			UObject* BoundObject,
			UMovieSceneAudioSection& AudioSection,
			FAudioComponentEvaluationData& EvaluationData,
			const FMovieSceneContext& Context, 
			IMovieScenePlayer& Player) const
	{
		using FAudioInputsBySectionKey = UMovieSceneAudioSystem::FAudioInputsBySectionKey;
		using FAudioComponentInputEvaluationData = UMovieSceneAudioSystem::FAudioComponentInputEvaluationData;

		ensureMsgf(EvaluationData.AudioComponent.IsValid(), TEXT("Trying to evaluate audio track on an invalid audio component"));
		UAudioComponent& AudioComponent = *EvaluationData.AudioComponent.Get();

		AudioSystem->PreAnimatedStorage->CachePreAnimatedValue(
				FCachePreAnimatedValueParams(), &AudioComponent,
				[](FObjectKey InKey) { return EPreAnimatedAudioStateType::AudioPlaying; });

		if (AudioComponent.VolumeMultiplier != EvaluationData.VolumeMultiplier)
		{
			AudioComponent.SetVolumeMultiplier(EvaluationData.VolumeMultiplier);
		}

		if (AudioComponent.PitchMultiplier != EvaluationData.PitchMultiplier)
		{
			AudioComponent.SetPitchMultiplier(EvaluationData.PitchMultiplier);
		}

		AudioComponent.bSuppressSubtitles = AudioSection.GetSuppressSubtitles();

		// Allow spatialization if we have any object we've been attached to.
		const bool bAllowSpatialization = (BoundObject != nullptr || AudioComponent.GetAttachParent() != nullptr);

		// Apply the input params.
		FAudioInputsBySectionKey& AudioInputsBySectionKey = AudioSystem->AudioInputsBySectionKey;
		FObjectKey SectionKey(&AudioSection);
		FAudioComponentInputEvaluationData* AudioInputs = AudioInputsBySectionKey.Find(SectionKey);
		if (AudioInputs)
		{
			SetAudioInputParameters(AudioInputs->Inputs_Float, AudioComponent);
			SetAudioInputParameters(AudioInputs->Inputs_String, AudioComponent);
			SetAudioInputParameters(AudioInputs->Inputs_Bool, AudioComponent);
			SetAudioInputParameters(AudioInputs->Inputs_Int, AudioComponent);
		}

		FFrameNumber SectionStartFrame = (AudioSection.HasStartFrame() ? AudioSection.GetInclusiveStartFrame() : 0);
		float SectionStartTimeSeconds = SectionStartFrame / AudioSection.GetTypedOuter<UMovieScene>()->GetTickResolution();

		const FFrameNumber AudioStartOffset = AudioSection.GetStartOffset();
		USoundBase* Sound = AudioSection.GetSound();

		float AudioTime = (Context.GetTime() / Context.GetFrameRate()) 
			- SectionStartTimeSeconds 
			+ (float)Context.GetFrameRate().AsSeconds(AudioStartOffset);
		if (AudioTime >= 0.f && Sound)
		{
			const float Duration = MovieSceneHelpers::GetSoundDuration(Sound);

			if (!AudioSection.GetLooping() && AudioTime > Duration && Duration != 0.f)
			{
				AudioComponent.Stop();
				return;
			}

			AudioTime = Duration > 0.f ? FMath::Fmod(AudioTime, Duration) : AudioTime;
		}

		// If the audio component is not playing we (may) need a state change. If the audio component is playing
		// the wrong sound then we need a state change. If the audio playback time is significantly out of sync 
		// with the desired time then we need a state change.
		bool bSoundNeedsStateChange = !AudioComponent.IsPlaying() || AudioComponent.Sound != Sound;
		bool bSoundNeedsTimeSync = false;

		UObject* PlaybackContext = Player.GetPlaybackContext();
		UWorld* World = PlaybackContext ? PlaybackContext->GetWorld() : nullptr;

		// Sync only if there is no time dilation because otherwise the system will constantly resync because audio 
		// playback is not dilated and will never match the expected playback time.
		const bool bDoTimeSync = 
			World && World->GetWorldSettings() &&
			(FMath::IsNearlyEqual(World->GetWorldSettings()->GetEffectiveTimeDilation(), 1.f) ||
			 !bIgnoreAudioSyncDuringWorldTimeDilationCVar);

		if (bDoTimeSync)
		{
			float CurrentGameTime = 0.0f;

			FAudioDevice* AudioDevice = World ? World->GetAudioDeviceRaw() : nullptr;
			if (UseAudioClockForSequencerDesyncCVar && AudioDevice)
			{
				CurrentGameTime = AudioDevice->GetAudioClock();
			}
			else
			{
				CurrentGameTime = World ? World->GetAudioTimeSeconds() : 0.f;
			}

			// This tells us how much time has passed in the game world (and thus, reasonably, the audio playback)
			// so if we calculate that we should be playing say, 15s into the section during evaluation, but
			// we're only 5s since the last Play call, then we know we're out of sync. 
			if (EvaluationData.PartialDesyncComputation.IsSet())
			{
				const float PartialDesyncComputation = EvaluationData.PartialDesyncComputation.GetValue();
				float Desync = PartialDesyncComputation + AudioTime - CurrentGameTime;

				if (!FMath::IsNearlyZero(MaxSequenceAudioDesyncToleranceCVar) && FMath::Abs(Desync) > MaxSequenceAudioDesyncToleranceCVar)
				{
					UE_LOG(LogMovieScene, Verbose, TEXT("Audio Component detected a significant mismatch in (assumed) playback time versus the desired time. Desync: %6.2f(s) Desired Time: %6.2f(s). Component: %s sound: %s"), Desync, AudioTime, *AudioComponent.GetName(), *GetNameSafe(AudioComponent.Sound));
					bSoundNeedsTimeSync = true;
				}
			}
		}

		if (bSoundNeedsStateChange || bSoundNeedsTimeSync)
		{
			AudioComponent.bAllowSpatialization = bAllowSpatialization;

			if (AudioSection.GetOverrideAttenuation())
			{
				AudioComponent.AttenuationSettings = AudioSection.GetAttenuationSettings();
			}

			// Only call stop on the sound if it is actually playing. This prevents spamming
			// stop calls when a sound cue with a duration of zero is played.
			if (AudioComponent.IsPlaying() || bSoundNeedsTimeSync)
			{
				UE_LOG(LogMovieScene, Verbose, TEXT("Audio Component stopped due to needing a state change bIsPlaying: %d bNeedsTimeSync: %d. Component: %s sound: %s"), AudioComponent.IsPlaying(), bSoundNeedsTimeSync, *AudioComponent.GetName(), *GetNameSafe(AudioComponent.Sound));
				AudioComponent.Stop();
			}

			// Only change the sound clip if it has actually changed. This calls Stop internally if needed.
			if (AudioComponent.Sound != Sound)
			{
				UE_LOG(LogMovieScene, Verbose, TEXT("Audio Component calling SetSound due to new sound. Component: %s OldSound: %s NewSound: %s"), *AudioComponent.GetName(), *GetNameSafe(AudioComponent.Sound), *GetNameSafe(Sound));
				AudioComponent.SetSound(Sound);
			}
#if WITH_EDITOR
			if (GIsEditor && World != nullptr && !World->IsPlayInEditor())
			{
				AudioComponent.bIsUISound = true;
				AudioComponent.bIsPreviewSound = true;
			}
			else
#endif // WITH_EDITOR
			{
				AudioComponent.bIsUISound = false;
			}

			if (AudioTime >= 0.f)
			{
				UE_LOG(LogMovieScene, Verbose, TEXT("Audio Component Play at Local Time: %6.2f CurrentTime: %6.2f(s) SectionStart: %6.2f(s), SoundDur: %6.2f OffsetIntoClip: %6.2f sound: %s"), AudioTime, (Context.GetTime() / Context.GetFrameRate()), SectionStartTimeSeconds, AudioComponent.Sound ? AudioComponent.Sound->GetDuration() : 0.0f, (float)Context.GetFrameRate().AsSeconds(AudioStartOffset), *GetNameSafe(AudioComponent.Sound));
				AudioComponent.Play(AudioTime);

				// Keep track of when we asked this audio clip to play (in game time) so that we can figure 
				// out if there's a significant desync in the future.
				//
				// The goal is later to compare:
				//   (NewAudioTime - PreviousAudioTime) and 
				//   (NewGameTime - PreviousGameTime)
				//
				// If their difference is larger than some threshold, we have a desync. NewGameTime and 
				// NewAudioTime will be known next update, but PreviousGameTime and PreviousAudioTime
				// are known now. Let's store (-PreviousAudioTime + PreviousGameTime), and we will only 
				// need to add (NewAudioTime - NewGameTime).
				if (World)
				{
					FAudioDevice* AudioDevice = World->GetAudioDeviceRaw();
					if (UseAudioClockForSequencerDesyncCVar && AudioDevice)
					{
						EvaluationData.PartialDesyncComputation = AudioDevice->GetAudioClock() - AudioTime;
					}
					else
					{
						EvaluationData.PartialDesyncComputation = World->GetAudioTimeSeconds() - AudioTime;
					}
				}
			}
		}

		if (Context.GetStatus() != EMovieScenePlayerStatus::Playing)
		{
			float ScrubDuration = AudioTrackConstants::ScrubDuration;
			if (FAudioDevice* AudioDevice = AudioComponent.GetAudioDevice())
			{
				constexpr float MinScrubFrameRateFactor = 1.5f;
				float DeviceDeltaTime = AudioDevice->GetGameDeltaTime();

				// When operating at very low frame-rates (<20fps), a single frame will be
				// longer than the hard coded scrub duration of 50ms in which case the delayed
				// stop will trigger on the same frame that the sound starts playing and
				// no audio will be heard. Here we increase the scrub duration to be greater than
				// a single frame if necessary.
				ScrubDuration = FMath::Max(ScrubDuration, DeviceDeltaTime * MinScrubFrameRateFactor);
			}

			// While scrubbing, play the sound for a short time and then cut it.
			AudioComponent.StopDelayed(ScrubDuration);
		}

		if (AudioComponent.IsPlaying() && AudioInputs)
		{
			SetAudioInputTriggers(AudioInputs->Inputs_Trigger, AudioComponent);
		}

		if (bAllowSpatialization)
		{
			if (FAudioDevice* AudioDevice = AudioComponent.GetAudioDevice())
			{
				DECLARE_CYCLE_STAT(TEXT("FAudioThreadTask.MovieSceneUpdateAudioTransform"), STAT_MovieSceneUpdateAudioTransform, STATGROUP_TaskGraphTasks);
				AudioDevice->SendCommandToActiveSounds(AudioComponent.GetAudioComponentID(), [ActorTransform = AudioComponent.GetComponentTransform()](FActiveSound& ActiveSound)
				{
					ActiveSound.bLocationDefined = true;
					ActiveSound.Transform = ActorTransform;
				}, GET_STATID(STAT_MovieSceneUpdateAudioTransform));
			}
		}
	}

	// Helper method for firing all triggered audio triggers.
	void SetAudioInputTriggers(const TArray<FName>& Inputs, IAudioParameterControllerInterface& InParamaterInterface) const
	{
		for (const FName& TriggerName : Inputs)
		{
			InParamaterInterface.SetTriggerParameter(TriggerName);
		};
	}

	// Helper template to set all audio input values previously evaluated.
	template<typename ValueType>
	void SetAudioInputParameters(TMap<FName, ValueType>& Inputs, IAudioParameterControllerInterface& InParamaterInterface) const
	{
		for (TPair<FName, ValueType>& Pair : Inputs)
		{
			InParamaterInterface.SetParameter<ValueType>(Pair.Key, MoveTempIfPossible(Pair.Value));
		};
	}
};

} // namespace UE::MovieScene

UMovieSceneAudioSystem::UMovieSceneAudioSystem(const FObjectInitializer& ObjInit)
	: UMovieSceneEntitySystem(ObjInit)
{
	using namespace UE::MovieScene;

	RelevantComponent = FMovieSceneTracksComponentTypes::Get()->Audio;
	Phase = ESystemPhase::Scheduling;

	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		const FBuiltInComponentTypes* BuiltInComponents = FBuiltInComponentTypes::Get();
		const FMovieSceneTracksComponentTypes* TrackComponents = FMovieSceneTracksComponentTypes::Get();

		// We consume the result of all possible audio input channels.
		for (int32 Index = 0; Index < 9; ++Index)
		{
			DefineComponentConsumer(GetClass(), BuiltInComponents->DoubleResult[Index]);
		}
		DefineComponentConsumer(GetClass(), BuiltInComponents->StringResult);
		DefineComponentConsumer(GetClass(), BuiltInComponents->IntegerResult);
		DefineComponentConsumer(GetClass(), BuiltInComponents->BoolResult);
		DefineComponentConsumer(GetClass(), TrackComponents->AudioTriggerName);
	}
}

void UMovieSceneAudioSystem::OnLink()
{
	using namespace UE::MovieScene;

	PreAnimatedStorage = Linker->PreAnimatedState.GetOrCreateStorage<FPreAnimatedAudioStorage>();
}

void UMovieSceneAudioSystem::OnUnlink()
{
	using namespace UE::MovieScene;

	for (const TPair<FObjectKey, FAudioComponentBySectionKey>& AudioComponentsForActor : AudioComponentsByActorKey)
	{
		for (const TPair<FObjectKey, FAudioComponentEvaluationData>& AudioComponentForSection : AudioComponentsForActor.Value)
		{
			UAudioComponent* AudioComponent = AudioComponentForSection.Value.AudioComponent.Get();
			if (AudioComponent)
			{
				UObject* Actor = AudioComponentsForActor.Key.ResolveObjectPtr();
				UObject* Section = AudioComponentForSection.Key.ResolveObjectPtr();
				UE_LOG(LogMovieScene, Warning, TEXT("Cleaning audio component '%s' for section '%s' on actor '%s'"),
						*AudioComponent->GetPathName(),
						Section ? *Section->GetPathName() : TEXT("<null>"),
						Actor ? *Actor->GetPathName() : TEXT("<null>"));
			}
		}
	}

	AudioComponentsByActorKey.Reset();
	AudioInputsBySectionKey.Reset();

}

void UMovieSceneAudioSystem::ResetSharedData()
{
	AudioInputsBySectionKey.Reset();
	for (TPair<FObjectKey, FAudioComponentBySectionKey>& AudioComponentsForActor : AudioComponentsByActorKey)
	{
		for (TPair<FObjectKey, FAudioComponentEvaluationData>& AudioComponentForSection : AudioComponentsForActor.Value)
		{
			AudioComponentForSection.Value.bEvaluatedThisFrame = false;
		}
	}
}

void UMovieSceneAudioSystem::OnSchedulePersistentTasks(UE::MovieScene::IEntitySystemScheduler* TaskScheduler)
{
	if (!GEngine || !GEngine->UseSound())
	{
		return;
	}

	using namespace UE::MovieScene;

	const FBuiltInComponentTypes* BuiltInComponents = FBuiltInComponentTypes::Get();
	const FMovieSceneTracksComponentTypes* TrackComponents = FMovieSceneTracksComponentTypes::Get();

	// Reset shared data.
	FTaskID ResetSharedDataTask = TaskScheduler->AddMemberFunctionTask(FTaskParams(TEXT("Reset Audio Data")), this, &UMovieSceneAudioSystem::ResetSharedData);

	// Gather audio input values computed by the channel evaluators.
	FTaskID GatherInputsTask = FEntityTaskBuilder()
	.Read(TrackComponents->Audio)
	.Read(TrackComponents->AudioInputs)
	.ReadOneOrMoreOf(
			BuiltInComponents->DoubleResult[0], BuiltInComponents->DoubleResult[1], BuiltInComponents->DoubleResult[2], 
			BuiltInComponents->DoubleResult[3], BuiltInComponents->DoubleResult[4], BuiltInComponents->DoubleResult[5], 
			BuiltInComponents->DoubleResult[6], BuiltInComponents->DoubleResult[7], BuiltInComponents->DoubleResult[8], 
			BuiltInComponents->StringResult,
			BuiltInComponents->IntegerResult,
			BuiltInComponents->BoolResult)
	.Schedule_PerAllocation<FGatherAudioInputs>(&Linker->EntityManager, TaskScheduler, this);

	TaskScheduler->AddPrerequisite(ResetSharedDataTask, GatherInputsTask);

	// Gather up audio triggers
	FTaskID GatherTriggersTask = FEntityTaskBuilder()
	.Read(TrackComponents->Audio)
	.Read(TrackComponents->AudioTriggerName)
	.Schedule_PerAllocation<FGatherAudioTriggers>(&Linker->EntityManager, TaskScheduler, this);

	TaskScheduler->AddPrerequisite(ResetSharedDataTask, GatherTriggersTask);

	// Next, evaluate audio to play and use the gathered audio input values to set on the audio components.
	FTaskID EvaluateAudioTask = FEntityTaskBuilder()
	.ReadEntityIDs()
	.Read(BuiltInComponents->RootInstanceHandle)
	.Read(BuiltInComponents->InstanceHandle)
	.Read(TrackComponents->Audio)
	.Read(BuiltInComponents->DoubleResult[0]) // Volume
	.Read(BuiltInComponents->DoubleResult[1]) // Pitch multiplier
	.ReadOptional(BuiltInComponents->BoundObject)
	.SetDesiredThread(Linker->EntityManager.GetGatherThread())
	.Schedule_PerAllocation<FEvaluateAudio>(&Linker->EntityManager, TaskScheduler, this);

	TaskScheduler->AddPrerequisite(GatherInputsTask, EvaluateAudioTask);
	TaskScheduler->AddPrerequisite(GatherTriggersTask, EvaluateAudioTask);
	TaskScheduler->AddPrerequisite(ResetSharedDataTask, EvaluateAudioTask);
}

void UMovieSceneAudioSystem::OnRun(FSystemTaskPrerequisites& InPrerequisites, FSystemSubsequentTasks& Subsequents)
{
	MOVIESCENE_DETAILED_SCOPE_CYCLE_COUNTER(MovieSceneEval_AudioTrack_Evaluate)

	if (!GEngine || !GEngine->UseSound())
	{
		return;
	}

	using namespace UE::MovieScene;

	const FBuiltInComponentTypes* BuiltInComponents = FBuiltInComponentTypes::Get();
	const FMovieSceneTracksComponentTypes* TrackComponents = FMovieSceneTracksComponentTypes::Get();

	// Reset shared data.
	ResetSharedData();

	// Gather audio input values computed by the channel evaluators.
	FSystemTaskPrerequisites Prereqs;

	FGraphEventRef Task = FEntityTaskBuilder()
	.Read(TrackComponents->Audio)
	.Read(TrackComponents->AudioInputs)
	.ReadOneOrMoreOf(
			BuiltInComponents->DoubleResult[0], BuiltInComponents->DoubleResult[1], BuiltInComponents->DoubleResult[2], 
			BuiltInComponents->DoubleResult[3], BuiltInComponents->DoubleResult[4], BuiltInComponents->DoubleResult[5], 
			BuiltInComponents->DoubleResult[6], BuiltInComponents->DoubleResult[7], BuiltInComponents->DoubleResult[8], 
			BuiltInComponents->StringResult,
			BuiltInComponents->IntegerResult,
			BuiltInComponents->BoolResult)
	.template Dispatch_PerAllocation<FGatherAudioInputs>(&Linker->EntityManager, InPrerequisites, nullptr, this);
	if (Task)
	{
		Prereqs.AddRootTask(Task);
	}

	Task = FEntityTaskBuilder()
	.Read(TrackComponents->Audio)
	.Read(TrackComponents->AudioTriggerName)
	.template Dispatch_PerAllocation<FGatherAudioTriggers>(&Linker->EntityManager, InPrerequisites, nullptr, this);
	if (Task)
	{
		Prereqs.AddRootTask(Task);
	}

	// Next, evaluate audio to play and use the gathered audio input values to set on the audio components.
	FEntityTaskBuilder()
	.ReadEntityIDs()
	.Read(BuiltInComponents->RootInstanceHandle)
	.Read(BuiltInComponents->InstanceHandle)
	.Read(TrackComponents->Audio)
	.Read(BuiltInComponents->DoubleResult[0]) // Volume
	.Read(BuiltInComponents->DoubleResult[1]) // Pitch multiplier
	.ReadOptional(BuiltInComponents->BoundObject)
	.SetDesiredThread(Linker->EntityManager.GetGatherThread())
	.template Dispatch_PerAllocation<FEvaluateAudio>(&Linker->EntityManager, Prereqs, &Subsequents, this);
}

UMovieSceneAudioSystem::FAudioComponentEvaluationData* UMovieSceneAudioSystem::GetAudioComponentEvaluationData(FObjectKey ActorKey, FObjectKey SectionKey)
{
	FAudioComponentBySectionKey* Map = AudioComponentsByActorKey.Find(ActorKey);
	if (Map != nullptr)
	{
		// First, check for an exact match for this entity
		FAudioComponentEvaluationData* ExistingData = Map->Find(SectionKey);
		if (ExistingData != nullptr)
		{
			if (ExistingData->AudioComponent.IsValid())
			{
				return ExistingData;
			}
		}

		// If no exact match, check for any AudioComponent that isn't busy
		for (FAudioComponentBySectionKey::ElementType& Pair : *Map)
		{
			UAudioComponent* ExistingComponent = Pair.Value.AudioComponent.Get();
			if (ExistingComponent && !ExistingComponent->IsPlaying())
			{
				// Replace this entry with the new entity ID to claim it
				FAudioComponentEvaluationData MovedData(Pair.Value);
				Map->Remove(Pair.Key);
				MovedData.PartialDesyncComputation.Reset();
				return &Map->Add(SectionKey, MovedData);
			}
		}
	}

	return nullptr;
}

UMovieSceneAudioSystem::FAudioComponentEvaluationData* UMovieSceneAudioSystem::AddBoundObjectAudioComponent(UMovieSceneAudioSection* Section, UObject* PrincipalObject)
{
	using namespace UE::MovieScene;

	FObjectKey ObjectKey(PrincipalObject);
	FObjectKey SectionKey(Section);

	FAudioComponentBySectionKey& ActorAudioComponentMap = AudioComponentsByActorKey.FindOrAdd(ObjectKey);

	FAudioComponentEvaluationData* ExistingData = GetAudioComponentEvaluationData(ObjectKey, SectionKey);
	if (!ExistingData)
	{
		USoundCue* TempPlaybackAudioCue = NewObject<USoundCue>();

		AActor* Actor = nullptr;
		USceneComponent* SceneComponent = nullptr;
		FString ObjectName;

		if (PrincipalObject->IsA<AActor>())
		{
			Actor = Cast<AActor>(PrincipalObject);
			SceneComponent = Actor->GetRootComponent();
			ObjectName =
#if WITH_EDITOR
				Actor->GetActorLabel();
#else
				Actor->GetName();
#endif
		}
		else if (PrincipalObject->IsA<UActorComponent>())
		{
			UActorComponent* ActorComponent = Cast<UActorComponent>(PrincipalObject);
			Actor = ActorComponent->GetOwner();
			SceneComponent = Cast<USceneComponent>(ActorComponent);
			ObjectName = ActorComponent->GetName();
		}

		if (!Actor || !SceneComponent)
		{
			const int32 RowIndex = Section->GetRowIndex();
			UE_LOG(LogMovieScene, Warning, TEXT("Failed to find scene component for spatialized audio track (row %d)."), RowIndex);
			return nullptr;
		}

		FAudioDevice::FCreateComponentParams Params(Actor->GetWorld(), Actor);
		UAudioComponent* NewComponent = FAudioDevice::CreateComponent(TempPlaybackAudioCue, Params);

		if (!NewComponent)
		{
			const int32 RowIndex = Section->GetRowIndex();
			UE_LOG(LogMovieScene, Warning, TEXT("Failed to create audio component for spatialized audio track (row %d on %s)."), RowIndex, *ObjectName);
			return nullptr;
		}

		NewComponent->SetFlags(RF_Transient);
		NewComponent->AttachToComponent(SceneComponent, FAttachmentTransformRules::KeepRelativeTransform);

		ExistingData = &ActorAudioComponentMap.Add(SectionKey);
		ExistingData->AudioComponent = NewComponent;
	}

	return ExistingData;
}

UMovieSceneAudioSystem::FAudioComponentEvaluationData* UMovieSceneAudioSystem::AddRootAudioComponent(UMovieSceneAudioSection* Section, UWorld* World)
{
	using namespace UE::MovieScene;

	FObjectKey NullKey;
	FObjectKey SectionKey(Section);

	FAudioComponentBySectionKey& RootAudioComponentMap = AudioComponentsByActorKey.FindOrAdd(NullKey);

	FAudioComponentEvaluationData* ExistingData = GetAudioComponentEvaluationData(NullKey, SectionKey);
	if (!ExistingData)
	{
		USoundCue* TempPlaybackAudioCue = NewObject<USoundCue>();

		FAudioDevice::FCreateComponentParams Params(World);

		UAudioComponent* NewComponent = FAudioDevice::CreateComponent(TempPlaybackAudioCue, Params);

		if (!NewComponent)
		{
			const int32 RowIndex = Section->GetRowIndex();
			UE_LOG(LogMovieScene, Warning, TEXT("Failed to create audio component for root audio track (row %d)."), RowIndex);
			return nullptr;
		}

		NewComponent->SetFlags(RF_Transient);

		ExistingData = &RootAudioComponentMap.Add(SectionKey);
		ExistingData->AudioComponent = NewComponent;
	}

	return ExistingData;
}

void UMovieSceneAudioSystem::StopSound(FObjectKey ActorKey, FObjectKey SectionKey)
{
	if (FAudioComponentBySectionKey* Map = AudioComponentsByActorKey.Find(ActorKey))
	{
		if (FAudioComponentEvaluationData* Data = Map->Find(SectionKey))
		{
			if (UAudioComponent* AudioComponent = Data->AudioComponent.Get())
			{
				AudioComponent->Stop();
			}
		}
	}
}

