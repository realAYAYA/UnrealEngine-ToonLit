// Copyright Epic Games, Inc. All Rights Reserved.

#include "TimeSources/AvaPropertyAnimatorSequenceTimeSource.h"

#include "AvaSequence.h"
#include "AvaSequencePlayer.h"
#include "MovieSceneSequence.h"
#include "UObject/UObjectIterator.h"

#if WITH_EDITOR
#include "ISequencerModule.h"
#include "Modules/ModuleManager.h"
#endif // WITH_EDITOR

#if WITH_EDITOR
void UAvaPropertyAnimatorSequenceTimeSource::PostEditChangeProperty(FPropertyChangedEvent& InPropertyChangedEvent)
{
	Super::PostEditChangeProperty(InPropertyChangedEvent);

	const FName MemberName = InPropertyChangedEvent.GetMemberPropertyName();

	static const FName SequenceNamePropertyName = GET_MEMBER_NAME_CHECKED(UAvaPropertyAnimatorSequenceTimeSource, SequenceName);

	if (MemberName == SequenceNamePropertyName)
	{
		OnSequenceChanged();
	}
}
#endif // WITH_EDITOR

void UAvaPropertyAnimatorSequenceTimeSource::SetSequenceName(const FString& InSequenceName)
{
	if (SequenceName == InSequenceName)
	{
		return;
	}

	const TArray<FString> Sequences = GetSequenceNames();
	if (!Sequences.Contains(InSequenceName))
	{
		return;
	}

	SequenceName = InSequenceName;
	OnSequenceChanged();
}

void UAvaPropertyAnimatorSequenceTimeSource::OnTimeSourceActive()
{
	Super::OnTimeSourceActive();

	OnSequenceChanged();
}

void UAvaPropertyAnimatorSequenceTimeSource::OnTimeSourceInactive()
{
	Super::OnTimeSourceInactive();

	UAvaSequencePlayer::OnSequenceFinished().RemoveAll(this);
	UAvaSequencePlayer::OnSequenceStarted().RemoveAll(this);

	SequencePlayerWeak.Reset();
	SequencePlayerWeak = nullptr;
}

#if WITH_EDITOR
void UAvaPropertyAnimatorSequenceTimeSource::OnSequencerCreated(TSharedRef<ISequencer> InSequencer)
{
	SequencerWeak = InSequencer;
}

TSharedPtr<ISequencer> UAvaPropertyAnimatorSequenceTimeSource::GetSequencer() const
{
	if (const UAvaPropertyAnimatorSequenceTimeSource* Default = GetDefault<UAvaPropertyAnimatorSequenceTimeSource>())
	{
		return Default->SequencerWeak.Pin();
	}

	return nullptr;
}
#endif // WITH_EDITOR

double UAvaPropertyAnimatorSequenceTimeSource::GetTimeElapsed()
{
	// Valid when a sequence player is playing
	if (const UAvaSequencePlayer* SequencePlayer = SequencePlayerWeak.Get())
	{
		return SequencePlayer->GetCurrentTime().AsSeconds();
	}

#if WITH_EDITOR
	// Use sequencer global time if sequencer active
	if (const TSharedPtr<ISequencer> Sequencer = GetSequencer())
	{
		// Scrub to global time only if root sequence is the selected sequence
		if (const UMovieSceneSequence* RootSequence = Sequencer->GetRootMovieSceneSequence())
		{
			if (RootSequence->GetName() == SequenceName)
			{
				return Sequencer->GetGlobalTime().AsSeconds();
			}
		}
	}
#endif

	return 0;
}

bool UAvaPropertyAnimatorSequenceTimeSource::IsTimeSourceReady() const
{
	return SequencePlayerWeak.IsValid()
#if WITH_EDITOR
		|| GetSequencer()
#endif
	;
}

void UAvaPropertyAnimatorSequenceTimeSource::OnTimeSourceRegistered()
{
	Super::OnTimeSourceRegistered();

#if WITH_EDITOR
	ISequencerModule& SequencerModule = FModuleManager::LoadModuleChecked<ISequencerModule>("Sequencer");
	OnSequencerCreatedHandle = SequencerModule.RegisterOnSequencerCreated(FOnSequencerCreated::FDelegate::CreateUObject(this, &UAvaPropertyAnimatorSequenceTimeSource::OnSequencerCreated));
#endif // WITH_EDITOR
}

void UAvaPropertyAnimatorSequenceTimeSource::OnTimeSourceUnregistered()
{
	Super::OnTimeSourceUnregistered();

#if WITH_EDITOR
	ISequencerModule& SequencerModule = FModuleManager::LoadModuleChecked<ISequencerModule>("Sequencer");
	SequencerModule.UnregisterOnSequencerCreated(OnSequencerCreatedHandle);
	OnSequencerCreatedHandle.Reset();
#endif // WITH_EDITOR
}

TArray<FString> UAvaPropertyAnimatorSequenceTimeSource::GetSequenceNames() const
{
	TArray<FString> SequenceNames {TEXT("")};

	const UWorld* World = GetWorld();

	for (const UMovieSceneSequence* MovieSceneSequence : TObjectRange<UMovieSceneSequence>())
	{
		if (MovieSceneSequence && MovieSceneSequence->GetWorld() == World)
		{
			SequenceNames.Add(MovieSceneSequence->GetName());
		}
	}

	return SequenceNames;
}

void UAvaPropertyAnimatorSequenceTimeSource::OnSequenceChanged()
{
	// Reset sequence selection
	if (SequenceName.IsEmpty())
	{
		SequenceWeak = nullptr;
		SequenceWeak.Reset();

		SequencePlayerWeak = nullptr;
		SequencePlayerWeak.Reset();

		UAvaSequencePlayer::OnSequenceFinished().RemoveAll(this);
		UAvaSequencePlayer::OnSequenceFinished().RemoveAll(this);
		UAvaSequencePlayer::OnSequenceStarted().RemoveAll(this);

		return;
	}

	// Find sequence with that name
	const UWorld* World = GetWorld();

	for (UMovieSceneSequence* MovieSceneSequence : TObjectRange<UMovieSceneSequence>())
	{
		if (MovieSceneSequence && MovieSceneSequence->GetWorld() == World)
		{
			if (MovieSceneSequence->GetName() == SequenceName)
			{
				SequenceWeak = MovieSceneSequence;
				break;
			}
		}
	}

	if (!SequenceWeak.IsValid())
	{
		return;
	}

	UAvaSequencePlayer::OnSequenceFinished().RemoveAll(this);
	UAvaSequencePlayer::OnSequenceStarted().RemoveAll(this);
	UAvaSequencePlayer::OnSequenceStarted().AddUObject(this, &UAvaPropertyAnimatorSequenceTimeSource::OnSequenceStarted);
}

void UAvaPropertyAnimatorSequenceTimeSource::OnSequenceStarted(UAvaSequencePlayer* InPlayer, UAvaSequence* InSequence)
{
	if (!InPlayer || !InSequence)
	{
		return;
	}

	const UMovieSceneSequence* Sequence = SequenceWeak.Get();
	if (InSequence != Sequence)
	{
		return;
	}

	SequencePlayerWeak = InPlayer;

	UAvaSequencePlayer::OnSequenceFinished().RemoveAll(this);
	UAvaSequencePlayer::OnSequenceFinished().AddUObject(this, &UAvaPropertyAnimatorSequenceTimeSource::OnSequenceFinished);
}

void UAvaPropertyAnimatorSequenceTimeSource::OnSequenceFinished(UAvaSequencePlayer* InPlayer, UAvaSequence* InSequence)
{
	if (!InPlayer || !InSequence)
	{
		return;
	}

	const UMovieSceneSequence* Sequence = SequenceWeak.Get();
	if (Sequence != InSequence)
	{
		return;
	}

	UAvaSequencePlayer::OnSequenceFinished().RemoveAll(this);
	SequencePlayerWeak = nullptr;
	SequencePlayerWeak.Reset();
}
