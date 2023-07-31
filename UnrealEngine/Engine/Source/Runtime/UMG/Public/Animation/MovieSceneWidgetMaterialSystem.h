// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EntitySystem/MovieSceneEntitySystem.h"
#include "Evaluation/PreAnimatedState/MovieScenePreAnimatedStateStorage.h"

#include "Systems/MovieSceneMaterialSystem.h"
#include "Animation/MovieSceneUMGComponentTypes.h"

#include "MovieSceneWidgetMaterialSystem.generated.h"

class UWidget;
class UMovieScenePiecewiseDoubleBlenderSystem;

namespace UE::MovieScene
{

struct FWidgetMaterialKey
{
	FObjectKey Object;
	FWidgetMaterialPath WidgetMaterialPath;

	friend uint32 GetTypeHash(const FWidgetMaterialKey& In)
	{
		uint32 Accumulator = GetTypeHash(In.Object);
		for (const FName& Name : In.WidgetMaterialPath.Path)
		{
			Accumulator ^= GetTypeHash(Name);
		}
		return Accumulator;
	}
	friend bool operator==(const FWidgetMaterialKey& A, const FWidgetMaterialKey& B)
	{
		return A.Object == B.Object && A.WidgetMaterialPath.Path == B.WidgetMaterialPath.Path;
	}
};

struct FWidgetMaterialAccessor
{
	using KeyType = FWidgetMaterialKey;

	UWidget* Widget;
	FWidgetMaterialPath WidgetMaterialPath;

	FWidgetMaterialAccessor(const FWidgetMaterialKey& InKey);
	FWidgetMaterialAccessor(UObject* InObject, FWidgetMaterialPath InWidgetMaterialPath);

	UMaterialInterface* GetMaterial() const;
	void SetMaterial(UMaterialInterface* InMaterial) const;
	UMaterialInstanceDynamic* CreateDynamicMaterial(UMaterialInterface* InMaterial);
	FString ToString() const;
};

using FPreAnimatedWidgetMaterialTraits          = TPreAnimatedMaterialTraits<FWidgetMaterialAccessor, UObject*, FWidgetMaterialPath>;
using FPreAnimatedWidgetMaterialParameterTraits = TPreAnimatedMaterialParameterTraits<FWidgetMaterialAccessor, UObject*, FWidgetMaterialPath>;

struct FPreAnimatedWidgetMaterialSwitcherStorage
	: public TPreAnimatedStateStorage<TPreAnimatedMaterialTraits<FWidgetMaterialAccessor, UObject*, FWidgetMaterialPath>>
{
	static TAutoRegisterPreAnimatedStorageID<FPreAnimatedWidgetMaterialSwitcherStorage> StorageID;
};

struct FPreAnimatedWidgetMaterialParameterStorage
	: public TPreAnimatedStateStorage<TPreAnimatedMaterialParameterTraits<FWidgetMaterialAccessor, UObject*, FWidgetMaterialPath>>
{
	static TAutoRegisterPreAnimatedStorageID<FPreAnimatedWidgetMaterialParameterStorage> StorageID;
};

} // namespace UE::MovieScene


UCLASS(MinimalAPI)
class UMovieSceneWidgetMaterialSystem
	: public UMovieSceneEntitySystem
	, public IMovieScenePreAnimatedStateSystemInterface
{
public:

	GENERATED_BODY()

	UMovieSceneWidgetMaterialSystem(const FObjectInitializer& ObjInit);

private:

	virtual void OnLink() override;
	virtual void OnUnlink() override;
	virtual void OnRun(FSystemTaskPrerequisites& InPrerequisites, FSystemSubsequentTasks& Subsequents) override;

	virtual void SavePreAnimatedState(const FPreAnimationParameters& InParameters) override;

private:

	UE::MovieScene::TMovieSceneMaterialSystem<UE::MovieScene::FWidgetMaterialAccessor, UObject*, UE::MovieScene::FWidgetMaterialPath> SystemImpl;
};
