// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EntitySystem/BuiltInComponentTypes.h"
#include "EntitySystem/TrackInstance/MovieSceneTrackInstance.h"
#include "UObject/ObjectMacros.h"
#include "MovieSceneCameraCutTrackInstance.generated.h"

class UMovieSceneCameraCutSection;
namespace UE { namespace MovieScene { struct FCameraCutAnimator; } }

UCLASS()
class UMovieSceneCameraCutTrackInstance : public UMovieSceneTrackInstance
{
	GENERATED_BODY()

private:
	virtual void OnAnimate() override;
	virtual void OnInputAdded(const FMovieSceneTrackInstanceInput& InInput) override;
	virtual void OnInputRemoved(const FMovieSceneTrackInstanceInput& InInput) override;
	virtual void OnEndUpdateInputs() override;
	virtual void OnDestroyed() override;

private:
	struct FCameraCutCache
	{
		TWeakObjectPtr<> LastLockedCamera;
		UE::MovieScene::FInstanceHandle LastInstanceHandle;
		TObjectPtr<UMovieSceneSection> LastSection;
	};

	struct FCameraCutInputInfo
	{
		FMovieSceneTrackInstanceInput Input;
		float GlobalStartTime = 0.f;
	};

	struct FCameraCutUseData
	{
		int32 UseCount = 0;
		bool bValid = false;
		bool bCanBlend = false;
	};

	FCameraCutCache CameraCutCache;
	TMap<IMovieScenePlayer*, FCameraCutUseData> PlayerUseCounts;
	TArray<FCameraCutInputInfo> SortedInputInfos;

	friend struct UE::MovieScene::FCameraCutAnimator;
};

