// Copyright Epic Games, Inc. All Rights Reserved.

#include "Systems/MovieSceneMaterialParameterCollectionSystem.h"

#include "IMovieScenePlayer.h"
#include "MovieSceneSequence.h"
#include "Evaluation/MovieSceneEvaluationTemplateInstance.h"
#include "Evaluation/PreAnimatedState/MovieScenePreAnimatedStorageID.inl"
#include "EntitySystem/BuiltInComponentTypes.h"
#include "EntitySystem/MovieSceneEntityMutations.h"
#include "Evaluation/PreAnimatedState/MovieScenePreAnimatedStorageID.inl"

#include "MovieSceneTracksComponentTypes.h"
#include "Systems/FloatChannelEvaluatorSystem.h"
#include "Systems/MovieScenePiecewiseDoubleBlenderSystem.h"

#include "Logging/MessageLog.h"
#include "Misc/UObjectToken.h"

#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialParameterCollectionInstance.h"

#include "Components/PrimitiveComponent.h"
#include "Components/DecalComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MovieSceneMaterialParameterCollectionSystem)

namespace UE::MovieScene
{


struct FMaterialParameterCollectionKey
{
	TObjectKey<UMaterialParameterCollectionInstance> MPCI;
	FName ParameterName;

	FMaterialParameterCollectionKey(UObject* InBoundMaterial, const FName& InParameterName)
		: MPCI(CastChecked<UMaterialParameterCollectionInstance>(InBoundMaterial))
		, ParameterName(InParameterName)
	{}

	friend uint32 GetTypeHash(const FMaterialParameterCollectionKey& InKey)
	{
		return GetTypeHash(InKey.MPCI) ^ GetTypeHash(InKey.ParameterName);
	}

	friend bool operator==(const FMaterialParameterCollectionKey& A, const FMaterialParameterCollectionKey& B)
	{
		return A.MPCI == B.MPCI && A.ParameterName == B.ParameterName;
	}
};

struct FMaterialParameterCollectionScalarTraits : FBoundObjectPreAnimatedStateTraits
{
	using KeyType = FMaterialParameterCollectionKey;
	using StorageType = float;

	static void ReplaceObject(FMaterialParameterCollectionKey& InOutKey, const FObjectKey& NewObject)
	{
		InOutKey.MPCI = Cast<UMaterialParameterCollectionInstance>(NewObject.ResolveObjectPtr());
	}

	static float CachePreAnimatedValue(UObject* InBoundMaterial, const FName& ParameterName)
	{
		UMaterialParameterCollectionInstance* MPCI = CastChecked<UMaterialParameterCollectionInstance>(InBoundMaterial);

		float ParameterValue = 0.f;
		MPCI->GetScalarParameterValue(ParameterName, ParameterValue);
		return ParameterValue;
	}

	static void RestorePreAnimatedValue(const FMaterialParameterCollectionKey& InKey, float OldValue, const FRestoreStateParams& Params)
	{
		UMaterialParameterCollectionInstance* MPCI = InKey.MPCI.ResolveObjectPtr();
		if (MPCI)
		{
			MPCI->SetScalarParameterValue(InKey.ParameterName, OldValue);
		}
	}
};

struct FMaterialParameterCollectionVectorTraits : FBoundObjectPreAnimatedStateTraits
{
	using KeyType = FMaterialParameterCollectionKey;
	using StorageType = FLinearColor;

	static void ReplaceObject(FMaterialParameterCollectionKey& InOutKey, const FObjectKey& NewObject)
	{
		InOutKey.MPCI = Cast<UMaterialParameterCollectionInstance>(NewObject.ResolveObjectPtr());
	}

	static FLinearColor CachePreAnimatedValue(UObject* InBoundMaterial, const FName& ParameterName)
	{
		UMaterialParameterCollectionInstance* MPCI = CastChecked<UMaterialParameterCollectionInstance>(InBoundMaterial);
		FLinearColor ParameterValue = FLinearColor::White;
		MPCI->GetVectorParameterValue(ParameterName, ParameterValue);
		return ParameterValue;
	}

	static void RestorePreAnimatedValue(const FMaterialParameterCollectionKey& InKey, const FLinearColor& OldValue, const FRestoreStateParams& Params)
	{
		UMaterialParameterCollectionInstance* MPCI = InKey.MPCI.ResolveObjectPtr();
		if (MPCI)
		{
			MPCI->SetVectorParameterValue(InKey.ParameterName, OldValue);
		}
	}
};

struct FPreAnimatedMPCScalarStorage
	: public TPreAnimatedStateStorage<FMaterialParameterCollectionScalarTraits>
{
	static TAutoRegisterPreAnimatedStorageID<FPreAnimatedMPCScalarStorage> StorageID;
};

struct FPreAnimatedMPCVectorStorage
	: public TPreAnimatedStateStorage<FMaterialParameterCollectionVectorTraits>
{
	static TAutoRegisterPreAnimatedStorageID<FPreAnimatedMPCVectorStorage> StorageID;
};


TAutoRegisterPreAnimatedStorageID<FPreAnimatedMPCScalarStorage> FPreAnimatedMPCScalarStorage::StorageID;
TAutoRegisterPreAnimatedStorageID<FPreAnimatedMPCVectorStorage> FPreAnimatedMPCVectorStorage::StorageID;

} // namespace UE::MovieScene

UMovieSceneMaterialParameterCollectionSystem::UMovieSceneMaterialParameterCollectionSystem(const FObjectInitializer& ObjInit)
	: Super(ObjInit)
{
	using namespace UE::MovieScene;

	FBuiltInComponentTypes* BuiltInComponents = FBuiltInComponentTypes::Get();
	FMovieSceneTracksComponentTypes* TracksComponents = FMovieSceneTracksComponentTypes::Get();

	RelevantComponent = TracksComponents->MPC;
	Phase = ESystemPhase::Instantiation;

	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		DefineComponentConsumer(GetClass(), BuiltInComponents->Tags.Master);
		DefineComponentProducer(GetClass(), TracksComponents->BoundMaterial);
		DefineImplicitPrerequisite(UMovieSceneCachePreAnimatedStateSystem::StaticClass(), GetClass());
	}
}

void UMovieSceneMaterialParameterCollectionSystem::OnLink()
{
	using namespace UE::MovieScene;

	ScalarParameterStorage = Linker->PreAnimatedState.GetOrCreateStorage<FPreAnimatedMPCScalarStorage>();
	VectorParameterStorage = Linker->PreAnimatedState.GetOrCreateStorage<FPreAnimatedMPCVectorStorage>();
}

void UMovieSceneMaterialParameterCollectionSystem::OnRun(FSystemTaskPrerequisites& InPrerequisites, FSystemSubsequentTasks& Subsequents)
{
	using namespace UE::MovieScene;

	FBuiltInComponentTypes*          BuiltInComponents = FBuiltInComponentTypes::Get();
	FMovieSceneTracksComponentTypes* TracksComponents  = FMovieSceneTracksComponentTypes::Get();

	struct FAddMPCMutation : IMovieSceneEntityMutation
	{
		FAddMPCMutation(UMovieSceneEntitySystemLinker* InLinker)
			: InstanceRegistry(InLinker->GetInstanceRegistry())
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
			TComponentWriter<UObject*> OutBoundMaterials = Allocation->WriteComponents(TracksComponents->BoundMaterial, FEntityAllocationWriteContext::NewAllocation());
			TComponentReader<UMaterialParameterCollection*> MPCs = Allocation->ReadComponents(TracksComponents->MPC);
			TComponentReader<FInstanceHandle> InstanceHandles = Allocation->ReadComponents(BuiltInComponents->InstanceHandle);

			TOptionalComponentReader<FName> ScalarParameterNames = Allocation->TryReadComponents(TracksComponents->ScalarParameterName);
			TOptionalComponentReader<FName> VectorParameterNames = Allocation->TryReadComponents(TracksComponents->VectorParameterName);
			TOptionalComponentReader<FName> ColorParameterNames = Allocation->TryReadComponents(TracksComponents->ColorParameterName);

			const int32 Num = Allocation->Num();
			for (int32 Index = 0; Index < Num; ++Index)
			{
				OutBoundMaterials[Index] = nullptr;

				UMaterialParameterCollection* Collection = MPCs[Index];
				IMovieScenePlayer* Player = InstanceRegistry->GetInstance(InstanceHandles[Index]).GetPlayer();
				UObject* WorldContextObject = Player->GetPlaybackContext();
				UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
				if (World && Collection)
				{
					UMaterialParameterCollectionInstance* Instance = World->GetParameterCollectionInstance(Collection);
					OutBoundMaterials[Index] = Instance;

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
					if (ensureAlwaysMsgf(Instance != nullptr,
						TEXT("Unable to create MPC instance for %s with World %s. Material parameter collection tracks will not function."),
						*Collection->GetName(), *World->GetName()))
					{
						if (ScalarParameterNames)
						{
							FName Name = ScalarParameterNames[Index];
							if (Collection->GetScalarParameterByName(Name) == nullptr)
							{
								if (!Instance->bLoggedMissingParameterWarning)
								{
									MissingParameters.FindOrAdd(MakeTuple(Instance, Player)).Add(Name.ToString());
								}
							}
						}
						else if (VectorParameterNames || ColorParameterNames)
						{
							FName Name = VectorParameterNames ? VectorParameterNames[Index] : ColorParameterNames[Index];
							if (Collection->GetVectorParameterByName(Name) == nullptr)
							{
								if (!Instance->bLoggedMissingParameterWarning)
								{
									MissingParameters.FindOrAdd(MakeTuple(Instance, Player)).Add(Name.ToString());
								}
							}
						}
					}
#endif
				}
			}
		}

		void Cleanup(UMovieSceneEntitySystemLinker* InLinker)
		{
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST

			for (TPair<TTuple<UMaterialParameterCollectionInstance*, IMovieScenePlayer*>, TArray<FString>>& Pair : MissingParameters)
			{
				FFormatNamedArguments Arguments;
				Arguments.Add(TEXT("ParamNames"), FText::FromString(FString::Join(Pair.Value, TEXT(", "))));
				FMessageLog("PIE").Warning()
					->AddToken(FTextToken::Create(NSLOCTEXT("MaterialParameterCollectionTrack", "InvalidParameterText", "Invalid parameter name or type applied in sequence")))
					->AddToken(FUObjectToken::Create(Pair.Key.Get<1>()->GetEvaluationTemplate().GetSequence(MovieSceneSequenceID::Root)))
					->AddToken(FTextToken::Create(NSLOCTEXT("MaterialParameterCollectionTrack", "OnText", "on")))
					->AddToken(FUObjectToken::Create(Pair.Key.Get<0>()))
					->AddToken(FTextToken::Create(FText::Format(NSLOCTEXT("MaterialParameterCollectionTrack", "InvalidParameterFormatText", "with the following invalid parameters: {ParamNames}."), Arguments)));
				Pair.Key.Get<0>()->bLoggedMissingParameterWarning = true;
			}

#endif
		}
	private:

		FInstanceRegistry* InstanceRegistry;
		FBuiltInComponentTypes* BuiltInComponents;
		FMovieSceneTracksComponentTypes* TracksComponents;

		mutable TMap<TTuple<UMaterialParameterCollectionInstance*, IMovieScenePlayer*>, TArray<FString>> MissingParameters;
	};


	// Only mutate things that are tagged as requiring linking
	FEntityComponentFilter Filter;
	Filter.All({ TracksComponents->MPC, BuiltInComponents->InstanceHandle, BuiltInComponents->Tags.NeedsLink });
	Filter.None({ BuiltInComponents->Tags.ImportedEntity });

	// Initialize bound dynamic materials (for material collection parameters)
	FAddMPCMutation BindMaterialsMutation(Linker);
	Linker->EntityManager.MutateAll(Filter, BindMaterialsMutation);
	BindMaterialsMutation.Cleanup(Linker);
}


void UMovieSceneMaterialParameterCollectionSystem::SavePreAnimatedState(const FPreAnimationParameters& InParameters)
{
	using namespace UE::MovieScene;

	FBuiltInComponentTypes*          BuiltInComponents = FBuiltInComponentTypes::Get();
	FMovieSceneTracksComponentTypes* TracksComponents  = FMovieSceneTracksComponentTypes::Get();

	TSavePreAnimatedStateParams<UObject*, FName> Params;

	Params.AdditionalFilter.All({ TracksComponents->MPC });
	ScalarParameterStorage->BeginTrackingEntitiesTask(Linker, Params, TracksComponents->BoundMaterial, TracksComponents->ScalarParameterName);
	ScalarParameterStorage->CachePreAnimatedValuesTask(Linker, Params, TracksComponents->BoundMaterial, TracksComponents->ScalarParameterName);

	VectorParameterStorage->BeginTrackingEntitiesTask(Linker, Params, TracksComponents->BoundMaterial, TracksComponents->VectorParameterName);
	VectorParameterStorage->BeginTrackingEntitiesTask(Linker, Params, TracksComponents->BoundMaterial, TracksComponents->ColorParameterName);
	VectorParameterStorage->CachePreAnimatedValuesTask(Linker, Params, TracksComponents->BoundMaterial, TracksComponents->VectorParameterName);
	VectorParameterStorage->CachePreAnimatedValuesTask(Linker, Params, TracksComponents->BoundMaterial, TracksComponents->ColorParameterName);
}


