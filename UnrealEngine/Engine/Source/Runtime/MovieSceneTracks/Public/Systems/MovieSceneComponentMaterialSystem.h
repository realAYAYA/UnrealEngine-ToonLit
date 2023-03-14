// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/TypeHash.h"
#include "EntitySystem/MovieSceneEntitySystem.h"
#include "Evaluation/PreAnimatedState/MovieScenePreAnimatedStateStorage.h"

#include "Systems/MovieSceneMaterialSystem.h"

#include "MovieSceneComponentMaterialSystem.generated.h"

class UMovieScenePiecewiseDoubleBlenderSystem;

namespace UE::MovieScene
{

struct FComponentMaterialKey
{
	FObjectKey Object;
	int32 MaterialIndex;

	friend uint32 GetTypeHash(const FComponentMaterialKey& In)
	{
		return GetTypeHash(In.Object) ^ ::GetTypeHash(In.MaterialIndex);
	}
	friend bool operator==(const FComponentMaterialKey& A, const FComponentMaterialKey& B)
	{
		return A.Object == B.Object && A.MaterialIndex == B.MaterialIndex;
	}
};

struct FComponentMaterialAccessor
{
	using KeyType = FComponentMaterialKey;

	UObject* Object;
	int32 MaterialIndex;

	FComponentMaterialAccessor(const FComponentMaterialKey& InKey);
	FComponentMaterialAccessor(UObject* InObject, int32 InMaterialIndex);

	UMaterialInterface* GetMaterial() const;
	void SetMaterial(UMaterialInterface* InMaterial) const;
	UMaterialInstanceDynamic* CreateDynamicMaterial(UMaterialInterface* InMaterial);
	FString ToString() const;
};

using FPreAnimatedComponentMaterialTraits          = TPreAnimatedMaterialTraits<FComponentMaterialAccessor, UObject*, int32>;
using FPreAnimatedComponentMaterialParameterTraits = TPreAnimatedMaterialParameterTraits<FComponentMaterialAccessor, UObject*, int32>;

struct FPreAnimatedComponentMaterialSwitcherStorage
	: public TPreAnimatedStateStorage<TPreAnimatedMaterialTraits<FComponentMaterialAccessor, UObject*, int32>>
{
	static TAutoRegisterPreAnimatedStorageID<FPreAnimatedComponentMaterialSwitcherStorage> StorageID;
};

struct FPreAnimatedComponentMaterialParameterStorage
	: public TPreAnimatedStateStorage<TPreAnimatedMaterialParameterTraits<FComponentMaterialAccessor, UObject*, int32>>
{
	static TAutoRegisterPreAnimatedStorageID<FPreAnimatedComponentMaterialParameterStorage> StorageID;
};

} // namespace UE::MovieScene


UCLASS(MinimalAPI)
class UMovieSceneComponentMaterialSystem
	: public UMovieSceneEntitySystem
	, public IMovieScenePreAnimatedStateSystemInterface
{
public:

	GENERATED_BODY()

	UMovieSceneComponentMaterialSystem(const FObjectInitializer& ObjInit);

private:

	virtual void OnLink() override;
	virtual void OnUnlink() override;
	virtual void OnRun(FSystemTaskPrerequisites& InPrerequisites, FSystemSubsequentTasks& Subsequents) override;

	virtual void SavePreAnimatedState(const FPreAnimationParameters& InParameters) override;

private:

	UE::MovieScene::TMovieSceneMaterialSystem<UE::MovieScene::FComponentMaterialAccessor, UObject*, int32> SystemImpl;
};
