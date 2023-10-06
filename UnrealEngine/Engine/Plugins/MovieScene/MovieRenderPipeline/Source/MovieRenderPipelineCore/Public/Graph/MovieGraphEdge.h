// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "MovieGraphEdge.generated.h"

class UMovieGraphPin;

UCLASS()
class MOVIERENDERPIPELINECORE_API UMovieGraphEdge : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TObjectPtr<UMovieGraphPin> InputPin;
	
	UPROPERTY()
	TObjectPtr<UMovieGraphPin> OutputPin;

	bool IsValid() const
	{
		return InputPin.Get() && OutputPin.Get();
	}

	UMovieGraphPin* GetOtherPin(const UMovieGraphPin* InFirstPin)
	{
		check(InFirstPin == InputPin || InFirstPin == OutputPin);
		return InFirstPin == InputPin ? OutputPin : InputPin;
	}

};