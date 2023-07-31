// Copyright Epic Games, Inc. All Rights Reserved.

#include "Render/Device/QuadBufferStereo/DisplayClusterDeviceQuadBufferStereoBase.h"

#include "Misc/DisplayClusterLog.h"

#include "Render/Viewport/IDisplayClusterViewportManager.h"

#include <utility>


FDisplayClusterDeviceQuadBufferStereoBase::FDisplayClusterDeviceQuadBufferStereoBase()
	: FDisplayClusterDeviceBase(EDisplayClusterRenderFrameMode::Stereo)
{
}
