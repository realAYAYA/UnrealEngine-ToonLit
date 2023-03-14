// Copyright Epic Games, Inc. All Rights Reserved.

#include "LiveLinkModule.h"

#include "Interfaces/IPluginManager.h"
#include "LiveLinkLogInstance.h"
#include "LiveLinkPreset.h"
#include "LiveLinkSettings.h"
#include "Misc/CommandLine.h"
#include "Misc/CoreDelegates.h"
#include "Styling/SlateStyleRegistry.h"

LLM_DEFINE_TAG(LiveLink);
#define LOCTEXT_NAMESPACE "LiveLinkModule"

FLiveLinkClient* FLiveLinkModule::LiveLinkClient_AnyThread = nullptr;

FLiveLinkModule::FLiveLinkModule()
	: LiveLinkClient()
	, LiveLinkMotionController(LiveLinkClient)
	, HeartbeatEmitter(MakeUnique<FLiveLinkHeartbeatEmitter>())
#if WITH_LIVELINK_DISCOVERY_MANAGER_THREAD
	, DiscoveryManager(MakeUnique<FLiveLinkMessageBusDiscoveryManager>())
#endif
	, LiveLinkDebugCommand(MakeUnique<FLiveLinkDebugCommand>(LiveLinkClient))
{
}

void FLiveLinkModule::StartupModule()
{
	LLM_SCOPE_BYTAG(LiveLink);
	FLiveLinkLogInstance::CreateInstance();
	CreateStyle();

	FPlatformAtomics::InterlockedExchangePtr((void**)&LiveLinkClient_AnyThread, &LiveLinkClient);
	IModularFeatures::Get().RegisterModularFeature(FLiveLinkClient::ModularFeatureName, &LiveLinkClient);
	LiveLinkMotionController.RegisterController();

	//Register for engine initialization completed so we can load default preset if any. Presets could depend on plugins loaded at a later stage.
	FCoreDelegates::OnFEngineLoopInitComplete.AddRaw(this, &FLiveLinkModule::OnEngineLoopInitComplete);
}

void FLiveLinkModule::ShutdownModule()
{
	LLM_SCOPE_BYTAG(LiveLink);
	FCoreDelegates::OnFEngineLoopInitComplete.RemoveAll(this);

	HeartbeatEmitter->Exit();
#if WITH_LIVELINK_DISCOVERY_MANAGER_THREAD
	DiscoveryManager->Stop();
#endif
	LiveLinkMotionController.UnregisterController();

	IModularFeatures::Get().UnregisterModularFeature(FLiveLinkClient::ModularFeatureName, &LiveLinkClient);
	FPlatformAtomics::InterlockedExchangePtr((void**)&LiveLinkClient_AnyThread, nullptr);

	FSlateStyleRegistry::UnRegisterSlateStyle(*StyleSet.Get());
	FLiveLinkLogInstance::DestroyInstance();
}

void FLiveLinkModule::CreateStyle()
{
	static FName LiveLinkStyle(TEXT("LiveLinkCoreStyle"));
	StyleSet = MakeShared<FSlateStyleSet>(LiveLinkStyle);
	FSlateStyleRegistry::RegisterSlateStyle(*StyleSet.Get());

	FString ContentDir = IPluginManager::Get().FindPlugin(TEXT("LiveLink"))->GetContentDir();

	const FVector2D Icon16x16(16.0f, 16.0f);

	StyleSet->Set("LiveLinkIcon", new FSlateImageBrush((ContentDir / TEXT("LiveLink_16x")) + TEXT(".png"), Icon16x16));
}

void FLiveLinkModule::OnEngineLoopInitComplete()
{
	ULiveLinkPreset* StartupPreset = nullptr;
	const FString& CommandLine = FCommandLine::Get();
	const TCHAR* PresetStr = TEXT("LiveLink.Preset.Apply Preset="); // expected inside an -ExecCmds="" argument. So our command should end either on ',' or '"'.
	const int32 CommandStartPos = CommandLine.Find(PresetStr);

	if (CommandStartPos != INDEX_NONE)
	{
		int32 PresetEndPos = CommandLine.Find(",", ESearchCase::IgnoreCase, ESearchDir::FromStart, CommandStartPos);
		const int32 NextDoubleQuotesPos = CommandLine.Find("\"", ESearchCase::IgnoreCase, ESearchDir::FromStart, CommandStartPos);

		if ((PresetEndPos != INDEX_NONE) && (NextDoubleQuotesPos != INDEX_NONE))
		{
			PresetEndPos = FMath::Min(PresetEndPos, NextDoubleQuotesPos);
		}
		else if (NextDoubleQuotesPos != INDEX_NONE)
		{
			PresetEndPos = NextDoubleQuotesPos;
		}

		if (PresetEndPos != INDEX_NONE)
		{
			const int32 PresetStartPos = CommandStartPos + FCString::Strlen(PresetStr);
			if (CommandLine.IsValidIndex(PresetStartPos) && CommandLine.IsValidIndex(PresetEndPos))
			{
				const FString LiveLinkPresetName = CommandLine.Mid(PresetStartPos, PresetEndPos - PresetStartPos);
				StartupPreset = Cast<ULiveLinkPreset>(StaticLoadObject(ULiveLinkPreset::StaticClass(), nullptr, *LiveLinkPresetName));
			}
		}
	}

	if (StartupPreset == nullptr)
	{
		StartupPreset = GetDefault<ULiveLinkSettings>()->DefaultLiveLinkPreset.LoadSynchronous();
	}

	if (StartupPreset)
	{
		StartupPreset->ApplyToClientLatent();
	}
}

IMPLEMENT_MODULE(FLiveLinkModule, LiveLink);

#undef LOCTEXT_NAMESPACE