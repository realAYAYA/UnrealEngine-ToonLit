// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraDataChannel_Global.h"
#include "NiagaraWorldManager.h"
#include "NiagaraDataChannelCommon.h"


int32 GbDebugDumpDumpHandlerTick = 0;
static FAutoConsoleVariableRef CVarDumpHandlerTick(
	TEXT("fx.Niagara.DataChannels.DumpHandlerTick"),
	GbDebugDumpDumpHandlerTick,
	TEXT(" \n"),
	ECVF_Default
);

DECLARE_CYCLE_STAT(TEXT("UNiagaraDataChannelHandler_Global::Tick"), STAT_DataChannelHandler_Global_Tick, STATGROUP_NiagaraDataChannels);

//////////////////////////////////////////////////////////////////////////

UNiagaraDataChannelHandler* UNiagaraDataChannel_Global::CreateHandler(UWorld* OwningWorld)const
{
	UNiagaraDataChannelHandler* NewHandler = NewObject<UNiagaraDataChannelHandler_Global>(OwningWorld);
	NewHandler->Init(this);
	return NewHandler;
}

//TODO: Other channel types.
// Octree etc.


//////////////////////////////////////////////////////////////////////////


UNiagaraDataChannelHandler_Global::UNiagaraDataChannelHandler_Global(FObjectInitializer const& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UNiagaraDataChannelHandler_Global::BeginDestroy()
{
	Super::BeginDestroy();
	Data.Reset();
}

void UNiagaraDataChannelHandler_Global::Init(const UNiagaraDataChannel* InChannel)
{
	check(InChannel);
	Super::Init(InChannel);
	Data = CreateData();
}

void UNiagaraDataChannelHandler_Global::BeginFrame(float DeltaTime, FNiagaraWorldManager* OwningWorld)
{
	Super::BeginFrame(DeltaTime, OwningWorld);
	Data->BeginFrame(this);
}

void UNiagaraDataChannelHandler_Global::EndFrame(float DeltaTime, FNiagaraWorldManager* OwningWorld)
{
	Super::EndFrame(DeltaTime, OwningWorld);
	Data->EndFrame(this);
}

void UNiagaraDataChannelHandler_Global::Tick(float DeltaSeconds, ETickingGroup TickGroup, FNiagaraWorldManager* OwningWorld)
{
	SCOPE_CYCLE_COUNTER(STAT_DataChannelHandler_Global_Tick);
	Super::Tick(DeltaSeconds, TickGroup, OwningWorld);

	Data->ConsumePublishRequests(this, TickGroup);
}

FNiagaraDataChannelDataPtr UNiagaraDataChannelHandler_Global::FindData(FNiagaraDataChannelSearchParameters SearchParams, ENiagaraResourceAccess AccessType)
{
	return Data;
	//For more complicated channels we could check the location + bounds of the system instance etc to return some spatially localized data.
}
