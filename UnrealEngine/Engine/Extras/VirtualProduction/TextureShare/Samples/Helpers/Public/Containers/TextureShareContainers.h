// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Containers/TextureShareSDKContainers.h"
#include "Containers/TextureShareObjectDesc.h"

// Support D3D11
#include "Containers/D3D11/TextureShareDeviceContextD3D11.h"
#include "Containers/D3D11/TextureShareResourceD3D11.h"
#include "Containers/D3D11/TextureShareImageD3D11.h"

// Support D3D12
#include "Containers/D3D12/TextureShareDeviceContextD3D12.h"
#include "Containers/D3D12/TextureShareResourceD3D12.h"
#include "Containers/D3D12/TextureShareImageD3D12.h"

// Support Vulkan (Currently not implemented)
#include "Containers/Vulkan/TextureShareDeviceContextVulkan.h"
#include "Containers/Vulkan/TextureShareResourceVulkan.h"
#include "Containers/Vulkan/TextureShareImageVulkan.h"

// Resource helpers
#include "Containers/Resource/TextureShareResourceRequest.h"
#include "Containers/Resource/TextureShareResourceDesc.h"
#include "Containers/Resource/TextureShareViewportResourceDesc.h"

#include "Containers/TextureShareObjectDesc.h"
#include "Containers/TextureShareEnums.h"
