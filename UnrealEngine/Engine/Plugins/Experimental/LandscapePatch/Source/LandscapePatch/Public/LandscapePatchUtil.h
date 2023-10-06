// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"

class FRHICommandListImmediate;
class FTextureResource;

namespace UE {
namespace Landscape {
namespace PatchUtil {

	void LANDSCAPEPATCH_API CopyTextureOnRenderThread(FRHICommandListImmediate& RHICmdList, const FTextureResource& Source, FTextureResource& Destination);

}}}//end UE::Landscape::PatchUtil

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif
