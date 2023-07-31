// Copyright Epic Games, Inc. All Rights Reserved.

#include "PlatformEventsModule.h"
#include "Analyzers/PlatformEventTraceAnalysis.h"
#include "AnalysisServicePrivate.h"
#include "Model/ContextSwitchesPrivate.h"
#include "Model/StackSamplesPrivate.h"

namespace TraceServices
{

static const FName PlatformEventsModuleName("TraceModule_PlatformEvents");

void FPlatformEventsModule::GetModuleInfo(FModuleInfo& OutModuleInfo)
{
	OutModuleInfo.Name = PlatformEventsModuleName;
	OutModuleInfo.DisplayName = TEXT("PlatformEvents");
}
	
void FPlatformEventsModule::OnAnalysisBegin(IAnalysisSession& InSession)
{
	FAnalysisSession& Session = static_cast<FAnalysisSession&>(InSession);

	TSharedPtr<FContextSwitchesProvider> ContextSwitchesProvider = MakeShared<FContextSwitchesProvider>(Session);
	TSharedPtr<FStackSamplesProvider> StackSamplesProvider = MakeShared<FStackSamplesProvider>(Session);
	
	Session.AddProvider(FContextSwitchesProvider::ProviderName, ContextSwitchesProvider);
	Session.AddProvider(FStackSamplesProvider::ProviderName, StackSamplesProvider);
	Session.AddAnalyzer(new FPlatformEventTraceAnalyzer(Session, *ContextSwitchesProvider, *StackSamplesProvider));
}

} // namespace TraceServices
