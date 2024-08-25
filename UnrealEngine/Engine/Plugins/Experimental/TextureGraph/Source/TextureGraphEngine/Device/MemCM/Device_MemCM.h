// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Device/Mem/Device_Mem.h"

class TEXTUREGRAPHENGINE_API Device_MemCM : public Device_Mem
{
public:
									Device_MemCM();
	virtual							~Device_MemCM() override;

	virtual FString					Name() const override { return "Device_MemCM"; }
	virtual void					Update(float Delta) override;

	//////////////////////////////////////////////////////////////////////////
	/// Static functions
	//////////////////////////////////////////////////////////////////////////
	static Device_MemCM*			Get();
};
