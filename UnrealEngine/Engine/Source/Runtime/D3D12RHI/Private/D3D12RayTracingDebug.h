// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "D3D12RHIPrivate.h"

#define D3D12_RHI_SUPPORT_RAYTRACING_SCENE_DEBUGGING (!UE_BUILD_SHIPPING && D3D12_RHI_RAYTRACING)

#if D3D12_RHI_SUPPORT_RAYTRACING_SCENE_DEBUGGING

void D3D12RayTracingSceneDebugUpdate(const FD3D12RayTracingScene& Scene, FD3D12Buffer* InstanceBuffer, uint32 InstanceBufferOffset, FD3D12CommandContext& CommandContext);

#endif // D3D12_RHI_SUPPORT_RAYTRACING_SCENE_DEBUGGING
