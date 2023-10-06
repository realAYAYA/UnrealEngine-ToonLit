// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "IRewindDebuggerExtension.h"
#include "RewindDebuggerSettings.h"
#include "UObject/WeakObjectPtr.h"

class ACameraActor;

// Rewind debugger extension for camera support
//  replay of recorded camera data
//  follow selected actor

class FRewindDebuggerCamera : public IRewindDebuggerExtension
{
public:


	FRewindDebuggerCamera();
	virtual ~FRewindDebuggerCamera() {};
	void Initialize();

	virtual void Update(float DeltaTime, IRewindDebugger* RewindDebugger) override;

	ERewindDebuggerCameraMode CameraMode() const;
	void SetCameraMode(ERewindDebuggerCameraMode Mode);
	
private:

	
	bool LastPositionValid;
	FVector LastPosition;
	
	TWeakObjectPtr<ACameraActor> CameraActor; 
};
