// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreTypes.h"

#if PLATFORM_MAC || PLATFORM_IOS

#include "ElectraTextureSample.h"

#if WITH_ENGINE
#include "RenderingThread.h"
#include "RHI.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "RHIStaticStates.h"
#include "MediaShaders.h"
#include "Containers/ResourceArray.h"
#include "PipelineStateCache.h"
#else
#include "Containers/Array.h"
#endif

#if WITH_ENGINE
extern void SafeReleaseMetalObject(id Object);
#endif

// ------------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------------

FElectraMediaTexConvApple::FElectraMediaTexConvApple()
{
#if WITH_ENGINE
	MetalTextureCache = nullptr;
#endif
}

FElectraMediaTexConvApple::~FElectraMediaTexConvApple()
{
#if WITH_ENGINE
	if (MetalTextureCache)
	{
		CVMetalTextureCacheRef TextureCacheCopy = MetalTextureCache;
		SafeReleaseMetalObject((id)TextureCacheCopy);
	}
#endif
}

#if WITH_ENGINE
namespace
{

/**
 * Passes a CV*TextureRef or CVPixelBufferRef through to the RHI to wrap in an RHI texture without traversing system memory.
 */
class FTexConvTexResourceWrapper
	: public FResourceBulkDataInterface
{
public:

	FTexConvTexResourceWrapper(CFTypeRef InImageBuffer)
		: ImageBuffer(InImageBuffer)
	{
		check(ImageBuffer);
		CFRetain(ImageBuffer);
	}

	virtual ~FTexConvTexResourceWrapper()
	{
		CFRelease(ImageBuffer);
		ImageBuffer = nullptr;
	}

public:

	//~ FResourceBulkDataInterface interface

	virtual void Discard() override
	{
		delete this;
	}

	virtual const void* GetResourceBulkData() const override
	{
		return ImageBuffer;
	}

	virtual uint32 GetResourceBulkDataSize() const override
	{
		return ImageBuffer ? ~0u : 0;
	}

	virtual EBulkDataType GetResourceType() const override
	{
		return EBulkDataType::MediaTexture;
	}

	CFTypeRef ImageBuffer;
};


/**
 * Allows for direct GPU mem allocation for texture resource from a CVImageBufferRef's system memory backing store.
 */
class FTexConvTexResourceMemory
	: public FResourceBulkDataInterface
{
public:
	FTexConvTexResourceMemory(CVImageBufferRef InImageBuffer)
	: ImageBuffer(InImageBuffer)
	{
		check(ImageBuffer);
		CFRetain(ImageBuffer);
	}

	/**
	 * @return ptr to the resource memory which has been preallocated
	 */
	virtual const void* GetResourceBulkData() const override
	{
		CVPixelBufferLockBaseAddress(ImageBuffer, kCVPixelBufferLock_ReadOnly);
		return CVPixelBufferGetBaseAddress(ImageBuffer);
	}

	/**
	 * @return size of resource memory
	 */
	virtual uint32 GetResourceBulkDataSize() const override
	{
		int32 Pitch = CVPixelBufferGetBytesPerRow(ImageBuffer);
		int32 Height = CVPixelBufferGetHeight(ImageBuffer);
		uint32 Size = (Pitch * Height);

		return Size;
	}

	/**
	 * Free memory after it has been used to initialize RHI resource
	 */
	virtual void Discard() override
	{
		CVPixelBufferUnlockBaseAddress(ImageBuffer, kCVPixelBufferLock_ReadOnly);
		delete this;
	}

	virtual ~FTexConvTexResourceMemory()
	{
		CFRelease(ImageBuffer);
		ImageBuffer = nullptr;
	}

	CVImageBufferRef ImageBuffer;
};

} // namespace anonymous


void FElectraMediaTexConvApple::ConvertTexture(FTexture2DRHIRef & InDstTexture, CVImageBufferRef InImageBufferRef, bool bFullRange, EMediaTextureSampleFormat Format, const FMatrix44f& YUVMtx, const FMatrix44f& GamutToXYZMtx, UE::Color::EEncoding EncodingType)
{
	check(IsInRenderingThread());

	const int32 FrameHeight = CVPixelBufferGetHeight(InImageBufferRef);
	const int32 FrameWidth = CVPixelBufferGetWidth(InImageBufferRef);
	const int32 FrameStride = CVPixelBufferGetBytesPerRow(InImageBufferRef);

	TRefCountPtr<FRHITexture2D> ShaderResource;

	// We have to support Metal for this object now
	check(COREVIDEO_SUPPORTS_METAL);
	check(IsMetalPlatform(GMaxRHIShaderPlatform));
	{
		if (!MetalTextureCache)
		{
			id<MTLDevice> Device = (id<MTLDevice>)GDynamicRHI->RHIGetNativeDevice();
			check(Device);

			CVReturn Return = CVMetalTextureCacheCreate(kCFAllocatorDefault, nullptr, Device, nullptr, &MetalTextureCache);
			check(Return == kCVReturnSuccess);
		}
		check(MetalTextureCache);

		if (CVPixelBufferIsPlanar(InImageBufferRef))
		{
			// Planar data: YbCrCb 420 etc. (NV12 / P010)

			bool bIs8Bit = (Format == EMediaTextureSampleFormat::CharNV12);

			const FMatrix* ColorTransform = bFullRange ? &MediaShaders::YuvToRgbRec709Unscaled : &MediaShaders::YuvToRgbRec709Scaled;
			FVector Off = bFullRange ? MediaShaders::YUVOffsetNoScale8bits : MediaShaders::YUVOffset8bits;

			// Expecting BiPlanar kCVPixelFormatType_420YpCbCr8BiPlanar Full/Video
			check(CVPixelBufferGetPlaneCount(InImageBufferRef) == 2);

			int32 YWidth = CVPixelBufferGetWidthOfPlane(InImageBufferRef, 0);
			int32 YHeight = CVPixelBufferGetHeightOfPlane(InImageBufferRef, 0);

			CVMetalTextureRef YTextureRef = nullptr;
			CVReturn Result = CVMetalTextureCacheCreateTextureFromImage(kCFAllocatorDefault, MetalTextureCache, InImageBufferRef, nullptr, bIs8Bit ? MTLPixelFormatR8Unorm : MTLPixelFormatR16Unorm, YWidth, YHeight, 0, &YTextureRef);
			check(Result == kCVReturnSuccess);
			check(YTextureRef);

			int32 UVWidth = CVPixelBufferGetWidthOfPlane(InImageBufferRef, 1);
			int32 UVHeight = CVPixelBufferGetHeightOfPlane(InImageBufferRef, 1);

			CVMetalTextureRef UVTextureRef = nullptr;
			Result = CVMetalTextureCacheCreateTextureFromImage(kCFAllocatorDefault, MetalTextureCache, InImageBufferRef, nullptr, bIs8Bit ? MTLPixelFormatRG8Unorm : MTLPixelFormatRG16Unorm, UVWidth, UVHeight, 1, &UVTextureRef);
			check(Result == kCVReturnSuccess);
			check(UVTextureRef);

			// Metal can upload directly from an IOSurface to a 2D texture, so we can just wrap it.

			const FRHITextureCreateDesc YDesc =
				FRHITextureCreateDesc::Create2D(TEXT("YTex"), YWidth, YHeight, bIs8Bit ? PF_G8 : PF_G16)
				.SetFlags(ETextureCreateFlags::Dynamic | ETextureCreateFlags::NoTiling | ETextureCreateFlags::ShaderResource)
				.SetBulkData(new FTexConvTexResourceWrapper(YTextureRef));

			const FRHITextureCreateDesc UVDesc =
				FRHITextureCreateDesc::Create2D(TEXT("UVTex"), UVWidth, UVHeight, bIs8Bit ? PF_R8G8 : PF_G16R16)
				.SetFlags(ETextureCreateFlags::Dynamic | ETextureCreateFlags::NoTiling | ETextureCreateFlags::ShaderResource)
				.SetBulkData(new FTexConvTexResourceWrapper(UVTextureRef));

			TRefCountPtr<FRHITexture> YTex = RHICreateTexture(YDesc);
			TRefCountPtr<FRHITexture> UVTex = RHICreateTexture(UVDesc);

			// render video frame into sink texture
			FRHICommandListImmediate& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();
			{
				// configure media shaders
				auto GlobalShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);

				FRHIRenderPassInfo RPInfo(InDstTexture, ERenderTargetActions::Load_Store);
				RHICmdList.BeginRenderPass(RPInfo, TEXT("AvfMediaSampler"));
				{
					FGraphicsPipelineStateInitializer GraphicsPSOInit;
					RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

					GraphicsPSOInit.BlendState = TStaticBlendStateWriteMask<CW_RGBA, CW_NONE, CW_NONE, CW_NONE, CW_NONE, CW_NONE, CW_NONE, CW_NONE>::GetRHI();
					GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
					GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
					GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GMediaVertexDeclaration.VertexDeclarationRHI;
					GraphicsPSOInit.PrimitiveType = PT_TriangleStrip;

					if (Format == EMediaTextureSampleFormat::CharNV12)
					{
						//
						// NV12
						// (this disrespects any "working color space")
						//

						TShaderMapRef<FMediaShadersVS> VertexShader(GlobalShaderMap);
						TShaderMapRef<FYCbCrConvertPS> PixelShader(GlobalShaderMap);

						PixelShader->SetParameters(RHICmdList, YTex, UVTex, *ColorTransform, Off, true);

						GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
						GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
					}
					else
					{
						//
						// P010
						//

						// Setup conversion from Rec2020 to current working color space
						const UE::Color::FColorSpace& Working = UE::Color::FColorSpace::GetWorking();
						FMatrix44f ColorSpaceMtx = UE::Color::Transpose<float>(Working.GetXYZToRgb()) * GamutToXYZMtx;
						// Normalize output (e.g. 80 or 100 nits == 1.0)
						ColorSpaceMtx = ColorSpaceMtx.ApplyScale(kMediaSample_HDR_NitsNormalizationFactor);

						// Get shaders.
						TShaderMapRef<FP010ConvertPS> PixelShader(GlobalShaderMap);
						TShaderMapRef<FMediaShadersVS> VertexShader(GlobalShaderMap);

						FShaderResourceViewRHIRef Y_SRV = RHICreateShaderResourceView(YTex, 0, 1, PF_G16);
						FShaderResourceViewRHIRef UV_SRV = RHICreateShaderResourceView(UVTex, 0, 1, PF_G16R16);

						// Update shader uniform parameters.
						PixelShader->SetParameters(RHICmdList, { YWidth, YHeight }, Y_SRV, UV_SRV, { YWidth, YHeight }, YUVMtx, ColorSpaceMtx, EncodingType == UE::Color::EEncoding::ST2084);

						GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
						GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
					}

					SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);

					FBufferRHIRef VertexBuffer = CreateTempMediaVertexBuffer();
					RHICmdList.SetStreamSource(0, VertexBuffer, 0);
					RHICmdList.SetViewport(0, 0, 0.0f, YWidth, YHeight, 1.0f);

					RHICmdList.DrawPrimitive(0, 2, 1);
				}
				RHICmdList.EndRenderPass();
				RHICmdList.Transition(FRHITransitionInfo(ShaderResource, ERHIAccess::RTV, ERHIAccess::SRVMask));
			}
			CFRelease(YTextureRef);
			CFRelease(UVTextureRef);
		}
		else
		{
			//
			// sRGB
			//

			//
			// Grab data directly from image buffer reference
			// (this will create the output texture here - but will always use the settings here, as data is not converted)
			//
			int32 Width = CVPixelBufferGetWidth(InImageBufferRef);
			int32 Height = CVPixelBufferGetHeight(InImageBufferRef);

			CVMetalTextureRef TextureRef = nullptr;
			CVReturn Result = CVMetalTextureCacheCreateTextureFromImage(kCFAllocatorDefault, MetalTextureCache, InImageBufferRef, nullptr, MTLPixelFormatBGRA8Unorm_sRGB, Width, Height, 0, &TextureRef);
			check(Result == kCVReturnSuccess);
			check(TextureRef);

			const FRHITextureCreateDesc Desc =
				FRHITextureCreateDesc::Create2D(TEXT("DstTexture"), Width, Height, PF_B8G8R8A8)
				.SetFlags(ETextureCreateFlags::SRGB | ETextureCreateFlags::Dynamic | ETextureCreateFlags::NoTiling | ETextureCreateFlags::ShaderResource)
				.SetBulkData(new FTexConvTexResourceWrapper(TextureRef));

			InDstTexture = RHICreateTexture(Desc);

			CFRelease(TextureRef);
		}
	}
}
#endif

// ------------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------------

void FElectraTextureSample::Initialize(FVideoDecoderOutput* InVideoDecoderOutput)
{
	IElectraTextureSampleBase::Initialize(InVideoDecoderOutput);
	VideoDecoderOutputApple = static_cast<FVideoDecoderOutputApple*>(InVideoDecoderOutput);
}


EMediaTextureSampleFormat FElectraTextureSample::GetFormat() const
{
	OSType PixelFormat = CVPixelBufferGetPixelFormatType(VideoDecoderOutputApple->GetImageBuffer());
	return ((PixelFormat == kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange) || (PixelFormat == kCVPixelFormatType_420YpCbCr8BiPlanarFullRange)) ? EMediaTextureSampleFormat::CharNV12 : EMediaTextureSampleFormat::P010;
}

uint32 FElectraTextureSample::GetStride() const
{
	if (VideoDecoderOutput)
	{
		return VideoDecoderOutputApple->GetStride();
	}
	return 0;
}

uint32 FElectraTextureSample::GetConverterInfoFlags() const
{
	if (VideoDecoderOutput)
	{
		return CVPixelBufferIsPlanar(VideoDecoderOutputApple->GetImageBuffer()) ? ConverterInfoFlags_Default : ConverterInfoFlags_WillCreateOutputTexture;
	}
	return ConverterInfoFlags_Default;
}

bool FElectraTextureSample::Convert(FTexture2DRHIRef & InDstTexture, const FConversionHints & Hints)
{
	if (VideoDecoderOutput)
	{
		if (TSharedPtr<FElectraMediaTexConvApple, ESPMode::ThreadSafe> PinnedTexConv = TexConv.Pin())
		{
			PinnedTexConv->ConvertTexture(InDstTexture, VideoDecoderOutputApple->GetImageBuffer(), VideoDecoderOutput->GetColorimetry()->GetMPEGDefinition()->VideoFullRangeFlag != 0, GetFormat(), GetSampleToRGBMatrix(), GetGamutToXYZMatrix(), GetEncodingType());
			return true;
		}
	}
	return false;
}

#endif
