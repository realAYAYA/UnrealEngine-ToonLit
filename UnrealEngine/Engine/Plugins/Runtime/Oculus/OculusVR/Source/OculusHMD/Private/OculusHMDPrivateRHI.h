// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if OCULUS_HMD_SUPPORTED_PLATFORMS

//-------------------------------------------------------------------------------------------------
// D3D11
//-------------------------------------------------------------------------------------------------

#if OCULUS_HMD_SUPPORTED_PLATFORMS_D3D11
#include "ID3D11DynamicRHI.h"
#endif // OCULUS_HMD_SUPPORTED_PLATFORMS_D3D11


//-------------------------------------------------------------------------------------------------
// D3D12
//-------------------------------------------------------------------------------------------------

#if OCULUS_HMD_SUPPORTED_PLATFORMS_D3D12
#include "ID3D12DynamicRHI.h"
#endif // OCULUS_HMD_SUPPORTED_PLATFORMS_D3D12


//-------------------------------------------------------------------------------------------------
// OpenGL
//-------------------------------------------------------------------------------------------------

#if OCULUS_HMD_SUPPORTED_PLATFORMS_OPENGL
#include "IOpenGLDynamicRHI.h"
#endif // OCULUS_HMD_SUPPORTED_PLATFORMS_OPENGL


//-------------------------------------------------------------------------------------------------
// Vulkan
//-------------------------------------------------------------------------------------------------

#if OCULUS_HMD_SUPPORTED_PLATFORMS_VULKAN
#include "IVulkanDynamicRHI.h"
#endif // OCULUS_HMD_SUPPORTED_PLATFORMS_VULKAN

#endif // OCULUS_HMD_SUPPORTED_PLATFORMS