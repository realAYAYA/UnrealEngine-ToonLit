// Copyright Epic Games, Inc. All Rights Reserved.

#include "EntitySystem/MovieSceneBindingLifetimeSystem.h"
#include "EntitySystem/MovieSceneEntitySystemLinker.h"
#include "EntitySystem/MovieSceneEntitySystemRunner.h"
#include "EntitySystem/MovieSceneEntitySystemTask.h"
#include "EntitySystem/BuiltInComponentTypes.h"

#include "Evaluation/MovieSceneEvaluationTemplateInstance.h"
#include "IMovieScenePlayer.h"
#include "MovieSceneSequence.h"

#include "EntitySystem/MovieSceneEntitySystemTypes.h"
#include "EntitySystem/MovieSceneEntityRange.h"
#include "EntitySystem/MovieSceneEntitySystemTask.h"
#include "EntitySystem/MovieSceneEntityInstantiatorSystem.h"
#include "Evaluation/MovieSceneEvaluationOperand.h"

#include "MovieScene.h"
#include "MovieSceneExecutionToken.h"
#include "IMovieScenePlaybackClient.h"
#include "EntitySystem/MovieSceneSpawnablesSystem.h"
#include "MovieSceneBindingEventReceiverInterface.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MovieSceneBindingLifetimeSystem)
#define LOCTEXT_NAMESPACE "MovieSceneBindingLifetimeSystem"

namespace UE
{
	namespace MovieScene
	{
		const FMovieSceneAnimTypeID BindingLifetimeAnimTypeID = FMovieSceneAnimTypeID::Unique();

		struct FBindingLifetimeTrackPreAnimatedTokenProducer : IMovieScenePreAnimatedTokenProducer
		{
			FMovieSceneEvaluationOperand Operand;
			FBindingLifetimeTrackPreAnimatedTokenProducer(FMovieSceneEvaluationOperand InOperand) : Operand(InOperand) {}

			virtual IMovieScenePreAnimatedTokenPtr CacheExistingState(UObject& Object) const
			{
				struct FToken : IMovieScenePreAnimatedToken
				{
					FMovieSceneEvaluationOperand OperandToDestroy;
					FToken(FMovieSceneEvaluationOperand InOperand) : OperandToDestroy(InOperand) {}

					virtual void RestoreState(UObject& Object, const UE::MovieScene::FRestoreStateParams& Params) override
					{
						TSharedPtr<const FSharedPlaybackState> PlaybackState = Params.GetTerminalPlaybackState();
						if (!ensure(PlaybackState))
						{
							return;
						}

						// Destroy any loaded bindings
						if (FMovieSceneEvaluationState* EvaluationState = PlaybackState->FindCapability<FMovieSceneEvaluationState>())
						{
							if (FMovieSceneObjectCache* Cache = EvaluationState->FindObjectCache(OperandToDestroy.SequenceID))
							{
								Cache->UnloadBinding(OperandToDestroy.ObjectBindingID, PlaybackState.ToSharedRef());
							}
						}
					}
				};

				return FToken(Operand);
			}
		};

	} // namespace MovieScene
} // namespace UE


UMovieSceneBindingLifetimeSystem::UMovieSceneBindingLifetimeSystem(const FObjectInitializer& ObjInit)
	: Super(ObjInit)
{
	using namespace UE::MovieScene;

	Phase = UE::MovieScene::ESystemPhase::Spawn;
	FBuiltInComponentTypes* BuiltInComponentTypes = FBuiltInComponentTypes::Get();
	RelevantComponent = BuiltInComponentTypes->BindingLifetime;

	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		DefineImplicitPrerequisite(GetClass(), UMovieSceneSpawnablesSystem::StaticClass());
	}
}

FMovieSceneAnimTypeID UMovieSceneBindingLifetimeSystem::GetAnimTypeID()
{
	return UE::MovieScene::BindingLifetimeAnimTypeID;
}

void UMovieSceneBindingLifetimeSystem::OnRun(FSystemTaskPrerequisites& InPrerequisites, FSystemSubsequentTasks& Subsequents)
{
	using namespace UE::MovieScene;
	FBuiltInComponentTypes* BuiltInComponents = FBuiltInComponentTypes::Get();
	if (!Linker->EntityManager.Contains(FEntityComponentFilter().Any({ BuiltInComponents->Tags.NeedsLink, BuiltInComponents->Tags.NeedsUnlink })))
	{
		return;
	}

	FInstanceRegistry* InstanceRegistry = Linker->GetInstanceRegistry();

	bool bLink = false;
	auto SetBindingActivation = [InstanceRegistry, &bLink](FMovieSceneEntityID EntityID, FInstanceHandle InstanceHandle, const FMovieSceneBindingLifetimeComponentData& BindingLifetime)
	{
		const FSequenceInstance& SequenceInstance = InstanceRegistry->GetInstance(InstanceHandle);

		FMovieSceneSequenceID SequenceID = SequenceInstance.GetSequenceID();
		IMovieScenePlayer* Player = SequenceInstance.GetPlayer();
		if (Player)
		{
			// For now we use the linking/unlinking of the inactive ranges to set the binding activations
			if (BindingLifetime.BindingLifetimeState == EMovieSceneBindingLifetimeState::Inactive)
			{
				Player->State.SetBindingActivation(BindingLifetime.BindingGuid, SequenceID, !bLink);
			}
			else
			{
				for (TWeakObjectPtr<> WeakBoundObject : Player->FindBoundObjects(BindingLifetime.BindingGuid, SequenceID))
				{
					if (UObject* BoundObject = WeakBoundObject.Get())
					{
						if (BoundObject->Implements<UMovieSceneBindingEventReceiverInterface>())
						{
							TScriptInterface<IMovieSceneBindingEventReceiverInterface> BindingEventReceiver = BoundObject;
							if (BindingEventReceiver.GetObject())
							{
								FMovieSceneObjectBindingID BindingID = UE::MovieScene::FRelativeObjectBindingID(MovieSceneSequenceID::Root, SequenceID, BindingLifetime.BindingGuid, *Player);
								if (bLink)
								{
									IMovieSceneBindingEventReceiverInterface::Execute_OnObjectBoundBySequencer(BindingEventReceiver.GetObject(), Cast<UMovieSceneSequencePlayer>(Player->AsUObject()), BindingID);
								}
								else
								{
									IMovieSceneBindingEventReceiverInterface::Execute_OnObjectUnboundBySequencer(BindingEventReceiver.GetObject(), Cast<UMovieSceneSequencePlayer>(Player->AsUObject()), BindingID);
								}
							}
						}

						if (bLink)
						{
							Player->PreAnimatedState.SavePreAnimatedState(*BoundObject, BindingLifetimeAnimTypeID, FBindingLifetimeTrackPreAnimatedTokenProducer({SequenceID, BindingLifetime.BindingGuid}));
						}
					}
				}
				if (!bLink)
				{
					// In addition, on unlink of the active range, unload any loaded objects with that binding id
					if (FMovieSceneObjectCache* Cache = Player->State.FindObjectCache(SequenceID))
					{
						Cache->UnloadBinding(BindingLifetime.BindingGuid, Player->GetSharedPlaybackState());
					}
				}
			}
		}
	};

	FBuiltInComponentTypes* BuiltInComponentTypes = FBuiltInComponentTypes::Get();

	// Unlink stale bindinglifetime entities
	FEntityTaskBuilder()
		.ReadEntityIDs()
		.Read(BuiltInComponents->InstanceHandle)
		.Read(BuiltInComponentTypes->BindingLifetime)
		.FilterAll({ BuiltInComponents->Tags.NeedsUnlink })
		.Iterate_PerEntity(&Linker->EntityManager, SetBindingActivation);

	// Link new bindinglifetime entities
	bLink = true;
	FEntityTaskBuilder()
		.ReadEntityIDs()
		.Read(BuiltInComponents->InstanceHandle)
		.Read(BuiltInComponentTypes->BindingLifetime)
		.FilterAll({ BuiltInComponents->Tags.NeedsLink })
		.Iterate_PerEntity(&Linker->EntityManager, SetBindingActivation);
}

#undef LOCTEXT_NAMESPACE

