// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "DeviceType.h"
#include "Profiling/StatGroup.h"
#include "Helper/Promise.h"
#include <memory>

class Device;
class Device_Mem;

using AsyncBool = cti::continuable<bool>;

class TEXTUREGRAPHENGINE_API DeviceManager
{
private:
	Device*							Devices[(size_t)DeviceType::Count] = {};	/// The devices available within the system
	Device_Mem*						DefaultDevice = nullptr;					/// What is the default device on the system. This is always the CPU device

	void							InitDevices();
	void							ReleaseDevices();

public:
									DeviceManager();
	virtual							~DeviceManager();

	void							Update(float dt);
	AsyncBool						WaitForQueuedTasks(ENamedThreads::Type returnThread = ENamedThreads::UnusedAnchor);
	Device*							GetDevice(DeviceType type) const;
	Device*							GetDevice(size_t Index) const;

	//////////////////////////////////////////////////////////////////////////
	/// Inline functions
	//////////////////////////////////////////////////////////////////////////
	FORCEINLINE size_t				GetNumDevices() const { return (size_t)DeviceType::Count; }
	FORCEINLINE Device_Mem*			GetDefaultDevice() const { return DefaultDevice; }
};

typedef std::unique_ptr<DeviceManager> DeviceManagerPtr;
