// Copyright Epic Games, Inc. All Rights Reserved.

#include "MovieSceneSpawnRegister.h"
#include "MovieScene.h"
#include "IMovieScenePlayer.h"
#include "Evaluation/MovieSceneEvaluationTemplateInstance.h"
#include "MovieSceneSpawnableAnnotation.h"

TWeakObjectPtr<> FMovieSceneSpawnRegister::FindSpawnedObject(const FGuid& BindingId, FMovieSceneSequenceIDRef TemplateID) const
{
	FMovieSceneSpawnRegisterKey Key(TemplateID, BindingId);

	const FSpawnedObject* Existing = Register.Find(Key);
	return Existing ? Existing->Object : TWeakObjectPtr<>();
}

UObject* FMovieSceneSpawnRegister::SpawnObject(const FGuid& BindingId, UMovieScene& MovieScene, FMovieSceneSequenceIDRef TemplateID, IMovieScenePlayer& Player)
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

	UObject* SpawnedActor = SpawnObject(*Spawnable, TemplateID, Player);
	
	if (SpawnedActor)
	{
		UMovieSceneSequence* Sequence = Player.GetEvaluationTemplate().GetSequence(TemplateID);
		FMovieSceneSpawnableAnnotation::Add(SpawnedActor, BindingId, TemplateID, Sequence);

		FMovieSceneSpawnRegisterKey Key(TemplateID, BindingId);
		Register.Add(Key, FSpawnedObject(BindingId, *SpawnedActor, Spawnable->GetSpawnOwnership()));

		Player.State.Invalidate(BindingId, TemplateID);
	}

	return SpawnedActor;
}

bool FMovieSceneSpawnRegister::DestroySpawnedObject(const FGuid& BindingId, FMovieSceneSequenceIDRef TemplateID, IMovieScenePlayer& Player)
{
	FMovieSceneSpawnRegisterKey Key(TemplateID, BindingId);
	
	FSpawnedObject* Existing = Register.Find(Key);
	UObject* SpawnedObject = Existing ? Existing->Object.Get() : nullptr;
	if (SpawnedObject)
	{
		PreDestroyObject(*SpawnedObject, BindingId, TemplateID);
		DestroySpawnedObject(*SpawnedObject);
	}

	Register.Remove(Key);

	Player.State.Invalidate(BindingId, TemplateID);

	return SpawnedObject != nullptr;
}

void FMovieSceneSpawnRegister::DestroyObjectsByPredicate(IMovieScenePlayer& Player, const TFunctionRef<bool(const FGuid&, ESpawnOwnership, FMovieSceneSequenceIDRef)>& Predicate)
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

void FMovieSceneSpawnRegister::ForgetExternallyOwnedSpawnedObjects(FMovieSceneEvaluationState& State, IMovieScenePlayer& Player)
{
	for (auto It = Register.CreateIterator(); It; ++It)
	{
		if (It.Value().Ownership == ESpawnOwnership::External)
		{
			Player.State.Invalidate(It.Key().BindingId, It.Key().TemplateID);
			It.RemoveCurrent();
		}
	}
}

void FMovieSceneSpawnRegister::CleanUp(IMovieScenePlayer& Player)
{
	TGuardValue<bool> CleaningUp(bCleaningUp, true);

	DestroyObjectsByPredicate(Player, [&](const FGuid&, ESpawnOwnership, FMovieSceneSequenceIDRef){
		return true;
	});
}

void FMovieSceneSpawnRegister::CleanUpSequence(FMovieSceneSequenceIDRef TemplateID, IMovieScenePlayer& Player)
{
	DestroyObjectsByPredicate(Player, [&](const FGuid&, ESpawnOwnership, FMovieSceneSequenceIDRef ThisTemplateID){
		return ThisTemplateID == TemplateID;
	});
}

void FMovieSceneSpawnRegister::OnSequenceExpired(FMovieSceneSequenceIDRef TemplateID, IMovieScenePlayer& Player)
{
	DestroyObjectsByPredicate(Player, [&](const FGuid& ObjectId, ESpawnOwnership Ownership, FMovieSceneSequenceIDRef ThisTemplateID){
		return (Ownership == ESpawnOwnership::InnerSequence) && (TemplateID == ThisTemplateID);
	});
}
