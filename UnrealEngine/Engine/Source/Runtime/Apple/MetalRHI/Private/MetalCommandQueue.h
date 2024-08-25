// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <Metal/Metal.h>
#include "Containers/LockFreeList.h"

class FMetalCommandList;

/**
 * Enumeration of features which are present only on some OS/device combinations.
 * These have to be checked at runtime as well as compile time to ensure backward compatibility.
 */
typedef NS_OPTIONS(uint64, EMetalFeatures)
{
	/** Support for specifying an update to the buffer offset only */
	EMetalFeaturesSetBufferOffset = 1 << 0,
	/** Supports NSUInteger counting visibility queries */
	EMetalFeaturesCountingQueries = 1 << 1,
	/** Supports base vertex/instance for draw calls */
	EMetalFeaturesBaseVertexInstance = 1 << 2,
	/** Supports indirect buffers for draw calls */
	EMetalFeaturesIndirectBuffer = 1 << 3,
	/** Supports layered rendering */
	EMetalFeaturesLayeredRendering = 1 << 4,
	/** Support for specifying small buffers as byte arrays */
	EMetalFeaturesSetBytes = 1 << 5,
	/** Unused Reserved Bit */
	EMetalFeaturesUnusedReservedBit6 = 1 << 6, // was EMetalFeaturesTessellation
	/** Supports framework-level validation */
	EMetalFeaturesValidation = 1 << 7,
	/** Supports detailed statistics */
	EMetalFeaturesStatistics = 1 << 8,
	/** Supports the explicit MTLHeap APIs */
	EMetalFeaturesHeaps = 1 << 9,
	/** Supports the explicit MTLFence APIs */
	EMetalFeaturesFences = 1 << 10,
	/** Supports MSAA Depth Resolves */
	EMetalFeaturesMSAADepthResolve = 1 << 11,
	/** Supports Store & Resolve in a single store action */
	EMetalFeaturesMSAAStoreAndResolve = 1 << 12,
	/** Supports framework GPU frame capture */
	EMetalFeaturesGPUTrace = 1 << 13,
	/** Supports the use of cubemap arrays */
	EMetalFeaturesCubemapArrays = 1 << 14,
	/** Supports the specification of multiple viewports and scissor rects */
	EMetalFeaturesMultipleViewports = 1 << 15,
    /** Supports minimum on-glass duration for drawables */
    EMetalFeaturesPresentMinDuration = 1llu << 16llu,
    /** Supports programmatic frame capture API */
    EMetalFeaturesGPUCaptureManager = 1llu << 17llu,
	/** Supports efficient buffer-blits */
	EMetalFeaturesEfficientBufferBlits = 1llu << 18llu,
	/** Supports any kind of buffer sub-allocation */
	EMetalFeaturesBufferSubAllocation = 1llu << 19llu,
	/** Supports private buffer sub-allocation */
	EMetalFeaturesPrivateBufferSubAllocation = 1llu << 20llu,
	/** Supports texture buffers */
	EMetalFeaturesTextureBuffers = 1llu << 21llu,
	/** Supports max. compute threads per threadgroup */
	EMetalFeaturesMaxThreadsPerThreadgroup = 1llu << 22llu,
	/** Supports parallel render encoders */
	EMetalFeaturesParallelRenderEncoders = 1llu << 23llu,
	/** Supports indirect argument buffers */
	EMetalFeaturesIABs = 1llu << 24llu,
	/** Supports specifying the mutability of buffers bound to PSOs */
    EMetalFeaturesPipelineBufferMutability = 1llu << 25llu,
    /** Supports tile shaders */
    EMetalFeaturesTileShaders = 1llu << 26llu,
	/** Unused Reserved Bit */
	EMetalFeaturesUnusedReservedBit27 = 1llu << 27llu, // was EMetalFeaturesSeparateTessellation
	/** Supports indirect argument buffers Tier 2 */
	EMetalFeaturesTier2IABs = 1llu << 28llu,
};

/**
 * FMetalCommandQueue:
 */
class FMetalCommandQueue
{
public:
#pragma mark - Public C++ Boilerplate -

	/**
	 * Constructor
	 * @param Device The Metal device to create on.
	 * @param MaxNumCommandBuffers The maximum number of incomplete command-buffers, defaults to 0 which implies the system default.
	 */
	FMetalCommandQueue(MTL::Device* Device, uint32 const MaxNumCommandBuffers = 0);
	
	/** Destructor */
	~FMetalCommandQueue(void);
	
#pragma mark - Public Command Buffer Mutators -

	/**
	 * Start encoding to CommandBuffer. It is an error to call this with any outstanding command encoders or current command buffer.
	 * Instead call EndEncoding & CommitCommandBuffer before calling this.
	 * @param CommandBuffer The new command buffer to begin encoding to.
	 */
    FMetalCommandBuffer* CreateCommandBuffer(void);
	
	/**
	 * Commit the supplied command buffer immediately.
	 * @param CommandBuffer The command buffer to commit, must not be null
 	 */
	void CommitCommandBuffer(FMetalCommandBuffer* CommandBuffer);

	/** @returns Creates a new MTLFence or nullptr if this is unsupported */
	FMetalFence* CreateFence(NS::String* Label) const;
	
	/** @params Fences An array of command-buffer fences for the committed command-buffers */
	void GetCommittedCommandBufferFences(TArray<TSharedPtr<FMetalCommandBufferFence, ESPMode::ThreadSafe>>& Fences);
	
#pragma mark - Public Command Queue Accessors -
	
	/** @returns The command queue's native device. */
	MTL::Device* GetDevice(void);
	
	/** @returns The command queue's native device. */
	MTL::CommandQueue* GetQueue(void) { return CommandQueue; }

	/** Converts a Metal v1.1+ resource option to something valid on the current version. */
	static MTL::ResourceOptions GetCompatibleResourceOptions(MTL::ResourceOptions Options);
	
	/**
	 * @param InFeature A specific Metal feature to check for.
	 * @returns True if the requested feature is supported, else false.
	 */
	static inline bool SupportsFeature(EMetalFeatures InFeature) { return ((Features & InFeature) != 0); }

	/**
	* @param InFeature A specific Metal feature to check for.
	* @returns True if RHISupportsSeparateMSAAAndResolveTextures will be true.  
	* Currently Mac only.
	*/
	static inline bool SupportsSeparateMSAAAndResolveTarget() { return (PLATFORM_MAC != 0 || GMaxRHIFeatureLevel >= ERHIFeatureLevel::SM5); }

	/** @returns True on UMA system; false otherwise.  */
	static inline bool IsUMASystem() { return IsRHIDeviceApple(); }

#pragma mark - Public Debug Support -

	/** Inserts a boundary that marks the end of a frame for the debug capture tool. */
	void InsertDebugCaptureBoundary(void);
	
	/** Enable or disable runtime debugging features. */
	void SetRuntimeDebuggingLevel(int32 const Level);
	
	/** @returns The level of runtime debugging features enabled. */
	int32 GetRuntimeDebuggingLevel(void) const;

private:
#pragma mark - Private Member Variables -
	MTL::Device* Device;
	MTL::CommandQueue* CommandQueue;
	TArray<TSharedPtr<FMetalCommandBufferFence, ESPMode::ThreadSafe>> CommandBufferFences;
	int32 RuntimeDebuggingLevel;
	static NS::UInteger PermittedOptions;
	static uint64 Features;
};
