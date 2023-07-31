// Copyright Epic Games, Inc. All Rights Reserved.

#include "SlateUpdatableBuffer.h"
#include "RenderingThread.h"

DECLARE_CYCLE_STAT(TEXT("UpdateInstanceBuffer Time (RT)"), STAT_SlateUpdateInstanceBuffer_RT, STATGROUP_Slate);
DECLARE_CYCLE_STAT(TEXT("UpdateInstanceBuffer Time (RHIT)"), STAT_SlateUpdateInstanceBuffer_RHIT, STATGROUP_Slate);

FSlateUpdatableInstanceBuffer::FSlateUpdatableInstanceBuffer(int32 InitialInstanceCount)
{
	Proxy = new FRenderProxy;
	Proxy->InstanceBufferResource.Init(InitialInstanceCount);
}

FSlateUpdatableInstanceBuffer::~FSlateUpdatableInstanceBuffer()
{
	ENQUEUE_RENDER_COMMAND(SlateUpdatableInstanceBuffer_DeleteProxy)(
		[Ptr = Proxy](FRHICommandListImmediate&)
	{
		delete Ptr;
	});

	Proxy = nullptr;
}

void FSlateUpdatableInstanceBuffer::Update(FSlateInstanceBufferData& Data)
{
	check(IsThreadSafeForSlateRendering());

	NumInstances = Data.Num();
	if (NumInstances > 0)
	{
		// Enqueue a render thread command to update the proxy with the new data.
		ENQUEUE_RENDER_COMMAND(SlateUpdatableInstanceBuffer_Update)(
			[Ptr = Proxy, LocalDataRT = MoveTemp(Data)](FRHICommandListImmediate& RHICmdList) mutable
		{
			Ptr->Update(RHICmdList, LocalDataRT);
		});
	}
}

void FSlateUpdatableInstanceBuffer::FRenderProxy::Update(FRHICommandListImmediate& RHICmdList, FSlateInstanceBufferData& Data)
{
	SCOPE_CYCLE_COUNTER(STAT_SlateUpdateInstanceBuffer_RT);

	InstanceBufferResource.PreFillBuffer(Data.Num(), false);

	// Enqueue the lock/unlock to the RHI thread
	FRHIBuffer* VertexBuffer = InstanceBufferResource.VertexBufferRHI;
	RHICmdList.EnqueueLambda([VertexBuffer, LocalData = MoveTemp(Data)](FRHICommandListImmediate& InRHICmdList)
	{
		SCOPE_CYCLE_COUNTER(STAT_SlateUpdateInstanceBuffer_RHIT);

		int32 RequiredVertexBufferSize = LocalData.Num() * LocalData.GetTypeSize();
		uint8* InstanceBufferData = (uint8*)InRHICmdList.LockBuffer(VertexBuffer, 0, RequiredVertexBufferSize, RLM_WriteOnly);

		FMemory::Memcpy(InstanceBufferData, LocalData.GetData(), RequiredVertexBufferSize);
	
		InRHICmdList.UnlockBuffer(VertexBuffer);
	});

	RHICmdList.RHIThreadFence(true);
}

void FSlateUpdatableInstanceBuffer::FRenderProxy::BindStreamSource(FRHICommandList& RHICmdList, int32 StreamIndex, uint32 InstanceOffset)
{
	RHICmdList.SetStreamSource(StreamIndex, InstanceBufferResource.VertexBufferRHI, InstanceOffset * sizeof(FVector4f));
}
