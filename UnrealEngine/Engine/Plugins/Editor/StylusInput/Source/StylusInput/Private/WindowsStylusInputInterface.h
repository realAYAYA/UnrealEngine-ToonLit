// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IStylusInputModule.h"

class FWindowsStylusInputInterfaceImpl;
class SWindow;

class FWindowsStylusInputInterface : public IStylusInputInterfaceInternal
{
public:
	FWindowsStylusInputInterface(TUniquePtr<FWindowsStylusInputInterfaceImpl> InImpl);
	virtual ~FWindowsStylusInputInterface();

	virtual void Tick() override;
	virtual int32 NumInputDevices() const override;
	virtual IStylusInputDevice* GetInputDevice(int32 Index) const override;

private:
	// pImpl to avoid including Windows headers.
	TUniquePtr<FWindowsStylusInputInterfaceImpl> Impl;
	TArray<IStylusMessageHandler*> MessageHandlers;

	void CreateStylusPluginForHWND(void* HwndPtr);
	void RemovePluginForWindow(const TSharedRef<SWindow>& Window);
};
