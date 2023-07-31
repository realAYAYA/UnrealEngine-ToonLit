// Copyright Epic Games, Inc. All Rights Reserved.

#include "XRSwapChain.h"
#include "RenderingThread.h"
#include "XRThreadUtils.h"
#include "CoreGlobals.h"

#ifdef _DEBUG
#include "CoreMinimal.h"
#include "HAL/RunnableThread.h"
#include "RHI.h"
#endif // _DEBUG

//-------------------------------------------------------------------------------------------------
// FTextureSetProxy
//-------------------------------------------------------------------------------------------------


FORCEINLINE void CheckInRenderThread()
{
#if DO_CHECK
	check(IsInRenderingThread());
#endif
}

bool InRHIOrValidThread()
{
	if (GIsThreadedRendering && !GIsRenderingThreadSuspended.Load(EMemoryOrder::Relaxed))
	{
		if (IsRHIThreadRunning())
		{
			if (IsInRHIThread())
			{
				return true;
			}

			if (IsInActualRenderingThread())
			{
				return GetImmediateCommandList_ForRenderCommand().Bypass();
			}

			return false;
		}
		else
		{
			return IsInActualRenderingThread();
		}
	}
	else
	{
		return IsInGameThread();
	}
}


FORCEINLINE void CheckInRHIThread()
{
#if DO_CHECK
	check(InRHIOrValidThread());
#endif
}

FXRSwapChain::FXRSwapChain(TArray<FTextureRHIRef>&& InRHITextureSwapChain, const FTextureRHIRef& AliasedTexture)
	: RHITexture(AliasedTexture)
	, RHITextureSwapChain(InRHITextureSwapChain)
	, SwapChainIndex_RHIThread(0)
{
	check(RHITexture);
	// @todo: The *correct* way to create the RHITexture object (that's just an alias over swap-chain textures)
	//		  would be via a new RHI API to create an aliased texture. For now we just let the clients of this class
	//		  create themselves (since they all create differently).
	RHITexture->SetName(TEXT("XRSwapChainAliasedTexture"));
	for (int ChainElement = 0; ChainElement < RHITextureSwapChain.Num(); ++ChainElement)
	{
		RHITextureSwapChain[ChainElement]->SetName(FName(*FString::Printf(TEXT("XRSwapChainBackingTex%d"), ChainElement)));
	}
}


void FXRSwapChain::GenerateMips_RenderThread(FRHICommandListImmediate& RHICmdList)
{
	CheckInRenderThread();

	if (RHITexture->GetNumMips() > 1 && RHITexture->GetTextureCube() == nullptr)
	{
#if PLATFORM_WINDOWS
		RHICmdList.GenerateMips(RHITexture);
#endif
	}
}


void FXRSwapChain::IncrementSwapChainIndex_RHIThread()
{
	CheckInRHIThread();

	SwapChainIndex_RHIThread = (SwapChainIndex_RHIThread + 1) % GetSwapChainLength();
	GDynamicRHI->RHIAliasTextureResources((FTextureRHIRef&)RHITexture, (FTextureRHIRef&)RHITextureSwapChain[SwapChainIndex_RHIThread]);
}


void FXRSwapChain::ReleaseResources_RHIThread()
{
	CheckInRHIThread();

	RHITexture = nullptr;
	RHITextureSwapChain.Empty();
}

