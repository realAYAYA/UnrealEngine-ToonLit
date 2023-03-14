// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Logging/LogMacros.h"
#include "RHI.h"
#include "RHIResources.h"
#include "RHIContext.h"

RHICORE_API DECLARE_LOG_CATEGORY_EXTERN(LogRHICore, Log, VeryVerbose);

namespace UE::RHICore
{

struct FResolveTextureInfo
{
	FResolveTextureInfo() = default;
	
	FResolveTextureInfo(
		FRHITexture* InSourceTexture,
		FRHITexture* InDestTexture,
		uint8 InMipLevel,
		int32 InArraySlice,
		FResolveRect InResolveRect)
		: SourceTexture(InSourceTexture)
		, DestTexture(InDestTexture)
		, MipLevel(InMipLevel)
		, ArraySlice(InArraySlice)
	{}

	FRHITexture* SourceTexture = nullptr;
	FRHITexture* DestTexture = nullptr;
	uint8 MipLevel = 0;
	int32 ArraySlice = -1;
	FResolveRect ResolveRect;
};

RHICORE_API void ResolveRenderPassTargets(const FRHIRenderPassInfo& Info, TFunction<void(FResolveTextureInfo)> Function);

} //! UE::RHICore