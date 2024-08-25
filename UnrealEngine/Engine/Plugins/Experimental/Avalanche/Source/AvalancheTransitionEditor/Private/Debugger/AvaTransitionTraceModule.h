// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_STATETREE_DEBUGGER

#include "Trace/StoreClient.h"
#include "TraceServices/ModuleService.h"

class FAvaTransitionTraceModule : public TraceServices::IModule
{
	static FAvaTransitionTraceModule* Get();

public:
	static void Startup();
	static void Shutdown();

private:
	//~ Begin IModule
	virtual void GetModuleInfo(TraceServices::FModuleInfo& OutModuleInfo) override;
	virtual void OnAnalysisBegin(TraceServices::IAnalysisSession& InSession) override;
	//~ End IModule
};

#endif // WITH_STATETREE_DEBUGGER
