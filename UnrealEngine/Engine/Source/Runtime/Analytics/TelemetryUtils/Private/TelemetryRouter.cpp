// Copyright Epic Games, Inc. All Rights Reserved.
 
#include "TelemetryRouter.h"
#include "TelemetryUtils.h"
#include "Misc/ScopeRWLock.h"

FTelemetryRouter::FTelemetryRouter()
{
}

FTelemetryRouter::~FTelemetryRouter()
{
}

FTelemetryRouter& FTelemetryRouter::Get()
{
    return FTelemetryUtils::GetRouter();
}

void FTelemetryRouter::ProvideTelemetryInternal(FGuid Key, const void* Data) 
{
    FReadScopeLock Lock(SinkLock);
    TGuardValue Guard(ReentrancyGuard, FPlatformTLS::GetCurrentThreadId());
    TMap<FDelegateHandle, TFunction<bool(const void*)>>* Sinks = KeyToSinks.Find(Key);
    if (Sinks)
    {
        for (auto It = Sinks->CreateIterator(); It; ++It)
        {
            bool bStillBound = It->Value(Data);
            if (!bStillBound)
            {
                It.RemoveCurrent();
            }
        }
    }
}

void FTelemetryRouter::RegisterTelemetrySinkInternal(FGuid Key, FDelegateHandle InHandle, TFunction<bool(const void*)> Sink)
{
    FWriteScopeLock Lock(SinkLock);
    TGuardValue Guard(ReentrancyGuard, FPlatformTLS::GetCurrentThreadId());
    TMap<FDelegateHandle, TFunction<bool(const void*)>>& Sinks = KeyToSinks.FindOrAdd(Key);
    Sinks.Add(InHandle, MoveTemp(Sink));
}

void FTelemetryRouter::UnregisterTelemetrySinkInternal(FGuid Key, FDelegateHandle InHandle)
{
    FWriteScopeLock Lock(SinkLock);
    TGuardValue Guard(ReentrancyGuard, FPlatformTLS::GetCurrentThreadId());
    TMap<FDelegateHandle, TFunction<bool(const void*)>>* Sinks = KeyToSinks.Find(Key);
    if (Sinks)
    {
        Sinks->Remove(InHandle);
    }
}