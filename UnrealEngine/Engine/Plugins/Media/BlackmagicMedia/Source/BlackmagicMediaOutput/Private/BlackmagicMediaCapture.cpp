// Copyright Epic Games, Inc. All Rights Reserved.

#include "BlackmagicMediaCapture.h"

#include "BlackmagicLib.h"
#include "BlackmagicMediaOutput.h"
#include "BlackmagicMediaOutputModule.h"
#include "Engine/Engine.h"
#include "HAL/Event.h"
#include "HAL/IConsoleManager.h"
#include "IBlackmagicMediaModule.h"
#include "MediaIOCoreSubsystem.h"
#include "MediaIOCoreFileWriter.h"
#include "Misc/ScopeLock.h"
#include "Modules/ModuleManager.h"
#include "Slate/SceneViewport.h"
#include "Widgets/SViewport.h"

#if WITH_EDITOR
#include "AnalyticsEventAttribute.h"
#include "EngineAnalytics.h"
#endif

TAutoConsoleVariable<int32> CVarBlackmagicEnableGPUDirect(
	TEXT("Blackmagic.EnableGPUDirect"), 0,
	TEXT("Whether to enable GPU direct for faster video frame copies. (Experimental)"),
	ECVF_RenderThreadSafe);

bool bBlackmagicWritInputRawDataCmdEnable = false;
static FAutoConsoleCommand BlackmagicWriteInputRawDataCmd(
	TEXT("Blackmagic.WriteInputRawData"),
	TEXT("Write Blackmagic raw input buffer to file."),
	FConsoleCommandDelegate::CreateLambda([]() { bBlackmagicWritInputRawDataCmdEnable = true; })
	);

namespace BlackmagicMediaCaptureHelpers
{
	class FBlackmagicMediaCaptureEventCallback : public BlackmagicDesign::IOutputEventCallback
	{
	public:
		FBlackmagicMediaCaptureEventCallback(UBlackmagicMediaCapture* InOwner, const BlackmagicDesign::FChannelInfo& InChannelInfo)
			: RefCounter(0)
			, Owner(InOwner)
			, ChannelInfo(InChannelInfo)
			, LastFramesDroppedCount(0)
		{
		}

		bool Initialize(const BlackmagicDesign::FOutputChannelOptions& InChannelOptions)
		{
			AddRef();

			check(!BlackmagicIdendifier.IsValid());
			BlackmagicDesign::ReferencePtr<BlackmagicDesign::IOutputEventCallback> SelfCallbackRef(this);
			BlackmagicIdendifier = BlackmagicDesign::RegisterOutputChannel(ChannelInfo, InChannelOptions, SelfCallbackRef);
			return BlackmagicIdendifier.IsValid();
		}

		void Uninitialize()
		{
			{
				FScopeLock Lock(&CallbackLock);
				BlackmagicDesign::UnregisterOutputChannel(ChannelInfo, BlackmagicIdendifier, true);
				Owner = nullptr;
			}

			Release();
		}

		bool SendVideoFrameData(BlackmagicDesign::FFrameDescriptor& InFrameDescriptor)
		{
			return BlackmagicDesign::SendVideoFrameData(ChannelInfo, InFrameDescriptor);
		}

		bool SendVideoFrameData(BlackmagicDesign::FFrameDescriptor_GPUDMA& InFrameDescriptor)
		{
			return BlackmagicDesign::SendVideoFrameData(ChannelInfo, InFrameDescriptor);
		}

		bool SendAudioSamples(const BlackmagicDesign::FAudioSamplesDescriptor& InAudioDescriptor)
		{
			return BlackmagicDesign::SendAudioSamples(ChannelInfo, InAudioDescriptor);
		}

	private:

		virtual void AddRef() override
		{
			++RefCounter;
		}

		virtual void Release() override
		{
			--RefCounter;
			if (RefCounter == 0)
			{
				delete this;
			}
		}

		virtual void OnInitializationCompleted(bool bSuccess)
		{
			FScopeLock Lock(&CallbackLock);
			if (Owner != nullptr)
			{
				Owner->SetState(bSuccess ? EMediaCaptureState::Capturing : EMediaCaptureState::Error);
			}
		}

		virtual void OnShutdownCompleted() override
		{
			FScopeLock Lock(&CallbackLock);
			if (Owner != nullptr)
			{
				Owner->SetState(EMediaCaptureState::Stopped);
				if (Owner->WakeUpEvent)
				{
					Owner->WakeUpEvent->Trigger();
				}
			}
		}


		virtual void OnOutputFrameCopied(const FFrameSentInfo& InFrameInfo)
		{
			FScopeLock Lock(&CallbackLock);
			if (Owner != nullptr)
			{
				if (Owner->WakeUpEvent)
				{
					Owner->WakeUpEvent->Trigger();
				}

				if (Owner->bLogDropFrame)
				{
					const uint32 FrameDropCount = InFrameInfo.FramesDropped;
					if (FrameDropCount > LastFramesDroppedCount)
					{
						UE_LOG(LogBlackmagicMediaOutput, Warning, TEXT("Lost %d frames on Blackmagic device %d. Frame rate may be too slow."), FrameDropCount - LastFramesDroppedCount, ChannelInfo.DeviceIndex);
					}
					LastFramesDroppedCount = FrameDropCount;
				}
			}
		}

		virtual void OnPlaybackStopped()
		{
			FScopeLock Lock(&CallbackLock);
			if (Owner != nullptr)
			{
				Owner->SetState(EMediaCaptureState::Error);
				if (Owner->WakeUpEvent)
				{
					Owner->WakeUpEvent->Trigger();
				}
			}
		}

		virtual void OnInterlacedOddFieldEvent()
		{
			FScopeLock Lock(&CallbackLock);
			if (Owner != nullptr && Owner->WakeUpEvent)
			{
				Owner->WakeUpEvent->Trigger();
			}
		}


	private:
		TAtomic<int32> RefCounter;
		mutable FCriticalSection CallbackLock;
		UBlackmagicMediaCapture* Owner;

		BlackmagicDesign::FChannelInfo ChannelInfo;
		BlackmagicDesign::FUniqueIdentifier BlackmagicIdendifier;
		uint32 LastFramesDroppedCount;
	};

	BlackmagicDesign::EFieldDominance GetFieldDominanceFromMediaStandard(EMediaIOStandardType StandardType)
	{
		switch(StandardType)
		{
			case EMediaIOStandardType::Interlaced:
				return BlackmagicDesign::EFieldDominance::Interlaced;
			case EMediaIOStandardType::ProgressiveSegmentedFrame:
				return BlackmagicDesign::EFieldDominance::ProgressiveSegmentedFrame;
			case EMediaIOStandardType::Progressive:
			default:
				return BlackmagicDesign::EFieldDominance::Progressive;
		}
	}

	BlackmagicDesign::EPixelFormat ConvertPixelFormat(EBlackmagicMediaOutputPixelFormat PixelFormat)
	{
		switch (PixelFormat)
        {
	        case EBlackmagicMediaOutputPixelFormat::PF_8BIT_YUV:
        		return BlackmagicDesign::EPixelFormat::pf_8Bits;
	        case EBlackmagicMediaOutputPixelFormat::PF_10BIT_YUV:
	        default:
        		return BlackmagicDesign::EPixelFormat::pf_10Bits;
        }
	}
	
	BlackmagicDesign::ETimecodeFormat ConvertTimecodeFormat(EMediaIOTimecodeFormat TimecodeFormat)
	{
		switch (TimecodeFormat)
		{
			case EMediaIOTimecodeFormat::LTC:
				return BlackmagicDesign::ETimecodeFormat::TCF_LTC;
			case EMediaIOTimecodeFormat::VITC:
				return BlackmagicDesign::ETimecodeFormat::TCF_VITC1;
			case EMediaIOTimecodeFormat::None:
			default:
				return BlackmagicDesign::ETimecodeFormat::TCF_None;
		}
	}

	BlackmagicDesign::ELinkConfiguration ConvertTransportType(EMediaIOTransportType TransportType, EMediaIOQuadLinkTransportType QuadlinkTransportType)
	{
		switch (TransportType)
		{
		case EMediaIOTransportType::SingleLink:
		case EMediaIOTransportType::HDMI: // Blackmagic support HDMI but it is not shown in UE's UI. It's configured in BMD design tool and it's considered a normal link by UE.
			return BlackmagicDesign::ELinkConfiguration::SingleLink;
		case EMediaIOTransportType::DualLink:
			return BlackmagicDesign::ELinkConfiguration::DualLink;
		case EMediaIOTransportType::QuadLink:
		default:
			if (QuadlinkTransportType == EMediaIOQuadLinkTransportType::SquareDivision)
			{
				return BlackmagicDesign::ELinkConfiguration::QuadLinkSqr;
			}
			return BlackmagicDesign::ELinkConfiguration::QuadLinkTSI;
		}
	}

	BlackmagicDesign::EAudioBitDepth ConvertAudioBitDepth(EBlackmagicMediaOutputAudioBitDepth BitDepth)
	{
		switch(BitDepth)
		{
		case EBlackmagicMediaOutputAudioBitDepth::Signed_16Bits: return BlackmagicDesign::EAudioBitDepth::Signed_16Bits;
		case EBlackmagicMediaOutputAudioBitDepth::Signed_32Bits: return BlackmagicDesign::EAudioBitDepth::Signed_32Bits;
		default:
			checkNoEntry();
			return BlackmagicDesign::EAudioBitDepth::Signed_32Bits;
		}
	}

	void EncodeTimecodeInTexel(EBlackmagicMediaOutputPixelFormat BlackmagicMediaOutputPixelFormat, EMediaCaptureConversionOperation ConversionOperation, BlackmagicDesign::FTimecode Timecode, void* Buffer, int32 Width, int32 Height)
	{
		switch (BlackmagicMediaOutputPixelFormat)
		{
			case EBlackmagicMediaOutputPixelFormat::PF_8BIT_YUV:
			{
				if (ConversionOperation == EMediaCaptureConversionOperation::RGBA8_TO_YUV_8BIT)
				{
					FMediaIOCoreEncodeTime EncodeTime(EMediaIOCoreEncodePixelFormat::CharUYVY, Buffer, Width * 4, Width * 2, Height);
					EncodeTime.Render(Timecode.Hours, Timecode.Minutes, Timecode.Seconds, Timecode.Frames);
					break;
				}
				else
				{
					FMediaIOCoreEncodeTime EncodeTime(EMediaIOCoreEncodePixelFormat::CharBGRA, Buffer, Width * 4, Width, Height);
					EncodeTime.Render(Timecode.Hours, Timecode.Minutes, Timecode.Seconds, Timecode.Frames);
					break;
				}
			}
			case EBlackmagicMediaOutputPixelFormat::PF_10BIT_YUV:
			{
				FMediaIOCoreEncodeTime EncodeTime(EMediaIOCoreEncodePixelFormat::YUVv210, Buffer, Width * 16, Width * 6, Height);
				EncodeTime.Render(Timecode.Hours, Timecode.Minutes, Timecode.Seconds, Timecode.Frames);
				break;
			}
		}
	}
}

/* namespace BlackmagicMediaCaptureDevice
*****************************************************************************/
namespace BlackmagicMediaCaptureDevice
{
	BlackmagicDesign::FTimecode ConvertToBlackmagicTimecode(const FTimecode& InTimecode, float InEngineFPS, float InBlackmagicFPS)
	{
		const float Divider = InEngineFPS / InBlackmagicFPS;

		BlackmagicDesign::FTimecode Timecode;
		Timecode.Hours = InTimecode.Hours;
		Timecode.Minutes = InTimecode.Minutes;
		Timecode.Seconds = InTimecode.Seconds;
		Timecode.Frames = int32(float(InTimecode.Frames) / Divider);
		Timecode.bIsDropFrame = InTimecode.bDropFrameFormat;
		return Timecode;
	}
}

#if WITH_EDITOR
namespace BlackmagicMediaCaptureAnalytics
{
	/**
	 * @EventName MediaFramework.BlackmagicCaptureStarted
	 * @Trigger Triggered when a Blackmagic capture of the viewport or render target is started.
	 * @Type Client
	 * @Owner MediaIO Team
	 */
	void SendCaptureEvent(const FIntPoint& Resolution, const FFrameRate FrameRate, const FString& CaptureType)
	{
		if (FEngineAnalytics::IsAvailable())
		{
			TArray<FAnalyticsEventAttribute> EventAttributes;
			EventAttributes.Add(FAnalyticsEventAttribute(TEXT("CaptureType"), CaptureType));
			EventAttributes.Add(FAnalyticsEventAttribute(TEXT("ResolutionWidth"), FString::Printf(TEXT("%d"), Resolution.X)));
			EventAttributes.Add(FAnalyticsEventAttribute(TEXT("ResolutionHeight"), FString::Printf(TEXT("%d"), Resolution.Y)));
			EventAttributes.Add(FAnalyticsEventAttribute(TEXT("FrameRate"), FrameRate.ToPrettyText().ToString()));
			FEngineAnalytics::GetProvider().RecordEvent(TEXT("MediaFramework.BlackmagicCaptureStarted"), EventAttributes);
		}
	}
}
#endif

///* UBlackmagicMediaCapture implementation
//*****************************************************************************/
UBlackmagicMediaCapture::UBlackmagicMediaCapture(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bWaitForSyncEvent(false)
	, bEncodeTimecodeInTexel(false)
	, bLogDropFrame(false)
	, BlackmagicMediaOutputPixelFormat(EBlackmagicMediaOutputPixelFormat::PF_8BIT_YUV)
	, bSavedIgnoreTextureAlpha(false)
	, bIgnoreTextureAlphaChanged(false)
	, FrameRate(30, 1)
	, WakeUpEvent(nullptr)
	, LastFrameDropCount_BlackmagicThread(0)
{
	bGPUTextureTransferAvailable = FBlackmagicMediaOutputModule::Get().IsGPUTextureTransferAvailable();
}

bool UBlackmagicMediaCapture::ValidateMediaOutput() const
{
	UBlackmagicMediaOutput* BlackmagicMediaOutput = Cast<UBlackmagicMediaOutput>(MediaOutput);
	if (!BlackmagicMediaOutput)
	{
		UE_LOG(LogBlackmagicMediaOutput, Error, TEXT("Can not start the capture. MediaOutput's class is not supported."));
		return false;
	}

	return true;
}

bool UBlackmagicMediaCapture::InitializeCapture()
{
	UBlackmagicMediaOutput* BlackmagicMediaOutput = CastChecked<UBlackmagicMediaOutput>(MediaOutput);
	BlackmagicMediaOutputPixelFormat = BlackmagicMediaOutput->PixelFormat;
	bool bInitialized = InitBlackmagic(BlackmagicMediaOutput);
	if(bInitialized)
	{
#if WITH_EDITOR
		BlackmagicMediaCaptureAnalytics::SendCaptureEvent(BlackmagicMediaOutput->GetRequestedSize(), FrameRate, GetCaptureSourceType());
#endif
	}

	return bInitialized;
}

bool UBlackmagicMediaCapture::PostInitializeCaptureViewport(TSharedPtr<FSceneViewport>& InSceneViewport)
{
	ApplyViewportTextureAlpha(InSceneViewport);
	return true;
}

bool UBlackmagicMediaCapture::UpdateSceneViewportImpl(TSharedPtr<FSceneViewport>& InSceneViewport)
{
	RestoreViewportTextureAlpha(GetCapturingSceneViewport());
	ApplyViewportTextureAlpha(InSceneViewport);
	return true;
}

bool UBlackmagicMediaCapture::UpdateRenderTargetImpl(UTextureRenderTarget2D* InRenderTarget)
{
	RestoreViewportTextureAlpha(GetCapturingSceneViewport());
	return true;
}

void UBlackmagicMediaCapture::StopCaptureImpl(bool bAllowPendingFrameToBeProcess)
{
	if (!bAllowPendingFrameToBeProcess)
	{
		{
			// Prevent the rendering thread from copying while we are stopping the capture.
			FScopeLock ScopeLock(&RenderThreadCriticalSection);
			ENQUEUE_RENDER_COMMAND(BlackmagicMediaCaptureInitialize)(
				[this](FRHICommandListImmediate& RHICmdList) mutable
				{
					for (FTextureRHIRef& Texture : TexturesToRelease)
					{
						BlackmagicDesign::UnregisterDMATexture(Texture->GetTexture2D()->GetNativeResource());
					}

					TexturesToRelease.Reset();

					if (EventCallback)
					{
						EventCallback->Uninitialize();
						EventCallback = nullptr;
					}
				});

			if (WakeUpEvent)
			{
				FPlatformProcess::ReturnSynchEventToPool(WakeUpEvent);
				WakeUpEvent = nullptr;
			}
		}

		RestoreViewportTextureAlpha(GetCapturingSceneViewport());

		AudioOutput.Reset();
	}
}

bool UBlackmagicMediaCapture::ShouldCaptureRHIResource() const
{
	// Todo: also test if dvp was initialized correctly.
	return bGPUTextureTransferAvailable && CVarBlackmagicEnableGPUDirect.GetValueOnAnyThread() == 1;
}

void UBlackmagicMediaCapture::ApplyViewportTextureAlpha(TSharedPtr<FSceneViewport> InSceneViewport)
{
	if (InSceneViewport.IsValid())
	{
		TSharedPtr<SViewport> Widget(InSceneViewport->GetViewportWidget().Pin());
		if (Widget.IsValid())
		{
			bSavedIgnoreTextureAlpha = Widget->GetIgnoreTextureAlpha();

			UBlackmagicMediaOutput* BlackmagicMediaOutput = CastChecked<UBlackmagicMediaOutput>(MediaOutput);
			if (BlackmagicMediaOutput->OutputConfiguration.OutputType == EMediaIOOutputType::FillAndKey)
			{
				if (bSavedIgnoreTextureAlpha)
				{
					bIgnoreTextureAlphaChanged = true;
					Widget->SetIgnoreTextureAlpha(false);
				}
			}
		}
	}
}

void UBlackmagicMediaCapture::RestoreViewportTextureAlpha(TSharedPtr<FSceneViewport> InSceneViewport)
{
	// restore the ignore texture alpha state
	if (bIgnoreTextureAlphaChanged)
	{
		if (InSceneViewport.IsValid())
		{
			TSharedPtr<SViewport> Widget(InSceneViewport->GetViewportWidget().Pin());
			if (Widget.IsValid())
			{
				Widget->SetIgnoreTextureAlpha(bSavedIgnoreTextureAlpha);
			}
		}
		bIgnoreTextureAlphaChanged = false;
	}
}

bool UBlackmagicMediaCapture::HasFinishedProcessing() const
{
	return Super::HasFinishedProcessing() || EventCallback == nullptr;
}

bool UBlackmagicMediaCapture::InitBlackmagic(UBlackmagicMediaOutput* InBlackmagicMediaOutput)
{
	check(InBlackmagicMediaOutput);

	IBlackmagicMediaModule& MediaModule = FModuleManager::LoadModuleChecked<IBlackmagicMediaModule>(TEXT("BlackmagicMedia"));
	if (!MediaModule.CanBeUsed())
	{
		UE_LOG(LogBlackmagicMediaOutput, Warning, TEXT("The BlackmagicMediaCapture can't open MediaOutput '%s' because Blackmagic card cannot be used. Are you in a Commandlet? You may override this behavior by launching with -ForceBlackmagicUsage"), *InBlackmagicMediaOutput->GetName());
		return false;
	}

	// Init general settings
	bWaitForSyncEvent = InBlackmagicMediaOutput->bWaitForSyncEvent;
	bEncodeTimecodeInTexel = InBlackmagicMediaOutput->bEncodeTimecodeInTexel;
	bLogDropFrame = InBlackmagicMediaOutput->bLogDropFrame;
	FrameRate = InBlackmagicMediaOutput->GetRequestedFrameRate();

	if (ShouldCaptureRHIResource())
	{
		if (InBlackmagicMediaOutput->OutputConfiguration.MediaConfiguration.MediaMode.Standard != EMediaIOStandardType::Progressive)
		{
			UE_LOG(LogBlackmagicMediaOutput, Warning, TEXT("GPU DMA is not supported with interlaced, defaulting to regular path."));
			bGPUTextureTransferAvailable = false;
		}
	}


	// Init Device options
	BlackmagicDesign::FOutputChannelOptions ChannelOptions;
	ChannelOptions.FormatInfo.DisplayMode = InBlackmagicMediaOutput->OutputConfiguration.MediaConfiguration.MediaMode.DeviceModeIdentifier;
	
	ChannelOptions.FormatInfo.Width = InBlackmagicMediaOutput->OutputConfiguration.MediaConfiguration.MediaMode.Resolution.X;
	ChannelOptions.FormatInfo.Height = InBlackmagicMediaOutput->OutputConfiguration.MediaConfiguration.MediaMode.Resolution.Y;
	ChannelOptions.FormatInfo.FrameRateNumerator = InBlackmagicMediaOutput->OutputConfiguration.MediaConfiguration.MediaMode.FrameRate.Numerator;
	ChannelOptions.FormatInfo.FrameRateDenominator = InBlackmagicMediaOutput->OutputConfiguration.MediaConfiguration.MediaMode.FrameRate.Denominator;
	ChannelOptions.bOutputAudio = InBlackmagicMediaOutput->bOutputAudio;
	ChannelOptions.AudioBitDepth = BlackmagicMediaCaptureHelpers::ConvertAudioBitDepth(InBlackmagicMediaOutput->AudioBitDepth);
	ChannelOptions.NumAudioChannels = static_cast<BlackmagicDesign::EAudioChannelConfiguration>(InBlackmagicMediaOutput->OutputChannelCount);
	ChannelOptions.AudioSampleRate = static_cast<BlackmagicDesign::EAudioSampleRate>(InBlackmagicMediaOutput->AudioSampleRate);

	ChannelOptions.FormatInfo.FieldDominance = BlackmagicMediaCaptureHelpers::GetFieldDominanceFromMediaStandard(InBlackmagicMediaOutput->OutputConfiguration.MediaConfiguration.MediaMode.Standard);
	ChannelOptions.PixelFormat = BlackmagicMediaCaptureHelpers::ConvertPixelFormat(InBlackmagicMediaOutput->PixelFormat);
	ChannelOptions.TimecodeFormat = BlackmagicMediaCaptureHelpers::ConvertTimecodeFormat(InBlackmagicMediaOutput->TimecodeFormat);
	ChannelOptions.LinkConfiguration = BlackmagicMediaCaptureHelpers::ConvertTransportType(InBlackmagicMediaOutput->OutputConfiguration.MediaConfiguration.MediaConnection.TransportType, InBlackmagicMediaOutput->OutputConfiguration.MediaConfiguration.MediaConnection.QuadTransportType);

	ChannelOptions.bOutputKey = InBlackmagicMediaOutput->OutputConfiguration.OutputType == EMediaIOOutputType::FillAndKey;
	ChannelOptions.NumberOfBuffers = FMath::Clamp(InBlackmagicMediaOutput->NumberOfBlackmagicBuffers, 3, 4);
	ChannelOptions.bOutputVideo = true;
	ChannelOptions.bOutputInterlacedFieldsTimecodeNeedToMatch = InBlackmagicMediaOutput->bInterlacedFieldsTimecodeNeedToMatch && InBlackmagicMediaOutput->OutputConfiguration.MediaConfiguration.MediaMode.Standard == EMediaIOStandardType::Interlaced && InBlackmagicMediaOutput->TimecodeFormat != EMediaIOTimecodeFormat::None;
	ChannelOptions.bLogDropFrames = bLogDropFrame;
	ChannelOptions.bUseGPUDMA = ShouldCaptureRHIResource();
	ChannelOptions.bScheduleInDifferentThread = InBlackmagicMediaOutput->bUseMultithreadedScheduling;

	AudioBitDepth = InBlackmagicMediaOutput->AudioBitDepth;
	bOutputAudio = InBlackmagicMediaOutput->bOutputAudio;
	NumOutputChannels = static_cast<uint8>(InBlackmagicMediaOutput->OutputChannelCount);

	if (GEngine && bOutputAudio)
	{
		UMediaIOCoreSubsystem::FCreateAudioOutputArgs Args;
		Args.NumOutputChannels = static_cast<int32>(ChannelOptions.NumAudioChannels);
		Args.TargetFrameRate = FrameRate;
		Args.MaxSampleLatency = Align(InBlackmagicMediaOutput->AudioBufferSize, 4);
		Args.OutputSampleRate = static_cast<uint32>(InBlackmagicMediaOutput->AudioSampleRate);
		AudioOutput = GEngine->GetEngineSubsystem<UMediaIOCoreSubsystem>()->CreateAudioOutput(Args);
	}
	
	check(EventCallback == nullptr);
	BlackmagicDesign::FChannelInfo ChannelInfo;
	ChannelInfo.DeviceIndex = InBlackmagicMediaOutput->OutputConfiguration.MediaConfiguration.MediaConnection.Device.DeviceIdentifier;
	EventCallback = new BlackmagicMediaCaptureHelpers::FBlackmagicMediaCaptureEventCallback(this, ChannelInfo);

	const bool bSuccess = EventCallback->Initialize(ChannelOptions);
	if (!bSuccess)
	{
		UE_LOG(LogBlackmagicMediaOutput, Warning, TEXT("The Blackmagic output port for '%s' could not be opened."), *InBlackmagicMediaOutput->GetName());
		EventCallback->Uninitialize();
		EventCallback = nullptr;
		return false;
	}

	if (bSuccess && bWaitForSyncEvent)
	{
		const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.VSync"));
		bool bLockToVsync = CVar->GetValueOnGameThread() != 0;
		if (bLockToVsync)
		{
			UE_LOG(LogBlackmagicMediaOutput, Warning, TEXT("The Engine use VSync and '%s' wants to wait for the sync event. This may break the \"gen-lock\"."));
		}

		const bool bIsManualReset = false;
		WakeUpEvent = FPlatformProcess::GetSynchEventFromPool(bIsManualReset);
	}

	if (ShouldCaptureRHIResource())
	{
		UE_LOG(LogBlackmagicMediaOutput, Display, TEXT("BlackmagicMedia capture started using GPU Direct"));
	}

	return true;
}

void UBlackmagicMediaCapture::LockDMATexture_RenderThread(FTextureRHIRef InTexture)
{
	if (ShouldCaptureRHIResource())
	{
		if (!TexturesToRelease.Contains(InTexture))
		{
			TexturesToRelease.Add(InTexture);

			FRHITexture2D* Texture = InTexture->GetTexture2D();
			BlackmagicDesign::FRegisterDMATextureArgs Args;
			Args.RHITexture = Texture->GetNativeResource();
			//Args.RHIResourceMemory = Texture->GetNativeResource(); todo: VulkanTexture->Surface->GetAllocationHandle for Vulkan
			BlackmagicDesign::RegisterDMATexture(Args);

		}

		{
			TRACE_CPUPROFILER_EVENT_SCOPE(UBlackmagicMediaCapture::LockDMATexture);
			BlackmagicDesign::LockDMATexture(InTexture->GetTexture2D()->GetNativeResource());
		}
	}
}

void UBlackmagicMediaCapture::UnlockDMATexture_RenderThread(FTextureRHIRef InTexture)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UBlackmagicMediaCapture::UnlockDMATexture);
	BlackmagicDesign::UnlockDMATexture(InTexture->GetTexture2D()->GetNativeResource());
}

void UBlackmagicMediaCapture::OnFrameCaptured_RenderingThread(const FCaptureBaseData& InBaseData, TSharedPtr<FMediaCaptureUserData, ESPMode::ThreadSafe> InUserData, void* InBuffer, int32 Width, int32 Height, int32 BytesPerRow)
{
	// Prevent the rendering thread from copying while we are stopping the capture.
	TRACE_CPUPROFILER_EVENT_SCOPE(UBlackmagicMediaCapture::OnFrameCaptured_RenderingThread);
	FScopeLock ScopeLock(&RenderThreadCriticalSection);
	if (EventCallback)
	{
		BlackmagicDesign::FTimecode Timecode = BlackmagicMediaCaptureDevice::ConvertToBlackmagicTimecode(InBaseData.SourceFrameTimecode, InBaseData.SourceFrameTimecodeFramerate.AsDecimal(), FrameRate.AsDecimal());

		if (bEncodeTimecodeInTexel)
		{
			BlackmagicMediaCaptureHelpers::EncodeTimecodeInTexel(BlackmagicMediaOutputPixelFormat, GetConversionOperation(), Timecode, InBuffer, Width, Height);
		}

		BlackmagicDesign::FFrameDescriptor Frame;
		Frame.VideoBuffer = reinterpret_cast<uint8_t*>(InBuffer);
		Frame.VideoWidth = Width;
		Frame.VideoHeight = Height;
		Frame.Timecode = Timecode;
		Frame.FrameIdentifier = InBaseData.SourceFrameNumber;
		Frame.bEvenFrame = GFrameCounterRenderThread % 2 == 0;

		bool bSent = false;
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(UBlackmagicMediaCapture::SendVideoFrameData);
			bSent = EventCallback->SendVideoFrameData(Frame);
		}
		
		
		if (bLogDropFrame && !bSent)
		{
			UE_LOG(LogBlackmagicMediaOutput, Warning, TEXT("Frame couldn't be sent to Blackmagic device. Engine might be running faster than output."));
		}

		OutputAudio_RenderingThread(InBaseData, Timecode);

		if (bBlackmagicWritInputRawDataCmdEnable)
		{
			FString OutputFilename;
			uint32 Stride = 0;

			switch (BlackmagicMediaOutputPixelFormat)
			{
			case EBlackmagicMediaOutputPixelFormat::PF_8BIT_YUV:
				if (GetConversionOperation() == EMediaCaptureConversionOperation::RGBA8_TO_YUV_8BIT)
				{
					OutputFilename = TEXT("Blackmagic_Input_8_YUV");
					Stride = Width * 4;
					break;
				}
				else
				{
					OutputFilename = TEXT("Blackmagic_Input_8_RGBA");
					Stride = Width * 4;
					break;
				}
			case EBlackmagicMediaOutputPixelFormat::PF_10BIT_YUV:
				OutputFilename = TEXT("Blackmagic_Input_10_YUV");
				Stride = Width * 16;
				break;
			}

			MediaIOCoreFileWriter::WriteRawFile(OutputFilename, reinterpret_cast<uint8*>(InBuffer), Stride * Height);
			bBlackmagicWritInputRawDataCmdEnable = false;
		}


		WaitForSync_RenderingThread();
	}
	else if (GetState() != EMediaCaptureState::Stopped)
	{
		SetState(EMediaCaptureState::Error);
	}
}

void UBlackmagicMediaCapture::OnRHIResourceCaptured_RenderingThread(const FCaptureBaseData& InBaseData, TSharedPtr<FMediaCaptureUserData, ESPMode::ThreadSafe> InUserData, FTextureRHIRef InTexture)
{
	if (!InTexture)
	{
		return;
	}
	// Prevent the rendering thread from copying while we are stopping the capture.
	TRACE_CPUPROFILER_EVENT_SCOPE(UBlackmagicMediaCapture::OnFrameCaptured_RenderingThread);

	FScopeLock ScopeLock(&RenderThreadCriticalSection);


	if (EventCallback)
	{
		BlackmagicDesign::FTimecode Timecode = BlackmagicMediaCaptureDevice::ConvertToBlackmagicTimecode(InBaseData.SourceFrameTimecode, InBaseData.SourceFrameTimecodeFramerate.AsDecimal(), FrameRate.AsDecimal());

		BlackmagicDesign::FFrameDescriptor_GPUDMA Frame;
		Frame.RHITexture = InTexture->GetTexture2D()->GetNativeResource();
		Frame.Timecode = Timecode;
		Frame.FrameIdentifier = InBaseData.SourceFrameNumber;
		Frame.bEvenFrame = GFrameCounterRenderThread % 2 == 0;

		bool bSent = false;
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(UBlackmagicMediaCapture::SendVideoFrameData_GPUDirect);
			bSent = EventCallback->SendVideoFrameData(Frame);
		}

		if (bLogDropFrame && !bSent)
		{
			UE_LOG(LogBlackmagicMediaOutput, Warning, TEXT("Frame couldn't be sent to Blackmagic device. Engine might be running faster than output."));
		}

		OutputAudio_RenderingThread(InBaseData, Timecode);

		WaitForSync_RenderingThread();
	}
	else if (GetState() != EMediaCaptureState::Stopped)
	{
		SetState(EMediaCaptureState::Error);
	}
}

void UBlackmagicMediaCapture::WaitForSync_RenderingThread()
{
	if (bWaitForSyncEvent)
	{
		if (WakeUpEvent && GetState() == EMediaCaptureState::Capturing) // In render thread, could be shutdown in a middle of a frame
		{
			const uint32 NumberOfMilliseconds = 1000;
			if (!WakeUpEvent->Wait(NumberOfMilliseconds))
			{
				SetState(EMediaCaptureState::Error);
				UE_LOG(LogBlackmagicMediaOutput, Error, TEXT("Could not synchronize with the device."));
			}
		}
	}
}

void UBlackmagicMediaCapture::OutputAudio_RenderingThread(const FCaptureBaseData& InBaseData, const BlackmagicDesign::FTimecode& InTimecode)
{
	if (bOutputAudio && AudioOutput)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(UBlackmagicMediaCapture::OutputAudio);

		const double NewTimestamp = FPlatformTime::Seconds();
		double CurrentFrameTime = NewTimestamp - OutputAudioTimestamp;
		
		const float TargetFrametime = 1 / FrameRate.AsDecimal();
		if (UNLIKELY(OutputAudioTimestamp == 0))
		{
			CurrentFrameTime = TargetFrametime;
		}

		float FrameTimeRatio = CurrentFrameTime / TargetFrametime;

		uint32_t NumSamplesToPull = FrameTimeRatio * AudioOutput->NumSamplesPerFrame;
		NumSamplesToPull = FMath::Clamp<uint32_t>(NumSamplesToPull, 0, AudioOutput->NumSamplesPerFrame);

		BlackmagicDesign::FAudioSamplesDescriptor AudioSamples;
		AudioSamples.Timecode = InTimecode;
		AudioSamples.FrameIdentifier = InBaseData.SourceFrameNumber;
		check(NumOutputChannels != 0);
		
		if (AudioBitDepth == EBlackmagicMediaOutputAudioBitDepth::Signed_32Bits)
		{
			TArray<int32> AudioBuffer = AudioOutput->GetAudioSamples<int32>(NumSamplesToPull);
			AudioSamples.AudioBuffer = reinterpret_cast<uint8_t*>(AudioBuffer.GetData());
			AudioSamples.NumAudioSamples = AudioBuffer.Num() / NumOutputChannels;
			AudioSamples.AudioBufferLength = AudioBuffer.Num() * sizeof(int32);
			EventCallback->SendAudioSamples(AudioSamples);
		}
		else if (AudioBitDepth == EBlackmagicMediaOutputAudioBitDepth::Signed_16Bits)
		{
			TArray<int16> AudioBuffer = AudioOutput->GetAudioSamples<int16>(NumSamplesToPull);
			AudioSamples.AudioBuffer = reinterpret_cast<uint8_t*>(AudioBuffer.GetData());
			AudioSamples.NumAudioSamples = AudioBuffer.Num() / NumOutputChannels;
			AudioSamples.AudioBufferLength = AudioBuffer.Num() * sizeof(int16);

			EventCallback->SendAudioSamples(AudioSamples);
		}
		else
		{
			checkNoEntry();
		}
		
		OutputAudioTimestamp = NewTimestamp;
	}
}
