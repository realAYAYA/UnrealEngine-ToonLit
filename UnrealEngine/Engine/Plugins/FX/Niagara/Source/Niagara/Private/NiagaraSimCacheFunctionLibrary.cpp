// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraSimCacheFunctionLibrary.h"
#include "NiagaraComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NiagaraSimCacheFunctionLibrary)

void UAsyncNiagaraCaptureSimCache::Activate()
{
	Super::Activate();

	// In this path we are going to manually advance the simulation until we complete
	if (bAdvanceSimulation)
	{
		while (CaptureComponent != nullptr)
		{
			CaptureComponent->AdvanceSimulation(1, AdvanceDeltaTime);
			OnFrameTick(AdvanceDeltaTime);
		}
	}
	// In this path we are going to monitor the simulation and capture each frame
	else
	{
		if (CaptureComponent != nullptr)
		{
			if (UWorld* OwnerWorld = CaptureComponent->GetWorld())
			{
				TickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UAsyncNiagaraCaptureSimCache::OnFrameTick), 0);
			}
		}

		if (CaptureSimCache == nullptr || CaptureComponent == nullptr || TickerHandle.IsValid() == false)
		{
			SetReadyToDestroy();
		}
	}
}

void UAsyncNiagaraCaptureSimCache::SetReadyToDestroy()
{
	Super::SetReadyToDestroy();

	if (TickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
	}

	if ( CaptureSimCache != nullptr )
	{
		CaptureSimCache->EndWrite();
	}
	CaptureComplete.Broadcast(CaptureSimCache ? CaptureSimCache->IsCacheValid() : false);

	CaptureComponent = nullptr;
}

bool UAsyncNiagaraCaptureSimCache::OnFrameTick(float DeltaTime)
{
	// Component invalid or not active?  If so complete the cache recording
	if ( CaptureComponent == nullptr || !CaptureComponent->IsActive() || CaptureSimCache == nullptr )
	{
		SetReadyToDestroy();
		return true;
	}

	// Should we record this frame?
	if ( (CaptureFrameCounter % CaptureFrameRate) == 0 )
	{
		// If we fail to capture the frame it might be because things became invalid
		// Or it might be because the simulation was not ticked since the last capture in which case don't advance the counter
		if ( CaptureSimCache->WriteFrame(CaptureComponent) == false )
		{
			if ( CaptureSimCache->IsCacheValid() == false )
			{
				SetReadyToDestroy();
			}

			// Make sure we don't keep this alive forever, if we didn't managed to capture anything in 10 ticks something has probably gone wrong so bail
			if (TimeOutCounter++ > 10)
			{
				UE_LOG(LogNiagara, Warning, TEXT("SimCache Write has failed too many times, abandoning capturing for (%s)"), *GetFullNameSafe(CaptureSimCache));
				SetReadyToDestroy();
			}
			return true;
		}
	}

	TimeOutCounter = 0;
	++CaptureFrameCounter;

	// Have we recorded all the frames we need?
	// Note: the -1 is because T0 was the initial frame
	if ( (CaptureNumFrames > 0) && (CaptureFrameCounter > (CaptureFrameRate * (CaptureNumFrames - 1))) )
	{
		SetReadyToDestroy();
		return true;
	}

	return true;
}

UAsyncNiagaraCaptureSimCache* UAsyncNiagaraCaptureSimCache::CaptureNiagaraSimCacheMultiFrame(UNiagaraSimCache* SimCache, FNiagaraSimCacheCreateParameters CreateParameters, UNiagaraComponent* NiagaraComponent, UNiagaraSimCache*& OutSimCache, int32 NumFrames, int32 CaptureRate, bool bAdvanceSimulation, float AdvanceDeltaTime)
{
	UAsyncNiagaraCaptureSimCache* CaptureAction = NewObject<UAsyncNiagaraCaptureSimCache>();
	CaptureAction->CaptureSimCache = SimCache;
	CaptureAction->CaptureComponent = NiagaraComponent;
	CaptureAction->CaptureNumFrames = FMath::Max(1, NumFrames);
	CaptureAction->CaptureFrameRate = FMath::Max(1, CaptureRate);
	CaptureAction->CaptureFrameCounter = 0;
	CaptureAction->bAdvanceSimulation = bAdvanceSimulation;
	CaptureAction->AdvanceDeltaTime = AdvanceDeltaTime;

	if (SimCache != nullptr)
	{
		SimCache->BeginWrite(CreateParameters, NiagaraComponent);
	}
	OutSimCache = SimCache;

	return CaptureAction;
}

UAsyncNiagaraCaptureSimCache* UAsyncNiagaraCaptureSimCache::CaptureNiagaraSimCacheUntilComplete(UNiagaraSimCache* SimCache, FNiagaraSimCacheCreateParameters CreateParameters, UNiagaraComponent* NiagaraComponent, UNiagaraSimCache*& OutSimCache, int32 CaptureRate, bool bAdvanceSimulation, float AdvanceDeltaTime)
{
	UAsyncNiagaraCaptureSimCache* CaptureAction = NewObject<UAsyncNiagaraCaptureSimCache>();
	CaptureAction->CaptureSimCache = SimCache;
	CaptureAction->CaptureComponent = NiagaraComponent;
	CaptureAction->CaptureNumFrames = 0;
	CaptureAction->CaptureFrameRate = FMath::Max(1, CaptureRate);
	CaptureAction->CaptureFrameCounter = 0;
	CaptureAction->bAdvanceSimulation = bAdvanceSimulation;
	CaptureAction->AdvanceDeltaTime = AdvanceDeltaTime;

	if (SimCache != nullptr)
	{
		SimCache->BeginWrite(CreateParameters, NiagaraComponent);
	}
	OutSimCache = SimCache;

	return CaptureAction;
}

UNiagaraSimCacheFunctionLibrary::UNiagaraSimCacheFunctionLibrary(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UNiagaraSimCacheFunctionLibrary::CaptureNiagaraSimCacheImmediate(UNiagaraSimCache* SimCache, FNiagaraSimCacheCreateParameters CreateParameters, UNiagaraComponent* NiagaraComponent, UNiagaraSimCache*& OutSimCache, bool bAdvanceSimulation, float AdvanceDeltaTime)
{
	bool bSuccess = false;
	if (SimCache != nullptr && NiagaraComponent != nullptr)
	{
		if ( bAdvanceSimulation )
		{
			NiagaraComponent->AdvanceSimulation(1, AdvanceDeltaTime);
		}

		SimCache->BeginWrite(CreateParameters, NiagaraComponent);
		SimCache->WriteFrame(NiagaraComponent);
		SimCache->EndWrite();
		bSuccess = SimCache->IsCacheValid();
	}
	OutSimCache = SimCache;
	return bSuccess;
}

UNiagaraSimCache* UNiagaraSimCacheFunctionLibrary::CreateNiagaraSimCache(UObject* WorldContextObject)
{
	if ( UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) )
	{
		return NewObject<UNiagaraSimCache>(WorldContextObject);
	}

	return nullptr;
}
