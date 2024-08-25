// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "USDTestsBlueprintLibrary.generated.h"

/**
 * Library of functions that can be used via Python scripting to help testing the other USD functionality
 */
UCLASS(meta = (ScriptName = "USDTestingLibrary"))
class USDTESTS_API USDTestsBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	/**
	 * Helps test the effects of blueprint recompilation on the spawned actors and assets when a stage is opened.
	 * Returns whether it compiled successfully or not.
	 */
	UE_DEPRECATED(5.4, "These functions were meant for internal testing only and will be removed in a future release.")
	UFUNCTION(BlueprintCallable, Category = "Blueprint")
	static bool RecompileBlueprintStageActor(AUsdStageActor* BlueprintDerivedStageActor);

	/**
	 * Intentionally dirties the UBlueprint for the given stage actor's generated class.
	 * This is useful for testing how the stage actor behaves when going into PIE with a dirty blueprint, as that usually triggers
	 * a recompile at the very sensitive PIE transition
	 */
	UE_DEPRECATED(5.4, "These functions were meant for internal testing only and will be removed in a future release.")
	UFUNCTION(BlueprintCallable, Category = "Blueprint")
	static void DirtyStageActorBlueprint(AUsdStageActor* BlueprintDerivedStageActor);

	/**
	 * Queries a subtree vertex count using the stage actor's info cache, which is not yet exposed to blueprint.
	 * May return -1 in case of an error.
	 */
	UE_DEPRECATED(5.4, "These functions were meant for internal testing only and will be removed in a future release.")
	UFUNCTION(BlueprintCallable, Category = "Counts")
	static int64 GetSubtreeVertexCount(AUsdStageActor* StageActor, const FString& PrimPath);

	/**
	 * Queries a subtree material slot count using the stage actor's info cache, which is not yet exposed to blueprint.
	 * May return -1 in case of an error.
	 */
	UE_DEPRECATED(5.4, "These functions were meant for internal testing only and will be removed in a future release.")
	UFUNCTION(BlueprintCallable, Category = "Counts")
	static int64 GetSubtreeMaterialSlotCount(AUsdStageActor* StageActor, const FString& PrimPath);

	/**
	 * Internally opens the provided stage and sets it on the stage actor via C++
	 */
	UE_DEPRECATED(5.4, "These functions were meant for internal testing only and will be removed in a future release.")
	UFUNCTION(BlueprintCallable, Category = "Counts")
	static void SetUsdStageCpp(AUsdStageActor* StageActor, const FString& NewStageRootLayer);

	/**
	 * Clears the editor transaction history (useful to be run before GC, to check for otherwise unreferenced assets)
	 */
	UE_DEPRECATED(5.4, "These functions were meant for internal testing only and will be removed in a future release.")
	UFUNCTION(BlueprintCallable, Category = "Transactions")
	static void ClearTransactionHistory();
};
