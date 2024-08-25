// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/MediaTextureResource.h"
#include "MediaAssetsPrivate.h"
#include "Modules/ModuleManager.h"
#include "DeviceProfiles/DeviceProfile.h"
#include "DeviceProfiles/DeviceProfileManager.h"
#include "ExternalTexture.h"
#include "IMediaModule.h"
#include "IMediaClock.h"
#include "IMediaPlayer.h"
#include "IMediaSamples.h"
#include "IMediaTextureSample.h"
#include "IMediaTextureSampleConverter.h"
#include "MediaDelegates.h"
#include "MediaPlayerFacade.h"
#include "MediaSampleSource.h"
#include "MediaShaders.h"
#include "PipelineStateCache.h"
#include "RenderGraphUtils.h"
#include "SceneUtils.h"
#include "Shader.h"
#include "StaticBoundShaderState.h"
#include "RenderUtils.h"
#include "RHIStaticStates.h"
#include "GenerateMips.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "Async/Async.h"


#include "HDRHelper.h"

#include "MediaTexture.h"

#if PLATFORM_ANDROID
# include "Android/AndroidPlatformMisc.h"
#endif

/** Time spent in media player facade closing media. */
DECLARE_CYCLE_STAT(TEXT("MediaAssets MediaTextureResource Render"), STAT_MediaAssets_MediaTextureResourceRender, STATGROUP_Media);

/** Sample time of texture last rendered. */
DECLARE_FLOAT_COUNTER_STAT(TEXT("MediaAssets MediaTextureResource Sample"), STAT_MediaUtils_TextureSampleTime, STATGROUP_Media);

CSV_DECLARE_CATEGORY_MODULE_EXTERN(MEDIA_API, MediaStreaming);

DECLARE_GPU_STAT_NAMED(MediaTextureResource, TEXT("MediaTextureResource"));

static const FMatrix44f SplatMtx = { {1.0f, 0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 0.0f} };

/* GPU data deletion helper
 *****************************************************************************/

template<typename ObjectRefType> struct TGPUsyncedDataDeleter : public IMediaClockSink
{
	static TSharedRef<TGPUsyncedDataDeleter<ObjectRefType>, ESPMode::ThreadSafe> Create()
	{
		auto Ret = MakeShared<TGPUsyncedDataDeleter<ObjectRefType>, ESPMode::ThreadSafe>();
		Ret->WeakThis = Ret;
		return Ret;
	}

	virtual ~TGPUsyncedDataDeleter()
	{
		// See if all samples are ready to be retired now...
		if (!Update())
		{
			// They are. No need for any async task...
			return;
		}

		// Start having us ticked so we can retire the rest over the next game loop iterations...
		IMediaModule* MediaModule = FModuleManager::GetModulePtr<IMediaModule>(TEXT("MediaModule"));
		if (MediaModule)
		{
			MediaModule->GetClock().AddSink(StaticCastSharedRef<IMediaClockSink, TGPUsyncedDataDeleter<ObjectRefType>, ESPMode::ThreadSafe>(WeakThis.Pin().ToSharedRef()));
		}
	}

	void Retire(const ObjectRefType& Object)
	{
		// Prep "retirement package"
		FRetiringObjectInfo Info;
		Info.Object = Object;
		Info.GPUFence = RHICreateGPUFence(TEXT("MediaTextureResourceReuseFence"));
		Info.RetireTime = FPlatformTime::Seconds();

		// Insert fence. We assume that GPU-workload-wise this marks the spot usage of the sample is done
		FRHICommandListExecutor::GetImmediateCommandList().WriteGPUFence(Info.GPUFence);

		// Recall for later checking...
		FScopeLock Lock(&CS);
		Objects.Push(Info);
	}

	bool Update()
	{
		FScopeLock Lock(&CS);

		// Check for any retired samples that are not done being touched by the GPU...
		int32 Idx = 0;
		for (; Idx < Objects.Num(); ++Idx)
		{
			// Either no fence present or the fence has been signaled?
			if (Objects[Idx].GPUFence.IsValid() && !Objects[Idx].GPUFence->Poll())
			{
				// No. This one is still busy, we can stop...
				break;
			}
		}
		// Remove (hence return to the pool / free up fence) all the finished ones...
		if (Idx != 0)
		{
			Objects.RemoveAt(0, Idx);
		}
		return Objects.Num() != 0;
	}

private:
	virtual void TickInput(FTimespan DeltaTime, FTimespan Timecode) override
	{
		// Check what data is still not signaled...
		if (!Update())
		{
			// All is gone! Remove reference to this (which will delete this instance)
			IMediaModule* MediaModule = FModuleManager::GetModulePtr<IMediaModule>(TEXT("MediaModule"));
			if (MediaModule)
			{
				MediaModule->GetClock().RemoveSink(StaticCastSharedRef<IMediaClockSink, TGPUsyncedDataDeleter<ObjectRefType>, ESPMode::ThreadSafe>(WeakThis.Pin().ToSharedRef()));
			}
		}
	}

	struct FRetiringObjectInfo
	{
		ObjectRefType Object;
		FGPUFenceRHIRef GPUFence;
		double RetireTime;
	};

	TWeakPtr<TGPUsyncedDataDeleter<ObjectRefType>, ESPMode::ThreadSafe> WeakThis;
	TArray<FRetiringObjectInfo> Objects;
	FCriticalSection CS;
};

struct FPriorSamples : public TGPUsyncedDataDeleter<TSharedPtr<IMediaTextureSample, ESPMode::ThreadSafe>>
{
	using BaseType = TGPUsyncedDataDeleter<TSharedPtr<IMediaTextureSample, ESPMode::ThreadSafe>>;

	static TSharedRef<FPriorSamples, ESPMode::ThreadSafe> Create()
	{
		return StaticCastSharedRef<FPriorSamples, BaseType, ESPMode::ThreadSafe>(BaseType::Create());
	}
};

/* Local helpers
 *****************************************************************************/

namespace MediaTextureResourceHelpers
{
	/**
	 * Get the pixel format for a given sample.
	 *
	 * @param Sample The sample.
	 * @return The sample's pixel format.
	 */
	EPixelFormat GetPixelFormat(const TSharedPtr<IMediaTextureSample, ESPMode::ThreadSafe>& Sample)
	{
		switch (Sample->GetFormat())
		{
		case EMediaTextureSampleFormat::CharAYUV:
		case EMediaTextureSampleFormat::CharBGRA:
		case EMediaTextureSampleFormat::CharBMP:
		case EMediaTextureSampleFormat::CharUYVY:
		case EMediaTextureSampleFormat::CharYUY2:
		case EMediaTextureSampleFormat::Char2VUY:
		case EMediaTextureSampleFormat::CharYVYU:
			return PF_B8G8R8A8;

		case EMediaTextureSampleFormat::CharRGBA:
			return PF_R8G8B8A8;

		case EMediaTextureSampleFormat::CharNV12:
		case EMediaTextureSampleFormat::CharNV21:
			return PF_G8; // note: right now this case will be encountered only if CPU-side data in NV12/21 format is in sample -> in this case we cannot create a true NV12 texture OR the platforms view it as U8s anyway 

		case EMediaTextureSampleFormat::FloatRGB:
			return PF_FloatRGB;

		case EMediaTextureSampleFormat::FloatRGBA:
			return PF_FloatRGBA;

		case EMediaTextureSampleFormat::CharBGR10A2:
			return PF_A2B10G10R10;

		case EMediaTextureSampleFormat::ABGR16:
			return PF_A16B16G16R16;

		case EMediaTextureSampleFormat::RGBA16:
			return PF_R16G16B16A16_UNORM;

		case EMediaTextureSampleFormat::ARGB16_BIG:
			return PF_R16G16B16A16_UINT;

		case EMediaTextureSampleFormat::YUVv210:
			return PF_A2B10G10R10;

		case EMediaTextureSampleFormat::YUVv216:
			return PF_A16B16G16R16;

		case EMediaTextureSampleFormat::Y416:
			return PF_A16B16G16R16;

		case EMediaTextureSampleFormat::R4FL:
			return PF_A32B32G32R32F;

		case EMediaTextureSampleFormat::P010:
			return PF_G16; // note: right now this case will be encountered only if CPU-side data in NV12/21 format is in sample -> in this case we cannot create a true P010 texture OR the platforms view it as U16s anyway 

		case EMediaTextureSampleFormat::P010_RGB1010102:
			return PF_A2B10G10R10;

		case EMediaTextureSampleFormat::DXT1:
			return PF_DXT1;

		case EMediaTextureSampleFormat::DXT5:
			return PF_DXT5;

		case EMediaTextureSampleFormat::BC4:
			return PF_BC4;

		case EMediaTextureSampleFormat::YCoCg_DXT5:
			return PF_DXT5;

		default:
			return PF_Unknown;
		}
	}

	bool SupportsComputeMipGen(EPixelFormat InFormat)
	{
		return RHIRequiresComputeGenerateMips() && UE::PixelFormat::HasCapabilities(InFormat, EPixelFormatCapabilities::TypedUAVLoad);
	}

	EPixelFormat GetConvertedPixelFormat(const TSharedPtr<IMediaTextureSample, ESPMode::ThreadSafe>& Sample)
	{
		switch (Sample->GetFormat())
		{
		case EMediaTextureSampleFormat::CharBGR10A2:
		case EMediaTextureSampleFormat::YUVv210:
		{
			if (Sample->GetEncodingType() != UE::Color::EEncoding::Linear)
			{
				return PF_FloatRGB;
			}
			return PF_A2B10G10R10;
		}

		case EMediaTextureSampleFormat::FloatRGB:
		case EMediaTextureSampleFormat::P010:
		case EMediaTextureSampleFormat::YUVv216:
			return PF_FloatRGB;

		case EMediaTextureSampleFormat::Y416:
		case EMediaTextureSampleFormat::R4FL:
		case EMediaTextureSampleFormat::FloatRGBA:
		case EMediaTextureSampleFormat::P010_RGB1010102:
			return PF_FloatRGBA;

		case EMediaTextureSampleFormat::BC4:
			return PF_A8;

		case EMediaTextureSampleFormat::RGBA16:
			return PF_R16G16B16A16_UNORM;

		case EMediaTextureSampleFormat::ABGR16:
		case EMediaTextureSampleFormat::ARGB16_BIG:
			return PF_A16B16G16R16;

		// Everything else maps to 8-bit RGB if it is linear or sRGB (we use sRGB encoding in HW). Otherwise: half floats...
		default:
		{
			if (Sample->GetEncodingType() != UE::Color::EEncoding::Linear && Sample->GetEncodingType() != UE::Color::EEncoding::sRGB)
			{
				return PF_FloatRGB;
			}
			return PF_B8G8R8A8;
		}
		}
	}


	bool RequiresSrgbTexture(const TSharedPtr<IMediaTextureSample, ESPMode::ThreadSafe>& Sample)
	{
		return Sample->IsOutputSrgb();
	}

	bool RequiresUAVTexture(const TSharedPtr<IMediaTextureSample, ESPMode::ThreadSafe>& Sample, uint8 NumMips)
	{	
		//UAV output is needed if mips are required and uses CS to generate
		//or if sample converter asks for it
		const IMediaTextureSampleConverter* Converter = Sample->GetMediaTextureSampleConverter();
		bool bNeedsUAV = (NumMips > 1 && RHIRequiresComputeGenerateMips());
		bNeedsUAV |= (Converter && ((Converter->GetConverterInfoFlags() & IMediaTextureSampleConverter::ConverterInfoFlags_NeedUAVOutputTexture) != 0));
		return bNeedsUAV;
	}


	bool RequiresSrgbInputTexture(const TSharedPtr<IMediaTextureSample, ESPMode::ThreadSafe>& Sample)
	{
		if (!Sample->IsOutputSrgb())
		{
			return false;
		}
		/*
		* Input textures created to receive CPU side buffer sample data are ONLY created with sRGB attributes
		* if we have any 8-bit RGB(A) format. Any YUV (etc.) format does the conversion in SW when applicable in the
		* conversion process!
		*/
		EMediaTextureSampleFormat Fmt = Sample->GetFormat();
		return Fmt == EMediaTextureSampleFormat::CharBGRA ||
			Fmt == EMediaTextureSampleFormat::CharRGBA ||
			Fmt == EMediaTextureSampleFormat::CharBMP ||
			Fmt == EMediaTextureSampleFormat::DXT1 ||
			Fmt == EMediaTextureSampleFormat::DXT5;
	}

} //namespace

/* FMediaTextureResource structors
 *****************************************************************************/

FMediaTextureResource::FMediaTextureResource(UMediaTexture& InOwner, FIntPoint& InOwnerDim, SIZE_T& InOwnerSize, FLinearColor InClearColor, FGuid InTextureGuid, bool InEnableGenMips, uint8 InNumMips, UE::Color::EColorSpace OverrideColorSpaceType)
	: Cleared(false)
	, CurrentClearColor(InClearColor)
	, InitialTextureGuid(InTextureGuid)
	, Owner(InOwner)
	, OwnerDim(InOwnerDim)
	, OwnerSize(InOwnerSize)
	, bEnableGenMips(InEnableGenMips)
	, CurrentNumMips(InEnableGenMips ? InNumMips : 1)
	, CurrentSamplerFilter(ESamplerFilter_Num)
	, CurrentMipMapBias(-1)
	, PriorSamples(FPriorSamples::Create())
{
#if PLATFORM_ANDROID
	bUsesImageExternal = !Owner.NewStyleOutput && (!FAndroidMisc::ShouldUseVulkan() && GSupportsImageExternal);
#else
	bUsesImageExternal = !Owner.NewStyleOutput && GSupportsImageExternal;
#endif
	if (OverrideColorSpaceType != UE::Color::EColorSpace::None)
	{
		OverrideColorSpace.Reset(new UE::Color::FColorSpace(OverrideColorSpaceType));
		check(OverrideColorSpace.IsValid());
	}
}


void FMediaTextureResource::FlushPendingData()
{
	FScopeLock Lock(&PriorSamplesCS);
	PriorSamples = FPriorSamples::Create();
}

/* FMediaTextureResource interface
 *****************************************************************************/

void FMediaTextureResource::Render(const FRenderParams& Params)
{
	check(IsInRenderingThread());
	SCOPED_GPU_STAT(FRHICommandListExecutor::GetImmediateCommandList(), MediaTextureResource);

	LLM_SCOPE(ELLMTag::MediaStreaming);
	SCOPE_CYCLE_COUNTER(STAT_MediaAssets_MediaTextureResourceRender);
	CSV_SCOPED_TIMING_STAT(MediaStreaming, FMediaTextureResource_Render);

	TSharedPtr<FPriorSamples, ESPMode::ThreadSafe> LocalPriorSamples;
	{
	FScopeLock Lock(&PriorSamplesCS);
	LocalPriorSamples = PriorSamples;
	}

	LocalPriorSamples->Update();

	FLinearColor Rotation(1, 0, 0, 1);
	FLinearColor Offset(0, 0, 0, 0);

	TSharedPtr<FMediaTextureSampleSource, ESPMode::ThreadSafe> SampleSource = Params.SampleSource.Pin();

	// Do we either have a classic sample source (queue) or a single, explicit sample to display?
	if (SampleSource.IsValid() || Params.TextureSample.IsValid())
	{
		TSharedPtr<IMediaTextureSample, ESPMode::ThreadSafe> Sample;
		bool UseSample;

		// Yes, it it a queue?
		if (SampleSource.IsValid())
		{
			// Yes, find out what we will display...
			UseSample = false;

			// get the most current sample to be rendered
			TSharedPtr<IMediaTextureSample, ESPMode::ThreadSafe> TestSample;
			while (SampleSource->Peek(TestSample) && TestSample.IsValid())
			{
				const FMediaTimeStamp& StartTime = TestSample->GetTime();
				const FMediaTimeStamp EndTime = StartTime + TestSample->GetDuration();

				if ((Params.Rate >= 0.0f) && (Params.Time < StartTime))
				{
					break; // future sample (forward play)
				}

				if ((Params.Rate <= 0.0f) && (Params.Time >= EndTime))
				{
					break; // future sample (reverse play)
				}

#if UE_MEDIAUTILS_DEVELOPMENT_DELEGATE
				if (UseSample && Sample.IsValid())
				{
					FMediaDelegates::OnSampleDiscarded_RenderThread.Broadcast(&Owner, Sample);
				}
#endif
				UseSample = SampleSource->Dequeue(Sample);
			}
		}
		else
		{
			// We do have an explicit sample to display...
			// (or nothing)
			Sample = Params.TextureSample;
			UseSample = Sample.IsValid();
		}

#if UE_MEDIAUTILS_DEVELOPMENT_DELEGATE
		FMediaDelegates::OnPreSampleRender_RenderThread.Broadcast(&Owner, UseSample, Sample);
#endif
		// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

		const uint8 NumMips = bEnableGenMips ? Params.NumMips : 1;

		// If real "external texture" support is in place and no mips are used the image will "bypass" any of this processing via the GUID-based lookup for "ExternalTextures" and will
		// reach the reading material shader without any processing here...
		// (note that if "new style output" is enabled we will ALWAYS see bUsesImageExternal as FALSE)
		if (UseSample && !(bUsesImageExternal && !bEnableGenMips))
		{
			//
			// Valid sample & Sample should be shown
			//

			bool ConvertOrCopyNeeded = false;

			if (Sample->GetOutputDim().GetMin() <= 0)
			{
				//
				// Sample dimensions are invalid
				//
				ClearTexture(FLinearColor::Red, false); // mark corrupt sample
			}
			else if (IMediaTextureSampleConverter *Converter = Sample->GetMediaTextureSampleConverter())
			{
				//
				// Sample brings its own converter
				//

				const uint8 SampleNumMips = Sample->GetNumMips();

				IMediaTextureSampleConverter::FConversionHints Hints;
				Hints.NumMips = (SampleNumMips > 1) ? SampleNumMips : Params.NumMips;

				const bool bNeedsUAVTexture = MediaTextureResourceHelpers::RequiresUAVTexture(Sample, Hints.NumMips);

				// Does the conversion create its own output texture?
				if ((Converter->GetConverterInfoFlags() & IMediaTextureSampleConverter::ConverterInfoFlags_WillCreateOutputTexture) == 0)
				{
					// No. Does it actually do the conversion or just a pre-process step not yielding real output?
					if (Converter->GetConverterInfoFlags() & IMediaTextureSampleConverter::ConverterInfoFlags_PreprocessOnly)
					{
						// Preprocess...
						FTextureRHIRef DummyTexture;
						if (Converter->Convert(DummyTexture, Hints))
						{
							// ...followed by the built in conversion code as needed...
							ConvertOrCopyNeeded = true;
						}
					}
					else
					{
						// Conversion is fully handled by converter

						CreateIntermediateRenderTarget(Sample->GetOutputDim(), MediaTextureResourceHelpers::GetConvertedPixelFormat(Sample), MediaTextureResourceHelpers::RequiresSrgbTexture(Sample), Params.ClearColor, Hints.NumMips, bNeedsUAVTexture);
						Converter->Convert(RenderTargetTextureRHI, Hints);
					}
				}
				else
				{
					// The converter will create its own output texture for us to use
					FTextureRHIRef OutTexture;
					if (Converter->Convert(OutTexture, Hints))
					{
						// As the converter created the texture, we might need to convert it even more to make it fit our needs. Check...
						if (RequiresConversion(OutTexture, Sample->GetOutputDim(), NumMips))
						{
							CreateIntermediateRenderTarget(Sample->GetOutputDim(), MediaTextureResourceHelpers::GetConvertedPixelFormat(Sample), MediaTextureResourceHelpers::RequiresSrgbTexture(Sample), Params.ClearColor, Hints.NumMips, bNeedsUAVTexture);
							ConvertTextureToOutput(OutTexture.GetReference(), Sample);
						}
						else
						{
							UpdateTextureReference(OutTexture);
						}
					}
				}

				Cleared = false;
			}
			else
			{
				// No custom conversion, we need default processing...

				// Sanity check: do we have any data?
				if (Sample->GetTexture() == nullptr && Sample->GetBuffer() == nullptr)
				{
					UseSample = false;
				}
				else
				{
					ConvertOrCopyNeeded = true;
				}
			}

			if (UseSample)
			{
				if (ConvertOrCopyNeeded)
				{
					if (RequiresConversion(Sample, NumMips))
					{
						//
						// Sample needs to be converted by built in converter code
						//
						ConvertSample(Sample, Params.ClearColor, NumMips);
					}
					else
					{
						//
						// Sample can be used directly or is a simple copy
						//
						CopySample(Sample, Params.ClearColor, NumMips, Params.CurrentGuid);
					}
				}

				if (IMediaTextureSampleColorConverter* Converter = Sample->GetMediaTextureSampleColorConverter())
				{
					FTexture2DRHIRef TextureRef = IntermediateTarget ? IntermediateTarget : RenderTargetTextureRHI;

					if (bRecreateOutputTarget && TextureRef)
					{
						const static FLazyName ClassName(TEXT("FMediaTextureResource"));

						const FRHITextureDesc& IntermediateTextureDesc = TextureRef->GetDesc();

						const FRHITextureCreateDesc Desc =
							FRHITextureCreateDesc::Create2D(TEXT("MediaTextureColorConversionOutput"),
								IntermediateTextureDesc.GetSize().X,
								IntermediateTextureDesc.GetSize().Y,
								IntermediateTextureDesc.Format)
							.SetNumMips(IntermediateTextureDesc.NumMips)
							.SetClearValue(IntermediateTextureDesc.ClearValue)
							.SetClassName(ClassName)
							.SetOwnerName(GetOwnerName())
							.SetFlags(IntermediateTextureDesc.Flags);

						OutputTarget = RHICreateTexture(Desc);
						OutputTarget->SetName(TEXT("MediaTextureResourceOutput"));
						OutputTarget->SetOwnerName(GetOwnerName());

						bRecreateOutputTarget = false;
					}

					Converter->ApplyColorConversion(TextureRef, OutputTarget);

					if (RenderTargetTextureRHI != OutputTarget)
					{
						UpdateTextureReference(OutputTarget);
					}
				}
				else if (IntermediateTarget)
				{
					OutputTarget = IntermediateTarget;
				}

				Rotation = Sample->GetScaleRotation();
				Offset = Sample->GetOffset();

				if (CurrentSample)
				{
					// If we had a current sample (directly used as output), we can now schedule its retirement
					LocalPriorSamples->Retire(CurrentSample);
					CurrentSample = nullptr;
				}

				// Do we use a local copy as our output?
				if (OutputTarget == RenderTargetTextureRHI)
				{
					// Yes, we can schedule the actual sample for retirement right away
					LocalPriorSamples->Retire(Sample);
				}
				else
				{
					// No, we need to hold on to the sample
					CurrentSample = Sample;
				}

				// Generate mips as needed
				if (CurrentNumMips > 1 && !Cleared && Sample->GetNumMips() == 1)
				{
					check(OutputTarget);

					const EGenerateMipsPass GenerateMipsPass =
						MediaTextureResourceHelpers::SupportsComputeMipGen(OutputTarget->GetFormat()) ? EGenerateMipsPass::Compute : EGenerateMipsPass::Raster;

					CacheRenderTarget(OutputTarget, TEXT("MipGeneration"), MipGenerationCache);

					FRDGBuilder GraphBuilder(FRHICommandListExecutor::GetImmediateCommandList());
					FRDGTextureRef MipOutputTexture = GraphBuilder.RegisterExternalTexture(MipGenerationCache);
					FGenerateMips::Execute(GraphBuilder, GetFeatureLevel(), MipOutputTexture, FGenerateMipsParams{ SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp }, GenerateMipsPass);
					GraphBuilder.Execute();
				}

				SET_FLOAT_STAT(STAT_MediaUtils_TextureSampleTime, Sample->GetTime().Time.GetTotalMilliseconds());
			}
		}
	}
	else if (Params.CanClear)
	{
		//
		// No valid sample source & we should clear
		//

		// Need to clear the output?
		if (!Cleared || (Params.ClearColor != CurrentClearColor))
		{
			// Yes...
			ClearTexture(Params.ClearColor, false);

			if (CurrentSample)
			{
				// If we had a current sample (directly used as output), we can now schedule its retirement
				LocalPriorSamples->Retire(CurrentSample);
				CurrentSample = nullptr;
			}
		}
	}

	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

	// Cache next available sample time in the MediaTexture owner since we're the only one that can consume from the queue
	CacheNextAvailableSampleTime(SampleSource);

	// Update external texture registration in case we have no native support
	// (in that case there is support, the player will do this - but it is used all the time)
	if (!Owner.NewStyleOutput && !bUsesImageExternal)
	{
		SetupSampler();

		if (Params.CurrentGuid.IsValid())
		{
			FTextureRHIRef VideoTexture = (FTextureRHIRef)Owner.TextureReference.TextureReferenceRHI;
			FExternalTextureRegistry::Get().RegisterExternalTexture(Params.CurrentGuid, VideoTexture, SamplerStateRHI, Rotation, Offset);
		}

		if (Params.PreviousGuid.IsValid() && (Params.PreviousGuid != Params.CurrentGuid))
		{
			FExternalTextureRegistry::Get().UnregisterExternalTexture(Params.PreviousGuid);
		}
	}
	
	// Update usable Guid for the RenderThread
	Owner.SetRenderedExternalTextureGuid(Params.CurrentGuid);
}


/* FRenderTarget interface
 *****************************************************************************/

FIntPoint FMediaTextureResource::GetSizeXY() const
{
	return FIntPoint(Owner.GetWidth(), Owner.GetHeight());
}


/* FTextureResource interface
 *****************************************************************************/

FString FMediaTextureResource::GetFriendlyName() const
{
	return Owner.GetPathName();
}


uint32 FMediaTextureResource::GetSizeX() const
{
	return Owner.GetWidth();
}


uint32 FMediaTextureResource::GetSizeY() const
{
	return Owner.GetHeight();
}


void FMediaTextureResource::SetupSampler()
{
	ESamplerFilter OwnerFilter = (CurrentNumMips > 1 || Owner.NewStyleOutput) ? (ESamplerFilter)UDeviceProfileManager::Get().GetActiveProfile()->GetTextureLODSettings()->GetSamplerFilter(&Owner) : SF_Bilinear;
	float OwnerMipMapBias = Owner.GetMipMapBias();

	if (CurrentSamplerFilter != OwnerFilter || !FMath::IsNearlyEqual(CurrentMipMapBias, OwnerMipMapBias))
	{
		CurrentSamplerFilter = OwnerFilter;
		CurrentMipMapBias = OwnerMipMapBias;

		// create the sampler state
		FSamplerStateInitializerRHI SamplerStateInitializer(
			CurrentSamplerFilter,
			(Owner.AddressX == TA_Wrap) ? AM_Wrap : ((Owner.AddressX == TA_Clamp) ? AM_Clamp : AM_Mirror),
			(Owner.AddressY == TA_Wrap) ? AM_Wrap : ((Owner.AddressY == TA_Clamp) ? AM_Clamp : AM_Mirror),
			AM_Wrap,
			CurrentMipMapBias
		);

		SamplerStateRHI = GetOrCreateSamplerState(SamplerStateInitializer);
	}
}


void FMediaTextureResource::InitRHI(FRHICommandListBase&)
{
	SetupSampler();
	
	// Note: set up default texture, or we can get sampler bind errors on render
	// we can't leave here without having a valid bindable resource for some RHIs.

	ClearTexture(CurrentClearColor, Owner.SRGB);

	// Make sure init has done it's job - we can't leave here without valid bindable resources for some RHI's
	check(TextureRHI.IsValid());
	check(RenderTargetTextureRHI.IsValid());
	check(IntermediateTarget.IsValid());

	// Register "external texture" parameters if the platform does not support them (and hence the player does not set them)
	if (!bUsesImageExternal)
	{
		FTextureRHIRef VideoTexture = (FTextureRHIRef)Owner.TextureReference.TextureReferenceRHI;
		FExternalTextureRegistry::Get().RegisterExternalTexture(InitialTextureGuid, VideoTexture, SamplerStateRHI);
	}
}


void FMediaTextureResource::ReleaseRHI()
{
	Cleared = false;

	MipGenerationCache.SafeRelease();

	InputTarget.SafeRelease();
	IntermediateTarget.SafeRelease();
	OutputTarget.SafeRelease();
	RenderTargetTextureRHI.SafeRelease();
	TextureRHI.SafeRelease();

	UpdateTextureReference(nullptr);
}


/* FMediaTextureResource implementation
 *****************************************************************************/

void FMediaTextureResource::SetJustInTimeRenderParams(const FRenderParams& InJustInTimeRenderParams)
{
	check(IsInRenderingThread());

	JustInTimeRenderParams = MakeUnique<FRenderParams>();
	*JustInTimeRenderParams = InJustInTimeRenderParams;
}

void FMediaTextureResource::ResetJustInTimeRenderParams()
{
	check(IsInRenderingThread());

	JustInTimeRenderParams.Reset();
}

void FMediaTextureResource::JustInTimeRender()
{
	check(IsInRenderingThread());

	if (JustInTimeRenderParams.IsValid())
	{
		Render(*JustInTimeRenderParams);
		JustInTimeRenderParams.Reset();
	}
}


void FMediaTextureResource::ClearTexture(const FLinearColor& ClearColor, bool SrgbOutput)
{
	// create output render target if we don't have one yet
	constexpr uint8 NumMips = 1;
	constexpr bool bNeedsUAVTexture = false;
	CreateIntermediateRenderTarget(FIntPoint(2, 2), PF_B8G8R8A8, SrgbOutput, ClearColor, NumMips, bNeedsUAVTexture);

	// draw the clear color
	FRHICommandListImmediate& CommandList = FRHICommandListExecutor::GetImmediateCommandList();
	{
		SCOPED_DRAW_EVENT(CommandList, FMediaTextureResource_ClearTexture);
		SCOPED_GPU_STAT(CommandList, MediaTextureResource);

		CommandList.Transition(FRHITransitionInfo(RenderTargetTextureRHI, ERHIAccess::SRVMask, ERHIAccess::RTV));

		FRHIRenderPassInfo RPInfo(RenderTargetTextureRHI, ERenderTargetActions::Clear_Store);
		CommandList.BeginRenderPass(RPInfo, TEXT("ClearTexture"));
		CommandList.EndRenderPass();
		CommandList.Transition(FRHITransitionInfo(RenderTargetTextureRHI, ERHIAccess::RTV, ERHIAccess::SRVMask));
	}

	Cleared = true;
}


bool FMediaTextureResource::RequiresConversion(const FTextureRHIRef& SampleTexture, const FIntPoint & OutputDim, uint8 InNumMips) const
{
	if (Owner.NewStyleOutput)
	{
		//
		// New Style
		// 

		// If we have no mips in the sample, but want to have some in the output, we use the conversion pass
		// to setup level 0 and have a suitable output texture
		if (SampleTexture->GetNumMips() == 1 && InNumMips != 1)
		{
			return true;
		}
	}

	if (SampleTexture->GetSizeXY() != OutputDim)
	{
		return true;
	}

	// Only the following pixel formats are supported natively.
	// All other formats require a conversion on the GPU.

	const EPixelFormat Format = SampleTexture->GetFormat();

	return ((Format != PF_B8G8R8A8) &&
		(Format != PF_R8G8B8A8) &&
		(Format != PF_FloatRGB) &&
		(Format != PF_A16B16G16R16) &&
		(Format != PF_FloatRGBA));
}
 

bool FMediaTextureResource::RequiresConversion(const TSharedPtr<IMediaTextureSample, ESPMode::ThreadSafe>& Sample, uint8 InNumMips) const
{
	if (Owner.NewStyleOutput)
	{
		//
		// New Style
		// 

		FRHITexture *Texture = Sample->GetTexture();

		// If we have no mips in the sample, but want to have some in the output, we use the conversion pass
		// to setup level 0 and have a suitable output texture
		if (Texture && Texture->GetNumMips() == 1 && InNumMips != 1)
		{
			return true;
		}
	}

	// If the output dimensions are not the same as the sample's
	// dimensions, a resizing conversion on the GPU is required.
	//
	// Some reasons this could trigger:
	//	- Cropping
	//  - NV12's 1.5x height factor -> but the format would need conversion to RGB anyway
	//	- Various YCbCr formats will be stored with different widths as the final RGB output data -> a conversion is needed anyway
	//
	if (Sample->GetDim() != Sample->GetOutputDim())
	{
		return true;
	}

	// Color space different?
	const UE::Color::FColorSpace& Working = OverrideColorSpace.IsValid() ? *OverrideColorSpace : UE::Color::FColorSpace::GetWorking();
	const float Tollerance = 1.e-7f;
	if (!Sample->GetDisplayPrimaryRed().Equals(Working.GetRedChromaticity(), Tollerance) ||
		!Sample->GetDisplayPrimaryGreen().Equals(Working.GetGreenChromaticity(), Tollerance) ||
		!Sample->GetDisplayPrimaryBlue().Equals(Working.GetBlueChromaticity(), Tollerance) ||
		!Sample->GetWhitePoint().Equals(Working.GetWhiteChromaticity(), Tollerance))
	{
		// Yes! We need to convert...
		return true;
	}

	// No. Lets check if the encoding type is linear or not...
	UE::Color::EEncoding ColorEncoding = Sample->GetEncodingType();
	EMediaTextureSampleFormat Format = Sample->GetFormat();

	bool b8BitRGBA = (Format == EMediaTextureSampleFormat::CharBGRA ||
					  Format == EMediaTextureSampleFormat::CharRGBA ||
					  Format == EMediaTextureSampleFormat::DXT1 ||
					  Format == EMediaTextureSampleFormat::DXT5);

	if (b8BitRGBA)
	{
		// sRGB?
		if (ColorEncoding == UE::Color::EEncoding::sRGB)
		{
			// 8-bit RGBA and either CPU side buffer or texture with sRGB flag -> we can assume an inpout texture with HW translation to linear
			auto Texture = Sample->GetTexture();
			if (Texture == nullptr || ((uint32)Texture->GetFlags() & (uint32)TexCreate_SRGB))
			{
				return false;
			}
		}
	}

	bool bRGBA = b8BitRGBA ||
				 Format == EMediaTextureSampleFormat::FloatRGB ||
				 Format == EMediaTextureSampleFormat::FloatRGBA ||
				 Format == EMediaTextureSampleFormat::ABGR16 ||
				 Format == EMediaTextureSampleFormat::RGBA16 ||
				 Format == EMediaTextureSampleFormat::CharBGR10A2;

	// RGBA and linear?
	if (bRGBA && ColorEncoding == UE::Color::EEncoding::Linear)
	{
		return false;
	}

	// Either we have RGBA with non-linear encoding or its not RGBA -> we must convert!
	return true;
}



void FMediaTextureResource::ConvertSample(const TSharedPtr<IMediaTextureSample, ESPMode::ThreadSafe>& Sample, const FLinearColor& ClearColor, uint8 InNumMips)
{
	const EPixelFormat InputPixelFormat = MediaTextureResourceHelpers::GetPixelFormat(Sample);

	const uint8 SampleNumMips = Sample->GetNumMips();

	// get input texture
	FRHITexture* InputTexture = nullptr;
	{
		// If the sample already provides a texture resource, we simply use that
		// as the input texture. If the sample only provides raw data, then we
		// create our own input render target and copy the data into it.

		FRHITexture* SampleTexture = Sample->GetTexture();

		if (SampleTexture)
		{
			// Use the sample as source texture...

			InputTexture = SampleTexture;
			UpdateResourceSize();
			InputTarget = nullptr;
		}
		else
		{
			// Make a source texture so we can convert from it...

			const bool SrgbTexture = MediaTextureResourceHelpers::RequiresSrgbInputTexture(Sample);
			const ETextureCreateFlags InputCreateFlags = TexCreate_Dynamic | (SrgbTexture ? TexCreate_SRGB : TexCreate_None);
			const FIntPoint SampleDim = Sample->GetDim();

			// create a new temp input render target if necessary
			if (!InputTarget.IsValid() || (InputTarget->GetSizeXY() != SampleDim) || (InputTarget->GetFormat() != InputPixelFormat) || ((InputTarget->GetFlags() & InputCreateFlags) != InputCreateFlags) || (InputTarget->GetNumMips() != SampleNumMips))
			{
				const FRHITextureCreateDesc Desc =
					FRHITextureCreateDesc::Create2D(TEXT("FMediaTextureResource"), SampleDim, InputPixelFormat)
					.SetNumMips(SampleNumMips)
					.SetFlags(InputCreateFlags);

				InputTarget = RHICreateTexture(Desc);

				UpdateResourceSize();
			}

			// copy sample data to input render target
			const uint8* Data = (const uint8*)Sample->GetBuffer();
			check(Data);
			for (uint8 MipLevel = 0; MipLevel < SampleNumMips; ++MipLevel)
			{
				uint32 Stride = Sample->GetStride() >> MipLevel;
				uint32 Height = SampleDim.Y >> MipLevel;
				FUpdateTextureRegion2D Region(0, 0, 0, 0, SampleDim.X >> MipLevel, Height);
				RHIUpdateTexture2D(InputTarget, MipLevel, Region, Stride, Data);
				Data += Stride * Height;
			}

			InputTexture = InputTarget;
		}
	}

	// create the output texture
	const FIntPoint OutputDim = Sample->GetOutputDim();
	const uint8 NumMips = (SampleNumMips > 1) ? SampleNumMips : InNumMips;
	const bool bNeedsUAVTexture = MediaTextureResourceHelpers::RequiresUAVTexture(Sample, NumMips);
	CreateIntermediateRenderTarget(OutputDim, MediaTextureResourceHelpers::GetConvertedPixelFormat(Sample), MediaTextureResourceHelpers::RequiresSrgbTexture(Sample), ClearColor, NumMips, bNeedsUAVTexture);

	ConvertTextureToOutput(InputTexture, Sample);
}


void FMediaTextureResource::GetColorSpaceConversionMatrixForSample(const TSharedPtr<IMediaTextureSample, ESPMode::ThreadSafe> Sample, FMatrix44f& ColorSpaceMtx)
{
	const UE::Color::FColorSpace& Working = OverrideColorSpace.IsValid() ? *OverrideColorSpace : UE::Color::FColorSpace::GetWorking();
	
	if (Sample->GetMediaTextureSampleColorConverter())
	{
		ColorSpaceMtx = FMatrix44f::Identity;
	}
	else
	{
		ColorSpaceMtx = FMatrix44f(Working.GetXYZToRgb().GetTransposed() * Sample->GetGamutToXYZMatrix());
	}
	
	float NF = Sample->GetHDRNitsNormalizationFactor();
	if (NF != 1.0f)
	{
		ColorSpaceMtx = ColorSpaceMtx.ApplyScale(NF);
	}
}


 void FMediaTextureResource::ConvertTextureToOutput(FRHITexture* InputTexture, const TSharedPtr<IMediaTextureSample, ESPMode::ThreadSafe>& Sample)
 {
	// perform the conversion
	FRHICommandListImmediate& CommandList = FRHICommandListExecutor::GetImmediateCommandList();
	{
		// We should never get here with a sample that contains mips!
		check(Sample->GetNumMips() == 1);

		SCOPED_DRAW_EVENT(CommandList, FMediaTextureResource_Convert);
		SCOPED_GPU_STAT(CommandList, MediaTextureResource);

		FGraphicsPipelineStateInitializer GraphicsPSOInit;
		FRHITexture* RenderTarget = IntermediateTarget;
		CommandList.Transition(FRHITransitionInfo(RenderTarget, ERHIAccess::Unknown, ERHIAccess::RTV));

		FIntPoint OutputDim(RenderTarget->GetSizeXYZ().X, RenderTarget->GetSizeXYZ().Y);

		// note: we are not explicitly transitioning the input texture to be readable here
		// (we assume this to be the case already - main as some platforms may fail to orderly transition the resource due to special cases regarding their internal setup)
		CommandList.Transition(FRHITransitionInfo(RenderTargetTextureRHI, ERHIAccess::Unknown, ERHIAccess::RTV));

		FRHIRenderPassInfo RPInfo(RenderTarget, ERenderTargetActions::DontLoad_Store);
		CommandList.BeginRenderPass(RPInfo, TEXT("ConvertMedia"));
		{
			CommandList.ApplyCachedRenderTargets(GraphicsPSOInit);
			CommandList.SetViewport(0, 0, 0.0f, (float)OutputDim.X, (float)OutputDim.Y, 1.0f);

			GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
			GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
			GraphicsPSOInit.BlendState = TStaticBlendStateWriteMask<CW_RGBA, CW_NONE, CW_NONE, CW_NONE, CW_NONE, CW_NONE, CW_NONE, CW_NONE>::GetRHI();
			GraphicsPSOInit.PrimitiveType = PT_TriangleStrip;

			// configure media shaders
			auto ShaderMap = GetGlobalShaderMap(GetFeatureLevel());
			TShaderMapRef<FMediaShadersVS> VertexShader(ShaderMap);

			GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GMediaVertexDeclaration.VertexDeclarationRHI;
			GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();

			// Temporary SRV variables to hold references for the draw
			FShaderResourceViewRHIRef TempSRV0, TempSRV1;

			// Use the sample format to choose the conversion path
			EMediaTextureSampleFormat SampleFormat = Sample->GetFormat();
			switch (SampleFormat)
			{
				// Planar 8-bit YCbCr 420
				case EMediaTextureSampleFormat::CharNV12:
				case EMediaTextureSampleFormat::CharNV21:	// swapped CbCr (vs. NV12)
				{
					auto YUVMtx = Sample->GetSampleToRGBMatrix();
					FMatrix44f ColorSpaceMtx;
					GetColorSpaceConversionMatrixForSample(Sample, ColorSpaceMtx);
					
					if (InputTexture->GetFormat() == PF_NV12)
					{
						TShaderMapRef<FNV12ConvertPS> ConvertShader(ShaderMap);
						GraphicsPSOInit.BoundShaderState.PixelShaderRHI = ConvertShader.GetPixelShader();
						SetGraphicsPipelineState(CommandList, GraphicsPSOInit, 0);
						FIntPoint TexDim = InputTexture->GetSizeXY();
						TempSRV0 = CommandList.CreateShaderResourceView(InputTexture, 0, 1, PF_G8);								// note: RHI does provide "magic" to select Y vs. UV planes based on the pixel format (D3D/DXGI)
						TempSRV1 = CommandList.CreateShaderResourceView(InputTexture, 0, 1, PF_R8G8);
						SetShaderParametersLegacyPS(CommandList, ConvertShader, TexDim, TempSRV0, TempSRV1, OutputDim, YUVMtx, Sample->GetEncodingType(), ColorSpaceMtx, SampleFormat == EMediaTextureSampleFormat::CharNV21);
					}
					else
					{
						TShaderMapRef<FNV12ConvertAsBytesPS> ConvertShader(ShaderMap);
						GraphicsPSOInit.BoundShaderState.PixelShaderRHI = ConvertShader.GetPixelShader();
						SetGraphicsPipelineState(CommandList, GraphicsPSOInit, 0);
						SetShaderParametersLegacyPS(CommandList, ConvertShader, InputTexture, OutputDim, YUVMtx, Sample->GetEncodingType(), ColorSpaceMtx, SampleFormat == EMediaTextureSampleFormat::CharNV21);
					}
				}
				break;

				// Planar 10-bit YCbCr 420 (in 16-bit components)
				case EMediaTextureSampleFormat::P010:
				{
					auto YUVMtx = Sample->GetSampleToRGBMatrix();
					FMatrix44f ColorSpaceMtx;
					GetColorSpaceConversionMatrixForSample(Sample, ColorSpaceMtx);

					FIntPoint TexDim = InputTexture->GetSizeXY();
					if (InputTexture->GetFormat() == PF_P010)
					{
						TShaderMapRef<FP010ConvertPS> ConvertShader(ShaderMap);
						GraphicsPSOInit.BoundShaderState.PixelShaderRHI = ConvertShader.GetPixelShader();
						SetGraphicsPipelineState(CommandList, GraphicsPSOInit, 0);

						FShaderResourceViewRHIRef Y_SRV = CommandList.CreateShaderResourceView(InputTexture, 0, 1, PF_G16);		// note: RHI does provide "magic" to select Y vs. UV planes based on the pixel format (D3D/DXGI)
						FShaderResourceViewRHIRef UV_SRV = CommandList.CreateShaderResourceView(InputTexture, 0, 1, PF_G16R16);
						SetShaderParametersLegacyPS(CommandList, ConvertShader, TexDim, Y_SRV, UV_SRV, OutputDim, YUVMtx, ColorSpaceMtx, Sample->GetEncodingType());
					}
					else
					{
						TShaderMapRef<FP010ConvertAsUINT16sPS> ConvertShader(ShaderMap);
						GraphicsPSOInit.BoundShaderState.PixelShaderRHI = ConvertShader.GetPixelShader();
						SetGraphicsPipelineState(CommandList, GraphicsPSOInit, 0);

						SetShaderParametersLegacyPS(CommandList, ConvertShader, TexDim, InputTexture, OutputDim, YUVMtx, ColorSpaceMtx, Sample->GetEncodingType());
					}
				}
				break;
 
				// Various YCbCr 422 formats
				case EMediaTextureSampleFormat::CharYUY2:		// Y0CbY1Cr
				case EMediaTextureSampleFormat::CharYVYU:		// Y0CrY1Cb
				case EMediaTextureSampleFormat::Char2VUY:		// CbY0CrY1
				case EMediaTextureSampleFormat::YUVv216:		// CbY0CrY1
				case EMediaTextureSampleFormat::CharUYVY:		// CbY0CrY1
				{
					auto YUVMtx = Sample->GetSampleToRGBMatrix();
					FMatrix44f ColorSpaceMtx;
					GetColorSpaceConversionMatrixForSample(Sample, ColorSpaceMtx);

					TShaderMapRef<FYUVv216ConvertPS> ConvertShader(ShaderMap);
					GraphicsPSOInit.BoundShaderState.PixelShaderRHI = ConvertShader.GetPixelShader();
					SetGraphicsPipelineState(CommandList, GraphicsPSOInit, 0);
					SetShaderParametersLegacyPS(CommandList, ConvertShader, InputTexture, OutputDim, YUVMtx, Sample->GetEncodingType(), ColorSpaceMtx,
												SampleFormat!= EMediaTextureSampleFormat::CharYUY2 && SampleFormat != EMediaTextureSampleFormat::CharYVYU,	// Y or Cb first
												InputTexture->GetFormat() == PF_B8G8R8A8,																	// ARGB vs. ABGR (memory order)
												SampleFormat == EMediaTextureSampleFormat::CharYVYU															// Cb / Cr swap
												);
				}
				break;

				// 10-bit YCbCr 422 format (stored in A2R10G10B10)
				case EMediaTextureSampleFormat::YUVv210:
				{
					auto YUVMtx = Sample->GetSampleToRGBMatrix();
					FMatrix44f ColorSpaceMtx;
					GetColorSpaceConversionMatrixForSample(Sample, ColorSpaceMtx);

					TShaderMapRef<FYUVv210ConvertPS> ConvertShader(ShaderMap);
					GraphicsPSOInit.BoundShaderState.PixelShaderRHI = ConvertShader.GetPixelShader();
					SetGraphicsPipelineState(CommandList, GraphicsPSOInit, 0);
					SetShaderParametersLegacyPS(CommandList, ConvertShader, InputTexture, OutputDim, YUVMtx, Sample->GetEncodingType(), ColorSpaceMtx,
												true);
				}
				break;

				// Various YCbCR 444 formats
				case EMediaTextureSampleFormat::CharAYUV:
				case EMediaTextureSampleFormat::Y416:
				case EMediaTextureSampleFormat::R4FL:
				{
					auto YUVMtx = Sample->GetSampleToRGBMatrix();
					FMatrix44f ColorSpaceMtx;
					GetColorSpaceConversionMatrixForSample(Sample, ColorSpaceMtx);

					TShaderMapRef<FYUVY416ConvertPS> ConvertShader(ShaderMap);
					GraphicsPSOInit.BoundShaderState.PixelShaderRHI = ConvertShader.GetPixelShader();
					SetGraphicsPipelineState(CommandList, GraphicsPSOInit, 0);
					FShaderResourceViewRHIRef SRV = CommandList.CreateShaderResourceView(InputTexture, 0, 1, (Sample->GetFormat() == EMediaTextureSampleFormat::Y416) ? PF_A16B16G16R16 : PF_A32B32G32R32F);

					SetShaderParametersLegacyPS(CommandList, ConvertShader, SRV, YUVMtx, Sample->GetEncodingType(), ColorSpaceMtx,
												InputTexture->GetFormat() == PF_A8R8G8B8);
				}
				break;

				// Various RGB(A) formats
				case EMediaTextureSampleFormat::CharBGR10A2:
				case EMediaTextureSampleFormat::CharRGBA:
				case EMediaTextureSampleFormat::CharBGRA:
				case EMediaTextureSampleFormat::FloatRGB:
				case EMediaTextureSampleFormat::FloatRGBA:
				case EMediaTextureSampleFormat::RGBA16:
				case EMediaTextureSampleFormat::ABGR16:
				{
					// We get here if we detect the need for an inverse EOTF application, CS conversion OR if we need to generate mips for RGBA samples

					// Inverse EOTF?
					UE::Color::EEncoding Encoding;
					if ((uint32)InputTexture->GetFlags() & (uint32)TexCreate_SRGB)
					{
						// We do not need one: sRGB will be handled in HW
						Encoding = UE::Color::EEncoding::Linear;
					}
					else
					{
						Encoding = Sample->GetEncodingType();
					}

					FMatrix44f ColorSpaceMtx;
					GetColorSpaceConversionMatrixForSample(Sample, ColorSpaceMtx);

					TShaderMapRef<FRGBConvertPS> ConvertShader(ShaderMap);
					GraphicsPSOInit.BoundShaderState.PixelShaderRHI = ConvertShader.GetPixelShader();
					SetGraphicsPipelineState(CommandList, GraphicsPSOInit, 0);
					SetShaderParametersLegacyPS(CommandList, ConvertShader, InputTexture, OutputDim, Encoding, ColorSpaceMtx);
				}
				break;

				case EMediaTextureSampleFormat::ARGB16_BIG:
				{
					FMatrix44f ColorSpaceMtx;
					GetColorSpaceConversionMatrixForSample(Sample, ColorSpaceMtx);

					TShaderMapRef<FARGB16BigConvertPS> ConvertShader(ShaderMap);
					GraphicsPSOInit.BoundShaderState.PixelShaderRHI = ConvertShader.GetPixelShader();
					SetGraphicsPipelineState(CommandList, GraphicsPSOInit, 0);
					FShaderResourceViewRHIRef SRV = CommandList.CreateShaderResourceView(InputTexture, 0, 1, PF_R16G16B16A16_UINT);

					SetShaderParametersLegacyPS(CommandList, ConvertShader, SRV, OutputDim, Sample->GetEncodingType(), ColorSpaceMtx);
				}
				break;

				case EMediaTextureSampleFormat::CharBMP:
				{
					// Simple 1:1 copy plus flip & color adjustment (but using normal texture sampler: sRGB conversions may occur depending on setup; any manual sRGB/linear conversion is disabled)
					TShaderMapRef<FBMPConvertPS> ConvertShader(ShaderMap);
					GraphicsPSOInit.BoundShaderState.PixelShaderRHI = ConvertShader.GetPixelShader();
					SetGraphicsPipelineState(CommandList, GraphicsPSOInit, 0);
					SetShaderParametersLegacyPS(CommandList, ConvertShader, InputTexture, OutputDim, false);
				}
				break;

				case EMediaTextureSampleFormat::DXT1:
				case EMediaTextureSampleFormat::DXT5:
				{
					// Note: the shader used here will pass-through alpha data
					
					// Inverse EOTF?
					UE::Color::EEncoding Encoding;
					if ((uint32)InputTexture->GetFlags() & (uint32)TexCreate_SRGB)
					{
						// We do not need one: sRGB will be handled in HW
						Encoding = UE::Color::EEncoding::Linear;
					}
					else
					{
						Encoding = Sample->GetEncodingType();
					}

					FMatrix44f ColorSpaceMtx;
					GetColorSpaceConversionMatrixForSample(Sample, ColorSpaceMtx);

					// Convert the DXn compressed data into RGBA so we can more easily process it using the existing logic (e.g mip generation)
					// (this also will deal with any sRGB/Rec703 conversions - as well as any color space conversions)
					TShaderMapRef<FRGBConvertPS> ConvertShader(ShaderMap);
					GraphicsPSOInit.BoundShaderState.PixelShaderRHI = ConvertShader.GetPixelShader();
					SetGraphicsPipelineState(CommandList, GraphicsPSOInit, 0);
					SetShaderParametersLegacyPS(CommandList, ConvertShader, InputTexture, OutputDim, Encoding, ColorSpaceMtx);
				}
				break;

				case EMediaTextureSampleFormat::BC4:
				{
					// We are using the RGB conversion shader to just pipe BC4 data (read into R) and remap it to be usable with L8 or A8 etc. (RRRR) using the otherwise idle CS conversion matrix
					// (note: no EOTF or CS - this is alpha only!)
					TShaderMapRef<FRGBConvertPS> ConvertShader(ShaderMap);
					GraphicsPSOInit.BoundShaderState.PixelShaderRHI = ConvertShader.GetPixelShader();
					SetGraphicsPipelineState(CommandList, GraphicsPSOInit, 0);
					SetShaderParametersLegacyPS(CommandList, ConvertShader, InputTexture, OutputDim, UE::Color::EEncoding::Linear, SplatMtx);
				}
				break;

				case EMediaTextureSampleFormat::YCoCg_DXT5:
				{
					// Inverse EOTF?
					UE::Color::EEncoding Encoding;
					if ((uint32)InputTexture->GetFlags() & (uint32)TexCreate_SRGB)
					{
						// We do not need one: sRGB will be handled in HW
						Encoding = UE::Color::EEncoding::Linear;
					}
					else
					{
						Encoding = Sample->GetEncodingType();
					}

					FMatrix44f ColorSpaceMtx;
					GetColorSpaceConversionMatrixForSample(Sample, ColorSpaceMtx);

					// Convert the DXn compressed YCoCg data into RGBA so we can more easily process it using the existing logic (e.g mip generation)
					// (this also will deal with any sRGB/Rec703 conversions - as well as any color space conversions)
					TShaderMapRef<FYCoCgConvertPS> ConvertShader(ShaderMap);
					GraphicsPSOInit.BoundShaderState.PixelShaderRHI = ConvertShader.GetPixelShader();
					SetGraphicsPipelineState(CommandList, GraphicsPSOInit, 0);
					SetShaderParametersLegacyPS(CommandList, ConvertShader, InputTexture, OutputDim, Encoding, ColorSpaceMtx);
				}
				break;

				default:
				{
					// This should not happen in normal use: still - end the render pass to avoid any trouble with RHI
					CommandList.EndRenderPass();
					return; // unsupported format (either illegal value or needing a custom converter)
				}
			}

			// draw full size quad into render target
			FBufferRHIRef VertexBuffer = CreateTempMediaVertexBuffer();
			CommandList.SetStreamSource(0, VertexBuffer, 0);
			// set viewport to RT size
			CommandList.SetViewport(0, 0, 0.0f, (float)OutputDim.X, (float)OutputDim.Y, 1.0f);

			CommandList.DrawPrimitive(0, 2, 1);
		}
		CommandList.EndRenderPass();
		CommandList.Transition(FRHITransitionInfo(RenderTarget, ERHIAccess::RTV, ERHIAccess::SRVGraphics));
	}

	Cleared = false;
}


void FMediaTextureResource::CopySample(const TSharedPtr<IMediaTextureSample, ESPMode::ThreadSafe>& Sample, const FLinearColor& ClearColor, uint8 InNumMips, const FGuid & TextureGUID)
{
	FRHITexture* SampleTexture = Sample->GetTexture();
	const uint8 SampleNumMips = Sample->GetNumMips();

	// If the sample already provides a texture resource, we simply use that
	// as the output render target. If the sample only provides raw data, then
	// we create our own output render target and copy the data into it.

	if (SampleTexture != nullptr)
	{
		// Use sample's texture as the new render target - no copy
		if (TextureRHI != SampleTexture)
		{
			UpdateTextureReference(SampleTexture);

			MipGenerationCache.SafeRelease();
			IntermediateTarget.SafeRelease();
		}
		else
		{
			// Texture to receive texture from sample
			const uint8 NumMips = (SampleNumMips > 1) ? SampleNumMips : InNumMips;
			const bool bNeedsUAVTexture = MediaTextureResourceHelpers::RequiresUAVTexture(Sample, NumMips);
			CreateIntermediateRenderTarget(Sample->GetOutputDim(), MediaTextureResourceHelpers::GetPixelFormat(Sample), MediaTextureResourceHelpers::RequiresSrgbTexture(Sample), ClearColor, NumMips, bNeedsUAVTexture);

			// Copy data into the output texture to able to add mips later on
			FRHICommandListExecutor::GetImmediateCommandList().CopyTexture(SampleTexture, IntermediateTarget, FRHICopyTextureInfo());
		}
	}
	else
	{
		// Texture to receive precisely only output pixels via CPU copy
		const uint8 NumMips = (SampleNumMips > 1) ? SampleNumMips : InNumMips;
		const bool bNeedsUAVTexture = MediaTextureResourceHelpers::RequiresUAVTexture(Sample, NumMips);
		CreateIntermediateRenderTarget(Sample->GetDim(), MediaTextureResourceHelpers::GetPixelFormat(Sample), MediaTextureResourceHelpers::RequiresSrgbTexture(Sample), ClearColor, NumMips, bNeedsUAVTexture);

		// If we also have no source buffer and the platform generally would allow for use of external textures, we assume it is just that...
		// (as long as the player actually produces (dummy) samples, this will enable mips support as well as auto conversion for "new style output" mode)
		if (!Sample->GetBuffer())
		{
			// we expect an external texture image in this case - we should have no mips reported here!
			check(SampleNumMips == 1);

			if (GSupportsImageExternal)
			{
				CopyFromExternalTexture(Sample, TextureGUID);
			}
			else
			{
				// We never should get here, but could should a player pass us a "valid" sample with neither texture or buffer based data in it (and we don't have ExternalTexture support)

				// Just clear the texture so we don't show any random memory contents...
				FRHICommandListImmediate& CommandList = FRHICommandListExecutor::GetImmediateCommandList();
				CommandList.Transition(FRHITransitionInfo(RenderTargetTextureRHI, ERHIAccess::Unknown, ERHIAccess::RTV));
				FRHIRenderPassInfo RPInfo(RenderTargetTextureRHI, ERenderTargetActions::Clear_Store);
				CommandList.BeginRenderPass(RPInfo, TEXT("ClearTexture"));
				CommandList.EndRenderPass();
				CommandList.Transition(FRHITransitionInfo(RenderTargetTextureRHI, ERHIAccess::RTV, ERHIAccess::SRVMask));
			}
		}
		else
		{
			// Copy sample data (from CPU mem) to output render target
			const FIntPoint SampleDim = Sample->GetDim();
			const uint8* Data = (const uint8*)Sample->GetBuffer();
			for (uint8 MipLevel = 0; MipLevel < SampleNumMips; ++MipLevel)
			{
				uint32 Stride = Sample->GetStride() >> MipLevel;
				uint32 Height = SampleDim.Y >> MipLevel;
				FUpdateTextureRegion2D Region(0, 0, 0, 0, SampleDim.X >> MipLevel, Height);
				RHIUpdateTexture2D(RenderTargetTextureRHI, MipLevel, Region, Stride, Data);
				Data += Stride * Height;
			}

			// Make sure resource is in SRV mode again
			FRHICommandListExecutor::GetImmediateCommandList().Transition(FRHITransitionInfo(RenderTargetTextureRHI.GetReference(), ERHIAccess::Unknown, ERHIAccess::SRVMask));
		}
	}

	Cleared = false;
}


void FMediaTextureResource::CopyFromExternalTexture(const TSharedPtr <IMediaTextureSample, ESPMode::ThreadSafe>& Sample, const FGuid & TextureGUID)
{
	FRHICommandListImmediate& CommandList = FRHICommandListExecutor::GetImmediateCommandList();
	{
		FTextureRHIRef SampleTexture;
		FSamplerStateRHIRef SamplerState;
		if (!FExternalTextureRegistry::Get().GetExternalTexture(nullptr, TextureGUID, SampleTexture, SamplerState))
		{
			// This should never happen: we could not find the external texture data. Still, if it does we clear the output...
			FRHIRenderPassInfo RPInfo(RenderTargetTextureRHI, ERenderTargetActions::Clear_Store);
			CommandList.BeginRenderPass(RPInfo, TEXT("ClearTexture"));
			CommandList.EndRenderPass();
			CommandList.Transition(FRHITransitionInfo(RenderTargetTextureRHI, ERHIAccess::Unknown, ERHIAccess::SRVMask));
			return;
		}

		FLinearColor Offset, ScaleRotation;
		FExternalTextureRegistry::Get().GetExternalTextureCoordinateOffset(TextureGUID, Offset);
		FExternalTextureRegistry::Get().GetExternalTextureCoordinateScaleRotation(TextureGUID, ScaleRotation);

		SCOPED_DRAW_EVENT(CommandList, FMediaTextureResource_ConvertExternalTexture);
		SCOPED_GPU_STAT(CommandList, MediaTextureResource);

		FGraphicsPipelineStateInitializer GraphicsPSOInit;
		FRHITexture* RenderTarget = RenderTargetTextureRHI.GetReference();

		FRHIRenderPassInfo RPInfo(RenderTarget, ERenderTargetActions::DontLoad_Store);
		CommandList.BeginRenderPass(RPInfo, TEXT("ConvertMedia_ExternalTexture"));
		{
			const FIntPoint OutputDim = Sample->GetOutputDim();

			CommandList.ApplyCachedRenderTargets(GraphicsPSOInit);
			CommandList.SetViewport(0, 0, 0.0f, (float)OutputDim.X, (float)OutputDim.Y, 1.0f);
			
			GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
			GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
			GraphicsPSOInit.BlendState = TStaticBlendStateWriteMask<CW_RGBA, CW_NONE, CW_NONE, CW_NONE, CW_NONE, CW_NONE, CW_NONE, CW_NONE>::GetRHI();
			GraphicsPSOInit.PrimitiveType = PT_TriangleStrip;

			// configure media shaders
			auto ShaderMap = GetGlobalShaderMap(GetFeatureLevel());
			TShaderMapRef<FMediaShadersVS> VertexShader(ShaderMap);

			GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GMediaVertexDeclaration.VertexDeclarationRHI;
			GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();

			TShaderMapRef<FReadTextureExternalPS> CopyShader(ShaderMap);
			GraphicsPSOInit.BoundShaderState.PixelShaderRHI = CopyShader.GetPixelShader();
			SetGraphicsPipelineState(CommandList, GraphicsPSOInit, 0);
			SetShaderParametersLegacyPS(CommandList, CopyShader, SampleTexture, SamplerState, ScaleRotation, Offset);

			// draw full size quad into render target
			FBufferRHIRef VertexBuffer = CreateTempMediaVertexBuffer();
			CommandList.SetStreamSource(0, VertexBuffer, 0);
			// set viewport to RT size
			CommandList.SetViewport(0, 0, 0.0f, (float)OutputDim.X, (float)OutputDim.Y, 1.0f);

			CommandList.DrawPrimitive(0, 2, 1);
		}
		CommandList.EndRenderPass();
		CommandList.Transition(FRHITransitionInfo(RenderTargetTextureRHI, ERHIAccess::Unknown, ERHIAccess::SRVMask));
	}
}


void FMediaTextureResource::UpdateResourceSize()
{
	SIZE_T ResourceSize = 0;

	if (InputTarget.IsValid())
	{
		ResourceSize += CalcTextureSize(InputTarget->GetSizeX(), InputTarget->GetSizeY(), InputTarget->GetFormat(), 1);
	}
	
	if (IntermediateTarget.IsValid() && IntermediateTarget != OutputTarget)
	{
		ResourceSize += CalcTextureSize(IntermediateTarget->GetSizeX(), IntermediateTarget->GetSizeY(), IntermediateTarget->GetFormat(), 1);
	}

	if (OutputTarget.IsValid())
	{
		ResourceSize += CalcTextureSize(OutputTarget->GetSizeX(), OutputTarget->GetSizeY(), OutputTarget->GetFormat(), 1);
	}

	OwnerSize = ResourceSize;
}


void FMediaTextureResource::UpdateTextureReference(FRHITexture* NewTexture)
{
	TextureRHI = NewTexture;
	RenderTargetTextureRHI = NewTexture;

	RHIUpdateTextureReference(Owner.TextureReference.TextureReferenceRHI, NewTexture);
	// note: sRGB status for Owner.SRGB is handled (on game thread) in MediaTetxure.cpp

	if (RenderTargetTextureRHI != nullptr)
	{
		OwnerDim = FIntPoint(RenderTargetTextureRHI->GetSizeX(), RenderTargetTextureRHI->GetSizeY());
	}
	else
	{
		OwnerDim = FIntPoint::ZeroValue;
	}
}


void FMediaTextureResource::CreateIntermediateRenderTarget(const FIntPoint & InDim, EPixelFormat InPixelFormat, bool bInSRGB, const FLinearColor & InClearColor, uint8 InNumMips, bool bNeedsUAVSupport)
{
	// create output render target if necessary
	ETextureCreateFlags OutputCreateFlags = TexCreate_Dynamic | (bInSRGB ? TexCreate_SRGB : TexCreate_None);
	if (bNeedsUAVSupport && UE::PixelFormat::HasCapabilities(InPixelFormat, EPixelFormatCapabilities::TypedUAVLoad))
	{
		OutputCreateFlags |= TexCreate_UAV;
	}

	if (InNumMips > 1)
	{
		// Make sure can have mips & the mip generator has what it needs to work
		OutputCreateFlags |= TexCreate_GenerateMipCapable;

		// Make sure we only set a number of mips that actually makes sense, given the sample size
		uint8 MaxMips = (uint8)(FMath::Min(255, FGenericPlatformMath::FloorToInt(FGenericPlatformMath::Log2(static_cast<float>(FGenericPlatformMath::Min(InDim.X, InDim.Y))))));
		InNumMips = FMath::Min(InNumMips, MaxMips);
	}

	// Avoid setting things up as a render target if we are passing in a block compression format
	// (this happens only if these have no conversion needs at all - otherwise they will be promoted to RGBA8)
	if (InPixelFormat != PF_DXT1 && InPixelFormat != PF_DXT5 && InPixelFormat != PF_BC4)
	{
		OutputCreateFlags |= ETextureCreateFlags::RenderTargetable;
	}

	if ((InClearColor != CurrentClearColor) || !IntermediateTarget.IsValid() || (IntermediateTarget->GetSizeXY() != InDim) || (IntermediateTarget->GetFormat() != InPixelFormat) || ((IntermediateTarget->GetFlags() & OutputCreateFlags) != OutputCreateFlags) || CurrentNumMips != InNumMips)
	{
		MipGenerationCache.SafeRelease();

		const static FLazyName ClassName(TEXT("FMediaTextureResource"));
		const FRHITextureCreateDesc Desc =
			FRHITextureCreateDesc::Create2D(TEXT("MediaTextureResourceOutput"))
			.SetExtent(InDim)
			.SetFormat(InPixelFormat)
			.SetNumMips(InNumMips)
			.SetFlags(OutputCreateFlags | ETextureCreateFlags::ShaderResource)
			.SetInitialState(ERHIAccess::SRVMask)
			.SetClearValue(FClearValueBinding(InClearColor))
			.SetClassName(ClassName)
			.SetOwnerName(GetOwnerName());

		IntermediateTarget = RHICreateTexture(Desc);

		IntermediateTarget->SetName(TEXT("MediaTextureResourceOutput"));
		IntermediateTarget->SetOwnerName(GetOwnerName());

		CurrentClearColor = InClearColor;
		CurrentNumMips = InNumMips;
		UpdateResourceSize();

		Cleared = false;
		bRecreateOutputTarget = true;
	}
	
	if (RenderTargetTextureRHI != IntermediateTarget)
	{
		UpdateTextureReference(IntermediateTarget);
	}
}

void FMediaTextureResource::CacheNextAvailableSampleTime(const TSharedPtr<FMediaTextureSampleSource, ESPMode::ThreadSafe>& InSampleQueue) const
{
	FTimespan SampleTime(FTimespan::MinValue());

	if (InSampleQueue.IsValid())
	{
		TSharedPtr<IMediaTextureSample, ESPMode::ThreadSafe> Sample;
		if (InSampleQueue->Peek(Sample))
		{
			SampleTime = Sample->GetTime().Time;
		}
	}

	Owner.CacheNextAvailableSampleTime(SampleTime);
}
