// Copyright Epic Games, Inc. All Rights Reserved.

#include "StageMessages.h"

#include "Misc/App.h"


namespace StageProviderMessageUtils
{
	static const FQualifiedFrameTime InvalidTime = FQualifiedFrameTime(FFrameTime(FFrameNumber(-1)), FFrameRate(-1, -1));
}

FStageProviderMessage::FStageProviderMessage()
{
	//Common setup of timecode for all provider messages
	TOptional<FQualifiedFrameTime> CurrentFrameTime = FApp::GetCurrentFrameTime();
	DateTime = FDateTime::Now();
	
	if (CurrentFrameTime.IsSet())
	{
		FrameTime = CurrentFrameTime.GetValue();
	}
	else
	{
		FrameTime = StageProviderMessageUtils::InvalidTime;
	}
}

FString FCriticalStateProviderMessage::ToString() const
{
	switch (State)
	{
		case EStageCriticalStateEvent::Enter:
		{
			return FString::Printf(TEXT("%s: Entered critical state"), *SourceName.ToString());
		}
		case EStageCriticalStateEvent::Exit:
		default:
		{
			return FString::Printf(TEXT("%s: Exited critical state"), *SourceName.ToString());
		}
	}
}

FString FAssetLoadingStateProviderMessage::ToString() const
{
	switch (LoadingState)
	{
		case EStageLoadingState::PreLoad:
		{
			return FString::Printf(TEXT("Started loading asset: %s"), *AssetName);
		}
		case EStageLoadingState::PostLoad:
		default:
		{
			return FString::Printf(TEXT("Finished loading asset: %s"), *AssetName);
		}
	}
}

