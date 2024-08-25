// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"

#if PLATFORM_WINDOWS && !UE_SERVER

#include "ElectraTextureSample.h"

#include "ProfilingDebugging/RealtimeGPUProfiler.h"
#include "RenderUtils.h"

#include "ID3D12DynamicRHI.h"

#include "Windows/AllowWindowsPlatformTypes.h"
THIRD_PARTY_INCLUDES_START
#ifdef ELECTRA_HAVE_DX11
#include "D3D11State.h"
#include "D3D11Resources.h"
#endif
#include "d3d12.h"
THIRD_PARTY_INCLUDES_END
#include "Windows/HideWindowsPlatformTypes.h"


/*
	Short summary of how we get data:

	- Win10+ (HW decode is used at all times if handling H.264/5)
	-- DX11:   we receive data in GPU space as NV12/P010 texture
	-- DX12:   we receive data in CPU(yes) space as NV12/P010 texture -=UNLESS=- we have SDK 22621+ and suitable runtime support -> DX12 texture in GPU space
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
	VideoDecoderOutputPC = static_cast<FVideoDecoderOutputPC*>(InVideoDecoderOutput);
	IElectraTextureSampleBase::Initialize(InVideoDecoderOutput);

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

	EMediaTextureSampleFormat SampleFmt = GetFormat();
	bCanUseSRGB = (SampleFmt == EMediaTextureSampleFormat::CharBGRA ||
				   SampleFmt == EMediaTextureSampleFormat::CharRGBA ||
				   SampleFmt == EMediaTextureSampleFormat::CharBMP ||
				   SampleFmt == EMediaTextureSampleFormat::DXT1 ||
				   SampleFmt == EMediaTextureSampleFormat::DXT5);

	// Get rid of older textures if we switched around to CPU side buffers (which might happen if a single player switches between decoders)
	// (note: we do not care about the CPU side buffers as we do not keep a reference on that resource)
	if (Texture.IsValid() && (VideoDecoderOutputPC->GetTexture() == nullptr))
	{
		Texture = nullptr;
	}
}


#if !UE_SERVER
void FElectraTextureSample::ShutdownPoolable()
{
	// If we use DX12 with texture data, we use the RHI texture only to refer to the actual "native" D3D resource we created -> get rid of this "alias" as soon as we retire to the pool
//OPT: WE COULD TRACK IF WE STILL HAVE THE SAME NATIVE TEXTURE AND ATTEMPT TO REUSE THIS
	if (VideoDecoderOutputPC->GetTexture() && (RHIGetInterfaceType() == ERHIInterfaceType::D3D12))
	{
		Texture = nullptr;
	}
	IElectraTextureSampleBase::ShutdownPoolable();
}
#endif


#if WITH_ENGINE
FRHITexture* FElectraTextureSample::GetTexture() const
{
	check(IsInRenderingThread());

	// Is this DX12?
	if (RHIGetInterfaceType() == ERHIInterfaceType::D3D12)
	{
		// Yes, see if we have a real D3D resource...
		if (TRefCountPtr<IUnknown> TextureCommon = VideoDecoderOutputPC->GetTexture())
		{
			// Yes. Do we need a new RHI texture?
			FIntPoint Dim = VideoDecoderOutput->GetDim();
			if (!Texture.IsValid() || Texture->GetSizeX() != Dim.X || Texture->GetSizeY() != Dim.Y)
			{
				// Yes...
				TRefCountPtr<ID3D12Resource> TextureDX12;
				HRESULT Res = TextureCommon->QueryInterface(__uuidof(ID3D12Resource), (void**)TextureDX12.GetInitReference());
				check(Res == S_OK);
				if (Res == S_OK)
				{
					// Setup a suitable RHI texture (it will also become an additional owner of the data)
					ETextureCreateFlags Flags = ETextureCreateFlags::ShaderResource;
					if (IsOutputSrgb() && bCanUseSRGB)
					{
						Flags = Flags | ETextureCreateFlags::SRGB;
					}
					Texture = static_cast<ID3D12DynamicRHI*>(GDynamicRHI)->RHICreateTexture2DFromResource(VideoDecoderOutput->GetFormat(), Flags, FClearValueBinding(), TextureDX12);
				}
			}
		}
	}

	return Texture;
}
#endif //WITH_ENGINE


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


	bool bHasTexture = !!VideoDecoderOutputPC->GetTexture();

	// Get actual sample dimensions
	FIntPoint Dim = VideoDecoderOutput->GetDim();

	// Is this YCoCg data?
	if (SampleFormat == EMediaTextureSampleFormat::YCoCg_DXT5_Alpha_BC4)
	{
		check(!VideoDecoderOutputPC->GetTexture());
		check(!"EMediaTextureSampleFormat::YCoCg_DXT5_Alpha_BC4 special conversion code not yet implemented!");
		return false;
	}

	// If we have a texture and D3D12 is active, we will not need to do any custom conversion work, but we will need a sync!
	if (bHasTexture && (RHIGetInterfaceType() == ERHIInterfaceType::D3D12))
	{
		uint64 SyncFenceValue;
		TRefCountPtr<IUnknown> SyncCommon = VideoDecoderOutputPC->GetSync(SyncFenceValue);
		if (SyncCommon)
		{
			//
			// Synchronize with sample data copy on copy queue
			//
			TRefCountPtr<ID3D12Fence> SyncFence;
			HRESULT Res = SyncCommon->QueryInterface(__uuidof(ID3D12Fence), (void**)SyncFence.GetInitReference());
			check(SUCCEEDED(Res));
			if (Res == S_OK)
			{
				RHICmdList.EnqueueLambda([SyncFence, SyncFenceValue](FRHICommandList& RHICmdList)
				{
					GetID3D12DynamicRHI()->RHIWaitManualFence(RHICmdList, SyncFence, SyncFenceValue);
				});
			}
		}
		return true;
	}

	// Note: the converter code (from here on) is not used at all if we have texture data in a SW buffer!
	check(bHasTexture);

	bool bCrossDeviceCopy;
	EPixelFormat Format;
	if (VideoDecoderOutputPC->GetOutputType() != FVideoDecoderOutputPC::EOutputType::HardwareWin8Plus)
	{
		// Below case deliver DX11 textures on the rendering device, so it would be feasible to create an RHI texture from them directly.
		// The current code instead uses a copy of the data to populate an RHI texture. As this is DX11 only and is not the code path
		// affecting main line hardware decoders (H.264/5), we do currently NOT optimize for this.

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
			Format = VideoDecoderOutput->GetFormat();
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
		// We really only expect to be called for DX11
		check("Unexpected call to conversion method from another RHI than the D3D11 one!");
		return false;
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


float FElectraTextureSample::GetSampleDataScale(bool b10Bit) const
{
	if (VideoDecoderOutput)
	{
		auto Value = VideoDecoderOutputPC->GetDict().GetValue(IDecoderOutputOptionNames::PixelDataScale);
		if (Value.IsValid() && Value.IsType(Electra::FVariantValue::EDataType::TypeDouble))
		{
			return (float)Value.GetDouble();
		}
	}
	return 1.0f;
}

#endif
