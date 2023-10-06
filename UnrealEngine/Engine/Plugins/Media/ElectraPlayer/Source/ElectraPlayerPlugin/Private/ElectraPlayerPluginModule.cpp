// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"

#include "Modules/ModuleManager.h"
#include "RHI.h"

#include "ElectraPlayerMisc.h"
#include "IElectraPlayerPluginModule.h"
#include "IElectraPlayerRuntimeModule.h"
#include "ElectraPlayerPlugin.h"
#include "ParameterDictionary.h"

#define LOCTEXT_NAMESPACE "ElectraPlayerPluginModule"

DEFINE_LOG_CATEGORY(LogElectraPlayerPlugin);

// -----------------------------------------------------------------------------------------------------------------------------------

class FElectraPlayerPluginModule : public IElectraPlayerPluginModule
{
public:
	// IElectraPlayerPluginModule interface

	virtual bool IsInitialized() const override
	{
		return bInitialized;
	}

	virtual TSharedPtr<IMediaPlayer, ESPMode::ThreadSafe> CreatePlayer(IMediaEventSink& EventSink) override
	{
		if (bInitialized)
		{
			FElectraPlayerPlugin* NewRawPlayer = new FElectraPlayerPlugin();
			if (NewRawPlayer)
			{
				TSharedPtr<IMediaPlayer, ESPMode::ThreadSafe> NewPlayer = MakeShareable(NewRawPlayer);
				if (NewRawPlayer->Initialize(EventSink, SendAnalyticMetricsDelegate, SendAnalyticMetricsPerMinuteDelegate, ReportVideoStreamingErrorDelegate, ReportSubtitlesMetricsDelegate))
				{
					return NewPlayer;
				}
			}
		}
		return nullptr;
	}

	virtual void SendAnalyticMetrics(const TSharedPtr<IAnalyticsProviderET>& AnalyticsProvider, const FGuid& PlayerGuid) override
	{
		SendAnalyticMetricsDelegate.Broadcast(AnalyticsProvider, PlayerGuid);
	}

	virtual void SendAnalyticMetricsPerMinute(const TSharedPtr<IAnalyticsProviderET>& AnalyticsProvider) override
	{
		SendAnalyticMetricsPerMinuteDelegate.Broadcast(AnalyticsProvider);
	}

	virtual void ReportVideoStreamingError(const FGuid& PlayerGuid, const FString& LastError) override
	{
		ReportVideoStreamingErrorDelegate.Broadcast(PlayerGuid, LastError);
	}

	virtual void ReportSubtitlesMetrics(const FGuid& PlayerGuid, const FString& URL, double ResponseTime, const FString& LastError)
	{
		ReportSubtitlesMetricsDelegate.Broadcast(PlayerGuid, URL, ResponseTime, LastError);
	}

public:
	// IModuleInterface interface
	static void GetDynamicRHIInfo(void** OutGDynamicRHI, int64* OutGDynamicRHIType)
	{
		if (OutGDynamicRHI)
		{
			*OutGDynamicRHI = GDynamicRHI ? GDynamicRHI->RHIGetNativeDevice() : nullptr;
		}
		if (OutGDynamicRHIType)
		{
			*OutGDynamicRHIType = (int64)RHIGetInterfaceType();
		}
	}

	void StartupModule() override
	{
		// Check that we have the player module and that it has initialized successfully.
		if (FModuleManager::Get().GetModule("ElectraPlayerRuntime"))
		{
			IElectraPlayerRuntimeModule* ElectraPlayer = &FModuleManager::Get().GetModuleChecked<IElectraPlayerRuntimeModule>("ElectraPlayerRuntime");
			if (!ElectraPlayer || !ElectraPlayer->IsInitialized())
			{
				return;
			}
		}
		else
		{
			return;
		}

		if (!bInitialized)
		{
//WE COULD ALSO CHECK FOR SINGLE THREADING ISSUES HERE!? -- kinda piggy back on this?
			// to detect cooking and other commandlets that run with NullRHI
			if (GDynamicRHI == nullptr || RHIGetInterfaceType() == ERHIInterfaceType::Null)
			{
				UE_LOG(LogElectraPlayerPlugin, Log, TEXT("Dummy Dynamic RHI detected. Electra Player plugin is not initialised."));
				return;
			}

			Electra::FParamDict Params;
			Params.Set("GetDeviceTypeCallback", Electra::FVariantValue((void*)&FElectraPlayerPluginModule::GetDynamicRHIInfo));
			if (!FElectraPlayerPlatform::StartupPlatformResources(Params))
			{
				UE_LOG(LogElectraPlayerPlugin, Log, TEXT("Platform resource setup failed! Electra Player plugin is not initialised."));
				return;
			}

			bInitialized = true;
		}
	}

	void ShutdownModule() override
	{
		bInitialized = false;
	}

private:
	bool bInitialized = false;

	FElectraPlayerSendAnalyticMetricsDelegate			SendAnalyticMetricsDelegate;
	FElectraPlayerSendAnalyticMetricsPerMinuteDelegate	SendAnalyticMetricsPerMinuteDelegate;
	FElectraPlayerReportVideoStreamingErrorDelegate		ReportVideoStreamingErrorDelegate;
	FElectraPlayerReportSubtitlesMetricsDelegate		ReportSubtitlesMetricsDelegate;
};

IMPLEMENT_MODULE(FElectraPlayerPluginModule, ElectraPlayerPlugin);

#undef LOCTEXT_NAMESPACE


