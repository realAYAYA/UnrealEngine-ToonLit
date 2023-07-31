// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	D3D12RenderTarget.cpp: D3D render target implementation.
	=============================================================================*/

#include "D3D12RHIPrivate.h"
#include "BatchedElements.h"
#include "ScreenRendering.h"
#include "RHIStaticStates.h"
#include "ResolveShader.h"
#include "SceneUtils.h"
#include "PipelineStateCache.h"
#include "Math/PackedVector.h"
#include "RHISurfaceDataConversion.h"
#include "CommonRenderResources.h"

static uint32 ComputeBytesPerPixel(DXGI_FORMAT Format)
{
	uint32 BytesPerPixel = 0;

	switch (Format)
	{
	case DXGI_FORMAT_R8G8_TYPELESS:
	case DXGI_FORMAT_R8G8_UNORM:
	case DXGI_FORMAT_R8G8_UINT:
	case DXGI_FORMAT_R8G8_SNORM:
	case DXGI_FORMAT_R8G8_SINT:
	case DXGI_FORMAT_R16_TYPELESS:
	case DXGI_FORMAT_R16_FLOAT:
	case DXGI_FORMAT_D16_UNORM:
	case DXGI_FORMAT_R16_UNORM:
	case DXGI_FORMAT_R16_UINT:
	case DXGI_FORMAT_R16_SNORM:
	case DXGI_FORMAT_R16_SINT:
	case DXGI_FORMAT_B5G6R5_UNORM:
	case DXGI_FORMAT_B5G5R5A1_UNORM:
		BytesPerPixel = 2;
		break;
	case DXGI_FORMAT_B8G8R8A8_TYPELESS:
	case DXGI_FORMAT_B8G8R8A8_UNORM:
	case DXGI_FORMAT_R8G8B8A8_TYPELESS:
	case DXGI_FORMAT_R8G8B8A8_UNORM:
	case DXGI_FORMAT_R24G8_TYPELESS:
	case DXGI_FORMAT_R10G10B10A2_UNORM:
	case DXGI_FORMAT_R11G11B10_FLOAT:
	case DXGI_FORMAT_R16G16_UNORM:
	case DXGI_FORMAT_R32_UINT:
	case DXGI_FORMAT_R32_TYPELESS:
	case DXGI_FORMAT_R32_FLOAT:
	case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
	case DXGI_FORMAT_B8G8R8X8_TYPELESS:
	case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
	case DXGI_FORMAT_B8G8R8X8_UNORM:
	case DXGI_FORMAT_R10G10B10A2_TYPELESS:
	case DXGI_FORMAT_R10G10B10A2_UINT:
	case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
	case DXGI_FORMAT_R8G8B8A8_UINT:
	case DXGI_FORMAT_R8G8B8A8_SNORM:
	case DXGI_FORMAT_R8G8B8A8_SINT:
	case DXGI_FORMAT_R16G16_TYPELESS:
	case DXGI_FORMAT_R16G16_FLOAT:
	case DXGI_FORMAT_R16G16_UINT:
	case DXGI_FORMAT_R16G16_SNORM:
	case DXGI_FORMAT_R16G16_SINT:
	case DXGI_FORMAT_D32_FLOAT:
	case DXGI_FORMAT_R32_SINT:
	case DXGI_FORMAT_R9G9B9E5_SHAREDEXP:
		BytesPerPixel = 4;
		break;
	case DXGI_FORMAT_R16G16B16A16_FLOAT:
	case DXGI_FORMAT_R16G16B16A16_UNORM:
	case DXGI_FORMAT_R16G16B16A16_TYPELESS:
	case DXGI_FORMAT_R16G16B16A16_UINT:
	case DXGI_FORMAT_R16G16B16A16_SNORM:
	case DXGI_FORMAT_R16G16B16A16_SINT:
	case DXGI_FORMAT_R32G32_TYPELESS:
	case DXGI_FORMAT_R32G32_FLOAT:
	case DXGI_FORMAT_R32G32_UINT:
	case DXGI_FORMAT_R32G32_SINT:
		BytesPerPixel = 8;
		break;
	case DXGI_FORMAT_R32G8X24_TYPELESS:
	case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
	case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
		BytesPerPixel = 8;
		break;
	case DXGI_FORMAT_R8_TYPELESS:
	case DXGI_FORMAT_R8_UNORM:
	case DXGI_FORMAT_R8_UINT:
	case DXGI_FORMAT_R8_SNORM:
	case DXGI_FORMAT_R8_SINT:
	case DXGI_FORMAT_A8_UNORM:
	case DXGI_FORMAT_R1_UNORM:
		BytesPerPixel = 1;
		break;
	case DXGI_FORMAT_R32G32B32A32_UINT:
	case DXGI_FORMAT_R32G32B32A32_TYPELESS:
	case DXGI_FORMAT_R32G32B32A32_FLOAT:
	case DXGI_FORMAT_R32G32B32A32_SINT:
		BytesPerPixel = 16;
		break;
	}

	// format not supported yet
	check(BytesPerPixel);

	return BytesPerPixel;
}

static inline DXGI_FORMAT ConvertTypelessToUnorm(DXGI_FORMAT Format)
{
	// required to prevent 
	// D3D11: ERROR: ID3D11DeviceContext::ResolveSubresource: The Format (0x1b, R8G8B8A8_TYPELESS) is never able to resolve multisampled resources. [ RESOURCE_MANIPULATION ERROR #294: DEVICE_RESOLVESUBRESOURCE_FORMAT_INVALID ]
	// D3D11: **BREAK** enabled for the previous D3D11 message, which was: [ RESOURCE_MANIPULATION ERROR #294: DEVICE_RESOLVESUBRESOURCE_FORMAT_INVALID ]
	switch (Format)
	{
	case DXGI_FORMAT_R8G8B8A8_TYPELESS:
		return DXGI_FORMAT_R8G8B8A8_UNORM;

	case DXGI_FORMAT_B8G8R8A8_TYPELESS:
		return DXGI_FORMAT_B8G8R8A8_UNORM;

	default:
		return Format;
	}
}

static FResolveRect GetDefaultRect(const FResolveRect& Rect, uint32 DefaultWidth, uint32 DefaultHeight)
{
	if (Rect.X1 >= 0 && Rect.X2 >= 0 && Rect.Y1 >= 0 && Rect.Y2 >= 0)
	{
		return Rect;
	}
	else
	{
		return FResolveRect(0, 0, DefaultWidth, DefaultHeight);
	}
}

template<typename TPixelShader>
void FD3D12CommandContext::ResolveTextureUsingShader(
	FD3D12Texture* SourceTexture,
	FD3D12Texture* DestTexture,
	FD3D12RenderTargetView* DestTextureRTV,
	FD3D12DepthStencilView* DestTextureDSV,
	const D3D12_RESOURCE_DESC& ResolveTargetDesc,
	const FResolveRect& SourceRect,
	const FResolveRect& DestRect,
	typename TPixelShader::FParameter PixelShaderParameter
	)
{
	// Save the current viewports so they can be restored
	D3D12_VIEWPORT SavedViewports[D3D12_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
	uint32 NumSavedViewports = StateCache.GetNumViewports();
	StateCache.GetViewports(&NumSavedViewports, SavedViewports);

	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	// No alpha blending, no depth tests or writes, no stencil tests or writes, no backface culling.
	GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();

	// Make sure the destination is not bound as a shader resource.
	if (DestTexture)
	{
		ConditionalClearShaderResource(&DestTexture->ResourceLocation);
	}

	// Determine if the entire destination surface is being resolved to.
	// If the entire surface is being resolved to, then it means we can clear it and signal the driver that it can discard
	// the surface's previous contents, which breaks dependencies between frames when using alternate-frame SLI.
	const bool bClearDestTexture =
		DestRect.X1 == 0
		&& DestRect.Y1 == 0
		&& (uint64)DestRect.X2 == ResolveTargetDesc.Width
		&&	DestRect.Y2 == ResolveTargetDesc.Height;

	if (ResolveTargetDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)
	{
		// Clear the destination texture.
		if (bClearDestTexture)
		{
			if (IsDefaultContext())
			{
				GetParentDevice()->RegisterGPUWork(0);
			}

			TransitionResource(DestTextureDSV, D3D12_RESOURCE_STATE_DEPTH_WRITE);

			FlushResourceBarriers();

			GraphicsCommandList()->ClearDepthStencilView(DestTextureDSV->GetView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 0, 0, 0, nullptr);
			UpdateResidency(DestTextureDSV->GetResource());
		}

		// Write to the dest texture as a depth-stencil target.
		FD3D12RenderTargetView* NullRTV = nullptr;
		StateCache.SetRenderTargets(1, &NullRTV, DestTextureDSV);

		GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<true, CF_Always>::GetRHI();

		if (DestTexture)
		{
			GraphicsPSOInit.DepthStencilTargetFormat = DestTexture->GetFormat();
			GraphicsPSOInit.DepthStencilTargetFlag = DestTexture->GetFlags();
			GraphicsPSOInit.NumSamples = DestTexture->GetNumSamples();
		}
	}
	else
	{
		// Clear the destination texture.
		if (bClearDestTexture)
		{
			if (IsDefaultContext())
			{
				GetParentDevice()->RegisterGPUWork(0);
			}

			TransitionResource(DestTextureRTV, D3D12_RESOURCE_STATE_RENDER_TARGET);

			FlushResourceBarriers();

			FLinearColor ClearColor(0, 0, 0, 0);
			GraphicsCommandList()->ClearRenderTargetView(DestTextureRTV->GetView(), (float*)&ClearColor, 0, nullptr);
			UpdateResidency(DestTextureRTV->GetResource());
		}

		// Write to the dest surface as a render target.
		StateCache.SetRenderTargets(1, &DestTextureRTV, nullptr);

		GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

		if (DestTexture)
		{
			GraphicsPSOInit.RenderTargetFormats[0] = DestTexture->GetFormat();
			GraphicsPSOInit.RenderTargetFlags[0] = DestTexture->GetFlags();
			GraphicsPSOInit.NumSamples = DestTexture->GetNumSamples();
		}
	}

	{
		TRHICommandList_RecursiveHazardous<FD3D12CommandContext> RHICmdList(this);
		// Lambda to guard access to 'this'
		([&ResolveTargetDesc, &GraphicsPSOInit, &SourceRect, &DestRect, &PixelShaderParameter, &SourceTexture](auto& RHICmdList)
		{
			SCOPED_DRAW_EVENT(RHICmdList, ResolveTextureUsingShader);

			RHICmdList.SetViewport(0.0f, 0.0f, 0.0f, (uint32)ResolveTargetDesc.Width, ResolveTargetDesc.Height, 1.0f);

			// Set the vertex and pixel shader
			auto ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
			TShaderMapRef<FResolveVS> ResolveVertexShader(ShaderMap);
			TShaderMapRef<TPixelShader> ResolvePixelShader(ShaderMap);

			GraphicsPSOInit.BoundShaderState.VertexShaderRHI = ResolveVertexShader.GetVertexShader();
			GraphicsPSOInit.BoundShaderState.PixelShaderRHI = ResolvePixelShader.GetPixelShader();
			GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
			GraphicsPSOInit.PrimitiveType = PT_TriangleStrip;

			SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);
			RHICmdList.SetBlendFactor(FLinearColor::White);

			ResolveVertexShader->SetParameters(RHICmdList, SourceRect, DestRect, ResolveTargetDesc.Width, ResolveTargetDesc.Height);
			ResolvePixelShader->SetParameters(RHICmdList, PixelShaderParameter);

			const uint32 TextureIndex = ResolvePixelShader->UnresolvedSurface.GetBaseIndex();

			RHICmdList.RunOnContext([TextureIndex, SourceTexture](FD3D12CommandContext& Context)
			{
				// Set the source texture.
				Context.StateCache.SetShaderResourceView<SF_Pixel>(SourceTexture->GetShaderResourceView(), TextureIndex);
			});

			RHICmdList.DrawPrimitive(0, 2, 1);
		})(RHICmdList);
	}

	ConditionalClearShaderResource(&SourceTexture->ResourceLocation);

	// Reset saved viewport
	{
		StateCache.SetViewports(NumSavedViewports, SavedViewports);
	}
}

static DXGI_FORMAT GetPlaneFormat(DXGI_FORMAT InFormat, uint32 InPlaneSlice)
{
	if (InFormat == DXGI_FORMAT_R32G8X24_TYPELESS || InFormat == DXGI_FORMAT_R24G8_TYPELESS)
	{
		if (InPlaneSlice == 0)
		{
			return DXGI_FORMAT_R32_TYPELESS;
		}
		if (InPlaneSlice == 1)
		{
			return DXGI_FORMAT_R8_TYPELESS;
		}
	}

	return InFormat;
}

/**
* Copies the contents of the given surface to its resolve target texture.
* @param SourceSurface - surface with a resolve texture to copy to
* @param ResolveParams - optional resolve params
*/
void FD3D12CommandContext::RHICopyToResolveTarget(FRHITexture* SourceTextureRHI, FRHITexture* DestTextureRHI, const FResolveParams& ResolveParams)
{
	if (!SourceTextureRHI || !DestTextureRHI)
	{
		// no need to do anything (silently ignored)
		return;
	}

	uint32 GPUIndex = GetGPUIndex();
	FD3D12Texture* SourceTexture = GetD3D12TextureFromRHITexture(SourceTextureRHI, GPUIndex);
	FD3D12Texture* DestTexture = GetD3D12TextureFromRHITexture(DestTextureRHI, GPUIndex);

	const FRHITextureDesc& SourceDesc = SourceTexture->GetDesc();
	const FRHITextureDesc& DestDesc = DestTexture->GetDesc();

	if (SourceDesc.IsTexture2D() && DestDesc.IsTexture2D())
	{
		const D3D_FEATURE_LEVEL FeatureLevel = GetParentDevice()->GetParentAdapter()->GetFeatureLevel();
		if (SourceTexture != DestTexture)
		{
			if (IsDefaultContext())
			{
				GetParentDevice()->RegisterGPUWork();
			}

			if (SourceTexture->GetDepthStencilView(FExclusiveDepthStencil::DepthWrite_StencilWrite)
				&& SourceTextureRHI->IsMultisampled()
				&& !DestTextureRHI->IsMultisampled())
			{
				D3D12_RESOURCE_DESC const& ResolveTargetDesc = DestTexture->GetResource()->GetDesc();

				ResolveTextureUsingShader<FResolveDepthPS>(
					SourceTexture,
					DestTexture,
					DestTexture->GetRenderTargetView(0, -1),
					DestTexture->GetDepthStencilView(FExclusiveDepthStencil::DepthWrite_StencilWrite),
					ResolveTargetDesc,
					GetDefaultRect(ResolveParams.Rect, DestTexture->GetSizeX(), DestTexture->GetSizeY()),
					GetDefaultRect(ResolveParams.Rect, DestTexture->GetSizeX(), DestTexture->GetSizeY()),
					FDummyResolveParameter()
					);
			}
			else
			{
				DXGI_FORMAT SrcFmt = (DXGI_FORMAT)GPixelFormats[SourceTextureRHI->GetFormat()].PlatformFormat;
				DXGI_FORMAT DstFmt = (DXGI_FORMAT)GPixelFormats[DestTexture->GetFormat()].PlatformFormat;

				DXGI_FORMAT Fmt = ConvertTypelessToUnorm((DXGI_FORMAT)GPixelFormats[DestTexture->GetFormat()].PlatformFormat);

				// Determine whether a MSAA resolve is needed, or just a copy.
				if (SourceTextureRHI->IsMultisampled() && !DestTexture->IsMultisampled())
				{
					FScopedResourceBarrier ConditionalScopeResourceBarrierDst(*this, DestTexture->GetResource()  , D3D12_RESOURCE_STATE_RESOLVE_DEST  , ResolveParams.DestArrayIndex  );
					FScopedResourceBarrier ConditionalScopeResourceBarrierSrc(*this, SourceTexture->GetResource(), D3D12_RESOURCE_STATE_RESOLVE_SOURCE, ResolveParams.SourceArrayIndex);

					FlushResourceBarriers();
					GraphicsCommandList()->ResolveSubresource(
						DestTexture->GetResource()->GetResource(),
						ResolveParams.DestArrayIndex,
						SourceTexture->GetResource()->GetResource(),
						ResolveParams.SourceArrayIndex,
						Fmt
						);

					UpdateResidency(SourceTexture->GetResource());
					UpdateResidency(DestTexture->GetResource());
				}
				else
				{
					D3D12_RESOURCE_DESC const& srcDesc = SourceTexture->GetResource()->GetDesc();
					D3D12_RESOURCE_DESC const& ResolveTargetDesc = DestTexture->GetResource()->GetDesc();
					bool bCopySrcSubRect = ResolveParams.Rect.IsValid() && (ResolveParams.Rect.X1 != 0 || ResolveParams.Rect.Y1 != 0 || ResolveParams.Rect.X2 != srcDesc.Width || ResolveParams.Rect.Y2 != srcDesc.Height);
					bool bCopyDstSubRect = ResolveParams.DestRect.IsValid() && (ResolveParams.DestRect.X1 != 0 || ResolveParams.DestRect.Y1 != 0 || ResolveParams.DestRect.X2 != ResolveTargetDesc.Width || ResolveParams.DestRect.Y2 != ResolveTargetDesc.Height);

					if ((bCopySrcSubRect || bCopyDstSubRect)
						&& !SourceTextureRHI->IsMultisampled()
						&& !DestTexture->GetDepthStencilView(FExclusiveDepthStencil::DepthWrite_StencilWrite))
					{
						// currently no support for readback buffers
						check(ResolveTargetDesc.Dimension != D3D12_RESOURCE_DIMENSION_BUFFER);

						const FResolveRect& SrcRect = ResolveParams.Rect.IsValid() ? ResolveParams.Rect : FResolveRect(0, 0, srcDesc.Width, srcDesc.Height);
						D3D12_BOX SrcBox;

						SrcBox.left = SrcRect.X1;
						SrcBox.top = SrcRect.Y1;
						SrcBox.front = 0;
						SrcBox.right = SrcRect.X2;
						SrcBox.bottom = SrcRect.Y2;
						SrcBox.back = 1;

						const FResolveRect& DestRect = ResolveParams.DestRect.IsValid() ? ResolveParams.DestRect : SrcRect;

						FScopedResourceBarrier ConditionalScopeResourceBarrierDst(*this, DestTexture->GetResource()  , D3D12_RESOURCE_STATE_COPY_DEST  , ResolveParams.DestArrayIndex  );
						FScopedResourceBarrier ConditionalScopeResourceBarrierSrc(*this, SourceTexture->GetResource(), D3D12_RESOURCE_STATE_COPY_SOURCE, ResolveParams.SourceArrayIndex);

						CD3DX12_TEXTURE_COPY_LOCATION DestCopyLocation(DestTexture->GetResource()->GetResource(), ResolveParams.DestArrayIndex);
						CD3DX12_TEXTURE_COPY_LOCATION SourceCopyLocation(SourceTexture->GetResource()->GetResource(), ResolveParams.SourceArrayIndex);

						FlushResourceBarriers();
						GraphicsCommandList()->CopyTextureRegion(
							&DestCopyLocation,
							DestRect.X1, DestRect.Y1, 0,
							&SourceCopyLocation,
							&SrcBox);

						UpdateResidency(SourceTexture->GetResource());
						UpdateResidency(DestTexture->GetResource());
					}
					else
					{
						FScopedResourceBarrier ConditionalScopeResourceBarrierSrc(*this, SourceTexture->GetResource(), D3D12_RESOURCE_STATE_COPY_SOURCE, ResolveParams.SourceArrayIndex);

						// Resolve to a buffer.
						if (ResolveTargetDesc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
						{
							check(IsDefaultContext());

							const uint32 PlaneSlice = 0;
							const DXGI_FORMAT DestFormat = GetPlaneFormat(srcDesc.Format, PlaneSlice);

							const uint32 BlockBytes = ComputeBytesPerPixel(DestFormat);
							const uint32 XBytes = (uint32)srcDesc.Width * BlockBytes;
							const uint32 XBytesAligned = Align(XBytes, FD3D12_TEXTURE_DATA_PITCH_ALIGNMENT);

							D3D12_PLACED_SUBRESOURCE_FOOTPRINT DestFootprint{};
							DestFootprint.Footprint.Depth = 1;
							DestFootprint.Footprint.Height = srcDesc.Height;
							DestFootprint.Footprint.Width = srcDesc.Width;
							DestFootprint.Footprint.Format = DestFormat;
							DestFootprint.Footprint.RowPitch = XBytesAligned;

							const UINT SourceSubresource = D3D12CalcSubresource(ResolveParams.MipIndex, ResolveParams.SourceArrayIndex, PlaneSlice, srcDesc.MipLevels, srcDesc.DepthOrArraySize);

							CD3DX12_TEXTURE_COPY_LOCATION DestCopyLocation(DestTexture->GetResource()->GetResource(), DestFootprint);
							CD3DX12_TEXTURE_COPY_LOCATION SourceCopyLocation(SourceTexture->GetResource()->GetResource(), SourceSubresource);

							FlushResourceBarriers();
							GraphicsCommandList()->CopyTextureRegion(
								&DestCopyLocation,
								0, 0, 0,
								&SourceCopyLocation,
								nullptr);

							UpdateResidency(SourceTexture->GetResource());
							UpdateResidency(DestTexture->GetResource());
						}
						// Resolve to a texture.
						else
						{
							// Transition to the copy dest state.
							FScopedResourceBarrier ConditionalScopeResourceBarrierDst(*this, DestTexture->GetResource(), D3D12_RESOURCE_STATE_COPY_DEST, 0);

							CD3DX12_TEXTURE_COPY_LOCATION DestCopyLocation(DestTexture->GetResource()->GetResource(), ResolveParams.DestArrayIndex);
							CD3DX12_TEXTURE_COPY_LOCATION SourceCopyLocation(SourceTexture->GetResource()->GetResource(), ResolveParams.SourceArrayIndex);

							FlushResourceBarriers();
							GraphicsCommandList()->CopyTextureRegion(
								&DestCopyLocation,
								0, 0, 0,
								&SourceCopyLocation,
								nullptr);

							UpdateResidency(SourceTexture->GetResource());
							UpdateResidency(DestTexture->GetResource());
						}
					}
				}
			}
		}
	}
	else if (SourceDesc.IsTextureCube() && DestDesc.IsTextureCube())
	{
		if (SourceTexture != DestTexture)
		{
			if (IsDefaultContext())
			{
				GetParentDevice()->RegisterGPUWork();
			}

			// Determine the cubemap face being resolved.
			const uint32 D3DFace = GetD3D12CubeFace(ResolveParams.CubeFace);
			const uint32 SourceSubresource = CalcSubresource(ResolveParams.MipIndex, ResolveParams.SourceArrayIndex * 6 + D3DFace, SourceTexture->GetNumMips());
			const uint32 DestSubresource = CalcSubresource(ResolveParams.MipIndex, ResolveParams.DestArrayIndex * 6 + D3DFace, DestTexture->GetNumMips());

			// Determine whether a MSAA resolve is needed, or just a copy.
			if (SourceTextureRHI->IsMultisampled() && !DestTexture->IsMultisampled())
			{
				FScopedResourceBarrier ConditionalScopeResourceBarrierDst(*this, DestTexture->GetResource()  , D3D12_RESOURCE_STATE_RESOLVE_DEST  , DestSubresource  );
				FScopedResourceBarrier ConditionalScopeResourceBarrierSrc(*this, SourceTexture->GetResource(), D3D12_RESOURCE_STATE_RESOLVE_SOURCE, SourceSubresource);

				FlushResourceBarriers();
				GraphicsCommandList()->ResolveSubresource(
					DestTexture->GetResource()->GetResource(),
					DestSubresource,
					SourceTexture->GetResource()->GetResource(),
					SourceSubresource,
					(DXGI_FORMAT)GPixelFormats[DestTexture->GetFormat()].PlatformFormat
					);

				UpdateResidency(SourceTexture->GetResource());
				UpdateResidency(DestTexture->GetResource());
			}
			else
			{
				CD3DX12_TEXTURE_COPY_LOCATION DestCopyLocation(DestTexture->GetResource()->GetResource(), DestSubresource);
				CD3DX12_TEXTURE_COPY_LOCATION SourceCopyLocation(SourceTexture->GetResource()->GetResource(), SourceSubresource);

				FScopedResourceBarrier ConditionalScopeResourceBarrierDst(*this, DestTexture->GetResource()  , D3D12_RESOURCE_STATE_COPY_DEST  , DestCopyLocation.SubresourceIndex  );
				FScopedResourceBarrier ConditionalScopeResourceBarrierSrc(*this, SourceTexture->GetResource(), D3D12_RESOURCE_STATE_COPY_SOURCE, SourceCopyLocation.SubresourceIndex);

				FlushResourceBarriers();
				GraphicsCommandList()->CopyTextureRegion(
					&DestCopyLocation,
					0, 0, 0,
					&SourceCopyLocation,
					nullptr);

				UpdateResidency(SourceTexture->GetResource());
				UpdateResidency(DestTexture->GetResource());
			}
		}
	}
	else if (SourceDesc.IsTexture2D() && DestDesc.IsTextureCube())
	{
		// If source is 2D and Dest is a cube then copy the 2D texture to the specified cube face.
		// Determine the cubemap face being resolved.
		const uint32 D3DFace = GetD3D12CubeFace(ResolveParams.CubeFace);
		const uint32 Subresource = CalcSubresource(0, D3DFace, 1);

		CD3DX12_TEXTURE_COPY_LOCATION DestCopyLocation(DestTexture->GetResource()->GetResource(), Subresource);
		CD3DX12_TEXTURE_COPY_LOCATION SourceCopyLocation(SourceTexture->GetResource()->GetResource(), 0);

		FScopedResourceBarrier ConditionalScopeResourceBarrierDst(*this, DestTexture->GetResource()  , D3D12_RESOURCE_STATE_COPY_DEST  , DestCopyLocation.SubresourceIndex  );
		FScopedResourceBarrier ConditionalScopeResourceBarrierSrc(*this, SourceTexture->GetResource(), D3D12_RESOURCE_STATE_COPY_SOURCE, SourceCopyLocation.SubresourceIndex);

		FlushResourceBarriers();
		GraphicsCommandList()->CopyTextureRegion(
			&DestCopyLocation,
			0, 0, 0,
			&SourceCopyLocation,
			nullptr);

		UpdateResidency(SourceTexture->GetResource());
		UpdateResidency(DestTexture->GetResource());
	}

	else if (SourceDesc.IsTexture3D() && DestDesc.IsTexture3D())
	{
		// bit of a hack.  no one resolves slice by slice and 0 is the default value.  assume for the moment they are resolving the whole texture.
		check(ResolveParams.SourceArrayIndex == 0);
		check(SourceTexture == DestTexture);
	}

	// Force transtion to readable since RHI user expects both source and dest to be in readable state after calling RHICopyToResolveTarget
	{
		TRHICommandList_RecursiveHazardous<FD3D12CommandContext> RHICmdList(this);
		if (SourceTextureRHI && ResolveParams.SourceAccessFinal != ERHIAccess::Unknown)
		{
			RHICmdList.Transition(FRHITransitionInfo(SourceTextureRHI, ERHIAccess::Unknown, ResolveParams.SourceAccessFinal));
		}
		if (DestTextureRHI && SourceTextureRHI != DestTextureRHI && ResolveParams.DestAccessFinal != ERHIAccess::Unknown)
		{
			RHICmdList.Transition(FRHITransitionInfo(DestTextureRHI, ERHIAccess::Unknown, ResolveParams.DestAccessFinal));
		}
	}

	ConditionalSplitCommandList();

	DEBUG_EXECUTE_COMMAND_LIST(this);
}

void FD3D12CommandContext::ResolveTexture(UE::RHICore::FResolveTextureInfo Info)
{
	uint32 GPUIndex = GetGPUIndex();

	if (IsDefaultContext())
	{
		GetParentDevice()->RegisterGPUWork();
	}

	FD3D12Texture* SourceTexture         = GetD3D12TextureFromRHITexture(Info.SourceTexture, GPUIndex);
	FD3D12Resource* SourceResource       = SourceTexture->GetResource();
	const FRHITextureDesc& SourceDesc    = SourceTexture->GetDesc();

	FD3D12Texture* DestTexture           = GetD3D12TextureFromRHITexture(Info.DestTexture, GPUIndex);
	FD3D12Resource* DestResource         = DestTexture->GetResource();
	const FRHITextureDesc& DestDesc      = DestTexture->GetDesc();

	if (SourceDesc.Format == PF_DepthStencil)
	{
		ResolveTextureUsingShader<FResolveDepthPS>(
			SourceTexture,
			DestTexture,
			DestTexture->GetRenderTargetView(0, -1),
			DestTexture->GetDepthStencilView(FExclusiveDepthStencil::DepthWrite_StencilWrite),
			DestResource->GetDesc(),
			GetDefaultRect(Info.ResolveRect, SourceDesc.Extent.X, SourceDesc.Extent.Y),
			GetDefaultRect(Info.ResolveRect, DestDesc.Extent.X, DestDesc.Extent.Y),
			FDummyResolveParameter()
		);
	}
	else
	{
		const DXGI_FORMAT DestFormatTypeless = ConvertTypelessToUnorm((DXGI_FORMAT)GPixelFormats[DestDesc.Format].PlatformFormat);

		int32 ArraySliceBegin = Info.ArraySlice;
		int32 ArraySliceEnd   = Info.ArraySlice + 1;

		if (Info.ArraySlice < 0)
		{
			ArraySliceBegin = 0;
			ArraySliceEnd   = SourceDesc.ArraySize;
		}

		for (int32 ArraySlice = ArraySliceBegin; ArraySlice < ArraySliceEnd; ArraySlice++)
		{
			int32 DestSubresource   = CalcSubresource(Info.MipLevel, ArraySlice, DestDesc.NumMips);
			int32 SourceSubresource = CalcSubresource(Info.MipLevel, ArraySlice, SourceDesc.NumMips);

			FScopedResourceBarrier ConditionalScopeResourceBarrierDst(*this, DestTexture->GetResource()  , D3D12_RESOURCE_STATE_RESOLVE_DEST  , DestSubresource  );
			FScopedResourceBarrier ConditionalScopeResourceBarrierSrc(*this, SourceTexture->GetResource(), D3D12_RESOURCE_STATE_RESOLVE_SOURCE, SourceSubresource);

			FlushResourceBarriers();
			GraphicsCommandList()->ResolveSubresource(DestResource->GetResource(), DestSubresource, SourceTexture->GetResource()->GetResource(), SourceSubresource, DestFormatTypeless);
		}
	}

	UpdateResidency(SourceTexture->GetResource());
	UpdateResidency(DestTexture->GetResource());

	ConditionalSplitCommandList();

	DEBUG_EXECUTE_COMMAND_LIST(this);
}

TRefCountPtr<FD3D12Resource> FD3D12DynamicRHI::GetStagingTexture(FRHITexture* TextureRHI, FIntRect InRect, FIntRect& StagingRectOUT, FReadSurfaceDataFlags InFlags, D3D12_PLACED_SUBRESOURCE_FOOTPRINT& ReadbackHeapDesc, uint32 GPUIndex)
{
	FD3D12Device* Device = GetRHIDevice(GPUIndex);
	FD3D12Adapter* Adapter = Device->GetParentAdapter();
	const FRHIGPUMask Node = Device->GetGPUMask();

	FD3D12CommandContext& Context = Device->GetDefaultCommandContext();

	FD3D12Texture* Texture = GetD3D12TextureFromRHITexture(TextureRHI, GPUIndex);
	D3D12_RESOURCE_DESC const& SourceDesc = Texture->GetResource()->GetDesc();

	// Ensure we're dealing with a Texture2D, which the rest of this function already assumes
	check(TextureRHI->GetTexture2D());

	bool bRequiresTempStagingTexture = Texture->GetResource()->GetHeapType() != D3D12_HEAP_TYPE_READBACK;
	if (bRequiresTempStagingTexture == false)
	{
		// Returning the same texture is considerably faster than creating and copying to
		// a new staging texture as we do not have to wait for the GPU pipeline to catch up
		// to the staging texture preparation work.

		// Texture2Ds on the readback heap will have been flattened to 1D, so we need to retrieve pitch
		// information from the original 2D version to correctly use sub-rects.
		Texture->GetReadBackHeapDesc(ReadbackHeapDesc, InFlags.GetMip());
		StagingRectOUT = InRect;

		return (Texture->GetResource());
	}

	// a temporary staging texture is needed.
	int32 SizeX = InRect.Width();
	int32 SizeY = InRect.Height();
	// Read back the surface data in the defined rect
	D3D12_BOX Rect;
	Rect.left = InRect.Min.X;
	Rect.top = InRect.Min.Y;
	Rect.right = InRect.Max.X;
	Rect.bottom = InRect.Max.Y;
	Rect.back = 1;
	Rect.front = 0;

	// create a temp 2d texture to copy render target to
	TRefCountPtr<FD3D12Resource> TempTexture2D;

	const uint32 PlaneSlice = 0;
	const DXGI_FORMAT DestFormat = GetPlaneFormat(SourceDesc.Format, PlaneSlice);

	const uint32 BlockBytes = ComputeBytesPerPixel(DestFormat);
	const uint32 XBytesAligned = Align((uint32)SourceDesc.Width * BlockBytes, FD3D12_TEXTURE_DATA_PITCH_ALIGNMENT);
	const uint32 MipBytesAligned = XBytesAligned * SourceDesc.Height;
	VERIFYD3D12RESULT(Adapter->CreateBuffer(D3D12_HEAP_TYPE_READBACK, Node, Node, MipBytesAligned, TempTexture2D.GetInitReference(), nullptr));

	// Staging rectangle is now the whole surface.
	StagingRectOUT.Min = FIntPoint::ZeroValue;
	StagingRectOUT.Max = FIntPoint(SizeX, SizeY);

	// Copy the data to a staging resource.
	uint32 Subresource = 0;
	if (Texture->GetDesc().IsTextureCube())
	{
		uint32 D3DFace = GetD3D12CubeFace(InFlags.GetCubeFace());
		Subresource = CalcSubresource(InFlags.GetMip(), D3DFace, TextureRHI->GetNumMips());
	}
	else
	{
		Subresource = CalcSubresource(InFlags.GetMip(), 0, TextureRHI->GetNumMips());
	}

	D3D12_BOX* RectPtr = nullptr; // API prefers NULL for entire texture.
	if (Rect.left != 0 || Rect.top != 0 || Rect.right != SourceDesc.Width || Rect.bottom != SourceDesc.Height)
	{
		// ..Sub rectangle required, use the D3D12_BOX.
		RectPtr = &Rect;
	}

	D3D12_PLACED_SUBRESOURCE_FOOTPRINT DestFootprint{};
	DestFootprint.Footprint.Depth = 1;
	DestFootprint.Footprint.Height = SourceDesc.Height;
	DestFootprint.Footprint.Width = SourceDesc.Width;
	DestFootprint.Footprint.Format = DestFormat;
	DestFootprint.Footprint.RowPitch = XBytesAligned;
	check((DestFootprint.Footprint.RowPitch % FD3D12_TEXTURE_DATA_PITCH_ALIGNMENT) == 0);	// Make sure we align correctly.

	CD3DX12_TEXTURE_COPY_LOCATION DestCopyLocation(TempTexture2D->GetResource(), DestFootprint);
	CD3DX12_TEXTURE_COPY_LOCATION SourceCopyLocation(Texture->GetResource()->GetResource(), Subresource);

	FScopedResourceBarrier ScopeResourceBarrierSource(Context, Texture->GetResource(), D3D12_RESOURCE_STATE_COPY_SOURCE, SourceCopyLocation.SubresourceIndex);
	Context.FlushResourceBarriers();
	// Upload heap doesn't need to transition

	Context.GraphicsCommandList()->CopyTextureRegion(
		&DestCopyLocation,
		0, 0, 0,
		&SourceCopyLocation,
		RectPtr);

	Context.UpdateResidency(Texture->GetResource());

	// Remember the width, height, pitch, etc...
	ReadbackHeapDesc = DestFootprint;

	// We need to execute the command list so we can read the data from readback heap
	Context.FlushCommands(ED3D12FlushFlags::WaitForCompletion);

	return TempTexture2D;
}

void FD3D12DynamicRHI::ReadSurfaceDataNoMSAARaw(FRHITexture* TextureRHI, FIntRect InRect, TArray<uint8>& OutData, FReadSurfaceDataFlags InFlags)
{
	const uint32 GPUIndex = InFlags.GetGPUIndex();
	FD3D12Texture* Texture = GetD3D12TextureFromRHITexture(TextureRHI, GPUIndex);

	const uint32 SizeX = InRect.Width();
	const uint32 SizeY = InRect.Height();

	// Check the format of the surface
	FIntRect StagingRect;
	D3D12_PLACED_SUBRESOURCE_FOOTPRINT readBackHeapDesc;
	TRefCountPtr<FD3D12Resource> TempTexture2D = GetStagingTexture(TextureRHI, InRect, StagingRect, InFlags, readBackHeapDesc, GPUIndex);

	uint32 BytesPerPixel = GPixelFormats[TextureRHI->GetFormat()].BlockBytes;

	// Allocate the output buffer.
	OutData.Empty();
	OutData.AddUninitialized(SizeX * SizeY * BytesPerPixel);

	uint32 BytesPerLine = BytesPerPixel * InRect.Width();
	const uint32 XBytesAligned = Align((uint32)readBackHeapDesc.Footprint.Width * BytesPerPixel, FD3D12_TEXTURE_DATA_PITCH_ALIGNMENT);
	uint64 SrcStart = readBackHeapDesc.Offset + StagingRect.Min.X * BytesPerPixel + StagingRect.Min.Y * XBytesAligned;

	// Lock the staging resource.
	void* pData;
	D3D12_RANGE ReadRange = { SrcStart, SrcStart + XBytesAligned * (SizeY - 1) + BytesPerLine };
	VERIFYD3D12RESULT(TempTexture2D->GetResource()->Map(0, &ReadRange, &pData));

	uint8* DestPtr = OutData.GetData();
	uint8* SrcPtr = (uint8*)pData + SrcStart;
	for (uint32 Y = 0; Y < SizeY; Y++)
	{
		memcpy(DestPtr, SrcPtr, BytesPerLine);
		DestPtr += BytesPerLine;
		SrcPtr += XBytesAligned;
	}

	TempTexture2D->GetResource()->Unmap(0, nullptr);
}

/** Helper for accessing R10G10B10A2 colors. */
struct FD3DR10G10B10A2
{
	uint32 R : 10;
	uint32 G : 10;
	uint32 B : 10;
	uint32 A : 2;
};

/** Helper for accessing R16G16 colors. */
struct FD3DRG16
{
	uint16 R;
	uint16 G;
};

/** Helper for accessing R16G16B16A16 colors. */
struct FD3DRGBA16
{
	uint16 R;
	uint16 G;
	uint16 B;
	uint16 A;
};

/** Convert D3D format type to general pixel format type*/
static void ConvertDXGIToFColor(DXGI_FORMAT Format, uint32 Width, uint32 Height, uint8 *In, uint32 SrcPitch, FColor* Out, FReadSurfaceDataFlags InFlags)
{
	bool bLinearToGamma = InFlags.GetLinearToGamma();
	switch (Format)
	{
	case DXGI_FORMAT_R16_TYPELESS:
		ConvertRawR16DataToFColor(Width, Height, In, SrcPitch, Out);
		break;
	case DXGI_FORMAT_R8G8B8A8_TYPELESS:
	case DXGI_FORMAT_R8G8B8A8_UNORM:
	case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
		ConvertRawR8G8B8A8DataToFColor(Width, Height, In, SrcPitch, Out);
		break;
	case DXGI_FORMAT_B8G8R8A8_TYPELESS:
	case DXGI_FORMAT_B8G8R8A8_UNORM:
	case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
		ConvertRawB8G8R8A8DataToFColor(Width, Height, In, SrcPitch, Out);
		break;
	case DXGI_FORMAT_R10G10B10A2_UNORM:
		ConvertRawR10G10B10A2DataToFColor(Width, Height, In, SrcPitch, Out);
		break;
	case DXGI_FORMAT_R16G16B16A16_FLOAT:
		ConvertRawR16G16B16A16FDataToFColor(Width, Height, In, SrcPitch, Out, bLinearToGamma);
		break;
	case DXGI_FORMAT_R11G11B10_FLOAT:
		ConvertRawR11G11B10DataToFColor(Width, Height, In, SrcPitch, Out, bLinearToGamma);
		break;
	case DXGI_FORMAT_R9G9B9E5_SHAREDEXP:
		ConvertRawR9G9B9E5DataToFColor(Width, Height, In, SrcPitch, Out, bLinearToGamma);
		break;
	case DXGI_FORMAT_R32G32B32A32_FLOAT:
		ConvertRawR32G32B32A32DataToFColor(Width, Height, In, SrcPitch, Out, bLinearToGamma);
		break;
	case DXGI_FORMAT_R24G8_TYPELESS:
		ConvertRawR24G8DataToFColor(Width, Height, In, SrcPitch, Out, InFlags);
		break;
	case DXGI_FORMAT_R32G8X24_TYPELESS:
		ConvertRawR32DataToFColor(Width, Height, In, SrcPitch, Out, InFlags);
		break;
	case DXGI_FORMAT_R16G16B16A16_UNORM:
		ConvertRawR16G16B16A16DataToFColor(Width, Height, In, SrcPitch, Out);
		break;
	case DXGI_FORMAT_R16G16_UNORM:
		ConvertRawR16G16DataToFColor(Width, Height, In, SrcPitch, Out);
		break;
	case DXGI_FORMAT_R8_UNORM:
		ConvertRawR8DataToFColor(Width, Height, In, SrcPitch, Out);
		break;
	default:
		checkf(0, TEXT("Unknown surface format!"));
		break;
	}
}



static void ConvertRAWSurfaceDataToFLinearColor(EPixelFormat Format, uint32 Width, uint32 Height, uint8 *In, uint32 SrcPitch, FLinearColor* Out, FReadSurfaceDataFlags InFlags)
{
	if (Format == PF_R16F || Format == PF_R16F_FILTER)
	{
		// e.g. shadow maps
		for (uint32 Y = 0; Y < Height; Y++)
		{
			uint16* SrcPtr = (uint16*)(In + Y * SrcPitch);
			FLinearColor* DestPtr = Out + Y * Width;

			for (uint32 X = 0; X < Width; X++)
			{
				uint16 Value16 = *SrcPtr;
				float Value = Value16 / (float)(0xffff);

				*DestPtr = FLinearColor(Value, Value, Value);
				++SrcPtr;
				++DestPtr;
			}
		}
	}
	else if (Format == PF_R8G8B8A8)
	{
		// Read the data out of the buffer, converting it from ABGR to ARGB.
		for (uint32 Y = 0; Y < Height; Y++)
		{
			FColor* SrcPtr = (FColor*)(In + Y * SrcPitch);
			FLinearColor* DestPtr = Out + Y * Width;
			for (uint32 X = 0; X < Width; X++)
			{
				FColor sRGBColor = FColor(SrcPtr->B, SrcPtr->G, SrcPtr->R, SrcPtr->A);
				*DestPtr = FLinearColor(sRGBColor);
				++SrcPtr;
				++DestPtr;
			}
		}
	}
	else if (Format == PF_B8G8R8A8)
	{
		for (uint32 Y = 0; Y < Height; Y++)
		{
			FColor* SrcPtr = (FColor*)(In + Y * SrcPitch);
			FLinearColor* DestPtr = Out + Y * Width;
			for (uint32 X = 0; X < Width; X++)
			{
				FColor sRGBColor = FColor(SrcPtr->R, SrcPtr->G, SrcPtr->B, SrcPtr->A);
				*DestPtr = FLinearColor(sRGBColor);
				++SrcPtr;
				++DestPtr;
			}
		}
	}
	else if (Format == PF_A2B10G10R10)
	{
		// Read the data out of the buffer, converting it from R10G10B10A2 to FLinearColor.
		for (uint32 Y = 0; Y < Height; Y++)
		{
			FD3DR10G10B10A2* SrcPtr = (FD3DR10G10B10A2*)(In + Y * SrcPitch);
			FLinearColor* DestPtr = Out + Y * Width;
			for (uint32 X = 0; X < Width; X++)
			{
				*DestPtr = FLinearColor(
					(float)SrcPtr->R / 1023.0f,
					(float)SrcPtr->G / 1023.0f,
					(float)SrcPtr->B / 1023.0f,
					(float)SrcPtr->A / 3.0f
				);
				++SrcPtr;
				++DestPtr;
			}
		}
	}
	else if (Format == PF_FloatRGBA)
	{
		if (InFlags.GetCompressionMode() == RCM_MinMax)
		{
			for (uint32 Y = 0; Y < Height; Y++)
			{
				FFloat16* SrcPtr = (FFloat16*)(In + Y * SrcPitch);
				FLinearColor* DestPtr = Out + Y * Width;

				for (uint32 X = 0; X < Width; X++)
				{
					*DestPtr = FLinearColor((float)SrcPtr[0], (float)SrcPtr[1], (float)SrcPtr[2], (float)SrcPtr[3]);
					++DestPtr;
					SrcPtr += 4;
				}
			}
		}
		else
		{
			FPlane	MinValue(0.0f, 0.0f, 0.0f, 0.0f);
			FPlane	MaxValue(1.0f, 1.0f, 1.0f, 1.0f);

			check(sizeof(FFloat16) == sizeof(uint16));

			for (uint32 Y = 0; Y < Height; Y++)
			{
				FFloat16* SrcPtr = (FFloat16*)(In + Y * SrcPitch);

				for (uint32 X = 0; X < Width; X++)
				{
					MinValue.X = FMath::Min<float>(SrcPtr[0], MinValue.X);
					MinValue.Y = FMath::Min<float>(SrcPtr[1], MinValue.Y);
					MinValue.Z = FMath::Min<float>(SrcPtr[2], MinValue.Z);
					MinValue.W = FMath::Min<float>(SrcPtr[3], MinValue.W);
					MaxValue.X = FMath::Max<float>(SrcPtr[0], MaxValue.X);
					MaxValue.Y = FMath::Max<float>(SrcPtr[1], MaxValue.Y);
					MaxValue.Z = FMath::Max<float>(SrcPtr[2], MaxValue.Z);
					MaxValue.W = FMath::Max<float>(SrcPtr[3], MaxValue.W);
					SrcPtr += 4;
				}
			}

			for (uint32 Y = 0; Y < Height; Y++)
			{
				FFloat16* SrcPtr = (FFloat16*)(In + Y * SrcPitch);
				FLinearColor* DestPtr = Out + Y * Width;

				for (uint32 X = 0; X < Width; X++)
				{
					*DestPtr = FLinearColor(
						(SrcPtr[0] - MinValue.X) / (MaxValue.X - MinValue.X),
						(SrcPtr[1] - MinValue.Y) / (MaxValue.Y - MinValue.Y),
						(SrcPtr[2] - MinValue.Z) / (MaxValue.Z - MinValue.Z),
						(SrcPtr[3] - MinValue.W) / (MaxValue.W - MinValue.W)
					);
					++DestPtr;
					SrcPtr += 4;
				}
			}
		}
	}
	else if (Format == PF_FloatRGB || Format == PF_FloatR11G11B10)
	{
		check(sizeof(FFloat3Packed) == sizeof(uint32));

		for (uint32 Y = 0; Y < Height; Y++)
		{
			FFloat3Packed* SrcPtr = (FFloat3Packed*)(In + Y * SrcPitch);
			FLinearColor* DestPtr = Out + Y * Width;

			for (uint32 X = 0; X < Width; X++)
			{
				*DestPtr = (*SrcPtr).ToLinearColor();
				++DestPtr;
				++SrcPtr;
			}
		}
	}
	else if (Format == PF_A32B32G32R32F)
	{
		if (InFlags.GetCompressionMode() == RCM_MinMax)
		{
			// Copy data directly, respecting existing min-max values
			FLinearColor* SrcPtr = (FLinearColor*)In;
			FLinearColor* DestPtr = (FLinearColor*)Out;
			const int32 ImageSize = sizeof(FLinearColor) * Height * Width;

			FMemory::Memcpy(DestPtr, SrcPtr, ImageSize);
		}
		else
		{
			// Normalize data
			FPlane MinValue(0.0f, 0.0f, 0.0f, 0.0f);
			FPlane MaxValue(1.0f, 1.0f, 1.0f, 1.0f);

			for (uint32 Y = 0; Y < Height; Y++)
			{
				float* SrcPtr = (float*)(In + Y * SrcPitch);

				for (uint32 X = 0; X < Width; X++)
				{
					MinValue.X = FMath::Min<float>(SrcPtr[0], MinValue.X);
					MinValue.Y = FMath::Min<float>(SrcPtr[1], MinValue.Y);
					MinValue.Z = FMath::Min<float>(SrcPtr[2], MinValue.Z);
					MinValue.W = FMath::Min<float>(SrcPtr[3], MinValue.W);
					MaxValue.X = FMath::Max<float>(SrcPtr[0], MaxValue.X);
					MaxValue.Y = FMath::Max<float>(SrcPtr[1], MaxValue.Y);
					MaxValue.Z = FMath::Max<float>(SrcPtr[2], MaxValue.Z);
					MaxValue.W = FMath::Max<float>(SrcPtr[3], MaxValue.W);
					SrcPtr += 4;
				}
			}

			float* SrcPtr = (float*)In;

			for (uint32 Y = 0; Y < Height; Y++)
			{
				FLinearColor* DestPtr = Out + Y * Width;

				for (uint32 X = 0; X < Width; X++)
				{
					*DestPtr = FLinearColor(
						(SrcPtr[0] - MinValue.X) / (MaxValue.X - MinValue.X),
						(SrcPtr[1] - MinValue.Y) / (MaxValue.Y - MinValue.Y),
						(SrcPtr[2] - MinValue.Z) / (MaxValue.Z - MinValue.Z),
						(SrcPtr[3] - MinValue.W) / (MaxValue.W - MinValue.W)
					);
					++DestPtr;
					SrcPtr += 4;
				}
			}
		}
	}
	else if (Format == PF_DepthStencil || Format == PF_D24)
	{
		// Depth stencil
		for (uint32 Y = 0; Y < Height; Y++)
		{
			uint32* SrcPtr = (uint32 *)(In + Y * SrcPitch);
			FLinearColor* DestPtr = Out + Y * Width;

			for (uint32 X = 0; X < Width; X++)
			{
				float DeviceStencil = 0.0f;
				DeviceStencil = (float)((*SrcPtr & 0xFF000000) >> 24) / 255.0f;
				float DeviceZ = (*SrcPtr & 0xffffff) / (float)(1 << 24);
				float LinearValue = FMath::Min(InFlags.ComputeNormalizedDepth(DeviceZ), 1.0f);
				*DestPtr = FLinearColor(LinearValue, DeviceStencil, 0.0f, 0.0f);
				++DestPtr;
				++SrcPtr;
			}
		}
	}
	// Changing Depth Buffers to 32 bit on Dingo as D24S8 is actually implemented as a 32 bit buffer in the hardware
	else if (Format == PF_DepthStencil)
	{
		// Depth stencil
		for (uint32 Y = 0; Y < Height; Y++)
		{
			uint8* SrcStart = (uint8 *)(In + Y * SrcPitch);
			FLinearColor* DestPtr = Out + Y * Width;

			for (uint32 X = 0; X < Width; X++)
			{
				float DeviceZ = *((float *)(SrcStart));
				float LinearValue = FMath::Min(InFlags.ComputeNormalizedDepth(DeviceZ), 1.0f);
				float DeviceStencil = (float)(*(SrcStart + 4)) / 255.0f;
				*DestPtr = FLinearColor(LinearValue, DeviceStencil, 0.0f, 0.0f);
				SrcStart += 8; //64 bit format with the last 24 bit ignore
			}
		}
	}
	else if (Format == PF_A16B16G16R16)
	{
		// Read the data out of the buffer, converting it to FLinearColor.
		for (uint32 Y = 0; Y < Height; Y++)
		{
			FD3DRGBA16* SrcPtr = (FD3DRGBA16*)(In + Y * SrcPitch);
			FLinearColor* DestPtr = Out + Y * Width;
			for (uint32 X = 0; X < Width; X++)
			{
				*DestPtr = FLinearColor(
					(float)SrcPtr->R / 65535.0f,
					(float)SrcPtr->G / 65535.0f,
					(float)SrcPtr->B / 65535.0f,
					(float)SrcPtr->A / 65535.0f
				);
				++SrcPtr;
				++DestPtr;
			}
		}
	}
	else if (Format == PF_G16R16)
	{
		// Read the data out of the buffer, converting it to FLinearColor.
		for (uint32 Y = 0; Y < Height; Y++)
		{
			FD3DRG16* SrcPtr = (FD3DRG16*)(In + Y * SrcPitch);
			FLinearColor* DestPtr = Out + Y * Width;
			for (uint32 X = 0; X < Width; X++)
			{
				*DestPtr = FLinearColor(
					(float)SrcPtr->R / 65535.0f,
					(float)SrcPtr->G / 65535.0f,
					0);
				++SrcPtr;
				++DestPtr;
			}
		}
	}
	else
	{
		// not supported yet
		check(0);
	}
}


void FD3D12DynamicRHI::RHIReadSurfaceData(FRHITexture* TextureRHI, FIntRect InRect, TArray<FLinearColor>& OutData, FReadSurfaceDataFlags InFlags)
{
	TArray<uint8> OutDataRaw;

	FD3D12Texture* Texture = GetD3D12TextureFromRHITexture(TextureRHI, InFlags.GetGPUIndex());

	// Check the format of the surface
	D3D12_RESOURCE_DESC const& TextureDesc = Texture->GetResource()->GetDesc();

	check(TextureDesc.SampleDesc.Count >= 1);

	if (TextureDesc.SampleDesc.Count == 1)
	{
		ReadSurfaceDataNoMSAARaw(TextureRHI, InRect, OutDataRaw, InFlags);
	}
	else
	{
		ReadSurfaceDataMSAARaw(TextureRHI, InRect, OutDataRaw, InFlags);
	}

	const uint32 SizeX = InRect.Width() * TextureDesc.SampleDesc.Count;
	const uint32 SizeY = InRect.Height();

	// Allocate the output buffer.
	OutData.Empty();
	OutData.AddUninitialized(SizeX * SizeY);

	uint32 BytesPerPixel = ComputeBytesPerPixel(TextureDesc.Format);
	uint32 SrcPitch = SizeX * BytesPerPixel;
	EPixelFormat Format = TextureRHI->GetFormat();
	if (Format != PF_Unknown)
	{
		ConvertRAWSurfaceDataToFLinearColor(Format, SizeX, SizeY, OutDataRaw.GetData(), SrcPitch, OutData.GetData(), InFlags);
	}
}
void FD3D12DynamicRHI::RHIReadSurfaceData(FRHITexture* InRHITexture, FIntRect InRect, TArray<FColor>& OutData, FReadSurfaceDataFlags InFlags)
{
	if (!ensure(InRHITexture))
	{
		OutData.Empty();
		OutData.AddZeroed(InRect.Width() * InRect.Height());
		return;
	}

	const uint32 GPUIndex = InFlags.GetGPUIndex();
	TArray<uint8> OutDataRaw;

	// Could be back buffer reference texture, so get the correct D3D12 texture here
	// We know already that it's a FD3D12Texture2D so cast is safe
#if D3D12_USE_DUMMY_BACKBUFFER
	if (EnumHasAnyFlags(InRHITexture->GetFlags(), TexCreate_Presentable))
	{
		FD3D12BackBufferReferenceTexture2D* BufferBufferReferenceTexture = (FD3D12BackBufferReferenceTexture2D*)InRHITexture;
		InRHITexture = BufferBufferReferenceTexture->GetBackBufferTexture();
	}
#endif

	// Retrieve the base texture
	FD3D12Device* Device = GetRHIDevice(GPUIndex);
	Device->BlockUntilIdle();

	FD3D12CommandContext& CommandContext = Device->GetDefaultCommandContext();
	FD3D12Texture* DestTexture2D = CommandContext.RetrieveTexture(InRHITexture);

	// Check the format of the surface
	D3D12_RESOURCE_DESC const& TextureDesc = DestTexture2D->GetResource()->GetDesc();

	check(TextureDesc.SampleDesc.Count >= 1);

	if (TextureDesc.SampleDesc.Count == 1)
	{
		ReadSurfaceDataNoMSAARaw(DestTexture2D, InRect, OutDataRaw, InFlags);
	}
	else
	{
		ReadSurfaceDataMSAARaw(DestTexture2D, InRect, OutDataRaw, InFlags);
	}

	const uint32 SizeX = InRect.Width() * TextureDesc.SampleDesc.Count;
	const uint32 SizeY = InRect.Height();

	// Allocate the output buffer.
	OutData.Empty();
	OutData.AddUninitialized(SizeX * SizeY);

	FPixelFormatInfo FormatInfo = GPixelFormats[DestTexture2D->GetFormat()];
	uint32 BytesPerPixel = FormatInfo.BlockBytes;
	uint32 SrcPitch = SizeX * BytesPerPixel;

	ConvertDXGIToFColor((DXGI_FORMAT)FormatInfo.PlatformFormat, SizeX, SizeY, OutDataRaw.GetData(), SrcPitch, OutData.GetData(), InFlags);
}

void FD3D12DynamicRHI::ReadSurfaceDataMSAARaw(FRHITexture* TextureRHI, FIntRect InRect, TArray<uint8>& OutData, FReadSurfaceDataFlags InFlags)
{	
	const uint32 GPUIndex = InFlags.GetGPUIndex();
	FD3D12Device* Device = GetRHIDevice(GPUIndex);
	FD3D12Adapter* Adapter = Device->GetParentAdapter();
	const FRHIGPUMask NodeMask = Device->GetGPUMask();

	FD3D12CommandContext& DefaultContext = Device->GetDefaultCommandContext();

	FD3D12Texture* Texture = GetD3D12TextureFromRHITexture(TextureRHI, GPUIndex);

	const uint32 SizeX = InRect.Width();
	const uint32 SizeY = InRect.Height();

	// Check the format of the surface
	D3D12_RESOURCE_DESC const& TextureDesc = Texture->GetResource()->GetDesc();

	uint32 BytesPerPixel = ComputeBytesPerPixel(TextureDesc.Format);

	const uint32 NumSamples = TextureDesc.SampleDesc.Count;

	// Read back the surface data from the define rect
	D3D12_BOX	Rect;
	Rect.left = InRect.Min.X;
	Rect.top = InRect.Min.Y;
	Rect.right = InRect.Max.X;
	Rect.bottom = InRect.Max.Y;
	Rect.back = 1;
	Rect.front = 0;

	// Create a non-MSAA render target to resolve individual samples of the source surface to.
	D3D12_RESOURCE_DESC NonMSAADesc;
	ZeroMemory(&NonMSAADesc, sizeof(NonMSAADesc));
	NonMSAADesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	NonMSAADesc.Width = SizeX;
	NonMSAADesc.Height = SizeY;
	NonMSAADesc.MipLevels = 1;
	NonMSAADesc.DepthOrArraySize = 1;
	NonMSAADesc.Format = TextureDesc.Format;
	NonMSAADesc.SampleDesc.Count = 1;
	NonMSAADesc.SampleDesc.Quality = 0;
	NonMSAADesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	TRefCountPtr<FD3D12Resource> NonMSAATexture2D;

	const D3D12_HEAP_PROPERTIES HeapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT, NodeMask.GetNative(), NodeMask.GetNative());
	VERIFYD3D12RESULT(Adapter->CreateCommittedResource(NonMSAADesc, NodeMask, HeapProps, D3D12_RESOURCE_STATE_RENDER_TARGET, nullptr, NonMSAATexture2D.GetInitReference(), nullptr));

	FD3D12ResourceLocation ResourceLocation(Device);
	ResourceLocation.AsStandAlone(NonMSAATexture2D);

	TRefCountPtr<FD3D12RenderTargetView> NonMSAARTV;
	D3D12_RENDER_TARGET_VIEW_DESC RTVDesc;
	FMemory::Memset(&RTVDesc, 0, sizeof(RTVDesc));

	// typeless is not supported, similar code might be needed for other typeless formats
	RTVDesc.Format = ConvertTypelessToUnorm(NonMSAADesc.Format);

	RTVDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	RTVDesc.Texture2D.MipSlice = 0;
	NonMSAARTV = new FD3D12RenderTargetView(Device, RTVDesc, Texture, ResourceLocation);

	// Create a CPU-accessible staging texture to copy the resolved sample data to.
	TRefCountPtr<FD3D12Resource> StagingTexture2D;

	const uint32 PlaneSlice = 0;
	const DXGI_FORMAT DestFormat = GetPlaneFormat(TextureDesc.Format, PlaneSlice);

	const uint32 BlockBytes = ComputeBytesPerPixel(DestFormat);
	const uint32 XBytesAligned = Align(SizeX * BlockBytes, FD3D12_TEXTURE_DATA_PITCH_ALIGNMENT);
	const uint32 MipBytesAligned = XBytesAligned * SizeY;
	VERIFYD3D12RESULT(Adapter->CreateBuffer(D3D12_HEAP_TYPE_READBACK, NodeMask, NodeMask, MipBytesAligned, StagingTexture2D.GetInitReference(), nullptr));

	// Ensure we're dealing with a Texture2D, which the rest of this function already assumes
	check(TextureRHI->GetTexture2D());

	// Determine the subresource index for cubemaps.
	uint32 Subresource = 0;
	if (Texture->GetDesc().IsTextureCube())
	{
		uint32 D3DFace = GetD3D12CubeFace(InFlags.GetCubeFace());
		Subresource = CalcSubresource(InFlags.GetMip(), D3DFace, TextureRHI->GetNumMips());
	}
	else
	{
		Subresource = CalcSubresource(InFlags.GetMip(), 0, TextureRHI->GetNumMips());
	}

	// Setup the descriptions for the copy to the readback heap.

	D3D12_PLACED_SUBRESOURCE_FOOTPRINT DestFootprint{};
	DestFootprint.Footprint.Depth = 1;
	DestFootprint.Footprint.Height = SizeY;
	DestFootprint.Footprint.Width = SizeX;
	DestFootprint.Footprint.Format = DestFormat;
	DestFootprint.Footprint.RowPitch = XBytesAligned;
	check((DestFootprint.Footprint.RowPitch % FD3D12_TEXTURE_DATA_PITCH_ALIGNMENT) == 0);	// Make sure we align correctly.

	CD3DX12_TEXTURE_COPY_LOCATION DestCopyLocation(StagingTexture2D->GetResource(), DestFootprint);
	CD3DX12_TEXTURE_COPY_LOCATION SourceCopyLocation(NonMSAATexture2D->GetResource(), Subresource);

	// Allocate the output buffer.
	OutData.Empty();
	OutData.AddUninitialized(SizeX * SizeY * NumSamples * BytesPerPixel);

	// Can be optimized by doing all subsamples into a large enough rendertarget in one pass (multiple draw calls)
	for (uint32 SampleIndex = 0; SampleIndex < NumSamples; ++SampleIndex)
	{
		// Resolve the sample to the non-MSAA render target.
		DefaultContext.ResolveTextureUsingShader<FResolveSingleSamplePS>(
			ResourceCast(TextureRHI->GetTexture2D()),
			NULL,
			NonMSAARTV,
			NULL,
			NonMSAADesc,
			FResolveRect(InRect.Min.X, InRect.Min.Y, InRect.Max.X, InRect.Max.Y),
			FResolveRect(0, 0, SizeX, SizeY),
			SampleIndex
			);

		FScopedResourceBarrier ScopeResourceBarrierSource(DefaultContext, NonMSAATexture2D, D3D12_RESOURCE_STATE_COPY_SOURCE, SourceCopyLocation.SubresourceIndex);
		// Upload heap doesn't need to transition

		// Copy the resolved sample data to the staging texture.
		DefaultContext.GraphicsCommandList()->CopyTextureRegion(
			&DestCopyLocation,
			0, 0, 0,
			&SourceCopyLocation,
			&Rect);

		DefaultContext.UpdateResidency(StagingTexture2D);
		DefaultContext.UpdateResidency(NonMSAATexture2D);

		// We need to execute the command list so we can read the data in the map below
		DefaultContext.FlushCommands(ED3D12FlushFlags::WaitForCompletion);

		// Lock the staging texture.
		void* pData;
		VERIFYD3D12RESULT(StagingTexture2D->GetResource()->Map(0, nullptr, &pData));

		// Read the data out of the buffer, could be optimized
		for (int32 Y = InRect.Min.Y; Y < InRect.Max.Y; Y++)
		{
			uint8* SrcPtr = (uint8*)pData + (Y - InRect.Min.Y) * XBytesAligned + InRect.Min.X * BytesPerPixel;
			uint8* DestPtr = &OutData[(Y - InRect.Min.Y) * SizeX * NumSamples * BytesPerPixel + SampleIndex * BytesPerPixel];

			for (int32 X = InRect.Min.X; X < InRect.Max.X; X++)
			{
				for (uint32 i = 0; i < BytesPerPixel; ++i)
				{
					*DestPtr++ = *SrcPtr++;
				}

				DestPtr += (NumSamples - 1) * BytesPerPixel;
			}
		}

		StagingTexture2D->GetResource()->Unmap(0, nullptr);
	}
}

void FD3D12DynamicRHI::RHIMapStagingSurface(FRHITexture* TextureRHI, FRHIGPUFence* FenceRHI, void*& OutData, int32& OutWidth, int32& OutHeight, uint32 GPUIndex)
{	
	FD3D12Texture* DestTexture2D = ResourceCast(TextureRHI->GetTexture2D(), GPUIndex);

	check(DestTexture2D);
	FD3D12Resource* Texture = DestTexture2D->GetResource();

	DXGI_FORMAT Format = (DXGI_FORMAT)GPixelFormats[DestTexture2D->GetFormat()].PlatformFormat;

	uint32 BytesPerPixel = ComputeBytesPerPixel(Format);

	if (FenceRHI && !FenceRHI->Poll())
	{
		ResourceCast(FenceRHI)->WaitCPU();
	}

	void* pData;
	D3D12_RANGE ReadRange = { 0, Texture->GetDesc().Width };

	VERIFYD3D12RESULT_EX(
		Texture->GetResource()->Map(0, &ReadRange, &pData),
		GetAdapter().GetD3DDevice()
	);

	D3D12_PLACED_SUBRESOURCE_FOOTPRINT ReadBackHeapDesc;
	DestTexture2D->GetReadBackHeapDesc(ReadBackHeapDesc, 0);
	OutData = pData;
	OutWidth = ReadBackHeapDesc.Footprint.RowPitch / BytesPerPixel;
	OutHeight = ReadBackHeapDesc.Footprint.Height;

	// MS: It seems like the second frame in some scenes comes into RHIMapStagingSurface BEFORE the copy to the staging texture, thus the readbackHeapDesc isn't set. This could be bug in UE.
	if (ReadBackHeapDesc.Footprint.Format != DXGI_FORMAT_UNKNOWN)
	{
		check(OutWidth != 0);
		check(OutHeight != 0);
	}

	check(OutData);
}

void FD3D12DynamicRHI::RHIUnmapStagingSurface(FRHITexture* TextureRHI, uint32 GPUIndex)
{	
	FD3D12Texture* DestTexture2D = ResourceCast(TextureRHI->GetTexture2D(), GPUIndex);

	check(DestTexture2D);
	ID3D12Resource* Texture = DestTexture2D->GetResource()->GetResource();

	Texture->Unmap(0, nullptr);
}

void FD3D12DynamicRHI::RHIReadSurfaceFloatData(FRHITexture* TextureRHI, FIntRect InRect, TArray<FFloat16Color>& OutData, ECubeFace CubeFace, int32 ArrayIndex, int32 MipIndex)
{
	FReadSurfaceDataFlags ReadFlags(RCM_MinMax, CubeFace);
	ReadFlags.SetArrayIndex(ArrayIndex);
	ReadFlags.SetMip(MipIndex);
	RHIReadSurfaceFloatData(TextureRHI, InRect, OutData, ReadFlags);
}

void FD3D12DynamicRHI::RHIReadSurfaceFloatData(FRHITexture* TextureRHI, FIntRect InRect, TArray<FFloat16Color>& OutData, FReadSurfaceDataFlags InFlags)
{
	const uint32 GPUIndex = InFlags.GetGPUIndex();
	FD3D12Device* Device = GetRHIDevice(GPUIndex);
	FD3D12Adapter* Adapter = Device->GetParentAdapter();
	const FRHIGPUMask Node = Device->GetGPUMask();

	FD3D12CommandContext& DefaultContext = Device->GetDefaultCommandContext();

	FD3D12Texture* Texture = GetD3D12TextureFromRHITexture(TextureRHI, GPUIndex);

	uint32 SizeX = InRect.Width();
	uint32 SizeY = InRect.Height();

	// Check the format of the surface
	D3D12_RESOURCE_DESC const& TextureDesc = Texture->GetResource()->GetDesc();

	check(TextureDesc.Format == GPixelFormats[PF_FloatRGBA].PlatformFormat);

	// Allocate the output buffer.
	OutData.Empty(SizeX * SizeY);

	// Read back the surface data from defined rect
	D3D12_BOX	Rect;
	Rect.left = InRect.Min.X;
	Rect.top = InRect.Min.Y;
	Rect.right = InRect.Max.X;
	Rect.bottom = InRect.Max.Y;
	Rect.back = 1;
	Rect.front = 0;

	// create a temp 2d texture to copy render target to
	TRefCountPtr<FD3D12Resource> TempTexture2D;

	const uint32 PlaneSlice = 0;
	const DXGI_FORMAT DestFormat = GetPlaneFormat(TextureDesc.Format, PlaneSlice);

	const uint32 BlockBytes = ComputeBytesPerPixel(DestFormat);
	const uint32 XBytesAligned = Align((uint32)TextureDesc.Width * BlockBytes, FD3D12_TEXTURE_DATA_PITCH_ALIGNMENT);
	const uint32 MipBytesAligned = XBytesAligned * TextureDesc.Height;
	VERIFYD3D12RESULT(Adapter->CreateBuffer(D3D12_HEAP_TYPE_READBACK, Node, Node, MipBytesAligned, TempTexture2D.GetInitReference(), nullptr));

	// Ensure we're dealing with a Texture2D, which the rest of this function already assumes
	bool bIsTextureCube = Texture->GetDesc().IsTextureCube();
	
	// Copy the data to a staging resource.
	uint32 Subresource = 0;
	if (bIsTextureCube)
	{
		uint32 D3DFace = GetD3D12CubeFace(InFlags.GetCubeFace());
		Subresource = CalcSubresource(InFlags.GetMip(), InFlags.GetArrayIndex() * 6 + D3DFace, TextureDesc.MipLevels);
	}
	else
	{
		const bool bIsTextureArray = Texture->GetDesc().IsTextureArray();
		Subresource = CalcSubresource(InFlags.GetMip(), bIsTextureArray ? InFlags.GetArrayIndex() : 0, TextureDesc.MipLevels);
	}

	D3D12_PLACED_SUBRESOURCE_FOOTPRINT DestFootprint{};
	DestFootprint.Footprint.Depth = 1;
	DestFootprint.Footprint.Height = TextureDesc.Height;
	DestFootprint.Footprint.Width = TextureDesc.Width;
	DestFootprint.Footprint.Format = DestFormat;
	DestFootprint.Footprint.RowPitch = XBytesAligned;
	check((DestFootprint.Footprint.RowPitch % FD3D12_TEXTURE_DATA_PITCH_ALIGNMENT) == 0);	// Make sure we align correctly.

	CD3DX12_TEXTURE_COPY_LOCATION DestCopyLocation(TempTexture2D->GetResource(), DestFootprint);
	CD3DX12_TEXTURE_COPY_LOCATION SourceCopyLocation(Texture->GetResource()->GetResource(), Subresource);

	{
		FScopedResourceBarrier ConditionalScopeResourceBarrier(DefaultContext, Texture->GetResource(), D3D12_RESOURCE_STATE_COPY_SOURCE, SourceCopyLocation.SubresourceIndex);
		// Don't need to transition upload heaps

		DefaultContext.FlushResourceBarriers();
		DefaultContext.GraphicsCommandList()->CopyTextureRegion(
			&DestCopyLocation,
			0, 0, 0,
			&SourceCopyLocation,
			&Rect);

		DefaultContext.UpdateResidency(Texture->GetResource());
	}

	// We need to execute the command list so we can read the data from the map below
	DefaultContext.FlushCommands(ED3D12FlushFlags::WaitForCompletion);

	// Lock the staging resource.
	void* pData;
	D3D12_RANGE Range = { 0, MipBytesAligned };
	VERIFYD3D12RESULT(TempTexture2D->GetResource()->Map(0, &Range, &pData));

	// Presize the array
	int32 TotalCount = SizeX * SizeY;
	if (TotalCount >= OutData.Num())
	{
		OutData.AddZeroed(TotalCount);
	}

	for (int32 Y = InRect.Min.Y; Y < InRect.Max.Y; Y++)
	{
		FFloat16Color* SrcPtr = (FFloat16Color*)((uint8*)pData + (Y - InRect.Min.Y) * XBytesAligned);
		int32 Index = (Y - InRect.Min.Y) * SizeX;
		check(Index + ((int32)SizeX - 1) < OutData.Num());
		FFloat16Color* DestColor = &OutData[Index];
		FFloat16* DestPtr = (FFloat16*)(DestColor);
		FMemory::Memcpy(DestPtr, SrcPtr, SizeX * sizeof(FFloat16) * 4);
	}

	TempTexture2D->GetResource()->Unmap(0, nullptr);
}

void FD3D12DynamicRHI::RHIRead3DSurfaceFloatData(FRHITexture* TextureRHI, FIntRect InRect, FIntPoint ZMinMax, TArray<FFloat16Color>& OutData)
{
	RHIRead3DSurfaceFloatData(TextureRHI, InRect, ZMinMax, OutData, FReadSurfaceDataFlags(RCM_MinMax));
}

void FD3D12DynamicRHI::RHIRead3DSurfaceFloatData(FRHITexture* TextureRHI, FIntRect InRect, FIntPoint ZMinMax, TArray<FFloat16Color>& OutData, FReadSurfaceDataFlags InFlags)
{
	const uint32 GPUIndex = InFlags.GetGPUIndex();
	FD3D12Device* Device = GetRHIDevice(GPUIndex);
	FD3D12Adapter* Adapter = Device->GetParentAdapter();
	const FRHIGPUMask Node = Device->GetGPUMask();

	FD3D12CommandContext& DefaultContext = Device->GetDefaultCommandContext();

	FD3D12Texture* Texture = GetD3D12TextureFromRHITexture(TextureRHI, GPUIndex);

	uint32 SizeX = InRect.Width();
	uint32 SizeY = InRect.Height();
	uint32 SizeZ = ZMinMax.Y - ZMinMax.X;

	// Check the format of the surface
	D3D12_RESOURCE_DESC const& TextureDesc = Texture->GetResource()->GetDesc();
	bool bIsRGBAFmt = TextureDesc.Format == GPixelFormats[PF_FloatRGBA].PlatformFormat;
	bool bIsR16FFmt = TextureDesc.Format == GPixelFormats[PF_R16F].PlatformFormat;
	bool bIsR32FFmt = TextureDesc.Format == GPixelFormats[PF_R32_FLOAT].PlatformFormat;
	check(bIsRGBAFmt || bIsR16FFmt || bIsR32FFmt);

	// Allocate the output buffer.
	OutData.Empty(SizeX * SizeY * SizeZ * sizeof(FFloat16Color));

	// Read back the surface data from defined rect
	D3D12_BOX	Rect;
	Rect.left = InRect.Min.X;
	Rect.top = InRect.Min.Y;
	Rect.right = InRect.Max.X;
	Rect.bottom = InRect.Max.Y;
	Rect.back = ZMinMax.Y;
	Rect.front = ZMinMax.X;

	// create a temp 3d texture to copy render target to
	TRefCountPtr<FD3D12Resource> TempTexture3D;
	const uint32 BlockBytes = GPixelFormats[TextureRHI->GetFormat()].BlockBytes;
	const uint32 XBytesAligned = Align(TextureDesc.Width * BlockBytes, FD3D12_TEXTURE_DATA_PITCH_ALIGNMENT);
	const uint32 DepthBytesAligned = XBytesAligned * TextureDesc.Height;
	const uint32 MipBytesAligned = DepthBytesAligned * TextureDesc.DepthOrArraySize;
	VERIFYD3D12RESULT(Adapter->CreateBuffer(D3D12_HEAP_TYPE_READBACK, Node, Node, MipBytesAligned, TempTexture3D.GetInitReference(), nullptr));

	// Copy the data to a staging resource.
	uint32 Subresource = 0;
	uint32 BytesPerPixel = ComputeBytesPerPixel(TextureDesc.Format);
	D3D12_SUBRESOURCE_FOOTPRINT DestSubresource;
	DestSubresource.Depth = TextureDesc.DepthOrArraySize;
	DestSubresource.Height = TextureDesc.Height;
	DestSubresource.Width = TextureDesc.Width;
	DestSubresource.Format = TextureDesc.Format;
	DestSubresource.RowPitch = XBytesAligned;
	check(DestSubresource.RowPitch % FD3D12_TEXTURE_DATA_PITCH_ALIGNMENT == 0);	// Make sure we align correctly.

	D3D12_PLACED_SUBRESOURCE_FOOTPRINT PlacedTexture3D = { 0 };
	PlacedTexture3D.Offset = 0;
	PlacedTexture3D.Footprint = DestSubresource;

	CD3DX12_TEXTURE_COPY_LOCATION DestCopyLocation(TempTexture3D->GetResource(), PlacedTexture3D);
	CD3DX12_TEXTURE_COPY_LOCATION SourceCopyLocation(Texture->GetResource()->GetResource(), Subresource);

	{
		FScopedResourceBarrier ConditionalScopeResourceBarrier(DefaultContext, Texture->GetResource(), D3D12_RESOURCE_STATE_COPY_SOURCE, SourceCopyLocation.SubresourceIndex);
		// Don't need to transition upload heaps

		DefaultContext.FlushResourceBarriers();
		DefaultContext.GraphicsCommandList()->CopyTextureRegion(
			&DestCopyLocation,
			0, 0, 0,
			&SourceCopyLocation,
			&Rect);

		DefaultContext.UpdateResidency(Texture->GetResource());
	}

	// We need to execute the command list so we can read the data from the map below
	DefaultContext.FlushCommands(ED3D12FlushFlags::WaitForCompletion);

	// Lock the staging resource.
	void* pData;
	VERIFYD3D12RESULT(TempTexture3D->GetResource()->Map(0, nullptr, &pData));

	// Presize the array
	int32 TotalCount = SizeX * SizeY * SizeZ;
	if (TotalCount >= OutData.Num())
	{
		OutData.AddZeroed(TotalCount);
	}

	// Read the data out of the buffer
	if (bIsRGBAFmt)
	{
		// Texture is RGBA16F format
		for (int32 Z = ZMinMax.X; Z < ZMinMax.Y; ++Z)
		{
			for (int32 Y = InRect.Min.Y; Y < InRect.Max.Y; ++Y)
			{
				const FFloat16Color* SrcPtr = (const FFloat16Color*)((const uint8*)pData + (Y - InRect.Min.Y) * XBytesAligned + (Z - ZMinMax.X) * DepthBytesAligned);
				int32 Index = (Y - InRect.Min.Y) * SizeX + (Z - ZMinMax.X) * SizeX * SizeY;
				check(Index < OutData.Num());
				FFloat16Color* DestPtr = &OutData[Index];
				FMemory::Memcpy(DestPtr, SrcPtr, SizeX * sizeof(FFloat16Color));
			}
		}
	}
	else if (bIsR16FFmt)
	{
		// Texture is R16F format
		for (int32 Z = ZMinMax.X; Z < ZMinMax.Y; ++Z)
		{
			for (int32 Y = InRect.Min.Y; Y < InRect.Max.Y; ++Y)
			{
				const FFloat16* SrcPtr = (const FFloat16*)((const uint8*)pData + (Y - InRect.Min.Y) * XBytesAligned + (Z - ZMinMax.X) * DepthBytesAligned);
				for (int32 X = InRect.Min.X; X < InRect.Max.X; ++X)
				{
					int32 Index = (Y - InRect.Min.Y) * SizeX + (Z - ZMinMax.X) * SizeX * SizeY + X;
					check(Index < OutData.Num());
					OutData[Index].R = SrcPtr[X];
					OutData[Index].A = FFloat16(1.0f); // ensure full alpha (as if you sampled on GPU)
				}
			}
		}
	}
	else if (bIsR32FFmt)
	{
		// Texture is R16F format
		for (int32 Z = ZMinMax.X; Z < ZMinMax.Y; ++Z)
		{
			for (int32 Y = InRect.Min.Y; Y < InRect.Max.Y; ++Y)
			{
				const float* SrcPtr = (const float*)((const uint8*)pData + (Y - InRect.Min.Y) * XBytesAligned + (Z - ZMinMax.X) * DepthBytesAligned);
				for (int32 X = InRect.Min.X; X < InRect.Max.X; ++X)
				{
					int32 Index = (Y - InRect.Min.Y) * SizeX + (Z - ZMinMax.X) * SizeX * SizeY + X;
					check(Index < OutData.Num());
					OutData[Index].R = FFloat16(SrcPtr[X]);
					OutData[Index].A = FFloat16(1.0f); // ensure full alpha (as if you sampled on GPU)
				}
			}
		}
	}

	TempTexture3D->GetResource()->Unmap(0, nullptr);
}
