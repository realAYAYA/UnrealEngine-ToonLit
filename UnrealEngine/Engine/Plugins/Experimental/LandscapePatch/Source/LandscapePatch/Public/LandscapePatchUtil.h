// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FRHICommandListImmediate;
class FTextureResource;

namespace UE {
namespace Landscape {
namespace PatchUtil {

	void LANDSCAPEPATCH_API CopyTextureOnRenderThread(FRHICommandListImmediate& RHICmdList, const FTextureResource& Source, FTextureResource& Destination);

}}}//end UE::Landscape::PatchUtil