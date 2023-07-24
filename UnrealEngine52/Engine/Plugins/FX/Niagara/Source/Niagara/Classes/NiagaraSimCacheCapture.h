// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraSimCache.h"
#include "Containers/Ticker.h"
#include "NiagaraSimCacheCapture.generated.h"

class UNiagaraComponent;
class UNiagaraSimCache;
struct FNiagaraSimCacheCreateParameters;

USTRUCT(BlueprintType)
struct FNiagaraSimCacheCaptureParameters
{
	GENERATED_BODY()

	/**
	 The max number of frames to capture. The capture will stop when the simulation completes or we have rendered this many frames, whichever happens first.
	 Set to 0 to capture until simulation completes.
	 **/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimCache")
	int32 NumFrames = 0;

	/**
	 Allows for reducing the frequency of captured frames in relation to the simulation's frames. The ratio is 1 / CaptureRate, so a CaptureRate of 2 would captures frames 0, 2, 4, etc.
	 */	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimCache")
	int32 CaptureRate = 1;

	/**
	 Controls manually advancing the simulation in a loop vs reading from it every frame. If bAdvanceSimulation is true, this capture will manually advance the simulation
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimCache")
	bool bManuallyAdvanceSimulation = false;

	/*
	 The delta time in seconds to use when manually advancing the simulation. Defaults to 1/60th of a second (0.01666).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SimCache", meta = (EditCondition = "bManuallyAdvanceSimulation"))
	float AdvanceDeltaTime = 0.01666f;
};

class NIAGARA_API FNiagaraSimCacheCapture
{
public:
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnCaptureComplete, UNiagaraSimCache*);

	FOnCaptureComplete& OnCaptureComplete() {return CaptureComplete;}
	
	
	// Captures a niagara sim cache. The caller is responsible for managing the lifetime of the provided component and sim cache. 
	void CaptureNiagaraSimCache(UNiagaraSimCache* SimCache, FNiagaraSimCacheCreateParameters CreateParameters, UNiagaraComponent* NiagaraComponent, FNiagaraSimCacheCaptureParameters CaptureParameters);

	/**
	Captures the simulations current frame data into the SimCache.
	This happens immediately so you may need to be careful with tick order of the component you are capturing from.
	The return can be invalid if the component can not be captured for some reason (i.e. not active).
	When AdvanceSimulation is true we will manually advance the simulation one frame using the provided AdvanceDeltaTime before capturing.
	*/
	bool CaptureCurrentFrameImmediate(UNiagaraSimCache* SimCache, FNiagaraSimCacheCreateParameters CreateParameters, UNiagaraComponent* NiagaraComponent, UNiagaraSimCache*& OutSimCache, bool bAdvanceSimulation=false, float AdvanceDeltaTime=0.01666f);

private:

	TWeakObjectPtr<UNiagaraSimCache> WeakCaptureSimCache;

	TWeakObjectPtr<UNiagaraComponent> WeakCaptureComponent;
	
	FOnCaptureComplete CaptureComplete;

	FTSTicker::FDelegateHandle TickerHandle;

	FNiagaraSimCacheCaptureParameters CaptureParameters;

	int32 CaptureFrameCounter = 0;

	int32 TimeOutCounter = 0;
	
	bool OnFrameTick(float DeltaTime);

	void FinishCapture();
};
