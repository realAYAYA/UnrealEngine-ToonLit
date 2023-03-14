// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BackChannel/Transport/IBackChannelSocketConnection.h"
#include "HAL/Runnable.h"
#include "HAL/ThreadSafeBool.h"

class FSocket;

DECLARE_DELEGATE_RetVal_OneParam(bool, FBackChannelListenerDelegate, TSharedRef<IBackChannelSocketConnection>);

/**
 * BackChannelClient implementation.
 */
class BACKCHANNEL_API FBackChannelThreadedListener : public FRunnable, public TSharedFromThis<FBackChannelThreadedListener>
{
public:

	FBackChannelThreadedListener();
	~FBackChannelThreadedListener();

	void Start(TSharedRef<IBackChannelSocketConnection> InConnection, FBackChannelListenerDelegate InDelegate);

	virtual void Stop() override;

	bool IsRunning() const;

protected:

	virtual uint32 Run() override;

private:

	TSharedPtr<IBackChannelSocketConnection>		Connection;
	FBackChannelListenerDelegate			Delegate;
	
	FThreadSafeBool							bExitRequested;
	FThreadSafeBool							bIsRunning;
	FCriticalSection						RunningCS;
};
