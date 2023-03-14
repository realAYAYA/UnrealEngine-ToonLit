// Copyright Epic Games, Inc. All Rights Reserved.

#include "AjaMediaCapture.h"

#include "AJALib.h"
#include "AjaDeviceProvider.h"
#include "AjaMediaOutput.h"
#include "AjaMediaOutputModule.h"
#include "Misc/CoreDelegates.h"
#include "Engine/Engine.h"
#include "HAL/Event.h"
#include "HAL/IConsoleManager.h"
#include "IAjaMediaOutputModule.h"
#include "IAjaMediaModule.h"
#include "MediaIOCoreFileWriter.h"
#include "MediaIOCoreSubsystem.h"
#include "Misc/ScopeLock.h"
#include "Slate/SceneViewport.h"
#include "Widgets/SViewport.h"

#if WITH_EDITOR
#include "AnalyticsEventAttribute.h"
#include "EngineAnalytics.h"
#include "Interfaces/IMainFrameModule.h"
#endif

static TAutoConsoleVariable<int32> CVarAjaEnableGPUDirect(
	TEXT("Aja.EnableGPUDirect"), 0,
	TEXT("Whether to enable GPU direct for faster video frame copies. (Experimental)"),
	ECVF_RenderThreadSafe);

/* namespace AjaMediaCaptureDevice
*****************************************************************************/
namespace AjaMediaCaptureDevice
{
	struct FAjaMediaEncodeOptions
	{
		FAjaMediaEncodeOptions(const int32 InWidth, const int32 InHeight, const EAjaMediaOutputPixelFormat InEncodePixelFormat, bool bInUseKey)
		{
			switch (InEncodePixelFormat)
			{
			case EAjaMediaOutputPixelFormat::PF_8BIT_YUV:
				if (bInUseKey)
				{
					Initialize(InWidth * 4, InWidth, EMediaIOCoreEncodePixelFormat::CharBGRA, TEXT("Aja_Input_8_RGBA"));
					break;
				}
				else
				{
					Initialize(InWidth * 4, InWidth * 2, EMediaIOCoreEncodePixelFormat::CharUYVY, TEXT("Aja_Input_8_YUV"));
					break;
				}
			case EAjaMediaOutputPixelFormat::PF_10BIT_YUV:
				if (bInUseKey)
				{
					Initialize(InWidth * 4, InWidth, EMediaIOCoreEncodePixelFormat::A2B10G10R10, TEXT("Aja_Input_10_RGBA"));
					break;
				}
				else
				{
					Initialize(InWidth * 16, InWidth * 6, EMediaIOCoreEncodePixelFormat::YUVv210, TEXT("Aja_Input_10_YUV"));
					break;
				}
			default:
				checkNoEntry();
			}
		}

		void Initialize(const uint32 InStride, const uint32 InTimeEncodeWidth, const EMediaIOCoreEncodePixelFormat InEncodePixelFormat, const FString InOutputFilename)
		{
			Stride = InStride;
			TimeEncodeWidth = InTimeEncodeWidth;
			EncodePixelFormat = InEncodePixelFormat;
			OutputFilename = InOutputFilename;
		}

		uint32 Stride;
		uint32 TimeEncodeWidth;
		EMediaIOCoreEncodePixelFormat EncodePixelFormat;
		FString OutputFilename;
	};
	
	AJA::FTimecode ConvertToAJATimecode(const FTimecode& InTimecode, float InEngineFPS, float InAjaFPS)
	{
		const float Divider = InEngineFPS / InAjaFPS;

		AJA::FTimecode Timecode;
		Timecode.Hours = InTimecode.Hours;
		Timecode.Minutes = InTimecode.Minutes;
		Timecode.Seconds = InTimecode.Seconds;
		Timecode.Frames = int32(float(InTimecode.Frames) / Divider);
		return Timecode;
	}
}

namespace AjaMediaCaptureUtils
{
	AJA::ETransportType ConvertTransportType(const EMediaIOTransportType TransportType, const EMediaIOQuadLinkTransportType QuadTransportType)
	{
		switch (TransportType)
		{
		case EMediaIOTransportType::SingleLink:
			return AJA::ETransportType::TT_SdiSingle;
		case EMediaIOTransportType::DualLink:
			return AJA::ETransportType::TT_SdiDual;
		case EMediaIOTransportType::QuadLink:
			return QuadTransportType == EMediaIOQuadLinkTransportType::SquareDivision ? AJA::ETransportType::TT_SdiQuadSQ : AJA::ETransportType::TT_SdiQuadTSI;
		case EMediaIOTransportType::HDMI:
			return AJA::ETransportType::TT_Hdmi;
		default:
			checkNoEntry();
			return AJA::ETransportType::TT_SdiSingle;
		}
	}
	
	AJA::EPixelFormat ConvertPixelFormat(EAjaMediaOutputPixelFormat PixelFormat, bool bUseKey)
	{
		switch (PixelFormat)
		{
		case EAjaMediaOutputPixelFormat::PF_8BIT_YUV:
			return bUseKey ? AJA::EPixelFormat::PF_8BIT_ARGB : AJA::EPixelFormat::PF_8BIT_YCBCR;
		case EAjaMediaOutputPixelFormat::PF_10BIT_YUV:
			return bUseKey ? AJA::EPixelFormat::PF_10BIT_RGB : AJA::EPixelFormat::PF_10BIT_YCBCR;
		default:
			return AJA::EPixelFormat::PF_8BIT_YCBCR;
		}
	}
	
	AJA::ETimecodeFormat ConvertTimecode(EMediaIOTimecodeFormat TimecodeFormat)
	{
		switch (TimecodeFormat)
		{
		case EMediaIOTimecodeFormat::None:
			return AJA::ETimecodeFormat::TCF_None;
		case EMediaIOTimecodeFormat::LTC:
			return AJA::ETimecodeFormat::TCF_LTC;
		case EMediaIOTimecodeFormat::VITC:
			return AJA::ETimecodeFormat::TCF_VITC1;
		default:
			return AJA::ETimecodeFormat::TCF_None;
		}
	}

	AJA::EAJAReferenceType Convert(EMediaIOReferenceType OutputReference)
	{
		switch(OutputReference)
		{
		case EMediaIOReferenceType::External:
			return AJA::EAJAReferenceType::EAJA_REFERENCETYPE_EXTERNAL;
		case EMediaIOReferenceType::Input:
			return AJA::EAJAReferenceType::EAJA_REFERENCETYPE_INPUT;
		default:
			return AJA::EAJAReferenceType::EAJA_REFERENCETYPE_FREERUN;
		}
	}
}

#if WITH_EDITOR
namespace AjaMediaCaptureAnalytics
{
	/**
	 * @EventName MediaFramework.AjaCaptureStarted
	 * @Trigger Triggered when a Aja capture of the viewport or render target is started.
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
			FEngineAnalytics::GetProvider().RecordEvent(TEXT("MediaFramework.AjaCaptureStarted"), EventAttributes);
		}
	}
}
#endif

bool bAjaWriteInputRawDataCmdEnable = false;
static FAutoConsoleCommand AjaWriteInputRawDataCmd(
	TEXT("Aja.WriteInputRawData"),
	TEXT("Write Aja raw input buffer to file."),
	FConsoleCommandDelegate::CreateLambda([]() { bAjaWriteInputRawDataCmdEnable = true; })
	);

///* FAjaOutputCallback definition
//*****************************************************************************/
struct UAjaMediaCapture::FAjaOutputCallback : public AJA::IAJAInputOutputChannelCallbackInterface
{
	virtual void OnInitializationCompleted(bool bSucceed) override;
	virtual bool OnRequestInputBuffer(const AJA::AJARequestInputBufferData& RequestBuffer, AJA::AJARequestedInputBufferData& OutRequestedBuffer) override;
	virtual bool OnInputFrameReceived(const AJA::AJAInputFrameData& InInputFrame, const AJA::AJAAncillaryFrameData& InAncillaryFrame, const AJA::AJAAudioFrameData& AudioFrame, const AJA::AJAVideoFrameData& VideoFrame) override;
	virtual bool OnOutputFrameCopied(const AJA::AJAOutputFrameData& InFrameData) override;
	virtual void OnOutputFrameStarted() override;
	virtual void OnCompletion(bool bSucceed) override;
	UAjaMediaCapture* Owner;

	/** Last frame drop count to detect count */
	uint64 LastFrameDropCount = 0;
	uint64 PreviousDroppedCount = 0;
};

///* FAjaOutputCallback definition
//*****************************************************************************/
struct UAjaMediaCapture::FAJAOutputChannel : public AJA::AJAOutputChannel
{
	FAJAOutputChannel() = default;
};

///* UAjaMediaCapture implementation
//*****************************************************************************/
UAjaMediaCapture::UAjaMediaCapture()
	: bWaitForSyncEvent(false)
	, bLogDropFrame(false)
	, bEncodeTimecodeInTexel(false)
	, PixelFormat(EAjaMediaOutputPixelFormat::PF_8BIT_YUV)
	, UseKey(false)
	, bSavedIgnoreTextureAlpha(false)
	, bIgnoreTextureAlphaChanged(false)
	, FrameRate(30, 1)
	, WakeUpEvent(nullptr)
{
	bGPUTextureTransferAvailable = ((FAjaMediaOutputModule*)&IAjaMediaOutputModule::Get())->IsGPUTextureTransferAvailable();

#if WITH_EDITOR
	// In editor, an asset re-save dialog can prevent AJA from cleaning up in the regular PreExit callback,
	// So we have to do our cleanup before the regular callback is called.
	IMainFrameModule& MainFrame = FModuleManager::LoadModuleChecked<IMainFrameModule>("MainFrame");
	CanCloseEditorDelegateHandle = MainFrame.RegisterCanCloseEditor(IMainFrameModule::FMainFrameCanCloseEditor::CreateUObject(this, &UAjaMediaCapture::CleanupPreEditorExit));
#else
	FCoreDelegates::OnEnginePreExit.AddUObject(this, &UAjaMediaCapture::OnEnginePreExit);
#endif
}

UAjaMediaCapture::~UAjaMediaCapture()
{
#if WITH_EDITOR
	if (IMainFrameModule* MainFrame = FModuleManager::GetModulePtr<IMainFrameModule>("MainFrame"))
	{
		MainFrame->UnregisterCanCloseEditor(CanCloseEditorDelegateHandle);
	}
#else
	FCoreDelegates::OnEnginePreExit.RemoveAll(this);
#endif
}

bool UAjaMediaCapture::ValidateMediaOutput() const
{
	UAjaMediaOutput* AjaMediaOutput = Cast<UAjaMediaOutput>(MediaOutput);
	if (!AjaMediaOutput)
	{
		UE_LOG(LogAjaMediaOutput, Error, TEXT("Can not start the capture. MediaSource's class is not supported."));
		return false;
	}

	return true;
}

bool UAjaMediaCapture::InitializeCapture()
{
	UAjaMediaOutput* AjaMediaSource = CastChecked<UAjaMediaOutput>(MediaOutput);
	const bool bResult = InitAJA(AjaMediaSource);
	if (bResult)
	{
#if WITH_EDITOR
		AjaMediaCaptureAnalytics::SendCaptureEvent(AjaMediaSource->GetRequestedSize(), FrameRate, GetCaptureSourceType());
#endif
	}
	return bResult;
}

bool UAjaMediaCapture::PostInitializeCaptureViewport(TSharedPtr<FSceneViewport>& InSceneViewport)
{
	ApplyViewportTextureAlpha(InSceneViewport);
	return true;
}

bool UAjaMediaCapture::UpdateSceneViewportImpl(TSharedPtr<FSceneViewport>& InSceneViewport)
{
	RestoreViewportTextureAlpha(GetCapturingSceneViewport());
	ApplyViewportTextureAlpha(InSceneViewport);
	return true;
}

bool UAjaMediaCapture::UpdateRenderTargetImpl(UTextureRenderTarget2D* InRenderTarget)
{
	RestoreViewportTextureAlpha(GetCapturingSceneViewport());
	return true;
}

void UAjaMediaCapture::StopCaptureImpl(bool bAllowPendingFrameToBeProcess)
{
	if (!bAllowPendingFrameToBeProcess)
	{
		{
			// Prevent the rendering thread from copying while we are stopping the capture.
			FScopeLock ScopeLock(&RenderThreadCriticalSection);

			ENQUEUE_RENDER_COMMAND(AjaMediaCaptureInitialize)(
			[this](FRHICommandListImmediate& RHICmdList) mutable
			{
				// Unregister texture before closing channel.
				if (ShouldCaptureRHIResource())
				{
					for (FTextureRHIRef& Texture : TexturesToRelease)
					{
						AJA::UnregisterDMATexture(Texture->GetTexture2D()->GetNativeResource());
					}

					TexturesToRelease.Reset();
				}

				if (OutputChannel)
				{
					// Close the aja channel in the another thread.
					OutputChannel->Uninitialize();
					OutputChannel.Reset();
					OutputCallback.Reset();
				}
			});
			
		
			if (WakeUpEvent)
			{
				FPlatformProcess::ReturnSynchEventToPool(WakeUpEvent);
				WakeUpEvent = nullptr;
			}
		}

		AudioOutput.Reset();

		RestoreViewportTextureAlpha(GetCapturingSceneViewport());
	}
}

bool UAjaMediaCapture::ShouldCaptureRHIResource() const
{
	return bGPUTextureTransferAvailable && CVarAjaEnableGPUDirect.GetValueOnAnyThread() == 1;
}

void UAjaMediaCapture::ApplyViewportTextureAlpha(TSharedPtr<FSceneViewport> InSceneViewport)
{
	if (InSceneViewport.IsValid())
	{
		TSharedPtr<SViewport> Widget(InSceneViewport->GetViewportWidget().Pin());
		if (Widget.IsValid())
		{
			bSavedIgnoreTextureAlpha = Widget->GetIgnoreTextureAlpha();

			UAjaMediaOutput* AjaMediaSource = CastChecked<UAjaMediaOutput>(MediaOutput);
			if (AjaMediaSource->OutputConfiguration.OutputType == EMediaIOOutputType::FillAndKey)
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

void UAjaMediaCapture::RestoreViewportTextureAlpha(TSharedPtr<FSceneViewport> InSceneViewport)
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

bool UAjaMediaCapture::CleanupPreEditorExit()
{
	OnEnginePreExit();
	return true;
}

void UAjaMediaCapture::OnEnginePreExit()
{
	if (OutputChannel)
	{
		// Close the aja channel in the another thread.
		OutputChannel->Uninitialize();
		OutputChannel.Reset();
		OutputCallback.Reset();
	}
}

bool UAjaMediaCapture::HasFinishedProcessing() const
{
	return Super::HasFinishedProcessing() || !OutputChannel;
}

bool UAjaMediaCapture::InitAJA(UAjaMediaOutput* InAjaMediaOutput)
{
	check(InAjaMediaOutput);

	IAjaMediaModule& MediaModule = FModuleManager::LoadModuleChecked<IAjaMediaModule>(TEXT("AjaMedia"));
	if (!MediaModule.CanBeUsed())
	{
		UE_LOG(LogAjaMediaOutput, Warning, TEXT("The AjaMediaCapture can't open MediaOutput '%s' because Aja card cannot be used. Are you in a Commandlet? You may override this behavior by launching with -ForceAjaUsage"), *InAjaMediaOutput->GetName());
		return false;
	}

	// Init general settings
	bWaitForSyncEvent = InAjaMediaOutput->bWaitForSyncEvent;
	bLogDropFrame = InAjaMediaOutput->bLogDropFrame;
	bEncodeTimecodeInTexel = InAjaMediaOutput->bEncodeTimecodeInTexel;
	FrameRate = InAjaMediaOutput->GetRequestedFrameRate();
	PortName = FAjaDeviceProvider().ToText(InAjaMediaOutput->OutputConfiguration.MediaConfiguration.MediaConnection).ToString();

	if (ShouldCaptureRHIResource())
	{
		if (InAjaMediaOutput->OutputConfiguration.MediaConfiguration.MediaMode.Standard != EMediaIOStandardType::Progressive)
		{
			UE_LOG(LogAjaMediaOutput, Warning, TEXT("GPU DMA is not supported with interlaced, defaulting to regular path."));
			bGPUTextureTransferAvailable = false;
		}
	}

	// Init Device options
	AJA::AJADeviceOptions DeviceOptions(InAjaMediaOutput->OutputConfiguration.MediaConfiguration.MediaConnection.Device.DeviceIdentifier);

	OutputCallback = MakePimpl<UAjaMediaCapture::FAjaOutputCallback>();
	OutputCallback->Owner = this;

	AJA::AJAVideoFormats::VideoFormatDescriptor Descriptor = AJA::AJAVideoFormats::GetVideoFormat(InAjaMediaOutput->OutputConfiguration.MediaConfiguration.MediaMode.DeviceModeIdentifier);

	// Init Channel options
	AJA::AJAInputOutputChannelOptions ChannelOptions(TEXT("ViewportOutput"), InAjaMediaOutput->OutputConfiguration.MediaConfiguration.MediaConnection.PortIdentifier);
	ChannelOptions.CallbackInterface = OutputCallback.Get();
	ChannelOptions.bOutput = true;
	ChannelOptions.NumberOfAudioChannel = static_cast<int32>(InAjaMediaOutput->NumOutputAudioChannels);
	ChannelOptions.SynchronizeChannelIndex = InAjaMediaOutput->OutputConfiguration.ReferencePortIdentifier;
	ChannelOptions.KeyChannelIndex = InAjaMediaOutput->OutputConfiguration.KeyPortIdentifier;
	ChannelOptions.OutputNumberOfBuffers = InAjaMediaOutput->NumberOfAJABuffers;
	ChannelOptions.VideoFormatIndex = InAjaMediaOutput->OutputConfiguration.MediaConfiguration.MediaMode.DeviceModeIdentifier;
	ChannelOptions.bUseAutoCirculating = InAjaMediaOutput->bOutputWithAutoCirculating;
	ChannelOptions.bUseKey = InAjaMediaOutput->OutputConfiguration.OutputType == EMediaIOOutputType::FillAndKey;  // must be RGBA to support Fill+Key
	ChannelOptions.bUseAncillary = false;
	ChannelOptions.bUseAudio = InAjaMediaOutput->bOutputAudio;
	ChannelOptions.bUseVideo = true;
	ChannelOptions.bOutputInterlacedFieldsTimecodeNeedToMatch = InAjaMediaOutput->bInterlacedFieldsTimecodeNeedToMatch && Descriptor.bIsInterlacedStandard && InAjaMediaOutput->TimecodeFormat != EMediaIOTimecodeFormat::None;
	ChannelOptions.bDisplayWarningIfDropFrames = bLogDropFrame;
	ChannelOptions.bConvertOutputLevelAToB = InAjaMediaOutput->bOutputIn3GLevelB && Descriptor.bIsVideoFormatA;
	ChannelOptions.TransportType = AjaMediaCaptureUtils::ConvertTransportType(InAjaMediaOutput->OutputConfiguration.MediaConfiguration.MediaConnection.TransportType, InAjaMediaOutput->OutputConfiguration.MediaConfiguration.MediaConnection.QuadTransportType);
	ChannelOptions.PixelFormat = AjaMediaCaptureUtils::ConvertPixelFormat(InAjaMediaOutput->PixelFormat, ChannelOptions.bUseKey);
	ChannelOptions.TimecodeFormat =  AjaMediaCaptureUtils::ConvertTimecode(InAjaMediaOutput->TimecodeFormat);
	ChannelOptions.OutputReferenceType = AjaMediaCaptureUtils::Convert(InAjaMediaOutput->OutputConfiguration.OutputReference);
	ChannelOptions.bUseGPUDMA = ShouldCaptureRHIResource();

	bOutputAudio = InAjaMediaOutput->bOutputAudio;
	
	if (GEngine && bOutputAudio)
	{
		UMediaIOCoreSubsystem::FCreateAudioOutputArgs Args;
		Args.NumOutputChannels = static_cast<uint32>(InAjaMediaOutput->NumOutputAudioChannels);
		Args.TargetFrameRate = FrameRate;
		Args.MaxSampleLatency = InAjaMediaOutput->AudioBufferSize;
		Args.OutputSampleRate = static_cast<uint32>(InAjaMediaOutput->AudioSampleRate);
		AudioOutput = GEngine->GetEngineSubsystem<UMediaIOCoreSubsystem>()->CreateAudioOutput(Args);
	}
	
	PixelFormat = InAjaMediaOutput->PixelFormat;
	UseKey = ChannelOptions.bUseKey;
	
	OutputChannel = MakePimpl<FAJAOutputChannel>();
	if (!OutputChannel->Initialize(DeviceOptions, ChannelOptions))
	{
		UE_LOG(LogAjaMediaOutput, Warning, TEXT("The AJA output port for '%s' could not be opened."), *InAjaMediaOutput->GetName());
		OutputChannel.Reset();
		OutputCallback.Reset();
		return false;
	}

	if (bWaitForSyncEvent)
	{
		const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.VSync"));
		bool bLockToVsync = CVar->GetValueOnGameThread() != 0;
		if (bLockToVsync)
		{
			UE_LOG(LogAjaMediaOutput, Warning, TEXT("The Engine use VSync and '%s' wants to wait for the sync event. This may break the \"gen-lock\"."));
		}

		const bool bIsManualReset = false;
		WakeUpEvent = FPlatformProcess::GetSynchEventFromPool(bIsManualReset);
	}

	if (ShouldCaptureRHIResource())
	{
		UE_LOG(LogAjaMediaOutput, Display, TEXT("Aja capture started using GPU Direct"));
	}

	return true;
}

void UAjaMediaCapture::OnFrameCaptured_RenderingThread(const FCaptureBaseData& InBaseData, TSharedPtr<FMediaCaptureUserData, ESPMode::ThreadSafe> InUserData, void* InBuffer, int32 Width, int32 Height, int32 BytesPerRow)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UAjaMediaCapture::OnFrameCaptured_RenderingThread);

	// Prevent the rendering thread from copying while we are stopping the capture.
	FScopeLock ScopeLock(&RenderThreadCriticalSection);
	if (OutputChannel)
	{
		const AJA::FTimecode Timecode = AjaMediaCaptureDevice::ConvertToAJATimecode(InBaseData.SourceFrameTimecode, InBaseData.SourceFrameTimecodeFramerate.AsDecimal(), FrameRate.AsDecimal());
		const AjaMediaCaptureDevice::FAjaMediaEncodeOptions EncodeOptions(Width, Height, PixelFormat, UseKey);
		
		if (bEncodeTimecodeInTexel)
		{
			const FMediaIOCoreEncodeTime EncodeTime(EncodeOptions.EncodePixelFormat, InBuffer, EncodeOptions.Stride, EncodeOptions.TimeEncodeWidth, Height);
			EncodeTime.Render(Timecode.Hours, Timecode.Minutes, Timecode.Seconds, Timecode.Frames);
		}

		AJA::AJAOutputFrameBufferData FrameBuffer;
		FrameBuffer.Timecode = Timecode;
		FrameBuffer.FrameIdentifier = InBaseData.SourceFrameNumber;
		FrameBuffer.bEvenFrame = GFrameCounterRenderThread % 2 == 0;

		bool bSetVideoResult = false;
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(UAjaMediaCapture::OnFrameCaptured_RenderingThread::SetVideo);
			bSetVideoResult = OutputChannel->SetVideoFrameData(FrameBuffer, reinterpret_cast<uint8_t*>(InBuffer), EncodeOptions.Stride * Height);
		}

		// If the set video call fails, that means we probably didn't find an available frame to write to,
		// so don't pop from the audio buffer since we would lose these samples in the SetAudioFrameData call.
		if (bSetVideoResult)
		{
			OutputAudio_RenderingThread(FrameBuffer);
		}

		if (bAjaWriteInputRawDataCmdEnable)
		{
			MediaIOCoreFileWriter::WriteRawFile(EncodeOptions.OutputFilename, reinterpret_cast<uint8*>(InBuffer), EncodeOptions.Stride * Height);
			bAjaWriteInputRawDataCmdEnable = false;
		}

		WaitForSync_RenderingThread();
		
	}
	else if (GetState() != EMediaCaptureState::Stopped)
	{
		SetState(EMediaCaptureState::Error);
	}
}

void UAjaMediaCapture::OnRHIResourceCaptured_RenderingThread(const FCaptureBaseData& InBaseData, TSharedPtr<FMediaCaptureUserData, ESPMode::ThreadSafe> InUserData, FTextureRHIRef InTexture)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UAjaMediaCapture::OnFrameCaptured_RenderingThread);
	
	// Prevent the rendering thread from copying while we are stopping the capture.
	FScopeLock ScopeLock(&RenderThreadCriticalSection);
	if (OutputChannel)
	{
		const AJA::FTimecode Timecode = AjaMediaCaptureDevice::ConvertToAJATimecode(InBaseData.SourceFrameTimecode, InBaseData.SourceFrameTimecodeFramerate.AsDecimal(), FrameRate.AsDecimal());
		
		AJA::AJAOutputFrameBufferData FrameBuffer;
		FrameBuffer.Timecode = Timecode;
		FrameBuffer.FrameIdentifier = InBaseData.SourceFrameNumber;

		bool bSetVideoResult = false;
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(UAjaMediaCapture::OnFrameCaptured_RenderingThread::SetVideo_GPUDirect);
			bSetVideoResult = OutputChannel->SetVideoFrameData(FrameBuffer, InTexture->GetTexture2D()->GetNativeResource());
		}

		// If the set video call fails, that means we probably didn't find an available frame to write to,
		// so don't pop from the audio buffer since we would lose these samples in the SetAudioFrameData call.
		if (bSetVideoResult)
		{
			OutputAudio_RenderingThread(FrameBuffer);
		}
		
		WaitForSync_RenderingThread();
		
	}
	else if (GetState() != EMediaCaptureState::Stopped)
	{
		SetState(EMediaCaptureState::Error);
	}
}

void UAjaMediaCapture::LockDMATexture_RenderThread(FTextureRHIRef InTexture)
{
	if (ShouldCaptureRHIResource())
	{
		if (!TexturesToRelease.Contains(InTexture))
		{
			TexturesToRelease.Add(InTexture);

			FRHITexture2D* Texture = InTexture->GetTexture2D();
			AJA::FRegisterDMATextureArgs Args;
			Args.RHITexture = Texture->GetNativeResource();
			//Args.InRHIResourceMemory = Texture->GetNativeResource(); todo: VulkanTexture->Surface->GetAllocationHandle for Vulkan
			AJA::RegisterDMATexture(Args);
		}
	}

	AJA::LockDMATexture(InTexture->GetTexture2D()->GetNativeResource());
}

void UAjaMediaCapture::UnlockDMATexture_RenderThread(FTextureRHIRef InTexture)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UAjaMediaCapture::OnFrameCaptured_RenderingThread::UnlockDMATexture);
	AJA::UnlockDMATexture(InTexture->GetTexture2D()->GetNativeResource());
}

void UAjaMediaCapture::WaitForSync_RenderingThread() const
{
	if (bWaitForSyncEvent)
	{
		if (WakeUpEvent && GetState() != EMediaCaptureState::Error) // In render thread, could be shutdown in a middle of a frame
		{
			WakeUpEvent->Wait();
		}
	}
}

void UAjaMediaCapture::OutputAudio_RenderingThread(const AJA::AJAOutputFrameBufferData& FrameBuffer) const
{
	if (bOutputAudio)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(UAjaMediaCapture::OnFrameCaptured_RenderingThread::SetAudio);
		TArray<int32> AudioSamples = AudioOutput->GetAudioSamples<int32>();
		OutputChannel->SetAudioFrameData(FrameBuffer, reinterpret_cast<uint8*>(AudioSamples.GetData()), AudioSamples.Num() * sizeof(int32));
	}
}

/* namespace IAJAInputCallbackInterface implementation
// This is called from the AJA thread. There's a lock inside AJA to prevent this object from dying while in this thread.
*****************************************************************************/
void UAjaMediaCapture::FAjaOutputCallback::OnInitializationCompleted(bool bSucceed)
{
	check(Owner);
	if (Owner->GetState() != EMediaCaptureState::Stopped)
	{
		Owner->SetState(bSucceed ? EMediaCaptureState::Capturing : EMediaCaptureState::Error);
	}

	if (Owner->WakeUpEvent)
	{
		Owner->WakeUpEvent->Trigger();
	}
}

bool UAjaMediaCapture::FAjaOutputCallback::OnOutputFrameCopied(const AJA::AJAOutputFrameData& InFrameData)
{
	const uint32 FrameDropCount = InFrameData.FramesDropped;
	if (Owner->bLogDropFrame)
	{
		if (FrameDropCount > LastFrameDropCount)
		{
			PreviousDroppedCount += FrameDropCount - LastFrameDropCount;

			static const int32 NumMaxFrameBeforeWarning = 50;
			if (PreviousDroppedCount % NumMaxFrameBeforeWarning == 0)
			{
				UE_LOG(LogAjaMediaOutput, Warning, TEXT("Loosing frames on AJA output %s. The current count is %d."), *Owner->PortName, PreviousDroppedCount);
			}
		}
		else if (PreviousDroppedCount > 0)
		{
			UE_LOG(LogAjaMediaOutput, Warning, TEXT("Lost %d frames on AJA output %s. Frame rate may be too slow."), PreviousDroppedCount, *Owner->PortName);
			PreviousDroppedCount = 0;
		}
	}
	LastFrameDropCount = FrameDropCount;

	return true;
}

void UAjaMediaCapture::FAjaOutputCallback::OnOutputFrameStarted()
{
	if (Owner->WakeUpEvent)
	{
		Owner->WakeUpEvent->Trigger();
	}
}

void UAjaMediaCapture::FAjaOutputCallback::OnCompletion(bool bSucceed)
{
	if (!bSucceed)
	{
		Owner->SetState(EMediaCaptureState::Error);
	}

	if (Owner->WakeUpEvent)
	{
		Owner->WakeUpEvent->Trigger();
	}
}

bool UAjaMediaCapture::FAjaOutputCallback::OnRequestInputBuffer(const AJA::AJARequestInputBufferData& RequestBuffer, AJA::AJARequestedInputBufferData& OutRequestedBuffer)
{
	check(false);
	return false;
}

bool UAjaMediaCapture::FAjaOutputCallback::OnInputFrameReceived(const AJA::AJAInputFrameData& InInputFrame, const AJA::AJAAncillaryFrameData& InAncillaryFrame, const AJA::AJAAudioFrameData& AudioFrame, const AJA::AJAVideoFrameData& VideoFrame)
{
	check(false);
	return false;
}
