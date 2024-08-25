// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "IMovieScenePlayer.h"
#include "Evaluation/EventContextsPlaybackCapability.h"
#include "Evaluation/EventTriggerControlPlaybackCapability.h"
#include "Evaluation/MovieSceneEvaluationTemplateInstance.h"
#include "EntitySystem/MovieSceneSequenceInstance.h"
#include "EntitySystem/MovieSceneEntitySystemLinker.h"
#include "IMovieScenePlaybackClient.h"
#include "UniversalObjectLocatorResolveParams.h"
#include "Misc/ScopeRWLock.h"
#include "MovieSceneFwd.h"
#include "MovieSceneSequence.h"
#include "MovieSceneSequenceID.h"
#include "UniversalObjectLocatorResolveParameterBuffer.inl"

namespace UE
{
namespace MovieScene
{

static FRWLock                          GGlobalPlayerRegistryLock;
static TSparseArray<IMovieScenePlayer*> GGlobalPlayerRegistry;
static TBitArray<> GGlobalPlayerUpdateFlags;

TPlaybackCapabilityID<FPlayerIndexPlaybackCapability> FPlayerIndexPlaybackCapability::ID = TPlaybackCapabilityID<FPlayerIndexPlaybackCapability>::Register();

IMovieScenePlayer* FPlayerIndexPlaybackCapability::GetPlayer(TSharedRef<const FSharedPlaybackState> Owner)
{
	if (FPlayerIndexPlaybackCapability* Cap = Owner->FindCapability<FPlayerIndexPlaybackCapability>())
	{
		return IMovieScenePlayer::Get(Cap->PlayerIndex);
	}
	return nullptr;
}

uint16 FPlayerIndexPlaybackCapability::GetPlayerIndex(TSharedRef<const FSharedPlaybackState> Owner)
{
	if (FPlayerIndexPlaybackCapability* Cap = Owner->FindCapability<FPlayerIndexPlaybackCapability>())
	{
		return Cap->PlayerIndex;
	}
	return (uint16)-1;
}

} // namespace MovieScene
} // namespace UE

UE::MovieScene::TPlaybackCapabilityID<IMovieScenePlaybackClient> IMovieScenePlaybackClient::ID = UE::MovieScene::TPlaybackCapabilityID<IMovieScenePlaybackClient>::Register();

IMovieScenePlayer::IMovieScenePlayer()
{
	FWriteScopeLock ScopeLock(UE::MovieScene::GGlobalPlayerRegistryLock);

	UE::MovieScene::GGlobalPlayerRegistry.Shrink();
	UniqueIndex = UE::MovieScene::GGlobalPlayerRegistry.Add(this);

	UE::MovieScene::GGlobalPlayerUpdateFlags.PadToNum(UniqueIndex + 1, false);
	UE::MovieScene::GGlobalPlayerUpdateFlags[UniqueIndex] = 0;
}

IMovieScenePlayer::~IMovieScenePlayer()
{	
	FWriteScopeLock ScopeLock(UE::MovieScene::GGlobalPlayerRegistryLock);

	UE::MovieScene::GGlobalPlayerUpdateFlags[UniqueIndex] = 0;
	UE::MovieScene::GGlobalPlayerRegistry.RemoveAt(UniqueIndex, 1);
}

IMovieScenePlayer* IMovieScenePlayer::Get(uint16 InUniqueIndex)
{
	FReadScopeLock ScopeLock(UE::MovieScene::GGlobalPlayerRegistryLock);
	check(UE::MovieScene::GGlobalPlayerRegistry.IsValidIndex(InUniqueIndex));
	return UE::MovieScene::GGlobalPlayerRegistry[InUniqueIndex];
}

void IMovieScenePlayer::Get(TArray<IMovieScenePlayer*>& OutPlayers, bool bOnlyUnstoppedPlayers)
{
	FReadScopeLock ScopeLock(UE::MovieScene::GGlobalPlayerRegistryLock);
	for (auto It = UE::MovieScene::GGlobalPlayerRegistry.CreateIterator(); It; ++It)
	{
		if (IMovieScenePlayer* Player = *It)
		{
			if (!bOnlyUnstoppedPlayers || Player->GetPlaybackStatus() != EMovieScenePlayerStatus::Stopped)
			{
				OutPlayers.Add(*It);
			}
		}
	}
}

void IMovieScenePlayer::SetIsEvaluatingFlag(uint16 InUniqueIndex, bool bIsUpdating)
{
	check(UE::MovieScene::GGlobalPlayerUpdateFlags.IsValidIndex(InUniqueIndex));
	UE::MovieScene::GGlobalPlayerUpdateFlags[InUniqueIndex] = bIsUpdating;
}

bool IMovieScenePlayer::IsEvaluating() const
{
	return UE::MovieScene::GGlobalPlayerUpdateFlags[UniqueIndex];
}

void IMovieScenePlayer::PopulateUpdateFlags(UE::MovieScene::ESequenceInstanceUpdateFlags& OutFlags)
{
	using namespace UE::MovieScene;

	OutFlags |= ESequenceInstanceUpdateFlags::NeedsPreEvaluation | ESequenceInstanceUpdateFlags::NeedsPostEvaluation;
}

void IMovieScenePlayer::ResolveBoundObjects(const FGuid& InBindingId, FMovieSceneSequenceID SequenceID, UMovieSceneSequence& Sequence, UObject* ResolutionContext, TArray<UObject*, TInlineAllocator<1>>& OutObjects) const
{
	// This deprecated version of ResolveBoundObjects no longer gets called directly by the FMovieSceneObjectCache- that uses the ResolveParams overload below.
	// In order to ensure things continue to work properly for anyone that may have been calling this directly rather than FindBoundObjects, we direct
	// this towards FindBoundObjects below.

	TArrayView<TWeakObjectPtr<>> BoundObjects = const_cast<IMovieScenePlayer*>(this)->FindBoundObjects(InBindingId, SequenceID);
	for (TWeakObjectPtr<> BoundObject : BoundObjects)
	{
		if (UObject* Obj = BoundObject.Get())
		{
			OutObjects.Add(Obj);
		}
	}
}

void IMovieScenePlayer::ResolveBoundObjects(UE::UniversalObjectLocator::FResolveParams& ResolveParams, const FGuid& InBindingId, FMovieSceneSequenceID SequenceID, UMovieSceneSequence& Sequence, TArray<UObject*, TInlineAllocator<1>>& OutObjects) const
{
	Sequence.LocateBoundObjects(InBindingId, ResolveParams, OutObjects);
}

TArrayView<TWeakObjectPtr<>> IMovieScenePlayer::FindBoundObjects(const FGuid& ObjectBindingID, FMovieSceneSequenceIDRef SequenceID)
{
	using namespace UE::MovieScene;

	if (TSharedPtr<const FSharedPlaybackState> SharedPlaybackState = FindSharedPlaybackState())
	{
		return State.FindBoundObjects(ObjectBindingID, SequenceID, SharedPlaybackState.ToSharedRef());
	}

	return TArrayView<TWeakObjectPtr<>>();
}

void IMovieScenePlayer::InvalidateCachedData()
{
	FMovieSceneRootEvaluationTemplateInstance& Template = GetEvaluationTemplate();

	UE::MovieScene::FSequenceInstance* RootInstance = Template.FindInstance(MovieSceneSequenceID::Root);
	if (RootInstance)
	{
		RootInstance->InvalidateCachedData();
	}
}

TSharedPtr<UE::MovieScene::FSharedPlaybackState> IMovieScenePlayer::FindSharedPlaybackState()
{
	return GetEvaluationTemplate().GetSharedPlaybackState();
}

TSharedRef<UE::MovieScene::FSharedPlaybackState> IMovieScenePlayer::GetSharedPlaybackState()
{
	// ToSharedRef will assert if evaluation template isn't initialized
	return GetEvaluationTemplate().GetSharedPlaybackState().ToSharedRef();
}

void IMovieScenePlayer::ResetDirectorInstances()
{
	using namespace UE::MovieScene;

	TSharedPtr<const FSharedPlaybackState> SharedPlaybackState = FindSharedPlaybackState();
	if (!SharedPlaybackState)
	{
		return;
	}

	FSequenceDirectorPlaybackCapability* Cap = SharedPlaybackState->FindCapability<FSequenceDirectorPlaybackCapability>();
	if (Cap)
	{
		Cap->ResetDirectorInstances();
	}
}

UObject* IMovieScenePlayer::GetOrCreateDirectorInstance(TSharedRef<const UE::MovieScene::FSharedPlaybackState> SharedPlaybackState, FMovieSceneSequenceIDRef SequenceID)
{
	using namespace UE::MovieScene;

	FSequenceDirectorPlaybackCapability* Cap = SharedPlaybackState->FindCapability<FSequenceDirectorPlaybackCapability>();
	if (Cap)
	{
		return Cap->GetOrCreateDirectorInstance(SharedPlaybackState, SequenceID);
	}
	return nullptr;
}

TArray<UObject*> IMovieScenePlayer::GetEventContexts() const
{
	using namespace UE::MovieScene;

	// By default, look for the playback capability, for backwards compatibility.
	IMovieScenePlayer* This = const_cast<IMovieScenePlayer*>(this);
	if (TSharedPtr<const FSharedPlaybackState> SharedPlaybackState = This->FindSharedPlaybackState())
	{
		if (IEventContextsPlaybackCapability* EventContextsCapability = SharedPlaybackState->FindCapability<IEventContextsPlaybackCapability>())
		{
			return EventContextsCapability->GetEventContexts();
		}
	}
	return TArray<UObject*>();
}

bool IMovieScenePlayer::IsDisablingEventTriggers(FFrameTime& DisabledUntilTime) const
{
	using namespace UE::MovieScene;

	// By default, look for the playback capability, for backwards compatibility.
	IMovieScenePlayer* This = const_cast<IMovieScenePlayer*>(this);
	if (TSharedPtr<const FSharedPlaybackState> SharedPlaybackState = This->FindSharedPlaybackState())
	{
		if (FEventTriggerControlPlaybackCapability* TriggerControlCapability = SharedPlaybackState->FindCapability<FEventTriggerControlPlaybackCapability>())
		{
			return TriggerControlCapability->IsDisablingEventTriggers(DisabledUntilTime);
		}
	}
	return false;
}

void IMovieScenePlayer::InitializeRootInstance(TSharedRef<UE::MovieScene::FSharedPlaybackState> NewSharedPlaybackState)
{
	using namespace UE::MovieScene;

	NewSharedPlaybackState->AddCapability<FPlayerIndexPlaybackCapability>(UniqueIndex);
	NewSharedPlaybackState->AddCapabilityRaw(&State);
	NewSharedPlaybackState->AddCapabilityRaw(&GetSpawnRegister());
	NewSharedPlaybackState->AddCapabilityRaw((IObjectBindingNotifyPlaybackCapability*)this);
	NewSharedPlaybackState->AddCapabilityRaw((IStaticBindingOverridesPlaybackCapability*)this);

	if (IMovieScenePlaybackClient* PlaybackClient = GetPlaybackClient())
	{
		NewSharedPlaybackState->AddCapabilityRaw(PlaybackClient);
	}

	UMovieSceneEntitySystemLinker* Linker = NewSharedPlaybackState->GetLinker();

	if (ensure(Linker))
	{
		FInstanceRegistry* InstanceRegistry = Linker->GetInstanceRegistry();

		if (ensure(InstanceRegistry))
		{
			FSequenceInstance& RootInstance = InstanceRegistry->MutateInstance(NewSharedPlaybackState->GetRootInstanceHandle());
			RootInstance.Initialize();
		}
	}
}

