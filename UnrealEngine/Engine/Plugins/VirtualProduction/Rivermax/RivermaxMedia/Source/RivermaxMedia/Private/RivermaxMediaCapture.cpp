// Copyright Epic Games, Inc. All Rights Reserved.

#include "RivermaxMediaCapture.h"

#include "Async/Async.h"
#include "Async/Fundamental/Task.h"
#include "IRivermaxCoreModule.h"
#include "IRivermaxManager.h"
#include "IRivermaxOutputStream.h"
#include "RenderGraphUtils.h"
#include "RivermaxMediaLog.h"
#include "RivermaxMediaOutput.h"
#include "RivermaxMediaUtils.h"
#include "RivermaxShaders.h"
#include "RivermaxTracingUtils.h"
#include "Slate/SceneViewport.h"
#include "Widgets/SViewport.h"

#if WITH_EDITOR
#include "AnalyticsEventAttribute.h"
#include "EngineAnalytics.h"
#endif


DECLARE_GPU_STAT(Rmax_Capture);
DECLARE_GPU_STAT(Rmax_FrameReservation);


/* namespace RivermaxMediaCaptureDevice
*****************************************************************************/

#if WITH_EDITOR
namespace RivermaxMediaCaptureAnalytics
{
	/**
	 * @EventName MediaFramework.RivermaxCaptureStarted
	 * @Trigger Triggered when a Rivermax capture of the viewport or render target is started.
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
			FEngineAnalytics::GetProvider().RecordEvent(TEXT("MediaFramework.RivermaxCaptureStarted"), EventAttributes);
		}
	}
}
#endif

namespace UE::RivermaxMediaCaptureUtil
{
	FIntPoint GetAlignedResolution(const UE::RivermaxCore::FVideoFormatInfo& InFormatInfo, const FIntPoint& InSize)
	{
		// We need to adjust horizontal resolution if it's not aligned to pgroup pixel coverage
		// RTP header has to describe a full pgroup even if it's outside of effective resolution.
		const uint32 PixelAlignment = InFormatInfo.PixelGroupCoverage;
		const uint32 AlignedHorizontalResolution = (InSize.X % PixelAlignment) ? InSize.X + (PixelAlignment - (InSize.X % PixelAlignment)) : InSize.X;
		return FIntPoint(AlignedHorizontalResolution, InSize.Y);
	}

	void GetOutputEncodingInfo(ERivermaxMediaOutputPixelFormat InPixelFormat, const FIntPoint& InSize, uint32& OutBytesPerElement, uint32& OutElementsPerFrame)
	{
		using namespace UE::RivermaxCore;

		const ESamplingType SamplingType = UE::RivermaxMediaUtils::Private::MediaOutputPixelFormatToRivermaxSamplingType(InPixelFormat);
		const FVideoFormatInfo Info = FStandardVideoFormat::GetVideoFormatInfo(SamplingType);

		// Compute horizontal byte count (stride) of aligned resolution
		const uint32 PixelAlignment = Info.PixelGroupCoverage;
		const FIntPoint AlignedResolution = GetAlignedResolution(Info, InSize);
		const uint32 PixelCount = AlignedResolution.X * AlignedResolution.Y;
		const uint32 PixelGroupCount = PixelCount / PixelAlignment;
		const uint32 FrameByteCount = PixelGroupCount * Info.PixelGroupSize;

		switch (InPixelFormat)
		{
		case ERivermaxMediaOutputPixelFormat::PF_8BIT_YUV422:
		{
			OutBytesPerElement = sizeof(UE::RivermaxShaders::FRGBToYUV8Bit422CS::FYUV8Bit422Buffer);
			break;
		}
		case ERivermaxMediaOutputPixelFormat::PF_10BIT_YUV422:
		{
			OutBytesPerElement = sizeof(UE::RivermaxShaders::FRGBToYUV10Bit422LittleEndianCS::FYUV10Bit422LEBuffer);
			break;
		}
		case ERivermaxMediaOutputPixelFormat::PF_8BIT_RGB:
		{
			OutBytesPerElement = sizeof(UE::RivermaxShaders::FRGBToRGB8BitCS::FRGB8BitBuffer);
			break;
		}
		case ERivermaxMediaOutputPixelFormat::PF_10BIT_RGB:
		{
			OutBytesPerElement = sizeof(UE::RivermaxShaders::FRGBToRGB10BitCS::FRGB10BitBuffer);
			break;
		}
		case ERivermaxMediaOutputPixelFormat::PF_12BIT_RGB:
		{
			OutBytesPerElement = sizeof(UE::RivermaxShaders::FRGBToRGB12BitCS::FRGB12BitBuffer);
			break;
		}
		case ERivermaxMediaOutputPixelFormat::PF_FLOAT16_RGB:
		{
			OutBytesPerElement = sizeof(UE::RivermaxShaders::FRGBToRGB16fCS::FRGB16fBuffer);
			break;
		}
		}

		// Shader encoding might not align with pixel group size so we need to have enough elements to represent the last pixel group
		OutElementsPerFrame = FMath::CeilToInt32((float)FrameByteCount / OutBytesPerElement);
	}
}


///* URivermaxMediaCapture implementation
//*****************************************************************************/

UE::RivermaxCore::FRivermaxOutputStreamOptions URivermaxMediaCapture::GetOutputStreamOptions() const
{
	return Options;
}

void URivermaxMediaCapture::GetLastPresentedFrameInformation(UE::RivermaxCore::FPresentedFrameInfo& OutFrameInfo) const
{
	if (RivermaxStream)
	{
		RivermaxStream->GetLastPresentedFrame(OutFrameInfo);
	}
}

bool URivermaxMediaCapture::ValidateMediaOutput() const
{
	URivermaxMediaOutput* RivermaxMediaOutput = Cast<URivermaxMediaOutput>(MediaOutput);
	if (!RivermaxMediaOutput)
	{
		UE_LOG(LogRivermaxMedia, Error, TEXT("Can not start the capture. MediaOutput's class is not supported."));
		return false;
	}

	return true;
}

bool URivermaxMediaCapture::InitializeCapture()
{
	URivermaxMediaOutput* RivermaxOutput = CastChecked<URivermaxMediaOutput>(MediaOutput);
	bool bResult = Initialize(RivermaxOutput);
#if WITH_EDITOR
	if (bResult)
	{
		RivermaxMediaCaptureAnalytics::SendCaptureEvent(GetDesiredSize(), RivermaxOutput->FrameRate, GetCaptureSourceType());
	}
#endif
	return bResult;
}

bool URivermaxMediaCapture::UpdateSceneViewportImpl(TSharedPtr<FSceneViewport>& InSceneViewport)
{
	return true;
}

bool URivermaxMediaCapture::UpdateRenderTargetImpl(UTextureRenderTarget2D* InRenderTarget)
{
	return true;
}

void URivermaxMediaCapture::StopCaptureImpl(bool bAllowPendingFrameToBeProcess)
{
	bIsActive = false;

	if (RivermaxStream)
	{
		RivermaxStream->Uninitialize();
		RivermaxStream.Reset();
	}
}

bool URivermaxMediaCapture::ShouldCaptureRHIResource() const
{
	if (RivermaxStream)
	{
		return RivermaxStream->IsGPUDirectSupported();
	}

	return false;
}

bool URivermaxMediaCapture::HasFinishedProcessing() const
{
	return Super::HasFinishedProcessing();
}

bool URivermaxMediaCapture::Initialize(URivermaxMediaOutput* InMediaOutput)
{
	using namespace UE::RivermaxCore;

	check(InMediaOutput);
	bIsActive = false;

	IRivermaxCoreModule* Module = FModuleManager::GetModulePtr<IRivermaxCoreModule>("RivermaxCore");
	if (Module)
	{
		if (ConfigureStream(InMediaOutput, Options))
		{
			RivermaxStream = Module->CreateOutputStream();
			if (RivermaxStream)
			{
				bIsActive = RivermaxStream->Initialize(Options, *this);
			}
		}
	}

	return bIsActive;
}

bool URivermaxMediaCapture::ConfigureStream(URivermaxMediaOutput* InMediaOutput, UE::RivermaxCore::FRivermaxOutputStreamOptions& OutOptions) const
{
	using namespace UE::RivermaxCore;

	// Resolve interface address
	IRivermaxCoreModule* Module = FModuleManager::GetModulePtr<IRivermaxCoreModule>("RivermaxCore");
	if (Module == nullptr)
	{
		return false;
	}
	
	if (!Module->GetRivermaxManager()->ValidateLibraryIsLoaded())
	{
		return false;
	}

	const bool bFoundDevice = Module->GetRivermaxManager()->GetMatchingDevice(InMediaOutput->InterfaceAddress, OutOptions.InterfaceAddress);
	if (bFoundDevice == false)
	{
		UE_LOG(LogRivermaxMedia, Error, TEXT("Could not find a matching interface for IP '%s'"), *InMediaOutput->InterfaceAddress);
		return false;
	}

	OutOptions.StreamAddress = InMediaOutput->StreamAddress;
	OutOptions.Port = InMediaOutput->Port;
	OutOptions.Resolution = GetDesiredSize();

	if (OutOptions.Resolution.X <= 0 || OutOptions.Resolution.Y <= 0)
	{
		UE_LOG(LogRivermaxMedia, Warning, TEXT("Can't start capture. Invalid resolution requested: %dx%d"), OutOptions.Resolution.X, OutOptions.Resolution.Y);
		return false;
	}

	OutOptions.FrameRate = InMediaOutput->FrameRate;
	OutOptions.NumberOfBuffers = InMediaOutput->PresentationQueueSize;
	OutOptions.bUseGPUDirect = InMediaOutput->bUseGPUDirect;
	OutOptions.AlignmentMode = UE::RivermaxMediaUtils::Private::MediaOutputAlignmentToRivermaxAlignment(InMediaOutput->AlignmentMode);
	OutOptions.FrameLockingMode = UE::RivermaxMediaUtils::Private::MediaOutputFrameLockingToRivermax(InMediaOutput->FrameLockingMode);

	// Setup alignment dependent configs
	OutOptions.bDoContinuousOutput = OutOptions.AlignmentMode == ERivermaxAlignmentMode::AlignmentPoint ? InMediaOutput->bDoContinuousOutput : false;
	OutOptions.bDoFrameCounterTimestamping = OutOptions.AlignmentMode == ERivermaxAlignmentMode::FrameCreation ? InMediaOutput->bDoFrameCounterTimestamping : false;

	OutOptions.PixelFormat = UE::RivermaxMediaUtils::Private::MediaOutputPixelFormatToRivermaxSamplingType(InMediaOutput->PixelFormat);
	const FVideoFormatInfo Info = FStandardVideoFormat::GetVideoFormatInfo(OutOptions.PixelFormat);
	OutOptions.AlignedResolution = UE::RivermaxMediaCaptureUtil::GetAlignedResolution(Info, OutOptions.Resolution);

	return true;
}

void URivermaxMediaCapture::AddFrameReservationPass(FRDGBuilder& GraphBuilder)
{
	RDG_GPU_STAT_SCOPE(GraphBuilder, Rmax_FrameReservation);
	SCOPED_DRAW_EVENT(GraphBuilder.RHICmdList, Rmax_FrameReservation);
	
	// Scene rendering will already be enqueued but capture conversion pass will not
	// Revisit to push slot reservation till last minute
	URivermaxMediaCapture* Capturer = this;
	GraphBuilder.RHICmdList.EnqueueLambda([Capturer, FrameCounter = GFrameCounterRenderThread](FRHICommandList& RHICmdList)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(RmaxFrameReservation);
		TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(*UE::RivermaxCore::FRivermaxTracingUtils::RmaxOutMediaCapturePipeTraceEvents[FrameCounter % 10]);

		if (Capturer->bIsActive)
		{
			Capturer->RivermaxStream->ReserveFrame(FrameCounter);
		}
	});
}

void URivermaxMediaCapture::OnFrameCapturedInternal_AnyThread(const FCaptureBaseData& InBaseData, TSharedPtr<FMediaCaptureUserData, ESPMode::ThreadSafe> InUserData, void* InBuffer, int32 Width, int32 Height, int32 BytesPerRow)
{
	using namespace UE::RivermaxCore;
	TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(*FRivermaxTracingUtils::RmaxOutMediaCapturePipeTraceEvents[InBaseData.SourceFrameNumberRenderThread % 10]);

	FRivermaxOutputVideoFrameInfo NewFrame;
	NewFrame.Height = Height;
	NewFrame.Width = Width;
	NewFrame.Stride = BytesPerRow;
	NewFrame.VideoBuffer = InBuffer;
	NewFrame.FrameIdentifier = InBaseData.SourceFrameNumberRenderThread;
	if (RivermaxStream->PushVideoFrame(NewFrame) == false)
	{
		UE_LOG(LogRivermaxMedia, Verbose, TEXT("Failed to pushed captured frame"));
	}
}

void URivermaxMediaCapture::OnRHIResourceCapturedInternal_AnyThread(const FCaptureBaseData& InBaseData, TSharedPtr<FMediaCaptureUserData, ESPMode::ThreadSafe> InUserData, FBufferRHIRef InBuffer)
{
	using namespace UE::RivermaxCore;
	TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(*FRivermaxTracingUtils::RmaxOutMediaCapturePipeTraceEvents[InBaseData.SourceFrameNumberRenderThread % 10]);

	FRivermaxOutputVideoFrameInfo NewFrame;
	NewFrame.FrameIdentifier = InBaseData.SourceFrameNumberRenderThread;
	NewFrame.GPUBuffer = InBuffer;
	if (RivermaxStream->PushVideoFrame(NewFrame) == false)
	{
		UE_LOG(LogRivermaxMedia, Verbose, TEXT("Failed to pushed captured frame"));
	}
}

void URivermaxMediaCapture::OnFrameCaptured_RenderingThread(const FCaptureBaseData& InBaseData, TSharedPtr<FMediaCaptureUserData, ESPMode::ThreadSafe> InUserData, void* InBuffer, int32 Width, int32 Height, int32 BytesPerRow)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(URivermaxMediaCapture::OnFrameCaptured_RenderingThread);
	OnFrameCapturedInternal_AnyThread(InBaseData, InUserData, InBuffer, Width, Height, BytesPerRow);
}

void URivermaxMediaCapture::OnRHIResourceCaptured_RenderingThread(const FCaptureBaseData& InBaseData, TSharedPtr<FMediaCaptureUserData, ESPMode::ThreadSafe> InUserData, FBufferRHIRef InBuffer)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(URivermaxMediaCapture::OnRHIResourceCaptured_RenderingThread);
	OnRHIResourceCapturedInternal_AnyThread(InBaseData, InUserData, InBuffer);
}

void URivermaxMediaCapture::OnRHIResourceCaptured_AnyThread(const FCaptureBaseData& InBaseData, TSharedPtr<FMediaCaptureUserData, ESPMode::ThreadSafe> InUserData, FBufferRHIRef InBuffer)
{
	using namespace UE::RivermaxCore;

	TRACE_CPUPROFILER_EVENT_SCOPE(URivermaxMediaCapture::OnRHIResourceCaptured_AnyThread);
	OnRHIResourceCapturedInternal_AnyThread(InBaseData, InUserData, InBuffer);
}

void URivermaxMediaCapture::OnFrameCaptured_AnyThread(const FCaptureBaseData& InBaseData, TSharedPtr<FMediaCaptureUserData, ESPMode::ThreadSafe> InUserData, const FMediaCaptureResourceData& InResourceData)
{
	using namespace UE::RivermaxCore;

	TRACE_CPUPROFILER_EVENT_SCOPE(URivermaxMediaCapture::OnFrameCaptured_AnyThread);
	OnFrameCapturedInternal_AnyThread(InBaseData, InUserData, InResourceData.Buffer, InResourceData.Width, InResourceData.Height, InResourceData.BytesPerRow);
}

FIntPoint URivermaxMediaCapture::GetCustomOutputSize(const FIntPoint& InSize) const
{
	URivermaxMediaOutput* RivermaxOutput = CastChecked<URivermaxMediaOutput>(MediaOutput);

	uint32 BytesPerElement = 0;
	uint32 ElementsPerFrame = 0;
	UE::RivermaxMediaCaptureUtil::GetOutputEncodingInfo(RivermaxOutput->PixelFormat, InSize, BytesPerElement, ElementsPerFrame);
	return FIntPoint(ElementsPerFrame, 1);
}

EMediaCaptureResourceType URivermaxMediaCapture::GetCustomOutputResourceType() const
{
	URivermaxMediaOutput* RivermaxOutput = CastChecked<URivermaxMediaOutput>(MediaOutput);

	switch (RivermaxOutput->PixelFormat)
	{
	case ERivermaxMediaOutputPixelFormat::PF_8BIT_YUV422:
	case ERivermaxMediaOutputPixelFormat::PF_10BIT_YUV422:
	case ERivermaxMediaOutputPixelFormat::PF_8BIT_RGB:
	case ERivermaxMediaOutputPixelFormat::PF_10BIT_RGB:
	case ERivermaxMediaOutputPixelFormat::PF_12BIT_RGB:
	case ERivermaxMediaOutputPixelFormat::PF_FLOAT16_RGB:
	{
		return EMediaCaptureResourceType::Buffer; //we use compute shader for all since output format doesn't match texture formats
	}
	default:
		return EMediaCaptureResourceType::Texture;
	}
}

FRDGBufferDesc URivermaxMediaCapture::GetCustomBufferDescription(const FIntPoint& InDesiredSize) const
{
	URivermaxMediaOutput* RivermaxOutput = CastChecked<URivermaxMediaOutput>(MediaOutput);

	uint32 BytesPerElement = 0;
	uint32 ElementsPerFrame = 0;
	UE::RivermaxMediaCaptureUtil::GetOutputEncodingInfo(RivermaxOutput->PixelFormat, InDesiredSize, BytesPerElement, ElementsPerFrame);
	FRDGBufferDesc Desc = FRDGBufferDesc::CreateStructuredDesc(BytesPerElement, ElementsPerFrame);
	
	// Required when GPUDirect using CUDA will be involved
	Desc.Usage |= EBufferUsageFlags::Shared;
	return Desc;
}

void URivermaxMediaCapture::OnCustomCapture_RenderingThread(FRDGBuilder& GraphBuilder, const FCaptureBaseData& InBaseData, TSharedPtr<FMediaCaptureUserData, ESPMode::ThreadSafe> InUserData, FRDGTextureRef InSourceTexture, FRDGBufferRef OutputBuffer, const FRHICopyTextureInfo& CopyInfo, FVector2D CropU, FVector2D CropV)
{
	RDG_GPU_STAT_SCOPE(GraphBuilder, Rmax_Capture)
	TRACE_CPUPROFILER_EVENT_SCOPE(URivermaxMediaCapture::OnCustomCapture_RenderingThread);
	TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(*UE::RivermaxCore::FRivermaxTracingUtils::RmaxOutMediaCapturePipeTraceEvents[GFrameCounterRenderThread % 10]);

	using namespace UE::RivermaxShaders;
	URivermaxMediaOutput* RivermaxOutput = CastChecked<URivermaxMediaOutput>(MediaOutput);

	FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);

	// Rectangle area to use from source. This is used when source render target is bigger than output resolution
	const FIntRect ViewRect(CopyInfo.GetSourceRect());
	constexpr bool bDoLinearToSRGB = false;
	const FIntPoint CaptureSize = Options.AlignedResolution;
	const FIntVector GroupCount = FComputeShaderUtils::GetGroupCount(DesiredOutputSize.X, 64);
	
	switch (RivermaxOutput->PixelFormat)
	{
	case ERivermaxMediaOutputPixelFormat::PF_8BIT_YUV422:
	{
		TShaderMapRef<FRGBToYUV8Bit422CS> ComputeShader(GlobalShaderMap);
		FRGBToYUV8Bit422CS::FParameters* Parameters = ComputeShader->AllocateAndSetParameters(GraphBuilder, InSourceTexture, CaptureSize, ViewRect, DesiredOutputSize, MediaShaders::RgbToYuvRec709Scaled, MediaShaders::YUVOffset8bits, bDoLinearToSRGB, OutputBuffer);

		FComputeShaderUtils::AddPass(
			GraphBuilder
			, RDG_EVENT_NAME("RGBAToYUV8Bit422")
			, ComputeShader
			, Parameters
			, GroupCount);

		break;
	}
	case ERivermaxMediaOutputPixelFormat::PF_10BIT_YUV422:
	{
		TShaderMapRef<FRGBToYUV10Bit422LittleEndianCS> ComputeShader(GlobalShaderMap);
		FRGBToYUV10Bit422LittleEndianCS::FParameters* Parameters = ComputeShader->AllocateAndSetParameters(GraphBuilder, InSourceTexture, CaptureSize, ViewRect, DesiredOutputSize, MediaShaders::RgbToYuvRec709Scaled, MediaShaders::YUVOffset10bits, bDoLinearToSRGB, OutputBuffer);

		FComputeShaderUtils::AddPass(
			GraphBuilder
			, RDG_EVENT_NAME("RGBAToYUV10Bit422LE")
			, ComputeShader
			, Parameters
			, GroupCount);
	
		break;
	}
	case ERivermaxMediaOutputPixelFormat::PF_8BIT_RGB:
	{
		TShaderMapRef<FRGBToRGB8BitCS> ComputeShader(GlobalShaderMap);
		FRGBToRGB8BitCS::FParameters* Parameters = ComputeShader->AllocateAndSetParameters(GraphBuilder, InSourceTexture, CaptureSize, ViewRect, DesiredOutputSize, OutputBuffer);

		FComputeShaderUtils::AddPass(
			GraphBuilder
			, RDG_EVENT_NAME("RGBAToRGB8Bit")
			, ComputeShader
			, Parameters
			, GroupCount);
		
		break;
	}
	case ERivermaxMediaOutputPixelFormat::PF_10BIT_RGB:
	{
		TShaderMapRef<FRGBToRGB10BitCS> ComputeShader(GlobalShaderMap);
		FRGBToRGB10BitCS::FParameters* Parameters = ComputeShader->AllocateAndSetParameters(GraphBuilder, InSourceTexture, CaptureSize, ViewRect, DesiredOutputSize, OutputBuffer);

		FComputeShaderUtils::AddPass(
			GraphBuilder
			, RDG_EVENT_NAME("RGBAToRGB10Bit")
			, ComputeShader
			, Parameters
			, GroupCount);

		break;
	}
	case ERivermaxMediaOutputPixelFormat::PF_12BIT_RGB:
	{
		TShaderMapRef<FRGBToRGB12BitCS> ComputeShader(GlobalShaderMap);
		FRGBToRGB12BitCS::FParameters* Parameters = ComputeShader->AllocateAndSetParameters(GraphBuilder, InSourceTexture, CaptureSize, ViewRect, DesiredOutputSize, OutputBuffer);

		FComputeShaderUtils::AddPass(
			GraphBuilder
			, RDG_EVENT_NAME("RGBAToRGB12Bit")
			, ComputeShader
			, Parameters
			, GroupCount);

		break;
	}
	case ERivermaxMediaOutputPixelFormat::PF_FLOAT16_RGB:
	{
		TShaderMapRef<FRGBToRGB16fCS> ComputeShader(GlobalShaderMap);
		FRGBToRGB16fCS::FParameters* Parameters = ComputeShader->AllocateAndSetParameters(GraphBuilder, InSourceTexture, CaptureSize, ViewRect, DesiredOutputSize, OutputBuffer);

		FComputeShaderUtils::AddPass(
			GraphBuilder
			, RDG_EVENT_NAME("RGBAToRGB16f")
			, ComputeShader
			, Parameters
			, GroupCount);

		break;
	}
	}
	
	AddFrameReservationPass(GraphBuilder);
}

bool URivermaxMediaCapture::IsReadyForFinishDestroy()
{
	return Super::IsReadyForFinishDestroy();
}

void URivermaxMediaCapture::OnInitializationCompleted(bool bHasSucceed)
{
	if (GetState() != EMediaCaptureState::Stopped)
	{
		SetState(bHasSucceed ? EMediaCaptureState::Capturing : EMediaCaptureState::Error);
	}
}

void URivermaxMediaCapture::OnStreamError()
{
	UE_LOG(LogRivermaxMedia, Error, TEXT("Outputstream has caught an error. Stopping capture."));
	if (GetState() != EMediaCaptureState::Stopped)
	{
		SetState(EMediaCaptureState::Error);
	}
}

void URivermaxMediaCapture::OnPreFrameEnqueue()
{
	// Will need to add some logic in that callback chain for the case where margin wasn't enough
	// For now, we act blindly that frames presented are all the same but we need a way to detect 
	// if it's not and correct it.
	TRACE_CPUPROFILER_EVENT_SCOPE(RmaxOutputSynchronization);
	OnOutputSynchronization.ExecuteIfBound();
}
