// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/ObjectPtr.h"
#include "NiagaraDataChannelPublic.h"

class FNiagaraWorldManager;
class UNiagaraDataChannel;
class UNiagaraDataChannelHandler;
enum ETickingGroup : int;

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

	UNiagaraDataChannelHandler* InitDataChannel(const UNiagaraDataChannel* Channel, bool bForce);
	void RemoveDataChannel(const UNiagaraDataChannel* Channel);

	void BeginFrame(float DeltaSeconds);
	void EndFrame(float DeltaSeconds);

	void Tick(float DeltaSeconds, ETickingGroup TickGroup);

	void RefreshDataChannels();
	
	/**
	Return the DataChannel handler for the given channel.
	*/
	UNiagaraDataChannelHandler* FindDataChannelHandler(const UNiagaraDataChannel* Channel);
	UNiagaraDataChannelHandler* FindDataChannelHandler(const UNiagaraDataChannelAsset* Channel) { return Channel ? FindDataChannelHandler(Channel->Get()) : nullptr; }

	UWorld* GetWorld()const;

private:
	FNiagaraWorldManager* WorldMan = nullptr;

	/** Runtime handlers for each DataChannel channel. */
	TMap<TWeakObjectPtr<const UNiagaraDataChannel>, TObjectPtr<UNiagaraDataChannelHandler>> Channels;
	bool bIsCleanedUp = false;
};
