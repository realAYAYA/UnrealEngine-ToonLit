// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RenderResource.h"

namespace Nanite
{

struct FResources;
class FVertexFactoryResource;

extern ENGINE_API TGlobalResource<FVertexFactoryResource> GVertexFactoryResource;

enum class ERayTracingMode : uint8
{
	Fallback = 0u,
	StreamOut = 1u,
};

ENGINE_API ERayTracingMode GetRayTracingMode();

ENGINE_API bool GetSupportsCustomDepthRendering();

}