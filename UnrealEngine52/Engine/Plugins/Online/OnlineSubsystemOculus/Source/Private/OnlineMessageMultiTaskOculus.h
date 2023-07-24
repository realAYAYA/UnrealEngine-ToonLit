// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "OnlineSubsystemOculusPrivate.h"
#include "OnlineDelegateMacros.h"
#include "OnlineMessageTaskManagerOculus.h"
#include "OnlineSubsystemOculusPackage.h"

/**
 *
 */
class FOnlineMessageMultiTaskOculus
{
private:
	/** Requests that are waiting to be completed */
	TArray<ovrRequest> InProgressRequests;

protected:
	bool bDidAllRequestsFinishedSuccessfully = true;

	DECLARE_DELEGATE(FFinalizeDelegate);

	FOnlineMessageMultiTaskOculus::FFinalizeDelegate Delegate;

PACKAGE_SCOPE:
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	FOnlineSubsystemOculus& OculusSubsystem;
PRAGMA_ENABLE_DEPRECATION_WARNINGS

	FOnlineMessageMultiTaskOculus(
PRAGMA_DISABLE_DEPRECATION_WARNINGS
		FOnlineSubsystemOculus& InOculusSubsystem,
PRAGMA_ENABLE_DEPRECATION_WARNINGS
		const FOnlineMessageMultiTaskOculus::FFinalizeDelegate& InDelegate)
		: Delegate(InDelegate)
		, OculusSubsystem(InOculusSubsystem)
	{
	}

	void AddNewRequest(ovrRequest RequestId);
};
