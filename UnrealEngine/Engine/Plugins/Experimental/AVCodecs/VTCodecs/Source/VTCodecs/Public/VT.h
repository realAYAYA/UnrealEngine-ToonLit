// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AVUtility.h"

THIRD_PARTY_INCLUDES_START
#include <VideoToolbox/VideoToolbox.h>
THIRD_PARTY_INCLUDES_END

class VTCODECS_API FVT : public FAPI
{
public:
	bool bHasCompatibleGPU = true;

	FVT();

	virtual bool IsValid() const override;
};

DECLARE_TYPEID(FVT, VTCODECS_API);
