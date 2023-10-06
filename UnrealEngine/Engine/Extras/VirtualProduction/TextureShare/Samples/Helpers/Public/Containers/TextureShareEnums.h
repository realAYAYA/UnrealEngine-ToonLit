// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Containers/TextureShareSDKContainers.h"

/**
 * Resource operation state
 */
enum class EResourceState : uint8
{
	Success = 0,
	E_UnknownError,

	E_INVALID_ARGS,
	E_INVALID_ARGS_TYPECAST,
	E_INVALID_DEVICE_TYPE,

	E_FrameSyncLost,
	E_ResourceSyncError,
	E_UnsupportedDevice,

	E_SharedResourceOpenFailed,
	E_SharedResourceSizeFormatNotEqual,

	E_NOT_IMPLEMENTED,
};
