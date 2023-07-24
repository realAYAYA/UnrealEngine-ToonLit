// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraDataChannelHandler.h"
#include "NiagaraDataChannelAccessor.h"

UNiagaraDataChannelHandler::~UNiagaraDataChannelHandler()
{
	if (FNiagaraDataChannelHandlerRTProxyBase* ReleasedProxy = RTProxy.Release())
	{
		ENQUEUE_RENDER_COMMAND(FDeleteRTProxyCmd) (
			[RT_Proxy = ReleasedProxy](FRHICommandListImmediate& CmdList) mutable
			{
				delete RT_Proxy;
			}
		);
		check(RTProxy.IsValid() == false);
	}
}

void UNiagaraDataChannelHandler::Init(const UNiagaraDataChannel* InChannel)
{
	DataChannel = InChannel;
}

void UNiagaraDataChannelHandler::Tick(float DeltaTime, ETickingGroup TickGroup, FNiagaraWorldManager* OwningWorld)
{
}

void UNiagaraDataChannelHandler::Publish(const FNiagaraDataChannelPublishRequest& Request)
{
	PublishRequests.Add(Request);
}

void UNiagaraDataChannelHandler::RemovePublishRequests(const FNiagaraDataSet* DataSet)
{
	for (auto It = PublishRequests.CreateIterator(); It; ++It)
	{
		const FNiagaraDataChannelPublishRequest& PublishRequest = *It;

		//First remove any buffers we've queued up to push to the GPU
		for (auto BufferIt = BuffersForGPU.CreateIterator(); BufferIt; ++BufferIt)
		{
			FNiagaraDataBuffer* Buffer = *BufferIt;
			if (Buffer == nullptr || Buffer->GetOwner() == DataSet)
			{
				It.RemoveCurrentSwap();
			}
		}

		if (PublishRequest.Data && PublishRequest.Data->GetOwner() == DataSet)
		{
			It.RemoveCurrentSwap();
		}
	}
}


UNiagaraDataChannelWriter* UNiagaraDataChannelHandler::GetDataChannelWriter()
{
	if(Writer == nullptr)
	{
		Writer =  NewObject<UNiagaraDataChannelWriter>();
		Writer->Owner = this;
	}
	return Writer;
}

UNiagaraDataChannelReader* UNiagaraDataChannelHandler::GetDataChannelReader()
{
	if (Reader == nullptr)
	{
		Reader = NewObject<UNiagaraDataChannelReader>();
		Reader->Owner = this;
	}
	return Reader;
}