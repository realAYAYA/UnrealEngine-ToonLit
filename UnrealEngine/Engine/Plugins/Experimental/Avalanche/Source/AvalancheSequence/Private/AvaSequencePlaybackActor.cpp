// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaSequencePlaybackActor.h"
#include "AvaSequencePlayer.h"
#include "AvaSequenceSubsystem.h"
#include "Engine/World.h"
#include "IAvaSequenceProvider.h"
#include "MovieSceneSequenceTickManager.h"
#include "UObject/UObjectThreadContext.h"

DEFINE_LOG_CATEGORY_STATIC(LogAvaSequencePlayback, Log, All);

AAvaSequencePlaybackActor::AAvaSequencePlaybackActor()
{
	// Tick Settings that follow through FMovieSceneSequenceTickInterval
	PrimaryActorTick.TickInterval = 0.f;
	PrimaryActorTick.bTickEvenWhenPaused = true;

	UAvaSequencePlayer::OnSequenceFinished().AddUObject(this, &AAvaSequencePlaybackActor::OnSequenceFinished);
}

void AAvaSequencePlaybackActor::SetSequenceProvider(IAvaSequenceProvider& InSequenceProvider)
{
	SequenceProvider.SetObject(InSequenceProvider.ToUObject());
	SequenceProvider.SetInterface(&InSequenceProvider);
}

void AAvaSequencePlaybackActor::CleanupPlayers()
{
	// "OnSequenceFinished" is called while cleaning up a player which as a result removes the players from these sets.
	// To prevent removing element one by one and while iterating these sets, these sets are moved to temp set.

	TSet<TObjectPtr<UAvaSequencePlayer>> SequencePlayers;
	SequencePlayers.Reserve(StoppedSequencePlayers.Num() + ActiveSequencePlayers.Num());

	SequencePlayers.Append(MoveTemp(StoppedSequencePlayers));
	SequencePlayers.Append(MoveTemp(ActiveSequencePlayers));

	StoppedSequencePlayers.Reset();
	ActiveSequencePlayers.Reset();

	for (UAvaSequencePlayer* Player : SequencePlayers)
	{
		if (Player)
		{
			Player->Cleanup();
		}
	}
}

UAvaSequencePlayer* AAvaSequencePlaybackActor::PlaySequence(UAvaSequence* InSequence, const FAvaSequencePlayParams& InPlaySettings)
{
	if (!IsValid(InSequence))
	{
		return nullptr;
	}

	if (UAvaSequencePlayer* const Player = GetOrAddSequencePlayer(InSequence))
	{
		Player->SetPlaySettings(InPlaySettings);
		Player->PlaySequence();

		UE_LOG(LogAvaSequencePlayback, Verbose
			, TEXT("Playing Sequence '%s' ('%s') with Player '%s'")
			, *InSequence->GetLabel().ToString()
			, *InSequence->GetName()
			, *Player->GetName());

		return Player;
	}

	UE_LOG(LogAvaSequencePlayback, Warning
		, TEXT("Failed to play Sequence '%s' ('%s'). It was valid, but a Player could not be allocated.")
		, *InSequence->GetLabel().ToString()
		, *InSequence->GetName());

	return nullptr;
}

UAvaSequencePlayer* AAvaSequencePlaybackActor::PlaySequenceBySoftReference(TSoftObjectPtr<UAvaSequence> InSequence, FAvaSequencePlayParams InPlaySettings)
{
	if (UAvaSequence* const ResolvedSequence = InSequence.Get())
	{
		return PlaySequence(ResolvedSequence, InPlaySettings);
	}

	UE_LOG(LogAvaSequencePlayback, Warning
		, TEXT("Failed to play sequence '%s'. It is invalid, or not yet loaded")
		, *InSequence.ToString());

	return nullptr;
}

TArray<UAvaSequencePlayer*> AAvaSequencePlaybackActor::PlaySequencesByLabel(FName InSequenceLabel, FAvaSequencePlayParams InPlaySettings)
{
	return PlaySequencesByLabels({ InSequenceLabel }, MoveTemp(InPlaySettings));
}

TArray<UAvaSequencePlayer*> AAvaSequencePlaybackActor::PlaySequencesBySoftReference(const TArray<TSoftObjectPtr<UAvaSequence>>& InSequences, FAvaSequencePlayParams InPlaySettings)
{
	TArray<UAvaSequencePlayer*> Players;
	Players.Reserve(InSequences.Num());

	for (const TSoftObjectPtr<UAvaSequence>& Sequence : InSequences)
	{
		Players.Add(PlaySequenceBySoftReference(Sequence, InPlaySettings));
	}

	return Players;
}

TArray<UAvaSequencePlayer*> AAvaSequencePlaybackActor::PlaySequencesByTag(const FAvaTag& InTag, bool bInExactMatch, FAvaSequencePlayParams InPlaySettings)
{
	if (!InTag.IsValid())
	{
		return TArray<UAvaSequencePlayer*>();
	}

	const TArray<UAvaSequence*> SequencesToPlay = GetSequencesByTag(InTag, bInExactMatch);

	TArray<UAvaSequencePlayer*> SequencePlayers;
	SequencePlayers.Reserve(SequencesToPlay.Num());

	for (UAvaSequence* Sequence : SequencesToPlay)
	{
		if (UAvaSequencePlayer* SequencePlayer = PlaySequence(Sequence, InPlaySettings))
		{
			SequencePlayers.Add(SequencePlayer);	
		}
	}

	return SequencePlayers;
}

TArray<UAvaSequencePlayer*> AAvaSequencePlaybackActor::PlaySequencesByLabels(const TArray<FName>& InSequenceLabels, FAvaSequencePlayParams InPlaySettings)
{
	if (InSequenceLabels.IsEmpty())
	{
		return TArray<UAvaSequencePlayer*>();
	}

	const TArray<UAvaSequence*> SequencesToPlay = GetSequencesByLabel(InSequenceLabels);

	TArray<UAvaSequencePlayer*> SequencePlayers;
	SequencePlayers.Reserve(SequencesToPlay.Num());

	for (UAvaSequence* Sequence : SequencesToPlay)
	{
		SequencePlayers.Add(PlaySequence(Sequence, InPlaySettings));
	}

	return SequencePlayers;
}

TArray<UAvaSequencePlayer*> AAvaSequencePlaybackActor::PlayScheduledSequences()
{
	return PlaySequencesByLabels(ScheduledSequenceNames, ScheduledPlaySettings);
}

UAvaSequencePlayer* AAvaSequencePlaybackActor::ContinueSequence(UAvaSequence* InSequence)
{
	if (UAvaSequencePlayer* const Player = GetSequencePlayer(InSequence))
	{
		Player->ContinueSequence();
		return Player;
	}
	return nullptr;
}

TArray<UAvaSequencePlayer*> AAvaSequencePlaybackActor::ContinueSequencesByLabel(FName InSequenceLabel)
{
	return ContinueSequencesByLabels({ InSequenceLabel });
}

TArray<UAvaSequencePlayer*> AAvaSequencePlaybackActor::ContinueSequencesByTag(const FAvaTag& InTag, bool bInExactMatch)
{
	TArray<UAvaSequencePlayer*> SequencePlayers;
	
	const TArray<UAvaSequence*> SequencesToContinue = GetSequencesByTag(InTag, bInExactMatch);
	for (UAvaSequence* Sequence : SequencesToContinue)
	{
		if (UAvaSequencePlayer* Player = ContinueSequence(Sequence))
		{
			SequencePlayers.Add(Player);
		}
	}

	return SequencePlayers;
}

TArray<UAvaSequencePlayer*> AAvaSequencePlaybackActor::ContinueSequencesByLabels(const TArray<FName>& InSequenceLabels)
{
	TArray<UAvaSequencePlayer*> SequencePlayers;
	SequencePlayers.Reserve(InSequenceLabels.Num());

	const TArray<UAvaSequence*> SequencesToContinue = GetSequencesByLabel(InSequenceLabels);
	for (UAvaSequence* Sequence : SequencesToContinue)
	{
		SequencePlayers.Add(ContinueSequence(Sequence));
	}

	return SequencePlayers;
}

void AAvaSequencePlaybackActor::StopSequence(UAvaSequence* InSequence)
{
	if (UAvaSequencePlayer* const Player = GetSequencePlayer(InSequence))
	{
		Player->Stop();
	}
}

void AAvaSequencePlaybackActor::UpdateCameraCut(const UE::MovieScene::FOnCameraCutUpdatedParams& InCameraCutParams)
{
	OnCameraCut.Broadcast(InCameraCutParams.ViewTarget, InCameraCutParams.bIsJumpCut);
}

void AAvaSequencePlaybackActor::OnSequenceFinished(UAvaSequencePlayer* InPlayer, UAvaSequence* InSequence)
{
	if (ActiveSequencePlayers.Remove(InPlayer) > 0)
	{
		StoppedSequencePlayers.Add(InPlayer);
	}
}

const TArray<UAvaSequence*>& AAvaSequencePlaybackActor::GetAllSequences() const
{
	IAvaSequenceProvider* SequenceProviderInterface = SequenceProvider.GetInterface();

	if (!SequenceProviderInterface)
	{
		UE_LOG(LogAvaSequencePlayback, Warning
			, TEXT("Failed to get any sequences by name. Sequence Provider is invalid in playback actor %s")
			, *GetName());

		static const TArray<UAvaSequence*> EmptySequences;
		return EmptySequences;
	}

	const TArray<TObjectPtr<UAvaSequence>>& AllSequences = SequenceProviderInterface->GetSequences();

	if (AllSequences.IsEmpty())
	{
		UE_LOG(LogAvaSequencePlayback, Warning
			, TEXT("Failed to get sequences. Sequence Provider '%s' returned an empty list of playable sequences")
			, *SequenceProviderInterface->GetSequenceProviderDebugName().ToString());
		return AllSequences;
	}

	return AllSequences;
}

TArray<UAvaSequence*> AAvaSequencePlaybackActor::GetSequencesByLabel(TConstArrayView<FName> InSequenceLabels) const
{
	if (InSequenceLabels.IsEmpty())
	{
		UE_LOG(LogAvaSequencePlayback, Verbose
			, TEXT("Input Sequence Names Array was empty in playback actor %s")
			, *GetName());
		return TArray<UAvaSequence*>();
	}

	const TArray<UAvaSequence*>& AllSequences = GetAllSequences();
	if (AllSequences.IsEmpty())
	{
		return TArray<UAvaSequence*>();
	}

	TMap<FName, TArray<UAvaSequence*>> SequenceLabelMap;
	SequenceLabelMap.Reserve(AllSequences.Num());

	for (UAvaSequence* Sequence : AllSequences)
	{
		if (Sequence)
		{
			SequenceLabelMap.FindOrAdd(Sequence->GetLabel()).Add(Sequence);
		}
	}

	TArray<UAvaSequence*> OutSequences;
	OutSequences.Reserve(InSequenceLabels.Num());

	for (FName SequenceLabel : InSequenceLabels)
	{
		if (TArray<UAvaSequence*>* FoundSequences = SequenceLabelMap.Find(SequenceLabel))
		{
			OutSequences.Append(*FoundSequences);
		}
		else
		{
			UE_LOG(LogAvaSequencePlayback, Warning
				, TEXT("Failed to find '%s', as a valid sequence with such label could not be found in the Sequence Provider list of playable sequences.")
				, *SequenceLabel.ToString());
		}
	}

	return OutSequences;
}

TArray<UAvaSequence*> AAvaSequencePlaybackActor::GetSequencesByTag(const FAvaTag& InTag, bool bInExactMatch) const
{
	if (!InTag.IsValid())
	{
		UE_LOG(LogAvaSequencePlayback, Verbose
			, TEXT("Input Tag was empty in playback actor %s")
			, *GetName());
		return TArray<UAvaSequence*>();
	}

	const TArray<UAvaSequence*>& AllSequences = GetAllSequences();
	if (AllSequences.IsEmpty())
	{
		return TArray<UAvaSequence*>();
	}

	// Worst case: All Sequences match the tag
	TArray<UAvaSequence*> OutSequences;
	OutSequences.Reserve(AllSequences.Num());

	for (UAvaSequence* Sequence : AllSequences)
	{
		if (Sequence && Sequence->GetSequenceTag() == InTag)
		{
			OutSequences.Add(Sequence);
		}
	}

	return OutSequences;
}

void AAvaSequencePlaybackActor::BP_PlayScheduledSequences()
{
	PlayScheduledSequences();
}

UObject* AAvaSequencePlaybackActor::GetPlaybackContext() const
{
	return GetWorld();
}

UObject* AAvaSequencePlaybackActor::CreateDirectorInstance(IMovieScenePlayer& InPlayer, FMovieSceneSequenceID InSequenceID)
{
	return this;
}

UAvaSequencePlayer* AAvaSequencePlaybackActor::GetSequencePlayer(const UAvaSequence* InSequence) const
{
	if (!InSequence)
	{
		return nullptr;
	}

	for (UAvaSequencePlayer* ActivePlayer : ActiveSequencePlayers)
	{
		if (ActivePlayer && ActivePlayer->GetSequence() == InSequence)
		{
			return ActivePlayer;
		}
	}

	return nullptr;
}

TArray<UAvaSequencePlayer*> AAvaSequencePlaybackActor::GetSequencePlayersByLabel(FName InSequenceLabel) const
{
	return GetSequencePlayersByLabels({ InSequenceLabel });
}

TArray<UAvaSequencePlayer*> AAvaSequencePlaybackActor::GetSequencePlayersByTag(const FAvaTag& InTag, bool bInExactMatch) const
{
	const TArray<UAvaSequence*> Sequences = GetSequencesByTag(InTag, bInExactMatch);

	TArray<UAvaSequencePlayer*> SequencePlayers;
	SequencePlayers.Reserve(Sequences.Num());

	for (UAvaSequence* Sequence : Sequences)
	{
		SequencePlayers.Add(GetSequencePlayer(Sequence));
	}

	return SequencePlayers;
}

TArray<UAvaSequencePlayer*> AAvaSequencePlaybackActor::GetAllSequencePlayers() const
{
	TArray<UAvaSequencePlayer*> SequencePlayers;
	SequencePlayers.Reserve(ActiveSequencePlayers.Num());
	for (UAvaSequencePlayer* SequencePlayer : ActiveSequencePlayers)
	{
		if (SequencePlayer)
		{
			SequencePlayers.Add(SequencePlayer);	
		}
	}
	return SequencePlayers;
}

TArray<UAvaSequencePlayer*> AAvaSequencePlaybackActor::GetSequencePlayersByLabels(const TArray<FName>& InSequenceLabels) const
{
	const TArray<UAvaSequence*> Sequences = GetSequencesByLabel(InSequenceLabels);

	TArray<UAvaSequencePlayer*> SequencePlayers;
	SequencePlayers.Reserve(Sequences.Num());

	for (UAvaSequence* Sequence : Sequences)
	{
		SequencePlayers.Add(GetSequencePlayer(Sequence));
	}

	return SequencePlayers;
}

UAvaSequencePlayer* AAvaSequencePlaybackActor::GetOrAddSequencePlayer(UAvaSequence* InSequence)
{
	if (InSequence && !bStoppingAllSequences)
	{
		RegisterPlaybackObject();

		UAvaSequencePlayer* PlayerToGet = GetSequencePlayer(InSequence);

		if (!PlayerToGet)
		{
			PlayerToGet = NewObject<UAvaSequencePlayer>(this, NAME_None, RF_Transient);
			ActiveSequencePlayers.Add(PlayerToGet);
			PlayerToGet->InitSequence(InSequence, this, GetLevel());
		}

		return PlayerToGet;
	}
	return nullptr;
}

void AAvaSequencePlaybackActor::RegisterPlaybackObject()
{
	if (UAvaSequenceSubsystem* SequenceSubsystem = UAvaSequenceSubsystem::Get(this))
	{
		SequenceSubsystem->AddPlaybackObject(this);
	}
}

void AAvaSequencePlaybackActor::UnregisterPlaybackObject()
{
	if (UAvaSequenceSubsystem* SequenceSubsystem = UAvaSequenceSubsystem::Get(this))
	{
		SequenceSubsystem->RemovePlaybackObject(this);
	}

	CleanupPlayers();
}

void AAvaSequencePlaybackActor::EndPlay(const EEndPlayReason::Type InEndPlayReason)
{
	Super::EndPlay(InEndPlayReason);
	UnregisterPlaybackObject();
}
