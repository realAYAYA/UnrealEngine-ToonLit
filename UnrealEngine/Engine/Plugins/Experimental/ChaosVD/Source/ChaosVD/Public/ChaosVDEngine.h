// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"
#include "Containers/Ticker.h"
#include "Misc/Guid.h"

class FChaosVDTraceManager;
class FChaosVDPlaybackController;
class FChaosVDScene;
class FChaosVisualDebuggerMainUI;

/** Core Implementation of the visual debugger - Owns the systems that are not UI */
class FChaosVDEngine : public FTSTickerObjectBase, public TSharedFromThis<FChaosVDEngine>
{
public:
	FChaosVDEngine()
	{
		InstanceGUID = FGuid::NewGuid();
	}

	void Initialize();
	
	void DeInitialize();

	virtual bool Tick(float DeltaTime) override;

	const FGuid& GetInstanceGuid() const { return InstanceGUID; };
	TSharedPtr<FChaosVDScene>& GetCurrentScene() { return CurrentScene; };
	TSharedPtr<FChaosVDPlaybackController>& GetPlaybackController() { return PlaybackController; };

	void LoadRecording(const FString& FilePath);
	
private:

	FGuid InstanceGUID;

	FString CurrentSessionName;

	TSharedPtr<FChaosVDScene> CurrentScene;
	TSharedPtr<FChaosVDPlaybackController> PlaybackController;
	
	bool bIsInitialized = false;
};
