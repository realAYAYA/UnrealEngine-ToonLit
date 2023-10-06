// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "LeaderboardBlueprintLibrary.generated.h"

class APlayerController;

/**
 * A beacon host used for taking reservations for an existing game session
 */
UCLASS(meta=(ScriptName="LeaderboardLibrary"))
class ONLINESUBSYSTEMUTILS_API ULeaderboardBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()

	/** Writes an integer value to the specified leaderboard */
	UFUNCTION(BlueprintCallable, Category = "Online|Leaderboard")
	static bool WriteLeaderboardInteger(APlayerController* PlayerController, FName StatName, int32 StatValue);

private:
	static bool WriteLeaderboardObject(APlayerController* PlayerController, class FOnlineLeaderboardWrite& WriteObject);
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif
