// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraDataChannelCommon.h"


/**
Manager class for all Niagara DataChannels at the System/World level.
*/
class FNiagaraDataChannelManager
{
public:

	explicit FNiagaraDataChannelManager(FNiagaraWorldManager* InWorldMan);

	void AddReferencedObjects(FReferenceCollector& Collector);

	void Init();
	void Cleanup();

	void InitDataChannel(const UNiagaraDataChannel* Channel, bool bForce);
	void RemoveDataChannel(FName ChannelName);

	void Tick(float DeltaSeconds, ETickingGroup TickGroup);
	
	/**
	Return the DataChannel handler for the given channel.
	*/
	UNiagaraDataChannelHandler* FindDataChannelHandler(FName ChannelName);

	UWorld* GetWorld()const;

private:
	FNiagaraWorldManager* WorldMan = nullptr;

	/** Runtime handlers for each DataChannel channel. */
	TMap<FName, TObjectPtr<UNiagaraDataChannelHandler>> Channels;
};