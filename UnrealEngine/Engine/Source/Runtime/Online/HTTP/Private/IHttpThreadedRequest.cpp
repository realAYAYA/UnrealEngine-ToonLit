// Copyright Epic Games, Inc. All Rights Reserved.

#include "IHttpThreadedRequest.h"
#include "HttpModule.h"
#include "HttpManager.h"

bool IHttpThreadedRequest::FinishRequestNotInHttpManager()
{
	if (IsInGameThread())
	{
		if (DelegateThreadPolicy == EHttpRequestDelegateThreadPolicy::CompleteOnGameThread)
		{
			FinishRequest();
			return false;
		}
		else
		{
			FHttpModule::Get().GetHttpManager().AddHttpThreadTask([StrongThis = StaticCastSharedRef<IHttpThreadedRequest>(AsShared())]()
				{
					StrongThis->FinishRequest();
				});
			return true;
		}
	}
	else
	{
		if (DelegateThreadPolicy == EHttpRequestDelegateThreadPolicy::CompleteOnHttpThread)
		{
			FinishRequest();
			return false;
		}
		else
		{
			FHttpModule::Get().GetHttpManager().AddGameThreadTask([StrongThis = StaticCastSharedRef<IHttpThreadedRequest>(AsShared())]()
				{
					StrongThis->FinishRequest();
				});
			return true;
		}
	}
}

