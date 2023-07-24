// Copyright Epic Games, Inc. All Rights Reserved.

#include "MediaCapture.h"

#include "Application/ThrottleManager.h"
#include "Async/Async.h"
#include "Engine/GameEngine.h"
#include "Engine/RendererSettings.h"
#include "Engine/TextureRenderTarget2D.h"
#include "EngineModule.h"
#include "HAL/ThreadManager.h"
#include "MediaCaptureSceneViewExtension.h"
#include "MediaCaptureSources.h"
#include "MediaIOFrameManager.h"
#include "MediaIOCoreModule.h"
#include "MediaOutput.h"
#include "MediaShaders.h"
#include "Misc/App.h"
#include "Misc/ScopeLock.h"
#include "RendererInterface.h"
#include "RenderGraphResources.h"
#include "RenderGraphUtils.h"
#include "ScreenPass.h"
#include "Slate/SceneViewport.h"
#include "TextureResource.h"
#include "ProfilingDebugging/ScopedTimers.h"
#include "UObject/WeakObjectPtrTemplates.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MediaCapture)

#if WITH_EDITOR
#include "AnalyticsEventAttribute.h"
#include "Editor/EditorEngine.h"
#include "Engine/Engine.h"
#include "Engine/EngineTypes.h"
#include "EngineAnalytics.h"
#include "Framework/Notifications/NotificationManager.h"
#include "IAssetViewport.h"
#include "LevelEditor.h"
#include "Widgets/Notifications/SNotificationList.h"
#endif //WITH_EDITOR

#define LOCTEXT_NAMESPACE "MediaCapture"

static TAutoConsoleVariable<int32> CVarMediaIOEnableExperimentalScheduling(
	TEXT("MediaIO.EnableExperimentalScheduling"), 1,
	TEXT("Whether to send out frame  in a separate thread. (Experimental)"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarMediaIOScheduleOnAnyThread(
	TEXT("MediaIO.ScheduleOnAnyThread"), 1,
	TEXT("Whether to wait for resource readback in a separate thread. (Experimental)"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarMediaIOCapturePollTaskPriority(
	TEXT("MediaIO.Capture.PollTaskPriority"), static_cast<int32>(LowLevelTasks::ETaskPriority::High),
	TEXT("Priority of the task responsible to poll the render fence"),
	ECVF_RenderThreadSafe);

/** Time spent in media capture sending a frame. */
DECLARE_CYCLE_STAT(TEXT("MediaCapture RenderThread FrameCapture"), STAT_MediaCapture_RenderThread_FrameCapture, STATGROUP_Media);
DECLARE_CYCLE_STAT(TEXT("MediaCapture RenderThread LockResource"), STAT_MediaCapture_RenderThread_LockResource, STATGROUP_Media);
DECLARE_CYCLE_STAT(TEXT("MediaCapture AnyThread LockResource"), STAT_MediaCapture_AnyThread_LockResource, STATGROUP_Media);
DECLARE_CYCLE_STAT(TEXT("MediaCapture RenderThread RHI Capture Callback"), STAT_MediaCapture_RenderThread_RHI_CaptureCallback, STATGROUP_Media);
DECLARE_CYCLE_STAT(TEXT("MediaCapture RenderThread CPU Capture Callback"), STAT_MediaCapture_RenderThread_CaptureCallback, STATGROUP_Media);
DECLARE_GPU_STAT(MediaCapture_CaptureFrame);
DECLARE_GPU_STAT(MediaCapture_CustomCapture);
DECLARE_GPU_STAT(MediaCapture_Conversion);
DECLARE_GPU_STAT(MediaCapture_Readback);
DECLARE_GPU_STAT(MediaCapture_ProcessCapture);
DECLARE_GPU_STAT(MediaCapture_SyncPointPass);


/* namespace MediaCaptureDetails definition
*****************************************************************************/

namespace MediaCaptureDetails
{
	bool FindSceneViewportAndLevel(TSharedPtr<FSceneViewport>& OutSceneViewport);

	//Validation that there is a capture
	bool ValidateIsCapturing(const UMediaCapture* CaptureToBeValidated);

	void ShowSlateNotification();

	/** Returns bytes per pixel based on pixel format */
	int32 GetBytesPerPixel(EPixelFormat InPixelFormat);

	static const FName LevelEditorName(TEXT("LevelEditor"));
}

#if WITH_EDITOR
namespace MediaCaptureAnalytics
{
	/**
	 * @EventName MediaFramework.CaptureStarted
	 * @Trigger Triggered when a capture of the viewport or render target is started.
	 * @Type Client
	 * @Owner MediaIO Team
	 */
	void SendCaptureEvent(const FString& CaptureType)
	{
		if (FEngineAnalytics::IsAvailable())
		{
			TArray<FAnalyticsEventAttribute> EventAttributes;
			EventAttributes.Add(FAnalyticsEventAttribute(TEXT("CaptureType"), CaptureType));
			FEngineAnalytics::GetProvider().RecordEvent(TEXT("MediaFramework.CaptureStarted"), EventAttributes);
		}
	}
}
#endif

namespace UE::MediaCaptureData
{
	class FCaptureFrame : public FFrame
	{
	public:
		FCaptureFrame(int32 InFrameId)
			: FrameId(InFrameId)
		{
		}

		virtual int32 GetId() const override
		{
			return FrameId;
		}

		virtual bool IsPending() const override
		{
			return bReadbackRequested || bDoingGPUCopy; 
		}
		
		/** Returns true if its output resource is valid */
		virtual bool HasValidResource() const = 0;
		
		virtual FRDGViewableResource* RegisterResource(FRDGBuilder& RDGBuilder) = 0;

		/** Simple way to validate the resource type and cast safely */
		virtual bool IsTextureResource() const = 0;
		virtual bool IsBufferResource() const = 0;

		/** Locks the readback resource and returns a pointer to access data from system memory */
		virtual void* Lock(FRHICommandListImmediate& RHICmdList, int32& OutRowStride) = 0;

		virtual void* Lock_Unsafe(int32& OutRowStride) { return nullptr; }

		/** Unlocks the readback resource */
		virtual void Unlock() = 0;

		virtual void Unlock_Unsafe() {}

		/** Returns true if the readback is ready to be used */
		virtual bool IsReadbackReady(FRHIGPUMask GPUMask) = 0;
		
		virtual void EnqueueCopy(FRDGBuilder& RDGBuilder, FRDGViewableResource* ResourceToReadback, bool bIsAnyThreadSupported) = 0;
		
		virtual FRHITexture* GetTextureResource() { return nullptr; }
		virtual FRHIBuffer* GetBufferResource() { return nullptr; }
		virtual FRHIResource* GetRHIResource() = 0;

		int32 FrameId = 0;
		UMediaCapture::FCaptureBaseData CaptureBaseData;
		std::atomic<bool> bReadbackRequested = false;
		std::atomic<bool> bDoingGPUCopy = false;
		std::atomic<bool> bMediaCaptureActive = true;
		TSharedPtr<FMediaCaptureUserData> UserData;
	};

	/** Parameter to make our sync pass needing the convert pass as a prereq */
	BEGIN_SHADER_PARAMETER_STRUCT(FMediaCaptureTextureSyncPassParameters, )
		RDG_TEXTURE_ACCESS(Resource, ERHIAccess::CopySrc)
	END_SHADER_PARAMETER_STRUCT()

	BEGIN_SHADER_PARAMETER_STRUCT(FMediaCaptureBufferSyncPassParameters, )
		RDG_BUFFER_ACCESS(Resource, ERHIAccess::CopySrc)
	END_SHADER_PARAMETER_STRUCT()
	
	class FTextureCaptureFrame : public FCaptureFrame
	{
	public:
		/** Type alias for the output resource type used during capture frame */
		using FOutputResourceType = FRDGTextureRef;
		using PassParametersType = FMediaCaptureTextureSyncPassParameters;

		FTextureCaptureFrame(int32 InFrameId)
			: FCaptureFrame(InFrameId)
		{
		}

		//~ Begin FCaptureFrame interface
		virtual bool HasValidResource() const override
		{
			return RenderTarget != nullptr;
		}

		virtual bool IsTextureResource() const override
		{
			return true;
		}

		virtual bool IsBufferResource() const override
		{
			return false;
		}

		virtual void* Lock(FRHICommandListImmediate& RHICmdList, int32& OutRowStride) override
		{
			if (ReadbackTexture->IsReady() == false)
			{
				UE_LOG(LogMediaIOCore, Verbose, TEXT("Fence for texture readback was not ready"));
			}

			int32 ReadbackWidth;
			void* ReadbackPointer = ReadbackTexture->Lock(ReadbackWidth);
			OutRowStride = ReadbackWidth * MediaCaptureDetails::GetBytesPerPixel(RenderTarget->GetDesc().Format);
			return ReadbackPointer;
		}

		virtual void* Lock_Unsafe(int32& OutRowStride) override
		{
			void* ReadbackPointer = nullptr;
			int32 ReadbackWidth, ReadbackHeight;
			GDynamicRHI->RHIMapStagingSurface(ReadbackTexture->DestinationStagingTextures[ReadbackTexture->GetLastCopyGPUMask().GetFirstIndex()], nullptr, ReadbackPointer, ReadbackWidth, ReadbackHeight, ReadbackTexture->GetLastCopyGPUMask().GetFirstIndex());
			OutRowStride = ReadbackWidth * MediaCaptureDetails::GetBytesPerPixel(RenderTarget->GetDesc().Format);
			return ReadbackPointer;
		}

		virtual void Unlock() override
		{
			ReadbackTexture->Unlock();
		}

		virtual void Unlock_Unsafe() override
		{
			GDynamicRHI->RHIUnmapStagingSurface(ReadbackTexture->DestinationStagingTextures[ReadbackTexture->GetLastCopyGPUMask().GetFirstIndex()], ReadbackTexture->GetLastCopyGPUMask().GetFirstIndex());
		}

		virtual bool IsReadbackReady(FRHIGPUMask GPUMask) override
		{
			return ReadbackTexture->IsReady(GPUMask);
		}

		//~ End FCaptureFrame interface

		/** Registers an external texture to be tracked by the graph and returns a pointer to the tracked resource */
		virtual FRDGViewableResource* RegisterResource(FRDGBuilder& RDGBuilder) override
		{
			return RDGBuilder.RegisterExternalTexture(RenderTarget, TEXT("OutputTexture"));
		}

		/** Adds a readback pass to the graph */
		virtual void EnqueueCopy(FRDGBuilder& RDGBuilder, FRDGViewableResource* ResourceToReadback, bool bIsAnyThreadSupported) override
		{
			AddEnqueueCopyPass(RDGBuilder, ReadbackTexture.Get(), static_cast<FRDGTexture*>(ResourceToReadback));
		}

		virtual FRHITexture* GetTextureResource() override
		{
			return static_cast<FRHITexture*>(GetRHIResource());
		}

		/** Returns RHI resource of the allocated pooled resource */
		virtual FRHIResource* GetRHIResource() override
		{
			return RenderTarget->GetRHI();
		}
		
	public:
		TRefCountPtr<IPooledRenderTarget> RenderTarget;
		TUniquePtr<FRHIGPUTextureReadback> ReadbackTexture;
	};

	class FBufferCaptureFrame : public FCaptureFrame, public TSharedFromThis<UE::MediaCaptureData::FBufferCaptureFrame, ESPMode::ThreadSafe>
	{
	public:
		/** Type alias for the output resource type used during capture frame */
		using FOutputResourceType = FRDGBufferRef;
		using PassParametersType = FMediaCaptureBufferSyncPassParameters;
		
		FBufferCaptureFrame(int32 InFrameId)
			: FCaptureFrame(InFrameId)
		{
		}

		//~ Begin FCaptureFrame interface
		virtual bool HasValidResource() const override
		{
			return Buffer != nullptr;
		}

		virtual bool IsTextureResource() const override
		{
			return false;
		}

		virtual bool IsBufferResource() const override
		{
			return true;
		}

		virtual void* Lock(FRHICommandListImmediate& RHICmdList, int32& OutRowStride) override
		{
			if (ReadbackBuffer->IsReady() == false)
			{
				UE_LOG(LogMediaIOCore, Verbose, TEXT("Fence for buffer readback was not ready, blocking."));
				RHICmdList.BlockUntilGPUIdle();
			}

			OutRowStride = Buffer->GetRHI()->GetStride();
			return ReadbackBuffer->Lock(Buffer->GetRHI()->GetSize());
		}

		virtual void* Lock_Unsafe(int32& OutRowStride) override
		{
			void* ReadbackPointer = GDynamicRHI->RHILockStagingBuffer(DestinationStagingBuffers[LastCopyGPUMask.GetFirstIndex()], nullptr, 0, Buffer->GetRHI()->GetSize());
			OutRowStride = Buffer->GetRHI()->GetStride();
			return ReadbackPointer;
		}
		
		virtual void Unlock() override
		{
			ReadbackBuffer->Unlock();
		}
		
		virtual void Unlock_Unsafe() override
		{
			GDynamicRHI->RHIUnlockStagingBuffer(DestinationStagingBuffers[LastCopyGPUMask.GetFirstIndex()]);
		}

		virtual bool IsReadbackReady(FRHIGPUMask GPUMask) override
		{
			return ReadbackBuffer->IsReady(GPUMask);
		}
		//~ End FCaptureFrame interface

		/** Registers an external texture to be tracked by the graph and returns a pointer to the tracked resource */
		virtual FRDGViewableResource* RegisterResource(FRDGBuilder& RDGBuilder) override
		{
			return RDGBuilder.RegisterExternalBuffer(Buffer, TEXT("OutputBuffer"));
		}

		BEGIN_SHADER_PARAMETER_STRUCT(FEnqueueCopyBufferPass, )
		RDG_BUFFER_ACCESS(Buffer, ERHIAccess::CopySrc)
		END_SHADER_PARAMETER_STRUCT()


		/** Adds a readback pass to the graph */
		virtual void EnqueueCopy(FRDGBuilder& RDGBuilder, FRDGViewableResource* ResourceToReadback, bool bIsAnyThreadSupported) override
		{
			if (bIsAnyThreadSupported)
            {
				FEnqueueCopyBufferPass* PassParameters = RDGBuilder.AllocParameters<FEnqueueCopyBufferPass>();
				PassParameters->Buffer = static_cast<FRDGBuffer*>(ResourceToReadback);

				TSharedPtr<FBufferCaptureFrame> CaptureFramePtr = AsShared();
				RDGBuilder.AddPass(
					RDG_EVENT_NAME("EnqueueCopy(%s)", ResourceToReadback->Name),
					PassParameters,
					ERDGPassFlags::Readback,
					[CaptureFramePtr, ResourceToReadback](FRHICommandList& RHICmdList)
				{
					CaptureFramePtr->LastCopyGPUMask = RHICmdList.GetGPUMask();

					for (uint32 GPUIndex : CaptureFramePtr->LastCopyGPUMask)
					{
						SCOPED_GPU_MASK(RHICmdList, FRHIGPUMask::FromIndex(GPUIndex));

						if (!CaptureFramePtr->DestinationStagingBuffers[GPUIndex])
						{
							CaptureFramePtr->DestinationStagingBuffers[GPUIndex] = RHICreateStagingBuffer();
						}

						RHICmdList.CopyToStagingBuffer(static_cast<FRHIBuffer*>(ResourceToReadback->GetRHI()), CaptureFramePtr->DestinationStagingBuffers[GPUIndex], 0, CaptureFramePtr->Buffer->GetRHI()->GetSize());
					}
				});
            }
			else
			{
				AddEnqueueCopyPass(RDGBuilder, ReadbackBuffer.Get(), static_cast<FRDGBuffer*>(ResourceToReadback), Buffer->GetRHI()->GetSize());
			}
		}

		virtual FRHIBuffer* GetBufferResource() override
		{
			return static_cast<FRHIBuffer*>(GetRHIResource());
		}

		/** Returns RHI resource of the allocated pooled resource */
		virtual FRHIResource* GetRHIResource() override
		{
			return Buffer->GetRHI();
		}

	public:
		TRefCountPtr<FRDGPooledBuffer> Buffer;
		TUniquePtr<FRHIGPUBufferReadback> ReadbackBuffer;
		
// Used for the ExperimentalScheduling path
#if WITH_MGPU
		FStagingBufferRHIRef DestinationStagingBuffers[MAX_NUM_GPUS];
#else
		FStagingBufferRHIRef DestinationStagingBuffers[1];
#endif

		// Copied from FRHIGPUReadback.h
		FRHIGPUMask LastCopyGPUMask;
		uint32 LastLockGPUIndex = 0; 
	};

	/** Helper struct to contain arguments for CaptureFrame */
	struct FCaptureFrameArgs
	{
		FRDGBuilder& GraphBuilder;
		TObjectPtr<UMediaCapture> MediaCapture = nullptr;
		FTexture2DRHIRef ResourceToCapture;
		FRDGTextureRef RDGResourceToCapture = nullptr;
		FIntPoint DesiredSize = FIntPoint::ZeroValue;
		FIntRect SourceViewRect{0,0,0,0};

		EPixelFormat GetFormat() const
		{
			if (RDGResourceToCapture)
			{
				return RDGResourceToCapture->Desc.Format;
			}
			return ResourceToCapture->GetFormat();
		}

		bool HasValidResource() const
		{
			return ResourceToCapture || RDGResourceToCapture;
		}

		FIntPoint GetSizeXY() const
		{
			if (RDGResourceToCapture)
			{
				return FIntPoint(RDGResourceToCapture->Desc.GetSize().X, RDGResourceToCapture->Desc.GetSize().Y);
			}
			return ResourceToCapture->GetSizeXY();
		}

		int32 GetSizeX() const
		{
			return GetSizeXY().X;
		}

		int32 GetSizeY() const
		{
			return GetSizeXY().Y;
		}
	};

	/** Helper struct to contain arguments for AddConversionPass */
	struct FConversionPassArgs
	{
		const FRDGTextureRef& SourceRGBTexture;
		bool bRequiresFormatConversion = false;
		FRHICopyTextureInfo CopyInfo;
		FVector2D SizeU = FVector2D::ZeroVector;
		FVector2D SizeV = FVector2D::ZeroVector;
	};
	
	/** Helper class to be able to friend it and call methods on input media capture */
	class FMediaCaptureHelper
	{
	public:

		static bool AreInputsValid(const FCaptureFrameArgs& Args)
		{
			// If it is a simple rgba swizzle we can handle the conversion. Supported formats
			// contained in SupportedRgbaSwizzleFormats. Warning would've been displayed on start of capture.
			if (Args.MediaCapture->DesiredPixelFormat != Args.GetFormat() &&
				(!UMediaCapture::GetSupportedRgbaSwizzleFormats().Contains(Args.GetFormat()) || !Args.MediaCapture->DesiredCaptureOptions.bConvertToDesiredPixelFormat))
			{
				UE_LOG(LogMediaIOCore, Error, TEXT("The capture will stop for '%s'. The Source pixel format doesn't match with the user requested pixel format. %sRequested: %s Source: %s")
					, *Args.MediaCapture->MediaOutputName
					, (UMediaCapture::GetSupportedRgbaSwizzleFormats().Contains(Args.GetFormat()) && !Args.MediaCapture->DesiredCaptureOptions.bConvertToDesiredPixelFormat) ? TEXT("Please enable \"Convert To Desired Pixel Format\" option in Media Capture settings. ") : TEXT("")
					, GetPixelFormatString(Args.MediaCapture->DesiredPixelFormat)
					, GetPixelFormatString(Args.GetFormat()));

				return false;
			}

			bool bFoundSizeMismatch = false;
			FIntPoint RequestSize = FIntPoint::ZeroValue;
			if (Args.MediaCapture->DesiredCaptureOptions.Crop == EMediaCaptureCroppingType::None)
			{
				if (Args.SourceViewRect.Area() != 0)
				{
					if (Args.DesiredSize.X != Args.SourceViewRect.Width() || Args.DesiredSize.Y != Args.SourceViewRect.Height())
					{
						RequestSize = { Args.SourceViewRect.Width(), Args.SourceViewRect.Height() };
						bFoundSizeMismatch = true;
					}
					else
					{
						// If source view rect is passed, it will override the crop passed as argument.
						Args.MediaCapture->DesiredCaptureOptions.Crop = EMediaCaptureCroppingType::Custom;
						Args.MediaCapture->DesiredCaptureOptions.CustomCapturePoint = Args.SourceViewRect.Min;
					}
				}
				else if (Args.SourceViewRect.Area() == 0 && (Args.DesiredSize.X != Args.GetSizeX() || Args.DesiredSize.Y != Args.GetSizeY()))
				{
					RequestSize = { Args.DesiredSize };
					bFoundSizeMismatch = true;
				}
			}
			else
			{
				FIntPoint StartCapturePoint = FIntPoint::ZeroValue;
				if (Args.MediaCapture->DesiredCaptureOptions.Crop == EMediaCaptureCroppingType::Custom)
				{
					StartCapturePoint = Args.MediaCapture->DesiredCaptureOptions.CustomCapturePoint;
				}

				if ((Args.DesiredSize.X + StartCapturePoint.X) > Args.GetSizeX() || (Args.DesiredSize.Y + StartCapturePoint.Y) > Args.GetSizeY())
				{
					RequestSize = { Args.DesiredSize };
					bFoundSizeMismatch = true;
				}
			}

			if (bFoundSizeMismatch)
			{
				if (Args.MediaCapture->SupportsAutoRestart() 
					&& Args.MediaCapture->bUseRequestedTargetSize 
					&& Args.MediaCapture->DesiredCaptureOptions.bAutoRestartOnSourceSizeChange)
				{
					Args.MediaCapture->bIsAutoRestartRequired = true;

					UE_LOG(LogMediaIOCore, Log, TEXT("The capture will auto restart for '%s'. The Source size doesn't match with the user requested size. Requested: %d,%d  Source: %d,%d")
						, *Args.MediaCapture->MediaOutputName
						, RequestSize.X, RequestSize.Y
						, Args.GetSizeX(), Args.GetSizeY());
					
					return false;
				}
				else
				{
					UE_LOG(LogMediaIOCore, Error, TEXT("The capture will stop for '%s'. The Source size doesn't match with the user requested size. Requested: %d,%d  Source: %d,%d")
						, *Args.MediaCapture->MediaOutputName
						, RequestSize.X, RequestSize.Y
						, Args.GetSizeX(), Args.GetSizeY());

					return false;
				}
			}

			return true;
		}

		static void GetCopyInfo(const FCaptureFrameArgs& Args, FRHICopyTextureInfo& OutCopyInfo, FVector2D& OutSizeU, FVector2D& OutSizeV)
		{
			// Default to no crop
			OutSizeU = { 0.0f, 1.0f };
			OutSizeV = { 0.0f, 1.0f };
			OutCopyInfo.Size = FIntVector(Args.DesiredSize.X, Args.DesiredSize.Y, 1);
			if (Args.MediaCapture->DesiredCaptureOptions.Crop != EMediaCaptureCroppingType::None)
			{
				switch (Args.MediaCapture->DesiredCaptureOptions.Crop)
				{
				case EMediaCaptureCroppingType::Center:
					OutCopyInfo.SourcePosition = FIntVector((Args.GetSizeX() - Args.DesiredSize.X) / 2, (Args.GetSizeY() - Args.DesiredSize.Y) / 2, 0);
					break;
				case EMediaCaptureCroppingType::TopLeft:
					break;
				case EMediaCaptureCroppingType::Custom:
					OutCopyInfo.SourcePosition = FIntVector(Args.MediaCapture->DesiredCaptureOptions.CustomCapturePoint.X, Args.MediaCapture->DesiredCaptureOptions.CustomCapturePoint.Y, 0);
					break;
				default:
					break;
				}

				OutSizeU.X = (float)(OutCopyInfo.SourcePosition.X)                      / (float)Args.GetSizeX();
				OutSizeU.Y = (float)(OutCopyInfo.SourcePosition.X + OutCopyInfo.Size.X) / (float)Args.GetSizeX();
				OutSizeV.X = (float)(OutCopyInfo.SourcePosition.Y)                      / (float)Args.GetSizeY();
				OutSizeV.Y = (float)(OutCopyInfo.SourcePosition.Y + OutCopyInfo.Size.Y) / (float)Args.GetSizeY();
			}
		}

		static void AddConversionPass(const FCaptureFrameArgs& Args, const FConversionPassArgs& ConversionPassArgs, FRDGViewableResource* OutputResource)
		{
			if (OutputResource->Type == FRDGBuffer::StaticType)
			{
				return;
			}

			check(OutputResource->Type == FRDGTexture::StaticType);
			FRDGTexture* OutputTexture =  static_cast<FRDGTexture*>(OutputResource);
			
			//Based on conversion type, this might be changed
			bool bRequiresFormatConversion = ConversionPassArgs.bRequiresFormatConversion;

			// Rectangle area to use from source
			const FIntRect ViewRect(ConversionPassArgs.CopyInfo.GetSourceRect());

			//Dummy ViewFamily/ViewInfo created to use built in Draw Screen/Texture Pass
			FSceneViewFamily ViewFamily(FSceneViewFamily::ConstructionValues(nullptr, nullptr, FEngineShowFlags(ESFIM_Game))
				.SetTime(FGameTime())
				.SetGammaCorrection(1.0f));
			FSceneViewInitOptions ViewInitOptions;
			ViewInitOptions.ViewFamily = &ViewFamily;
			ViewInitOptions.SetViewRectangle(ViewRect);
			ViewInitOptions.ViewOrigin = FVector::ZeroVector;
			ViewInitOptions.ViewRotationMatrix = FMatrix::Identity;
			ViewInitOptions.ProjectionMatrix = FMatrix::Identity;
			FViewInfo ViewInfo = FViewInfo(ViewInitOptions);

			// If no conversion was required, go through a simple copy
			if (Args.MediaCapture->ConversionOperation == EMediaCaptureConversionOperation::NONE && !bRequiresFormatConversion)
			{
				AddCopyTexturePass(Args.GraphBuilder, ConversionPassArgs.SourceRGBTexture, OutputTexture, ConversionPassArgs.CopyInfo);
			}
			else
			{
				//At some point we should support color conversion (ocio) but for now we push incoming texture as is
				const bool bDoLinearToSRGB = Args.MediaCapture->DesiredCaptureOptions.bApplyLinearToSRGBConversion;

				FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
				TShaderMapRef<FScreenPassVS> VertexShader(GlobalShaderMap);

				switch (Args.MediaCapture->ConversionOperation)
				{
				case EMediaCaptureConversionOperation::RGBA8_TO_YUV_8BIT:
				{
					//Configure source/output viewport to get the right UV scaling from source texture to output texture
					FScreenPassTextureViewport InputViewport(ConversionPassArgs.SourceRGBTexture, ViewRect);
					FScreenPassTextureViewport OutputViewport(OutputTexture);
					TShaderMapRef<FRGB8toUYVY8ConvertPS> PixelShader(GlobalShaderMap);
					FRGB8toUYVY8ConvertPS::FParameters* Parameters = PixelShader->AllocateAndSetParameters(Args.GraphBuilder, ConversionPassArgs.SourceRGBTexture, MediaShaders::RgbToYuvRec709Scaled, MediaShaders::YUVOffset8bits, bDoLinearToSRGB, OutputTexture);
					AddDrawScreenPass(Args.GraphBuilder, RDG_EVENT_NAME("RGBToUYVY 8 bit"), ViewInfo, OutputViewport, InputViewport, VertexShader, PixelShader, Parameters);
				}
				break;
				case EMediaCaptureConversionOperation::RGB10_TO_YUVv210_10BIT:
				{
					//Configure source/output viewport to get the right UV scaling from source texture to output texture
					const FIntPoint InExtent = FIntPoint((((ConversionPassArgs.SourceRGBTexture->Desc.Extent.X + 47) / 48) * 48), ConversionPassArgs.SourceRGBTexture->Desc.Extent.Y);;
					FScreenPassTextureViewport InputViewport(ConversionPassArgs.SourceRGBTexture, ViewRect);
					FScreenPassTextureViewport OutputViewport(OutputTexture);
					TShaderMapRef<FRGB10toYUVv210ConvertPS> PixelShader(GlobalShaderMap);
					FRGB10toYUVv210ConvertPS::FParameters* Parameters = PixelShader->AllocateAndSetParameters(Args.GraphBuilder, ConversionPassArgs.SourceRGBTexture, MediaShaders::RgbToYuvRec709Scaled, MediaShaders::YUVOffset10bits, bDoLinearToSRGB, OutputTexture);
					AddDrawScreenPass(Args.GraphBuilder, RDG_EVENT_NAME("RGBToYUVv210"), ViewInfo, OutputViewport, InputViewport, VertexShader, PixelShader, Parameters);
				}
				break;
				case EMediaCaptureConversionOperation::INVERT_ALPHA:
					// fall through
				case EMediaCaptureConversionOperation::SET_ALPHA_ONE:
					// fall through
				case EMediaCaptureConversionOperation::NONE:
					bRequiresFormatConversion = true;
				default:
					if (bRequiresFormatConversion)
					{
						//Configure source/output viewport to get the right UV scaling from source texture to output texture
						FScreenPassTextureViewport InputViewport(ConversionPassArgs.SourceRGBTexture, ViewRect);
						FScreenPassTextureViewport OutputViewport(OutputTexture);

						// In cases where texture is converted from a format that doesn't have A channel, we want to force set it to 1.
						EMediaCaptureConversionOperation MediaConversionOperation = Args.MediaCapture->DesiredCaptureOptions.bForceAlphaToOneOnConversion ? EMediaCaptureConversionOperation::SET_ALPHA_ONE : Args.MediaCapture->ConversionOperation;
						FModifyAlphaSwizzleRgbaPS::FPermutationDomain PermutationVector;
						PermutationVector.Set<FModifyAlphaSwizzleRgbaPS::FConversionOp>(static_cast<int32>(MediaConversionOperation));

						TShaderMapRef<FModifyAlphaSwizzleRgbaPS> PixelShader(GlobalShaderMap, PermutationVector);
						FModifyAlphaSwizzleRgbaPS::FParameters* Parameters = PixelShader->AllocateAndSetParameters(Args.GraphBuilder, ConversionPassArgs.SourceRGBTexture, OutputTexture);
						AddDrawScreenPass(Args.GraphBuilder, RDG_EVENT_NAME("MediaCaptureSwizzle"), ViewInfo, OutputViewport, InputViewport, VertexShader, PixelShader, Parameters);
					}
					break;
				}
			}
		}

		template <typename CaptureType>
		static void OnReadbackComplete(FRHICommandList& RHICmdList, UMediaCapture* MediaCapture, TSharedPtr<CaptureType> ReadyFrame)
		{
			UE_LOG(LogMediaIOCore, Verbose, TEXT("[%s - %s] - Processing pending frame %d"), *MediaCapture->MediaOutputName, *FThreadManager::GetThreadName(FPlatformTLS::GetCurrentThreadId()), ReadyFrame->GetId());
			TRACE_CPUPROFILER_EVENT_SCOPE(UMediaCapture::OnReadbackComplete);

			{
				ON_SCOPE_EXIT
				{
					if (IsInRenderingThread())
					{
						--MediaCapture->WaitingForRenderCommandExecutionCounter;
					}
				};



				// Path where resource ready callback (readback / rhi capture) is on render thread (old method)
				if (IsInRenderingThread())
				{
					// Scoped gpu mask shouldn't be needed for readback since we specify the gpu mask used during copy when we lock
					// Keeping it for old render thread path
					FRHIGPUMask GPUMask;
#if WITH_MGPU
					GPUMask = RHICmdList.GetGPUMask();

					// If GPUMask is not set to a specific GPU we and since we are reading back the texture, it shouldn't matter which GPU we do this on.
					if (!GPUMask.HasSingleIndex())
					{
						GPUMask = FRHIGPUMask::FromIndex(GPUMask.GetFirstIndex());
					}
					SCOPED_GPU_MASK(RHICmdList, GPUMask);
#endif

					// Are we doing a GPU Direct transfer
					if (MediaCapture->ShouldCaptureRHIResource())
					{
						if (ReadyFrame->IsTextureResource())
						{
							{
								TRACE_CPUPROFILER_EVENT_SCOPE(UMediaCapture::UnlockDMATexture_RenderThread);
								MediaCapture->UnlockDMATexture_RenderThread(ReadyFrame->GetTextureResource());
							}

							TRACE_CPUPROFILER_EVENT_SCOPE(UMediaCapture::RHIResourceCaptured);
							TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(*FString::Printf(TEXT("MediaCapture Output Frame %d"), ReadyFrame->CaptureBaseData.SourceFrameNumberRenderThread));
							SCOPE_CYCLE_COUNTER(STAT_MediaCapture_RenderThread_RHI_CaptureCallback)

							MediaCapture->OnRHIResourceCaptured_RenderingThread(ReadyFrame->CaptureBaseData, ReadyFrame->UserData, ReadyFrame->GetTextureResource());
						}
						else
						{
							TRACE_CPUPROFILER_EVENT_SCOPE(UMediaCapture::RHIResourceCaptured);
							MediaCapture->OnRHIResourceCaptured_RenderingThread(ReadyFrame->CaptureBaseData, ReadyFrame->UserData, ReadyFrame->GetBufferResource());
						}
					}
					else
					{
						// Lock & read
						void* ColorDataBuffer = nullptr;
						int32 RowStride = 0;

						// Readback should be ready since we're after the sync point.
						SCOPE_CYCLE_COUNTER(STAT_MediaCapture_RenderThread_LockResource);
						ColorDataBuffer = ReadyFrame->Lock(FRHICommandListExecutor::GetImmediateCommandList(), RowStride);

						if (ensure(ColorDataBuffer))
						{
							{
								TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(*FString::Printf(TEXT("MediaCapture Output Frame %d"), ReadyFrame->GetId()));
								SCOPE_CYCLE_COUNTER(STAT_MediaCapture_AnyThread_LockResource)
								MediaCapture->OnFrameCaptured_RenderingThread(ReadyFrame->CaptureBaseData, ReadyFrame->UserData, ColorDataBuffer, MediaCapture->DesiredOutputSize.X, MediaCapture->DesiredOutputSize.Y, RowStride);
							}

							ReadyFrame->Unlock();
						}
					}
				}
				else
				{
					// Are we doing a GPU Direct transfer
					if (MediaCapture->ShouldCaptureRHIResource())
					{
						if (ReadyFrame->IsTextureResource())
						{
							{
								TRACE_CPUPROFILER_EVENT_SCOPE(UMediaCapture::UnlockDMATexture_RenderThread);
								MediaCapture->UnlockDMATexture_RenderThread(ReadyFrame->GetTextureResource());
							}

							TRACE_CPUPROFILER_EVENT_SCOPE(UMediaCapture::RHIResourceCaptured);
							TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(*FString::Printf(TEXT("MediaCapture Output Frame %d"), ReadyFrame->CaptureBaseData.SourceFrameNumberRenderThread));
							SCOPE_CYCLE_COUNTER(STAT_MediaCapture_RenderThread_RHI_CaptureCallback)

							MediaCapture->OnRHIResourceCaptured_AnyThread(ReadyFrame->CaptureBaseData, ReadyFrame->UserData, ReadyFrame->GetTextureResource());
						}
						else
						{
							TRACE_CPUPROFILER_EVENT_SCOPE(UMediaCapture::RHIResourceCaptured);
							MediaCapture->OnRHIResourceCaptured_AnyThread(ReadyFrame->CaptureBaseData, ReadyFrame->UserData, ReadyFrame->GetBufferResource());
						}
					}
					else
					{
						// Lock & read
						void* ColorDataBuffer = nullptr;
						int32 RowStride = 0;

						// Readback should be ready since we're after the sync point.
						SCOPE_CYCLE_COUNTER(STAT_MediaCapture_RenderThread_LockResource);
						ColorDataBuffer = ReadyFrame->Lock_Unsafe(RowStride);

						if (ensure(ColorDataBuffer))
						{
							{
								TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(*FString::Printf(TEXT("MediaCapture Output Frame %d"), ReadyFrame->GetId()));
								SCOPE_CYCLE_COUNTER(STAT_MediaCapture_AnyThread_LockResource)

								UMediaCapture::FMediaCaptureResourceData ResourceData;
								ResourceData.Buffer = ColorDataBuffer;
								ResourceData.Width = MediaCapture->DesiredOutputSize.X;
								ResourceData.Height = MediaCapture->DesiredOutputSize.Y;
								ResourceData.BytesPerRow = RowStride;
								MediaCapture->OnFrameCaptured_AnyThread(ReadyFrame->CaptureBaseData, ReadyFrame->UserData, MoveTemp(ResourceData));
							}

							ReadyFrame->Unlock_Unsafe();
						}
					}
				}
				
				ReadyFrame->bDoingGPUCopy = false;
				ReadyFrame->bReadbackRequested = false;

				UE_LOG(LogMediaIOCore, Verbose, TEXT("[%s - %s] - Completed pending frame %d."), *MediaCapture->MediaOutputName, *FThreadManager::GetThreadName(FPlatformTLS::GetCurrentThreadId()), ReadyFrame->GetId());
				MediaCapture->FrameManager->CompleteNextPending(*ReadyFrame);
				--MediaCapture->PendingFrameCount;
			};
		}


		template <typename CaptureType>
		static void AddSyncPointPass(FRDGBuilder& GraphBuilder, UMediaCapture* MediaCapture, TSharedPtr<FCaptureFrame> CapturingFrame, FRDGViewableResource* OutputResource)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(MediaCaptureSyncPoint);
			TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(*FString::Printf(TEXT("MediaCaptureSyncPoint_%d"), CapturingFrame->CaptureBaseData.SourceFrameNumberRenderThread));
			RDG_GPU_STAT_SCOPE(GraphBuilder, MediaCapture_SyncPointPass);

			// Initialize sync handlers only the first time. 
			if (MediaCapture->bSyncHandlersInitialized == false)
			{
				MediaCapture->InitializeSyncHandlers_RenderThread();
			}

			// Add buffer output as a parameter to depend on the compute shader pass
			typename CaptureType::PassParametersType* PassParameters = GraphBuilder.AllocParameters<typename CaptureType::PassParametersType>();
			PassParameters->Resource = static_cast<typename CaptureType::FOutputResourceType>(OutputResource);

			GraphBuilder.AddPass(
				RDG_EVENT_NAME("MediaCaptureCopySyncPass"),
				PassParameters,
				ERDGPassFlags::Copy | ERDGPassFlags::NeverCull,
				[MediaCapture, CapturingFrame](FRHICommandListImmediate& RHICmdList)
				{
					if (CapturingFrame && CapturingFrame->bMediaCaptureActive)
					{
						// Get available sync handler to create a sync point
						TSharedPtr<UMediaCapture::FMediaCaptureSyncData> SyncDataPtr = MediaCapture->GetAvailableSyncHandler();
						if (ensure(SyncDataPtr))
						{
							// This will happen after the conversion pass has completed
							RHICmdList.WriteGPUFence(SyncDataPtr->RHIFence);
							SyncDataPtr->bIsBusy = true;

							// Here we request a number to process the frames in order.
							const uint32 ExecutionNumber = MediaCapture->OrderedAsyncGateCaptureReady.GetANumber();

							// Spawn a task that will wait (poll) and continue the process of providing a new texture
							UE::Tasks::FTask Task = UE::Tasks::Launch(UE_SOURCE_LOCATION, [MediaCapture, CapturingFrame, SyncDataPtr, ExecutionNumber]()
								{
									TRACE_CPUPROFILER_EVENT_SCOPE(UMediaCapture::SyncPass);

									ON_SCOPE_EXIT
									{
										// Make sure that we give up our turn to execute when exiting this task,
										// which will allow the other async tasks to execute after waiting for the fence.
										MediaCapture->OrderedAsyncGateCaptureReady.GiveUpTurn(ExecutionNumber);
									};

									double WaitTime = 0.0;
									bool bWaitedForCompletion = false;
									{
										FScopedDurationTimer Timer(WaitTime);
									
										// Wait until fence has been written (shader has completed)
										while (true)
										{
											if (SyncDataPtr->RHIFence->Poll())
											{
												bWaitedForCompletion = true;
												break;
											}

											if (!CapturingFrame->bMediaCaptureActive)
											{
												bWaitedForCompletion = false;
												break;
											}

											constexpr float SleepTimeSeconds = 50 * 1E-6;
											FPlatformProcess::SleepNoStats(SleepTimeSeconds);
										}

										SyncDataPtr->RHIFence->Clear();
										SyncDataPtr->bIsBusy = false;
									}

									if (CapturingFrame->bMediaCaptureActive && bWaitedForCompletion && MediaCapture)
									{
										// Ensure that we do not run the following code out of order with respect to the other sibling async tasks,
										// because the Pending Frames are expected to be processed in order.
										if (!MediaCapture->OrderedAsyncGateCaptureReady.IsMyTurn(ExecutionNumber))
										{
											TRACE_CPUPROFILER_EVENT_SCOPE(UMediaCapture::SyncPass::OutOfOrderWait);

											UE_LOG(LogMediaIOCore, Warning, TEXT(
												"The wait for the GPU fence for the next frame of MediaCapture '%s' unexpectedly happened out of order. "
												"Order will be enforced by waiting until the previous frames are processed. "),
												*MediaCapture->MediaOutputName);

											MediaCapture->OrderedAsyncGateCaptureReady.WaitForTurn(ExecutionNumber);
										}

										if (MediaCapture->UseAnyThreadCapture())
										{
											OnReadbackComplete(FRHICommandListExecutor::GetImmediateCommandList(), MediaCapture, CapturingFrame);
										}
										else
										{
											++MediaCapture->WaitingForRenderCommandExecutionCounter;

											ENQUEUE_RENDER_COMMAND(MediaOutputCaptureReadbackComplete)([MediaCapture, CapturingFrame](FRHICommandList& RHICommandList)
											{
												OnReadbackComplete(RHICommandList, MediaCapture, CapturingFrame);
											});
										}
									}

								}, static_cast<LowLevelTasks::ETaskPriority>(CVarMediaIOCapturePollTaskPriority.GetValueOnRenderThread()));

							{
								// Add the task to the capture's list of pending tasks.
								FScopeLock ScopedLock(&MediaCapture->PendingReadbackTasksCriticalSection);
								MediaCapture->PendingReadbackTasks.Add(MoveTemp(Task));
							}
						}
						else
						{
							UE_LOG(LogMediaIOCore, Error, TEXT(
								"GetAvailableSyncHandler of MediaCapture '%s' failed to provide a fence, the captured buffers may not become available anymore."),
								*MediaCapture->MediaOutputName);
						}
					}
				});
		}

		static bool CaptureFrame(const FCaptureFrameArgs& Args, const TSharedPtr<UE::MediaCaptureData::FCaptureFrame> CapturingFrame)
		{
			RDG_GPU_STAT_SCOPE(Args.GraphBuilder, MediaCapture_CaptureFrame)

			// Validate if we have a resources used to capture source texture
			if (!CapturingFrame->HasValidResource())
			{
				UE_LOG(LogMediaIOCore, Error, TEXT("The capture will stop for '%s'. A capture frame had an invalid render resource."), *Args.MediaCapture->MediaOutputName);
				return false;
			}

			if (!Args.HasValidResource())
			{
				UE_LOG(LogMediaIOCore, Error, TEXT("Can't grab the Texture to capture for '%s'."), *Args.MediaCapture->MediaOutputName);
				return false;
			}

			// If true, we will need to go through our different shader to convert from source format to out format (i.e RGB to YUV)
			const bool bRequiresFormatConversion = Args.MediaCapture->DesiredPixelFormat != Args.GetFormat();

			// Validate pixel formats and sizes before pursuing
			if (!AreInputsValid(Args))
			{
				return false;
			}

			{
				TRACE_CPUPROFILER_EVENT_SCOPE(UMediaCapture::LockDMATexture_RenderThread);
				TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(*FString::Printf(TEXT("LockDmaTexture Output Frame %d"), CapturingFrame->CaptureBaseData.SourceFrameNumberRenderThread));

				if (CapturingFrame->IsTextureResource())
				{
					Args.MediaCapture->LockDMATexture_RenderThread(CapturingFrame->GetTextureResource());
				}
			}
			
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(UMediaCapture::FrameCapture);
				SCOPE_CYCLE_COUNTER(STAT_MediaCapture_RenderThread_FrameCapture);

				FRHICopyTextureInfo CopyInfo;
				FVector2D SizeU;
				FVector2D SizeV;
				GetCopyInfo(Args, CopyInfo, SizeU, SizeV);

				FRDGTextureRef SourceRGBTexture = Args.RDGResourceToCapture;
				
				if (!SourceRGBTexture)
				{
					// If we weren't passed a rdg texture, register the external rhi texture.
					SourceRGBTexture = Args.GraphBuilder.RegisterExternalTexture(CreateRenderTarget(Args.ResourceToCapture, TEXT("SourceTexture")));
				}

				// Register output resource used by the current capture method (texture or buffer)
				FRDGViewableResource* OutputResource = CapturingFrame->RegisterResource(Args.GraphBuilder);

				{
					TRACE_CPUPROFILER_EVENT_SCOPE(UMediaCapture::GraphSetup);
					SCOPED_DRAW_EVENTF(Args.GraphBuilder.RHICmdList, MediaCapture, TEXT("MediaCapture"));

					// If custom conversion was requested from implementation, give it useful information to apply 
					if (Args.MediaCapture->ConversionOperation == EMediaCaptureConversionOperation::CUSTOM)
					{
						RDG_GPU_STAT_SCOPE(Args.GraphBuilder, MediaCapture_CustomCapture)
						TRACE_CPUPROFILER_EVENT_SCOPE(UMediaCapture::CustomCapture);
						
						if (CapturingFrame->IsTextureResource())
						{
							Args.MediaCapture->OnCustomCapture_RenderingThread(Args.GraphBuilder, CapturingFrame->CaptureBaseData, CapturingFrame->UserData
								, SourceRGBTexture, static_cast<FRDGTextureRef>(OutputResource), CopyInfo, SizeU, SizeV);
						}
						else
						{
							Args.MediaCapture->OnCustomCapture_RenderingThread(Args.GraphBuilder, CapturingFrame->CaptureBaseData, CapturingFrame->UserData
								, SourceRGBTexture, static_cast<FRDGBufferRef>(OutputResource), CopyInfo, SizeU, SizeV);
						}
					}
					else
					{
						RDG_GPU_STAT_SCOPE(Args.GraphBuilder, MediaCapture_Conversion)
						TRACE_CPUPROFILER_EVENT_SCOPE(UMediaCapture::FormatConversion);
						AddConversionPass(Args, { SourceRGBTexture, bRequiresFormatConversion,  CopyInfo, SizeU, SizeV }, OutputResource);
					}
				}

				
				if (Args.MediaCapture->bShouldCaptureRHIResource == false)
				{
					RDG_GPU_STAT_SCOPE(Args.GraphBuilder, MediaCapture_Readback)
					TRACE_CPUPROFILER_EVENT_SCOPE(UMediaCapture::EnqueueReadback);

					CapturingFrame->EnqueueCopy(Args.GraphBuilder, OutputResource, Args.MediaCapture->UseAnyThreadCapture());
					CapturingFrame->bReadbackRequested = true;
				}
				else
				{
					CapturingFrame->bDoingGPUCopy = true;
				}

				Args.MediaCapture->FrameManager->MarkPending(*CapturingFrame);
				++Args.MediaCapture->PendingFrameCount;

				if (Args.MediaCapture->UseExperimentalScheduling())
				{
					if (CapturingFrame->IsTextureResource())
					{
						AddSyncPointPass<FTextureCaptureFrame>(Args.GraphBuilder, Args.MediaCapture, CapturingFrame, OutputResource);
					}
					else
					{
						AddSyncPointPass<FBufferCaptureFrame>(Args.GraphBuilder, Args.MediaCapture, CapturingFrame, OutputResource);
					}
				}
			}

			return true;
		}
	};
}

/* FMediaCaptureOptions
*****************************************************************************/
FMediaCaptureOptions::FMediaCaptureOptions()
	: Crop(EMediaCaptureCroppingType::None)
	, CustomCapturePoint(FIntPoint::ZeroValue)
	, bResizeSourceBuffer(false)
	, bSkipFrameWhenRunningExpensiveTasks(true)
	, bConvertToDesiredPixelFormat(true)
	, bForceAlphaToOneOnConversion(false)
	, bAutostopOnCapture(false)
	, NumberOfFramesToCapture(-1)
{

}


/* UMediaCapture
*****************************************************************************/

UMediaCapture::UMediaCapture()
	: ValidSourceGPUMask(FRHIGPUMask::All())
	, bOutputResourcesInitialized(false)
	, bShouldCaptureRHIResource(false)
	, WaitingForRenderCommandExecutionCounter(0)
	, PendingFrameCount(0)
	, bIsAutoRestartRequired(false)
{
}

UMediaCapture::~UMediaCapture() = default;

const TSet<EPixelFormat>& UMediaCapture::GetSupportedRgbaSwizzleFormats()
{
	static TSet<EPixelFormat> SupportedFormats =
		{
			PF_A32B32G32R32F,
			PF_B8G8R8A8,
			PF_G8,
			PF_G16,
			PF_FloatRGB,
			PF_FloatRGBA,
			PF_R32_FLOAT,
			PF_G16R16,
			PF_G16R16F,
			PF_G32R32F,
			PF_A2B10G10R10,
			PF_A16B16G16R16,
			PF_R16F,
			PF_FloatR11G11B10,
			PF_A8,
			PF_R32_UINT,
			PF_R32_SINT,
			PF_R16_UINT,
			PF_R16_SINT,
			PF_R16G16B16A16_UINT,
			PF_R16G16B16A16_SINT,
			PF_R5G6B5_UNORM,
			PF_R8G8B8A8,
			PF_A8R8G8B8,
			PF_R8G8,
			PF_R32G32B32A32_UINT,
			PF_R16G16_UINT,
			PF_R8_UINT,
			PF_R8G8B8A8_UINT,
			PF_R8G8B8A8_SNORM,
			PF_R16G16B16A16_UNORM,
			PF_R16G16B16A16_SNORM,
			PF_R32G32_UINT,
			PF_R8,
		};
	
	return SupportedFormats;
}

void UMediaCapture::BeginDestroy()
{
	if (GetState() == EMediaCaptureState::Capturing || GetState() == EMediaCaptureState::Preparing)
	{
		UE_LOG(LogMediaIOCore, Warning, TEXT("%s will be destroyed and the capture was not stopped."), *GetName());
	}
	StopCapture(false);

	Super::BeginDestroy();
}

FString UMediaCapture::GetDesc()
{
	if (MediaOutput)
	{
		return FString::Printf(TEXT("%s [%s]"), *Super::GetDesc(), *MediaOutput->GetDesc());
	}
	return FString::Printf(TEXT("%s [none]"), *Super::GetDesc());
}

bool UMediaCapture::CaptureActiveSceneViewport(FMediaCaptureOptions CaptureOptions)
{
	StopCapture(false);

	check(IsInGameThread());

	TSharedPtr<FSceneViewport> FoundSceneViewport;
	if (!MediaCaptureDetails::FindSceneViewportAndLevel(FoundSceneViewport) || !FoundSceneViewport.IsValid())
	{
		UE_LOG(LogMediaIOCore, Warning, TEXT("Can not start the capture. No viewport could be found."));
		return false;
	}

	return CaptureSceneViewport(FoundSceneViewport, CaptureOptions);
}

bool UMediaCapture::CaptureSceneViewport(TSharedPtr<FSceneViewport>& SceneViewport, FMediaCaptureOptions CaptureOptions)
{
	using namespace UE::MediaCapture::Private;
	return StartSourceCapture(MakeShared<FSceneViewportCaptureSource>(this, CaptureOptions, SceneViewport));
}

bool UMediaCapture::CaptureRHITexture(const FRHICaptureResourceDescription& ResourceDescription, FMediaCaptureOptions CaptureOptions)
{
	using namespace UE::MediaCapture::Private;
	return StartSourceCapture(MakeShared<FRHIResourceCaptureSource>(this, CaptureOptions, ResourceDescription));
}

bool UMediaCapture::CaptureTextureRenderTarget2D(UTextureRenderTarget2D* RenderTarget, FMediaCaptureOptions CaptureOptions)
{
	using namespace UE::MediaCapture::Private;
	return StartSourceCapture(MakeShared<FRenderTargetCaptureSource>(this, CaptureOptions, RenderTarget));
}

bool UMediaCapture::StartSourceCapture(TSharedPtr<UE::MediaCapture::Private::FCaptureSource> InSource)
{
	StopCapture(false);
	
	if (!InSource)
	{
		ensure(false);
		return false;
	}
	
	check(IsInGameThread());

	DesiredCaptureOptions = InSource->CaptureOptions;

	if (!ValidateMediaOutput())
	{
		MediaCaptureDetails::ShowSlateNotification();
		return false;
	}

	CacheMediaOutput(InSource->GetSourceType());

	if (bUseRequestedTargetSize)
	{
		DesiredSize = InSource->GetSize();
	}
	else if (DesiredCaptureOptions.bResizeSourceBuffer)
	{
		InSource->ResizeSourceBuffer(DesiredSize);
	}

	CacheOutputOptions();

	constexpr bool bCurrentlyCapturing = false;
	if (!InSource->ValidateSource(DesiredSize, DesiredPixelFormat, bCurrentlyCapturing))
	{
		MediaCaptureDetails::ShowSlateNotification();
		return false;
	}

	SetState(EMediaCaptureState::Preparing);

	bool bInitialized = InitializeCapture();
	if (bInitialized)
	{
		bInitialized = InSource->PostInitialize();
	}

	// This could have been updated by the initialization done by the implementation
	bShouldCaptureRHIResource = ShouldCaptureRHIResource();

	if (bInitialized)
	{
		InitializeOutputResources(MediaOutput->NumberOfTextureBuffers);
		bInitialized = GetState() != EMediaCaptureState::Stopped;
	}

	if (bInitialized)
	{
		//no lock required the command on the render thread is not active yet
		CaptureSource = MoveTemp(InSource);
		
		if (DesiredCaptureOptions.CapturePhase != EMediaCapturePhase::EndFrame)
		{
			ViewExtension = FSceneViewExtensions::NewExtension<FMediaCaptureSceneViewExtension>(this, DesiredCaptureOptions.CapturePhase);
		}

		if (ViewExtension)
		{
			ensure(CaptureSource->GetCaptureType() == UE::MediaCapture::Private::ECaptureType::Immediate);
		}

		if (CaptureSource->GetCaptureType() == UE::MediaCapture::Private::ECaptureType::Immediate)
		{
			// Immediate capture requires us to prepare frame info in OnBeginFrame
			FCoreDelegates::OnBeginFrame.AddUObject(this, &UMediaCapture::OnBeginFrame_GameThread);
		}
		else
		{
			FCoreDelegates::OnEndFrame.AddUObject(this, &UMediaCapture::OnEndFrame_GameThread);
		}
	}
	else
	{
		SetState(EMediaCaptureState::Stopped);
		MediaCaptureDetails::ShowSlateNotification();
	}

#if WITH_EDITOR
	MediaCaptureAnalytics::SendCaptureEvent(GetCaptureSourceType());
#endif


	if (!GRHISupportsMultithreading)
	{
		UE_LOG(LogMediaIOCore, Display, TEXT("Experimental scheduling and AnyThread Capture was disabled because the current RHI does not support Multithreading."));
	}
	else if (!SupportsAnyThreadCapture())
	{
		UE_LOG(LogMediaIOCore, Display, TEXT("AnyThread Capture was disabled because the media capture implementation does not have a AnyThread callback."));
	}

	return bInitialized;
}

void UMediaCapture::CacheMediaOutput(EMediaCaptureSourceType InSourceType)
{
	check(MediaOutput);
	CaptureSourceType = InSourceType;
	DesiredSize = MediaOutput->GetRequestedSize();
	bUseRequestedTargetSize = DesiredSize == UMediaOutput::RequestCaptureSourceSize;
	DesiredPixelFormat = MediaOutput->GetRequestedPixelFormat();
	ConversionOperation = MediaOutput->GetConversionOperation(InSourceType);
}

void UMediaCapture::CacheOutputOptions()
{
	DesiredOutputSize = GetOutputSize(DesiredSize, ConversionOperation);
	DesiredOutputResourceType = GetOutputResourceType(ConversionOperation);
	DesiredOutputPixelFormat = GetOutputPixelFormat(DesiredPixelFormat, ConversionOperation);
	DesiredOutputBufferDescription = GetOutputBufferDescription(ConversionOperation);
	MediaOutputName = *MediaOutput->GetName();
	bShouldCaptureRHIResource = ShouldCaptureRHIResource();
}

FIntPoint UMediaCapture::GetOutputSize(const FIntPoint & InSize, const EMediaCaptureConversionOperation & InConversionOperation) const
{
	switch (InConversionOperation)
	{
	case EMediaCaptureConversionOperation::RGBA8_TO_YUV_8BIT:
		return FIntPoint(InSize.X / 2, InSize.Y);
	case EMediaCaptureConversionOperation::RGB10_TO_YUVv210_10BIT:
		// Padding aligned on 48 (16 and 6 at the same time)
		return FIntPoint((((InSize.X + 47) / 48) * 48) / 6, InSize.Y);
	case EMediaCaptureConversionOperation::CUSTOM:
		return GetCustomOutputSize(InSize);
	case EMediaCaptureConversionOperation::NONE:
	default:
		return InSize;
	}
}

EPixelFormat UMediaCapture::GetOutputPixelFormat(const EPixelFormat & InPixelFormat, const EMediaCaptureConversionOperation & InConversionOperation) const
{
	switch (InConversionOperation)
	{
	case EMediaCaptureConversionOperation::RGBA8_TO_YUV_8BIT:
		return EPixelFormat::PF_B8G8R8A8;
	case EMediaCaptureConversionOperation::RGB10_TO_YUVv210_10BIT:
		return EPixelFormat::PF_R32G32B32A32_UINT;
	case EMediaCaptureConversionOperation::CUSTOM:
		return GetCustomOutputPixelFormat(InPixelFormat);
	case EMediaCaptureConversionOperation::NONE:
	default:
		return InPixelFormat;
	}
}

EMediaCaptureResourceType UMediaCapture::GetOutputResourceType(const EMediaCaptureConversionOperation& InConversionOperation) const
{
	switch (InConversionOperation)
	{
	case EMediaCaptureConversionOperation::CUSTOM:
		return GetCustomOutputResourceType();
	default:
		return EMediaCaptureResourceType::Texture;
	}
}

FRDGBufferDesc UMediaCapture::GetOutputBufferDescription(EMediaCaptureConversionOperation InConversionOperation) const
{
	switch (InConversionOperation)
	{
	case EMediaCaptureConversionOperation::CUSTOM:
		return GetCustomBufferDescription(DesiredSize);
	default:
		return FRDGBufferDesc();
	}
}

bool UMediaCapture::UpdateSource(TSharedPtr<UE::MediaCapture::Private::FCaptureSource> InCaptureSource)
{
	if (!MediaCaptureDetails::ValidateIsCapturing(this))
	{
		StopCapture(false);
		return false;
	}

	check(IsInGameThread());

	const TSharedPtr<UE::MediaCapture::Private::FCaptureSource> PreviousCaptureSource = CaptureSource;
	CaptureSource = MoveTemp(InCaptureSource);

	if (!bUseRequestedTargetSize && DesiredCaptureOptions.bResizeSourceBuffer)
	{
		CaptureSource->ResizeSourceBuffer(DesiredSize);
	}

	const bool bCurrentlyCapturing = true;
	if (!CaptureSource->ValidateSource(DesiredSize, DesiredPixelFormat, bCurrentlyCapturing))
	{
		constexpr bool bFlushRenderingCommands = false;
		CaptureSource->ResetSourceBufferSize(bFlushRenderingCommands);
		StopCapture(false);
		MediaCaptureDetails::ShowSlateNotification();
		return false;
	}

	if (!CaptureSource->UpdateSourceImpl())
	{
		constexpr bool bFlushRenderingCommands = false;
		CaptureSource->ResetSourceBufferSize(bFlushRenderingCommands);
		StopCapture(false);
		MediaCaptureDetails::ShowSlateNotification();
		return false;
	}

	{
		FScopeLock Lock(&AccessingCapturingSource);

		WaitForPendingTasks();
		
		constexpr bool bFlushRenderingCommands = false;
		PreviousCaptureSource->ResetSourceBufferSize(bFlushRenderingCommands);
	}

	return true;
}

bool UMediaCapture::UseExperimentalScheduling() const
{
	return GRHISupportsMultithreading && CVarMediaIOEnableExperimentalScheduling.GetValueOnAnyThread() == 1;
}

bool UMediaCapture::UseAnyThreadCapture() const
{
	return GRHISupportsMultithreading
		&& CVarMediaIOEnableExperimentalScheduling.GetValueOnAnyThread() 
		&& CVarMediaIOScheduleOnAnyThread.GetValueOnAnyThread()
		&& SupportsAnyThreadCapture();
}

bool UMediaCapture::UpdateSceneViewport(TSharedPtr<FSceneViewport>& InSceneViewport)
{
	check(CaptureSource);
	return UpdateSource(MakeShared<UE::MediaCapture::Private::FSceneViewportCaptureSource>(this, CaptureSource->CaptureOptions, InSceneViewport));
}

bool UMediaCapture::UpdateTextureRenderTarget2D(UTextureRenderTarget2D* InRenderTarget2D)
{
	check(CaptureSource);
	return UpdateSource(MakeShared<UE::MediaCapture::Private::FRenderTargetCaptureSource>(this, CaptureSource->CaptureOptions, InRenderTarget2D));
}

void UMediaCapture::StopCapture(bool bAllowPendingFrameToBeProcess)
{
	check(IsInGameThread());

	if (GetState() != EMediaCaptureState::StopRequested && GetState() != EMediaCaptureState::Capturing)
	{
		bAllowPendingFrameToBeProcess = false;
	}

	if (bAllowPendingFrameToBeProcess)
	{
		if (GetState() != EMediaCaptureState::Stopped && GetState() != EMediaCaptureState::StopRequested)
		{
			SetState(EMediaCaptureState::StopRequested);

			//Do not flush when auto stopping to avoid hitches.
			if(DesiredCaptureOptions.bAutostopOnCapture != true)
			{
				WaitForPendingTasks();
			}
		}
	}
	else
	{
		if (FrameManager)
		{
			FrameManager->ForEachFrame([](const TSharedPtr<UE::MediaCaptureData::FFrame> InFrame)
			{
				StaticCastSharedPtr<UE::MediaCaptureData::FCaptureFrame>(InFrame)->bMediaCaptureActive = false;
			});
		}

		if (GetState() != EMediaCaptureState::Stopped)
		{
			SetState(EMediaCaptureState::Stopped);

			FCoreDelegates::OnBeginFrame.RemoveAll(this);
			FCoreDelegates::OnEndFrame.RemoveAll(this);

			while (WaitingForRenderCommandExecutionCounter.load() > 0 || !bOutputResourcesInitialized)
			{
				FlushRenderingCommands();
			}
			
			StopCaptureImpl(bAllowPendingFrameToBeProcess);

			if (CaptureSource)
			{
				constexpr bool bFlushRenderingCommands = false;
				CaptureSource->ResetSourceBufferSize(bFlushRenderingCommands);
				CaptureSource.Reset();
			}
			
			DesiredSize = FIntPoint(1280, 720);
			DesiredPixelFormat = EPixelFormat::PF_A2B10G10R10;
			DesiredOutputSize = FIntPoint(1280, 720);
			DesiredOutputPixelFormat = EPixelFormat::PF_A2B10G10R10;
			DesiredCaptureOptions = FMediaCaptureOptions();
			ConversionOperation = EMediaCaptureConversionOperation::NONE;
			ViewExtension.Reset();
			
			MediaOutputName.Reset();
		}
	}
}

void UMediaCapture::SetMediaOutput(UMediaOutput* InMediaOutput)
{
	if (GetState() == EMediaCaptureState::Stopped)
	{
		MediaOutput = InMediaOutput;
	}
}

bool UMediaCapture::SetCaptureAudioDevice(const FAudioDeviceHandle& InAudioDeviceHandle)
{
	bool bSuccess = true;
	if (CaptureSource)
	{
		bSuccess = UpdateAudioDeviceImpl(InAudioDeviceHandle);
	}
	if (bSuccess)
	{
		AudioDeviceHandle = InAudioDeviceHandle;
	}
	return bSuccess;
}

void UMediaCapture::CaptureImmediate_RenderThread(FRDGBuilder& GraphBuilder, FRHITexture* InSourceTexture, FIntRect SourceViewRect)
{
	UE::MediaCaptureData::FCaptureFrameArgs CaptureArgs{GraphBuilder};
	CaptureArgs.MediaCapture = this;
    CaptureArgs.DesiredSize = DesiredSize;
    CaptureArgs.ResourceToCapture = InSourceTexture;
	CaptureArgs.SourceViewRect = SourceViewRect;
	
	CaptureImmediate_RenderThread(CaptureArgs);
}


void UMediaCapture::CaptureImmediate_RenderThread(FRDGBuilder& GraphBuilder,  FRDGTextureRef InSourceTextureRef, FIntRect SourceViewRect)
{
	UE::MediaCaptureData::FCaptureFrameArgs CaptureArgs{GraphBuilder};
	CaptureArgs.MediaCapture = this;
	CaptureArgs.DesiredSize = DesiredSize;
	CaptureArgs.RDGResourceToCapture = InSourceTextureRef;
	CaptureArgs.SourceViewRect = SourceViewRect;
	
	CaptureImmediate_RenderThread(CaptureArgs);
}

void UMediaCapture::CaptureImmediate_RenderThread(const UE::MediaCaptureData::FCaptureFrameArgs& Args)
{
	using namespace UE::MediaCaptureData;
	TRACE_CPUPROFILER_EVENT_SCOPE(UMediaCapture::CaptureImmediate_RenderThread);
	
	check(IsInRenderingThread());

	if (bIsAutoRestartRequired)
	{
		return;
	}

	// This could happen if the capture immediate is called before our resources were initialized. We can't really flush as we are in a command.
	if (!bOutputResourcesInitialized)
	{
		UE_LOG(LogMediaIOCore, VeryVerbose, TEXT("Could not capture frame. Output resources haven't been initialized"));
		return;
	}

	if (!MediaOutput)
	{
		return;
	}

	if (GetState() == EMediaCaptureState::Error)
	{
		return;
	}

	if (CaptureSource && CaptureSource->GetCaptureType() != UE::MediaCapture::Private::ECaptureType::Immediate)
	{
		UE_LOG(LogMediaIOCore, Error, TEXT("[%s] - Trying to capture a RHI resource with another capture type."), *MediaOutputName);
		SetState(EMediaCaptureState::Error);
	}

	// Keep resource size up to date with incoming resource to capture
 	StaticCastSharedPtr<UE::MediaCapture::Private::FRHIResourceCaptureSource>(CaptureSource)->ResourceDescription.ResourceSize = Args.GetSizeXY();

	if (GetState() != EMediaCaptureState::Capturing && GetState() != EMediaCaptureState::StopRequested)
	{
		return;
	}

	if (DesiredCaptureOptions.bSkipFrameWhenRunningExpensiveTasks && !FSlateThrottleManager::Get().IsAllowingExpensiveTasks())
	{
		return;
	}

	// Get cached capture data from game thread. We want to find a cached frame matching current render thread frame number
	bool bFoundMatchingData = false;
	FQueuedCaptureData NextCaptureData;

	{
		FScopeLock ScopeLock(&CaptureDataQueueCriticalSection);
		for (auto It = CaptureDataQueue.CreateIterator(); It; ++It)
		{
			if (It->BaseData.SourceFrameNumberRenderThread == GFrameCounterRenderThread)
			{
				NextCaptureData = *It;
				bFoundMatchingData = true;
				It.RemoveCurrent();
				break;
			}
			else if (GFrameCounterRenderThread > It->BaseData.SourceFrameNumberRenderThread &&
					GFrameCounterRenderThread - It->BaseData.SourceFrameNumberRenderThread > MaxCaptureDataAgeInFrames)
			{
				// Remove old frame data that wasn't used.
				It.RemoveCurrent();
			}
		}
	}
	if (bFoundMatchingData == false)
	{
		UE_LOG(LogMediaIOCore, Warning, TEXT("Can't capture frame. Could not find the matching game frame %d."), GFrameCounterRenderThread);
		return;
	}

	if (GetState() == EMediaCaptureState::StopRequested && PendingFrameCount.load() <= 0)
	{
		// All the requested frames have been captured.
		StopCapture(false);
		return;
	}
		
	// Get next available frame from the store. Can be invalid.
	TSharedPtr<FCaptureFrame> CapturingFrame;
	if (GetState() != EMediaCaptureState::StopRequested)
	{
		CapturingFrame = FrameManager->GetNextAvailable<FCaptureFrame>();
	}

	if (!CapturingFrame && GetState() != EMediaCaptureState::StopRequested)
	{
		if (DesiredCaptureOptions.OverrunAction == EMediaCaptureOverrunAction::Flush)
		{
			WaitForSingleExperimentalSchedulingTaskToComplete();

			CapturingFrame = FrameManager->GetNextAvailable<FCaptureFrame>();

			if (!CapturingFrame)
			{
				UE_LOG(LogMediaIOCore, Error, TEXT("[%s] - No frames available for capture for frame %llu. This should not happen."), 
					*MediaOutputName, GFrameCounterRenderThread);

				SetState(EMediaCaptureState::Error);
				return;
			}
		}
		else
		{
			//In case we are skipping frames, just keep capture frame as invalid
			UE_LOG(LogMediaIOCore, Warning, TEXT("[%s] - No frames available for capture of frame %llu. Skipping"), 
				*MediaOutputName, GFrameCounterRenderThread);
		}
	}

	PrintFrameState();

	if (!CapturingFrame || CapturingFrame->FrameId == -1)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(UMediaCapture::Capture_Invalid);
	}
	
	UE_LOG(LogMediaIOCore, Verbose, TEXT("[%s - %s] - Capturing frame %d"), *MediaOutputName, *FThreadManager::GetThreadName(FPlatformTLS::GetCurrentThreadId()), CapturingFrame ? CapturingFrame->FrameId : -1);
	
	if (CapturingFrame)
	{
		// Prepare frame to capture

		// Use queued capture base data for this frame to capture
		CapturingFrame->CaptureBaseData = MoveTemp(NextCaptureData.BaseData);
		CapturingFrame->CaptureBaseData.SourceFrameNumber = ++CaptureRequestCount;
		CapturingFrame->UserData = MoveTemp(NextCaptureData.UserData);

		ProcessCapture_RenderThread(CapturingFrame, Args);
	}

	// If CVarMediaIOEnableExperimentalScheduling is enabled, it means that a sync point pass was added, and the ready frame processing will be done later
	if (!UseExperimentalScheduling())
	{
		if (TSharedPtr<FCaptureFrame> NextPending = FrameManager->PeekNextPending<FCaptureFrame>())
		{
			UE_LOG(LogMediaIOCore, Verbose, TEXT("[%s - %s] - Processing pending frame %d"), *MediaOutputName, *FThreadManager::GetThreadName(FPlatformTLS::GetCurrentThreadId()), NextPending ? NextPending->FrameId : -1);
			ProcessReadyFrame_RenderThread(Args.GraphBuilder.RHICmdList, this, NextPending);
		}
	}
}

UTextureRenderTarget2D* UMediaCapture::GetTextureRenderTarget() const
{
	if (CaptureSource)
	{
		return CaptureSource->GetRenderTarget();
	}
	return nullptr;
}

TSharedPtr<FSceneViewport> UMediaCapture::GetCapturingSceneViewport() const
{
	if (CaptureSource)
	{
		return CaptureSource->GetSceneViewport();
	}
	return nullptr;
}

FString UMediaCapture::GetCaptureSourceType() const
{
	const UEnum* EnumType = StaticEnum<EMediaCaptureSourceType>();
	check(EnumType);
	return EnumType->GetNameStringByValue((int8)CaptureSourceType);
}

void UMediaCapture::SetState(EMediaCaptureState InNewState)
{
	if (MediaState != InNewState)
	{
		MediaState = InNewState;
		if (IsInGameThread())
		{
			BroadcastStateChanged();
		}
		else
		{
			TWeakObjectPtr<UMediaCapture> Self = this;
			AsyncTask(ENamedThreads::GameThread, [Self]
			{
				UMediaCapture* MediaCapture = Self.Get();
				if (UObjectInitialized() && MediaCapture)
				{
					MediaCapture->BroadcastStateChanged();
				}
			});
		}
	}
}

void UMediaCapture::RestartCapture()
{
	check(IsInGameThread());

	UE_LOG(LogMediaIOCore, Log, TEXT("Media Capture restarting for new size %dx%d"), CaptureSource->GetSize().X, CaptureSource->GetSize().Y);

	if (FrameManager)
	{
		FrameManager->ForEachFrame([](const TSharedPtr<UE::MediaCaptureData::FFrame> InFrame)
		{
			StaticCastSharedPtr<UE::MediaCaptureData::FCaptureFrame>(InFrame)->bMediaCaptureActive = false;
		});
	}

	constexpr bool bAllowPendingFrameToBeProcess = false;
	if (GetState() != EMediaCaptureState::Stopped)
	{
		SetState(EMediaCaptureState::Preparing);

		WaitForPendingTasks();

		StopCaptureImpl(bAllowPendingFrameToBeProcess);

		DesiredSize = CaptureSource->GetSize();
		CacheOutputOptions();

		bool bInitialized = InitializeCapture();
		if (bInitialized)
		{
			bInitialized = CaptureSource->PostInitialize();
		}

		// This could have been updated by the initialization done by the implementation
		bShouldCaptureRHIResource = ShouldCaptureRHIResource();

		if (bInitialized)
		{
			bOutputResourcesInitialized = false;
			InitializeOutputResources(MediaOutput->NumberOfTextureBuffers);
			bInitialized = GetState() != EMediaCaptureState::Stopped;
		}

		if (!bInitialized)
		{
			SetState(EMediaCaptureState::Stopped);
			MediaCaptureDetails::ShowSlateNotification();
		}
	}

	bIsAutoRestartRequired = false;
}

void UMediaCapture::BroadcastStateChanged()
{
	OnStateChanged.Broadcast();
	OnStateChangedNative.Broadcast();
}

void UMediaCapture::SetFixedViewportSize(TSharedPtr<FSceneViewport> InSceneViewport, FIntPoint InSize)
{
	InSceneViewport->SetFixedViewportSize(InSize.X, InSize.Y);
	bViewportHasFixedViewportSize = true;
}

void UMediaCapture::ResetFixedViewportSize(TSharedPtr<FSceneViewport> InViewport, bool bInFlushRenderingCommands)
{
	if (bViewportHasFixedViewportSize && InViewport.IsValid())
	{
		if (bInFlushRenderingCommands)
		{
			WaitForPendingTasks();
		}
		InViewport->SetFixedViewportSize(0, 0);
		bViewportHasFixedViewportSize = false;
	}
}

void UMediaCapture::WaitForSingleExperimentalSchedulingTaskToComplete()
{
	if (UseExperimentalScheduling())
	{
		// Presumably the rendering thread could be in the process of dispatching the task
		FScopeLock ScopedLock(&PendingReadbackTasksCriticalSection);
		if (PendingReadbackTasks.Num())
		{
			auto It = PendingReadbackTasks.CreateIterator();

			// Go through the list to find a pending task.
			while (It && It->IsCompleted())
			{
				It.RemoveCurrent();
				++It;
			}

			if (It)
			{
				// We have a pending task, wait until it's completeted.
				It->BusyWait();
			}
		}

		if (!UseAnyThreadCapture())
		{
			// We flush after task completion in case a render thread task was launched.
			if (IsInGameThread())
			{
				FlushRenderingCommands();
			}
		}
	}
}

void UMediaCapture::CleanupCompletedExperimentalSchedulingTasks()
{
	FScopeLock ScopedLock(&PendingReadbackTasksCriticalSection);

	for (auto It = PendingReadbackTasks.CreateIterator(); It; ++It)
	{
		if (It->IsCompleted())
		{
			It.RemoveCurrent();
		}
	}
}

void UMediaCapture::WaitForAllExperimentalSchedulingTasksToComplete()
{
	if (UseExperimentalScheduling())
	{
		FScopeLock ScopedLock(&PendingReadbackTasksCriticalSection);

		// Clean up completed tasks.
		for (auto It = PendingReadbackTasks.CreateIterator(); It; ++It)
		{
			if (It->IsCompleted())
			{
				It.RemoveCurrent();
			}
			else
			{
				It->BusyWait();
				It.RemoveCurrent();
			}
		}

		
		if (!UseAnyThreadCapture())
		{
			// This code might have dispatched a task on the render thread, so we need to wait again
			if (IsInGameThread())
			{
				FlushRenderingCommands();
			}
		}
	}
}

void UMediaCapture::WaitForPendingTasks()
{
	while (WaitingForRenderCommandExecutionCounter.load() > 0)
	{
		FlushRenderingCommands();
	}

	WaitForAllExperimentalSchedulingTasksToComplete();
}

void UMediaCapture::ProcessCapture_GameThread()
{
	using namespace UE::MediaCaptureData;
	
	CleanupCompletedExperimentalSchedulingTasks();

	// Acquire a frame
	TSharedPtr<FCaptureFrame> CapturingFrame;
	if(GetState() != EMediaCaptureState::StopRequested)
	{
		CapturingFrame = FrameManager->GetNextAvailable<FCaptureFrame>();
	}

	// Handle frame overrun (couldn't acquire a frame)
	if (!CapturingFrame && GetState() != EMediaCaptureState::StopRequested)
	{
		if (DesiredCaptureOptions.OverrunAction == EMediaCaptureOverrunAction::Flush)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(MediaCapture::FlushRenderingCommands);
			UE_LOG(LogMediaIOCore, VeryVerbose, TEXT("[%s] - Flushing commands."), *MediaOutputName);

			FlushRenderingCommands();
			
			WaitForSingleExperimentalSchedulingTaskToComplete();

			// After flushing, we should have access to an available frame, if not, it's not expected.
			CapturingFrame = FrameManager->GetNextAvailable<FCaptureFrame>();
			if (!CapturingFrame)
			{
				UE_LOG(LogMediaIOCore, Error, TEXT("[%s] - Flushing commands didn't give us back available frames to process.") , *MediaOutputName);
				StopCapture(false);
				return;
			}
		}
		else if (DesiredCaptureOptions.OverrunAction == EMediaCaptureOverrunAction::Skip)
		{
			// Selected options is to skip capturing a frame if overrun happens
			UE_LOG(LogMediaIOCore, Verbose, TEXT("[%s] - No frames available for capture. Skipping"), *MediaOutputName);
		}
	}

	// Initialize capture frame
	InitializeCaptureFrame(CapturingFrame);

	PrintFrameState();
	
	PrepareAndDispatchCapture_GameThread(CapturingFrame);
}

void UMediaCapture::PrepareAndDispatchCapture_GameThread(const TSharedPtr<UE::MediaCaptureData::FCaptureFrame>& CapturingFrame)
{
	using namespace UE::MediaCaptureData;
	
	// Init variables for ENQUEUE_RENDER_COMMAND.
	//The Lock only synchronize while we are copying the value to the enqueue. The viewport and the rendertarget may change while we are in the enqueue command.
	{
		FScopeLock Lock(&AccessingCapturingSource);

		FIntPoint InDesiredSize = DesiredSize;
		UMediaCapture* InMediaCapture = this;

		if (CaptureSource)
		{
			++WaitingForRenderCommandExecutionCounter;

			const FRHIGPUMask SourceGPUMask = ValidSourceGPUMask;

			UE_LOG(LogMediaIOCore, Verbose, TEXT("[%s] - Queuing frame to capture %d"), *InMediaCapture->MediaOutputName, CapturingFrame ? CapturingFrame->FrameId : -1);

			// RenderCommand to be executed on the RenderThread
			ENQUEUE_RENDER_COMMAND(FMediaOutputCaptureFrameCreateTexture)(
				[InMediaCapture, CapturingFrame, InDesiredSize, SourceGPUMask](FRHICommandListImmediate& RHICmdList)
			{
				SCOPED_GPU_MASK(RHICmdList, SourceGPUMask);

				TSharedPtr<FCaptureFrame> NextPending = InMediaCapture->FrameManager->PeekNextPending<FCaptureFrame>();
				if (NextPending && CapturingFrame)
				{
					ensure(NextPending->FrameId != CapturingFrame->FrameId);
				}
				
				// Capture frame
				{
					FRDGBuilder GraphBuilder(RHICmdList);
					
					FTexture2DRHIRef SourceTexture = InMediaCapture->CaptureSource->GetSourceTextureForInput_RenderThread(RHICmdList);
					
					FCaptureFrameArgs CaptureArgs{ GraphBuilder };
					CaptureArgs.MediaCapture = InMediaCapture;
					CaptureArgs.DesiredSize = InDesiredSize;
					CaptureArgs.ResourceToCapture = MoveTemp(SourceTexture);

					InMediaCapture->ProcessCapture_RenderThread(CapturingFrame, CaptureArgs);
					UE_LOG(LogMediaIOCore, Verbose, TEXT("[%s] - Captured frame %d"), *InMediaCapture->MediaOutputName, CapturingFrame ? CapturingFrame->FrameId : -1);
					
					GraphBuilder.Execute();
				}
				
				if (!InMediaCapture->UseExperimentalScheduling())
				{
					//Process the next pending frame
					UE_LOG(LogMediaIOCore, Verbose, TEXT("[%s] - Processing pending frame %d"), *InMediaCapture->MediaOutputName, NextPending ? NextPending->FrameId : -1);
					if (NextPending)
					{
						InMediaCapture->ProcessReadyFrame_RenderThread(RHICmdList, InMediaCapture, NextPending);
					}
				}

				// Whatever happens, we want to decrement our counter to track enqueued commands
				--InMediaCapture->WaitingForRenderCommandExecutionCounter;
			});

			//If auto-stopping, count the number of frame captures requested and stop when reaching 0.
			if (DesiredCaptureOptions.bAutostopOnCapture && GetState() == EMediaCaptureState::Capturing && --DesiredCaptureOptions.NumberOfFramesToCapture <= 0)
			{
				StopCapture(true);
			}
		}
	}
}

bool UMediaCapture::HasFinishedProcessing() const
{
	return WaitingForRenderCommandExecutionCounter.load() == 0
		|| GetState() == EMediaCaptureState::Error
		|| GetState() == EMediaCaptureState::Stopped;
}

void UMediaCapture::SetValidSourceGPUMask(FRHIGPUMask GPUMask)
{
	ValidSourceGPUMask = GPUMask;
}

void UMediaCapture::InitializeOutputResources(int32 InNumberOfBuffers)
{
	using namespace UE::MediaCaptureData;

	if (DesiredOutputSize.X <= 0 || DesiredOutputSize.Y <= 0)
	{
		UE_LOG(LogMediaIOCore, Error, TEXT("Can't start the capture. The size requested is negative or zero."));
		SetState(EMediaCaptureState::Stopped);
		return;
	}

	// Recreate frame manager which can trigger cleaning up its captured frames if it exists
	FrameManager = MakePimpl<FFrameManager>();

	bOutputResourcesInitialized = false;
	bSyncHandlersInitialized = false;
	NumberOfCaptureFrame = InNumberOfBuffers;

	UMediaCapture* This = this;
	ENQUEUE_RENDER_COMMAND(MediaOutputCaptureFrameCreateResources)(
	[This](FRHICommandListImmediate& RHICmdList)
		{
			for (int32 Index = 0; Index < This->NumberOfCaptureFrame; ++Index)
			{
				if (This->DesiredOutputResourceType == EMediaCaptureResourceType::Texture)
				{
					TSharedPtr<FTextureCaptureFrame> NewFrame = MakeShared<FTextureCaptureFrame>(Index);
					FRDGTextureDesc OutputDesc = FRDGTextureDesc::Create2D(
						This->DesiredOutputSize,
						This->DesiredOutputPixelFormat,
						FClearValueBinding::None,
						TexCreate_Shared | TexCreate_RenderTargetable | TexCreate_UAV);


					NewFrame->RenderTarget = AllocatePooledTexture(OutputDesc, *FString::Format(TEXT("MediaCapture RenderTarget {0}"), { Index }));
					
					// Only create CPU readback resource when we are using the CPU callback
					if (!This->bShouldCaptureRHIResource)
					{
						NewFrame->ReadbackTexture = MakeUnique<FRHIGPUTextureReadback>(*FString::Printf(TEXT("MediaCaptureTextureReadback_%d"), Index));
					}

					This->FrameManager->AddFrame(MoveTemp(NewFrame));
				}
				else
				{
					TSharedPtr<FBufferCaptureFrame> NewFrame = MakeShared<FBufferCaptureFrame>(Index);

					if (This->DesiredOutputBufferDescription.NumElements > 0)
					{
						NewFrame->Buffer = AllocatePooledBuffer(This->DesiredOutputBufferDescription, *FString::Format(TEXT("MediaCapture BufferResource {0}"), { Index }));

						// Only create CPU readback resource when we are using the CPU callback
						if (!This->bShouldCaptureRHIResource)
						{
							NewFrame->ReadbackBuffer = MakeUnique<FRHIGPUBufferReadback>(*FString::Printf(TEXT("MediaCaptureBufferReadback_%d"), Index));
						}

						This->FrameManager->AddFrame(MoveTemp(NewFrame));
					}
					else
					{
						UE_LOG(LogMediaIOCore, Error, TEXT("Can't start the capture. Trying to allocate buffer resource but number of elements to allocate was 0."));
						This->SetState(EMediaCaptureState::Error);
					}
				}
			}
			This->bOutputResourcesInitialized = true;
		});
}

void UMediaCapture::InitializeSyncHandlers_RenderThread()
{
	SyncHandlers.Reset(NumberOfCaptureFrame);
	for (int32 Index = 0; Index < NumberOfCaptureFrame; ++Index)
	{
		TSharedPtr<FMediaCaptureSyncData> SyncData = MakeShared<FMediaCaptureSyncData>();
		SyncData->RHIFence = RHICreateGPUFence(*FString::Printf(TEXT("MediaCaptureSync_%02d"), Index));
		SyncHandlers.Add(MoveTemp(SyncData));
	}

	bSyncHandlersInitialized = true;
}

void UMediaCapture::InitializeCaptureFrame(const TSharedPtr<UE::MediaCaptureData::FCaptureFrame>& CaptureFrame)
{
	if (CaptureFrame)
	{
		CaptureFrame->CaptureBaseData.SourceFrameTimecode = FApp::GetTimecode();
		CaptureFrame->CaptureBaseData.SourceFrameTimecodeFramerate = FApp::GetTimecodeFrameRate();
		CaptureFrame->CaptureBaseData.SourceFrameNumberRenderThread = GFrameCounter;
		CaptureFrame->CaptureBaseData.SourceFrameNumber = ++CaptureRequestCount;
		CaptureFrame->UserData = GetCaptureFrameUserData_GameThread();
	}
}

TSharedPtr<UMediaCapture::FMediaCaptureSyncData> UMediaCapture::GetAvailableSyncHandler() const
{
	const auto FindAvailableHandlerFunc = [](const TSharedPtr<FMediaCaptureSyncData>& Item)
	{
		if (Item->bIsBusy == false)
		{
			return true;
		}

		return false;
	};

	if (const TSharedPtr<FMediaCaptureSyncData>* FoundItem = SyncHandlers.FindByPredicate(FindAvailableHandlerFunc))
	{
		return *FoundItem;
	}

	return nullptr;
}

bool UMediaCapture::ValidateMediaOutput() const
{
	if (MediaOutput == nullptr)
	{
		UE_LOG(LogMediaIOCore, Error, TEXT("Can not start the capture. The Media Output is invalid."));
		return false;
	}

	FString FailureReason;
	if (!MediaOutput->Validate(FailureReason))
	{
		UE_LOG(LogMediaIOCore, Error, TEXT("Can not start the capture. %s."), *FailureReason);
		return false;
	}

	if(DesiredCaptureOptions.bAutostopOnCapture && DesiredCaptureOptions.NumberOfFramesToCapture < 1)
	{
		UE_LOG(LogMediaIOCore, Error, TEXT("Can not start the capture. Please set the Number Of Frames To Capture when using Autostop On Capture in the Media Capture Options"));
		return false;
	}

	return true;
}

bool UMediaCapture::CaptureSceneViewportImpl(TSharedPtr<FSceneViewport>&InSceneViewport)
{
	return false;
}

bool UMediaCapture::CaptureRenderTargetImpl(UTextureRenderTarget2D * InRenderTarget)
{
	return false;
}

void UMediaCapture::OnBeginFrame_GameThread()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(MediaCapture::BeginFrame);

	if (ViewExtension && !ViewExtension->IsValid())
	{
		SetState(EMediaCaptureState::Error);
	}

	if (GetState() == EMediaCaptureState::Error)
	{
		StopCapture(false);
		return;
	}

	if (CaptureSource->GetCaptureType() == UE::MediaCapture::Private::ECaptureType::Immediate && (GetState() == EMediaCaptureState::Capturing))
	{
		// Queue capture data to be consumed when capture requests are done on render thread
		FQueuedCaptureData CaptureData;
		CaptureData.BaseData.SourceFrameTimecode = FApp::GetTimecode();
		CaptureData.BaseData.SourceFrameTimecodeFramerate = FApp::GetTimecodeFrameRate();
		CaptureData.BaseData.SourceFrameNumberRenderThread = GFrameCounter;
		CaptureData.UserData = GetCaptureFrameUserData_GameThread();

		FScopeLock Lock(&CaptureDataQueueCriticalSection);
		CaptureDataQueue.Insert(MoveTemp(CaptureData), 0);
	}
}

void UMediaCapture::OnEndFrame_GameThread()
{
	using namespace UE::MediaCaptureData;

	TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(*FString::Printf(TEXT("MediaCapture End Frame %d"), GFrameCounter));

	if (!bOutputResourcesInitialized)
	{
		FlushRenderingCommands();
	}

	if (bIsAutoRestartRequired)
	{
		return;
	}

	if (!MediaOutput)
	{
		return;
	}

	if (GetState() == EMediaCaptureState::Error)
	{
		StopCapture(false);
	}

	if (GetState() != EMediaCaptureState::Capturing && GetState() != EMediaCaptureState::StopRequested)
	{
		return;
	}

	if (DesiredCaptureOptions.bSkipFrameWhenRunningExpensiveTasks && !FSlateThrottleManager::Get().IsAllowingExpensiveTasks())
	{
		return;
	}

	if (GetState() == EMediaCaptureState::StopRequested && PendingFrameCount.load() <= 0)
	{
		// All the requested frames have been captured.
		StopCapture(false);
		return;
	}

	{
		FScopeLock ScopedLock(&PendingReadbackTasksCriticalSection);

		// Clean up completed tasks.
		for (auto It = PendingReadbackTasks.CreateIterator(); It; ++It)
		{
			if (It->IsCompleted())
			{
				It.RemoveCurrent();
			}
		}
	}

	ProcessCapture_GameThread();
}

bool UMediaCapture::ProcessCapture_RenderThread(const TSharedPtr<UE::MediaCaptureData::FCaptureFrame>& CapturingFrame, const UE::MediaCaptureData::FCaptureFrameArgs& Args)
{
	using namespace UE::MediaCaptureData;
	
	RDG_GPU_STAT_SCOPE(Args.GraphBuilder, MediaCapture_ProcessCapture)
	TRACE_CPUPROFILER_EVENT_SCOPE(UMediaCapture::ProcessCapture_RenderThread);
	int FrameNumber = -1;
	if (CapturingFrame)
	{
		FrameNumber = CapturingFrame->CaptureBaseData.SourceFrameNumberRenderThread;
	}
	TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(*FString::Printf(TEXT("Process Capture Render Thread Frame %d"), FrameNumber));

	if (CapturingFrame)
	{
		// Call the capture frame algo based on the specific type of resource we are using
		bool bHasCaptureSuceeded = false;
		if (DesiredOutputResourceType == EMediaCaptureResourceType::Texture)
		{
			if (ensure(CapturingFrame->IsTextureResource()))
			{
				Args.MediaCapture->BeforeFrameCaptured_RenderingThread(CapturingFrame->CaptureBaseData, CapturingFrame->UserData, CapturingFrame->GetTextureResource());
				bHasCaptureSuceeded = FMediaCaptureHelper::CaptureFrame(Args, CapturingFrame);
			}
			else
			{
				UE_LOG(LogMediaIOCore, Error, TEXT("The capture will stop for '%s'. Capture frame was expected to use Texture resource but wasn't."), *Args.MediaCapture->MediaOutputName);
			}
		}
		else
		{
			if (ensure(CapturingFrame->IsBufferResource()))
			{
				Args.MediaCapture->BeforeFrameCaptured_RenderingThread(CapturingFrame->CaptureBaseData, CapturingFrame->UserData, CapturingFrame->GetBufferResource());
				bHasCaptureSuceeded = FMediaCaptureHelper::CaptureFrame(Args, CapturingFrame);
			}
			else
			{
				UE_LOG(LogMediaIOCore, Error, TEXT("The capture will stop for '%s'. Capture frame was expected to use Buffer resource but wasn't."), *Args.MediaCapture->MediaOutputName);
			}
		}
		
		if (bHasCaptureSuceeded == false)
		{
			if (Args.MediaCapture->bIsAutoRestartRequired)
			{
				TWeakObjectPtr<UMediaCapture> Self = this;
				AsyncTask(ENamedThreads::GameThread, [Self]
				{
					UMediaCapture* MediaCapture = Self.Get();
					if (UObjectInitialized() && MediaCapture)
					{
						MediaCapture->RestartCapture();
					}
				});
			}
			else
			{
				Args.MediaCapture->SetState(EMediaCaptureState::Error);
			}
		}

		return bHasCaptureSuceeded;
	}

	return false;
}

bool UMediaCapture::ProcessReadyFrame_RenderThread(FRHICommandListImmediate& RHICmdList, UMediaCapture* InMediaCapture, const TSharedPtr<UE::MediaCaptureData::FCaptureFrame>& ReadyFrame)
{
	using namespace UE::MediaCaptureData;
	TRACE_CPUPROFILER_EVENT_SCOPE(UMediaCapture::ProcessReadyFrame_RenderThread);

	bool bWasFrameProcessed = true;
	if (InMediaCapture->GetState() != EMediaCaptureState::Error)
	{
		if (ReadyFrame->bReadbackRequested)
		{
			FRHIGPUMask GPUMask;
#if WITH_MGPU
			GPUMask = RHICmdList.GetGPUMask();

			// If GPUMask is not set to a specific GPU we and since we are reading back the texture, it shouldn't matter which GPU we do this on.
			if (!GPUMask.HasSingleIndex())
			{
				GPUMask = FRHIGPUMask::FromIndex(GPUMask.GetFirstIndex());
			}

			SCOPED_GPU_MASK(RHICmdList, GPUMask);
#endif
			// Lock & read
			void* ColorDataBuffer = nullptr;
			int32 RowStride = 0;

			// If readback is ready, proceed. 
			// If not, proceed with locking only if we are in flush mode since it will block until gpu is idle.
			const bool bIsReadbackReady = InMediaCapture->DesiredCaptureOptions.OverrunAction == EMediaCaptureOverrunAction::Flush || ReadyFrame->IsReadbackReady(GPUMask) == true;
			if (bIsReadbackReady)
			{
				SCOPE_CYCLE_COUNTER(STAT_MediaCapture_RenderThread_LockResource);
				ColorDataBuffer = ReadyFrame->Lock(RHICmdList, RowStride);
			}
			else
			{
				UE_LOG(LogMediaIOCore, VeryVerbose, TEXT("[%s] - Readback %d not ready. Skipping."), *InMediaCapture->MediaOutputName, ReadyFrame->FrameId);
				bWasFrameProcessed = false;
			}

			if (ColorDataBuffer)
			{
				{
					SCOPE_CYCLE_COUNTER(STAT_MediaCapture_RenderThread_RHI_CaptureCallback)
					TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(*FString::Printf(TEXT("MediaCapture Output Frame %d"), ReadyFrame->CaptureBaseData.SourceFrameNumberRenderThread));

					// The Width/Height of the surface may be different then the DesiredOutputSize : Some underlying implementations enforce a specific stride, therefore
					// there may be padding at the end of each row.
					InMediaCapture->OnFrameCaptured_RenderingThread(ReadyFrame->CaptureBaseData, ReadyFrame->UserData, ColorDataBuffer, InMediaCapture->DesiredOutputSize.X, InMediaCapture->DesiredOutputSize.Y, RowStride);
				}

				ReadyFrame->Unlock();
			}

			if (bIsReadbackReady)
			{
				UE_LOG(LogMediaIOCore, Verbose, TEXT("[%s - %s] - Completed pending frame %d."), *InMediaCapture->MediaOutputName, *FThreadManager::GetThreadName(FPlatformTLS::GetCurrentThreadId()), ReadyFrame->FrameId);
				ReadyFrame->bReadbackRequested = false;
				--PendingFrameCount;
				InMediaCapture->FrameManager->CompleteNextPending(*ReadyFrame);
			}
		}
		else if (InMediaCapture->bShouldCaptureRHIResource && ReadyFrame->bDoingGPUCopy)
		{
			if (ReadyFrame->IsTextureResource())
			{
				{
					TRACE_CPUPROFILER_EVENT_SCOPE(UMediaCapture::UnlockDMATexture_RenderThread);
					UnlockDMATexture_RenderThread(ReadyFrame->GetTextureResource());
				}

				TRACE_CPUPROFILER_EVENT_SCOPE(UMediaCapture::RHIResourceCaptured);
				TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(*FString::Printf(TEXT("MediaCapture Output Frame %d"), ReadyFrame->CaptureBaseData.SourceFrameNumberRenderThread));
				SCOPE_CYCLE_COUNTER(STAT_MediaCapture_RenderThread_CaptureCallback)
					InMediaCapture->OnRHIResourceCaptured_RenderingThread(ReadyFrame->CaptureBaseData, ReadyFrame->UserData, ReadyFrame->GetTextureResource());
			}
			else
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(UMediaCapture::RHIResourceCaptured);
				InMediaCapture->OnRHIResourceCaptured_RenderingThread(ReadyFrame->CaptureBaseData, ReadyFrame->UserData, ReadyFrame->GetBufferResource());
			}

			UE_LOG(LogMediaIOCore, Verbose, TEXT("[%s - %s] - Completed pending frame %d."), *InMediaCapture->MediaOutputName, *FThreadManager::GetThreadName(FPlatformTLS::GetCurrentThreadId()), ReadyFrame->FrameId);
			ReadyFrame->bDoingGPUCopy = false;
			InMediaCapture->FrameManager->CompleteNextPending(*ReadyFrame);
			--PendingFrameCount;
		}
	}

	return bWasFrameProcessed;
}

void UMediaCapture::PrintFrameState()
{
	UE_LOG(LogMediaIOCore, VeryVerbose, TEXT("%s"), *FrameManager->GetFramesState());
}

/* namespace MediaCaptureDetails implementation
*****************************************************************************/
namespace MediaCaptureDetails
{
	bool FindSceneViewportAndLevel(TSharedPtr<FSceneViewport>& OutSceneViewport)
	{
#if WITH_EDITOR
		if (GIsEditor)
		{
			for (const FWorldContext& Context : GEngine->GetWorldContexts())
			{
				if (Context.WorldType == EWorldType::PIE)
				{
					UEditorEngine* EditorEngine = CastChecked<UEditorEngine>(GEngine);
					FSlatePlayInEditorInfo& Info = EditorEngine->SlatePlayInEditorMap.FindChecked(Context.ContextHandle);

					// The PIE window has priority over the regular editor window, so we need to break out of the loop if either of these are found
					if (TSharedPtr<IAssetViewport> DestinationLevelViewport = Info.DestinationSlateViewport.Pin())
					{
						OutSceneViewport = DestinationLevelViewport->GetSharedActiveViewport();
						break;
					}
					else if (Info.SlatePlayInEditorWindowViewport.IsValid())
					{
						OutSceneViewport = Info.SlatePlayInEditorWindowViewport;
						break;
					}
				}
				else if (Context.WorldType == EWorldType::Editor)
				{
					if (FLevelEditorModule* LevelEditorModule = FModuleManager::GetModulePtr<FLevelEditorModule>(LevelEditorName))
					{
						TSharedPtr<IAssetViewport> ActiveLevelViewport = LevelEditorModule->GetFirstActiveViewport();
						if (ActiveLevelViewport.IsValid())
						{
							OutSceneViewport = ActiveLevelViewport->GetSharedActiveViewport();
						}
					}
				}
			}
		}
		else
#endif
		{
			UGameEngine* GameEngine = CastChecked<UGameEngine>(GEngine);
			OutSceneViewport = GameEngine->SceneViewport;
		}

		return (OutSceneViewport.IsValid());
	}

	bool ValidateIsCapturing(const UMediaCapture* CaptureToBeValidated)
	{
		if (CaptureToBeValidated->GetState() != EMediaCaptureState::Capturing && CaptureToBeValidated->GetState() != EMediaCaptureState::Preparing)
		{
			UE_LOG(LogMediaIOCore, Error, TEXT("Can not update the capture. There is no capture currently.\
			Only use UpdateSceneViewport or UpdateTextureRenderTarget2D when the state is Capturing or Preparing"));
			return false;
		}

		return true;
	}

	void ShowSlateNotification()
	{
#if WITH_EDITOR
		if (GIsEditor)
		{
			static double PreviousWarningTime = 0.0;
			const double TimeNow = FPlatformTime::Seconds();
			const double TimeBetweenWarningsInSeconds = 3.0f;

			if (TimeNow - PreviousWarningTime > TimeBetweenWarningsInSeconds)
			{
				FNotificationInfo NotificationInfo(LOCTEXT("MediaCaptureFailedError", "The media failed to capture. Check Output Log for details!"));
				NotificationInfo.ExpireDuration = 2.0f;
				FSlateNotificationManager::Get().AddNotification(NotificationInfo);

				PreviousWarningTime = TimeNow;
			}
		}
#endif // WITH_EDITOR
	}

	int32 GetBytesPerPixel(EPixelFormat InPixelFormat)
	{
		//We can capture viewports and render targets. Possible pixel format is limited by that
		switch (InPixelFormat)
		{

		case PF_A8:
		case PF_R8_UINT:
		case PF_R8_SINT:
		case PF_G8:
		{
			return 1;
		}
		case PF_R16_UINT:
		case PF_R16_SINT:
		case PF_R5G6B5_UNORM:
		case PF_R8G8:
		case PF_R16F:
		case PF_R16F_FILTER:
		case PF_V8U8:
		case PF_R8G8_UINT:
		case PF_B5G5R5A1_UNORM:
		{
			return 2;
		}
		case PF_R32_UINT:
		case PF_R32_SINT:
		case PF_R8G8B8A8:
		case PF_A8R8G8B8:
		case PF_FloatR11G11B10:
		case PF_A2B10G10R10:
		case PF_G16R16:
		case PF_G16R16F:
		case PF_G16R16F_FILTER:
		case PF_R32_FLOAT:
		case PF_R16G16_UINT:
		case PF_R8G8B8A8_UINT:
		case PF_R8G8B8A8_SNORM:
		case PF_B8G8R8A8:
		case PF_G16R16_SNORM:
		case PF_FloatRGB: //Equivalent to R11G11B10
		{
			return 4;
		}
		case PF_R16G16B16A16_UINT:
		case PF_R16G16B16A16_SINT:
		case PF_A16B16G16R16:
		case PF_G32R32F:
		case PF_R16G16B16A16_UNORM:
		case PF_R16G16B16A16_SNORM:
		case PF_R32G32_UINT:
		case PF_R64_UINT:
		case PF_FloatRGBA: //Equivalent to R16G16B16A16
		{
			return 8;
		}
		case PF_A32B32G32R32F:
		case PF_R32G32B32A32_UINT:
		{
			return 16;
		}
		default:
		{
			ensureMsgf(false, TEXT("MediaCapture - Pixel format (%d) not handled. Invalid bytes per pixel returned."), InPixelFormat);
			return 0;
		}
		}
	}
}

#undef LOCTEXT_NAMESPACE

