// Copyright Epic Games, Inc. All Rights Reserved.

#include "RHIStats.h"
#include "HAL/Platform.h"
#include "HAL/PlatformAtomics.h"
#include "Templates/Atomic.h"

int32 GNumDrawCallsRHI[MAX_NUM_GPUS] = {};
int32 GNumPrimitivesDrawnRHI[MAX_NUM_GPUS] = {};

// Define counter stats.
#if HAS_GPU_STATS
	DEFINE_STAT(STAT_RHIDrawPrimitiveCalls);
	DEFINE_STAT(STAT_RHITriangles);
	DEFINE_STAT(STAT_RHILines);
#endif

// Define memory stats.
DEFINE_STAT(STAT_RenderTargetMemory2D);
DEFINE_STAT(STAT_RenderTargetMemory3D);
DEFINE_STAT(STAT_RenderTargetMemoryCube);
DEFINE_STAT(STAT_UAVTextureMemory);
DEFINE_STAT(STAT_TextureMemory2D);
DEFINE_STAT(STAT_TextureMemory3D);
DEFINE_STAT(STAT_TextureMemoryCube);
DEFINE_STAT(STAT_UniformBufferMemory);
DEFINE_STAT(STAT_IndexBufferMemory);
DEFINE_STAT(STAT_VertexBufferMemory);
DEFINE_STAT(STAT_RTAccelerationStructureMemory);
DEFINE_STAT(STAT_StructuredBufferMemory);
DEFINE_STAT(STAT_ByteAddressBufferMemory);
DEFINE_STAT(STAT_DrawIndirectBufferMemory);
DEFINE_STAT(STAT_MiscBufferMemory);

DEFINE_STAT(STAT_SamplerDescriptorsAllocated);
DEFINE_STAT(STAT_ResourceDescriptorsAllocated);

DEFINE_STAT(STAT_BindlessSamplerHeapMemory);
DEFINE_STAT(STAT_BindlessResourceHeapMemory);
DEFINE_STAT(STAT_BindlessSamplerDescriptorsAllocated);
DEFINE_STAT(STAT_BindlessResourceDescriptorsAllocated);
