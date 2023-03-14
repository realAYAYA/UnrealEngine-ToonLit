// Copyright Epic Games, Inc. All Rights Reserved.

/*==============================================================================
Controller class running on game clients that handles the passing of messages to a connected Niagara debugger.
==============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "NiagaraCommon.h"
#include "NiagaraDebuggerCommon.h"
#include "IMessageContext.h"
#include "Particles/ParticlePerfStatsManager.h"
#include "NiagaraSimCache.h"
#include "UObject/StrongObjectPtr.h"

class FMessageEndpoint;

#if WITH_PARTICLE_PERF_STATS

/**
Listener that accumulates a short run of stats for all systems and components in the scene and reports those stats to the outliner.
*/
class NIAGARA_API FNiagaraOutlinerPerfListener : public FParticlePerfStatsListener_GatherAll
{
public:
	FNiagaraOutlinerPerfListener() : FParticlePerfStatsListener_GatherAll(true, true, true)
	{

	}
};

#endif

#if WITH_NIAGARA_DEBUGGER


struct FNiagaraSimCacheCaptureInfo
{
	uint32 ProcessedFrames = 0;
	FNiagaraSystemSimCacheCaptureRequest Request;
	TWeakObjectPtr<UNiagaraComponent> Component;

	TStrongObjectPtr<UNiagaraSimCache> SimCache = nullptr;

	/** Process this request. Captures data where needed. Returns true if complete. */
	bool Process();
};

DECLARE_LOG_CATEGORY_EXTERN(LogNiagaraDebuggerClient, Log, All);

class FNiagaraDebuggerClient
{
public:

	static FNiagaraDebuggerClient* Get();

	FNiagaraDebuggerClient();
	~FNiagaraDebuggerClient();

	bool Tick(float DeltaSeconds);

	void UpdateClientInfo();

private:

	void HandleConnectionRequestMessage(const FNiagaraDebuggerRequestConnection& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context);
	void HandleConnectionClosedMessage(const FNiagaraDebuggerConnectionClosed& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context);
	void HandleExecConsoleCommandMessage(const FNiagaraDebuggerExecuteConsoleCommand& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context);
	void HandleDebugHUDSettingsMessage(const FNiagaraDebugHUDSettingsData& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context);
	void HandleRequestSimpleClientInfoMessage(const FNiagaraRequestSimpleClientInfoMessage& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context);
	void HandleOutlinerSettingsMessage(const FNiagaraOutlinerCaptureSettings& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context);
	void HandleSimCacheCaptureRequestMessage(const FNiagaraSystemSimCacheCaptureRequest& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context);

	/** Closes any currently active connection. */
	void CloseConnection();

	/** Handle any cleanup needed whether we close the connection or the client does. */
	void OnConnectionClosed();

	void ExecuteConsoleCommand(const TCHAR* Cmd, bool bRequiresWorld);

	bool UpdateOutliner(float DeltaSeconds);

	/** Holds the session and instance identifier. */
	FGuid SessionId;
	FGuid InstanceId;
	FString InstanceName;

	/** Holds the messaging endpoint. */
	TSharedPtr<FMessageEndpoint, ESPMode::ThreadSafe> MessageEndpoint;

	/** The address of the connected debugger, if any. */
	FMessageAddress Connection;

	FNiagaraOutlinerCaptureSettings OutlinerSettings;
	FTSTicker::FDelegateHandle TickerHandle;

	uint32 OutlinerCountdown = 0;

#if WITH_PARTICLE_PERF_STATS
	TSharedPtr<FNiagaraOutlinerPerfListener, ESPMode::ThreadSafe> StatsListener;
#endif

	/* All currently ongoing sim cache captures. */
	TArray<FNiagaraSimCacheCaptureInfo> SimCacheCaptures;
};

#endif