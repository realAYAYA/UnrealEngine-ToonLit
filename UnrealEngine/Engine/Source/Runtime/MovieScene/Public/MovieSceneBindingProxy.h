// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Misc/Guid.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ObjectPtr.h"

#include "MovieSceneBindingProxy.generated.h"

class UMovieScene;
class UMovieSceneSequence;

//
// Movie Scene Binding Proxy represents the binding ID (an FGuid) and the corresponding sequence that it relates to. 
// This is primarily used for scripting where there is no support for FMovieSceneSequenceID and use cases where 
// managing multiple bindings with their corresponding sequences is necessary.
//
USTRUCT(BlueprintType)
struct MOVIESCENE_API FMovieSceneBindingProxy
{
	GENERATED_BODY()

	FMovieSceneBindingProxy()
		: Sequence(nullptr)
	{}

	FMovieSceneBindingProxy(const FGuid& InBindingID, UMovieSceneSequence* InSequence)
		: BindingID(InBindingID)
		, Sequence(InSequence)
	{}

	UMovieScene* GetMovieScene() const;

	UPROPERTY(BlueprintReadOnly, Category=Binding)
	FGuid BindingID;

	UPROPERTY(BlueprintReadOnly, Category=Binding)
	TObjectPtr<UMovieSceneSequence> Sequence;
};
