// Copyright Epic Games, Inc. All Rights Reserved.

#include "Systems/MovieSceneInitialValueSystem.h"
#include "EntitySystem/MovieSceneEntitySystemLinker.h"
#include "EntitySystem/MovieScenePropertyRegistry.h"
#include "EntitySystem/MovieSceneEntityMutations.h"
#include "EntitySystem/BuiltInComponentTypes.h"
#include "EntitySystem/MovieSceneInitialValueCache.h"
#include "EntitySystem/Interrogation/MovieSceneInterrogationExtension.h"

#include "Systems/MovieScenePropertyInstantiator.h"
#include "EntitySystem/Interrogation/MovieSceneInterrogatedPropertyInstantiator.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MovieSceneInitialValueSystem)

namespace UE
{
namespace MovieScene
{

struct FInitialValueMutation : IMovieSceneEntityMutation
{
	TSortedMap<FComponentTypeID, IInitialValueProcessor*> PropertyTypeToProcessor;
	FInitialValueCache* InitialValueCache;
	FBuiltInComponentTypes* BuiltInComponents;
	FComponentMask AnyInitialValue;

	FInitialValueMutation(UMovieSceneEntitySystemLinker* Linker)
	{
		BuiltInComponents = FBuiltInComponentTypes::Get();

		InitialValueCache = Linker->FindExtension<FInitialValueCache>();
		for (const FPropertyDefinition& Definition : BuiltInComponents->PropertyRegistry.GetProperties())
		{
			IInitialValueProcessor* Processor = Definition.Handler->GetInitialValueProcessor();
			if (Processor && Definition.InitialValueType && Linker->EntityManager.ContainsComponent(Definition.PropertyType))
			{
				Processor->Initialize(Linker, &Definition, InitialValueCache);

				AnyInitialValue.Set(Definition.InitialValueType);
				PropertyTypeToProcessor.Add(Definition.InitialValueType, Processor);
			}
		}
	}

	~FInitialValueMutation()
	{
		for (TPair<FComponentTypeID, IInitialValueProcessor*> Pair : PropertyTypeToProcessor)
		{
			Pair.Value->Finalize();
		}
	}

	bool IsCached() const
	{
		return InitialValueCache != nullptr;
	}

	virtual void CreateMutation(FEntityManager* EntityManager, FComponentMask* InOutEntityComponentTypes) const override
	{
		if (IsCached())
		{
			InOutEntityComponentTypes->Set(BuiltInComponents->InitialValueIndex);
		}
		InOutEntityComponentTypes->Set(BuiltInComponents->Tags.HasAssignedInitialValue);
	}

	virtual void InitializeAllocation(FEntityAllocation* Allocation, const FComponentMask& AllocationType) const
	{
		FComponentTypeID InitialValueType = FComponentMask::BitwiseAND(AllocationType, AnyInitialValue, EBitwiseOperatorFlags::MinSize).First();

		IInitialValueProcessor* Processor = PropertyTypeToProcessor.FindRef(InitialValueType);
		if (ensure(Processor))
		{
			Processor->Process(Allocation, AllocationType);
		}
	}
};

} // namespace MovieScene
} // namespace UE

UMovieSceneInitialValueSystem::UMovieSceneInitialValueSystem(const FObjectInitializer& ObjInit)
	: Super(ObjInit)
{
	SystemCategories = UE::MovieScene::EEntitySystemCategory::Core;

	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		DefineImplicitPrerequisite(UMovieScenePropertyInstantiatorSystem::StaticClass(), StaticClass());
		DefineImplicitPrerequisite(UMovieSceneInterrogatedPropertyInstantiatorSystem::StaticClass(), StaticClass());
	}
}

bool UMovieSceneInitialValueSystem::IsRelevantImpl(UMovieSceneEntitySystemLinker* InLinker) const
{
	using namespace UE::MovieScene;

	FBuiltInComponentTypes* BuiltInComponents = FBuiltInComponentTypes::Get();
	for (const FPropertyDefinition& Definition : BuiltInComponents->PropertyRegistry.GetProperties())
	{
		if (InLinker->EntityManager.ContainsComponent(Definition.InitialValueType))
		{
			return true;
		}
	}
	return false;
}

void UMovieSceneInitialValueSystem::OnLink()
{

}

void UMovieSceneInitialValueSystem::OnUnlink()
{

}

void UMovieSceneInitialValueSystem::OnRun(FSystemTaskPrerequisites& InPrerequisites, FSystemSubsequentTasks& Subsequents)
{
	using namespace UE::MovieScene;

	FBuiltInComponentTypes* BuiltInComponents = FBuiltInComponentTypes::Get();

	FInitialValueMutation Mutation(Linker);

	// If we don't have any initial value processors, we've no work to do
	if (Mutation.AnyInitialValue.NumComponents() == 0)
	{
		return;
	}

	if (Mutation.IsCached() && Linker->FindExtension<IInterrogationExtension>() == nullptr)
	{
		// When there is an initial value cache extension, we mutate anything with an initial value component on it by
		// adding an additional index that refers to its cached position. This ensures we are able to clean up the cache
		// easily.
		{
			FEntityComponentFilter Filter;
			Filter.Any(Mutation.AnyInitialValue);
			Filter.All({ BuiltInComponents->Tags.NeedsLink });
			Filter.None({ BuiltInComponents->InitialValueIndex });
			Filter.None({ BuiltInComponents->Tags.HasAssignedInitialValue });

			Linker->EntityManager.MutateAll(Filter, Mutation);
		}

		// Clean up any stale cache entries
		{
			FEntityComponentFilter Filter;
			Filter.Any(Mutation.AnyInitialValue);
			Filter.All({ BuiltInComponents->InitialValueIndex, BuiltInComponents->Tags.NeedsUnlink });

			for (FEntityAllocationIteratorItem Item : Linker->EntityManager.Iterate(&Filter))
			{
				const FEntityAllocation* Allocation     = Item.GetAllocation();
				FComponentMask           AllocationType = Item.GetAllocationType();

				FComponentTypeID InitialValueType = FComponentMask::BitwiseAND(AllocationType, Mutation.AnyInitialValue, EBitwiseOperatorFlags::MinSize).First();

				TComponentReader<FInitialValueIndex> Indices = Allocation->ReadComponents(BuiltInComponents->InitialValueIndex);
				Mutation.InitialValueCache->Reset(InitialValueType, Indices.AsArray(Allocation->Num()));
			}
		}
	}
	else
	{
		// When there is no caching extension, or we are interrogating we simply initialize any initial values directly without going through the cache
		FEntityComponentFilter Filter;
		Filter.Any(Mutation.AnyInitialValue);
		Filter.Any({ BuiltInComponents->BoundObject, BuiltInComponents->Interrogation.OutputKey });
		Filter.All({ BuiltInComponents->Tags.NeedsLink });
		Filter.None({ BuiltInComponents->Tags.HasAssignedInitialValue });

		Linker->EntityManager.MutateAll(Filter, Mutation);
	}
}

