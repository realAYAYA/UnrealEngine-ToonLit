// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineBaseTypes.h"
#include "HAL/Runnable.h"

/**
* Implementation of WaitSyncThread
*/

class IMediaIOCoreHardwareSync;

class MEDIAIOCORE_API FMediaIOCoreWaitVSyncThread : public FRunnable
{
public:
	FMediaIOCoreWaitVSyncThread(TSharedPtr<IMediaIOCoreHardwareSync> InHardwareSync);

	virtual bool Init() override;
	virtual uint32 Run() override;

	virtual void Stop() override;
	virtual void Exit() override;

public:
	bool Wait_GameOrRenderThread();

protected:
	TSharedPtr<IMediaIOCoreHardwareSync> HardwareSync;
	FEvent* WaitVSync;
	TAtomic<bool> bWaitingForSignal;
	TAtomic<bool> bAlive;
};
