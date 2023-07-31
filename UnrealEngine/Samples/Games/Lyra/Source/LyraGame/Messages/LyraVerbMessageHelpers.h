// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameplayEffectTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "LyraVerbMessage.h"
#include "UObject/UObjectGlobals.h"

#include "LyraVerbMessageHelpers.generated.h"

class APlayerController;
class APlayerState;
class UObject;
struct FFrame;


UCLASS()
class LYRAGAME_API ULyraVerbMessageHelpers : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Lyra")
	static APlayerState* GetPlayerStateFromObject(UObject* Object);

	UFUNCTION(BlueprintCallable, Category = "Lyra")
	static APlayerController* GetPlayerControllerFromObject(UObject* Object);

	UFUNCTION(BlueprintCallable, Category = "Lyra")
	static FGameplayCueParameters VerbMessageToCueParameters(const FLyraVerbMessage& Message);

	UFUNCTION(BlueprintCallable, Category = "Lyra")
	static FLyraVerbMessage CueParametersToVerbMessage(const FGameplayCueParameters& Params);
};
