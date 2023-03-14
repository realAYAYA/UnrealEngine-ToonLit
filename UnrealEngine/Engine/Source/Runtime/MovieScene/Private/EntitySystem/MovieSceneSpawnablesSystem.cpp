// Copyright Epic Games, Inc. All Rights Reserved.

#include "EntitySystem/MovieSceneSpawnablesSystem.h"
#include "EntitySystem/MovieSceneEntitySystemTypes.h"
#include "EntitySystem/MovieSceneEntitySystemLinker.h"
#include "EntitySystem/MovieSceneEntityRange.h"
#include "EntitySystem/MovieSceneEntitySystemTask.h"
#include "EntitySystem/MovieSceneEntityInstantiatorSystem.h"
#include "EntitySystem/BuiltInComponentTypes.h"
#include "Evaluation/MovieSceneEvaluationOperand.h"

#include "MovieScene.h"
#include "MovieSceneSequence.h"
#include "MovieSceneExecutionToken.h"
#include "IMovieScenePlayer.h"
#include "IMovieScenePlaybackClient.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MovieSceneSpawnablesSystem)

namespace UE
{
namespace MovieScene
{

DECLARE_CYCLE_STAT(TEXT("Spawnables System"), MovieSceneEval_SpawnablesSystem, STATGROUP_MovieSceneECS);
DECLARE_CYCLE_STAT(TEXT("Spawnables: Spawn"), MovieSceneEval_SpawnSpawnables, STATGROUP_MovieSceneEval);
DECLARE_CYCLE_STAT(TEXT("Spawnables: Destroy"), MovieSceneEval_DestroySpawnables, STATGROUP_MovieSceneEval);

struct FSpawnTrackPreAnimatedTokenProducer : IMovieScenePreAnimatedTokenProducer
{
	FMovieSceneEvaluationOperand Operand;
	FSpawnTrackPreAnimatedTokenProducer(FMovieSceneEvaluationOperand InOperand) : Operand(InOperand) {}

	virtual IMovieScenePreAnimatedTokenPtr CacheExistingState(UObject& Object) const
	{
		struct FToken : IMovieScenePreAnimatedToken
		{
			FMovieSceneEvaluationOperand OperandToDestroy;
			FToken(FMovieSceneEvaluationOperand InOperand) : OperandToDestroy(InOperand) {}

			virtual void RestoreState(UObject& Object, const UE::MovieScene::FRestoreStateParams& Params) override
			{
				IMovieScenePlayer* Player = Params.GetTerminalPlayer();
				if (!ensure(Player))
				{
					return;
				}

				if (!Player->GetSpawnRegister().DestroySpawnedObject(OperandToDestroy.ObjectBindingID, OperandToDestroy.SequenceID, *Player))
				{
					// This branch should only be taken for Externally owned spawnables that have been 'forgotten',
					// but still had RestoreState tokens generated for them (ie, in FSequencer, or if bRestoreState is enabled)
					// on a UMovieSceneSequencePlayer
					Player->GetSpawnRegister().DestroyObjectDirectly(Object);
				}
			}
		};
		
		return FToken(Operand);
	}
};

const FMovieSceneAnimTypeID SpawnableAnimTypeID = FMovieSceneAnimTypeID::Unique();

} // namespace MovieScene
} // namespace UE




UMovieSceneSpawnablesSystem::UMovieSceneSpawnablesSystem(const FObjectInitializer& ObjInit)
	: Super(ObjInit)
{
	Phase = UE::MovieScene::ESystemPhase::Spawn;
	RelevantComponent = UE::MovieScene::FBuiltInComponentTypes::Get()->SpawnableBinding;
}

FMovieSceneAnimTypeID UMovieSceneSpawnablesSystem::GetAnimTypeID()
{
	return UE::MovieScene::SpawnableAnimTypeID;
}

void UMovieSceneSpawnablesSystem::OnRun(FSystemTaskPrerequisites& InPrerequisites, FSystemSubsequentTasks& Subsequents)
{
	using namespace UE::MovieScene;

	SCOPE_CYCLE_COUNTER(MovieSceneEval_SpawnablesSystem)

	FBuiltInComponentTypes* BuiltInComponents = FBuiltInComponentTypes::Get();
	FInstanceRegistry*      InstanceRegistry  = Linker->GetInstanceRegistry();

	// Re-link any spawnables that were invalidated
	if (InstanceRegistry->HasInvalidatedBindings())
	{
		TArray<FMovieSceneEntityID> StaleSpawnables;

		auto GatherStaleSpawnables = [InstanceRegistry, &StaleSpawnables](FMovieSceneEntityID EntityID, FInstanceHandle InstanceHandle, const FGuid& InObjectBindingID)
		{
			if (InstanceRegistry->IsBindingInvalidated(InObjectBindingID, InstanceHandle))
			{
				StaleSpawnables.Add(EntityID);
			}
		};

		// Invalidate any spawnables that have been invalidated or destroyed
		FEntityTaskBuilder()
		.ReadEntityIDs()
		.Read(BuiltInComponents->InstanceHandle)
		.Read(BuiltInComponents->SpawnableBinding)
		.FilterNone({ BuiltInComponents->Tags.NeedsUnlink })
		.Iterate_PerEntity(&Linker->EntityManager, GatherStaleSpawnables);

		for (FMovieSceneEntityID Entity : StaleSpawnables)
		{
			// Tag this as needs link, and all children as needs unlink
			Linker->EntityManager.AddComponent(Entity, BuiltInComponents->Tags.NeedsLink);
			Linker->EntityManager.AddComponent(Entity, BuiltInComponents->Tags.NeedsUnlink, EEntityRecursion::Children);
		}
	}


	// ----------------------------------------------------------------------------------------------------------------------------------------
	// Step 1 - iterate all pending spawnables and spawn their objects if necessary
	auto SpawnNewObjects = [InstanceRegistry](FInstanceHandle InstanceHandle, const FGuid& SpawnableBindingID)
	{
		SCOPE_CYCLE_COUNTER(MovieSceneEval_SpawnSpawnables)

		const FSequenceInstance& SequenceInstance = InstanceRegistry->GetInstance(InstanceHandle);

		FMovieSceneSequenceID SequenceID  = SequenceInstance.GetSequenceID();
		IMovieScenePlayer*    Player      = SequenceInstance.GetPlayer();

		const FMovieSceneEvaluationOperand SpawnableOperand(SequenceID, SpawnableBindingID);
		if (const FMovieSceneEvaluationOperand* OperandOverride = Player->BindingOverrides.Find(SpawnableOperand))
		{
			// Don't do anything if this operand was overriden... someone else will take care of it (either another spawn track, or
			// some possessable).
			return;
		}

		// Check if we already have a spawned object in the sapwn register - if we have we use that
		if (UObject* ExistingSpawnedObject = Player->GetSpawnRegister().FindSpawnedObject(SpawnableBindingID, SequenceID).Get())
		{
			return;
		}

		// Check whether the binding is overridden - if it is we cannot spawn a new object
		if (const IMovieScenePlaybackClient* PlaybackClient = Player->GetPlaybackClient())
		{
			TArray<UObject*, TInlineAllocator<1>> FoundObjects;
			bool bUseDefaultBinding = PlaybackClient->RetrieveBindingOverrides(SpawnableBindingID, SequenceID, FoundObjects);
			if (!bUseDefaultBinding)
			{
				// This spawnable is overridden so don't try and spawn anything
				return;
			}
		}

		// At this point we've decided that we need to spawn a whole new object
		const UMovieSceneSequence* Sequence = Player->State.FindSequence(SequenceID);
		if (!Sequence)
		{
			return;
		}

		UObject* SpawnedObject = Player->GetSpawnRegister().SpawnObject(SpawnableBindingID, *Sequence->GetMovieScene(), SequenceID, *Player);
		if (SpawnedObject)
		{
			FMovieSceneEvaluationOperand Operand(SequenceID, SpawnableBindingID);
			Player->OnObjectSpawned(SpawnedObject, Operand);

			Player->SavePreAnimatedState(*SpawnedObject, SpawnableAnimTypeID, FSpawnTrackPreAnimatedTokenProducer(Operand));
		}

		InstanceRegistry->InvalidateObjectBinding(SpawnableBindingID, InstanceHandle);
	};
	FEntityTaskBuilder()
	.Read(BuiltInComponents->InstanceHandle)
	.Read(BuiltInComponents->SpawnableBinding)
	.FilterAll({ BuiltInComponents->Tags.NeedsLink })
	.Iterate_PerEntity(&Linker->EntityManager, SpawnNewObjects);

	// ----------------------------------------------------------------------------------------------------------------------------------------
	// Step 2 - destroy any spawnable objects that are no longer relevant
	auto DestroyOldSpawnables = [InstanceRegistry](FInstanceHandle InstanceHandle, const FGuid& SpawnableObjectID)
	{
		SCOPE_CYCLE_COUNTER(MovieSceneEval_DestroySpawnables)

		if (ensure(InstanceRegistry->IsHandleValid(InstanceHandle)))
		{
			const FSequenceInstance& Instance = InstanceRegistry->GetInstance(InstanceHandle);
			IMovieScenePlayer* Player = Instance.GetPlayer();

			// If the sequence instance has finished and it is a sub sequence, we do not destroy the spawnable
			// if it is owned by the master sequence or externally. These will get destroyed or forgotten by the player when it ends
			if (Instance.HasFinished() && Instance.IsSubSequence())
			{
				const UMovieSceneSequence* Sequence = Player->State.FindSequence(Instance.GetSequenceID());
				FMovieSceneSpawnable* Spawnable = Sequence ? Sequence->GetMovieScene()->FindSpawnable(SpawnableObjectID) : nullptr;
				if (!Spawnable || Spawnable->GetSpawnOwnership() != ESpawnOwnership::InnerSequence)
				{
					return;
				}
			}

			Player->GetSpawnRegister().DestroySpawnedObject(SpawnableObjectID, Instance.GetSequenceID(), *Player);
		}
	};

	FEntityTaskBuilder()
	.Read(BuiltInComponents->InstanceHandle)
	.Read(BuiltInComponents->SpawnableBinding)
	.FilterAll({ BuiltInComponents->Tags.NeedsUnlink })
	.Iterate_PerEntity(&Linker->EntityManager, DestroyOldSpawnables);
}

