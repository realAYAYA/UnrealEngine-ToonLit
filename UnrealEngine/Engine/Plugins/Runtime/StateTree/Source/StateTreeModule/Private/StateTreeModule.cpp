// Copyright Epic Games, Inc. All Rights Reserved.

#include "StateTreeModule.h"

#include "StateTreeTypes.h"

#if WITH_STATETREE_DEBUGGER
#include "Debugger/StateTreeTrace.h"
#include "Debugger/StateTreeTraceModule.h"
#include "Features/IModularFeatures.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CoreDelegates.h"
#include "ProfilingDebugging/TraceAuxiliary.h"
#include "StateTreeDelegates.h"
#include "StateTreeSettings.h"
#include "Trace/StoreClient.h"
#include "Trace/StoreService.h"
#include "TraceServices/AnalysisService.h"
#include "TraceServices/ITraceServicesModule.h"

#endif // WITH_STATETREE_DEBUGGER

#if WITH_EDITORONLY_DATA
#include "StateTreeInstanceData.h"
#endif // WITH_EDITORONLY_DATA

#define LOCTEXT_NAMESPACE "StateTree"

class FStateTreeModule : public IStateTreeModule
{
	// Begin IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	virtual bool StartTraces(int32& OutTraceId) override;
	virtual bool IsTracing() const override;
	virtual void StopTraces() override;

#if WITH_STATETREE_DEBUGGER
	/**
	 * Gets the store client.
	 */
	virtual UE::Trace::FStoreClient* GetStoreClient() override
	{
		if (!StoreClient.IsValid())
		{
			StoreClient = TUniquePtr<UE::Trace::FStoreClient>(UE::Trace::FStoreClient::Connect(TEXT("localhost")));
		}
		return StoreClient.Get();
	}
	
	TSharedPtr<TraceServices::IAnalysisService> TraceAnalysisService;
	TSharedPtr<TraceServices::IModuleService> TraceModuleService;

	TArray<const FString> ChannelsToRestore;

	/** The client used to connect to the trace store. */
	TUniquePtr<UE::Trace::FStoreClient> StoreClient;
	
	FStateTreeTraceModule StateTreeTraceModule;

	/** Keep track if StartTraces was explicitly called. */
	bool bIsTracing = false;

	FAutoConsoleCommand StartDebuggerTracesCommand = FAutoConsoleCommand(
		TEXT("statetree.startdebuggertraces"),
		TEXT("Turns on StateTree debugger traces if not already active."),
		FConsoleCommandDelegate::CreateLambda([]
			{
				IStateTreeModule& StateTreeModule = FModuleManager::GetModuleChecked<IStateTreeModule>("StateTreeModule");
				int32 TraceId = 0;
				StateTreeModule.StartTraces(TraceId);
			}));

	FAutoConsoleCommand StopDebuggerTracesCommand = FAutoConsoleCommand(
		TEXT("statetree.stopdebuggertraces"),
		TEXT("Turns off StateTree debugger traces if active."),
		FConsoleCommandDelegate::CreateLambda([]
			{
				IStateTreeModule& StateTreeModule = FModuleManager::GetModuleChecked<IStateTreeModule>("StateTreeModule");
				StateTreeModule.StopTraces();
			}));
#endif // WITH_STATETREE_DEBUGGER
};

IMPLEMENT_MODULE(FStateTreeModule, StateTreeModule)

void FStateTreeModule::StartupModule()
{
#if WITH_STATETREE_DEBUGGER
	ITraceServicesModule& TraceServicesModule = FModuleManager::LoadModuleChecked<ITraceServicesModule>("TraceServices");
	TraceAnalysisService = TraceServicesModule.GetAnalysisService();
	TraceModuleService = TraceServicesModule.GetModuleService();

	IModularFeatures::Get().RegisterModularFeature(TraceServices::ModuleFeatureName, &StateTreeTraceModule);

	UE::StateTreeTrace::RegisterGlobalDelegates();
#if !WITH_EDITOR
	// We don't automatically start traces for Editor targets since we rely on the debugger
	// to start recording either on user action or on PIE session start.
	if (UStateTreeSettings::Get().bAutoStartDebuggerTracesOnNonEditorTargets)
	{
		int32 TraceId = INDEX_NONE;
		StartTraces(TraceId);
	}
#endif // !WITH_EDITOR

#endif // WITH_STATETREE_DEBUGGER

#if WITH_EDITORONLY_DATA
	UE::StateTree::RegisterInstanceDataForLocalization();
#endif // WITH_EDITORONLY_DATA
}

void FStateTreeModule::ShutdownModule()
{
#if WITH_STATETREE_DEBUGGER
	StopTraces();

	if (StoreClient.IsValid())
	{
		StoreClient.Reset();
	}

	UE::StateTreeTrace::UnregisterGlobalDelegates();
	
	IModularFeatures::Get().UnregisterModularFeature(TraceServices::ModuleFeatureName, &StateTreeTraceModule);
#endif // WITH_STATETREE_DEBUGGER
}

bool FStateTreeModule::StartTraces(int32& OutTraceId)
{
	OutTraceId = INDEX_NONE;
#if WITH_STATETREE_DEBUGGER
	if (IsRunningCommandlet() || bIsTracing)
	{
		return false;
	}

	FGuid SessionGuid, TraceGuid;
	const bool bAlreadyConnected = FTraceAuxiliary::IsConnected(SessionGuid, TraceGuid);

	if (UE::Trace::FStoreClient* Client = GetStoreClient())
	{
		const UE::Trace::FStoreClient::FSessionInfo* SessionInfo = StoreClient->GetSessionInfoByGuid(TraceGuid);
		// Note that 0 is returned instead of INDEX_NONE to match default invalid value for GetTraceId 
		OutTraceId = SessionInfo != nullptr ? SessionInfo->GetTraceId(): 0;
	}

	// If trace is already connected let's keep track of enabled channels to restore them when we stop recording
	if (bAlreadyConnected)
	{
		UE::Trace::EnumerateChannels([](const ANSICHAR* Name, const bool bIsEnabled, void* Channels)
		{
			TArray<FString>* EnabledChannels = static_cast<TArray<FString>*>(Channels); 
			if (bIsEnabled)
			{
				EnabledChannels->Emplace(ANSI_TO_TCHAR(Name));
			}		
		}, &ChannelsToRestore);
	}
	else
	{
		// Disable all channels and then enable only those we need to minimize trace file size.
		UE::Trace::EnumerateChannels([](const ANSICHAR* ChannelName, const bool bEnabled, void*)
			{
				if (bEnabled)
				{
					FString ChannelNameFString(ChannelName);
					UE::Trace::ToggleChannel(ChannelNameFString.GetCharArray().GetData(), false);
				}
			}
		, nullptr);
	}

	UE::Trace::ToggleChannel(TEXT("StateTreeDebugChannel"), true);
	UE::Trace::ToggleChannel(TEXT("FrameChannel"), true);

	bool bAreTracesStarted = false;
	if (bAlreadyConnected == false)
	{
		FTraceAuxiliary::FOptions Options;
		Options.bExcludeTail = true;
		bAreTracesStarted = FTraceAuxiliary::Start(FTraceAuxiliary::EConnectionType::Network, TEXT("localhost"), TEXT(""), &Options, LogStateTree);
	}

	bIsTracing = true;
	if (UE::StateTree::Delegates::OnTracingStateChanged.IsBound())
	{
		UE_LOG(LogStateTree, Log, TEXT("StateTree traces enabled"));
		UE::StateTree::Delegates::OnTracingStateChanged.Broadcast(bIsTracing);
	}

	return bAreTracesStarted;
#else
	return false;
#endif // WITH_STATETREE_DEBUGGER
}

bool FStateTreeModule::IsTracing() const
{
#if WITH_STATETREE_DEBUGGER
	return bIsTracing;
#else
	return false;
#endif // WITH_STATETREE_DEBUGGER
}

void FStateTreeModule::StopTraces()
{
#if WITH_STATETREE_DEBUGGER
	if (bIsTracing == false)
	{
		return;
	}

	UE::Trace::ToggleChannel(TEXT("StateTreeDebugChannel"), false);
	UE::Trace::ToggleChannel(TEXT("FrameChannel"), false);

	// When we have channels to restore it also indicates that the trace were active
	// so we only toggle the channels back (i.e. not calling FTraceAuxiliary::Stop)
	if (ChannelsToRestore.Num() > 0)
	{
		for (const FString& ChannelName : ChannelsToRestore)
		{
			UE::Trace::ToggleChannel(ChannelName.GetCharArray().GetData(), true);
		}
		ChannelsToRestore.Reset();
	}
	else
	{
		FTraceAuxiliary::Stop();
	}

	bIsTracing = false;

	if (UE::StateTree::Delegates::OnTracingStateChanged.IsBound())
	{
		UE_LOG(LogStateTree, Log, TEXT("StateTree traces disabled"));
		UE::StateTree::Delegates::OnTracingStateChanged.Broadcast(bIsTracing);
	}
#endif // WITH_STATETREE_DEBUGGER
}

#undef LOCTEXT_NAMESPACE
