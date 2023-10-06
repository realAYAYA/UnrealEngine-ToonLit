// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosVDEngine.h"

#include "ChaosVDModule.h"
#include "ChaosVDPlaybackController.h"
#include "ChaosVDScene.h"
#include "Trace/ChaosVDTraceManager.h"

void FChaosVDEngine::Initialize()
{
	if (bIsInitialized)
	{
		return;
	}

	// Create an Empty Scene
	//TODO: Handle multiple scenes. We will need it to represent multiple worlds
	CurrentScene = MakeShared<FChaosVDScene>();
	CurrentScene->Initialize();

	PlaybackController = MakeShared<FChaosVDPlaybackController>(CurrentScene);

	bIsInitialized = true;
}

void FChaosVDEngine::DeInitialize()
{
	if (!bIsInitialized)
	{
		return;
	}

	CurrentScene->DeInitialize();
	CurrentScene.Reset();
	PlaybackController.Reset();

	if (const TSharedPtr<FChaosVDTraceManager> CVDTraceManager = FChaosVDModule::Get().GetTraceManager())
	{
		CVDTraceManager->CloseSession(CurrentSessionName);
	}

	CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
}

void FChaosVDEngine::LoadRecording(const FString& FilePath)
{
	CurrentSessionName = FChaosVDModule::Get().GetTraceManager()->LoadTraceFile(FilePath);

	PlaybackController->LoadChaosVDRecordingFromTraceSession(CurrentSessionName);
}

bool FChaosVDEngine::Tick(float DeltaTime)
{
	return true;
}
