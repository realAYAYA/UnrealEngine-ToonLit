// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "RequiredProgramMainCPPInclude.h" // required for ue programs

IMPLEMENT_APPLICATION(AutoRTFMTests, "AutoRTFMTests");

#include "catch_amalgamated.cpp"

class SetupListener final : public Catch::EventListenerBase
{
public:
	using Catch::EventListenerBase::EventListenerBase;

	void testRunStarting(const Catch::TestRunInfo&) override
	{
		GEngineLoop.PreInit(0, nullptr);
		FModuleManager::Get().StartProcessingNewlyLoadedObjects();

		AutoRTFM::ForTheRuntime::SetAutoRTFMRuntime(AutoRTFM::ForTheRuntime::EAutoRTFMEnabledState::AutoRTFM_Enabled);
	}

	void testRunEnded(const Catch::TestRunStats&) override
	{
		FEngineLoop::AppPreExit();
		FModuleManager::Get().UnloadModulesAtShutdown();
		FEngineLoop::AppExit();

		FPlatformMisc::RequestExit(false);
	}
};

CATCH_REGISTER_LISTENER(SetupListener)
