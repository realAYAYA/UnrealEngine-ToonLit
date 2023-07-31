// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EntitySystem/MovieSceneEntitySystem.h"
#include "EntitySystem/MovieScenePreAnimatedStateSystem.h"
#include "EntitySystem/MovieSceneEntityMutations.h"

#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInstanceDynamic.h"

#include "EntitySystem/BuiltInComponentTypes.h"
#include "EntitySystem/MovieSceneEntitySystemRunner.h"
#include "EntitySystem/MovieSceneComponentTypeInfo.h"
#include "MovieSceneTracksComponentTypes.h"

#include "MovieSceneMaterialSystem.generated.h"

USTRUCT()
struct FMovieScenePreAnimatedMaterialParameters
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UMaterialInterface> PreviousMaterial = nullptr;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> PreviousParameterContainer = nullptr;
};

namespace UE::MovieScene
{

template<typename AccessorType, typename... RequiredComponents>
struct TPreAnimatedMaterialTraits : FBoundObjectPreAnimatedStateTraits
{
	using KeyType     = typename AccessorType::KeyType;
	using StorageType = UMaterialInterface*;

	static_assert(THasAddReferencedObjectForComponent<StorageType>::Value, "StorageType is not correctly exposed to the reference graph!");

	static UMaterialInterface* CachePreAnimatedValue(typename TCallTraits<RequiredComponents>::ParamType... InRequiredComponents)
	{
		return AccessorType{ InRequiredComponents... }.GetMaterial();
	}

	static void RestorePreAnimatedValue(const KeyType& InKeyType, UMaterialInterface* OldMaterial, const FRestoreStateParams& Params)
	{
		AccessorType{ InKeyType }.SetMaterial(OldMaterial);
	}
};

template<typename AccessorType, typename... RequiredComponents>
struct TPreAnimatedMaterialParameterTraits : FBoundObjectPreAnimatedStateTraits
{
	using KeyType     = typename AccessorType::KeyType;
	using StorageType = FMovieScenePreAnimatedMaterialParameters;

	static_assert(THasAddReferencedObjectForComponent<StorageType>::Value, "StorageType is not correctly exposed to the reference graph!");

	static FMovieScenePreAnimatedMaterialParameters CachePreAnimatedValue(typename TCallTraits<RequiredComponents>::ParamType... InRequiredComponents)
	{
		AccessorType Accessor{ InRequiredComponents... };

		FMovieScenePreAnimatedMaterialParameters Parameters;
		Parameters.PreviousMaterial = Accessor.GetMaterial();

		// If the current material we're overriding is already a material instance dynamic, copy it since we will be modifying the data. 
		// The copied material will be used to restore the values in RestoreState.
		UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(Parameters.PreviousMaterial);
		if (MID)
		{
			Parameters.PreviousParameterContainer = DuplicateObject<UMaterialInterface>(MID, MID->GetOuter());
		}

		return Parameters;
	}

	static void RestorePreAnimatedValue(const KeyType& InKey, const FMovieScenePreAnimatedMaterialParameters& PreAnimatedValue, const FRestoreStateParams& Params)
	{
		AccessorType Accessor{ InKey };
		if (PreAnimatedValue.PreviousParameterContainer != nullptr)
		{
			// If we cached parameter values in CachePreAnimatedValue that means the previous material was already a MID
			// and we probably did not replace it with a new one when resolving bound materials. Therefore we
			// just copy the parameters back over without changing the material
			UMaterialInstanceDynamic* CurrentMID = Cast<UMaterialInstanceDynamic>(Accessor.GetMaterial());
			if (CurrentMID)
			{
				CurrentMID->CopyMaterialUniformParameters(PreAnimatedValue.PreviousParameterContainer);
				return;
			}
		}

		Accessor.SetMaterial(PreAnimatedValue.PreviousMaterial);
	}
};

template<typename AccessorType, typename... RequiredComponents>
class TMovieSceneMaterialSystem
{
public:

	using MaterialSwitcherStorageType = TPreAnimatedStateStorage<TPreAnimatedMaterialTraits<AccessorType, RequiredComponents...>>;
	using MaterialParameterStorageType = TPreAnimatedStateStorage<TPreAnimatedMaterialParameterTraits<AccessorType, RequiredComponents...>>;

	TSharedPtr<MaterialSwitcherStorageType> MaterialSwitcherStorage;
	TSharedPtr<MaterialParameterStorageType> MaterialParameterStorage;

	void OnLink(UMovieSceneEntitySystemLinker* Linker, TComponentTypeID<RequiredComponents>... InRequiredComponents);
	void OnUnlink(UMovieSceneEntitySystemLinker* Linker);
	void OnRun(UMovieSceneEntitySystemLinker* Linker, TComponentTypeID<RequiredComponents>... InRequiredComponents, FSystemTaskPrerequisites& InPrerequisites, FSystemSubsequentTasks& Subsequents);

	void SavePreAnimatedState(UMovieSceneEntitySystemLinker* Linker, TComponentTypeID<RequiredComponents>... InRequiredComponents, const IMovieScenePreAnimatedStateSystemInterface::FPreAnimationParameters& InParameters);

	void OnPostSpawn(UMovieSceneEntitySystemLinker* InLinker, TComponentTypeID<RequiredComponents>... InRequiredComponents);

protected:

	UE::MovieScene::FEntityComponentFilter MaterialSwitcherFilter;
	UE::MovieScene::FEntityComponentFilter MaterialParameterFilter;
};

template<typename AccessorType, typename... RequiredComponents>
struct TApplyMaterialSwitchers
{
	static void ForEachEntity(typename TCallTraits<RequiredComponents>::ParamType... Inputs, UObject* ObjectResult)
	{
		// ObjectResult must be a material
		UMaterialInterface* NewMaterial = Cast<UMaterialInterface>(ObjectResult);

		AccessorType Accessor(Inputs...);

		UMaterialInterface*        ExistingMaterial = Accessor.GetMaterial();
		UMaterialInstanceDynamic*  ExistingMID      = Cast<UMaterialInstanceDynamic>(ExistingMaterial);

		if (ExistingMID && ExistingMID->Parent && ExistingMID->Parent == NewMaterial)
		{
			// Do not re-assign materials when a dynamic instance is already assigned with the same parent (since that's basically the same material, just with animated parameters)
			// This is required for supporting material switchers alongside parameter tracks
			return;
		}

		Accessor.SetMaterial(NewMaterial);
	}
};

template<typename AccessorType, typename... RequiredComponents>
struct TInitializeBoundMaterials
{
	static bool InitializeBoundMaterial(typename TCallTraits<RequiredComponents>::ParamType... Inputs, UObject*& OutDynamicMaterial)
	{
		AccessorType Accessor(Inputs...);

		UMaterialInterface* ExistingMaterial = Accessor.GetMaterial();

		if (!ExistingMaterial)
		{
			OutDynamicMaterial = nullptr;
			return true;
		}

		if (UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(ExistingMaterial))
		{
			if (OutDynamicMaterial != MID)
			{
				OutDynamicMaterial = MID;
				return true;
			}
			return false;
		}

		UMaterialInstanceDynamic* CurrentMID = Cast<UMaterialInstanceDynamic>(OutDynamicMaterial);
		if (CurrentMID && CurrentMID->Parent == ExistingMaterial)
		{
			Accessor.SetMaterial(CurrentMID);
			return false;
		}
		
		UMaterialInstanceDynamic* NewMaterial = Accessor.CreateDynamicMaterial(ExistingMaterial);
		OutDynamicMaterial = NewMaterial;
		Accessor.SetMaterial(NewMaterial);
		return true;
	}

	static void ForEachEntity(typename TCallTraits<RequiredComponents>::ParamType... Inputs, UObject*& OutDynamicMaterial)
	{
		InitializeBoundMaterial(Inputs..., OutDynamicMaterial);
	}
};


template<typename AccessorType, typename... RequiredComponents>
struct TReinitializeBoundMaterials
{
	UMovieSceneEntitySystemLinker* Linker;
	TArray<FMovieSceneEntityID> ReboundMaterials;

	TReinitializeBoundMaterials(UMovieSceneEntitySystemLinker* InLinker)
		: Linker(InLinker)
	{}

	void ForEachEntity(FMovieSceneEntityID EntityID, typename TCallTraits<RequiredComponents>::ParamType... Inputs, UObject*& OutDynamicMaterial)
	{
		if (TInitializeBoundMaterials<AccessorType, RequiredComponents...>::InitializeBoundMaterial(Inputs..., OutDynamicMaterial))
		{
			ReboundMaterials.Add(EntityID);
		}
	}

	void PostTask()
	{
		FMovieSceneTracksComponentTypes* TracksComponents = FMovieSceneTracksComponentTypes::Get();
		for (FMovieSceneEntityID EntityID : ReboundMaterials)
		{
			Linker->EntityManager.AddComponent(EntityID, TracksComponents->Tags.BoundMaterialChanged);
		}
	}
};

template<typename...>
struct TAddBoundMaterialMutationImpl;

template<typename AccessorType, typename... RequiredComponents, int... Indices>
struct TAddBoundMaterialMutationImpl<AccessorType, TIntegerSequence<int, Indices...>, RequiredComponents...> : IMovieSceneEntityMutation
{
	TAddBoundMaterialMutationImpl(TComponentTypeID<RequiredComponents>... InRequiredComponents)
		: ComponentTypes(InRequiredComponents...)
	{
		BuiltInComponents = FBuiltInComponentTypes::Get();
		TracksComponents  = FMovieSceneTracksComponentTypes::Get();
	}
	virtual void CreateMutation(FEntityManager* EntityManager, FComponentMask* InOutEntityComponentTypes) const override
	{
		InOutEntityComponentTypes->Set(TracksComponents->BoundMaterial);
	}
	virtual void InitializeAllocation(FEntityAllocation* Allocation, const FComponentMask& AllocationType) const
	{
		TComponentWriter<UObject*> BoundMaterials = Allocation->WriteComponents(TracksComponents->BoundMaterial, FEntityAllocationWriteContext::NewAllocation());
		InitializeAllocation(Allocation, BoundMaterials, Allocation->ReadComponents(ComponentTypes.template Get<Indices>())...);
	}

	void InitializeAllocation(FEntityAllocation* Allocation, UObject** OutBoundMaterials, const RequiredComponents*... InRequiredComponents) const
	{
		const int32 Num = Allocation->Num();
		for (int32 Index = 0; Index < Num; ++Index)
		{
			OutBoundMaterials[Index] = nullptr;
		}
	}
private:

	FBuiltInComponentTypes* BuiltInComponents;
	FMovieSceneTracksComponentTypes* TracksComponents;

	TTuple<TComponentTypeID<RequiredComponents>...> ComponentTypes;
};


template<typename AccessorType, typename... RequiredComponents>
void TMovieSceneMaterialSystem<AccessorType, RequiredComponents...>::OnLink(UMovieSceneEntitySystemLinker* Linker, TComponentTypeID<RequiredComponents>... InRequiredComponents)
{
	using namespace UE::MovieScene;

	FBuiltInComponentTypes*          BuiltInComponents = FBuiltInComponentTypes::Get();
	FMovieSceneTracksComponentTypes* TracksComponents  = FMovieSceneTracksComponentTypes::Get();

	MaterialSwitcherFilter.Reset();
	MaterialSwitcherFilter.All({ InRequiredComponents..., BuiltInComponents->ObjectResult });

	// Currently the only supported entities that we initialize are ones that contain Scalar, Vector or Color parameters
	// Imported entities are implicitly excluded by way of filtering by BoundObject, which do not exist on imported entities
	MaterialParameterFilter.Reset();
	MaterialParameterFilter.All({ InRequiredComponents... });
	MaterialParameterFilter.Any({ TracksComponents->ScalarParameterName, TracksComponents->ColorParameterName, TracksComponents->VectorParameterName });

	Linker->Events.PostSpawnEvent.AddRaw(this, &TMovieSceneMaterialSystem<AccessorType, RequiredComponents...>::OnPostSpawn, InRequiredComponents...);
}

template<typename AccessorType, typename... RequiredComponents>
void TMovieSceneMaterialSystem<AccessorType, RequiredComponents...>::OnUnlink(UMovieSceneEntitySystemLinker* Linker)
{
	Linker->Events.PostSpawnEvent.RemoveAll(this);
}

template<typename AccessorType, typename... RequiredComponents>
void TMovieSceneMaterialSystem<AccessorType, RequiredComponents...>::OnPostSpawn(UMovieSceneEntitySystemLinker* InLinker, TComponentTypeID<RequiredComponents>... InRequiredComponents)
{
	using namespace UE::MovieScene;

	FBuiltInComponentTypes*          BuiltInComponents = FBuiltInComponentTypes::Get();
	FMovieSceneTracksComponentTypes* TracksComponents  = FMovieSceneTracksComponentTypes::Get();

	TReinitializeBoundMaterials<AccessorType, RequiredComponents...> ReinitializeBoundMaterialsTask(InLinker);

	// Reinitialize bound dynamic materials, adding NeedsLink during PostTask to any that changed
	// This will cause the instantiation phase to be re-run for these entities (and any other new or expired ones)
	FEntityTaskBuilder()
	.ReadEntityIDs()
	.ReadAllOf(InRequiredComponents...)
	.Write(TracksComponents->BoundMaterial)
	.FilterNone({ BuiltInComponents->Tags.NeedsUnlink })
	.RunInline_PerEntity(&InLinker->EntityManager, ReinitializeBoundMaterialsTask);
}

template<typename AccessorType, typename... RequiredComponents>
void TMovieSceneMaterialSystem<AccessorType, RequiredComponents...>::OnRun(UMovieSceneEntitySystemLinker* Linker, TComponentTypeID<RequiredComponents>... InRequiredComponents, FSystemTaskPrerequisites& InPrerequisites, FSystemSubsequentTasks& Subsequents)
{
	using namespace UE::MovieScene;

	FBuiltInComponentTypes*          BuiltInComponents = FBuiltInComponentTypes::Get();
	FMovieSceneTracksComponentTypes* TracksComponents  = FMovieSceneTracksComponentTypes::Get();

	FMovieSceneEntitySystemRunner* ActiveRunner = Linker->GetActiveRunner();
	if (!ensure(ActiveRunner))
	{
		return;
	}

	ESystemPhase CurrentPhase = ActiveRunner->GetCurrentPhase();
	if (CurrentPhase == ESystemPhase::Instantiation)
	{
		// --------------------------------------------------------------------------------------
		// Apply material switchers
		TApplyMaterialSwitchers<AccessorType, RequiredComponents...> ApplyMaterialSwitchers;

		FEntityTaskBuilder()
		.ReadAllOf(InRequiredComponents...)
		.Read(BuiltInComponents->ObjectResult)
		.FilterAll({ BuiltInComponents->Tags.NeedsLink })
		.RunInline_PerEntity(&Linker->EntityManager, ApplyMaterialSwitchers);

		// --------------------------------------------------------------------------------------
		// Add bound materials for any NeedsLink entities that have material parameters
		FEntityComponentFilter Filter(MaterialParameterFilter);
		Filter.All({ BuiltInComponents->Tags.NeedsLink });
		Filter.None({ BuiltInComponents->Tags.ImportedEntity });

		using MutationType = TAddBoundMaterialMutationImpl<AccessorType, TMakeIntegerSequence<int, sizeof...(RequiredComponents)>, RequiredComponents...>;
		Linker->EntityManager.MutateAll(Filter, MutationType(InRequiredComponents...));

		// --------------------------------------------------------------------------------------
		// (Re)initialize bound materials for any NeedsLink materials
		TReinitializeBoundMaterials<AccessorType, RequiredComponents...> ReinitializeBoundMaterialsTask(Linker);

		FEntityTaskBuilder()
		.ReadEntityIDs()
		.ReadAllOf(InRequiredComponents...)
		.Write(TracksComponents->BoundMaterial)
		.FilterAll({ BuiltInComponents->Tags.NeedsLink })
		.FilterNone({ BuiltInComponents->Tags.NeedsUnlink })
		.RunInline_PerEntity(&Linker->EntityManager, ReinitializeBoundMaterialsTask);
	}
}

template<typename AccessorType, typename... RequiredComponents>
void TMovieSceneMaterialSystem<AccessorType, RequiredComponents...>::SavePreAnimatedState(UMovieSceneEntitySystemLinker* Linker, TComponentTypeID<RequiredComponents>... InRequiredComponents, const IMovieScenePreAnimatedStateSystemInterface::FPreAnimationParameters& InParameters)
{
	using namespace UE::MovieScene;

	FBuiltInComponentTypes*          BuiltInComponents = FBuiltInComponentTypes::Get();
	FMovieSceneTracksComponentTypes* TracksComponents  = FMovieSceneTracksComponentTypes::Get();

	// If we have material results to apply save those as well
	if (Linker->EntityManager.Contains(MaterialSwitcherFilter))
	{
		TSavePreAnimatedStateParams<RequiredComponents...> Params;
		Params.AdditionalFilter = MaterialSwitcherFilter;

		MaterialSwitcherStorage->BeginTrackingAndCachePreAnimatedValuesTask(Linker, Params, InRequiredComponents...);
	}

	// If we have bound materials to resolve save the current material
	if (Linker->EntityManager.Contains(MaterialParameterFilter))
	{
		TSavePreAnimatedStateParams<RequiredComponents...> Params;
		Params.AdditionalFilter = MaterialParameterFilter;

		MaterialParameterStorage->BeginTrackingAndCachePreAnimatedValuesTask(Linker, Params, InRequiredComponents...);
	}
}

} // namespace UE::MovieScene