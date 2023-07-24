// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraDataChannel_Global.h"

#include "NiagaraSystemInstance.h"
#include "NiagaraDataChannelManager.h"
#include "DataInterface/NiagaraDataInterfaceDataChannelWrite.h"
#include "NiagaraGpuComputeDispatchInterface.h"
#include "NiagaraWorldManager.h"


int32 GbDebugDumpDumpHandlerTick = 0;
static FAutoConsoleVariableRef CVarDumpHandlerTick(
	TEXT("fx.Niagara.DataChannels.DumpHandlerTick"),
	GbDebugDumpDumpHandlerTick,
	TEXT(" \n"),
	ECVF_Default
);

DECLARE_CYCLE_STAT(TEXT("UNiagaraDataChannelHandler_Global::Tick"), STAT_DataChannelHandler_Global_Tick, STATGROUP_NiagaraDataChannels);

//////////////////////////////////////////////////////////////////////////

UNiagaraDataChannelHandler* UNiagaraDataChannel_Global::CreateHandler()const
{
	UNiagaraDataChannelHandler* NewHandler = NewObject<UNiagaraDataChannelHandler_Global>();
	NewHandler->Init(this);
	return NewHandler;
}

//TODO: Other channel types.
// Octree etc.


//////////////////////////////////////////////////////////////////////////

class FNiagaraDataChannelHandlerRTProxy_Global : public FNiagaraDataChannelHandlerRTProxyBase
{	
public:
	FNiagaraDataSet* GPUDataSet = nullptr;
	FString DebugName;

	void BeginFrame(FNiagaraGpuComputeDispatchInterface* DispathInterface, FRHICommandListImmediate& CmdList)
	{
	}

	void EndFrame(FNiagaraGpuComputeDispatchInterface* DispathInterface, FRHICommandListImmediate& CmdList, const TArray<FNiagaraDataBuffer*>& BuffersForGPU)
	{
		uint32 NumInstance = 0;
		for(FNiagaraDataBuffer* Buffer : BuffersForGPU)
		{
			NumInstance += Buffer->GetNumInstances();
		}		

		GPUDataSet->BeginSimulate();
		FNiagaraDataBuffer* DestBuffer = GPUDataSet->GetDestinationData();
		DestBuffer->PushCPUBuffersToGPU(BuffersForGPU, true, CmdList, DispathInterface->GetFeatureLevel(), *DebugName);
		GPUDataSet->EndSimulate();
		
		//For now we need not deal with the instance count manager but when we do GPU->GPU writes we will
		//DestBuffer->SetGPUInstanceCountBufferOffset()
	}
};

UNiagaraDataChannelHandler_Global::UNiagaraDataChannelHandler_Global(FObjectInitializer const& ObjectInitializer)
	: Super(ObjectInitializer)
{
	RTProxy.Reset(new FNiagaraDataChannelHandlerRTProxy_Global());
}

void UNiagaraDataChannelHandler_Global::BeginDestroy()
{
	Super::BeginDestroy();
	Data.Empty();
	
	ENQUEUE_RENDER_COMMAND(FDeleteContextCommand)(
		[RT_GameDataStaging = GameDataStaging](FRHICommandListImmediate& RHICmdList)
		{
			if (RT_GameDataStaging != nullptr)
			{
				delete RT_GameDataStaging;
			}
		}
	);
	GameDataStaging = nullptr;
}

void UNiagaraDataChannelHandler_Global::Init(const UNiagaraDataChannel* InChannel)
{
	Super::Init(InChannel);
	Data.Init(this);

	const FNiagaraDataSetCompiledData& CompiledData = GetDataChannel()->GetCompiledData(ENiagaraSimTarget::CPUSim);
	GameDataStaging = new FNiagaraDataSet();
	GameDataStaging->Init(&CompiledData);

	FString GPUDebugName;
#if !UE_BUILD_SHIPPING
	GPUDebugName = FString::Printf(TEXT("%s__GPUData"), *InChannel->GetChannelName().ToString());
#endif

	ENQUEUE_RENDER_COMMAND(FInitProxy) (
		[RT_Proxy = GetRTProxyAs<FNiagaraDataChannelHandlerRTProxy_Global>(), GPUDataSet = Data.GPUSimData, DebugName=MoveTemp(GPUDebugName)](FRHICommandListImmediate& CmdList)mutable
	{
		RT_Proxy->GPUDataSet = GPUDataSet;
		RT_Proxy->DebugName = MoveTemp(DebugName);
	}
	);
}

void UNiagaraDataChannelHandler_Global::Tick(float DeltaSeconds, ETickingGroup TickGroup, FNiagaraWorldManager* OwningWorld)
{
	SCOPE_CYCLE_COUNTER(STAT_DataChannelHandler_Global_Tick);
	Super::Tick(DeltaSeconds, TickGroup, OwningWorld);

	if (Data.IsInitialized() == false)
	{
		Data.Init(this);
	}

	if(GameDataStaging == nullptr)
	{
		const FNiagaraDataSetCompiledData CompiledData = GetDataChannel()->GetCompiledData(ENiagaraSimTarget::CPUSim);
		GameDataStaging = new FNiagaraDataSet();
		GameDataStaging->Init(&CompiledData);
	}

	bool bIsFirstTick = TickGroup == NiagaraFirstTickGroup;
	bool bIsLastTick = TickGroup == NiagaraLastTickGroup;

	if (bIsFirstTick)
	{
		//TODO: Add reference struct to handle these ref counts and make add/release private.
		if(DataChannel->KeepPreviousFrameData())
		{
			if(Data.PrevCPUSimData)
			{
				Data.PrevCPUSimData->ReleaseReadRef();
			}
			Data.PrevCPUSimData = Data.CPUSimData->GetCurrentData();
			Data.PrevCPUSimData->AddReadRef();
		}

		FNiagaraGpuComputeDispatchInterface* DispathInterface = FNiagaraGpuComputeDispatchInterface::Get(OwningWorld->GetWorld());
		ENQUEUE_RENDER_COMMAND(FDataChannelProxyBeginFrame) (
			[DispathInterface, RT_Proxy = GetRTProxyAs<FNiagaraDataChannelHandlerRTProxy_Global>()](FRHICommandListImmediate& CmdList)
		{
			if(RT_Proxy)
			{
				RT_Proxy->BeginFrame(DispathInterface, CmdList);
			}
		});
	}

	//Gather various sources of DataChannel and ship them around as heeded.

	int32 GameDataOrigSize = bIsFirstTick ? 0 : Data.GameData->Num();
	int32 CPUDataOrigSize = bIsFirstTick ? 0 : Data.CPUSimData->GetCurrentData()->GetNumInstances();
	int32 NewGameDataChannel = GameDataOrigSize;
	int32 NewCPUDataChannel = CPUDataOrigSize;

	//Do a pass to gather the new total size for our DataChannel data.

	//Each DI that generates DataChannel can control whether it's pushed to Game/CPU/GPU.
	BuffersForGPU.Reserve(BuffersForGPU.Num() + PublishRequests.Num());
	for (FNiagaraDataChannelPublishRequest& PublishRequest : PublishRequests)
	{
		uint32 NumInsts = 0;

		if(FNiagaraDataChannelGameData* RequestGameData = PublishRequest.GameData.Get())
		{
			NumInsts = RequestGameData->Num();
			if (RequestGameData->Num() > 0 && (PublishRequest.bVisibleToCPUSims || PublishRequest.bVisibleToGPUSims))
			{
				//For now we copy game data into a staging buffer for pushing to CPU & GPU buffers.
				//We can avoid this in future but will add complexity to the transfer code.
				GameDataStaging->BeginSimulate();
				GameDataStaging->Allocate(RequestGameData->Num());
				RequestGameData->WriteToDataSet(GameDataStaging->GetDestinationData(), 0);
				GameDataStaging->EndSimulate();

				if (GbDebugDumpDumpHandlerTick)
				{
					FNiagaraDataBuffer& Buffer = GameDataStaging->GetCurrentDataChecked();
					FString DebugString = FString::Printf(TEXT("%s Game Data Staging"), *DataChannel->GetChannelName().ToString());
					Buffer.Dump(0, Buffer.GetNumInstances(), FString::Printf(TEXT("=== Data Channle Staging Game Data: %d Elements --> %s ==="), Buffer.GetNumInstances(), *DebugString));
				}

				PublishRequest.Data = GameDataStaging->GetCurrentData();
				PublishRequest.Data->AddReadRef();
			}
		}
		else if( ensure(PublishRequest.Data) )
		{
			NumInsts = PublishRequest.Data->GetNumInstances();
		}
	
		if (PublishRequest.bVisibleToGame)
		{
			NewGameDataChannel += NumInsts;
		}
		if (PublishRequest.bVisibleToCPUSims)
		{
			NewCPUDataChannel += NumInsts;
		}
		if(PublishRequest.bVisibleToGPUSims)
		{
			PublishRequest.Data->AddReadRef();
			BuffersForGPU.Add(PublishRequest.Data);
		}
	}

	//Allocate Sim buffers ready for gather.
	Data.CPUSimData->BeginSimulate();
	Data.CPUSimData->Allocate(NewCPUDataChannel, bIsFirstTick == false);

	//Now do the actual data collection.
	
	//Allocate game buffers ready for gather.
	if(bIsFirstTick)
	{
		Data.GameData->Reset();
	}
	
	Data.GameData->SetNum(NewGameDataChannel);

	for (FNiagaraDataChannelPublishRequest& PublishRequest : PublishRequests)
	{
		if (FNiagaraDataChannelGameData* RequestGameData = PublishRequest.GameData.Get())
		{
			if(RequestGameData->Num() && PublishRequest.bVisibleToGame)
			{
				Data.GameData->AppendFromGameData(*RequestGameData);
			}
		}

		if (PublishRequest.Data)
		{
			const ENiagaraSimTarget SimTarget = PublishRequest.Data->GetOwner()->GetSimTarget();
			if (SimTarget == ENiagaraSimTarget::CPUSim)
			{
				if (PublishRequest.bVisibleToGame)
				{
					Data.GameData->AppendFromDataSet(PublishRequest.Data, PublishRequest.LwcTile);
				}
				if (PublishRequest.bVisibleToCPUSims)
				{
					PublishRequest.Data->CopyToUnrelated(Data.CPUSimData->GetDestinationDataChecked(), 0, Data.CPUSimData->GetDestinationDataChecked().GetNumInstances(), PublishRequest.Data->GetNumInstances());
				}
			}
			else if (SimTarget == ENiagaraSimTarget::GPUComputeSim)
			{
				//TODO: GPU->GPU handling will be done all RT side. GPU->CPU may be done here in future but we'll have to pull the data from the GPU on the RT and pass beck to GT here.
			}
			else
			{
				check(0);
			}
		}
	}

	Data.CPUSimData->EndSimulate();

	if (GbDebugDumpDumpHandlerTick)
	{
		FNiagaraDataBuffer& Buffer = Data.CPUSimData->GetCurrentDataChecked();
		FString DebugString = FString::Printf(TEXT("%s CPU Sim Data"), *DataChannel->GetChannelName().ToString());
		Buffer.Dump(0, Buffer.GetNumInstances(), FString::Printf(TEXT("=== Updated CPU Sim Data: %d Elements --> %s ==="), CPUDataOrigSize, *DebugString));
	}

	PublishRequests.Reset();

	//Now we push the CPU sim data to the RT for upload to the GPU.
	//We may need to do a final tick from PostActorTick in the world manager?
	if(bIsLastTick)
	{
		FNiagaraDataChannelHandlerRTProxy_Global* Proxy = GetRTProxyAs<FNiagaraDataChannelHandlerRTProxy_Global>();

		check(Proxy);
		check(Proxy->GPUDataSet);
		check(Proxy->GPUDataSet->GetSimTarget() == ENiagaraSimTarget::GPUComputeSim);
		FNiagaraGpuComputeDispatchInterface* DispathInterface = FNiagaraGpuComputeDispatchInterface::Get(OwningWorld->GetWorld());
		ENQUEUE_RENDER_COMMAND(FDataChannelProxyEndFrame) (
			[DispathInterface, RT_Proxy = Proxy, RT_BuffersForGPU=MoveTemp(BuffersForGPU)](FRHICommandListImmediate& CmdList) 
			{
				RT_Proxy->EndFrame(DispathInterface, CmdList, RT_BuffersForGPU);				
			});
		BuffersForGPU.Reset();
	}
}

void UNiagaraDataChannelHandler_Global::GetData(FNiagaraSystemInstance* SystemInstance, FNiagaraDataBuffer*& OutCPUData, bool bGetPrevFrameData)
{
	if(bGetPrevFrameData && Data.PrevCPUSimData)
	{
		OutCPUData = Data.PrevCPUSimData;
	}
	else
	{
		OutCPUData = Data.CPUSimData->GetCurrentData();	
	}

	if(OutCPUData)
	{
		OutCPUData->AddReadRef();
	}
	//For more complicated channels we could check the location + bounds of the system instance etc to return some spatially localized data.
}

void UNiagaraDataChannelHandler_Global::GetDataGPU(FNiagaraSystemInstance* SystemInstance, FNiagaraDataSet*& OutCPUData)
{
	OutCPUData = Data.GPUSimData;	
	//For more complicated channels we could check the location + bounds of the system instance etc to return some spatially localized data.
}


FNiagaraDataChannelGameDataPtr UNiagaraDataChannelHandler_Global::GetGameData(UActorComponent* OwningComponent)
{
	return Data.GameData;
}
