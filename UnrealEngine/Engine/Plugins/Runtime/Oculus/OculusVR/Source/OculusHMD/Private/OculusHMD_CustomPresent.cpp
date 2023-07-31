// Copyright Epic Games, Inc. All Rights Reserved.

#include "OculusHMD_CustomPresent.h"

#if OCULUS_HMD_SUPPORTED_PLATFORMS
#include "OculusHMD.h"
#include "ScreenRendering.h"
#include "PipelineStateCache.h"
#include "ClearQuad.h"
#include "OculusShaders.h"
#include "CommonRenderResources.h"

#if PLATFORM_ANDROID
#include "Android/AndroidJNI.h"
#include "Android/AndroidApplication.h"
#include "Android/AndroidPlatformMisc.h"
#endif

#define VULKAN_CUBEMAP_POSITIVE_Y 2
#define VULKAN_CUBEMAP_NEGATIVE_Y 3

namespace OculusHMD
{

//-------------------------------------------------------------------------------------------------
// FCustomPresent
//-------------------------------------------------------------------------------------------------

FCustomPresent::FCustomPresent(class FOculusHMD* InOculusHMD, ovrpRenderAPIType InRenderAPI, EPixelFormat InDefaultPixelFormat, bool bInSupportsSRGB)
	: OculusHMD(InOculusHMD)
	, RenderAPI(InRenderAPI)
	, DefaultPixelFormat(InDefaultPixelFormat)
	, bSupportsSRGB(bInSupportsSRGB)
    , bSupportsSubsampled(false)
	, bIsStandaloneStereoDevice(false)
{
	CheckInGameThread();

	DefaultOvrpTextureFormat = GetOvrpTextureFormat(GetDefaultPixelFormat());
	DefaultDepthOvrpTextureFormat = ovrpTextureFormat_None;

#if PLATFORM_ANDROID
	bIsStandaloneStereoDevice = FAndroidMisc::GetDeviceMake() == FString("Oculus");
#endif

	// grab a pointer to the renderer module for displaying our mirror window
	static const FName RendererModuleName("Renderer");
	RendererModule = FModuleManager::GetModulePtr<IRendererModule>(RendererModuleName);
}


void FCustomPresent::ReleaseResources_RHIThread()
{
	CheckInRHIThread();

	if (MirrorTextureRHI.IsValid())
	{
		FOculusHMDModule::GetPluginWrapper().DestroyMirrorTexture2();
		MirrorTextureRHI = nullptr;
	}
}


void FCustomPresent::Shutdown()
{
	CheckInGameThread();

	// OculusHMD is going away, but this object can live on until viewport is destroyed
	ExecuteOnRenderThread([this]()
	{
		ExecuteOnRHIThread([this]()
		{
			OculusHMD = nullptr;
		});
	});
}


bool FCustomPresent::NeedsNativePresent()
{
	CheckInRenderThread();

	bool bNeedsNativePresent = true;

	if (OculusHMD)
	{
		FGameFrame* Frame_RenderThread = OculusHMD->GetFrame_RenderThread();

		if (Frame_RenderThread)
		{
			bNeedsNativePresent = Frame_RenderThread->Flags.bSpectatorScreenActive;
		}
	}

	if (bIsStandaloneStereoDevice)
	{
		bNeedsNativePresent = false;
	}

	return bNeedsNativePresent;
}


bool FCustomPresent::Present(int32& SyncInterval)
{
	CheckInRHIThread();

	bool bNeedsNativePresent = true;

	if (OculusHMD)
	{
		FGameFrame* Frame_RHIThread = OculusHMD->GetFrame_RHIThread();

		if (Frame_RHIThread)
		{
			bNeedsNativePresent = Frame_RHIThread->Flags.bSpectatorScreenActive;
			FinishRendering_RHIThread();
		}
	}

	if (bIsStandaloneStereoDevice)
	{
		bNeedsNativePresent = false;
	}

	if (bNeedsNativePresent)
	{
		SyncInterval = 0; // VSync off
	}

	return bNeedsNativePresent;
}


void FCustomPresent::UpdateMirrorTexture_RenderThread()
{
	SCOPE_CYCLE_COUNTER(STAT_BeginRendering);

	CheckInRenderThread();

	const ESpectatorScreenMode MirrorWindowMode = OculusHMD->GetSpectatorScreenMode_RenderThread();
	const FVector2D MirrorWindowSize = OculusHMD->GetFrame_RenderThread()->WindowSize;

	if (FOculusHMDModule::GetPluginWrapper().GetInitialized())
	{
		// Need to destroy mirror texture?
		if (MirrorTextureRHI.IsValid() && (MirrorWindowMode != ESpectatorScreenMode::Distorted ||
			MirrorWindowSize != FVector2D(MirrorTextureRHI->GetSizeX(), MirrorTextureRHI->GetSizeY())))
		{
			ExecuteOnRHIThread([]()
			{
				FOculusHMDModule::GetPluginWrapper().DestroyMirrorTexture2();
			});

			MirrorTextureRHI = nullptr;
		}

		// Need to create mirror texture?
		if (!MirrorTextureRHI.IsValid() &&
			MirrorWindowMode == ESpectatorScreenMode::Distorted &&
			MirrorWindowSize.X != 0 && MirrorWindowSize.Y != 0)
		{
			int Width = (int)MirrorWindowSize.X;
			int Height = (int)MirrorWindowSize.Y;
			ovrpTextureHandle TextureHandle;

			ExecuteOnRHIThread([&]()
			{
				FOculusHMDModule::GetPluginWrapper().SetupMirrorTexture2(GetOvrpDevice(), Height, Width, GetDefaultOvrpTextureFormat(), &TextureHandle);
			});

			UE_LOG(LogHMD, Log, TEXT("Allocated a new mirror texture (size %d x %d)"), Width, Height);

			ETextureCreateFlags TexCreateFlags = TexCreate_ShaderResource | TexCreate_RenderTargetable;

			MirrorTextureRHI = CreateTexture_RenderThread(Width, Height, GetDefaultPixelFormat(), FClearValueBinding::None, 1, 1, 1, RRT_Texture2D, TextureHandle, TexCreateFlags)->GetTexture2D();
		}
	}
}


void FCustomPresent::FinishRendering_RHIThread()
{
	SCOPE_CYCLE_COUNTER(STAT_FinishRendering);
	CheckInRHIThread();

#if STATS
	if (OculusHMD->GetFrame_RHIThread()->ShowFlags.Rendering)
	{
		ovrpAppLatencyTimings AppLatencyTimings;
		if(OVRP_SUCCESS(FOculusHMDModule::GetPluginWrapper().GetAppLatencyTimings2(&AppLatencyTimings)))
		{
			SET_FLOAT_STAT(STAT_LatencyRender, AppLatencyTimings.LatencyRender * 1000.0f);
			SET_FLOAT_STAT(STAT_LatencyTimewarp, AppLatencyTimings.LatencyTimewarp * 1000.0f);
			SET_FLOAT_STAT(STAT_LatencyPostPresent, AppLatencyTimings.LatencyPostPresent * 1000.0f);
			SET_FLOAT_STAT(STAT_ErrorRender, AppLatencyTimings.ErrorRender * 1000.0f);
			SET_FLOAT_STAT(STAT_ErrorTimewarp, AppLatencyTimings.ErrorTimewarp * 1000.0f);
		}
	}
#endif

	OculusHMD->FinishRHIFrame_RHIThread();

#if PLATFORM_ANDROID
	float GPUFrameTime = 0.0f;
	if (OVRP_SUCCESS(FOculusHMDModule::GetPluginWrapper().GetGPUFrameTime(&GPUFrameTime)))
	{
		SubmitGPUFrameTime(GPUFrameTime);
	}
#endif
}


EPixelFormat FCustomPresent::GetPixelFormat(EPixelFormat Format) const
{
	switch (Format)
	{
//	case PF_B8G8R8A8:
	case PF_FloatRGBA:
	case PF_FloatR11G11B10:
//	case PF_R8G8B8A8:
		return Format;
	}

	return GetDefaultPixelFormat();
}


EPixelFormat FCustomPresent::GetPixelFormat(ovrpTextureFormat Format) const
{
	switch(Format)
	{
//		case ovrpTextureFormat_R8G8B8A8_sRGB:
//		case ovrpTextureFormat_R8G8B8A8:
//			return PF_R8G8B8A8;
		case ovrpTextureFormat_R16G16B16A16_FP:
			return PF_FloatRGBA;
		case ovrpTextureFormat_R11G11B10_FP:
			return PF_FloatR11G11B10;
//		case ovrpTextureFormat_B8G8R8A8_sRGB:
//		case ovrpTextureFormat_B8G8R8A8:
//			return PF_B8G8R8A8;
	}

	return GetDefaultPixelFormat();
}


ovrpTextureFormat FCustomPresent::GetOvrpTextureFormat(EPixelFormat Format, bool usesRGB) const
{
	switch (GetPixelFormat(Format))
	{
	case PF_B8G8R8A8:
		return bSupportsSRGB && usesRGB ? ovrpTextureFormat_B8G8R8A8_sRGB : ovrpTextureFormat_B8G8R8A8;
	case PF_FloatRGBA:
		return ovrpTextureFormat_R16G16B16A16_FP;
	case PF_FloatR11G11B10:
		return ovrpTextureFormat_R11G11B10_FP;
	case PF_R8G8B8A8:
		return bSupportsSRGB && usesRGB ? ovrpTextureFormat_R8G8B8A8_sRGB : ovrpTextureFormat_R8G8B8A8;
	}

	return ovrpTextureFormat_None;
}


bool FCustomPresent::IsSRGB(ovrpTextureFormat InFormat)
{
	switch (InFormat)
	{
	case ovrpTextureFormat_B8G8R8A8_sRGB:
	case ovrpTextureFormat_R8G8B8A8_sRGB:
		return true;
	}

	return false;
}


int FCustomPresent::GetSystemRecommendedMSAALevel() const
{
	int SystemRecommendedMSAALevel = 1;
	FOculusHMDModule::GetPluginWrapper().GetSystemRecommendedMSAALevel2(&SystemRecommendedMSAALevel);
	return SystemRecommendedMSAALevel;
}


FXRSwapChainPtr FCustomPresent::CreateSwapChain_RenderThread(uint32 InSizeX, uint32 InSizeY, EPixelFormat InFormat, FClearValueBinding InBinding, uint32 InNumMips, uint32 InNumSamples, uint32 InNumSamplesTileMem, ERHIResourceType InResourceType, const TArray<ovrpTextureHandle>& InTextures, ETextureCreateFlags InTexCreateFlags, const TCHAR* DebugName)
{
	CheckInRenderThread();

	FTextureRHIRef RHITexture;
	TArray<FTextureRHIRef> RHITextureSwapChain;
	{
		for (int32 TextureIndex = 0; TextureIndex < InTextures.Num(); ++TextureIndex)
		{
			FTextureRHIRef TexRef = CreateTexture_RenderThread(InSizeX, InSizeY, InFormat, InBinding, InNumMips, InNumSamples, InNumSamplesTileMem, InResourceType, InTextures[TextureIndex], InTexCreateFlags);

			FString TexName = FString::Printf(TEXT("%s (%d/%d)"), DebugName, TextureIndex, InTextures.Num());
			TexRef->SetName(*TexName);
			RHIBindDebugLabelName(TexRef, *TexName);
			
			RHITextureSwapChain.Add(TexRef);
		}
	}

	RHITexture = GDynamicRHI->RHICreateAliasedTexture(RHITextureSwapChain[0]);

	return CreateXRSwapChain(MoveTemp(RHITextureSwapChain), RHITexture);
}


void FCustomPresent::CopyTexture_RenderThread(FRHICommandListImmediate& RHICmdList, FRHITexture* DstTexture, FRHITexture* SrcTexture,
	FIntRect DstRect, FIntRect SrcRect, bool bAlphaPremultiply, bool bNoAlphaWrite, bool bInvertY, bool sRGBSource) const
{
	CheckInRenderThread();

	FRHITexture2D* DstTexture2D = DstTexture->GetTexture2D();
	FRHITextureCube* DstTextureCube = DstTexture->GetTextureCube();
	FRHITexture2D* SrcTexture2D = SrcTexture->GetTexture2DArray() ? SrcTexture->GetTexture2DArray() : SrcTexture->GetTexture2D();
	FRHITextureCube* SrcTextureCube = SrcTexture->GetTextureCube();

	FIntPoint DstSize;
	FIntPoint SrcSize;

	if (DstTexture2D && SrcTexture2D)
	{
		DstSize = FIntPoint(DstTexture2D->GetSizeX(), DstTexture2D->GetSizeY());
		SrcSize = FIntPoint(SrcTexture2D->GetSizeX(), SrcTexture2D->GetSizeY());
	}
	else if(DstTextureCube && SrcTextureCube)
	{
		DstSize = FIntPoint(DstTextureCube->GetSize(), DstTextureCube->GetSize());
		SrcSize = FIntPoint(SrcTextureCube->GetSize(), SrcTextureCube->GetSize());
	}
	else
	{
		return;
	}

	if (DstRect.IsEmpty())
	{
		DstRect = FIntRect(FIntPoint::ZeroValue, DstSize);
	}

	if (SrcRect.IsEmpty())
	{
		SrcRect = FIntRect(FIntPoint::ZeroValue, SrcSize);
	}

	const uint32 ViewportWidth = DstRect.Width();
	const uint32 ViewportHeight = DstRect.Height();
	const FIntPoint TargetSize(ViewportWidth, ViewportHeight);
	float U = SrcRect.Min.X / (float) SrcSize.X;
	float V = SrcRect.Min.Y / (float) SrcSize.Y;
	float USize = SrcRect.Width() / (float) SrcSize.X;
	float VSize = SrcRect.Height() / (float) SrcSize.Y;

#if PLATFORM_ANDROID // on android, top-left isn't 0/0 but 1/0.
	if (bInvertY)
	{
		V = 1.0f - V;
		VSize = -VSize;
	}
#endif

	FRHITexture* SrcTextureRHI = SrcTexture;
	RHICmdList.Transition(FRHITransitionInfo(SrcTextureRHI, ERHIAccess::Unknown, ERHIAccess::SRVGraphics));
	FGraphicsPipelineStateInitializer GraphicsPSOInit;

	if (bAlphaPremultiply)
	{
		if (bNoAlphaWrite)
		{
			// for quads, write RGB, RGB = src.rgb * 1 + dst.rgb * 0
			GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGB, BO_Add, BF_One, BF_Zero, BO_Add, BF_One, BF_Zero>::GetRHI();
		}
		else
		{
			// for quads, write RGBA, RGB = src.rgb * src.a + dst.rgb * 0, A = src.a + dst.a * 0
			GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGBA, BO_Add, BF_SourceAlpha, BF_Zero, BO_Add, BF_One, BF_Zero>::GetRHI();
		}
	}
	else
	{
		if (bNoAlphaWrite)
		{
			GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGB>::GetRHI();
		}
		else
		{
			// for mirror window, write RGBA, RGB = src.rgb * src.a + dst.rgb * (1 - src.a), A = src.a * 1 + dst.a * (1 - src a)
			GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGBA, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_One, BF_InverseSourceAlpha>::GetRHI();
		}
	}

	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
	GraphicsPSOInit.PrimitiveType = PT_TriangleList;

	const auto FeatureLevel = GMaxRHIFeatureLevel;
	auto ShaderMap = GetGlobalShaderMap(FeatureLevel);
	TShaderMapRef<FScreenVS> VertexShader(ShaderMap);
	GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();

	if (DstTexture2D)
	{
		sRGBSource &= EnumHasAnyFlags(SrcTexture->GetFlags(), TexCreate_SRGB);

		// Need to copy over mip maps on Android since they are not generated like they are on PC
#if PLATFORM_ANDROID
		uint32 NumMips = SrcTexture->GetNumMips();
#else
		uint32 NumMips = 1;
#endif

		for (uint32 MipIndex = 0; MipIndex < NumMips; MipIndex++)
		{
			FRHIRenderPassInfo RPInfo(DstTexture, ERenderTargetActions::Load_Store);
			RPInfo.ColorRenderTargets[0].MipIndex = MipIndex;

			RHICmdList.BeginRenderPass(RPInfo, TEXT("CopyTexture"));
			{
				const uint32 MipViewportWidth = ViewportWidth >> MipIndex;
				const uint32 MipViewportHeight = ViewportHeight >> MipIndex;
				const FIntPoint MipTargetSize(MipViewportWidth, MipViewportHeight);

				if (bNoAlphaWrite)
				{
					RHICmdList.SetViewport(DstRect.Min.X, DstRect.Min.Y, 0.0f, DstRect.Max.X, DstRect.Max.Y, 1.0f);
					DrawClearQuad(RHICmdList, bAlphaPremultiply ? FLinearColor::Black : FLinearColor::White);
				}

				RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
				FRHISamplerState* SamplerState = DstRect.Size() == SrcRect.Size() ? TStaticSamplerState<SF_Point>::GetRHI() : TStaticSamplerState<SF_Bilinear>::GetRHI();

				if (!sRGBSource)
				{
					TShaderMapRef<FScreenPSMipLevel> PixelShader(ShaderMap);
					GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
					SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);
					PixelShader->SetParameters(RHICmdList, SamplerState, SrcTextureRHI, MipIndex);
				}
				else
				{
					TShaderMapRef<FScreenPSsRGBSourceMipLevel> PixelShader(ShaderMap);
					GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
					SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);
					PixelShader->SetParameters(RHICmdList, SamplerState, SrcTextureRHI, MipIndex);
				}

				RHICmdList.SetViewport(DstRect.Min.X, DstRect.Min.Y, 0.0f, DstRect.Min.X + MipViewportWidth, DstRect.Min.Y + MipViewportHeight, 1.0f);

				RendererModule->DrawRectangle(
					RHICmdList,
					0, 0, MipViewportWidth, MipViewportHeight,
					U, V, USize, VSize,
					MipTargetSize,
					FIntPoint(1, 1),
					VertexShader,
					EDRF_Default);
			}
			RHICmdList.EndRenderPass();
		}
	}
	else
	{
		for (int FaceIndex = 0; FaceIndex < 6; FaceIndex++)
		{
			FRHIRenderPassInfo RPInfo(DstTexture, ERenderTargetActions::Load_Store);

			// On Vulkan the positive and negative Y faces of the cubemap need to be flipped
			if (RenderAPI == ovrpRenderAPI_Vulkan)
			{
				int NewFaceIndex = 0;

				if (FaceIndex == VULKAN_CUBEMAP_POSITIVE_Y)
					NewFaceIndex = VULKAN_CUBEMAP_NEGATIVE_Y;
				else if (FaceIndex == VULKAN_CUBEMAP_NEGATIVE_Y)
					NewFaceIndex = VULKAN_CUBEMAP_POSITIVE_Y;
				else
					NewFaceIndex = FaceIndex;

				RPInfo.ColorRenderTargets[0].ArraySlice = NewFaceIndex;
			}
			else
			{
				RPInfo.ColorRenderTargets[0].ArraySlice = FaceIndex;
			}

			RHICmdList.BeginRenderPass(RPInfo, TEXT("CopyTextureFace"));
			{
				if (bNoAlphaWrite)
				{
					DrawClearQuad(RHICmdList, bAlphaPremultiply ? FLinearColor::Black : FLinearColor::White);
				}

				RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

				TShaderMapRef<FOculusCubemapPS> PixelShader(ShaderMap);
				GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
				SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);
				FRHISamplerState* SamplerState = DstRect.Size() == SrcRect.Size() ? TStaticSamplerState<SF_Point>::GetRHI() : TStaticSamplerState<SF_Bilinear>::GetRHI();
				PixelShader->SetParameters(RHICmdList, SamplerState, SrcTextureRHI, FaceIndex);

				RHICmdList.SetViewport(DstRect.Min.X, DstRect.Min.Y, 0.0f, DstRect.Max.X, DstRect.Max.Y, 1.0f);

				RendererModule->DrawRectangle(
					RHICmdList,
					0, 0, ViewportWidth, ViewportHeight,
#if PLATFORM_ANDROID
					U, V, USize, VSize,
#else
					U, 1.0 - V, USize, -VSize,
#endif
					TargetSize,
					FIntPoint(1, 1),
					VertexShader,
					EDRF_Default);
			}
			RHICmdList.EndRenderPass();
		}
	}
}

void FCustomPresent::SubmitGPUCommands_RenderThread(FRHICommandListImmediate& RHICmdList)
{
	CheckInRenderThread();

	RHICmdList.SubmitCommandsHint();
}

} // namespace OculusHMD

#endif //OCULUS_HMD_SUPPORTED_PLATFORMS
