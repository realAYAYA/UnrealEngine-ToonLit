// Copyright Epic Games, Inc. All Rights Reserved.

#include "OnlineMessageMultiTaskOculus.h"
#include "OnlineSubsystemOculusPrivate.h"


void FOnlineMessageMultiTaskOculus::AddNewRequest(ovrRequest RequestId)
{
	InProgressRequests.Add(RequestId);
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	OculusSubsystem.AddRequestDelegate(
		RequestId,
		FOculusMessageOnCompleteDelegate::CreateLambda([this, RequestId](ovrMessageHandle Message, bool bIsError)
	{
		InProgressRequests.Remove(RequestId);
		if (bIsError)
		{
			bDidAllRequestsFinishedSuccessfully = false;
		}

		if (InProgressRequests.Num() == 0)
		{
			Delegate.ExecuteIfBound();
		}
	}));
PRAGMA_ENABLE_DEPRECATION_WARNINGS
}
