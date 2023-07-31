// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Render/Device/DisplayClusterDeviceBase.h"

/**
 * Abstract frame sequenced active stereo device
 */
class FDisplayClusterDeviceQuadBufferStereoBase :
	public FDisplayClusterDeviceBase
{
public:
	FDisplayClusterDeviceQuadBufferStereoBase();
	virtual ~FDisplayClusterDeviceQuadBufferStereoBase() = default;
};
