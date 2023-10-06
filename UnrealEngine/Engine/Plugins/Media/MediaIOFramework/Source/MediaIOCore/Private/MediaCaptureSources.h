// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/RendererSettings.h"
#include "Math/IntPoint.h"
#include "MediaCapture.h"
#include "MediaIOCoreModule.h"
#include "MediaOutput.h"
#include "Slate/SceneViewport.h"
#include "TextureResource.h"
#include "UObject/WeakObjectPtr.h"

namespace UE::MediaCapture::Private
{
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
		if (CaptureOptions.ResizeMethod != EMediaCaptureResizeMethod::ResizeInRenderPass && !ValidateSize(SceneViewportSize, DesiredSize, CaptureOptions, bCurrentlyCapturing))
		{
			return false;
		}

		static const auto CVarDefaultBackBufferPixelFormat = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.DefaultBackBufferPixelFormat"));
		EPixelFormat SceneTargetFormat = EDefaultBackBufferPixelFormat::Convert2PixelFormat(EDefaultBackBufferPixelFormat::FromInt(CVarDefaultBackBufferPixelFormat->GetValueOnGameThread()));
		if (DesiredPixelFormat != SceneTargetFormat)
		{
			if (!UMediaCapture::GetSupportedRgbaSwizzleFormats().Contains(SceneTargetFormat) || !CaptureOptions.bConvertToDesiredPixelFormat)
			{
				UE_LOG(LogMediaIOCore, Error, TEXT("Can not %s the capture. The Render Target pixel format doesn't match with the requested pixel format. %sRenderTarget: %s MediaOutput: %s")
					, bCurrentlyCapturing ? TEXT("continue") : TEXT("start")
					, (UMediaCapture::GetSupportedRgbaSwizzleFormats().Contains(SceneTargetFormat) && !CaptureOptions.bConvertToDesiredPixelFormat) ? TEXT("Please enable \"Convert To Desired Pixel Format\" option in Media Capture settings") : TEXT("")
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

		if (CaptureOptions.ResizeMethod != EMediaCaptureResizeMethod::ResizeInRenderPass && !ValidateSize(FIntPoint(InRenderTarget2D->SizeX, InRenderTarget2D->SizeY), DesiredSize, CaptureOptions, bCurrentlyCapturing))
		{
			return false;
		}

		if (DesiredPixelFormat != InRenderTarget2D->GetFormat())
		{
			if (!UMediaCapture::GetSupportedRgbaSwizzleFormats().Contains(InRenderTarget2D->GetFormat()) || !CaptureOptions.bConvertToDesiredPixelFormat)
			{
				UE_LOG(LogMediaIOCore, Error, TEXT("Can not %s the capture. The Render Target pixel format doesn't match with the requested pixel format. %sRenderTarget: %s MediaOutput: %s")
					, bCurrentlyCapturing ? TEXT("continue") : TEXT("start")
					, (UMediaCapture::GetSupportedRgbaSwizzleFormats().Contains(InRenderTarget2D->GetFormat()) && !CaptureOptions.bConvertToDesiredPixelFormat) ? TEXT("Please enable \"Convert To Desired Pixel Format\" option in Media Capture settings. ") : TEXT("")
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

	enum class ECaptureType : uint8
	{
		Immediate,
		OnTick
	};
	
	class FCaptureSource
	{
	public:
		FCaptureSource(UMediaCapture* InMediaCapture, FMediaCaptureOptions InCaptureOptions)
			: MediaCapture(InMediaCapture)
			, CaptureOptions(MoveTemp(InCaptureOptions))
		{
		}

		virtual ~FCaptureSource() = default;
		
		virtual void ResizeSourceBuffer(FIntPoint Size) {}
		virtual void ResetSourceBufferSize(bool bFlushRenderingCommands) {}
		virtual bool ValidateSource(const FIntPoint& DesiredSize, const EPixelFormat DesiredPixelFormat, const bool bCurrentlyCapturing) = 0;
		virtual bool PostInitialize() = 0;
		virtual EMediaCaptureSourceType GetSourceType() = 0;
		virtual FIntPoint GetSize() = 0;
		
		virtual ECaptureType GetCaptureType() const
		{
			return ECaptureType::OnTick;
		}
		
		virtual bool UpdateSourceImpl()
		{
			return true;
		}
		
		/** Only returns a valid texture for scene viewports and render targets as the RHI Sources don't keep a pointer to the RHI texture. */
		virtual FTexture2DRHIRef GetSourceTextureForInput_RenderThread(FRHICommandListImmediate& RHICmdList)
		{
			return nullptr;
		}

		virtual UTextureRenderTarget2D* GetRenderTarget() const
		{
			return nullptr;
		}
		
		virtual TSharedPtr<FSceneViewport> GetSceneViewport() const
		{
			return nullptr;
		}

	public:
		TWeakObjectPtr<UMediaCapture> MediaCapture;
		FMediaCaptureOptions CaptureOptions;
	};

	class FSceneViewportCaptureSource : public FCaptureSource
	{
	public:
		FSceneViewportCaptureSource(UMediaCapture* InMediaCapture, FMediaCaptureOptions InCaptureOptions, TSharedPtr<FSceneViewport> InSceneViewport)
			: FCaptureSource(InMediaCapture, MoveTemp(InCaptureOptions))
			, WeakViewport(InSceneViewport)
		{
		}

		virtual ECaptureType GetCaptureType() const override
		{
			if (CaptureOptions.CapturePhase != EMediaCapturePhase::EndFrame)
            {
				return ECaptureType::Immediate;
            }
			return ECaptureType::OnTick;
		}
		
		virtual EMediaCaptureSourceType GetSourceType() override
		{
			return EMediaCaptureSourceType::SCENE_VIEWPORT;
		}
		
		virtual void ResetSourceBufferSize(bool bFlushRenderingCommands) override
		{
			if (TSharedPtr<FSceneViewport> SceneViewport = WeakViewport.Pin())
			{
				if (MediaCapture.IsValid())
				{
					MediaCapture->ResetFixedViewportSize(SceneViewport, bFlushRenderingCommands);
				}
			}				
		}

		virtual void ResizeSourceBuffer(FIntPoint Size) override
		{
			if (TSharedPtr<FSceneViewport> SceneViewport = WeakViewport.Pin())
			{
				if (MediaCapture.IsValid())
				{
					MediaCapture->SetFixedViewportSize(SceneViewport, Size);
				}
			}
		}

		virtual bool ValidateSource(const FIntPoint& InDesiredSize, const EPixelFormat InDesiredPixelFormat, const bool bInCurrentlyCapturing) override
		{
			bool bResult = false;
			if (TSharedPtr<FSceneViewport> SceneViewport = WeakViewport.Pin())
			{
				bResult = ValidateSceneViewport(SceneViewport, CaptureOptions, InDesiredSize, InDesiredPixelFormat, bInCurrentlyCapturing);
				if (!bResult)
				{
					MediaCapture->ResetFixedViewportSize(SceneViewport, false);
				}
			}

			return bResult;
		}

		virtual bool PostInitialize() override
		{
			if (TSharedPtr<FSceneViewport> SceneViewport = WeakViewport.Pin())
			{
				return MediaCapture->PostInitializeCaptureViewport(SceneViewport);
			}
			return false;
		}
		
		virtual FIntPoint GetSize() override
		{
			if (TSharedPtr<FSceneViewport> SceneViewport = WeakViewport.Pin())
			{
				return SceneViewport->GetSize();
			}
			return FIntPoint();
		}

		virtual bool UpdateSourceImpl() override
		{
			if (MediaCapture.IsValid())
			{
				if (TSharedPtr<FSceneViewport> SceneViewport = WeakViewport.Pin())
				{
					return MediaCapture->UpdateSceneViewportImpl(SceneViewport);
				}
			}

			return false;
		}

		virtual FTexture2DRHIRef GetSourceTextureForInput_RenderThread(FRHICommandListImmediate& RHICmdList) override
		{
			FTexture2DRHIRef SourceTexture;
			if (TSharedPtr<FSceneViewport> Viewport = WeakViewport.Pin())
			{
#if WITH_EDITOR
				if (!IsRunningGame())
				{
					// PIE, PIE in windows, editor viewport
					SourceTexture = Viewport->GetRenderTargetTexture();
					if (!SourceTexture.IsValid() && Viewport->GetViewportRHI())
					{
						SourceTexture = RHIGetViewportBackBuffer(Viewport->GetViewportRHI());
					}
				}
				else
#endif
				if (Viewport->GetViewportRHI())
				{
					// Standalone and packaged
					SourceTexture = RHIGetViewportBackBuffer(Viewport->GetViewportRHI());
				}
			}

			return SourceTexture;
		}

		virtual TSharedPtr<FSceneViewport> GetSceneViewport() const override
		{
			return WeakViewport.Pin();
		}
		
		TWeakPtr<FSceneViewport> WeakViewport;
	};

	class FRHIResourceCaptureSource : public FCaptureSource
	{
	public:
		FRHIResourceCaptureSource(UMediaCapture* InMediaCapture, FMediaCaptureOptions InCaptureOptions, const FRHICaptureResourceDescription& InResourceDescription)
			: FCaptureSource(InMediaCapture, MoveTemp(InCaptureOptions))
			, ResourceDescription(InResourceDescription)
		{
		}

		virtual ECaptureType GetCaptureType() const override
		{
			return ECaptureType::Immediate;
		}

		virtual EMediaCaptureSourceType GetSourceType() override
		{
			return EMediaCaptureSourceType::RHI_RESOURCE;
		}

		virtual bool PostInitialize() override
		{
			return MediaCapture->PostInitializeCaptureRHIResource(ResourceDescription);
		}

		virtual FIntPoint GetSize() override
		{
			return ResourceDescription.ResourceSize;
		}

		virtual bool ValidateSource(const FIntPoint& DesiredSize, const EPixelFormat DesiredPixelFormat, const bool bCurrentlyCapturing) override
		{
			return true;
		}

		FRHICaptureResourceDescription ResourceDescription;
	};


	class FRenderTargetCaptureSource : public FCaptureSource
	{
	public:
		FRenderTargetCaptureSource(UMediaCapture* InMediaCapture, FMediaCaptureOptions InCaptureOptions, UTextureRenderTarget2D* InRenderTarget)
			: FCaptureSource(InMediaCapture, MoveTemp(InCaptureOptions))
			, RenderTarget(InRenderTarget)
		{
		}
		
		virtual EMediaCaptureSourceType GetSourceType() override
        {
        	return EMediaCaptureSourceType::RENDER_TARGET;
        }

		virtual void ResizeSourceBuffer(FIntPoint Size) override
		{
			if (RenderTarget.IsValid())
			{
				RenderTarget->ResizeTarget(Size.X, Size.Y);
			}
		}
		
		virtual bool ValidateSource(const FIntPoint& InDesiredSize, const EPixelFormat InDesiredPixelFormat, const bool bInCurrentlyCapturing) override
		{
			return ValidateTextureRenderTarget2D(RenderTarget.Get(), CaptureOptions, InDesiredSize, InDesiredPixelFormat, bInCurrentlyCapturing);
		}

		virtual bool PostInitialize() override
		{
			return MediaCapture->PostInitializeCaptureRenderTarget(RenderTarget.Get());
		}

		virtual FIntPoint GetSize() override
		{
			if (RenderTarget.IsValid())
			{
				return FIntPoint(RenderTarget->SizeX, RenderTarget->SizeY);
			}

			return FIntPoint();
		}
		
		virtual bool UpdateSourceImpl() override
		{
			if (MediaCapture.IsValid())
			{
				return MediaCapture->UpdateRenderTargetImpl(RenderTarget.Get());
			}
			return false;
		}

		virtual FTexture2DRHIRef GetSourceTextureForInput_RenderThread(FRHICommandListImmediate& RHICmdList)
		{
			constexpr bool bEvenIfPendingKill = false;
			constexpr bool bThreadSafeTest = true;
			if (RenderTarget.IsValid(bEvenIfPendingKill, bThreadSafeTest) && RenderTarget.GetEvenIfUnreachable()->GetRenderTargetResource())
			{
				return RenderTarget.GetEvenIfUnreachable()->GetRenderTargetResource()->GetTextureRenderTarget2DResource()->GetTextureRHI();
			}
			return nullptr;
		}

		virtual UTextureRenderTarget2D* GetRenderTarget() const override
		{
			return RenderTarget.Get();
		}

		TWeakObjectPtr<UTextureRenderTarget2D> RenderTarget;
	};
}

