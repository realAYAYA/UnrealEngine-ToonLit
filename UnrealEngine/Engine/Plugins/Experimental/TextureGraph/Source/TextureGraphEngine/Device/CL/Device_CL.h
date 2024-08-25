// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

/**
 * 
 */
class TEXTUREGRAPHENGINE_API Device_CL
{
public:
									Device_CL();
	virtual							~Device_CL();

	virtual const char*				Name() const { return "Device_CL"; }

	//////////////////////////////////////////////////////////////////////////
	/// Static functions
	//////////////////////////////////////////////////////////////////////////
	static Device_CL*				Get();
};
