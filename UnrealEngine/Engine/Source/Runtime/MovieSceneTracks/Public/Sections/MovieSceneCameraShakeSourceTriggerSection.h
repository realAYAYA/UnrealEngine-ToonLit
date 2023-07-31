// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Camera/CameraShakeBase.h"
#include "Channels/MovieSceneCameraShakeSourceTriggerChannel.h"
#include "CoreMinimal.h"
#include "MovieSceneSection.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"

#include "MovieSceneCameraShakeSourceTriggerSection.generated.h"

class UObject;

UCLASS(MinimalAPI)
class UMovieSceneCameraShakeSourceTriggerSection 
	: public UMovieSceneSection
{
public:
	GENERATED_BODY()

	UMovieSceneCameraShakeSourceTriggerSection(const FObjectInitializer& Init);

	const FMovieSceneCameraShakeSourceTriggerChannel& GetChannel() const { return Channel; }

private:
	UPROPERTY()
	FMovieSceneCameraShakeSourceTriggerChannel Channel;
};

