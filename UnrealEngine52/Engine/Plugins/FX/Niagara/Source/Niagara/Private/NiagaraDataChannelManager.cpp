// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraDataChannelManager.h"

#include "NiagaraWorldManager.h"
#include "NiagaraDataChannel.h"
#include "NiagaraDataChannelHandler.h"
#include "NiagaraDataChannelDefinitions.h"


FNiagaraDataChannelManager::FNiagaraDataChannelManager(FNiagaraWorldManager* InWorldMan)
	: WorldMan(InWorldMan)
{
}

void FNiagaraDataChannelManager::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObjects(Channels);
}

void FNiagaraDataChannelManager::Init()
{
	for (const UNiagaraDataChannelDefinitions* DataChannelDefs : UNiagaraDataChannelDefinitions::GetDataChannelDefinitions(false, false))
	{
		for (const UNiagaraDataChannel* DataChannel : DataChannelDefs->DataChannels)
		{
			InitDataChannel(DataChannel, true);
		}
	}
}

void FNiagaraDataChannelManager::Cleanup()
{
	Channels.Empty();
}

void FNiagaraDataChannelManager::Tick(float DeltaSeconds, ETickingGroup TickGroup)
{
	if(INiagaraModule::DataChannelsEnabled())
	{
		//Tick all DataChannel channel handlers.
		for (auto& ChannelPair : Channels)
		{
			ChannelPair.Value->Tick(DeltaSeconds, TickGroup, WorldMan);
		}
	}
	else
	{
		Channels.Empty();
	}
}

UNiagaraDataChannelHandler* FNiagaraDataChannelManager::FindDataChannelHandler(FName ChannelName)
{
	if (TObjectPtr<UNiagaraDataChannelHandler>* Found = Channels.Find(ChannelName))
	{
		return (*Found).Get();
	}
	return nullptr;
}

void FNiagaraDataChannelManager::InitDataChannel(const UNiagaraDataChannel* InChannel, bool bForce)
{
	if(INiagaraModule::DataChannelsEnabled() && InChannel->IsValid())
	{
		FName Channel = InChannel->GetChannelName();
		TObjectPtr<UNiagaraDataChannelHandler>& Handler = Channels.FindOrAdd(Channel);

		if (bForce || Handler == nullptr)
		{
			Handler = InChannel->CreateHandler();
		}
	}
}

void FNiagaraDataChannelManager::RemoveDataChannel(FName ChannelName)
{
	Channels.Remove(ChannelName);
}

UWorld* FNiagaraDataChannelManager::GetWorld()const
{
	return WorldMan->GetWorld();
}
