// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosVDRecordingStateScreenMessageHandler.h"

#if WITH_CHAOS_VISUAL_DEBUGGER
#include "ChaosVDRuntimeModule.h"
#include "Engine/Engine.h"

void FChaosVDRecordingStateScreenMessageHandler::HandleCVDRecordingStarted()
{
	if (!GEngine)
	{
		return;
	}
	
	static FText ChaosVDRecordingStartedMessage = NSLOCTEXT("ChaosVisualDebugger", "OnScreenChaosVDRecordingStartedMessage", "Chaos Visual Debugger recording in progress...");

	if (CVDRecordingMessageKey == 0)
	{
		CVDRecordingMessageKey = GetTypeHash(ChaosVDRecordingStartedMessage.ToString());
	}

	// Add a long duration value, we will remove the message manually when the recording stops
	constexpr float MessageDurationSeconds = 3600.0f;
	GEngine->AddOnScreenDebugMessage(CVDRecordingMessageKey, MessageDurationSeconds, FColor::Red,ChaosVDRecordingStartedMessage.ToString());
}

void FChaosVDRecordingStateScreenMessageHandler::HandleCVDRecordingStopped() const
{
	if (!GEngine)
	{
		return;
	}

	if (CVDRecordingMessageKey != 0)
	{
		GEngine->RemoveOnScreenDebugMessage(CVDRecordingMessageKey);
	}
}

void FChaosVDRecordingStateScreenMessageHandler::HandleCVDRecordingStartFailed(const FText& InFailureReason) const
{
#if !WITH_EDITOR
	// In non-editor builds we don't have an error pop-up, therefore we want to show the error message on screen
	FText ErrorMessage = FText::FormatOrdered(NSLOCTEXT("ChaosVisualDebugger","StartRecordingFailedOnScreenMessage", "Failed to start CVD recording. {0}"), InFailureReason);

	constexpr float MessageDurationSeconds = 4.0f;
	GEngine->AddOnScreenDebugMessage(CVDRecordingMessageKey, MessageDurationSeconds, FColor::Red, ErrorMessage.ToString());
#endif
}

void FChaosVDRecordingStateScreenMessageHandler::HandlePIEStarted(UGameInstance* GameInstance)
{
	// If we were already recording, show the message
	if (FChaosVDRuntimeModule::Get().IsRecording())
	{
		HandleCVDRecordingStarted();
	}
}

FChaosVDRecordingStateScreenMessageHandler& FChaosVDRecordingStateScreenMessageHandler::Get()
{
	static FChaosVDRecordingStateScreenMessageHandler MessageHandler;
	return MessageHandler;
}

void FChaosVDRecordingStateScreenMessageHandler::Initialize()
{
	RecordingStartedHandle = FChaosVDRuntimeModule::Get().RegisterRecordingStartedCallback(FChaosVDRecordingStateChangedDelegate::FDelegate::CreateRaw(this, &FChaosVDRecordingStateScreenMessageHandler::HandleCVDRecordingStarted));
	RecordingStoppedHandle = FChaosVDRuntimeModule::Get().RegisterRecordingStopCallback(FChaosVDRecordingStateChangedDelegate::FDelegate::CreateRaw(this, &FChaosVDRecordingStateScreenMessageHandler::HandleCVDRecordingStopped));
	RecordingStartFailedHandle = FChaosVDRuntimeModule::Get().RegisterRecordingStartFailedCallback(FChaosVDRecordingStartFailedDelegate::FDelegate::CreateRaw(this, &FChaosVDRecordingStateScreenMessageHandler::HandleCVDRecordingStartFailed));

#if WITH_EDITOR
	PIEStartedHandle = FWorldDelegates::OnPIEStarted.AddRaw(this, &FChaosVDRecordingStateScreenMessageHandler::HandlePIEStarted);
#endif

	// If we were already recording, show the message
	if (FChaosVDRuntimeModule::Get().IsRecording())
	{
		HandleCVDRecordingStarted();
	}
}

void FChaosVDRecordingStateScreenMessageHandler::TearDown()
{
	// Note: This works during engine shutdown because the Module Manager doesn't free the dll on module unload to account for use cases like this
	// If this appears in a callstack crash it means that assumption changed or was not correct to begin with.
	// A possible solution is just check if the module is loaded querying the module manager just using the module's name
	if (FChaosVDRuntimeModule::IsLoaded())
	{
		FChaosVDRuntimeModule::Get().RemoveRecordingStartedCallback(RecordingStartedHandle);
		FChaosVDRuntimeModule::Get().RemoveRecordingStopCallback(RecordingStoppedHandle);

#if WITH_EDITOR
		 FWorldDelegates::OnPIEStarted.Remove(PIEStartedHandle);
#endif

		// Make sure of removing the message from the screen in case the recording didn't quite stopped yet
		if (FChaosVDRuntimeModule::Get().IsRecording())
		{
			HandleCVDRecordingStopped();
		}
	}

}
#endif
