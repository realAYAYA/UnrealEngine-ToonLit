// Copyright Epic Games, Inc. All Rights Reserved.

#include "MediaCapture.h"


#include "Application/ThrottleManager.h"
#include "Async/Async.h"
#include "Engine/GameEngine.h"
#include "Engine/RendererSettings.h"
#include "Engine/TextureRenderTarget2D.h"
#include "EngineModule.h"
#include "MediaIOCoreModule.h"
#include "MediaOutput.h"
#include "MediaShaders.h"
#include "Misc/App.h"
#include "Misc/ScopeLock.h"
#include "PipelineStateCache.h"
#include "RendererInterface.h"
#include "RenderGraphResources.h"
#include "RenderGraphUtils.h"
#include "RenderUtils.h"
#include "RenderTargetPool.h"
#include "RHIStaticStates.h"
#include "SceneUtils.h"
#include "ScreenPass.h"
#include "Slate/SceneViewport.h"
#include "UObject/WeakObjectPtrTemplates.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MediaCapture)

#if WITH_EDITOR
#include "AnalyticsEventAttribute.h"
#include "Editor.h"
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


/** Time spent in media capture sending a frame. */
DECLARE_CYCLE_STAT(TEXT("MediaCapture RenderThread FrameCapture"), STAT_MediaCapture_RenderThread_FrameCapture, STATGROUP_Media);
DECLARE_CYCLE_STAT(TEXT("MediaCapture RenderThread LockResource"), STAT_MediaCapture_RenderThread_LockResource, STATGROUP_Media);
DECLARE_CYCLE_STAT(TEXT("MediaCapture RenderThread RHI Capture Callback"), STAT_MediaCapture_RenderThread_CaptureCallback, STATGROUP_Media);
DECLARE_GPU_STAT(MediaCapture_CaptureFrame);
DECLARE_GPU_STAT(MediaCapture_CustomCapture);
DECLARE_GPU_STAT(MediaCapture_Conversion);
DECLARE_GPU_STAT(MediaCapture_Readback);
DECLARE_GPU_STAT(MediaCapture_ProcessCapture);


/** These pixel formats do not require additional conversion except for swizzling and normalized sampling. */
static TSet<EPixelFormat> SupportedRgbaSwizzleFormats =
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

/* namespace MediaCaptureDetails definition
*****************************************************************************/

namespace MediaCaptureDetails
{
	bool FindSceneViewportAndLevel(TSharedPtr<FSceneViewport>& OutSceneViewport);

	//Validation for the source of a capture
	bool ValidateSceneViewport(const TSharedPtr<FSceneViewport>& SceneViewport, const FMediaCaptureOptions& CaptureOption, const FIntPoint& DesiredSize, const EPixelFormat DesiredPixelFormat, const bool bCurrentlyCapturing);
	bool ValidateTextureRenderTarget2D(const UTextureRenderTarget2D* RenderTarget, const FMediaCaptureOptions& CaptureOption, const FIntPoint& DesiredSize, const EPixelFormat DesiredPixelFormat, const bool bCurrentlyCapturing);

	//Validation that there is a capture
	bool ValidateIsCapturing(const UMediaCapture& CaptureToBeValidated);

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
	class FCaptureFrame
	{
	public:
		FCaptureFrame(int32 InFrameId)
			: FrameId(InFrameId)
			, bReadbackRequested(false)
			, bDoingGPUCopy(false)
		{

		}
		virtual ~FCaptureFrame() {};

		/** Returns true if its output resource is valid */
		virtual bool HasValidResource() const = 0;

		/** Simple way to validate the resource type and cast safely */
		virtual bool IsTextureResource() const = 0;
		virtual bool IsBufferResource() const = 0;

		/** Locks the readback resource and returns a pointer to access data from system memory */
		virtual void* Lock(FRHICommandListImmediate& RHICmdList, FRHIGPUMask GPUMask, int32& OutRowStride) = 0;

		/** Unlocks the readback resource */
		virtual void Unlock() = 0;

		/** Returns true if the readback is ready to be used */
		virtual bool IsReadbackReady(FRHIGPUMask GPUMask) = 0;

		virtual FRHITexture* GetTextureResource() { return nullptr; }
		virtual FRHIBuffer* GetBufferResource() { return nullptr; }

		bool IsPending() const { return bReadbackRequested || bDoingGPUCopy; }

		int32 FrameId = 0;
		UMediaCapture::FCaptureBaseData CaptureBaseData;
		TAtomic<bool> bReadbackRequested;
		TAtomic<bool> bDoingGPUCopy;
		TSharedPtr<FMediaCaptureUserData> UserData;
	};

	class FTextureCaptureFrame : public FCaptureFrame
	{
	public:
		/** Type alias for the output resource type used during capture frame */
		using FOutputResourceType = FRDGTextureRef;

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

		virtual void* Lock(FRHICommandListImmediate& RHICmdList, FRHIGPUMask GPUMask, int32& OutRowStride) override
		{
			if (ReadbackTexture->IsReady(GPUMask) == false)
			{
				UE_LOG(LogMediaIOCore, Verbose, TEXT("Fenced for texture readback was not ready"));
			}

			int32 ReadbackWidth;
			void* ReadbackPointer = ReadbackTexture->Lock(ReadbackWidth);
			OutRowStride = ReadbackWidth * MediaCaptureDetails::GetBytesPerPixel(RenderTarget->GetDesc().Format);
			return ReadbackPointer;
		}

		virtual void Unlock() override
		{
			ReadbackTexture->Unlock();
		}

		virtual bool IsReadbackReady(FRHIGPUMask GPUMask) override
		{
			return ReadbackTexture->IsReady(GPUMask);
		}

		//~ End FCaptureFrame interface

		/** Registers an external texture to be tracked by the graph and returns a pointer to the tracked resource */
		FRDGTextureRef RegisterResource(FRDGBuilder& RDGBuilder)
		{
			return RDGBuilder.RegisterExternalTexture(RenderTarget, TEXT("OutputTexture"));
		}

		/** Adds a readback pass to the graph */
		void EnqueueCopy(FRDGBuilder& RDGBuilder, FRDGTextureRef ResourceToReadback)
		{
			AddEnqueueCopyPass(RDGBuilder, ReadbackTexture.Get(), ResourceToReadback);
		}

		virtual FRHITexture* GetTextureResource() override
		{
			return GetRHIResource();
		}

		/** Returns RHI resource of the allocated pooled resource */
		FRHITexture* GetRHIResource()
		{
			return RenderTarget->GetRHI();
		}
		
	public:
		TRefCountPtr<IPooledRenderTarget> RenderTarget;
		TUniquePtr<FRHIGPUTextureReadback> ReadbackTexture;
	};

	class FBufferCaptureFrame : public FCaptureFrame
	{
	public:
		/** Type alias for the output resource type used during capture frame */
		using FOutputResourceType = FRDGBufferRef;

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

		virtual void* Lock(FRHICommandListImmediate& RHICmdList, FRHIGPUMask GPUMask, int32& OutRowStride) override
		{
			if (ReadbackBuffer->IsReady(GPUMask) == false)
			{
				UE_LOG(LogMediaIOCore, Verbose, TEXT("Fence for buffer readback was not ready, blocking."));
				RHICmdList.BlockUntilGPUIdle();
			}

			OutRowStride = Buffer->GetRHI()->GetStride();
			return ReadbackBuffer->Lock(Buffer->GetRHI()->GetSize());
		}

		virtual void Unlock() override
		{
			ReadbackBuffer->Unlock();
		}

		virtual bool IsReadbackReady(FRHIGPUMask GPUMask) override
		{
			return ReadbackBuffer->IsReady(GPUMask);
		}
		//~ End FCaptureFrame interface

		/** Registers an external texture to be tracked by the graph and returns a pointer to the tracked resource */
		FRDGBufferRef RegisterResource(FRDGBuilder& RDGBuilder)
		{
			return RDGBuilder.RegisterExternalBuffer(Buffer, TEXT("OutputBuffer"));
		}

		/** Adds a readback pass to the graph */
		void EnqueueCopy(FRDGBuilder& RDGBuilder, FRDGBufferRef ResourceToReadback)
		{
			AddEnqueueCopyPass(RDGBuilder, ReadbackBuffer.Get(), ResourceToReadback, Buffer->GetRHI()->GetSize());
		}

		virtual FRHIBuffer* GetBufferResource() override
		{
			return GetRHIResource();
		}

		/** Returns RHI resource of the allocated pooled resource */
		FRHIBuffer* GetRHIResource()
		{
			return Buffer->GetRHI();
		}

	public:
		TRefCountPtr<FRDGPooledBuffer> Buffer;
		TUniquePtr<FRHIGPUBufferReadback> ReadbackBuffer;
	};
	
	class FFrameManager
	{
	public:

		~FFrameManager()
		{
			ENQUEUE_RENDER_COMMAND(MediaCaptureFrameManagerCleaning)(
				[FramesToBeReleased = MoveTemp(CaptureFrames)](FRHICommandListImmediate& RHICmdList) mutable
			{
				FramesToBeReleased.Reset();
			});
		}

		TConstArrayView<TUniquePtr<FCaptureFrame>> GetFrames() const
		{
			return CaptureFrames;
		}

		void AddFrame(TUniquePtr<FCaptureFrame>&& NewFrame)
		{
			AvailableFrames.Enqueue(NewFrame->FrameId);
			CaptureFrames.Add(MoveTemp(NewFrame));
		}

		void MarkAvailable(const FCaptureFrame& InFrame)
		{
			if (ensure(!InFrame.IsPending()))
			{
				AvailableFrames.Enqueue(InFrame.FrameId);
			}
		}

		void MarkPending(const FCaptureFrame& InFrame)
		{
			if (ensure(InFrame.IsPending()))
			{
				PendingFrames.Enqueue(InFrame.FrameId);
			}
		}

		void CompleteNextPending(const FCaptureFrame& InFrame)
		{
			TOptional<int32> NextPending = PendingFrames.Dequeue();
			if (ensure(NextPending.IsSet() && InFrame.FrameId == NextPending.GetValue()))
			{
				MarkAvailable(InFrame);
			}
		}

		FCaptureFrame* GetNextAvailable()
		{
			TOptional<int32> NextAvailable = AvailableFrames.Dequeue();
			if (NextAvailable.IsSet())
			{
				return CaptureFrames[NextAvailable.GetValue()].Get();
			}

			return nullptr;
		}

		FCaptureFrame* PeekNextPending()
		{
			int32* NextPending = PendingFrames.Peek();
			if (NextPending)
			{
				return CaptureFrames[*NextPending].Get();
			}

			return nullptr;
		}

	private:
		TArray<TUniquePtr<FCaptureFrame>> CaptureFrames;
		TSpscQueue<int32> AvailableFrames;
		TSpscQueue<int32> PendingFrames;
	};

	/** Helper struct to contain arguments for CaptureFrame */
	struct FCaptureFrameArgs
	{
		FRDGBuilder& GraphBuilder;
		TObjectPtr<UMediaCapture> MediaCapture = nullptr;
		FTexture2DRHIRef ResourceToCapture;
		FIntPoint DesiredSize = FIntPoint::ZeroValue;
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

		static FTexture2DRHIRef GetSourceTextureForInput(FRHICommandListImmediate& RHICmdList, FSceneViewport* CapturingSceneViewport, FTextureRenderTargetResource* TextureRenderTargetResource)
		{
			FTexture2DRHIRef SourceTexture;
			if (CapturingSceneViewport)
			{
#if WITH_EDITOR
				if (!IsRunningGame())
				{
					// PIE, PIE in windows, editor viewport
					SourceTexture = CapturingSceneViewport->GetRenderTargetTexture();
					if (!SourceTexture.IsValid() && CapturingSceneViewport->GetViewportRHI())
					{
						SourceTexture = RHICmdList.GetViewportBackBuffer(CapturingSceneViewport->GetViewportRHI());
					}
				}
				else
#endif
					if (CapturingSceneViewport->GetViewportRHI())
					{
						// Standalone and packaged
						SourceTexture = RHICmdList.GetViewportBackBuffer(CapturingSceneViewport->GetViewportRHI());
					}
			}
			else if (TextureRenderTargetResource && TextureRenderTargetResource->GetTextureRenderTarget2DResource())
			{
				SourceTexture = TextureRenderTargetResource->GetTextureRenderTarget2DResource()->GetTextureRHI();
			}

			return SourceTexture;
		}

		static bool AreInputsValid(const FCaptureFrameArgs& Args)
		{
			// If it is a simple rgba swizzle we can handle the conversion. Supported formats
			// contained in SupportedRgbaSwizzleFormats. Warning would've been displayed on start of capture.
			if (Args.MediaCapture->DesiredPixelFormat != Args.ResourceToCapture->GetFormat() &&
				(!SupportedRgbaSwizzleFormats.Contains(Args.ResourceToCapture->GetFormat()) || !Args.MediaCapture->DesiredCaptureOptions.bConvertToDesiredPixelFormat))
			{
				UE_LOG(LogMediaIOCore, Error, TEXT("The capture will stop for '%s'. The Source pixel format doesn't match with the user requested pixel format. %sRequested: %s Source: %s")
					, *Args.MediaCapture->MediaOutputName
					, (SupportedRgbaSwizzleFormats.Contains(Args.ResourceToCapture->GetFormat()) && !Args.MediaCapture->DesiredCaptureOptions.bConvertToDesiredPixelFormat) ? TEXT("Please enable \"Convert To Desired Pixel Format\" option in Media Capture settings. ") : TEXT("")
					, GetPixelFormatString(Args.MediaCapture->DesiredPixelFormat)
					, GetPixelFormatString(Args.ResourceToCapture->GetFormat()));

				return false;
			}

			if (Args.MediaCapture->DesiredCaptureOptions.Crop == EMediaCaptureCroppingType::None)
			{
				if (Args.DesiredSize.X != Args.ResourceToCapture->GetSizeX() || Args.DesiredSize.Y != Args.ResourceToCapture->GetSizeY())
				{
					UE_LOG(LogMediaIOCore, Error, TEXT("The capture will stop for '%s'. The Source size doesn't match with the user requested size. Requested: %d,%d  Source: %d,%d")
						, *Args.MediaCapture->MediaOutputName
						, Args.DesiredSize.X, Args.DesiredSize.Y
						, Args.ResourceToCapture->GetSizeX(), Args.ResourceToCapture->GetSizeY());

					return false;
				}
			}
			else
			{
				FIntPoint StartCapturePoint = FIntPoint::ZeroValue;
				if (Args.MediaCapture->DesiredCaptureOptions.Crop == EMediaCaptureCroppingType::Custom)
				{
					StartCapturePoint = Args.MediaCapture->DesiredCaptureOptions.CustomCapturePoint;
				}

				if ((uint32)(Args.DesiredSize.X + StartCapturePoint.X) > Args.ResourceToCapture->GetSizeX() || (uint32)(Args.DesiredSize.Y + StartCapturePoint.Y) > Args.ResourceToCapture->GetSizeY())
				{
					UE_LOG(LogMediaIOCore, Error, TEXT("The capture will stop for '%s'. The Source size doesn't match with the user requested size. Requested: %d,%d  Source: %d,%d")
						, *Args.MediaCapture->MediaOutputName
						, Args.DesiredSize.X, Args.DesiredSize.Y
						, Args.ResourceToCapture->GetSizeX(), Args.ResourceToCapture->GetSizeY());

					return false;
				}
			}

			return true;
		}

		static void GetCopyInfo(const FCaptureFrameArgs& Args, const FTexture2DRHIRef& SourceTexture, FRHICopyTextureInfo& OutCopyInfo, FVector2D& OutSizeU, FVector2D& OutSizeV)
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
					OutCopyInfo.SourcePosition = FIntVector((SourceTexture->GetSizeX() - Args.DesiredSize.X) / 2, (SourceTexture->GetSizeY() - Args.DesiredSize.Y) / 2, 0);
					break;
				case EMediaCaptureCroppingType::TopLeft:
					break;
				case EMediaCaptureCroppingType::Custom:
					OutCopyInfo.SourcePosition = FIntVector(Args.MediaCapture->DesiredCaptureOptions.CustomCapturePoint.X, Args.MediaCapture->DesiredCaptureOptions.CustomCapturePoint.Y, 0);
					break;
				}

				OutSizeU.X = (float)(OutCopyInfo.SourcePosition.X)                      / (float)SourceTexture->GetSizeX();
				OutSizeU.Y = (float)(OutCopyInfo.SourcePosition.X + OutCopyInfo.Size.X) / (float)SourceTexture->GetSizeX();
				OutSizeV.X = (float)(OutCopyInfo.SourcePosition.Y)                      / (float)SourceTexture->GetSizeY();
				OutSizeV.Y = (float)(OutCopyInfo.SourcePosition.Y + OutCopyInfo.Size.Y) / (float)SourceTexture->GetSizeY();
			}
		}

		static void AddConversionPass(const FCaptureFrameArgs& Args, const FConversionPassArgs& ConversionPassArgs, FRDGTextureRef OutputResource)
		{
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
				AddCopyTexturePass(Args.GraphBuilder, ConversionPassArgs.SourceRGBTexture, OutputResource, ConversionPassArgs.CopyInfo);
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
					FScreenPassTextureViewport OutputViewport(OutputResource);
					TShaderMapRef<FRGB8toUYVY8ConvertPS> PixelShader(GlobalShaderMap);
					FRGB8toUYVY8ConvertPS::FParameters* Parameters = PixelShader->AllocateAndSetParameters(Args.GraphBuilder, ConversionPassArgs.SourceRGBTexture, MediaShaders::RgbToYuvRec709Scaled, MediaShaders::YUVOffset8bits, bDoLinearToSRGB, OutputResource);
					AddDrawScreenPass(Args.GraphBuilder, RDG_EVENT_NAME("RGBToUYVY 8 bit"), ViewInfo, OutputViewport, InputViewport, VertexShader, PixelShader, Parameters);
				}
				break;
				case EMediaCaptureConversionOperation::RGB10_TO_YUVv210_10BIT:
				{
					//Configure source/output viewport to get the right UV scaling from source texture to output texture
					const FIntPoint InExtent = FIntPoint((((ConversionPassArgs.SourceRGBTexture->Desc.Extent.X + 47) / 48) * 48), ConversionPassArgs.SourceRGBTexture->Desc.Extent.Y);;
					FScreenPassTextureViewport InputViewport(ConversionPassArgs.SourceRGBTexture, ViewRect);
					FScreenPassTextureViewport OutputViewport(OutputResource);
					TShaderMapRef<FRGB10toYUVv210ConvertPS> PixelShader(GlobalShaderMap);
					FRGB10toYUVv210ConvertPS::FParameters* Parameters = PixelShader->AllocateAndSetParameters(Args.GraphBuilder, ConversionPassArgs.SourceRGBTexture, MediaShaders::RgbToYuvRec709Scaled, MediaShaders::YUVOffset10bits, bDoLinearToSRGB, OutputResource);
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
						FScreenPassTextureViewport OutputViewport(OutputResource);

						// In cases where texture is converted from a format that doesn't have A channel, we want to force set it to 1.
						EMediaCaptureConversionOperation MediaConversionOperation = Args.MediaCapture->DesiredCaptureOptions.bForceAlphaToOneOnConversion ? EMediaCaptureConversionOperation::SET_ALPHA_ONE : Args.MediaCapture->ConversionOperation;
						FModifyAlphaSwizzleRgbaPS::FPermutationDomain PermutationVector;
						PermutationVector.Set<FModifyAlphaSwizzleRgbaPS::FConversionOp>(static_cast<int32>(MediaConversionOperation));

						TShaderMapRef<FModifyAlphaSwizzleRgbaPS> PixelShader(GlobalShaderMap, PermutationVector);
						FModifyAlphaSwizzleRgbaPS::FParameters* Parameters = PixelShader->AllocateAndSetParameters(Args.GraphBuilder, ConversionPassArgs.SourceRGBTexture, OutputResource);
						AddDrawScreenPass(Args.GraphBuilder, RDG_EVENT_NAME("MediaCaptureSwizzle"), ViewInfo, OutputViewport, InputViewport, VertexShader, PixelShader, Parameters);
					}
					break;
				}
			}
		}

		static void AddConversionPass(const FCaptureFrameArgs& Args, const FConversionPassArgs& ConversionPassArgs, FRDGBufferRef OutputResource)
		{
			/** Used at some point to deal with native buffer output resource type but there to validate compiled template */
		}

		template<typename TFrameType>
		static bool CaptureFrame(const FCaptureFrameArgs& Args, TFrameType* CapturingFrame)
		{
			RDG_GPU_STAT_SCOPE(Args.GraphBuilder, MediaCapture_CaptureFrame)

			// Validate if we have a resources used to capture source texture
			if (!CapturingFrame->HasValidResource())
			{
				UE_LOG(LogMediaIOCore, Error, TEXT("The capture will stop for '%s'. A capture frame had an invalid render resource."), *Args.MediaCapture->MediaOutputName);
				return false;
			}

			// If true, we will need to go through our different shader to convert from source format to out format (i.e RGB to YUV)
			const bool bRequiresFormatConversion = Args.MediaCapture->DesiredPixelFormat != Args.ResourceToCapture->GetFormat();

			if (!Args.ResourceToCapture.IsValid())
			{
				UE_LOG(LogMediaIOCore, Error, TEXT("Can't grab the Texture to capture for '%s'."), *Args.MediaCapture->MediaOutputName);
				return false;
			}

			// Validate pixel formats and sizes before pursuing
			if (AreInputsValid(Args) == false)
			{
				return false;
			}

			{
				TRACE_CPUPROFILER_EVENT_SCOPE(UMediaCapture::LockDMATexture_RenderThread);

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
				GetCopyInfo(Args, Args.ResourceToCapture, CopyInfo, SizeU, SizeV);

				// Register the source texture that we want to capture
				const FRDGTextureRef SourceRGBTexture = Args.GraphBuilder.RegisterExternalTexture(CreateRenderTarget(Args.ResourceToCapture, TEXT("SourceTexture")));

				// Register output resource used by the current capture method (texture or buffer)
				typename TFrameType::FOutputResourceType OutputResource = CapturingFrame->RegisterResource(Args.GraphBuilder);
				{
					TRACE_CPUPROFILER_EVENT_SCOPE(UMediaCapture::GraphSetup);
					SCOPED_DRAW_EVENTF(Args.GraphBuilder.RHICmdList, MediaCapture, TEXT("MediaCapture"));

					// If custom conversion was requested from implementation, give it useful information to apply 
					if (Args.MediaCapture->ConversionOperation == EMediaCaptureConversionOperation::CUSTOM)
					{
						RDG_GPU_STAT_SCOPE(Args.GraphBuilder, MediaCapture_CustomCapture)
						TRACE_CPUPROFILER_EVENT_SCOPE(UMediaCapture::CustomCapture);

						Args.MediaCapture->OnCustomCapture_RenderingThread(Args.GraphBuilder, CapturingFrame->CaptureBaseData, CapturingFrame->UserData
							, SourceRGBTexture, OutputResource, CopyInfo, SizeU, SizeV);
					}
					else
					{
						RDG_GPU_STAT_SCOPE(Args.GraphBuilder, MediaCapture_Conversion)
						TRACE_CPUPROFILER_EVENT_SCOPE(UMediaCapture::FormatConversion);
						AddConversionPass(Args, { SourceRGBTexture, bRequiresFormatConversion,  CopyInfo, SizeU, SizeV }, OutputResource);
					}
				}
				
				// If Capture implementation is not grabbing GPU resource directly, push a readback pass to access it from CPU
				if (Args.MediaCapture->bShouldCaptureRHIResource == false)
				{
					RDG_GPU_STAT_SCOPE(Args.GraphBuilder, MediaCapture_Readback)
					TRACE_CPUPROFILER_EVENT_SCOPE(UMediaCapture::EnqueueReadback);

					CapturingFrame->EnqueueCopy(Args.GraphBuilder, OutputResource);

					CapturingFrame->bReadbackRequested = true;
					UE_LOG(LogMediaIOCore, VeryVerbose, TEXT("Requested copy for capture frame %d"), CapturingFrame->FrameId);
				}
				else
				{
					CapturingFrame->bDoingGPUCopy = true;
				}

				Args.MediaCapture->FrameManager->MarkPending(*CapturingFrame);

				++Args.MediaCapture->PendingFrameCount;
			}

			return true;
		}
	};
}


/* UMediaCapture::FCaptureBaseData
*****************************************************************************/
UMediaCapture::FCaptureBaseData::FCaptureBaseData()
	: SourceFrameNumberRenderThread(0)
{

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

UMediaCapture::UMediaCapture(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, ValidSourceGPUMask(FRHIGPUMask::All())
	, bOutputResourcesInitialized(false)
	, bShouldCaptureRHIResource(false)
	, WaitingForRenderCommandExecutionCounter(0)
	, PendingFrameCount(0)
{
}

UMediaCapture::~UMediaCapture() = default;

UMediaCapture::UMediaCapture(FVTableHelper&)
{

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

bool UMediaCapture::CaptureSceneViewport(TSharedPtr<FSceneViewport>& InSceneViewport, FMediaCaptureOptions InCaptureOptions)
{
	StopCapture(false);

	check(IsInGameThread());

	DesiredCaptureOptions = InCaptureOptions;

	if (!ValidateMediaOutput())
	{
		MediaCaptureDetails::ShowSlateNotification();
		return false;
	}

	CacheMediaOutput(EMediaCaptureSourceType::SCENE_VIEWPORT);

	if (bUseRequestedTargetSize)
	{
		DesiredSize = InSceneViewport->GetSize();
	}
	else if (DesiredCaptureOptions.bResizeSourceBuffer)
	{
		SetFixedViewportSize(InSceneViewport);
	}

	CacheOutputOptions();

	const bool bCurrentlyCapturing = false;
	if (!MediaCaptureDetails::ValidateSceneViewport(InSceneViewport, DesiredCaptureOptions, DesiredSize, DesiredPixelFormat, bCurrentlyCapturing))
	{
		ResetFixedViewportSize(InSceneViewport, false);
		MediaCaptureDetails::ShowSlateNotification();
		return false;
	}

	SetState(EMediaCaptureState::Preparing);

	bool bInitialized = InitializeCapture();
	if (bInitialized)
	{
		bInitialized = PostInitializeCaptureViewport(InSceneViewport);
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
		//no lock required, the command on the render thread is not active
		CapturingSceneViewport = InSceneViewport;
		FCoreDelegates::OnEndFrame.AddUObject(this, &UMediaCapture::OnEndFrame_GameThread);
	}
	else
	{
		ResetFixedViewportSize(InSceneViewport, false);
		SetState(EMediaCaptureState::Stopped);
		MediaCaptureDetails::ShowSlateNotification();
	}

#if WITH_EDITOR
	MediaCaptureAnalytics::SendCaptureEvent(GetCaptureSourceType());
#endif
	
	return bInitialized;
}

bool UMediaCapture::CaptureRHITexture(const FRHICaptureResourceDescription& ResourceDescription, FMediaCaptureOptions CaptureOptions)
{
	StopCapture(false);

	check(IsInGameThread());

	DesiredCaptureOptions = CaptureOptions;

	if (!ValidateMediaOutput())
	{
		MediaCaptureDetails::ShowSlateNotification();
		return false;
	}

	CacheMediaOutput(EMediaCaptureSourceType::RHI_RESOURCE);

	// If output is configured to capture resource size directly (not enforced), configure desired size to be the same as resource
	if (bUseRequestedTargetSize)
	{
		DesiredSize = ResourceDescription.ResourceSize;
	}

	CacheOutputOptions();

	// Can't validate resource to capture as it will be given everytime a capture is requested
	SetState(EMediaCaptureState::Preparing);

	bool bInitialized = InitializeCapture();
	if (bInitialized)
	{
		bInitialized = PostInitializeCaptureRHIResource(ResourceDescription);
	}

	// This could have been updated by the call to implementation initialization
	bShouldCaptureRHIResource = ShouldCaptureRHIResource();

	if (bInitialized)
	{
		InitializeOutputResources(MediaOutput->NumberOfTextureBuffers);
		bInitialized = GetState() != EMediaCaptureState::Stopped;
	}

	if (bInitialized)
	{
		// Used to queue capture data production from the game thread to be consumed when frame capture is requested on render thread
		FCoreDelegates::OnBeginFrame.AddUObject(this, &UMediaCapture::OnBeginFrame_GameThread);
	}
	else
	{
		SetState(EMediaCaptureState::Stopped);
		MediaCaptureDetails::ShowSlateNotification();
	}

#if WITH_EDITOR
	MediaCaptureAnalytics::SendCaptureEvent(GetCaptureSourceType());
#endif

	return bInitialized;
}

bool UMediaCapture::CaptureTextureRenderTarget2D(UTextureRenderTarget2D* InRenderTarget2D, FMediaCaptureOptions CaptureOptions)
{
	StopCapture(false);

	check(IsInGameThread());

	DesiredCaptureOptions = CaptureOptions; 
	
	if (!ValidateMediaOutput())
	{
		MediaCaptureDetails::ShowSlateNotification();
		return false;
	}

	CacheMediaOutput(EMediaCaptureSourceType::RENDER_TARGET);

	if (bUseRequestedTargetSize)
	{
		DesiredSize = FIntPoint(InRenderTarget2D->SizeX, InRenderTarget2D->SizeY);
	}
	else if (DesiredCaptureOptions.bResizeSourceBuffer)
	{
		InRenderTarget2D->ResizeTarget(DesiredSize.X, DesiredSize.Y);
	}

	CacheOutputOptions();

	const bool bCurrentlyCapturing = false;
	if (!MediaCaptureDetails::ValidateTextureRenderTarget2D(InRenderTarget2D, DesiredCaptureOptions, DesiredSize, DesiredPixelFormat, bCurrentlyCapturing))
	{
		MediaCaptureDetails::ShowSlateNotification();
		return false;
	}

	SetState(EMediaCaptureState::Preparing);

	bool bInitialized = InitializeCapture();
	if (bInitialized)
	{
		bInitialized = PostInitializeCaptureRenderTarget(InRenderTarget2D);
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
		CapturingRenderTarget = InRenderTarget2D;

		FCoreDelegates::OnEndFrame.AddUObject(this, &UMediaCapture::OnEndFrame_GameThread);
	}
	else
	{
		SetState(EMediaCaptureState::Stopped);
		MediaCaptureDetails::ShowSlateNotification();
	}

#if WITH_EDITOR
	MediaCaptureAnalytics::SendCaptureEvent(GetCaptureSourceType());
#endif
	
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

bool UMediaCapture::UpdateSceneViewport(TSharedPtr<FSceneViewport>& InSceneViewport)
{
	if (!MediaCaptureDetails::ValidateIsCapturing(*this))
	{
		StopCapture(false);
		return false;
	}

	check(IsInGameThread());

	if (!bUseRequestedTargetSize && DesiredCaptureOptions.bResizeSourceBuffer)
	{
		SetFixedViewportSize(InSceneViewport);
	}

	const bool bCurrentlyCapturing = true;
	if (!MediaCaptureDetails::ValidateSceneViewport(InSceneViewport, DesiredCaptureOptions, DesiredSize, DesiredPixelFormat, bCurrentlyCapturing))
	{
		ResetFixedViewportSize(InSceneViewport, false);
		StopCapture(false);
		MediaCaptureDetails::ShowSlateNotification();
		return false;
	}

	if (!UpdateSceneViewportImpl(InSceneViewport))
	{
		ResetFixedViewportSize(InSceneViewport, false);
		StopCapture(false);
		MediaCaptureDetails::ShowSlateNotification();
		return false;
	}

	{
		FScopeLock Lock(&AccessingCapturingSource);
		while(WaitingForRenderCommandExecutionCounter.load() > 0)
		{
			FlushRenderingCommands();
		}
		ResetFixedViewportSize(CapturingSceneViewport.Pin(), true);
		CapturingSceneViewport = InSceneViewport;
		CapturingRenderTarget = nullptr;
	}

	return true;
}

bool UMediaCapture::UpdateTextureRenderTarget2D(UTextureRenderTarget2D * InRenderTarget2D)
{
	if (!MediaCaptureDetails::ValidateIsCapturing(*this))
	{
		StopCapture(false);
		MediaCaptureDetails::ShowSlateNotification();
		return false;
	}

	check(IsInGameThread());

	if (!bUseRequestedTargetSize && DesiredCaptureOptions.bResizeSourceBuffer)
	{
		InRenderTarget2D->ResizeTarget(DesiredSize.X, DesiredSize.Y);
	}

	const bool bCurrentlyCapturing = true;
	if (!MediaCaptureDetails::ValidateTextureRenderTarget2D(InRenderTarget2D, DesiredCaptureOptions, DesiredSize, DesiredPixelFormat, bCurrentlyCapturing))
	{
		StopCapture(false);
		MediaCaptureDetails::ShowSlateNotification();
		return false;
	}

	if (!UpdateRenderTargetImpl(InRenderTarget2D))
	{
		StopCapture(false);
		MediaCaptureDetails::ShowSlateNotification();
		return false;
	}

	{
		FScopeLock Lock(&AccessingCapturingSource);
		while (WaitingForRenderCommandExecutionCounter.load() > 0)
		{
			FlushRenderingCommands();
		}
		ResetFixedViewportSize(CapturingSceneViewport.Pin(), true);
		CapturingRenderTarget = InRenderTarget2D;
		CapturingSceneViewport.Reset();
	}

	return true;
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
				while (WaitingForRenderCommandExecutionCounter.load() > 0)
				{
					FlushRenderingCommands();
				}
			}
		}
	}
	else
	{
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
			ResetFixedViewportSize(CapturingSceneViewport.Pin(), false);

			CapturingRenderTarget = nullptr;
			CapturingSceneViewport.Reset();
			DesiredSize = FIntPoint(1280, 720);
			DesiredPixelFormat = EPixelFormat::PF_A2B10G10R10;
			DesiredOutputSize = FIntPoint(1280, 720);
			DesiredOutputPixelFormat = EPixelFormat::PF_A2B10G10R10;
			DesiredCaptureOptions = FMediaCaptureOptions();
			ConversionOperation = EMediaCaptureConversionOperation::NONE;
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

void UMediaCapture::CaptureImmediate_RenderThread(FRDGBuilder& GraphBuilder, FRHITexture* InSourceTexture)
{
	using namespace UE::MediaCaptureData;
	TRACE_CPUPROFILER_EVENT_SCOPE(UMediaCapture::CaptureImmediate_RenderThread);
	
	check(IsInRenderingThread());

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

	if (CaptureSourceType != EMediaCaptureSourceType::RHI_RESOURCE)
	{
		UE_LOG(LogMediaIOCore, Error, TEXT("[%s] - Trying to capture a RHI resource with another capture type."), *MediaOutputName);
		SetState(EMediaCaptureState::Error);
	}

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
	while (CaptureDataQueue.Dequeue(NextCaptureData))
	{
		if (NextCaptureData.BaseData.SourceFrameNumberRenderThread == GFrameCounterRenderThread)
		{
			bFoundMatchingData = true;
			break;
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
	FCaptureFrame* CapturingFrame = nullptr;
	if (GetState() != EMediaCaptureState::StopRequested)
	{
		CapturingFrame = FrameManager->GetNextAvailable();
	}

	if (CapturingFrame == nullptr && GetState() != EMediaCaptureState::StopRequested)
	{
		if (DesiredCaptureOptions.OverrunAction == EMediaCaptureOverrunAction::Flush)
		{
			UE_LOG(LogMediaIOCore, Error, TEXT("[%s] - No frames available for capture. This should not happen."), *MediaOutputName);
			SetState(EMediaCaptureState::Error);
			return;
		}
		else
		{
			//In case we are skipping frames, just keep capture frame as invalid
			UE_LOG(LogMediaIOCore, Verbose, TEXT("[%s] - No frames available for capture. Skipping"), *MediaOutputName);
		}
	}

	// Peek next pending before capturing to avoid getting the newly captured one. 
	// It shouldn't happen but if it happens it will start waiting for the capture to complete
	FCaptureFrame* NextPending = FrameManager->PeekNextPending();
	if (NextPending && CapturingFrame)
	{
		ensure(NextPending->FrameId != CapturingFrame->FrameId);
	}

	PrintFrameState();

	UE_LOG(LogMediaIOCore, Verbose, TEXT("[%s] - Capturing frame %d"), *MediaOutputName, CapturingFrame ? CapturingFrame->FrameId : -1);
	if (CapturingFrame)
	{
		// Prepare frame to capture

		// Use queued capture base data for this frame to capture
		CapturingFrame->CaptureBaseData = MoveTemp(NextCaptureData.BaseData);
		CapturingFrame->CaptureBaseData.SourceFrameNumber = ++CaptureRequestCount;
		CapturingFrame->UserData = MoveTemp(NextCaptureData.UserData);

		ProcessCapture_RenderThread(GraphBuilder, this, CapturingFrame, InSourceTexture, DesiredSize);
	}

	UE_LOG(LogMediaIOCore, Verbose, TEXT("[%s] - Processing pending frame %d"), *MediaOutputName, NextPending ? NextPending->FrameId : -1);
	if(NextPending)
	{
		ProcessReadyFrame_RenderThread(GraphBuilder.RHICmdList, this, NextPending);
	}
}

const FString& UMediaCapture::GetCaptureSourceType() const
{
	switch (CaptureSourceType)
	{
		case EMediaCaptureSourceType::SCENE_VIEWPORT:
		{
			static FString Viewport = TEXT("SceneViewport");
			return Viewport;
		}
		case EMediaCaptureSourceType::RENDER_TARGET:
		{
			static FString RenderTarget = TEXT("RenderTarget");
			return RenderTarget;
		}
		case EMediaCaptureSourceType::RHI_RESOURCE:
		{
			static FString RHIResource = TEXT("RHI");
			return RHIResource;
		}
		default:
		{
			static FString Invalid = TEXT("Invalid");
			return Invalid;
		}
	}
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

void UMediaCapture::BroadcastStateChanged()
{
	OnStateChanged.Broadcast();
	OnStateChangedNative.Broadcast();
}

void UMediaCapture::SetFixedViewportSize(TSharedPtr<FSceneViewport> InSceneViewport)
{
	InSceneViewport->SetFixedViewportSize(DesiredSize.X, DesiredSize.Y);
	bViewportHasFixedViewportSize = true;
}

void UMediaCapture::ResetFixedViewportSize(TSharedPtr<FSceneViewport> InViewport, bool bInFlushRenderingCommands)
{
	if (bViewportHasFixedViewportSize && InViewport.IsValid())
	{
		if (bInFlushRenderingCommands && WaitingForRenderCommandExecutionCounter.load() > 0)
		{
			FlushRenderingCommands();
		}
		InViewport->SetFixedViewportSize(0, 0);
		bViewportHasFixedViewportSize = false;
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
	FrameManager = MakeUnique<UE::MediaCaptureData::FFrameManager>();

	bOutputResourcesInitialized = false;
	NumberOfCaptureFrame = InNumberOfBuffers;

	check(NumberOfCaptureFrame >= 2);

	UMediaCapture* This = this;
	ENQUEUE_RENDER_COMMAND(MediaOutputCaptureFrameCreateResources)(
	[This](FRHICommandListImmediate& RHICmdList)
		{
			for (int32 Index = 0; Index < This->NumberOfCaptureFrame; ++Index)
			{
				if (This->DesiredOutputResourceType == EMediaCaptureResourceType::Texture)
				{
					TUniquePtr<FTextureCaptureFrame> NewFrame = MakeUnique<FTextureCaptureFrame>(Index);
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
					TUniquePtr<FBufferCaptureFrame> NewFrame = MakeUnique<FBufferCaptureFrame>(Index);

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

	if (CaptureSourceType == EMediaCaptureSourceType::RHI_RESOURCE && (GetState() == EMediaCaptureState::Capturing))
	{
		// Queue capture data to be consumed when capture requests are done on render thread
		FQueuedCaptureData CaptureData;
		CaptureData.BaseData.SourceFrameTimecode = FApp::GetTimecode();
		CaptureData.BaseData.SourceFrameTimecodeFramerate = FApp::GetTimecodeFrameRate();
		CaptureData.BaseData.SourceFrameNumberRenderThread = GFrameCounter;
		CaptureData.UserData = GetCaptureFrameUserData_GameThread();
		CaptureDataQueue.Enqueue(MoveTemp(CaptureData));
	}
}

void UMediaCapture::OnEndFrame_GameThread()
{
	using namespace UE::MediaCaptureData;

	TRACE_CPUPROFILER_EVENT_SCOPE(MediaCapture::OnEndFrame_GameThread);

	if (!bOutputResourcesInitialized)
	{
		FlushRenderingCommands();
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

	FCaptureFrame* CapturingFrame = nullptr;
	if(GetState() != EMediaCaptureState::StopRequested)
	{
		CapturingFrame = FrameManager->GetNextAvailable();
	}

	if (CapturingFrame == nullptr && GetState() != EMediaCaptureState::StopRequested)
	{
		if (DesiredCaptureOptions.OverrunAction == EMediaCaptureOverrunAction::Flush)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(MediaCapture::FlushRenderingCommands);
			UE_LOG(LogMediaIOCore, VeryVerbose, TEXT("[%s] - Flushing commands."), *MediaOutputName);
			FlushRenderingCommands();

			// After flushing, we should have access to an available frame, if not, it's not expected.
			CapturingFrame = FrameManager->GetNextAvailable();
			if (CapturingFrame == nullptr)
			{
				UE_LOG(LogMediaIOCore, Error, TEXT("[%s] - Flushing commands didn't give us back available frames to process.") , *MediaOutputName);
				StopCapture(false);
				return;
			}
		}
		else
		{
			// Selected options is to skip capturing a frame if overrun happens
			UE_LOG(LogMediaIOCore, Verbose, TEXT("[%s] - No frames available for capture. Skipping"), *MediaOutputName);
		}
	}

	if (CapturingFrame)
	{
		CapturingFrame->CaptureBaseData.SourceFrameTimecode = FApp::GetTimecode();
		CapturingFrame->CaptureBaseData.SourceFrameTimecodeFramerate = FApp::GetTimecodeFrameRate();
		CapturingFrame->CaptureBaseData.SourceFrameNumberRenderThread = GFrameCounter;
		CapturingFrame->CaptureBaseData.SourceFrameNumber = ++CaptureRequestCount;
		CapturingFrame->UserData = GetCaptureFrameUserData_GameThread();
	}

	PrintFrameState();
	
	// Init variables for ENQUEUE_RENDER_COMMAND.
	//The Lock only synchronize while we are copying the value to the enqueue. The viewport and the rendertarget may change while we are in the enqueue command.
	{
		FScopeLock Lock(&AccessingCapturingSource);

		TSharedPtr<FSceneViewport> CapturingSceneViewportPin = CapturingSceneViewport.Pin();
		FSceneViewport* InCapturingSceneViewport = CapturingSceneViewportPin.Get();
		FTextureRenderTargetResource* InTextureRenderTargetResource = CapturingRenderTarget ? CapturingRenderTarget->GameThread_GetRenderTargetResource() : nullptr;
		FIntPoint InDesiredSize = DesiredSize;
		UMediaCapture* InMediaCapture = this;


		if (InCapturingSceneViewport != nullptr || InTextureRenderTargetResource != nullptr)
		{
			++WaitingForRenderCommandExecutionCounter;

			const FRHIGPUMask SourceGPUMask = ValidSourceGPUMask;

			UE_LOG(LogMediaIOCore, Verbose, TEXT("[%s] - Queuing frame to capture %d"), *InMediaCapture->MediaOutputName, CapturingFrame ? CapturingFrame->FrameId : -1);

			// RenderCommand to be executed on the RenderThread
			ENQUEUE_RENDER_COMMAND(FMediaOutputCaptureFrameCreateTexture)(
				[InMediaCapture, CapturingFrame, InCapturingSceneViewport, InTextureRenderTargetResource, InDesiredSize, SourceGPUMask](FRHICommandListImmediate& RHICmdList)
			{
				SCOPED_GPU_MASK(RHICmdList, SourceGPUMask);

				// Peek next pending before capturing to avoid getting the newly captured one. 
				// It shouldn't happen but if it happens it will start waiting for the capture to complete
				FCaptureFrame* NextPending = InMediaCapture->FrameManager->PeekNextPending();
				if (NextPending && CapturingFrame)
				{
					ensure(NextPending->FrameId != CapturingFrame->FrameId);
				}

				// Capture frame
				{
					FRDGBuilder GraphBuilder(RHICmdList);

					FTexture2DRHIRef SourceTexture = FMediaCaptureHelper::GetSourceTextureForInput(RHICmdList, InCapturingSceneViewport, InTextureRenderTargetResource);
					InMediaCapture->ProcessCapture_RenderThread(GraphBuilder, InMediaCapture, CapturingFrame, MoveTemp(SourceTexture), InDesiredSize);
					UE_LOG(LogMediaIOCore, Verbose, TEXT("[%s] - Captured frame %d"), *InMediaCapture->MediaOutputName, CapturingFrame ? CapturingFrame->FrameId : -1);
					
					GraphBuilder.Execute();
				}
				
				//Process pending frame
				UE_LOG(LogMediaIOCore, Verbose, TEXT("[%s] - Processing pending frame %d"), *InMediaCapture->MediaOutputName, NextPending ? NextPending->FrameId : -1);
				if (NextPending)
				{
					InMediaCapture->ProcessReadyFrame_RenderThread(RHICmdList, InMediaCapture, NextPending);
				}
				

				// Whatever happens, we want to decrement our counter to track enqueued commands
				--InMediaCapture->WaitingForRenderCommandExecutionCounter;
			});

			//If auto-stopping, count the number of frame captures requested and stop when reaching 0.
			if(DesiredCaptureOptions.bAutostopOnCapture && GetState() == EMediaCaptureState::Capturing && --DesiredCaptureOptions.NumberOfFramesToCapture <= 0)
			{
				StopCapture(true);
			}
		}
	}
}

bool UMediaCapture::ProcessCapture_RenderThread(FRDGBuilder& GraphBuilder, UMediaCapture* InMediaCapture, UE::MediaCaptureData::FCaptureFrame* CapturingFrame, FTexture2DRHIRef InResourceToCapture,	FIntPoint InDesiredSize)
{
	using namespace UE::MediaCaptureData;
	
	RDG_GPU_STAT_SCOPE(GraphBuilder, MediaCapture_ProcessCapture)
	TRACE_CPUPROFILER_EVENT_SCOPE(UMediaCapture::ProcessCapture_RenderThread);

	if (CapturingFrame)
	{
		// Call the capture frame algo based on the specific type of resource we are using
		bool bHasCaptureSuceeded = false;
		FCaptureFrameArgs CaptureArgs = { GraphBuilder, InMediaCapture, MoveTemp(InResourceToCapture), InDesiredSize};
		if (DesiredOutputResourceType == EMediaCaptureResourceType::Texture)
		{
			if (ensure(CapturingFrame->IsTextureResource()))
			{
				FTextureCaptureFrame* TextureFrame = static_cast<FTextureCaptureFrame*>(CapturingFrame);
				InMediaCapture->BeforeFrameCaptured_RenderingThread(CapturingFrame->CaptureBaseData, CapturingFrame->UserData, TextureFrame->GetTextureResource());
				bHasCaptureSuceeded = FMediaCaptureHelper::CaptureFrame(CaptureArgs, TextureFrame);
			}
			else
			{
				UE_LOG(LogMediaIOCore, Error, TEXT("The capture will stop for '%s'. Capture frame was expected to use Texture resource but wasn't."), *InMediaCapture->MediaOutputName);
			}
		}
		else
		{
			if (ensure(CapturingFrame->IsBufferResource()))
			{
				FBufferCaptureFrame* BufferFrame = static_cast<FBufferCaptureFrame*>(CapturingFrame);
				InMediaCapture->BeforeFrameCaptured_RenderingThread(CapturingFrame->CaptureBaseData, CapturingFrame->UserData, BufferFrame->GetBufferResource());
				bHasCaptureSuceeded = FMediaCaptureHelper::CaptureFrame(CaptureArgs, BufferFrame);
			}
			else
			{
				UE_LOG(LogMediaIOCore, Error, TEXT("The capture will stop for '%s'. Capture frame was expected to use Buffer resource but wasn't."), *InMediaCapture->MediaOutputName);
			}
		}
		
		if (bHasCaptureSuceeded == false)
		{
			InMediaCapture->SetState(EMediaCaptureState::Error);
		}

		return bHasCaptureSuceeded;
	}

	return false;
}

bool UMediaCapture::ProcessReadyFrame_RenderThread(FRHICommandListImmediate& RHICmdList, UMediaCapture* InMediaCapture, UE::MediaCaptureData::FCaptureFrame* ReadyFrame)
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
				ColorDataBuffer = ReadyFrame->Lock(RHICmdList, GPUMask, RowStride);
			}
			else
			{
				UE_LOG(LogMediaIOCore, VeryVerbose, TEXT("[%s] - Readback %d not ready. Skipping."), *InMediaCapture->MediaOutputName, ReadyFrame->FrameId);
				bWasFrameProcessed = false;
			}

			if(ColorDataBuffer)
			{
				{
					SCOPE_CYCLE_COUNTER(STAT_MediaCapture_RenderThread_CaptureCallback)

					// The Width/Height of the surface may be different then the DesiredOutputSize : Some underlying implementations enforce a specific stride, therefore
					// there may be padding at the end of each row.
					InMediaCapture->OnFrameCaptured_RenderingThread(ReadyFrame->CaptureBaseData, ReadyFrame->UserData, ColorDataBuffer, InMediaCapture->DesiredOutputSize.X, InMediaCapture->DesiredOutputSize.Y, RowStride);
				}

				ReadyFrame->Unlock();
			}

			if (bIsReadbackReady)
			{
				UE_LOG(LogMediaIOCore, Verbose, TEXT("[%s] - Completed pending frame %d."), *InMediaCapture->MediaOutputName, ReadyFrame->FrameId);
				ReadyFrame->bReadbackRequested = false;
				--PendingFrameCount;
				InMediaCapture->FrameManager->CompleteNextPending(*ReadyFrame);
			}
		}
		else if (InMediaCapture->bShouldCaptureRHIResource && ReadyFrame->bDoingGPUCopy)
		{
			if (ReadyFrame->IsTextureResource())
			{
				FTextureCaptureFrame* TextureFrame = static_cast<FTextureCaptureFrame*>(ReadyFrame);
				{
					TRACE_CPUPROFILER_EVENT_SCOPE(UMediaCapture::UnlockDMATexture_RenderThread);
					UnlockDMATexture_RenderThread(TextureFrame->RenderTarget->GetRHI());
				}

				TRACE_CPUPROFILER_EVENT_SCOPE(UMediaCapture::RHIResourceCaptured);
				SCOPE_CYCLE_COUNTER(STAT_MediaCapture_RenderThread_CaptureCallback)
					InMediaCapture->OnRHIResourceCaptured_RenderingThread(ReadyFrame->CaptureBaseData, ReadyFrame->UserData, TextureFrame->GetRHIResource());
			}
			else
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(UMediaCapture::RHIResourceCaptured);
				FBufferCaptureFrame* BufferFrame = static_cast<FBufferCaptureFrame*>(ReadyFrame);
				InMediaCapture->OnRHIResourceCaptured_RenderingThread(ReadyFrame->CaptureBaseData, ReadyFrame->UserData, BufferFrame->GetRHIResource());
			}

			UE_LOG(LogMediaIOCore, Verbose, TEXT("[%s] - Completed pending frame %d."), *InMediaCapture->MediaOutputName, ReadyFrame->FrameId);
			ReadyFrame->bDoingGPUCopy = false;
			InMediaCapture->FrameManager->CompleteNextPending(*ReadyFrame);
			--PendingFrameCount;
		}
	}

	return bWasFrameProcessed;
}

void UMediaCapture::PrintFrameState()
{
	using namespace UE::MediaCaptureData;

	TStringBuilder<256> ReadbackInfoBuilder;
	ReadbackInfoBuilder << "\n";
	TConstArrayView<TUniquePtr<FCaptureFrame>> Frames = FrameManager->GetFrames();
	for (int32 Index = 0; Index < Frames.Num(); Index++)
	{
		ReadbackInfoBuilder << FString::Format(TEXT("Frame {0} readback requested: {1}\n"), { Index, Frames[Index]->IsPending() ? TEXT("true") : TEXT("false") });
	}
	UE_LOG(LogMediaIOCore, VeryVerbose, TEXT("%s"), ReadbackInfoBuilder.GetData());
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

	bool ValidateSize(const FIntPoint TargetSize, const FIntPoint& DesiredSize, const FMediaCaptureOptions& CaptureOptions, const bool bCurrentlyCapturing)
	{
		if (CaptureOptions.Crop == EMediaCaptureCroppingType::None)
		{
			if (DesiredSize.X != TargetSize.X || DesiredSize.Y != TargetSize.Y)
			{
				UE_LOG(LogMediaIOCore, Error, TEXT("Can not %s the capture. The Render Target size doesn't match with the requested size. SceneViewport: %d,%d  MediaOutput: %d,%d")
					, bCurrentlyCapturing ? TEXT("continue") : TEXT("start")
					, TargetSize.X, TargetSize.Y
					, DesiredSize.X, DesiredSize.Y);
				return false;
			}
		}
		else
		{
			FIntPoint StartCapturePoint = FIntPoint::ZeroValue;
			if (CaptureOptions.Crop == EMediaCaptureCroppingType::Custom)
			{
				if (CaptureOptions.CustomCapturePoint.X < 0 || CaptureOptions.CustomCapturePoint.Y < 0)
				{
					UE_LOG(LogMediaIOCore, Error, TEXT("Can not %s the capture. The start capture point is negatif. Start Point: %d,%d")
						, bCurrentlyCapturing ? TEXT("continue") : TEXT("start")
						, StartCapturePoint.X, StartCapturePoint.Y);
					return false;
				}
				StartCapturePoint = CaptureOptions.CustomCapturePoint;
			}

			if (DesiredSize.X + StartCapturePoint.X > TargetSize.X || DesiredSize.Y + StartCapturePoint.Y > TargetSize.Y)
			{
				UE_LOG(LogMediaIOCore, Error, TEXT("Can not %s the capture. The Render Target size is too small for the requested cropping options. SceneViewport: %d,%d  MediaOutput: %d,%d Start Point: %d,%d")
					, bCurrentlyCapturing ? TEXT("continue") : TEXT("start")
					, TargetSize.X, TargetSize.Y
					, DesiredSize.X, DesiredSize.Y
					, StartCapturePoint.X, StartCapturePoint.Y);
				return false;
			}
		}

		return true;
	}

	bool ValidateSceneViewport(const TSharedPtr<FSceneViewport>& SceneViewport, const FMediaCaptureOptions& CaptureOptions, const FIntPoint& DesiredSize, const EPixelFormat DesiredPixelFormat, const bool bCurrentlyCapturing)
	{
		if (!SceneViewport.IsValid())
		{
			UE_LOG(LogMediaIOCore, Error, TEXT("Can not %s the capture. The Scene Viewport is invalid.")
				, bCurrentlyCapturing ? TEXT("continue") : TEXT("start"));
			return false;
		}

		const FIntPoint SceneViewportSize = SceneViewport->GetRenderTargetTextureSizeXY();
		if (!ValidateSize(SceneViewportSize, DesiredSize, CaptureOptions, bCurrentlyCapturing))
		{
			return false;
		}

		static const auto CVarDefaultBackBufferPixelFormat = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.DefaultBackBufferPixelFormat"));
		EPixelFormat SceneTargetFormat = EDefaultBackBufferPixelFormat::Convert2PixelFormat(EDefaultBackBufferPixelFormat::FromInt(CVarDefaultBackBufferPixelFormat->GetValueOnGameThread()));
		if (DesiredPixelFormat != SceneTargetFormat)
		{
			if (!SupportedRgbaSwizzleFormats.Contains(SceneTargetFormat) || !CaptureOptions.bConvertToDesiredPixelFormat)
			{
				UE_LOG(LogMediaIOCore, Error, TEXT("Can not %s the capture. The Render Target pixel format doesn't match with the requested pixel format. %sRenderTarget: %s MediaOutput: %s")
					, bCurrentlyCapturing ? TEXT("continue") : TEXT("start")
					, (SupportedRgbaSwizzleFormats.Contains(SceneTargetFormat) && !CaptureOptions.bConvertToDesiredPixelFormat) ? TEXT("Please enable \"Convert To Desired Pixel Format\" option in Media Capture settings") : TEXT("")
					, GetPixelFormatString(SceneTargetFormat)
					, GetPixelFormatString(DesiredPixelFormat));
				return false;
			}
			else
			{
				UE_LOG(LogMediaIOCore, Warning, TEXT("The Render Target pixel format doesn't match with the requested pixel format. Render target will be automatically converted. This could have a slight performance impact. RenderTarget: %s MediaOutput: %s")
					, GetPixelFormatString(SceneTargetFormat)
					, GetPixelFormatString(DesiredPixelFormat));
			}
		}

		return true;
	}

	bool ValidateTextureRenderTarget2D(const UTextureRenderTarget2D* InRenderTarget2D, const FMediaCaptureOptions& CaptureOptions, const FIntPoint& DesiredSize, const EPixelFormat DesiredPixelFormat, const bool bCurrentlyCapturing)
	{
		if (InRenderTarget2D == nullptr)
		{
			UE_LOG(LogMediaIOCore, Error, TEXT("Couldn't %s the capture. The Render Target is invalid.")
				, bCurrentlyCapturing ? TEXT("continue") : TEXT("start"));
			return false;
		}

		if (!ValidateSize(FIntPoint(InRenderTarget2D->SizeX, InRenderTarget2D->SizeY), DesiredSize, CaptureOptions, bCurrentlyCapturing))
		{
			return false;
		}

		if (DesiredPixelFormat != InRenderTarget2D->GetFormat())
		{
			if (!SupportedRgbaSwizzleFormats.Contains(InRenderTarget2D->GetFormat()) || !CaptureOptions.bConvertToDesiredPixelFormat)
			{
				UE_LOG(LogMediaIOCore, Error, TEXT("Can not %s the capture. The Render Target pixel format doesn't match with the requested pixel format. %sRenderTarget: %s MediaOutput: %s")
					, bCurrentlyCapturing ? TEXT("continue") : TEXT("start")
					, (SupportedRgbaSwizzleFormats.Contains(InRenderTarget2D->GetFormat()) && !CaptureOptions.bConvertToDesiredPixelFormat) ? TEXT("Please enable \"Convert To Desired Pixel Format\" option in Media Capture settings. ") : TEXT("")
					, GetPixelFormatString(InRenderTarget2D->GetFormat())
					, GetPixelFormatString(DesiredPixelFormat));
				return false;
			}
			else
			{
				UE_LOG(LogMediaIOCore, Warning, TEXT("The Render Target pixel format doesn't match with the requested pixel format. Render target will be automatically converted. This could have a slight performance impact. RenderTarget: %s MediaOutput: %s")
					, GetPixelFormatString(InRenderTarget2D->GetFormat())
					, GetPixelFormatString(DesiredPixelFormat));
			}
		}

		return true;
	}

	bool ValidateIsCapturing(const UMediaCapture& CaptureToBeValidated)
	{
		if (CaptureToBeValidated.GetState() != EMediaCaptureState::Capturing && CaptureToBeValidated.GetState() != EMediaCaptureState::Preparing)
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

