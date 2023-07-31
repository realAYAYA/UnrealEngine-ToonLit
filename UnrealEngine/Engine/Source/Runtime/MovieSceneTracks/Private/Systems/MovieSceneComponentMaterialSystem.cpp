// Copyright Epic Games, Inc. All Rights Reserved.

#include "Systems/MovieSceneComponentMaterialSystem.h"

#include "Evaluation/PreAnimatedState/MovieScenePreAnimatedStorageID.inl"

#include "EntitySystem/MovieSceneEntityMutations.h"
#include "Evaluation/PreAnimatedState/MovieScenePreAnimatedStorageID.inl"

#include "Systems/FloatChannelEvaluatorSystem.h"
#include "Systems/MovieScenePiecewiseDoubleBlenderSystem.h"

#include "Materials/MaterialInstanceDynamic.h"

#include "Components/PrimitiveComponent.h"
#include "Components/DecalComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MovieSceneComponentMaterialSystem)

namespace UE::MovieScene
{

FComponentMaterialAccessor::FComponentMaterialAccessor(const FComponentMaterialKey& InKey)
	: Object(InKey.Object.ResolveObjectPtr())
	, MaterialIndex(InKey.MaterialIndex)
{}

FComponentMaterialAccessor::FComponentMaterialAccessor(UObject* InObject, int32 InMaterialIndex)
	: Object(InObject)
	, MaterialIndex(InMaterialIndex)
{}

FString FComponentMaterialAccessor::ToString() const
{
	return FString::Printf(TEXT("object %s (element %d)"), *Object->GetPathName(), MaterialIndex);
}

UMaterialInterface* FComponentMaterialAccessor::GetMaterial() const
{
	if (UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(Object))
	{
		return PrimitiveComponent->GetMaterial(MaterialIndex);
	}
	else if (UDecalComponent* DecalComponent = Cast<UDecalComponent>(Object))
	{
		return DecalComponent->GetDecalMaterial();
	}
	return nullptr;
}

void FComponentMaterialAccessor::SetMaterial(UMaterialInterface* InMaterial) const
{
	if (UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(Object))
	{
		PrimitiveComponent->SetMaterial(MaterialIndex, InMaterial);
	}
	else if (UDecalComponent* DecalComponent = Cast<UDecalComponent>(Object))
	{
		DecalComponent->SetDecalMaterial(InMaterial);
	}
}

UMaterialInstanceDynamic* FComponentMaterialAccessor::CreateDynamicMaterial(UMaterialInterface* InMaterial)
{
	// Need to create a new MID, either because the parent has changed, or because one doesn't already exist
	if (UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(Object))
	{
		TStringBuilder<128> DynamicName;
		InMaterial->GetFName().ToString(DynamicName);
		DynamicName.Append(TEXT("_Animated"));
		FName UniqueDynamicName = MakeUniqueObjectName(Object, UMaterialInstanceDynamic::StaticClass() , DynamicName.ToString());

		UMaterialInstanceDynamic* Result = UMaterialInstanceDynamic::Create(InMaterial, Object, UniqueDynamicName );
		PrimitiveComponent->SetMaterial(MaterialIndex, Result);
		return Result;
	}
	else if (UDecalComponent* DecalComponent = Cast<UDecalComponent>(Object))
	{
		return DecalComponent->CreateDynamicMaterialInstance();
	}
	return nullptr;
}

TAutoRegisterPreAnimatedStorageID<FPreAnimatedComponentMaterialSwitcherStorage> FPreAnimatedComponentMaterialSwitcherStorage::StorageID;
TAutoRegisterPreAnimatedStorageID<FPreAnimatedComponentMaterialParameterStorage> FPreAnimatedComponentMaterialParameterStorage::StorageID;

} // namespace UE::MovieScene

UMovieSceneComponentMaterialSystem::UMovieSceneComponentMaterialSystem(const FObjectInitializer& ObjInit)
	: Super(ObjInit)
{
	using namespace UE::MovieScene;

	FBuiltInComponentTypes* BuiltInComponents = FBuiltInComponentTypes::Get();
	FMovieSceneTracksComponentTypes* TracksComponents = FMovieSceneTracksComponentTypes::Get();

	RelevantComponent = TracksComponents->ComponentMaterialIndex;
	Phase = ESystemPhase::Instantiation | ESystemPhase::Evaluation;

	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		DefineComponentConsumer(GetClass(), BuiltInComponents->ObjectResult);
		DefineComponentConsumer(GetClass(), BuiltInComponents->BoundObject);
		DefineComponentProducer(GetClass(), TracksComponents->BoundMaterial);
		DefineImplicitPrerequisite(UMovieSceneCachePreAnimatedStateSystem::StaticClass(), GetClass());
	}
}

void UMovieSceneComponentMaterialSystem::OnLink()
{
	using namespace UE::MovieScene;

	SystemImpl.MaterialSwitcherStorage = Linker->PreAnimatedState.GetOrCreateStorage<FPreAnimatedComponentMaterialSwitcherStorage>();
	SystemImpl.MaterialParameterStorage = Linker->PreAnimatedState.GetOrCreateStorage<FPreAnimatedComponentMaterialParameterStorage>();

	SystemImpl.OnLink(Linker, FBuiltInComponentTypes::Get()->BoundObject, FMovieSceneTracksComponentTypes::Get()->ComponentMaterialIndex);
}

void UMovieSceneComponentMaterialSystem::OnUnlink()
{
	SystemImpl.OnUnlink(Linker);
}

void UMovieSceneComponentMaterialSystem::OnRun(FSystemTaskPrerequisites& InPrerequisites, FSystemSubsequentTasks& Subsequents)
{
	using namespace UE::MovieScene;

	SystemImpl.OnRun(Linker, FBuiltInComponentTypes::Get()->BoundObject, FMovieSceneTracksComponentTypes::Get()->ComponentMaterialIndex, InPrerequisites, Subsequents);
}

void UMovieSceneComponentMaterialSystem::SavePreAnimatedState(const FPreAnimationParameters& InParameters)
{
	using namespace UE::MovieScene;

	SystemImpl.SavePreAnimatedState(Linker, FBuiltInComponentTypes::Get()->BoundObject, FMovieSceneTracksComponentTypes::Get()->ComponentMaterialIndex, InParameters);
}
