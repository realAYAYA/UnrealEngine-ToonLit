// Copyright Epic Games, Inc. All Rights Reserved.

#include "MovieSceneSpawnRegister.h"
#include "MovieScene.h"
#include "IMovieScenePlayer.h"
#include "Evaluation/MovieSceneEvaluationTemplateInstance.h"
#include "MovieSceneDynamicBindingInvoker.h"
#include "MovieSceneSpawnableAnnotation.h"

UE::MovieScene::TPlaybackCapabilityID<FMovieSceneSpawnRegister> FMovieSceneSpawnRegister::ID = UE::MovieScene::TPlaybackCapabilityID<FMovieSceneSpawnRegister>::Register();

TWeakObjectPtr<> FMovieSceneSpawnRegister::FindSpawnedObject(const FGuid& BindingId, FMovieSceneSequenceIDRef TemplateID) const
{
	FMovieSceneSpawnRegisterKey Key(TemplateID, BindingId);

	const FSpawnedObject* Existing = Register.Find(Key);
	return Existing ? Existing->Object : TWeakObjectPtr<>();
}

UObject* FMovieSceneSpawnRegister::SpawnObject(const FGuid& BindingId, UMovieScene& MovieScene, FMovieSceneSequenceIDRef TemplateID, TSharedRef<const FSharedPlaybackState> SharedPlaybackState)
{
	TWeakObjectPtr<> WeakObjectInstance = FindSpawnedObject(BindingId, TemplateID);
	UObject*         ObjectInstance     = WeakObjectInstance.Get();

	if (ObjectInstance)
	{
		return ObjectInstance;
	}

	// Find the spawnable definition
	FMovieSceneSpawnable* Spawnable = MovieScene.FindSpawnable(BindingId);
	if (!Spawnable)
	{
		return nullptr;
	}

	if (WeakObjectInstance.IsStale() && !Spawnable->bContinuouslyRespawn)
	{
		return nullptr;
	}

	UMovieSceneSequence* Sequence = SharedPlaybackState->GetSequence(TemplateID);
	if (!ensure(Sequence))
	{
		return nullptr;
	}

	UObject* SpawnedActor = nullptr;
	ESpawnOwnership SpawnOwnership = Spawnable->GetSpawnOwnership();
	IMovieScenePlayer* Player = UE::MovieScene::FPlayerIndexPlaybackCapability::GetPlayer(SharedPlaybackState);

	// See if there is some dynamic binding logic to invoke, otherwise spawn the actor
	FMovieSceneDynamicBindingResolveResult ResolveResult = FMovieSceneDynamicBindingInvoker::ResolveDynamicBinding(SharedPlaybackState, Sequence, TemplateID, *Spawnable);
	if (ResolveResult.Object)
	{
		SpawnedActor = ResolveResult.Object;
		if (ResolveResult.bIsPossessedObject)
		{
			SpawnOwnership = ESpawnOwnership::External;
		}
	}
	if (!SpawnedActor)
	{
		SpawnedActor = SpawnObject(*Spawnable, TemplateID, SharedPlaybackState);
	}
	
	if (SpawnedActor)
	{
		FMovieSceneSpawnableAnnotation::Add(SpawnedActor, BindingId, TemplateID, Sequence);

		FMovieSceneSpawnRegisterKey Key(TemplateID, BindingId);
		Register.Add(Key, FSpawnedObject(BindingId, *SpawnedActor, SpawnOwnership));

		if (FMovieSceneEvaluationState* State = SharedPlaybackState->FindCapability<FMovieSceneEvaluationState>())
		{
			State->Invalidate(BindingId, TemplateID);
		}
	}

	return SpawnedActor;
}

bool FMovieSceneSpawnRegister::DestroySpawnedObject(const FGuid& BindingId, FMovieSceneSequenceIDRef TemplateID, TSharedRef<const FSharedPlaybackState> SharedPlaybackState)
{
	TGuardValue<bool> CleaningUp(bCleaningUp, true);

	FMovieSceneSpawnRegisterKey Key(TemplateID, BindingId);
	
	FSpawnedObject* Existing = Register.Find(Key);
	UObject* SpawnedObject = Existing ? Existing->Object.Get() : nullptr;
	if (SpawnedObject)
	{
		PreDestroyObject(*SpawnedObject, BindingId, TemplateID);
		DestroySpawnedObject(*SpawnedObject);
	}

	Register.Remove(Key);

	if (FMovieSceneEvaluationState* State = SharedPlaybackState->FindCapability<FMovieSceneEvaluationState>())
	{
		State->Invalidate(BindingId, TemplateID);
	}

	return SpawnedObject != nullptr;
}

void FMovieSceneSpawnRegister::DestroyObjectsByPredicate(TSharedRef<const FSharedPlaybackState> SharedPlaybackState, const TFunctionRef<bool(const FGuid&, ESpawnOwnership, FMovieSceneSequenceIDRef)>& Predicate)
{
	for (auto It = Register.CreateIterator(); It; ++It)
	{
		if (Predicate(It.Value().Guid, It.Value().Ownership, It.Key().TemplateID))
		{
			UObject* SpawnedObject = It.Value().Object.Get();
			if (SpawnedObject)
			{
				PreDestroyObject(*SpawnedObject, It.Key().BindingId, It.Key().TemplateID);
				DestroySpawnedObject(*SpawnedObject);
			}

			It.RemoveCurrent();
		}
	}
}

void FMovieSceneSpawnRegister::ForgetExternallyOwnedSpawnedObjects(TSharedRef<const FSharedPlaybackState> SharedPlaybackState)
{
	FMovieSceneEvaluationState* State = SharedPlaybackState->FindCapability<FMovieSceneEvaluationState>();
	for (auto It = Register.CreateIterator(); It; ++It)
	{
		if (It.Value().Ownership == ESpawnOwnership::External)
		{
			if (State)
			{
				State->Invalidate(It.Key().BindingId, It.Key().TemplateID);
			}
			It.RemoveCurrent();
		}
	}
}

void FMovieSceneSpawnRegister::CleanUp(TSharedRef<const FSharedPlaybackState> SharedPlaybackState)
{
	TGuardValue<bool> CleaningUp(bCleaningUp, true);

	DestroyObjectsByPredicate(SharedPlaybackState, [&](const FGuid&, ESpawnOwnership, FMovieSceneSequenceIDRef){
		return true;
	});
}

void FMovieSceneSpawnRegister::CleanUpSequence(FMovieSceneSequenceIDRef TemplateID, TSharedRef<const FSharedPlaybackState> SharedPlaybackState)
{
	DestroyObjectsByPredicate(SharedPlaybackState, [&](const FGuid&, ESpawnOwnership, FMovieSceneSequenceIDRef ThisTemplateID){
		return ThisTemplateID == TemplateID;
	});
}

void FMovieSceneSpawnRegister::OnSequenceExpired(FMovieSceneSequenceIDRef TemplateID, TSharedRef<const FSharedPlaybackState> SharedPlaybackState)
{
	DestroyObjectsByPredicate(SharedPlaybackState, [&](const FGuid& ObjectId, ESpawnOwnership Ownership, FMovieSceneSequenceIDRef ThisTemplateID){
		return (Ownership == ESpawnOwnership::InnerSequence) && (TemplateID == ThisTemplateID);
	});
}

// Deprecated method redirects

UObject* FMovieSceneSpawnRegister::SpawnObject(const FGuid& BindingId, UMovieScene& MovieScene, FMovieSceneSequenceIDRef Template, IMovieScenePlayer& Player)
{
	return SpawnObject(BindingId, MovieScene, Template, Player.GetSharedPlaybackState());
}

bool FMovieSceneSpawnRegister::DestroySpawnedObject(const FGuid& BindingId, FMovieSceneSequenceIDRef TemplateID, IMovieScenePlayer& Player)
{
	return DestroySpawnedObject(BindingId, TemplateID, Player.GetSharedPlaybackState());
}

void FMovieSceneSpawnRegister::DestroyObjectsByPredicate(IMovieScenePlayer& Player, const TFunctionRef<bool(const FGuid&, ESpawnOwnership, FMovieSceneSequenceIDRef)>& Predicate)
{
	DestroyObjectsByPredicate(Player.GetSharedPlaybackState(), Predicate);
}

void FMovieSceneSpawnRegister::ForgetExternallyOwnedSpawnedObjects(FMovieSceneEvaluationState& State, IMovieScenePlayer& Player)
{
	ForgetExternallyOwnedSpawnedObjects(Player.GetSharedPlaybackState());
}

void FMovieSceneSpawnRegister::CleanUp(IMovieScenePlayer& Player)
{
	CleanUp(Player.GetSharedPlaybackState());
}

void FMovieSceneSpawnRegister::CleanUpSequence(FMovieSceneSequenceIDRef TemplateID, IMovieScenePlayer& Player)
{
	CleanUpSequence(TemplateID, Player.GetSharedPlaybackState());
}

void FMovieSceneSpawnRegister::OnSequenceExpired(FMovieSceneSequenceIDRef TemplateID, IMovieScenePlayer& Player)
{
	OnSequenceExpired(TemplateID, Player.GetSharedPlaybackState());
}

#if WITH_EDITOR

void FMovieSceneSpawnRegister::SaveDefaultSpawnableState(FMovieSceneSpawnable& Spawnable, FMovieSceneSequenceIDRef TemplateID, IMovieScenePlayer& Player)
{
	SaveDefaultSpawnableState(Spawnable, TemplateID, Player.GetSharedPlaybackState());
}

void FMovieSceneSpawnRegister::HandleConvertPossessableToSpawnable(UObject* OldObject, IMovieScenePlayer& Player, TOptional<FTransformData>& OutTransformData)
{
	HandleConvertPossessableToSpawnable(OldObject, Player.GetSharedPlaybackState(), OutTransformData);
}

UObject* FMovieSceneSpawnRegister::SpawnObject(FMovieSceneSpawnable& Spawnable, FMovieSceneSequenceIDRef TemplateID, IMovieScenePlayer& Player)
{
	return SpawnObject(Spawnable, TemplateID, Player.GetSharedPlaybackState());
}

#endif

