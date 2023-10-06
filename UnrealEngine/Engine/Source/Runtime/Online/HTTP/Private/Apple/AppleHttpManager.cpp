// Copyright Epic Games, Inc. All Rights Reserved.

#include "Apple/AppleHttpManager.h"

#include "Apple/AppleEventLoopHttpThread.h"
#include "HAL/IConsoleManager.h"
#include "Math/UnrealMathUtility.h"
#include "HttpModule.h"
#include "Http.h"

TAutoConsoleVariable<int32> CVarAppleEventLoopEnableChance(
	TEXT("http.AppleEventLoopEnableChance"),
	0,
	TEXT("Enable chance of Apple event loop, from 0 to 100"),
	ECVF_SaveForNextBoot
);

FHttpThreadBase* FAppleHttpManager::CreateHttpThread()
{
	bool bUseEventLoop = !(FMath::RandRange(0, 99) < CVarAppleEventLoopEnableChance.GetValueOnGameThread());
	if (bUseEventLoop)
	{
		UE_LOG(LogHttp, Log, TEXT("CreateHttpThread using FAppleEventLoopHttpThread"));
		return new FAppleEventLoopHttpThread();
	}

	UE_LOG(LogHttp, Log, TEXT("CreateHttpThread using FLegacyHttpThread"));
	return new FLegacyHttpThread();
}
