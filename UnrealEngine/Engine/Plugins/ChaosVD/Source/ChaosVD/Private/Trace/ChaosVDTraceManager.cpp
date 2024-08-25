// Copyright Epic Games, Inc. All Rights Reserved.

#include "Trace/ChaosVDTraceManager.h"

#include "ChaosVDModule.h"
#include "Features/IModularFeatures.h"
#include "Modules/ModuleManager.h"
#include "Misc/Paths.h"
#include "Trace/ChaosVDTraceModule.h"
#include "Trace/StoreClient.h"
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
		if (const TSharedPtr<const TraceServices::IAnalysisSession> NewSession = TraceAnalysisService->StartAnalysis(*InTraceFilename))
		{
			AnalysisSessionByName.Add(InTraceFilename, NewSession);
			return NewSession->GetName();
		}
	}

	return FString();
}

FString FChaosVDTraceManager::ConnectToLiveSession(FStringView InSessionHost, uint32 SessionID)
{	
	using namespace UE::Trace;
	FStoreClient* StoreClient = FStoreClient::Connect(InSessionHost.GetData());

	FString SessionName;

	if (!StoreClient)
	{
		return SessionName;
	}

	FStoreClient::FTraceData TraceData = StoreClient->ReadTrace(SessionID);
	if (!TraceData.IsValid())
	{
		return SessionName;
	}

	FString TraceName(StoreClient->GetStatus()->GetStoreDir());
	const FStoreClient::FTraceInfo* TraceInfo = StoreClient->GetTraceInfoById(SessionID);
	if (TraceInfo != nullptr)
	{
		const FUtf8StringView Utf8NameView = TraceInfo->GetName();
		FString Name(Utf8NameView);
		if (!Name.EndsWith(TEXT(".utrace")))
		{
			Name += TEXT(".utrace");
		}
		TraceName = FPaths::Combine(TraceName, Name);
		FPaths::NormalizeFilename(TraceName);
	}

	ITraceServicesModule& TraceServicesModule = FModuleManager::LoadModuleChecked<ITraceServicesModule>("TraceServices");
	if (const TSharedPtr<TraceServices::IAnalysisService> TraceAnalysisService = TraceServicesModule.GetAnalysisService())
	{
		// Close this session in case we were already analysing it
		CloseSession(TraceName);
	
		const TSharedPtr<const TraceServices::IAnalysisSession> NewSession = TraceAnalysisService->StartAnalysis(SessionID, *TraceName, MoveTemp(TraceData));
		AnalysisSessionByName.Add(TraceName, NewSession);
		SessionName = NewSession->GetName();
	}

	return SessionName;
}

FString FChaosVDTraceManager::GetLocalTraceStoreDirPath()
{
	UE::Trace::FStoreClient* StoreClient = UE::Trace::FStoreClient::Connect(TEXT("localhost"));

	if (!StoreClient)
	{
		UE_LOG(LogChaosVDEditor, Error, TEXT("[%s] Failed to connect to local Trace Store client"), ANSI_TO_TCHAR(__FUNCTION__));
		return TEXT("");
	}

	const UE::Trace::FStoreClient::FStatus* Status = StoreClient->GetStatus();
	if (!Status)
	{
		UE_LOG(LogChaosVDEditor, Error, TEXT("[%s] Failed to to get Trace Store staus"), ANSI_TO_TCHAR(__FUNCTION__));
		return TEXT("");
	}

	return FString(Status->GetStoreDir());
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
