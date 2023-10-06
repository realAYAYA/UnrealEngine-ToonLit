// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"

#if PLATFORM_WINDOWS && !UE_SERVER

#include "ElectraTextureSample.h"

#include "ProfilingDebugging/RealtimeGPUProfiler.h"
#include "RenderUtils.h"

#include "Windows/AllowWindowsPlatformTypes.h"
#ifdef ELECTRA_HAVE_DX11
#include "D3D11State.h"
#include "D3D11Resources.h"
#endif
#include "d3d12.h"
#include "Windows/HideWindowsPlatformTypes.h"

/*
	Short summary of how we get data:

	- Win10+ (HW decode is used at all times if handling H.264/5)
	-- DX11:   we receive data in GPU space as NV12/P010 texture
	-- DX12:   we receive data in CPU(yes) space as NV12/P010 texture
	-- Vulkan: we receive data in CPU(yes) space as NV12/P010 texture
	-- Other codec's data usually arrives as CPU space texture buffer

	- Win8:
	-- SW-decode fall back: we receive data in a shared DX11 texture (despite it being SW decode) in NV12 format

	- Win7:
	-- we receive data in a CPU space buffer in NV12 format (no P010 support)
*/

// -------------------------------------------------------------------------------------------------------------------------

DECLARE_GPU_STAT_NAMED(MediaWinDecoder_Convert, TEXT("MediaWinDecoder_Convert"));

// --------------------------------------------------------------------------------------------------------------------------

void FElectraTextureSample::Initialize(FVideoDecoderOutput *InVideoDecoderOutput)
{
	IElectraTextureSampleBase::Initialize(InVideoDecoderOutput);
	VideoDecoderOutputPC = static_cast<FVideoDecoderOutputPC*>(InVideoDecoderOutput);

	EPixelFormat Format = VideoDecoderOutput->GetFormat();
	switch (Format)
	{
	case PF_NV12: SampleFormat = EMediaTextureSampleFormat::CharNV12; break;
	case PF_P010: SampleFormat = EMediaTextureSampleFormat::P010; break;
	case PF_DXT1: SampleFormat = EMediaTextureSampleFormat::DXT1; break;
	case PF_DXT5:
	{
		switch (VideoDecoderOutput->GetFormatEncoding())
		{
		case EVideoDecoderPixelEncoding::YCoCg:				SampleFormat = EMediaTextureSampleFormat::YCoCg_DXT5; break;
		case EVideoDecoderPixelEncoding::YCoCg_Alpha:		SampleFormat = EMediaTextureSampleFormat::YCoCg_DXT5_Alpha_BC4; break;
		case EVideoDecoderPixelEncoding::Native:			SampleFormat = EMediaTextureSampleFormat::DXT5; break;
		default:
			{
			check(!"Unsupported pixel format encoding");
			SampleFormat = EMediaTextureSampleFormat::Undefined;
			break;
			}
		}
		break;
	}
	case PF_BC4:											SampleFormat = EMediaTextureSampleFormat::BC4; break;
	case PF_A16B16G16R16:
	{
		switch (VideoDecoderOutput->GetFormatEncoding())
		{
		case EVideoDecoderPixelEncoding::CbY0CrY1:			SampleFormat = EMediaTextureSampleFormat::YUVv216; break;
		case EVideoDecoderPixelEncoding::Y0CbY1Cr:			SampleFormat = EMediaTextureSampleFormat::Undefined; break; // TODO!!!!!!!! ("swapped" v216 - seems there is no real format for this?)
		case EVideoDecoderPixelEncoding::YCbCr_Alpha:		SampleFormat = EMediaTextureSampleFormat::Y416; break;
		case EVideoDecoderPixelEncoding::ARGB_BigEndian:	SampleFormat = EMediaTextureSampleFormat::ARGB16_BIG; break;
		case EVideoDecoderPixelEncoding::Native:			SampleFormat = EMediaTextureSampleFormat::ABGR16; break;
		default:
			{
			check(!"Unsupported pixel format encoding");
			SampleFormat = EMediaTextureSampleFormat::Undefined;
			break;
			}
		}
		break;
	}
	case PF_R16G16B16A16_UNORM:
	{
		check(VideoDecoderOutput->GetFormatEncoding() == EVideoDecoderPixelEncoding::Native);
		SampleFormat = EMediaTextureSampleFormat::RGBA16;
		break;
	}
	case PF_A32B32G32R32F:
	{
		switch (VideoDecoderOutput->GetFormatEncoding())
		{
		case EVideoDecoderPixelEncoding::Native:			SampleFormat = EMediaTextureSampleFormat::FloatRGBA; break;
		case EVideoDecoderPixelEncoding::YCbCr_Alpha:		SampleFormat = EMediaTextureSampleFormat::R4FL; break;
		default:
		{
			check(!"Unsupported pixel format encoding");
			SampleFormat = EMediaTextureSampleFormat::Undefined;
			break;
		}
		}
		break;
	}
	case PF_B8G8R8A8:
	{
		switch (VideoDecoderOutput->GetFormatEncoding())
		{
		case EVideoDecoderPixelEncoding::CbY0CrY1:		SampleFormat = EMediaTextureSampleFormat::Char2VUY; break;
		case EVideoDecoderPixelEncoding::Y0CbY1Cr:		SampleFormat = EMediaTextureSampleFormat::CharYUY2; break;
		case EVideoDecoderPixelEncoding::YCbCr_Alpha:	SampleFormat = EMediaTextureSampleFormat::CharAYUV; break;
		case EVideoDecoderPixelEncoding::Native:		SampleFormat = EMediaTextureSampleFormat::CharBGRA; break;
		default:
		{
			check(!"Unsupported pixel format encoding");
			SampleFormat = EMediaTextureSampleFormat::Undefined;
			break;
		}
		}
		break;
	}
	case PF_R8G8B8A8:
	{
		switch (VideoDecoderOutput->GetFormatEncoding())
		{
		case EVideoDecoderPixelEncoding::Native:		SampleFormat = EMediaTextureSampleFormat::CharRGBA; break;
		default:
		{
			check(!"Unsupported pixel format encoding");
			SampleFormat = EMediaTextureSampleFormat::Undefined;
			break;
		}
		}
		break;
	}
	case PF_A2B10G10R10:
	{
		switch (VideoDecoderOutput->GetFormatEncoding())
		{
		case EVideoDecoderPixelEncoding::CbY0CrY1:			SampleFormat = EMediaTextureSampleFormat::YUVv210; break;
		case EVideoDecoderPixelEncoding::Native:			SampleFormat = EMediaTextureSampleFormat::CharBGR10A2; break;
		default:
			{
			check(!"Unsupported pixel format encoding");
			SampleFormat = EMediaTextureSampleFormat::Undefined;
			break;
			}
		}
		break;
	}
	default:
		check(!"Decoder sample format not supported in Electra texture sample!");
	}

	bCanUseSRGB = (Format == PF_B8G8R8A8 || Format == PF_R8G8B8A8 || Format == PF_DXT1 || Format == PF_DXT5 || Format == PF_BC4);

	if (RHIGetInterfaceType() == ERHIInterfaceType::D3D12)
	{
		ID3D12Device* ApplicationDxDevice = static_cast<ID3D12Device*>(GDynamicRHI->RHIGetNativeDevice());
		HRESULT Res;
		if (!D3DCmdAllocator.IsValid())
		{
			Res = ApplicationDxDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(D3DCmdAllocator.GetInitReference()));
			check(!FAILED(Res));
		}
		if (!D3DCmdList.IsValid())
		{
			Res = ApplicationDxDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, D3DCmdAllocator.GetReference(), nullptr, __uuidof(ID3D12CommandList), reinterpret_cast<void**>(D3DCmdList.GetInitReference()));
			check(!FAILED(Res));
			D3DCmdList->Close();
		}
	}
}

IMediaTextureSampleConverter* FElectraTextureSample::GetMediaTextureSampleConverter()
{
	if (VideoDecoderOutputPC)
	{
		bool bHasTexture = !!VideoDecoderOutputPC->GetTexture();

		// DXT5 & BC4 combo-data in CPU side buffer?
		if (!bHasTexture && SampleFormat == EMediaTextureSampleFormat::YCoCg_DXT5_Alpha_BC4)
		{
			return this;
		}

		// All other versions might need SW fallback - check if we have a real texture as source -> converter needed
		return VideoDecoderOutputPC->GetTexture() ? this : nullptr;
	}
	return nullptr;
}


struct FRHICommandCopyResourceDX12 final : public FRHICommand<FRHICommandCopyResourceDX12>
{
	TRefCountPtr<ID3D12Resource> SampleTexture;
	FTexture2DRHIRef SampleDestinationTexture;
	TRefCountPtr<ID3D12CommandAllocator> D3DCmdAllocator;
	TRefCountPtr<ID3D12GraphicsCommandList> D3DCmdList;
	TRefCountPtr<ID3D12Fence> D3DFence;
	uint64 FenceValue;

	FRHICommandCopyResourceDX12(ID3D12Resource* InSampleTexture, FRHITexture2D* InSampleDestinationTexture, TRefCountPtr<ID3D12CommandAllocator> InD3DCmdAllocator, TRefCountPtr<ID3D12GraphicsCommandList> InD3DCmdList, TRefCountPtr<ID3D12Fence> InD3DFence, uint64 InFenceValue)
		: SampleTexture(InSampleTexture)
		, SampleDestinationTexture(InSampleDestinationTexture)
		, D3DCmdAllocator(InD3DCmdAllocator)
		, D3DCmdList(InD3DCmdList)
		, D3DFence(InD3DFence)
		, FenceValue(InFenceValue)
	{
		LLM_SCOPE(ELLMTag::MediaStreaming);

		auto DstTexture = (ID3D12Resource*)SampleDestinationTexture->GetNativeResource();

		D3D12_RESOURCE_BARRIER PreCopyResourceBarriers[] = {
			{D3D12_RESOURCE_BARRIER_TYPE_TRANSITION, D3D12_RESOURCE_BARRIER_FLAG_NONE, {{ SampleTexture, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_SOURCE }}},
			{D3D12_RESOURCE_BARRIER_TYPE_TRANSITION, D3D12_RESOURCE_BARRIER_FLAG_NONE, {{ DstTexture, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST }}},
		};

		D3D12_RESOURCE_BARRIER PostCopyResourceBarriers[] = {
			{ D3D12_RESOURCE_BARRIER_TYPE_TRANSITION, D3D12_RESOURCE_BARRIER_FLAG_NONE, {{ SampleTexture, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_COMMON }}},
			{ D3D12_RESOURCE_BARRIER_TYPE_TRANSITION, D3D12_RESOURCE_BARRIER_FLAG_NONE, {{ DstTexture, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE }}},
		};

		D3DCmdAllocator->Reset();
		D3DCmdList->Reset(D3DCmdAllocator, nullptr);

		D3DCmdList->ResourceBarrier(2, PreCopyResourceBarriers);
		D3DCmdList->CopyResource(DstTexture, SampleTexture);
		D3DCmdList->ResourceBarrier(2, PostCopyResourceBarriers);

		D3DCmdList->Close();
	}

	void Execute(FRHICommandListBase& CmdList)
	{
		TRefCountPtr<ID3D12GraphicsCommandList> D3DCmdList2 = D3DCmdList;
		TRefCountPtr<ID3D12CommandAllocator> D3DCmdAllocator2 = D3DCmdAllocator;
		TRefCountPtr<ID3D12Resource> SampleTexture2 = SampleTexture;
		TRefCountPtr<ID3D12Fence> D3DFence2 = D3DFence;
		FTexture2DRHIRef SampleDestinationTexture2 = SampleDestinationTexture;
		ENQUEUE_RENDER_COMMAND(MediaCopyInputTexture)(
			[D3DCmdList2, D3DCmdAllocator2, SampleTexture2, SampleDestinationTexture2, D3DFence2, FenceValue2 = FenceValue](FRHICommandListImmediate& RHICmdList)
		{
			LLM_SCOPE(ELLMTag::MediaStreaming);
			auto D3DCmdQueue = (ID3D12CommandQueue*)FRHICommandListExecutor::GetImmediateCommandList().GetNativeGraphicsQueue();
			if (D3DFence2.IsValid())
			{
				D3DCmdQueue->Wait(D3DFence2, FenceValue2);
			}
			ID3D12CommandList* CmdLists[1] = {D3DCmdList2.GetReference()};
			D3DCmdQueue->ExecuteCommandLists(1, CmdLists);
		});
	}
};


#ifdef ELECTRA_HAVE_DX11
struct FRHICommandCopyResourceDX11 final : public FRHICommand<FRHICommandCopyResourceDX11>
{
	TRefCountPtr<ID3D11Texture2D> SampleTexture;
	FTexture2DRHIRef SampleDestinationTexture;
	bool bCrossDevice;

	FRHICommandCopyResourceDX11(ID3D11Texture2D* InSampleTexture, FRHITexture2D* InSampleDestinationTexture, bool bInCrossDevice)
		: SampleTexture(InSampleTexture)
		, SampleDestinationTexture(InSampleDestinationTexture)
		, bCrossDevice(bInCrossDevice)
	{
	}

	void Execute(FRHICommandListBase& CmdList)
	{
		LLM_SCOPE(ELLMTag::MediaStreaming);
		ID3D11Device* D3D11Device = static_cast<ID3D11Device*>(GDynamicRHI->RHIGetNativeDevice());
		ID3D11DeviceContext* D3D11DeviceContext = nullptr;

		D3D11Device->GetImmediateContext(&D3D11DeviceContext);
		if (D3D11DeviceContext)
		{
			ID3D11Resource* DestinationTexture = reinterpret_cast<ID3D11Resource*>(SampleDestinationTexture->GetNativeResource());
			if (DestinationTexture)
			{
				if (bCrossDevice)
				{
					TRefCountPtr<IDXGIResource> OtherResource(nullptr);
					SampleTexture->QueryInterface(__uuidof(IDXGIResource), (void**)OtherResource.GetInitReference());

					if (OtherResource)
					{
						//
						// Copy shared texture from decoder device to render device
						//
						HANDLE SharedHandle = nullptr;
						if (OtherResource->GetSharedHandle(&SharedHandle) == S_OK)
						{
							if (SharedHandle != 0)
							{
								TRefCountPtr<ID3D11Resource> SharedResource;
								D3D11Device->OpenSharedResource(SharedHandle, __uuidof(ID3D11Texture2D), (void**)SharedResource.GetInitReference());

								if (SharedResource)
								{
									TRefCountPtr<IDXGIKeyedMutex> KeyedMutex;
									SharedResource->QueryInterface(_uuidof(IDXGIKeyedMutex), (void**)KeyedMutex.GetInitReference());

									if (KeyedMutex)
									{
										// Key is 1 : Texture as just been updated
										// Key is 2 : Texture as already been updated.
										// Do not wait to acquire key 1 since there is race no condition between writer and reader.
										if (KeyedMutex->AcquireSync(1, 0) == S_OK)
										{
											// Copy from shared texture of FSink device to Rendering device
											D3D11DeviceContext->CopyResource(DestinationTexture, SharedResource);
											KeyedMutex->ReleaseSync(2);
										}
										else
										{
											// If key 1 cannot be acquired, another reader is already copying the resource
											// and will release key with 2. 
											// Wait to acquire key 2.
											if (KeyedMutex->AcquireSync(2, INFINITE) == S_OK)
											{
												KeyedMutex->ReleaseSync(2);
											}
										}
									}
								}
							}
						}
					}
				}
				else
				{
					//
					// Simple copy on render device
					//
					D3D11DeviceContext->CopyResource(DestinationTexture, SampleTexture);
				}
			}
			D3D11DeviceContext->Release();
		}
	}
};
#endif

/**
 * "Converter" for textures - here: a copy from the decoder owned texture (possibly in another device) into a RHI one (as prep for the real conversion to RGB etc.)
 */
bool FElectraTextureSample::Convert(FTexture2DRHIRef& InDstTexture, const FConversionHints& Hints)
{
	LLM_SCOPE(ELLMTag::MediaStreaming);

	check(IsInRenderingThread());

	FRHICommandListImmediate& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();

	SCOPED_DRAW_EVENT(RHICmdList, WinMediaOutputConvertTexture);
	SCOPED_GPU_STAT(RHICmdList, MediaWinDecoder_Convert);

	// Get actual sample dimensions
	FIntPoint Dim = VideoDecoderOutput->GetDim();

	// Is this YCoCg data?
	if (SampleFormat == EMediaTextureSampleFormat::YCoCg_DXT5_Alpha_BC4)
	{
		check(!VideoDecoderOutputPC->GetTexture());
		check(!"EMediaTextureSampleFormat::YCoCg_DXT5_Alpha_BC4 special conversion code not yet implemented!");
		return false;
	}

	// Note: the converter is not used at all if we have texture data in a SW buffer!

	bool bCrossDeviceCopy;
	EPixelFormat Format;
	if (VideoDecoderOutputPC->GetOutputType() != FVideoDecoderOutputPC::EOutputType::HardwareWin8Plus)
	{
		if (VideoDecoderOutputPC->GetOutputType() != FVideoDecoderOutputPC::EOutputType::Hardware_DX)
		{
			//
			// SW decoder has decoded into a HW texture (not known to RHI) -> copy it into an RHI one
			//
			check(VideoDecoderOutputPC->GetOutputType() == FVideoDecoderOutputPC::EOutputType::SoftwareWin8Plus);
			check(VideoDecoderOutput->GetFormat() == EPixelFormat::PF_NV12);

			Format = EPixelFormat::PF_G8; // use fixed format: we flag this as NV12, too - as it is - but DX11 will only support a "higher than normal G8 texture" (with specialized access in shader)
			bCrossDeviceCopy = false;
		}
		else
		{
			// We have a texture on the rendering device
			Format = (VideoDecoderOutput->GetFormat() == PF_NV12) ? PF_G8 : PF_G16; // return a texture, representing the whole NV12/P010 layout as a single texture (1.5 height)
			bCrossDeviceCopy = false;
		}
	}
	else
	{
		//
		// HW decoder has delivered a texture (this is already a copy) which is on its own device. Copy into one created by RHI (and hence on our rendering device)
		//

		// note: on DX platforms we won't get any SRV generated for NV12 -> so any user needs to do that manually! (as they please: R8, R8G8...)
		Format = VideoDecoderOutput->GetFormat();
		bCrossDeviceCopy = true;
	}

	// Do we need a new RHI texture?
	if (!Texture.IsValid() || Texture->GetSizeX() != Dim.X || Texture->GetSizeY() != Dim.Y)
	{
		const FRHITextureCreateDesc Desc =
			FRHITextureCreateDesc::Create2D(TEXT("FElectraTextureSample"), Dim, Format)
			.SetFlags(ETextureCreateFlags::Dynamic | ((bCanUseSRGB && IsOutputSrgb()) ? ETextureCreateFlags::SRGB : ETextureCreateFlags::None));

		Texture = RHICreateTexture(Desc);
	}

	uint64 SyncValue = 0;
	TRefCountPtr<IUnknown> TextureCommon = VideoDecoderOutputPC->GetTexture();
	TRefCountPtr<IUnknown> SyncCommon = VideoDecoderOutputPC->GetSync(SyncValue);

#ifdef ELECTRA_HAVE_DX11
	// DX11 texture?
	TRefCountPtr<ID3D11Texture2D> TextureDX11;
	HRESULT Res = TextureCommon->QueryInterface(__uuidof(ID3D11Texture2D), (void**)TextureDX11.GetInitReference());
	if (Res == S_OK)
	{
		check(RHIGetInterfaceType() == ERHIInterfaceType::D3D11);

		// Copy data into RHI texture
		if (RHICmdList.Bypass())
		{
			FRHICommandCopyResourceDX11 Cmd(TextureDX11, Texture, bCrossDeviceCopy);
			Cmd.Execute(RHICmdList);
		}
		else
		{
			new (RHICmdList.AllocCommand<FRHICommandCopyResourceDX11>()) FRHICommandCopyResourceDX11(TextureDX11, Texture, bCrossDeviceCopy);
		}
	}
	else
#endif
	{
		// No. Then it better be a DX12 resource...
		TRefCountPtr<ID3D12Resource> TextureDX12;
		Res = TextureCommon->QueryInterface(__uuidof(ID3D12Resource), (void**)TextureDX12.GetInitReference());
		check(Res == S_OK);
		if (Res != S_OK)
		{
			return true;
		}
	
		check(RHIGetInterfaceType() == ERHIInterfaceType::D3D12);

		TRefCountPtr<ID3D12Fence> D3DFence;
		if (SyncCommon)
		{
			Res = SyncCommon->QueryInterface(__uuidof(ID3D12Fence), (void**)D3DFence.GetInitReference());
			check(Res == S_OK);
			if (Res != S_OK)
			{
				return true;
			}
		}

		// Copy data into RHI texture
		if (RHICmdList.Bypass())
		{
			FRHICommandCopyResourceDX12 Cmd(TextureDX12, Texture, D3DCmdAllocator, D3DCmdList, D3DFence, SyncValue);
			Cmd.Execute(RHICmdList);
		}
		else
		{
			new (RHICmdList.AllocCommand<FRHICommandCopyResourceDX12>()) FRHICommandCopyResourceDX12(TextureDX12, Texture, D3DCmdAllocator, D3DCmdList, D3DFence, SyncValue);
		}
	}
	return true;
}


const void* FElectraTextureSample::GetBuffer()
{
	if (VideoDecoderOutput)
	{
		return VideoDecoderOutputPC->GetBuffer().GetData();
	}
	return nullptr;
}


uint32 FElectraTextureSample::GetStride() const
{
	if (VideoDecoderOutput)
	{
		return VideoDecoderOutputPC->GetStride();
	}
	return 0;
}


IMFSample* FElectraTextureSample::GetMFSample()
{
	if (VideoDecoderOutput)
	{
		return VideoDecoderOutputPC->GetMFSample().GetReference();
	}
	return nullptr;
}

#endif
