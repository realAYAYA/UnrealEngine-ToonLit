// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosVDEngine.h"

#include "ChaosVDModule.h"
#include "ChaosVDPlaybackController.h"
#include "ChaosVDRuntimeModule.h"
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

	// Listen for the recording stop event to clear the live session flag
	// TODO: We do something similar for the live flag on the CVD recording instance. We should unify both and have a single place where to check the live state
	// of a session, so we also have one single place to clear the flag
	LiveSessionStoppedDelegateHandle = FChaosVDRuntimeModule::Get().RegisterRecordingStopCallback(FChaosVDRecordingStateChangedDelegate::FDelegate::CreateLambda([WeakThis = AsWeak()]()
	{
		if (const TSharedPtr<FChaosVDEngine> CVDEngine = WeakThis.Pin())
		{
			CVDEngine->CurrentSessionDescriptor.bIsLiveSession = false;
		}
	}));

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
		CVDTraceManager->CloseSession(CurrentSessionDescriptor.SessionName);
	}

	if (FChaosVDRuntimeModule::IsLoaded())
	{
		FChaosVDRuntimeModule::Get().RemoveRecordingStopCallback(LiveSessionStoppedDelegateHandle);
	}

	LiveSessionStoppedDelegateHandle = FDelegateHandle();

	CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);

	bIsInitialized = false;
}

void FChaosVDEngine::LoadRecording(const FString& FilePath)
{
	FChaosVDTraceSessionDescriptor NewSessionFromFileDescriptor;
	NewSessionFromFileDescriptor.SessionName = FChaosVDModule::Get().GetTraceManager()->LoadTraceFile(FilePath);
	NewSessionFromFileDescriptor.bIsLiveSession = false;

	SetCurrentSession(NewSessionFromFileDescriptor);
}

void FChaosVDEngine::SetCurrentSession(const FChaosVDTraceSessionDescriptor& SessionDescriptor)
{
	CurrentSessionDescriptor = SessionDescriptor;
	PlaybackController->LoadChaosVDRecordingFromTraceSession(CurrentSessionDescriptor);
}

bool FChaosVDEngine::Tick(float DeltaTime)
{
	return true;
}
