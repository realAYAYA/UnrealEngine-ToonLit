// Copyright Epic Games, Inc. All Rights Reserved.

#include "Evaluation/MovieSceneEvaluationState.h"
#include "MovieSceneSequence.h"
#include "MovieScene.h"
#include "IMovieScenePlayer.h"
#include "IMovieScenePlaybackClient.h"
#include "MovieSceneObjectBindingID.h"

#include "Algo/Sort.h"
#include "Algo/Unique.h"

DECLARE_CYCLE_STAT(TEXT("Find Bound Objects"), MovieSceneEval_FindBoundObjects, STATGROUP_MovieSceneEval);
DECLARE_CYCLE_STAT(TEXT("Iterate Bound Objects"), MovieSceneEval_IterateBoundObjects, STATGROUP_MovieSceneEval);

FMovieSceneSharedDataId FMovieSceneSharedDataId::Allocate()
{
	static uint32 Counter = 0;

	FMovieSceneSharedDataId Value;
	Value.UniqueId = ++Counter;
	check(Counter != -1);
	return Value;
}

TArrayView<TWeakObjectPtr<>> FMovieSceneObjectCache::FindBoundObjects(const FGuid& InBindingID, IMovieScenePlayer& Player)
{
	MOVIESCENE_DETAILED_SCOPE_CYCLE_COUNTER(MovieSceneEval_FindBoundObjects)
	
	// Fast route - where everything's cached
	FBoundObjects* Bindings = BoundObjects.Find(InBindingID);
	if (Bindings && Bindings->bUpToDate)
	{
		return TArrayView<TWeakObjectPtr<>>(
			Bindings->Objects.GetData(),
			Bindings->Objects.Num()
			);
	}

	// Attempt to update the bindings
	UpdateBindings(InBindingID, Player);

	Bindings = BoundObjects.Find(InBindingID);
	if (Bindings)
	{
		return TArrayView<TWeakObjectPtr<>>(Bindings->Objects.GetData(), Bindings->Objects.Num());
	}

	// Just return nothing
	return TArrayView<TWeakObjectPtr<>>();
}

TArrayView<const TWeakObjectPtr<>> FMovieSceneObjectCache::IterateBoundObjects(const FGuid& InBindingID) const
{
	MOVIESCENE_DETAILED_SCOPE_CYCLE_COUNTER(MovieSceneEval_IterateBoundObjects)

	const FBoundObjects* Bindings = BoundObjects.Find(InBindingID);
	if (Bindings && Bindings->bUpToDate)
	{
		return TArrayView<const TWeakObjectPtr<>>(
			Bindings->Objects.GetData(),
			Bindings->Objects.Num()
			);
	}

	// Just return nothing
	return TArrayView<TWeakObjectPtr<>>();
}

FGuid FMovieSceneObjectCache::FindObjectId(UObject& InObject, IMovieScenePlayer& Player)
{
	UMovieSceneSequence* Sequence = WeakSequence.Get();
	UMovieScene* MovieScene = Sequence ? Sequence->GetMovieScene() : nullptr;
	if (!MovieScene)
	{
		return FGuid();
	}

	if (!bReentrantUpdate)
	{
		// @todo: Currently we delete the entire object cache when attempting to find an object's ID to ensure that we do a 
		// complete lookup from scratch. This is required for UMG as it interchanges content slots without notifying sequencer.
		Clear(Player);
	}

	return FindCachedObjectId(InObject, Player);
}

FGuid FMovieSceneObjectCache::FindCachedObjectId(UObject& InObject, IMovieScenePlayer& Player)
{
	UMovieSceneSequence* Sequence = WeakSequence.Get();
	UMovieScene* MovieScene = Sequence ? Sequence->GetMovieScene() : nullptr;
	if (!MovieScene)
	{
		return FGuid();
	}

	TWeakObjectPtr<> ObjectToFind(&InObject);

	// Search all possessables
	for (int32 Index = 0; Index < MovieScene->GetPossessableCount(); ++Index)
	{
		FGuid ThisGuid = MovieScene->GetPossessable(Index).GetGuid();
		if (FindBoundObjects(ThisGuid, Player).Contains(ObjectToFind))
		{
			return ThisGuid;
		}
	}

	// Search all spawnables
	for (int32 Index = 0; Index < MovieScene->GetSpawnableCount(); ++Index)
	{
		FGuid ThisGuid = MovieScene->GetSpawnable(Index).GetGuid();
		if (FindBoundObjects(ThisGuid, Player).Contains(ObjectToFind))
		{
			return ThisGuid;
		}
	}

	return FGuid();
}

void FMovieSceneObjectCache::FilterObjectBindings(UObject* PredicateObject, IMovieScenePlayer& Player, TArray<FMovieSceneObjectBindingID>* OutBindings)
{
	check(OutBindings);

	TArray<FGuid, TInlineAllocator<8>> OutOfDateBindings;
	for (const TTuple<FGuid, FBoundObjects>& Pair : BoundObjects)
	{
		if (Pair.Value.bUpToDate)
		{
			for (TWeakObjectPtr<> WeakObject : Pair.Value.Objects)
			{
				UObject* Object = WeakObject.Get();
				if (Object && Object == PredicateObject)
				{
					OutBindings->Add(UE::MovieScene::FFixedObjectBindingID(Pair.Key, SequenceID));
					break;
				}
			}
		}
		else
		{
			OutOfDateBindings.Add(Pair.Key);
		}
	}

	for (const FGuid& DirtyBinding : OutOfDateBindings)
	{
		UpdateBindings(DirtyBinding, Player);

		const FBoundObjects& Bindings = BoundObjects.FindChecked(DirtyBinding);
		for (TWeakObjectPtr<> WeakObject : Bindings.Objects)
		{
			UObject* Object = WeakObject.Get();
			if (Object && Object == PredicateObject)
			{
				OutBindings->Add(UE::MovieScene::FFixedObjectBindingID(DirtyBinding, SequenceID));
				break;
			}
		}
	}
}

void FMovieSceneObjectCache::InvalidateExpiredObjects()
{
	for (auto& Pair : BoundObjects)
	{
		if (!Pair.Value.bUpToDate)
		{
			continue;
		}

		for (TWeakObjectPtr<>& Ptr : Pair.Value.Objects)
		{
			if (!Ptr.Get())
			{
				InvalidateInternal(Pair.Key);
				break;
			}
		}
	}

	if (UMovieSceneSequence* Sequence = WeakSequence.Get())
	{
		TArray<FGuid> InvalidObjectIDs;
		Sequence->GatherExpiredObjects(*this, InvalidObjectIDs);

		for (const FGuid& ObjectID : InvalidObjectIDs)
		{
			InvalidateInternal(ObjectID);
		}
	}

	UpdateSerialNumber();
}

void FMovieSceneObjectCache::InvalidateIfValid(const FGuid& InGuid)
{
	const bool bInvalidated = InvalidateIfValidInternal(InGuid);
	if (bInvalidated)
	{
		UpdateSerialNumber();
	}
}

bool FMovieSceneObjectCache::InvalidateIfValidInternal(const FGuid& InGuid)
{
	// Don't manipulate the actual map structure, since this can be called from inside an iterator
	FBoundObjects* Cache = BoundObjects.Find(InGuid);

	if (Cache && Cache->bUpToDate == true)
	{
		Cache->bUpToDate = false;

		auto* Children = ChildBindings.Find(InGuid);
		if (Children)
		{
			for (const FGuid& Child : *Children)
			{
				InvalidateIfValidInternal(Child);
			}
		}

		OnBindingInvalidated.Broadcast(InGuid);

		return true;
	}

	return false;
}

void FMovieSceneObjectCache::Invalidate(const FGuid& InGuid)
{
	InvalidateInternal(InGuid);
	UpdateSerialNumber();
}

bool FMovieSceneObjectCache::InvalidateInternal(const FGuid& InGuid)
{
	// Don't manipulate the actual map structure, since this can be called from inside an iterator
	FBoundObjects* Cache = BoundObjects.Find(InGuid);
	if (Cache)
	{
		Cache->bUpToDate = false;

		auto* Children = ChildBindings.Find(InGuid);
		if (Children)
		{
			for (const FGuid& Child : *Children)
			{
				InvalidateInternal(Child);
			}
		}
	}

	OnBindingInvalidated.Broadcast(InGuid);

	return true;
}

void FMovieSceneObjectCache::Clear(IMovieScenePlayer& Player)
{
	BoundObjects.Reset();
	ChildBindings.Reset();

	UpdateSerialNumber();

	Player.NotifyBindingsChanged();
	OnBindingInvalidated.Broadcast(FGuid());
}


void FMovieSceneObjectCache::SetSequence(UMovieSceneSequence& InSequence, FMovieSceneSequenceIDRef InSequenceID, IMovieScenePlayer& Player)
{
	if (WeakSequence != &InSequence)
	{
		Clear(Player);
	}

	WeakSequence = &InSequence;
	SequenceID = InSequenceID;
}

void FMovieSceneObjectCache::UpdateBindings(const FGuid& InGuid, IMovieScenePlayer& Player)
{
	TGuardValue<bool> ReentrancyGuard(bReentrantUpdate, true);

	// Invalidate existing bindings, we're going to rebuild them.
	FBoundObjects* Bindings = &BoundObjects.FindOrAdd(InGuid);
	Bindings->Objects.Reset();

	// Update our serial now so it's done before any early returns.
	UpdateSerialNumber();

	if (auto* Children = ChildBindings.Find(InGuid))
	{
		for (const FGuid& Child : *Children)
		{
			InvalidateIfValidInternal(Child);
		}
	}

	// Find the sequence for this cache.
	UMovieSceneSequence* Sequence = WeakSequence.Get();
	if (!Sequence)
	{
		return;
	}

	// If we have overrides for this binding, ask the player to find it for us (most probably in a different cache
	// for a different sequence).
	// TODO-lchabant: we could technically end up in a circular override that creates an infinite loop...
	const FMovieSceneEvaluationOperand Operand(SequenceID, InGuid);
	if (const FMovieSceneEvaluationOperand* OverrideOperand = Player.BindingOverrides.Find(Operand))
	{
		const TArrayView<TWeakObjectPtr<>> OverrideBoundObjects = Player.FindBoundObjects(*OverrideOperand);
		Bindings->Objects.Append(OverrideBoundObjects.GetData(), OverrideBoundObjects.Num());
	}
	else
	{
		const bool bUseParentsAsContext = Sequence->AreParentContextsSignificant();

		UObject* Context = Player.GetPlaybackContext();

		const FMovieScenePossessable* Possessable = Sequence->GetMovieScene()->FindPossessable(InGuid);
		if (Possessable)
		{
			UObject* ResolutionContext = Context;

			// Because these are ordered parent-first, the parent must have already been bound, if it exists
			if (Possessable->GetParent().IsValid())
			{
				TArrayView<TWeakObjectPtr<>> ParentBoundObjects = FindBoundObjects(Possessable->GetParent(), Player);

				ChildBindings.FindOrAdd(Possessable->GetParent()).AddUnique(InGuid);

				// Refresh bindings in case of map changes
				Bindings = BoundObjects.Find(InGuid);
				for (TWeakObjectPtr<> Parent : ParentBoundObjects)
				{
					if (bUseParentsAsContext)
					{
						ResolutionContext = Parent.Get();
						if (!ResolutionContext)
						{
							continue;
						}
					}

					TArray<UObject*, TInlineAllocator<1>> FoundObjects;
					
					if (Possessable->GetSpawnableObjectBindingID().IsValid())
					{
						for (TWeakObjectPtr<> BoundObject : Possessable->GetSpawnableObjectBindingID().ResolveBoundObjects(SequenceID, Player))
						{
							if (BoundObject.IsValid())
							{
								FoundObjects.Add(BoundObject.Get());
							}
						}
					}
					else
					{
						Player.ResolveBoundObjects(InGuid, SequenceID, *Sequence, ResolutionContext, FoundObjects);
					}
					
					Bindings = BoundObjects.Find(InGuid);
					for (UObject* Object : FoundObjects)
					{
						Bindings->Objects.Add(Object);
					}
				}
			}
			else
			{
				TArray<UObject*, TInlineAllocator<1>> FoundObjects;

				if (Possessable->GetSpawnableObjectBindingID().IsValid())
				{
					for (TWeakObjectPtr<> BoundObject : Possessable->GetSpawnableObjectBindingID().ResolveBoundObjects(SequenceID, Player))
					{
						if (BoundObject.IsValid())
						{
							FoundObjects.Add(BoundObject.Get());
						}
					}				
				}
				else
				{
					Player.ResolveBoundObjects(InGuid, SequenceID, *Sequence, ResolutionContext, FoundObjects);
				}
				
				Bindings = BoundObjects.Find(InGuid);
				for (UObject* Object : FoundObjects)
				{
					Bindings->Objects.Add(Object);
				}
			}
		}
		else
		{
			// Probably a spawnable then (or an phantom)
			bool bUseDefault = true;

			// Allow external overrides for spawnables
			const IMovieScenePlaybackClient* PlaybackClient = Player.GetPlaybackClient();
			if (PlaybackClient)
			{
				TArray<UObject*, TInlineAllocator<1>> FoundObjects;
				bUseDefault = PlaybackClient->RetrieveBindingOverrides(InGuid, SequenceID, FoundObjects);
				for (UObject* Object : FoundObjects)
				{
					Bindings->Objects.Add(Object);
				}
			}

			// If we have no overrides, or they want to allow the default spawnable, do that now
			if (bUseDefault)
			{
				UObject* SpawnedObject = Player.GetSpawnRegister().FindSpawnedObject(InGuid, SequenceID).Get();
				if (SpawnedObject)
				{
					Bindings->Objects.Add(SpawnedObject);
				}
			}
		}
	}

	const int32 NumBoundObjects = Bindings->Objects.Num();

	// Remove duplicates from bound objects
	if (NumBoundObjects > 1)
	{
		Algo::Sort(Bindings->Objects, [](const TWeakObjectPtr<>& A, const TWeakObjectPtr<>& B) { return A.Get() < B.Get(); });
		const int32 EndIndex = Algo::Unique(Bindings->Objects);
		if (EndIndex < NumBoundObjects)
		{
			FMovieSceneBinding* Binding = Sequence->GetMovieScene()->FindBinding(InGuid);
			FString BindingName = Binding ? Binding->GetName() : TEXT("<invalid binding>");
			UE_LOG(LogMovieScene, Warning, TEXT("Found %d duplicate object(s) while resolving binding %s (%s) in %s"),
					NumBoundObjects - EndIndex, 
					*BindingName, *LexToString(InGuid), 
					*Sequence->GetPathName());
			Bindings->Objects.SetNum(EndIndex);
		}
	}

	if (NumBoundObjects > 0)
	{
		Bindings->bUpToDate = true;
		Player.NotifyBindingUpdate(InGuid, SequenceID, Bindings->Objects);

		if (auto* Children = ChildBindings.Find(InGuid))
		{
			for (const FGuid& Child : *Children)
			{
				InvalidateIfValidInternal(Child);
			}
		}
		ChildBindings.Remove(InGuid);
	}
}

void FMovieSceneObjectCache::UpdateSerialNumber()
{
	// Ok to overflow.
	++SerialNumber;
}

void FMovieSceneEvaluationState::InvalidateExpiredObjects()
{
	for (auto& Pair : ObjectCaches)
	{
		Pair.Value.ObjectCache.InvalidateExpiredObjects();
	}
}

void FMovieSceneEvaluationState::Invalidate(const FGuid& InGuid, FMovieSceneSequenceIDRef SequenceID)
{
	FVersionedObjectCache* Cache = ObjectCaches.Find(SequenceID);
	if (Cache)
	{
		Cache->ObjectCache.Invalidate(InGuid);
	}
}

void FMovieSceneEvaluationState::ClearObjectCaches(IMovieScenePlayer& Player)
{
	for (auto& Pair : ObjectCaches)
	{
		Pair.Value.ObjectCache.Clear(Player);
	}
}

void FMovieSceneEvaluationState::AssignSequence(FMovieSceneSequenceIDRef InSequenceID, UMovieSceneSequence& InSequence, IMovieScenePlayer& Player)
{
	GetObjectCache(InSequenceID).SetSequence(InSequence, InSequenceID, Player);
}

UMovieSceneSequence* FMovieSceneEvaluationState::FindSequence(FMovieSceneSequenceIDRef InSequenceID) const
{
	const FVersionedObjectCache* Cache = ObjectCaches.Find(InSequenceID);
	return Cache ? Cache->ObjectCache.GetSequence() : nullptr;
}

FMovieSceneSequenceID FMovieSceneEvaluationState::FindSequenceId(UMovieSceneSequence* InSequence) const
{
	for (auto& Pair : ObjectCaches)
	{
		if (Pair.Value.ObjectCache.GetSequence() == InSequence)
		{
			return Pair.Key;
		}
	}

	return FMovieSceneSequenceID();
}

FGuid FMovieSceneEvaluationState::FindObjectId(UObject& Object, FMovieSceneSequenceIDRef InSequenceID, IMovieScenePlayer& Player)
{
	FVersionedObjectCache* Cache = ObjectCaches.Find(InSequenceID);
	return Cache ? Cache->ObjectCache.FindObjectId(Object, Player) : FGuid();
}

FGuid FMovieSceneEvaluationState::FindCachedObjectId(UObject& Object, FMovieSceneSequenceIDRef InSequenceID, IMovieScenePlayer& Player)
{
	FVersionedObjectCache* Cache = ObjectCaches.Find(InSequenceID);
	return Cache ? Cache->ObjectCache.FindCachedObjectId(Object, Player) : FGuid();
}

void FMovieSceneEvaluationState::FilterObjectBindings(UObject* PredicateObject, IMovieScenePlayer& Player, TArray<FMovieSceneObjectBindingID>* OutBindings)
{
	check(OutBindings);

	for (TTuple<FMovieSceneSequenceID, FVersionedObjectCache>& Cache : ObjectCaches)
	{
		Cache.Value.ObjectCache.FilterObjectBindings(PredicateObject, Player, OutBindings);
	}
}

uint32 FMovieSceneEvaluationState::GetSerialNumber()
{
	bool bUpdateSerial = false;
	for (TTuple<FMovieSceneSequenceID, FVersionedObjectCache>& Cache : ObjectCaches)
	{
		const uint32 CurrentCacheSerial = Cache.Value.ObjectCache.GetSerialNumber();
		bUpdateSerial = bUpdateSerial || (CurrentCacheSerial != Cache.Value.LastKnownSerial);
		Cache.Value.LastKnownSerial = CurrentCacheSerial;
	}
	if (bUpdateSerial)
	{
		// Ok to overflow.
		++SerialNumber;
	}
	return SerialNumber;
}

