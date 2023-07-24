// Copyright Epic Games, Inc. All Rights Reserved.

#include "TraceAnalysisModule.h"

#include "MessageLog/Public/MessageLogModule.h"
#include "Modules/ModuleManager.h"

////////////////////////////////////////////////////////////////////////////////
void FTraceAnalysisModule::StartupModule()
{
	FMessageLogModule& MessageLogModule = FModuleManager::LoadModuleChecked<FMessageLogModule>("MessageLog");
	MessageLogModule.RegisterLogListing(GetMessageLogName(), NSLOCTEXT("Trace", "TraceAnalysis", "Trace Analysis"));
	MessageLogModule.EnableMessageLogDisplay(true);
}

////////////////////////////////////////////////////////////////////////////////
FName FTraceAnalysisModule::GetMessageLogName()
{
	static FName Name("TraceAnalysis");
	return Name;
}

////////////////////////////////////////////////////////////////////////////////
IMPLEMENT_MODULE(FTraceAnalysisModule, TraceAnalysis);
