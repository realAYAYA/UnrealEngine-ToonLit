// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_STATETREE_DEBUGGER

#include "AvaTransitionTraceModule.h"
#include "AvaTransitionTraceAnalyzer.h"
#include "Features/IModularFeatures.h"
#include "TraceServices/Model/AnalysisSession.h"

FAvaTransitionTraceModule* FAvaTransitionTraceModule::Get()
{
	static FAvaTransitionTraceModule TraceModule;
	return &TraceModule;
}

void FAvaTransitionTraceModule::Startup()
{
	IModularFeatures::Get().RegisterModularFeature(TraceServices::ModuleFeatureName, Get());
}

void FAvaTransitionTraceModule::Shutdown()
{
	IModularFeatures::Get().UnregisterModularFeature(TraceServices::ModuleFeatureName, Get());
}

void FAvaTransitionTraceModule::GetModuleInfo(TraceServices::FModuleInfo& OutModuleInfo)
{
	OutModuleInfo.Name = TEXT("TraceModule_AvaTransition");
	OutModuleInfo.DisplayName = TEXT("Motion Design Transition Trace");
}

void FAvaTransitionTraceModule::OnAnalysisBegin(TraceServices::IAnalysisSession& InSession)
{
	InSession.AddAnalyzer(new FAvaTransitionTraceAnalyzer);
}

#endif // WITH_STATETREE_DEBUGGER
