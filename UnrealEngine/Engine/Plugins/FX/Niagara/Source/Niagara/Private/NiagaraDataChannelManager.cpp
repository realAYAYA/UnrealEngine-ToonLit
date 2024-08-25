// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraDataChannelManager.h"

#include "NiagaraModule.h"
#include "NiagaraWorldManager.h"
#include "NiagaraDataChannel.h"
#include "NiagaraDataChannelHandler.h"

DECLARE_CYCLE_STAT(TEXT("FNiagaraDataChannelManager::BeginFrame"), STAT_DataChannelManager_BeginFrame, STATGROUP_NiagaraDataChannels);
DECLARE_CYCLE_STAT(TEXT("FNiagaraDataChannelManager::EndFrame"), STAT_DataChannelManager_EndFrame, STATGROUP_NiagaraDataChannels);
DECLARE_CYCLE_STAT(TEXT("FNiagaraDataChannelManager::Tick"), STAT_DataChannelManager_Tick, STATGROUP_NiagaraDataChannels);


static int GNDCAllowLazyHandlerInit = 1;
static FAutoConsoleVariableRef CVarNDCAllowLazyHandlerInit(
	TEXT("fx.Niagara.DataChannels.AllowLazyHandlerInit"),
	GNDCAllowLazyHandlerInit,
	TEXT("True if we allow lazy initialization of NDC handlers."),
	ECVF_Default
);

FNiagaraDataChannelManager::FNiagaraDataChannelManager(FNiagaraWorldManager* InWorldMan)
	: WorldMan(InWorldMan)
{
}

void FNiagaraDataChannelManager::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObjects(Channels);
}

void FNiagaraDataChannelManager::RefreshDataChannels()
{
	if (bIsCleanedUp)
	{
		return;
	}

	UNiagaraDataChannel::ForEachDataChannel([&](UNiagaraDataChannel* DataChannel)
	{
		InitDataChannel(DataChannel, false);
	});
}

void FNiagaraDataChannelManager::Init()
{
	bIsCleanedUp = false;

	//Initialize any existing data channels, more may be initialized later as they are loaded.
	UNiagaraDataChannel::ForEachDataChannel([&](UNiagaraDataChannel* DataChannel)
	{
		InitDataChannel(DataChannel, true);
	});
}

void FNiagaraDataChannelManager::Cleanup()
{
	Channels.Empty();
	bIsCleanedUp = true;
}

void FNiagaraDataChannelManager::BeginFrame(float DeltaSeconds)
{
	if (INiagaraModule::DataChannelsEnabled())
	{
		check(IsInGameThread());
		SCOPE_CYCLE_COUNTER(STAT_DataChannelManager_BeginFrame);
		//Tick all DataChannel channel handlers.
		for (auto It = Channels.CreateIterator(); It; ++It)
		{
			if(const UNiagaraDataChannel* Channel = It.Key().Get())
			{
				It.Value()->BeginFrame(DeltaSeconds, WorldMan);
			}
			else
			{
				It.RemoveCurrent();
			}
		}
	}
}

void FNiagaraDataChannelManager::EndFrame(float DeltaSeconds)
{
	if (INiagaraModule::DataChannelsEnabled())
	{
		check(IsInGameThread());
		SCOPE_CYCLE_COUNTER(STAT_DataChannelManager_EndFrame);
		//Tick all DataChannel channel handlers.
		for (auto& ChannelPair : Channels)
		{
			ChannelPair.Value->EndFrame(DeltaSeconds, WorldMan);
		}
	}
}

void FNiagaraDataChannelManager::Tick(float DeltaSeconds, ETickingGroup TickGroup)
{
	if(INiagaraModule::DataChannelsEnabled())
	{
		check(IsInGameThread());
		SCOPE_CYCLE_COUNTER(STAT_DataChannelManager_Tick);
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

UNiagaraDataChannelHandler* FNiagaraDataChannelManager::FindDataChannelHandler(const UNiagaraDataChannel* Channel)
{
	check(IsInGameThread());
	if(Channel == nullptr)
	{
		return nullptr;
	}

	if(GNDCAllowLazyHandlerInit != 0)
	{
		//Try to lazy init the channel.
		//It's possible we've just loaded the channel and it's pending init.
		return InitDataChannel(Channel, false);
	}
	else
	{
		if (TObjectPtr<UNiagaraDataChannelHandler>* Found = Channels.Find(Channel))
		{
			return (*Found).Get();
		}
		return nullptr;
	}
}

UNiagaraDataChannelHandler* FNiagaraDataChannelManager::InitDataChannel(const UNiagaraDataChannel* InChannel, bool bForce)
{
	check(IsInGameThread());
	UWorld* World = GetWorld();
	if(INiagaraModule::DataChannelsEnabled() && World && !World->IsNetMode(NM_DedicatedServer) && InChannel->IsValid())
	{
		TObjectPtr<UNiagaraDataChannelHandler>& Handler = Channels.FindOrAdd(InChannel);

		if (bForce || Handler == nullptr)
		{
			Handler = InChannel->CreateHandler(WorldMan->GetWorld());
		}
		return Handler;
	}
	return nullptr;
}

void FNiagaraDataChannelManager::RemoveDataChannel(const UNiagaraDataChannel* InChannel)
{
	check(IsInGameThread());
	Channels.Remove(InChannel);
}

UWorld* FNiagaraDataChannelManager::GetWorld()const
{
	return WorldMan->GetWorld();
}
