// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"
#include "Net/OnlineBlueprintCallProxyBase.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "OculusUpdateSessionCallbackProxy.generated.h"

/**
 * Exposes UpdateSession of the Platform SDK for blueprint use.
 */
UCLASS(MinimalAPI)
class UOculusUpdateSessionCallbackProxy : public UOnlineBlueprintCallProxyBase
{
	GENERATED_UCLASS_BODY()

	// Called when the session was updated successfully
	UPROPERTY(BlueprintAssignable)
	FEmptyOnlineDelegate OnSuccess;

	// Called when there was an error updating the session
	UPROPERTY(BlueprintAssignable)
	FEmptyOnlineDelegate OnFailure;

	// Kick off UpdateSession check. Asynchronous-- see OnUpdateCompleteDelegate for results.
	UFUNCTION(BlueprintCallable, Category = "Oculus|Session", meta = (BlueprintInternalUseOnly = "true"))
	static UOculusUpdateSessionCallbackProxy* SetSessionEnqueue(bool bShouldEnqueueInMatchmakingPool);

	/** UOnlineBlueprintCallProxyBase interface */
	virtual void Activate() override;

private:
	// Internal callback when session updates completes
	void OnUpdateCompleted(FName SessionName, bool bWasSuccessful);

	// The delegate executed by the online subsystem
	FOnUpdateSessionCompleteDelegate UpdateCompleteDelegate;

	// Handles to the registered delegates above
	FDelegateHandle UpdateCompleteDelegateHandle;

	bool bShouldEnqueueInMatchmakingPool;

};
