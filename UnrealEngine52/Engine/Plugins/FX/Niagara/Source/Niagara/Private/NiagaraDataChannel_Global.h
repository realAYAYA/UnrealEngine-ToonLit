// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraDataChannel.h"
#include "NiagaraDataChannelHandler.h"
#include "NiagaraDataChannel_Global.generated.h"


UCLASS(Experimental)
class UNiagaraDataChannel_Global : public UNiagaraDataChannel
{
	GENERATED_BODY()

	virtual UNiagaraDataChannelHandler* CreateHandler()const override;
};

/**
Basic DataChannel handler that makes all data visible globally.
*/
UCLASS(Experimental, BlueprintType)
class UNiagaraDataChannelHandler_Global : public UNiagaraDataChannelHandler
{
	GENERATED_UCLASS_BODY()

	FNiagaraWorldDataChannelStore Data;

	//UObject Interface
	virtual void BeginDestroy()override;
	//UObject Interface End

	virtual void Init(const UNiagaraDataChannel* InChannel) override;
	virtual void Tick(float DeltaTime, ETickingGroup TickGroup, FNiagaraWorldManager* OwningWorld) override;
	virtual void GetData(FNiagaraSystemInstance* SystemInstance, FNiagaraDataBuffer*& OutCPUData, bool bGetLastFrameData) override;
	virtual void GetDataGPU(FNiagaraSystemInstance* SystemInstance, FNiagaraDataSet*& OutCPUData) override;

	/** Returns the game level DataChannel data store. */
	virtual FNiagaraDataChannelGameDataPtr GetGameData(UActorComponent* OwningComponent);

private:

	/** Staging buffer for Game level data so we can push it around as a publish request like other niagara data. Simplifying the code. */
	FNiagaraDataSet* GameDataStaging;

	/** Last Frame's CPU data. */
	FNiagaraDataBuffer* PrevFrameCPU;
};

/**
DataChannel handler that separates DataChannel out by the hash of their position into spatially localized grid cells.
Useful for DataChannel that should only be visible to nearby consumers.
TODO: Likely unsuitable for LWC games as we need a defined min/max for the hash and storage. A sparse octree etc would be needed
*/
// UCLASS(EditInlineNew)
// class UNiagaraDataChannelChannelHandler_WorldGrid : public UObject
// {
// };

// UCLASS(EditInlineNew, abstract)
// class UNiagaraDataChannelChannelHandler_Octree : UObject
// {
// 
// };