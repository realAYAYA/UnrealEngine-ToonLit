// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Device/Device.h"

class TEXTUREGRAPHENGINE_API Device_Mem : public Device
{
protected:
									Device_Mem(DeviceType Type, DeviceBuffer* BufferFactory);

public:
									Device_Mem();
	virtual							~Device_Mem() override;

	virtual FString					Name() const override { return "Device_Mem"; }
	virtual void					Update(float Delta) override;
	virtual void					AddNativeTask(DeviceNativeTaskPtr Task) override;

	//////////////////////////////////////////////////////////////////////////
	/// Static functions
	//////////////////////////////////////////////////////////////////////////
	static Device_Mem*				Get();
};
