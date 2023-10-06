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
#include "Sections/MovieSceneConstrainedSection.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MovieSceneConstraintSystem)

class UTickableConstraint;




static UTickableConstraint* CreateConstraintIfNeeded(FConstraintsManagerController& Controller, FConstraintAndActiveChannel* ConstraintAndActiveChannel, const  FConstraintComponentData& ConstraintChannel)
{
	if (ConstraintAndActiveChannel->Constraint.IsPending())
	{
		ConstraintAndActiveChannel->Constraint.LoadSynchronous();
	}
	UTickableConstraint* Constraint = ConstraintAndActiveChannel->Constraint.Get();
	if (!Constraint) //if constraint doesn't exist it probably got unspawned so recreate it and add it
	{
		Constraint = Controller.AddConstraintFromCopy(ConstraintAndActiveChannel->ConstraintCopyToSpawn);
		ConstraintChannel.Section->ReplaceConstraint(ConstraintAndActiveChannel->ConstraintCopyToSpawn->GetFName(), Constraint);
	}
	else // it's possible that we have it but it's not in the manager, due to manager not being saved with it (due to spawning or undo/redo).
	{
		const TArray< TObjectPtr<UTickableConstraint>>& ConstraintsArray = Controller.GetConstraintsArray();
		if (ConstraintsArray.Find(Constraint) == INDEX_NONE)
		{
			//it's possible it belongs to another Manager
			if (Controller.DoesExistInAnyWorld(Constraint) == false) //it's no where make a copy and add it
			{
				Constraint = Controller.AddConstraintFromCopy(ConstraintAndActiveChannel->ConstraintCopyToSpawn);
				ConstraintChannel.Section->ReplaceConstraint(ConstraintAndActiveChannel->ConstraintCopyToSpawn->GetFName(), Constraint);
			}

		}
	}
	return Constraint;
}



namespace UE::MovieScene
{

struct FPreAnimatedConstraint
{
	FName ConstraintName;
	bool bDeleteIt;
	bool bPreviouslyEnabled;
	TWeakObjectPtr<USceneComponent> SceneComponent;
	TWeakObjectPtr<UMovieScene3DTransformSection> Section;
};

struct FPreAnimatedConstraintTraits : FBoundObjectPreAnimatedStateTraits
{
	/** Key type that stores pre-animated state associated with the object and its constraint name */
	struct FKeyType
	{
		FObjectKey Object;
		FName ConstraintName;

		/** Constructor that takes a BoundObject and ConstraintChannel component */
		FKeyType(UObject* InObject, const FConstraintComponentData& ComponentData)
			: Object(InObject)
			, ConstraintName(ComponentData.ConstraintName)
		{}

		/** Hashing and equality required for storage within a map */
		friend uint32 GetTypeHash(const FKeyType& InKey)
		{
			return HashCombine(GetTypeHash(InKey.Object), GetTypeHash(InKey.ConstraintName));
		}
		friend bool operator==(const FKeyType& A, const FKeyType& B)
		{
			return A.Object == B.Object && A.ConstraintName == B.ConstraintName;
		}
	};

	using KeyType = FKeyType;
	using StorageType = FPreAnimatedConstraint;

	static FPreAnimatedConstraint CachePreAnimatedValue(UObject* InBoundObject, const FConstraintComponentData& ConstraintData)
	{
		USceneComponent* SceneComponent = CastChecked<USceneComponent>(InBoundObject);
		FConstraintsManagerController& Controller = FConstraintsManagerController::Get(SceneComponent->GetWorld());
		if (FConstraintAndActiveChannel* ConstraintAndActiveChannel = ConstraintData.Section->GetConstraintChannel(ConstraintData.ConstraintName))
		{
			if (ConstraintAndActiveChannel->Constraint.IsPending())
			{
				ConstraintAndActiveChannel->Constraint.LoadSynchronous();
			}
			UTickableConstraint* Constraint = ConstraintAndActiveChannel->Constraint.Get();
			const bool bIsActive = Constraint ? Constraint->Active : false;
			const FName Name = Constraint ? Constraint->GetFName() : ConstraintData.ConstraintName;
			//also need to create it
			CreateConstraintIfNeeded(Controller, ConstraintAndActiveChannel, ConstraintData);
			return FPreAnimatedConstraint{ Name, (Constraint == nullptr), bIsActive, SceneComponent, ConstraintData.Section };
		}
		return FPreAnimatedConstraint{ ConstraintData.ConstraintName, false,false, SceneComponent, ConstraintData.Section };
	}

	static void RestorePreAnimatedValue(const FKeyType& InKey, const FPreAnimatedConstraint& OldValue, const FRestoreStateParams& Params)
	{
		if (USceneComponent* SceneComponent = OldValue.SceneComponent.Get())
		{
			if (OldValue.Section.IsValid() == false)
			{
				return;
			}
			if (FConstraintAndActiveChannel* ConstraintAndActiveChannel = OldValue.Section->GetConstraintChannel(OldValue.ConstraintName))
			{				
				if (ConstraintAndActiveChannel->Constraint.IsPending())
				{
					ConstraintAndActiveChannel->Constraint.LoadSynchronous();
				}
				UTickableConstraint* Constraint = ConstraintAndActiveChannel->Constraint.Get();
				if (Constraint)
				{
					Constraint->SetActive(OldValue.bPreviouslyEnabled);

					//we don't delete it since for some reason on first creation we get a restore state and so it immediately get's deleted
					if (OldValue.bDeleteIt)
					{
						UMovieScene3DTransformSection* ConstrainedSection = OldValue.Section.Get();
						if (ConstrainedSection)
						{
							ConstrainedSection->SetDoNoRemoveChannel(true);
						}
						FConstraintsManagerController& Controller = FConstraintsManagerController::Get(SceneComponent->GetWorld());
						Controller.RemoveConstraint(Constraint, true);

						if (ConstrainedSection)
						{
							ConstrainedSection->SetDoNoRemoveChannel(false);
						}
					}
				
				}
			}
		}
	}
};


struct FEvaluateConstraintChannels
{
	FEvaluateConstraintChannels(UWorld* InWorld, FInstanceRegistry* InInstanceRegistry, UMovieSceneConstraintSystem* InSystem)
		: World(InWorld), InstanceRegistry(InInstanceRegistry), Controller(nullptr), System(InSystem)
	{
		check(World && InstanceRegistry && System);
	}

	void PreTask()
	{
		// Retrieve the controller at the start of the task since this task is now stored persistently
		Controller = &FConstraintsManagerController::Get(World);
		check(Controller);

		System->DynamicOffsets.Reset();
	}

	void ForEachEntity(UObject* BoundObject, FInstanceHandle InstanceHandle, const FConstraintComponentData& ConstraintChannel, FFrameTime FrameTime) const
	{
		FConstraintAndActiveChannel* ConstraintAndActiveChannel = ConstraintChannel.Section->GetConstraintChannel(ConstraintChannel.ConstraintName);
		if (!ConstraintAndActiveChannel)
		{
			return;
		}

		UTickableConstraint* Constraint = CreateConstraintIfNeeded(*Controller, ConstraintAndActiveChannel, ConstraintChannel);
		if (!Constraint)
		{
			return;
		}

		const FSequenceInstance& TargetInstance = InstanceRegistry->GetInstance(InstanceHandle);

		bool Result = false;
		ConstraintAndActiveChannel->ActiveChannel.Evaluate(FrameTime, Result);
		Constraint->SetActive(Result);
		Constraint->ResolveBoundObjects(TargetInstance.GetSequenceID(), *TargetInstance.GetPlayer());
		if (UTickableTransformConstraint* TransformConstraint = Cast< UTickableTransformConstraint>(Constraint))
		{
			if (UTransformableComponentHandle* ComponentHandle = Cast<UTransformableComponentHandle>(TransformConstraint->ChildTRSHandle))
			{
				//bound component may change so need to update constraint
				ComponentHandle->Component = Cast<USceneComponent>(BoundObject);
				UMovieSceneConstraintSystem::FUpdateHandleForConstraint UpdateHandle;
				UpdateHandle.Constraint = TransformConstraint;
				UpdateHandle.TransformHandle = ComponentHandle;
				System->DynamicOffsets.Add(UpdateHandle);
			}
		}
	}

	UWorld* World;
	FInstanceRegistry* InstanceRegistry;
	FConstraintsManagerController* Controller;
	UMovieSceneConstraintSystem* System;
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
	Phase = ESystemPhase::Instantiation | ESystemPhase::Scheduling | ESystemPhase::Finalization;

	if (HasAnyFlags(RF_ClassDefaultObject) ) 
	{
		// Constraints must be evaluated before their transforms are evaluated.
		// This is only really necessary if they are in the same phase (which they are not), but I've
		// defined the prerequisite for safety if its phase changes in future
		DefineImplicitPrerequisite(GetClass(), UMovieSceneComponentTransformSystem::StaticClass());
		DefineComponentConsumer(GetClass(), FBuiltInComponentTypes::Get()->EvalTime);
	}
}

void UMovieSceneConstraintSystem::OnSchedulePersistentTasks(UE::MovieScene::IEntitySystemScheduler* TaskScheduler)
{
	using namespace UE::MovieScene;

	FMovieSceneTracksComponentTypes* TracksComponents = FMovieSceneTracksComponentTypes::Get();
	FBuiltInComponentTypes* BuiltInComponents = FBuiltInComponentTypes::Get();

	// Set up new constraints
	FEntityTaskBuilder()
	.Read(BuiltInComponents->BoundObject)
	.Read(BuiltInComponents->InstanceHandle)
	.Read(TracksComponents->ConstraintChannel)
	.Read(BuiltInComponents->EvalTime)
	.SetDesiredThread(Linker->EntityManager.GetGatherThread())
	.Schedule_PerEntity<FEvaluateConstraintChannels>(&Linker->EntityManager, TaskScheduler, GetWorld(), Linker->GetInstanceRegistry(), this);
}

void UMovieSceneConstraintSystem::OnRun(FSystemTaskPrerequisites& InPrerequisites, FSystemSubsequentTasks& Subsequents)
{
	using namespace UE::MovieScene;

	FMovieSceneEntitySystemRunner* ActiveRunner = Linker->GetActiveRunner();
	ESystemPhase CurrentPhase = ActiveRunner->GetCurrentPhase();

	if (CurrentPhase == ESystemPhase::Instantiation)
	{
		// Save pre-animated state
		TSharedPtr<FPreAnimatedConstraintStorage> PreAnimatedStorage = Linker->PreAnimatedState.GetOrCreateStorage<FPreAnimatedConstraintStorage>();

		FMovieSceneTracksComponentTypes* TracksComponents = FMovieSceneTracksComponentTypes::Get();
		FBuiltInComponentTypes* BuiltInComponents = FBuiltInComponentTypes::Get();
		PreAnimatedStorage->BeginTrackingAndCachePreAnimatedValues(Linker, BuiltInComponents->BoundObject, TracksComponents->ConstraintChannel);

	}
	else if (CurrentPhase == ESystemPhase::Evaluation)
	{
		// Backwards compat with legacy non-persistent tasks
		FMovieSceneTracksComponentTypes* TracksComponents = FMovieSceneTracksComponentTypes::Get();
		FBuiltInComponentTypes* BuiltInComponents = FBuiltInComponentTypes::Get();

		// Set up new constraints
		FEntityTaskBuilder()
		.Read(BuiltInComponents->BoundObject)
		.Read(BuiltInComponents->InstanceHandle)
		.Read(TracksComponents->ConstraintChannel)
		.Read(BuiltInComponents->EvalTime)
		.SetDesiredThread(Linker->EntityManager.GetGatherThread())
		.Dispatch_PerEntity<FEvaluateConstraintChannels>(&Linker->EntityManager, InPrerequisites, &Subsequents, GetWorld(), Linker->GetInstanceRegistry(), this);
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
