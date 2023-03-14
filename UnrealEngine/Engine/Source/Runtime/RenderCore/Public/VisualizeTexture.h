// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Containers/UnrealString.h"
#include "HAL/Platform.h"
#include "HAL/PlatformCrt.h"
#include "Misc/EnumClassFlags.h"
#include "Misc/Optional.h"
#include "Misc/WildcardString.h"
#include "RHIDefinitions.h"
#include "RenderGraph.h"
#include "RenderGraphDefinitions.h"
#include "RenderGraphResources.h"
#include "RenderResource.h"
#include "RendererInterface.h"
#include "Templates/RefCounting.h"

class FOutputDevice;
class FRDGBuilder;
class FRHICommandListImmediate;
class FWildcardString;

#define SUPPORTS_VISUALIZE_TEXTURE (WITH_ENGINE && (!(UE_BUILD_SHIPPING || UE_BUILD_TEST) || WITH_EDITOR))

class RENDERCORE_API FVisualizeTexture : public FRenderResource
{
public:
	FVisualizeTexture() = default;

	void ParseCommands(const TCHAR* Cmd, FOutputDevice &Ar);

	void DebugLogOnCrash();

	void GetTextureInfos_GameThread(TArray<FString>& Infos) const;

	/** Creates a new checkpoint (e.g. "SceneDepth@N") for the pooled render target. A null parameter is a no-op. */
#if SUPPORTS_VISUALIZE_TEXTURE
	void SetCheckPoint(FRDGBuilder& GraphBuilder, IPooledRenderTarget* PooledRenderTarget);
	void SetCheckPoint(FRHICommandListImmediate& RHICmdList, IPooledRenderTarget* PooledRenderTarget);
#else
	inline void SetCheckPoint(FRDGBuilder& GraphBuilder, IPooledRenderTarget* PooledRenderTarget) {}
	inline void SetCheckPoint(FRHICommandListImmediate& RHICmdList, IPooledRenderTarget* PooledRenderTarget) {}
#endif

private:
	enum class EFlags
	{
		None				= 0,
		SaveBitmap			= 1 << 0,
		SaveBitmapAsStencil = 1 << 1, // stencil normally displays in the alpha channel of depth buffer visualization. This option is just for BMP writeout to get a stencil only BMP.
	};
	FRIEND_ENUM_CLASS_FLAGS(EFlags);

	enum class ECommand
	{
		Unknown,
		DisableVisualization,
		VisualizeResource,
		DisplayHelp,
		DisplayPoolResourceList,
		DisplayResourceList,
	};

	enum class EInputUVMapping
	{
		LeftTop,
		Whole,
		PixelPerfectCenter,
		PictureInPicture
	};

	enum class EInputValueMapping
	{
		Color,
		Depth,
		Shadow
	};

	enum class EDisplayMode
	{
		MultiColomn,
		Detailed,
	};

	enum class ESortBy
	{
		Index,
		Name,
		Size
	};

	enum class EShaderOp
	{
		Frac,
		Saturate
	};

#if SUPPORTS_VISUALIZE_TEXTURE
	static void DisplayHelp(FOutputDevice &Ar);
	void DisplayPoolResourceListToLog(ESortBy SortBy);
	void DisplayResourceListToLog(const TOptional<FWildcardString>& Wildcard);

	/** Determine whether a texture should be captured for debugging purposes and return the capture id if needed. */
	TOptional<uint32> ShouldCapture(const TCHAR* DebugName, uint32 MipIndex);

	/** Create a pass capturing a texture. */
	void CreateContentCapturePass(FRDGBuilder& GraphBuilder, FRDGTextureRef Texture, uint32 CaptureId);

	void ReleaseDynamicRHI() override;

	void Visualize(const FString& InName, TOptional<uint32> InVersion = {});

	uint32 GetVersionCount(const TCHAR* InName) const;

	struct FConfig
	{
		float RGBMul = 1.0f;
		float AMul = 0.0f;

		// -1=off, 0=R, 1=G, 2=B, 3=A
		int32 SingleChannel = -1;
		float SingleChannelMul = 0.0f;

		EFlags Flags = EFlags::None;
		EInputUVMapping InputUVMapping = EInputUVMapping::PictureInPicture;
		EShaderOp ShaderOp = EShaderOp::Frac;
		uint32 MipIndex = 0;
		uint32 ArrayIndex = 0;
	} Config;

	struct FRequested
	{
		FString Name;
		TOptional<uint32> Version;
	} Requested;

	struct FCaptured
	{
		FCaptured()
		{
			Desc.DebugName = TEXT("VisualizeTexture");
		}

		TRefCountPtr<IPooledRenderTarget> PooledRenderTarget;
		FRDGTextureRef Texture = nullptr;
		FPooledRenderTargetDesc Desc;
		EInputValueMapping InputValueMapping = EInputValueMapping::Color;
	} Captured;

	ERHIFeatureLevel::Type FeatureLevel = ERHIFeatureLevel::SM5;

	// Maps a texture name to its checkpoint version.
	TMap<FString, uint32> VersionCountMap;
#endif

	friend class FRDGBuilder;
	friend class FVisualizeTexturePresent;
};

ENUM_CLASS_FLAGS(FVisualizeTexture::EFlags);

/** The global render targets for easy shading. */
extern RENDERCORE_API TGlobalResource<FVisualizeTexture> GVisualizeTexture;
