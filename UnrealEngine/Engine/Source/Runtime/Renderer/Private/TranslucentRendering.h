// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HitProxies.h"
#include "ShaderParameters.h"
#include "GlobalShader.h"
#include "SceneRendering.h"
#include "VolumeRendering.h"
#include "ScreenPass.h"
#include "SystemTextures.h"
#include "RenderGraphDefinitions.h"

struct FSeparateTranslucencyDimensions
{
	inline FScreenPassTextureViewport GetViewport(FIntRect ViewRect) const
	{
		return FScreenPassTextureViewport(Extent, GetScaledRect(ViewRect, Scale));
	}

	FScreenPassTextureViewport GetInstancedStereoViewport(const FViewInfo& View) const;

	// Extent of the separate translucency targets, if downsampled.
	FIntPoint Extent = FIntPoint::ZeroValue;

	// Amount the view rects should be scaled to match the new separate translucency extent.
	float Scale = 1.0f;

	// The number of MSAA samples to use when creating separate translucency textures.
	uint32 NumSamples = 1;
};

/** Resources of a translucency pass. */
struct FTranslucencyPassResources
{
	ETranslucencyPass::Type Pass = ETranslucencyPass::TPT_MAX;
	FIntRect ViewRect = FIntRect(0, 0, 0, 0);
	FRDGTextureMSAA ColorTexture;
	FRDGTextureMSAA ColorModulateTexture;
	FRDGTextureMSAA DepthTexture;

	inline bool IsValid() const
	{
		return ViewRect.Width() > 0 && ViewRect.Height() > 0;
	}

	inline FRDGTextureRef GetColorForRead(FRDGBuilder& GraphBuilder) const
	{
		if (!ColorTexture.IsValid())
		{
			return GSystemTextures.GetBlackAlphaOneDummy(GraphBuilder);
		}
		return ColorTexture.Resolve;
	}

	inline FRDGTextureRef GetColorModulateForRead(FRDGBuilder& GraphBuilder) const
	{
		if (!ColorModulateTexture.IsValid())
		{
			return GSystemTextures.GetWhiteDummy(GraphBuilder);
		}
		return ColorModulateTexture.Resolve;
	}

	inline FRDGTextureRef GetDepthForRead(FRDGBuilder& GraphBuilder) const
	{
		if (!DepthTexture.IsValid())
		{
			return GSystemTextures.GetMaxFP16Depth(GraphBuilder);
		}
		return DepthTexture.Resolve;
	}

	inline FScreenPassTextureViewport GetTextureViewport() const
	{
		check(IsValid());
		return FScreenPassTextureViewport(ColorTexture.Target->Desc.Extent, ViewRect);
	}
};

/** All resources of all translucency passes for a view family. */
struct FTranslucencyPassResourcesMap
{
	FTranslucencyPassResourcesMap(int32 NumViews);

	inline FTranslucencyPassResources& Get(int32 ViewIndex, ETranslucencyPass::Type Translucency)
	{
		return Array[ViewIndex][int32(Translucency)];
	};

	inline const FTranslucencyPassResources& Get(int32 ViewIndex, ETranslucencyPass::Type Translucency) const
	{
		check(ViewIndex < Array.Num());
		return Array[ViewIndex][int32(Translucency)];
	};

private:
	TArray<TStaticArray<FTranslucencyPassResources, ETranslucencyPass::TPT_MAX>, TInlineAllocator<4>> Array;
};

/** All resources of all translucency for one view. */
struct FTranslucencyViewResourcesMap
{
	FTranslucencyViewResourcesMap() = default;

	FTranslucencyViewResourcesMap(const FTranslucencyPassResourcesMap& InTranslucencyPassResourcesMap, int32 InViewIndex)
		: TranslucencyPassResourcesMap(&InTranslucencyPassResourcesMap)
		, ViewIndex(InViewIndex)
	{ }

	bool IsValid() const
	{
		return TranslucencyPassResourcesMap != nullptr;
	}

	inline const FTranslucencyPassResources& Get(ETranslucencyPass::Type Translucency) const
	{
		check(IsValid());
		return TranslucencyPassResourcesMap->Get(ViewIndex, Translucency);
	};

private:
	const FTranslucencyPassResourcesMap* TranslucencyPassResourcesMap = nullptr;
	int32 ViewIndex = 0;
};

DECLARE_GPU_DRAWCALL_STAT_EXTERN(Translucency);

/** Converts the the translucency pass into the respective mesh pass. */
EMeshPass::Type TranslucencyPassToMeshPass(ETranslucencyPass::Type TranslucencyPass);

/** Returns the translucency views to render for the requested view. */
ETranslucencyView GetTranslucencyView(const FViewInfo& View);

/** Returns the union of all translucency views to render. */
ETranslucencyView GetTranslucencyViews(TArrayView<const FViewInfo> Views);

/** Computes the translucency dimensions. */
FSeparateTranslucencyDimensions UpdateSeparateTranslucencyDimensions(const FSceneRenderer& SceneRenderer);

/** Returns whether the view family is requesting to render translucency. */
bool ShouldRenderTranslucency(const FSceneViewFamily& ViewFamily);

/** Check if separate translucency pass is needed for given pass and downsample scale */
bool IsSeparateTranslucencyEnabled(ETranslucencyPass::Type TranslucencyPass, float DownsampleScale);

/** Shared function to get the post DOF texture pixel format and creation flags */
const FRDGTextureDesc GetPostDOFTranslucentTextureDesc(ETranslucencyPass::Type TranslucencyPass, FSeparateTranslucencyDimensions& SeparateTranslucencyDimensions, bool bIsModulate, EShaderPlatform ShaderPlatform);

/** Shared function used to create Post DOF translucent textures */
FRDGTextureMSAA CreatePostDOFTranslucentTexture(FRDGBuilder& GraphBuilder, ETranslucencyPass::Type TranslucencyPass, FSeparateTranslucencyDimensions& SeparateTranslucencyDimensions, bool bIsModulate, EShaderPlatform ShaderPlatform);


/** Add a pass to compose separate translucency. */
struct FTranslucencyComposition
{
	enum class EOperation
	{
		UpscaleOnly,
		ComposeToExistingSceneColor,
		ComposeToNewSceneColor,
	};

	EOperation Operation = EOperation::UpscaleOnly;
	bool bApplyModulateOnly = false;

	FScreenPassTexture SceneColor;
	FScreenPassTexture SceneDepth;

	FScreenPassTextureViewport OutputViewport;
	EPixelFormat OutputPixelFormat = PF_Unknown;

	FScreenPassTexture AddPass(
		FRDGBuilder& GraphBuilder,
		const FViewInfo& View,
		const FTranslucencyPassResources& TranslucencyTextures) const;
};
