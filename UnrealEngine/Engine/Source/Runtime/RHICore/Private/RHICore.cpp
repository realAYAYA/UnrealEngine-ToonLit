// Copyright Epic Games, Inc. All Rights Reserved.

#include "RHICore.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogRHICore);
IMPLEMENT_MODULE(FDefaultModuleImpl, RHICore);

namespace UE
{
namespace RHICore
{

void ResolveRenderPassTargets(const FRHIRenderPassInfo& RenderPassInfo, TFunction<void(FResolveTextureInfo)> ResolveFunction)
{
	const auto ResolveTexture = [&](FResolveTextureInfo ResolveInfo)
	{
		if (!ResolveInfo.SourceTexture || !ResolveInfo.DestTexture || ResolveInfo.SourceTexture == ResolveInfo.DestTexture)
		{
			return;
		}

		const FRHITextureDesc& SourceDesc = ResolveInfo.SourceTexture->GetDesc();
		const FRHITextureDesc& DestDesc   = ResolveInfo.DestTexture->GetDesc();

		check(SourceDesc.Format == DestDesc.Format);
		check(SourceDesc.Extent == DestDesc.Extent);
		check(SourceDesc.IsMultisample() && !DestDesc.IsMultisample());
		check(SourceDesc.Format != PF_DepthStencil || (SourceDesc.IsTexture2D() && DestDesc.IsTexture2D()));
		check(!SourceDesc.IsTexture3D() && !DestDesc.IsTexture3D());

		ResolveFunction(ResolveInfo);
	};

	for (int32 Index = 0; Index < MaxSimultaneousRenderTargets; ++Index)
	{
		const auto& RTV = RenderPassInfo.ColorRenderTargets[Index];
		ResolveTexture({ RTV.RenderTarget, RTV.ResolveTarget, RTV.MipIndex, RTV.ArraySlice, RenderPassInfo.ResolveRect });
	}

	const auto& DSV = RenderPassInfo.DepthStencilRenderTarget;
	ResolveTexture({ DSV.DepthStencilTarget, DSV.ResolveTarget, 0, 0, RenderPassInfo.ResolveRect });
}

} //! RHICore
} //! UE