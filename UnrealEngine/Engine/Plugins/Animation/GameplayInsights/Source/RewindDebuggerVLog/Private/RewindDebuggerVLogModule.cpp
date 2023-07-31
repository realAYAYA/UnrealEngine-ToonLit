// Copyright Epic Games, Inc. All Rights Reserved.

#include "RewindDebuggerVLogModule.h"
#include "Modules/ModuleManager.h"
#include "Features/IModularFeatures.h"

#define LOCTEXT_NAMESPACE "RewindDebuggerVLogModule"

#ifndef ENABLE_REWINDDEBUGGER_VLOG_INTEGRATION
#define ENABLE_REWINDDEBUGGER_VLOG_INTEGRATION 0
#endif

void FRewindDebuggerVLogModule::StartupModule()
{
#if ENABLE_REWINDDEBUGGER_VLOG_INTEGRATION
	IModularFeatures::Get().RegisterModularFeature(IRewindDebuggerExtension::ModularFeatureName, &RewindDebuggerVLogExtension);
	IModularFeatures::Get().RegisterModularFeature(TraceServices::ModuleFeatureName, &VLogTraceModule);
#endif
}

void FRewindDebuggerVLogModule::ShutdownModule()
{
#if ENABLE_REWINDDEBUGGER_VLOG_INTEGRATION
	IModularFeatures::Get().UnregisterModularFeature(IRewindDebuggerExtension::ModularFeatureName, &RewindDebuggerVLogExtension);
	IModularFeatures::Get().UnregisterModularFeature(TraceServices::ModuleFeatureName, &VLogTraceModule);
#endif
}

IMPLEMENT_MODULE(FRewindDebuggerVLogModule, RewindDebuggerVLog);

#undef LOCTEXT_NAMESPACE
