// Copyright Epic Games, Inc. All Rights Reserved.

#include "Systems/MovieSceneConstraintSystem.h"
#include "ConstraintsManager.h"
#include "MovieSceneTracksComponentTypes.h"
#include "Sections/MovieScene3DTransformSection.h"
#include "EntitySystem/MovieSceneEntitySystemRunner.h"
#include "EntitySystem/MovieSceneBoundObjectInstantiator.h"
#include "EntitySystem/MovieSceneBoundSceneComponentInstantiator.h"

#include "PreAnimatedState/MovieScenePreAnimatedComponentTransformStorage.h"
#include "Evaluation/PreAnimatedState/MovieScenePreAnimatedStorageID.inl"

#include "EntitySystem/BuiltInComponentTypes.h"
#include "MovieSceneTracksComponentTypes.h"
#include "Systems/MovieSceneComponentTransformSystem.h"
#include "TransformConstraint.h"
#include "TransformableHandle.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MovieSceneConstraintSystem)

class UTickableConstraint;

namespace UE::MovieScene
{

struct FPreAnimatedConstraint
{
	TWeakObjectPtr<UTickableConstraint> WeakConstraint;
	bool bPreviouslyEnabled;
};

struct FPreAnimatedConstraintTraits : FBoundObjectPreAnimatedStateTraits
{
	using KeyType = TTuple<FObjectKey, FName>;
	using StorageType = FPreAnimatedConstraint;

	static FPreAnimatedConstraint CachePreAnimatedValue(UObject* InBoundObject, const FName& ConstraintName)
	{
		USceneComponent* SceneComponent = CastChecked<USceneComponent>(InBoundObject);
		FConstraintsManagerController& Controller = FConstraintsManagerController::Get(SceneComponent->GetWorld());

		// @todo: I don't know how to store pre-animated values - do we need to enable/disable constraints or add/remove them?
		//        Maybe we just need to also cache some other state other than Constraint->Active
		UTickableConstraint* Constraint = Controller.GetConstraint(ConstraintName);
		return FPreAnimatedConstraint{ Constraint, Constraint ? Constraint->Active : false };
	}

	static void RestorePreAnimatedValue(const TTuple<FObjectKey, FName>& InKey, const FPreAnimatedConstraint& OldValue, const FRestoreStateParams& Params)
	{
		if (UTickableConstraint* Constraint = OldValue.WeakConstraint.Get())
		{
			Constraint->SetActive(OldValue.bPreviouslyEnabled);
		}
	}
};

struct FPreAnimatedConstraintStorage
	: public TPreAnimatedStateStorage<FPreAnimatedConstraintTraits>
{
	static TAutoRegisterPreAnimatedStorageID<FPreAnimatedConstraintStorage> StorageID;
};

TAutoRegisterPreAnimatedStorageID<FPreAnimatedConstraintStorage> FPreAnimatedConstraintStorage::StorageID;

} // namespace UE::MovieScene

UMovieSceneConstraintSystem::UMovieSceneConstraintSystem(const FObjectInitializer& ObjInit)
	: Super(ObjInit)
{
	using namespace UE::MovieScene;

	FMovieSceneTracksComponentTypes* TracksComponents = FMovieSceneTracksComponentTypes::Get();

	RelevantComponent = TracksComponents->ConstraintChannel;

	// Run constraints during instantiation or evaluation
	Phase = ESystemPhase::Instantiation | ESystemPhase::Evaluation | ESystemPhase::Finalization;

	if (HasAnyFlags(RF_ClassDefaultObject) ) 
	{
		// Constraints must be evaluated before their transforms are evaluated.
		// This is only really necessary if they are in the same phase (which they are not), but I've
		// defined the prerequisite for safety if its phase changes in future
		DefineImplicitPrerequisite(GetClass(), UMovieSceneComponentTransformSystem::StaticClass());
		DefineComponentConsumer(GetClass(), UE::MovieScene::FBuiltInComponentTypes::Get()->EvalTime);
	}
}

void UMovieSceneConstraintSystem::OnRun(FSystemTaskPrerequisites& InPrerequisites, FSystemSubsequentTasks& Subsequents)
{
	using namespace UE::MovieScene;

	FMovieSceneEntitySystemRunner* ActiveRunner = Linker->GetActiveRunner();
	ESystemPhase CurrentPhase = ActiveRunner->GetCurrentPhase();

	if (CurrentPhase == ESystemPhase::Instantiation)
	{
		// Save pre-animated state
		//TSharedPtr<FPreAnimatedConstraintStorage> PreAnimatedStorage = Linker->PreAnimatedState.GetOrCreateStorage<FPreAnimatedConstraintStorage>();

		//andrew, had to remove this, last parameter not the correct variadic type
		//PreAnimatedStorage->BeginTrackingAndCachePreAnimatedValues(Linker, BuiltInComponents->BoundObject, TracksComponents->ConstraintName);	
	}
	else if (CurrentPhase == ESystemPhase::Evaluation)
	{
		DynamicOffsets.Reset();
		FConstraintsManagerController& Controller = FConstraintsManagerController::Get(GetWorld());
		FMovieSceneTracksComponentTypes* TracksComponents = FMovieSceneTracksComponentTypes::Get();
		FBuiltInComponentTypes* BuiltInComponents = FBuiltInComponentTypes::Get();

		struct FEvaluateConstraintChannels
		{
			FEvaluateConstraintChannels(FInstanceRegistry* InInstanceRegistry, FConstraintsManagerController* InController, UMovieSceneConstraintSystem* InSystem) : InstanceRegistry(InInstanceRegistry), Controller(InController), System(InSystem) { check(InstanceRegistry); check(Controller); check(System) };
			void ForEachEntity(UObject* BoundObject, UE::MovieScene::FInstanceHandle InstanceHandle, const FConstraintComponentData& ConstraintChannel, FFrameTime FrameTime)
			{
				const FSequenceInstance& TargetInstance = InstanceRegistry->GetInstance(InstanceHandle);

				if (ConstraintChannel.ConstraintAndActiveChannel->Constraint.IsPending())
				{
					ConstraintChannel.ConstraintAndActiveChannel->Constraint.LoadSynchronous();
				}
				UTickableConstraint* Constraint = ConstraintChannel.ConstraintAndActiveChannel->Constraint.Get();
				if (!Constraint) //if constraint doesn't exist it probably got unspawned so recreate it and add it
				{
					UTickableConstraint* NewOne = Controller->AddConstraintFromCopy(ConstraintChannel.ConstraintAndActiveChannel->ConstraintCopyToSpawn);
					Constraint = Controller->GetConstraint(ConstraintChannel.ConstraintName);
					ConstraintChannel.Section->ReplaceConstraint(ConstraintChannel.ConstraintName, Constraint);
				}
				else // it's possible that we have it but it's not in the manager, due to manager not being saved with it (due to spawning or undo/redo).
				{
					const TArray< TObjectPtr<UTickableConstraint>>& ConstraintsArray = Controller->GetConstraintsArray();
					if(ConstraintsArray.Find(Constraint) == INDEX_NONE)
					{
						Controller->AddConstraint(Constraint);
					}
				}
				if (Constraint)
				{
					bool Result = false;
					ConstraintChannel.ConstraintAndActiveChannel->ActiveChannel.Evaluate(FrameTime, Result);
					Constraint->SetActive(Result);
					Constraint->ResolveBoundObjects(TargetInstance.GetSequenceID(), *TargetInstance.GetPlayer());
					if (UTickableTransformConstraint* TransformConstraint = Cast< UTickableTransformConstraint>(Constraint))
					{
						if (UTransformableComponentHandle* ComponentHandle = Cast<UTransformableComponentHandle>(TransformConstraint->ChildTRSHandle))
						{
							FUpdateHandleForConstraint UpdateHandle;
							UpdateHandle.Constraint = TransformConstraint;
							UpdateHandle.TransformHandle = ComponentHandle;
							System->DynamicOffsets.Add(UpdateHandle);
						}
					}
				}
			}
			FInstanceRegistry* InstanceRegistry;
			FConstraintsManagerController* Controller;
			UMovieSceneConstraintSystem* System;
		};

		// Set up new constraints
		FEntityTaskBuilder()
			.Read(BuiltInComponents->BoundObject)
			.Read(BuiltInComponents->InstanceHandle)
			.Read(TracksComponents->ConstraintChannel)
			.Read(BuiltInComponents->EvalTime)
			.SetDesiredThread(Linker->EntityManager.GetGatherThread())
			.Dispatch_PerEntity<FEvaluateConstraintChannels>(&Linker->EntityManager, InPrerequisites, &Subsequents, Linker->GetInstanceRegistry(), &Controller, this);
	}
	else if (CurrentPhase == ESystemPhase::Finalization)
	{
		for (FUpdateHandleForConstraint& UpdateHandle : DynamicOffsets)
		{
			if (UpdateHandle.Constraint.IsValid() && UpdateHandle.TransformHandle.IsValid())
			{
				UpdateHandle.Constraint->OnHandleModified(UpdateHandle.TransformHandle.Get(),EHandleEvent::LocalTransformUpdated);
			}
		}
		DynamicOffsets.Reset();
	}
}



