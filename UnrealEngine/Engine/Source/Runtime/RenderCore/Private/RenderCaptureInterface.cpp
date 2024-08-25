// Copyright Epic Games, Inc. All Rights Reserved.

#include "RenderCaptureInterface.h"
#include "IRenderCaptureProvider.h"
#include "RenderGraphBuilder.h"
#include "RenderingThread.h"

namespace RenderCaptureInterface
{
	FScopedCapture::FScopedCapture(bool bEnable, TCHAR const* InEventName, TCHAR const* InFileName)
		: bCapture(bEnable && IRenderCaptureProvider::IsAvailable())
		, bEvent(InEventName != nullptr)
		, RHICommandList(nullptr)
		, GraphBuilder(nullptr)
	{
		check(!GIsThreadedRendering || !IsInRenderingThread());

		if (bCapture)
		{
			ENQUEUE_RENDER_COMMAND(BeginCaptureCommand)([bPushEvent = bEvent, EventName = FString(InEventName), FileName = FString(InFileName)](FRHICommandListImmediate& RHICommandListLocal)
			{
				IRenderCaptureProvider::Get().BeginCapture(&RHICommandListLocal, IRenderCaptureProvider::ECaptureFlags_Launch, FileName);

				if (bPushEvent)
				{
					RHICommandListLocal.PushEvent(*EventName, FColor::White);
				}
			});
		}
	}

	FScopedCapture::FScopedCapture(bool bEnable, FRHICommandList* InRHICommandList, TCHAR const* InEventName, TCHAR const* InFileName)
		: bCapture(bEnable && IRenderCaptureProvider::IsAvailable() && InRHICommandList->IsImmediate())
		, bEvent(InEventName != nullptr)
		, RHICommandList(InRHICommandList)
		, GraphBuilder(nullptr)
	{
		if (bCapture)
		{
			check(!GIsThreadedRendering || IsInRenderingThread());

			IRenderCaptureProvider::Get().BeginCapture(&FRHICommandListImmediate::Get(*RHICommandList), IRenderCaptureProvider::ECaptureFlags_Launch, FString(InFileName));
		
			if (bEvent)
			{
				RHICommandList->PushEvent(InEventName, FColor::White);
			}
		}
	}

	FScopedCapture::FScopedCapture(bool bEnable, FRDGBuilder& InGraphBuilder, TCHAR const* InEventName, TCHAR const* InFileName)
		: bCapture(bEnable&& IRenderCaptureProvider::IsAvailable())
		, bEvent(InEventName != nullptr)
		, RHICommandList(nullptr)
		, GraphBuilder(&InGraphBuilder) 
	{
		check(!GIsThreadedRendering || IsInRenderingThread());

		if (bCapture)
		{
			GraphBuilder->AddPass(
				RDG_EVENT_NAME("BeginCapture"),
				ERDGPassFlags::None, 
				[FileName = FString(InFileName)](FRHICommandListImmediate& RHICommandListLocal)
				{
					IRenderCaptureProvider::Get().BeginCapture(&RHICommandListLocal, IRenderCaptureProvider::ECaptureFlags_Launch, FString(FileName));
				});

			if (bEvent)
			{
				GraphBuilder->BeginEventScope(RDG_EVENT_NAME("%s", InEventName));
			}
		}
	}

	FScopedCapture::~FScopedCapture()
	{
		if (bCapture)
		{
			if (GraphBuilder != nullptr)
			{
				check(!GIsThreadedRendering || IsInRenderingThread());

				if (bEvent)
				{
					GraphBuilder->EndEventScope();
				}

				GraphBuilder->AddPass(
					RDG_EVENT_NAME("EndCapture"), 
					ERDGPassFlags::None, 
					[](FRHICommandListImmediate& RHICommandListLocal)
					{
						IRenderCaptureProvider::Get().EndCapture(&RHICommandListLocal);
					});
			}
			else if (RHICommandList != nullptr)
			{
				check(!GIsThreadedRendering || IsInRenderingThread());
				
				if (bEvent)
				{
					RHICommandList->PopEvent();
				}

				IRenderCaptureProvider::Get().EndCapture(&FRHICommandListImmediate::Get(*RHICommandList));
			}
			else
			{
				check(!GIsThreadedRendering || !IsInRenderingThread());

				ENQUEUE_RENDER_COMMAND(EndCaptureCommand)([bPopEvent = bEvent](FRHICommandListImmediate& RHICommandListLocal)
				{
					if (bPopEvent)
					{
						RHICommandListLocal.PopEvent();
					}

					IRenderCaptureProvider::Get().EndCapture(&RHICommandListLocal);
				});
			}
		}
 	}
}
