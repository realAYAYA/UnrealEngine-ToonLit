// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "NetworkPredictionTraceModule.h"
#include "Framework/Docking/TabManager.h"
#include "INetworkPredictionInsightsModule.h"
#include "Trace/StoreService.h"

struct FInsightsMajorTabExtender;

namespace UE
{
namespace Trace 
{
#if WITH_TRACE_STORE
	class FStoreService;
#endif
	class FStoreClient;
}
}

class FNetworkPredictionInsightsModule : public IModuleInterface
{
public:
	// IModuleInterface interface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	static void StartNetworkTrace();

	FNetworkPredictionTraceModule NetworkPredictionTraceModule;

	FTSTicker::FDelegateHandle TickerHandle;
	FDelegateHandle StoreServiceHandle;

#if WITH_TRACE_STORE
	TSharedPtr<UE::Trace::FStoreService> StoreService;
#endif
	//TWeakPtr<FTabManager> WeakTimingProfilerTabManager;

	static const FName InsightsTabName;	
};
