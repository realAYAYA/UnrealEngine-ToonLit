// Copyright Epic Games, Inc. All Rights Reserved.

#include "EntitySystem/MovieSceneBoundSceneComponentInstantiator.h"
#include "EntitySystem/MovieSceneEntityInstantiatorSystem.h"
#include "EntitySystem/MovieSceneEntitySystemTask.h"
#include "EntitySystem/MovieSceneEntityManager.h"
#include "EntitySystem/MovieSceneEntityFactoryTemplates.h"
#include "EntitySystem/BuiltInComponentTypes.h"
#include "EntitySystem/MovieSceneInstanceRegistry.h"
#include "EntitySystem/MovieSceneEntitySystemLinker.h"

#include "MovieSceneCommonHelpers.h"
#include "IMovieScenePlayer.h"

#include "Components/SceneComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MovieSceneBoundSceneComponentInstantiator)

UMovieSceneBoundSceneComponentInstantiator::UMovieSceneBoundSceneComponentInstantiator(const FObjectInitializer& ObjInit)
	: Super(ObjInit)
{
	using namespace UE::MovieScene;

	FBuiltInComponentTypes* Components = FBuiltInComponentTypes::Get();
	RelevantComponent = Components->SceneComponentBinding;

	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		DefineComponentProducer(GetClass(), Components->BoundObject);
		DefineComponentProducer(GetClass(), Components->SymbolicTags.CreatesEntities);
	}
}

void UMovieSceneBoundSceneComponentInstantiator::OnRun(FSystemTaskPrerequisites& InPrerequisites, FSystemSubsequentTasks& Subsequents)
{
	using namespace UE::MovieScene;

	FBuiltInComponentTypes* Components = FBuiltInComponentTypes::Get();

	UnlinkStaleObjectBindings(Components->SceneComponentBinding);

	struct FBoundSceneComponentBatch : FObjectFactoryBatch
	{
		virtual EResolveError ResolveObjects(FInstanceRegistry* InstanceRegistry, FInstanceHandle InstanceHandle, int32 InEntityIndex, const FGuid& ObjectBinding) override
		{
			EResolveError Result = EResolveError::UnresolvedBinding;

			FSequenceInstance& SequenceInstance = InstanceRegistry->MutateInstance(InstanceHandle);

			TArrayView<TWeakObjectPtr<>> BoundObjects = SequenceInstance.GetPlayer()->FindBoundObjects(ObjectBinding, SequenceInstance.GetSequenceID());
			if (BoundObjects.Num() == 0)
			{
				UE_LOG(LogMovieSceneECS, Verbose, TEXT("FBoundSceneComponentBatch::ResolveObjects: No bound objects returned for FGuid: %s"), *ObjectBinding.ToString());
				return Result;
			}

			for (TWeakObjectPtr<> WeakObject : BoundObjects)
			{
				if (UObject* Object = WeakObject.Get())
				{
					if (USceneComponent* SceneComponent = MovieSceneHelpers::SceneComponentFromRuntimeObject(Object))
					{
						if (!ensureMsgf(!FBuiltInComponentTypes::IsBoundObjectGarbage(SceneComponent), TEXT("Attempting to bind an object that is garbage or unreachable")))
						{
							continue;
						}

						// Make a child entity for this resolved binding
						Add(InEntityIndex, SceneComponent);
						Result = EResolveError::None;
					}
				}
				else
				{
					UE_LOG(LogMovieSceneECS, Verbose, TEXT("FBoundSceneComponentBatch::ResolveObjects: Invalid weak object returned for FGuid: %s"), *ObjectBinding.ToString());
				}
			}

			return Result;
		}
	};

	TBoundObjectTask<FBoundSceneComponentBatch> ObjectBindingTask(Linker);

	// Gather all newly instanced entities with an object binding ID
	FEntityTaskBuilder()
	.ReadEntityIDs()
	.Read(Components->InstanceHandle)
	.Read(Components->SceneComponentBinding)
	.FilterAny({ Components->Tags.NeedsLink, Components->Tags.HasUnresolvedBinding })
	.FilterNone({ Components->Tags.NeedsUnlink })
	.RunInline_PerAllocation(&Linker->EntityManager, ObjectBindingTask);
}

