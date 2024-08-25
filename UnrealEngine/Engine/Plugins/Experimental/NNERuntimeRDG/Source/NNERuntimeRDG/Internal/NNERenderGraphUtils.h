// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NNERuntimeCPU.h"
#include "NNERuntimeRDG.h"
#include "RenderGraph.h"
#include "RHIGPUReadback.h"

#if UE_TRACE_ENABLED
	#define NNE_TRACE_EVENT_SCOPED(Name) SCOPED_NAMED_EVENT_TEXT(#Name, FColor::Green)
#else
	#define NNE_TRACE_EVENT_SCOPED(Name)
#endif

namespace UE::NNE::Internal
{

BEGIN_SHADER_PARAMETER_STRUCT(FReadbackPassParameters, )
	RDG_BUFFER_ACCESS_ARRAY(Buffers)
END_SHADER_PARAMETER_STRUCT()

/*
* Utility class to manage GPU -> CPU readbacks
* The implementation follows single producer single consumer, 
* every call to EnqueueReadbacks() on render thread has corresponding Wait() on game thread.
* Don't enqueue multiple EnqueueReadbacks() while waiting on game thread, make sure that 
* all readbacks are processed (by calling Wait()), before calling EnqueueReadbacks() again.
*/
class FReadbackManager
{
	struct FReadback
	{
		FRHIGPUBufferReadback	BufferReadback;
		void*					DstData;
		uint32					NumBytes;
	};

	using ReadbackArray = TArray<FReadback, TInlineAllocator<16>>;

	ReadbackArray	Readbacks;
	FEvent*			Signal = nullptr;

public:

	FReadbackManager()
	{
		check(IsInGameThread());
		Signal = FGenericPlatformProcess::GetSynchEventFromPool(true);
	}

	~FReadbackManager()
	{
		check(IsInGameThread());
		FGenericPlatformProcess::ReturnSynchEventToPool(Signal);
		Signal = nullptr;
	}

	bool EnqueueReadbacks(FRDGBuilder& RDGBuilder, TConstArrayView<FTensorBindingRDG> InBindingsRDG, TConstArrayView<FTensorBindingCPU> InBindingsCPU)
	{
		if (!Readbacks.IsEmpty())
		{
			UE_LOG(LogNNE, Error, TEXT("FReadbackManager:Unprocessed readbacks detected"));
			return false;
		}

		Signal->Reset();

		NNE_TRACE_EVENT_SCOPED(NNE_FTensorReadback_AddReadbacks_RT);

		check(IsInRenderingThread());
		check(InBindingsRDG.Num() == InBindingsCPU.Num());

		if (InBindingsRDG.Num() != InBindingsCPU.Num())
		{
			UE_LOG(LogNNE, Error, TEXT("FReadbackManager:Number of bindings needs to be same"));
			return false;
		}

		FReadbackPassParameters* ReadbackParams = RDGBuilder.AllocParameters<FReadbackPassParameters>();

		for (int32 Idx = 0; Idx < InBindingsRDG.Num(); ++Idx)
		{
			ReadbackParams->Buffers.Emplace(InBindingsRDG[Idx].Buffer, ERHIAccess::CopySrc);
		}

		RDGBuilder.AddPass(
			RDG_EVENT_NAME("FReadbackManager_EnqueueReadbacks"),
			ReadbackParams,
			ERDGPassFlags::Readback | ERDGPassFlags::NeverCull,
			[this, ReadbackParams, InBindingsCPU](FRHICommandListImmediate& RHICmdList)
			{
				for (int32 Idx = 0; Idx < ReadbackParams->Buffers.Num(); ++Idx)
				{
					FReadback& Readback = Readbacks.Add_GetRef( 
						{
							FRHIGPUBufferReadback(*FString::Printf(TEXT("FReadbackManager_Readback_%d"), Idx)),
							InBindingsCPU[Idx].Data, 
							static_cast<uint32>(InBindingsCPU[Idx].SizeInBytes) 
						}
					);
				
					Readback.BufferReadback.EnqueueCopy(RHICmdList, ReadbackParams->Buffers[Idx]->GetRHI(), Readback.NumBytes);
				}
			}
		);

		return true;
	}

	// Note: call on the game thread
	bool Wait()
	{
		check(IsInGameThread());

		NNE_TRACE_EVENT_SCOPED(NNE_FTensorReadback_Wait);

		ENQUEUE_RENDER_COMMAND(NNE_FReadbackManager_Wait)
		(
			[this](FRHICommandListImmediate& RHICmdList)
			{
				NNE_TRACE_EVENT_SCOPED(NNE_FTensorReadback_Wait_RT);

				RHICmdList.BlockUntilGPUIdle();

				for (FReadback& Readback : Readbacks)
				{
					const void* SrcData = Readback.BufferReadback.Lock(Readback.NumBytes);
					check(SrcData);

					if (SrcData)
					{
						FMemory::Memcpy(Readback.DstData, SrcData, Readback.NumBytes);
						Readback.BufferReadback.Unlock();
					}
				}

				Readbacks.Reset();
				Signal->Trigger();

				// Clean-up resources (staging buffers, fences)
				RHICmdList.ImmediateFlush(EImmediateFlushType::FlushRHIThreadFlushResources);
			}
		);

		Signal->Wait();
		return Readbacks.IsEmpty();
	}
};

} // namespace UE::NNE
