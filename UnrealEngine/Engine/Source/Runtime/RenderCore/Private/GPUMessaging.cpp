// Copyright Epic Games, Inc. All Rights Reserved.

#include "GPUMessaging.h"
#include "RenderResource.h"
#include "RenderGraphUtils.h"
#include "RHIGPUReadback.h"
#include "GlobalShader.h"

DEFINE_LOG_CATEGORY_STATIC(LogGPUMessaging, Log, All);

RDG_REGISTER_BLACKBOARD_STRUCT(GPUMessage::FParameters)

class FClearMessageBufferCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FClearMessageBufferCS);
	SHADER_USE_PARAMETER_STRUCT(FClearMessageBufferCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, GPUMessageDataBuffer)
	END_SHADER_PARAMETER_STRUCT()
};

IMPLEMENT_GLOBAL_SHADER(FClearMessageBufferCS, "/Engine/Private/GPUMessaging.usf", "ClearMessageBuffer", SF_Compute);

namespace GPUMessage
{

int32 GLogAllMessages = 0;
static FAutoConsoleVariableRef CVarLogAllMessages(
	TEXT("r.GPUMessage.LogAllMessages"),
	GLogAllMessages,
	TEXT("Log all messages to the console.\n")
	TEXT("0: Disabled\n")
	TEXT("1: Enabled\n"),
	ECVF_RenderThreadSafe
);

int32 GMaxBufferSize = 64;
static FAutoConsoleVariableRef CVarMaxBufferSize(
	TEXT("r.GPUMessage.MaxBufferSize"),
	GMaxBufferSize,
	TEXT("Specifies the maximum size of the GPU message buffer, in KiB.\n")
	TEXT("default: 64\n"),
	ECVF_RenderThreadSafe | ECVF_ReadOnly
);

class FSystem : public FRenderResource
{
public:
	// Need to guard access because it may happen during FScene initialization (Game Thread) while another scene is being rendered.
	FCriticalSection AccessCS;


	FSocket RegisterHandler(const TSharedPtr<FHandler>& Handler)
	{
		FScopeLock Lock(&AccessCS);
		const FMessageId MessageId(NextMessageId++);
		check(!MessageHandlers.Contains(MessageId));
		MessageHandlers.Add(MessageId, Handler);
		return FSocket(MessageId);
	}

	void RemoveHandler(FMessageId MessageId)
	{
		FScopeLock Lock(&AccessCS);
		check(MessageId.IsValid());
		check(MessageHandlers.Contains(MessageId));
		MessageHandlers.Remove(MessageId);
	}

	void BeginMessageScope(FRDGBuilder& GraphBuilder)
	{
		check(MessageBuffer == nullptr);
		FScopeLock Lock(&AccessCS);

		PollMessages();

		if (MessageHandlers.IsEmpty())
		{
			return;
		}

		const uint32 MaxBufferSize = GetMaxBufferSize();

		FRDGBufferDesc Desc = FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), MaxBufferSize);
		Desc.Usage |= EBufferUsageFlags::SourceCopy;
		MessageBuffer = GraphBuilder.CreateBuffer(Desc, TEXT("GPUMessageManager.MessageBuffer"));

		FParameters& Parameters = GraphBuilder.Blackboard.Create<FParameters>();
		Parameters.GPUMessageDataBuffer = GraphBuilder.CreateUAV(MessageBuffer);
		Parameters.GPUMessageDataBufferSize = MaxBufferSize;

		TShaderMapRef<FClearMessageBufferCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));

		FClearMessageBufferCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FClearMessageBufferCS::FParameters>();
		PassParameters->GPUMessageDataBuffer = Parameters.GPUMessageDataBuffer;

		FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("ClearGPUMessageBuffer"), ComputeShader, PassParameters, FIntVector(1, 1, 1));
	}

	void EndMessageScope(FRDGBuilder& GraphBuilder)
	{
		FScopeLock Lock(&AccessCS);

		if (MessageHandlers.IsEmpty())
		{
			check(MessageBuffer == nullptr);
			return;
		}

		check(MessageBuffer != nullptr);

		FRHIGPUBufferReadback* GPUBufferReadback = nullptr;
		if (!MessageReadbackBuffersReady.IsEmpty())
		{
			GPUBufferReadback = MessageReadbackBuffersReady.Pop();
		}
		else
		{
			GPUBufferReadback = new FRHIGPUBufferReadback(TEXT("GPUMessageManager.Readback"));
		}

		AddEnqueueCopyPass(GraphBuilder, GPUBufferReadback, MessageBuffer, 0u);
		MessageReadbackBuffersInUse.Add(GPUBufferReadback);

		MessageBuffer = nullptr;
	}

private:
	void PollMessages()
	{
		for (int32 Index = 0; Index < MessageReadbackBuffersInUse.Num();)
		{
			FRHIGPUBufferReadback* GPUBufferReadback = MessageReadbackBuffersInUse[Index];

			if (GPUBufferReadback->IsReady())
			{
				const uint32 MaxBufferSize = GetMaxBufferSize();

				const uint32* BufferPtr = (const uint32*)GPUBufferReadback->Lock(MaxBufferSize * sizeof(uint32));
				uint32 ValidRangeEnd = BufferPtr[0] + 1U;

				FString LogMessage;

				const uint32 OriginalValidRangeEnd = ValidRangeEnd;
				ValidRangeEnd = FMath::Min(ValidRangeEnd, MaxBufferSize);
				ensureMsgf(ValidRangeEnd == OriginalValidRangeEnd, TEXT("GPU messages size %u exceeded maximum size %u. Results have been truncated."), OriginalValidRangeEnd, MaxBufferSize);

				if (GLogAllMessages != 0)
				{
					if (ValidRangeEnd != OriginalValidRangeEnd)
					{
						LogMessage.Appendf(TEXT("GPU messages size %u exceeded maximum size %u. Results have been truncated!\n\n"), OriginalValidRangeEnd, MaxBufferSize);
					}

					LogMessage.Appendf(TEXT("\nMessage Buffer[0] = %d;\n"), BufferPtr[0]);

					for (uint32 ReadOffset = 1U; ReadOffset < ValidRangeEnd; )
					{
						const FMessageId MessageId(BufferPtr[ReadOffset]);
						uint32 PayloadNumUints = BufferPtr[ReadOffset + 1];

						LogMessage.Appendf(TEXT("  Message[Offset: %d, ID: %d, PayloadSize: %d, "), ReadOffset, MessageId, PayloadNumUints);

						if (auto Handler = MessageHandlers.Find(MessageId))
						{
							LogMessage.Appendf(TEXT("Name: %s]"), (*Handler)->GetName());
						}
						else
						{
							LogMessage.Append(TEXT("<Unknown>]"));
						}

						if (PayloadNumUints > 0)
						{
							LogMessage.Append(TEXT(" Payload: { "));
							for (uint32 PayloadIndex = 0; PayloadIndex < PayloadNumUints; ++PayloadIndex)
							{
								LogMessage.Appendf(TEXT("%d%s"), BufferPtr[ReadOffset + 2U + PayloadIndex], PayloadIndex + 1U == PayloadNumUints ? TEXT("") : TEXT(", "));
							}
							LogMessage.Append(TEXT(" }"));
						}

						LogMessage.Append(TEXT("\n"));

						// Step past the message to the next
						ReadOffset += PayloadNumUints + 2U;
					}

					UE_LOG(LogGPUMessaging, Log, TEXT("%s"), *LogMessage);
				}

				for (uint32 ReadOffset = 1U; ReadOffset < ValidRangeEnd; )
				{
					const FMessageId MessageId(BufferPtr[ReadOffset]);
					uint32 PayloadNumUints = BufferPtr[ReadOffset + 1];

					if (auto Handler = MessageHandlers.Find(MessageId))
					{
						(*Handler)->Execute(FReader(MessageId, PayloadNumUints, &BufferPtr[ReadOffset + 2U]));
					}

					// Step past the message to the next
					ReadOffset += PayloadNumUints + 2U;
				}

				GPUBufferReadback->Unlock();

				MessageReadbackBuffersInUse.RemoveAt(Index);
				MessageReadbackBuffersReady.Push(GPUBufferReadback);
			}
			else
			{
				++Index;
			}
		}
	}

	void ReleaseDynamicRHI() override
	{
		FScopeLock Lock(&AccessCS);

		check(MessageBuffer == nullptr);

		for (FRHIGPUBufferReadback* ReadbackBuffer : MessageReadbackBuffersInUse)
		{
			delete ReadbackBuffer;
		}
		MessageReadbackBuffersInUse.Empty();

		for (FRHIGPUBufferReadback* ReadbackBuffer : MessageReadbackBuffersReady)
		{
			delete ReadbackBuffer;
		}
		MessageReadbackBuffersReady.Empty();
	}

	uint32 GetMaxBufferSize() const { return GMaxBufferSize * 1024; }

	TMap<FMessageId, TSharedPtr<FHandler>> MessageHandlers;

	TArray<FRHIGPUBufferReadback*, TInlineAllocator<8>> MessageReadbackBuffersInUse;
	TArray<FRHIGPUBufferReadback*, TInlineAllocator<8>> MessageReadbackBuffersReady;
	FRDGBuffer* MessageBuffer{};

	uint32 NextMessageId = 0;
};

static TGlobalResource<FSystem> GSystem;

void FSocket::Reset()
{
	if (MessageId.IsValid())
	{
		GSystem.RemoveHandler(MessageId);
	}

	MessageId = FMessageId::Null;
}

bool FScope::bRecursionCheck = false;

FScope::FScope(FRDGBuilder& InGraphBuilder)
	: GraphBuilder(InGraphBuilder)
{
	checkf(!bRecursionCheck, TEXT("GPUMessage::FScope (GPU_MESSAGE_SCOPE) has already been pushed. Only one scope can be active at a time."));
	bRecursionCheck = true;
	GSystem.BeginMessageScope(InGraphBuilder);
}

FScope::~FScope()
{
	GSystem.EndMessageScope(GraphBuilder);
	bRecursionCheck = false;
}

FSocket RegisterHandler(const TSharedPtr<FHandler>& Handler)
{
	return GSystem.RegisterHandler(Handler);
}

FParameters GetShaderParameters(FRDGBuilder& GraphBuilder)
{
	const FParameters* Parameters = GraphBuilder.Blackboard.Get<FParameters>();
	checkf(Parameters, TEXT("Shader parameters not initialized. GPUMessage::GetShaderParameters may only be called within a GPU_MESSAGE_SCOPE."));
	return *Parameters;
}

} // GPUMessage