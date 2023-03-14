// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if __UNREAL__
#include "CoreMinimal.h"
#endif

/**
 * Render Device interface type
 */
enum class ETextureShareDeviceType : uint8
{
	Undefined = 0,

	D3D11,
	D3D12,

	Vulkan
};

/**
 * Process type
 */
enum class ETextureShareProcessType : uint8
{
	Undefined = 0,

	// Unreal Engine base process
	UE,

	// TextureShare SDK process
	SDK,

	// Marked as Invalid process
	Invalid
};

/**
 * Multithread mutex index
 * Using this enum, you can access multi-threaded mutexes.
 */
enum class ETextureShareThreadMutex : uint8
{
	GameThread = 0,
	RenderingThread,

	COUNT
};

/**
 * Texture operation
 */
enum class ETextureShareTextureOp : uint8
{
	// Send texture to remote process
	Write = 0,

	// Receive texture
	Read,

	// Operation not defined (useful to find any type of operation)
	Undefined,
};

/**
 * View eye type
 */
enum class ETextureShareEyeType : uint8
{
	// Monoscopic
	Default = 0,

	// Stereoscopic rendering
	StereoLeft,
	StereoRight
};

/**
 * Sync steps template type
 */
enum class ETextureShareFrameSyncTemplate : int8
{
	// Sync logic for BP object
	Default = 0,

	// Minimalistic sync logic for SDK object
	SDK,

	// Sync logic for DC object
	DisplayCluster,
};

/**
 * Sync steps values
 */
enum class ETextureShareSyncStep : int8
{
	InterprocessConnection = 0,
	
	/**
	 * Frame sync steps (GameThread)
	 */

	FrameBegin,

	FramePreSetupBegin,
	FramePreSetupEnd,

	FrameSetupBegin,
	FrameSetupEnd,

	FramePostSetupBegin,
	FramePostSetupEnd,

	FrameEnd,

	/**
	 * FrameProxy sync steps (RenderThread)
	 */

	FrameProxyBegin,

	FrameSceneFinalColorBegin,
	FrameSceneFinalColorEnd,

	FrameProxyPreRenderBegin,
	FrameProxyPreRenderEnd,

	FrameProxyRenderBegin,
	FrameProxyRenderEnd,

	FrameProxyPostWarpBegin,
	FrameProxyPostWarpEnd,

	FrameProxyPostRenderBegin,
	FrameProxyPostRenderEnd,

	FrameProxyBackBufferReadyToPresentBegin,
	FrameProxyBackBufferReadyToPresentEnd,

	FrameProxyFlush,

	FrameProxyEnd,

	/**
	 * Special values
	 */
	COUNT,
	Undefined = -1,
};

/**
 * Object sync barrier pass
 */
enum class ETextureShareSyncPass : int8
{
	Undefined = 0,

	Enter,
	Exit,
	Complete,

	COUNT
};

/**
 * Object sync barrier state
 */
enum class ETextureShareSyncState : int8
{
	// After sync reset
	Undefined = 0,

	Enter,
	EnterCompleted,

	Exit,
	ExitCompleted,

	Completed,
};

/**
 * Projection matrix source type
 */
enum class ETextureShareCoreSceneViewManualProjectionType : uint8
{
	FrustumAngles = 0,
	Matrix
};

/**
 * Rotator operation type
 * How to change the projection view rotator value
 */
enum class ETextureShareViewRotationDataType : uint8
{
	// Use original value
	Original = 0,

	// Add value from manual projection
	Relative,

	// Use value from manual projection
	Absolute
};

/**
 * Location operation type
 * How to change the projection view location value
 */
enum class ETextureShareViewLocationDataType : uint8
{
	// Use original value
	Original = 0,

	// Add value from manual projection
	Relative,

	// Use value from manual projection
	Absolute,

	// Use original value, but without stereo view offset
	Original_NoViewOffset,

	// Add value from manual projection, but without stereo view offset
	Relative_NoViewOffset,

	// Use value from manual projection, but without stereo view offset
	Absolute_NoViewOffset
};
