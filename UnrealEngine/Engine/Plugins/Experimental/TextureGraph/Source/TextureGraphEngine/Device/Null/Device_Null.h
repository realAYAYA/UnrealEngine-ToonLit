// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Device/Device.h"

class TEXTUREGRAPHENGINE_API Device_Null : public Device
{
protected:

	virtual void					Collect(DeviceBuffer* Buffer) override;

public:
									Device_Null();
	virtual							~Device_Null() override;

	virtual FString					Name() const override { return "Device_Null"; }

	//////////////////////////////////////////////////////////////////////////
	/// Static functions
	//////////////////////////////////////////////////////////////////////////
	static Device_Null*				Get();
};
