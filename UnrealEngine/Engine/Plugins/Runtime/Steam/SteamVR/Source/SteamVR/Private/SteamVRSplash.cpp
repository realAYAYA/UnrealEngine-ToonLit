// Copyright Epic Games, Inc. All Rights Reserved.

#include "SteamVRSplash.h"
#include "SteamVRPrivate.h"
#include "SteamVRHMD.h"

#if STEAMVR_SUPPORTED_PLATFORMS

FSteamSplashTicker::FSteamSplashTicker(class FSteamVRHMD* InSteamVRHMD)
	: FTickableObjectRenderThread(false, true)
	, SteamVRHMD(InSteamVRHMD)
{}

void FSteamSplashTicker::RegisterForMapLoad()
{
	FCoreUObjectDelegates::PreLoadMap.AddSP(this, &FSteamSplashTicker::OnPreLoadMap);
	FCoreUObjectDelegates::PostLoadMapWithWorld.AddSP(this, &FSteamSplashTicker::OnPostLoadMap);
}

void FSteamSplashTicker::UnregisterForMapLoad()
{
	FCoreUObjectDelegates::PreLoadMap.RemoveAll(this);
	FCoreUObjectDelegates::PostLoadMapWithWorld.RemoveAll(this);
};

void FSteamSplashTicker::OnPreLoadMap(const FString&)
{
	FTickableObjectRenderThread* Ticker = this;
	ENQUEUE_RENDER_COMMAND(RegisterAsyncTick)(
		[Ticker](FRHICommandListImmediate& RHICmdList)
		{
			Ticker->Register();
		});
}

void FSteamSplashTicker::OnPostLoadMap(UWorld*)
{
	FTickableObjectRenderThread* Ticker = this;
	ENQUEUE_RENDER_COMMAND(UnregisterAsyncTick)(
		[Ticker](FRHICommandListImmediate& RHICmdList)
		{
			Ticker->Unregister();
		});
}

void FSteamSplashTicker::Tick(float DeltaTime)
{
	int32 Dummy = 0;

	// Note, that we use the fact that BridgeBaseImpl::Present only returns false when VRCompositor is null,
	// even though when used by the renderer as an indication whether normal present is needed.
	if (SteamVRHMD->bSplashIsShown && SteamVRHMD->pBridge && SteamVRHMD->pBridge->Present(Dummy))
	{
		check(SteamVRHMD->VRCompositor);
		SteamVRHMD->VRCompositor->PostPresentHandoff();
	}
}
TStatId FSteamSplashTicker::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(FSplashTicker, STATGROUP_Tickables);
}

bool FSteamSplashTicker::IsTickable() const
{
	return true;
}

#endif
