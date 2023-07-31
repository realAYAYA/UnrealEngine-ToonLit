// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "OculusHMDPrivate.h"
#include "IXRLoadingScreen.h"

#if WITH_EDITOR
#include "Editor.h"
#endif

#if OCULUS_HMD_SUPPORTED_PLATFORMS
#include "OculusHMD_GameFrame.h"
#include "OculusHMD_Layer.h"
#include "TickableObjectRenderThread.h"
#include "OculusHMDTypes.h"

namespace OculusHMD
{

class FOculusHMD;

//-------------------------------------------------------------------------------------------------
// FSplashLayer
//-------------------------------------------------------------------------------------------------

struct FSplashLayer
{
	FOculusSplashDesc Desc;
	FLayerPtr Layer;

public:
	FSplashLayer(const FOculusSplashDesc& InDesc) : Desc(InDesc) {}
	FSplashLayer(const FSplashLayer& InSplashLayer) : Desc(InSplashLayer.Desc), Layer(InSplashLayer.Layer) {}
};


//-------------------------------------------------------------------------------------------------
// FSplash
//-------------------------------------------------------------------------------------------------

class FSplash : public IXRLoadingScreen, public TSharedFromThis<FSplash>
{
protected:
	class FTSTicker : public FTickableObjectRenderThread, public TSharedFromThis<FTSTicker>
	{
	public:
		FTSTicker(FSplash* InSplash) : FTickableObjectRenderThread(false, true), pSplash(InSplash) {}

		virtual void Tick(float DeltaTime) override { pSplash->Tick_RenderThread(DeltaTime); }
		virtual TStatId GetStatId() const override  { RETURN_QUICK_DECLARE_CYCLE_STAT(FSplash, STATGROUP_Tickables); }
		virtual bool IsTickable() const override { return true; }
	
	protected:
		FSplash* pSplash;
	};

public:
	FSplash(FOculusHMD* InPlugin);
	virtual ~FSplash();

	void Tick_RenderThread(float DeltaTime);

	void Startup();
	void LoadSettings();
	void ReleaseResources_RHIThread();
	void PreShutdown();
	void Shutdown();

	void OnPreLoadMap(const FString&);
	void OnPostLoadMap(UWorld* LoadedWorld);
#if WITH_EDITOR
	void OnPieBegin(bool bIsSimulating);
#endif

	// Called from FOculusHMD
	void UpdateLoadingScreen_GameThread();

	// Internal extended API
	int AddSplash(const FOculusSplashDesc&);
	bool GetSplash(unsigned index, FOculusSplashDesc& OutDesc);
	void StopTicker();
	void StartTicker();

	// The standard IXRLoadingScreen interface
	virtual void ShowLoadingScreen() override;
	virtual void HideLoadingScreen() override;
	virtual void ClearSplashes() override;
	virtual void AddSplash(const FSplashDesc& Splash) override;
	virtual bool IsShown() const override { return bIsShown; }

protected:
	void DoShow();
	void DoHide();
	void UnloadTextures();
	void LoadTexture(FSplashLayer& InSplashLayer);
	void UnloadTexture(FSplashLayer& InSplashLayer);

	void RenderFrame_RenderThread(FRHICommandListImmediate& RHICmdList);
	IStereoLayers::FLayerDesc StereoLayerDescFromOculusSplashDesc(FOculusSplashDesc OculusDesc);

protected:
	FOculusHMD* OculusHMD;
	FCustomPresent* CustomPresent;
	TSharedPtr<FTSTicker> Ticker;
	int32 FramesOutstanding;
	FCriticalSection RenderThreadLock;
	FSettingsPtr Settings;
	FGameFramePtr Frame;
	TArray<FSplashLayer> SplashLayers;
	uint32 NextLayerId;
	FLayerPtr BlackLayer;
	FLayerPtr UELayer;
	TArray<TTuple<FLayerPtr, FQuat>> Layers_RenderThread_DeltaRotation;
	TArray<FLayerPtr> Layers_RenderThread_Input;
	TArray<FLayerPtr> Layers_RenderThread;
	TArray<FLayerPtr> Layers_RHIThread;

	// All these flags are only modified from the Game thread
	bool bInitialized;
	bool bIsShown;
	bool bNeedSplashUpdate;
	bool bShouldShowSplash;

	float SystemDisplayInterval;
	double LastTimeInSeconds;
	FDelegateHandle PreLoadLevelDelegate;
	FDelegateHandle PostLoadLevelDelegate;
#if WITH_EDITOR
	FDelegateHandle PieBeginDelegateHandle;
#endif
};

typedef TSharedPtr<FSplash> FSplashPtr;


} // namespace OculusHMD

#endif // OCULUS_HMD_SUPPORTED_PLATFORMS