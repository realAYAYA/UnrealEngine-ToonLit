// Copyright Epic Games, Inc. All Rights Reserved.

#include "Trace/ChaosVDTraceManager.h"

#include "Modules/ModuleManager.h"
#include "Features/IModularFeatures.h"
#include "Trace/ChaosVDTraceModule.h"
#include "TraceServices/AnalysisService.h"
#include "TraceServices/ITraceServicesModule.h"
#include "TraceServices/Model/AnalysisSession.h"

FChaosVDTraceManager::FChaosVDTraceManager() 
{
	ChaosVDTraceModule = MakeShared<FChaosVDTraceModule>();
	IModularFeatures::Get().RegisterModularFeature(TraceServices::ModuleFeatureName, ChaosVDTraceModule.Get());

	FString ChannelNameFString(TEXT("ChaosVD"));
	UE::Trace::ToggleChannel(ChannelNameFString.GetCharArray().GetData(), true);
}

FChaosVDTraceManager::~FChaosVDTraceManager()
{
	IModularFeatures::Get().UnregisterModularFeature(TraceServices::ModuleFeatureName, ChaosVDTraceModule.Get());
}

FString FChaosVDTraceManager::LoadTraceFile(const FString& InTraceFilename)
{
	CloseSession(InTraceFilename);

	ITraceServicesModule& TraceServicesModule = FModuleManager::LoadModuleChecked<ITraceServicesModule>("TraceServices");
	if (const TSharedPtr<TraceServices::IAnalysisService> TraceAnalysisService = TraceServicesModule.GetAnalysisService())
	{
		const TSharedPtr<const TraceServices::IAnalysisSession> NewSession = TraceAnalysisService->StartAnalysis(*InTraceFilename);
		AnalysisSessionByName.Add(InTraceFilename, NewSession);
		return NewSession->GetName();
	}

	return FString();
}

TSharedPtr<const TraceServices::IAnalysisSession> FChaosVDTraceManager::GetSession(const FString& InSessionName)
{
	if (TSharedPtr<const TraceServices::IAnalysisSession>* FoundSession = AnalysisSessionByName.Find(InSessionName))
	{
		return *FoundSession;
	}

	return nullptr;
}

void FChaosVDTraceManager::CloseSession(const FString& InSessionName)
{
	if (const TSharedPtr<const TraceServices::IAnalysisSession>* Session = AnalysisSessionByName.Find(InSessionName))
	{
		if (Session->IsValid())
		{
			(*Session)->Stop(true);
		}
		
		AnalysisSessionByName.Remove(InSessionName);
	}
}
