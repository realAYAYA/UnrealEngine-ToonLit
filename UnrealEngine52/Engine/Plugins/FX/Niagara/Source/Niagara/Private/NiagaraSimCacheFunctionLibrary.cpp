// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraSimCacheFunctionLibrary.h"
#include "Engine/Engine.h"
#include "NiagaraComponent.h"
#include "NiagaraSimCacheCapture.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NiagaraSimCacheFunctionLibrary)

void UAsyncNiagaraCaptureSimCache::Activate()
{
	Super::Activate();

	SimCacheCapture.OnCaptureComplete().AddUObject(this, &UAsyncNiagaraCaptureSimCache::CaptureFinished);
	
	SimCacheCapture.CaptureNiagaraSimCache(CaptureSimCache, CreateParameters, CaptureComponent, CaptureParameters);
	
}

void UAsyncNiagaraCaptureSimCache::SetReadyToDestroy()
{
	Super::SetReadyToDestroy();

	if ( CaptureSimCache != nullptr )
	{
		CaptureSimCache->EndWrite();
	}
	CaptureComplete.Broadcast(CaptureSimCache ? CaptureSimCache->IsCacheValid() : false);

	CaptureComponent = nullptr;
}

UAsyncNiagaraCaptureSimCache* UAsyncNiagaraCaptureSimCache::CaptureNiagaraSimCacheMultiFrame(UNiagaraSimCache* SimCache, FNiagaraSimCacheCreateParameters CreateParameters, UNiagaraComponent* NiagaraComponent, UNiagaraSimCache*& OutSimCache, int32 NumFrames, int32 CaptureRate, bool bAdvanceSimulation, float AdvanceDeltaTime)
{
	FNiagaraSimCacheCaptureParameters CaptureParameters;
	
	CaptureParameters.NumFrames = FMath::Max(1, NumFrames);
	CaptureParameters.CaptureRate = FMath::Max(1, CaptureRate);
	CaptureParameters.bManuallyAdvanceSimulation = bAdvanceSimulation;
	CaptureParameters.AdvanceDeltaTime = AdvanceDeltaTime;

	return CaptureNiagaraSimCacheImpl(SimCache, CreateParameters, NiagaraComponent, OutSimCache, CaptureParameters);
}

UAsyncNiagaraCaptureSimCache* UAsyncNiagaraCaptureSimCache::CaptureNiagaraSimCacheImpl(UNiagaraSimCache* SimCache,
	FNiagaraSimCacheCreateParameters CreateParameters, UNiagaraComponent* NiagaraComponent, UNiagaraSimCache*& OutSimCache,
	FNiagaraSimCacheCaptureParameters InCaptureParameters)
{
	UAsyncNiagaraCaptureSimCache* CaptureAction = NewObject<UAsyncNiagaraCaptureSimCache>();

	CaptureAction->CaptureParameters = InCaptureParameters;
	CaptureAction->CreateParameters = CreateParameters;
	CaptureAction->CaptureSimCache = SimCache;
	CaptureAction->CaptureComponent = NiagaraComponent;
	OutSimCache = SimCache;

	return CaptureAction;
}

UAsyncNiagaraCaptureSimCache* UAsyncNiagaraCaptureSimCache::CaptureNiagaraSimCacheUntilComplete(UNiagaraSimCache* SimCache, FNiagaraSimCacheCreateParameters CreateParameters, UNiagaraComponent* NiagaraComponent, UNiagaraSimCache*& OutSimCache, int32 CaptureRate, bool bAdvanceSimulation, float AdvanceDeltaTime)
{
	
	FNiagaraSimCacheCaptureParameters CaptureParameters;
	
	CaptureParameters.NumFrames = 0;
	CaptureParameters.CaptureRate = FMath::Max(1, CaptureRate);
	CaptureParameters.bManuallyAdvanceSimulation = bAdvanceSimulation;
	CaptureParameters.AdvanceDeltaTime = AdvanceDeltaTime;

	return CaptureNiagaraSimCacheImpl(SimCache, CreateParameters, NiagaraComponent, OutSimCache, CaptureParameters);
}

void UAsyncNiagaraCaptureSimCache::CaptureFinished(UNiagaraSimCache* InCapturedSimCache)
{
	CaptureSimCache = InCapturedSimCache;
	SetReadyToDestroy();
}

UNiagaraSimCacheFunctionLibrary::UNiagaraSimCacheFunctionLibrary(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UNiagaraSimCacheFunctionLibrary::CaptureNiagaraSimCacheImmediate(UNiagaraSimCache* SimCache, FNiagaraSimCacheCreateParameters CreateParameters, UNiagaraComponent* NiagaraComponent, UNiagaraSimCache*& OutSimCache, bool bAdvanceSimulation, float AdvanceDeltaTime)
{
	FNiagaraSimCacheCapture SimCacheCapture;

	return SimCacheCapture.CaptureCurrentFrameImmediate(SimCache, CreateParameters, NiagaraComponent, OutSimCache, bAdvanceSimulation, AdvanceDeltaTime);
}

UNiagaraSimCache* UNiagaraSimCacheFunctionLibrary::CreateNiagaraSimCache(UObject* WorldContextObject)
{
	if ( UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) )
	{
		return NewObject<UNiagaraSimCache>(WorldContextObject);
	}

	return nullptr;
}
