// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RHI.h"

enum class EDisplayClusterShaderParametersICVFX_ChromakeySource : uint8
{
	// Dont use chromakey
	Disabled = 0,

	// Render color over camera frame
	FrameColor,

	// Render specified layer from scene
	ChromakeyLayers,
};

enum class EDisplayClusterShaderParametersICVFX_LightcardRenderMode : uint8
{
	// Render incamera frame over lightcard
	Under = 0,

	// Over lightcard over incamera frame
	Over,
};

struct FDisplayClusterShaderParametersICVFX_CameraContext
{
	FRotator CameraViewRotation;
	FVector  CameraViewLocation;
	FMatrix  CameraPrjMatrix;
};

struct FDisplayClusterShaderParametersICVFX_ViewportResource
{
public:
	bool IsValid() const
	{
		return Texture != nullptr && Texture->IsValid();
	}

	bool IsDefined() const
	{
		return ViewportId.IsEmpty() == false;
	}

	void Reset()
	{
		ViewportId.Empty();
		Texture = nullptr;
	}

public:
	// Viewport name (Used to resolve viewport resource to texture ref)
	FString      ViewportId;

	// This ref resolved runtime
	FRHITexture* Texture = nullptr;
};

// Compact ICVFX data for viewport
class FDisplayClusterShaderParameters_ICVFX
{
public:
	FDisplayClusterShaderParameters_ICVFX()
	{ }

	FDisplayClusterShaderParameters_ICVFX(const FDisplayClusterShaderParameters_ICVFX& InParameters)
	{
		SetParameters(InParameters);
	}

public:
	bool IsAnyLightcardUsed() const
	{
		return IsLightcardOverUsed() || IsLightcardUnderUsed();
	}

	bool IsLightcardOverUsed() const
	{
		return Lightcard.IsValid() && LightcardMode == EDisplayClusterShaderParametersICVFX_LightcardRenderMode::Over;
	}

	bool IsLightcardUnderUsed() const
	{
		return Lightcard.IsValid() && LightcardMode == EDisplayClusterShaderParametersICVFX_LightcardRenderMode::Under;
	}

	bool IsCameraUsed(int32 CameraIndex) const
	{
		return (Cameras.Num() > CameraIndex) && CameraIndex >= 0 && Cameras[CameraIndex].IsUsed();
	}

	bool IsAnyCameraUsed() const
	{
		return (Cameras.Num() > 0);
	}

	bool IsMultiCamerasUsed() const
	{
		return (Cameras.Num() > 1);
	}

	bool IsValid()
	{
		return IsAnyLightcardUsed() || IsAnyCameraUsed();
	}

public:
	void Reset()
	{
		Cameras.Empty();
		Lightcard.Reset();

		UVLightCardMap = nullptr;
	}

	// Implement copy ref and arrays
	void SetParameters(const FDisplayClusterShaderParameters_ICVFX& InParameters)
	{
		Reset();

		Cameras = InParameters.Cameras;

		Lightcard      = InParameters.Lightcard;
		LightcardMode = InParameters.LightcardMode;

		UVLightCardMap = InParameters.UVLightCardMap;
	}

	void CollectRefViewports(TArray<FDisplayClusterShaderParametersICVFX_ViewportResource*>& Dst)
	{
		if (Lightcard.IsDefined())
		{
			Dst.Add(&Lightcard);
		}

		for (FCameraSettings& CameraIt : Cameras)
		{
			CameraIt.CollectRefViewports(Dst);
		}
	}

	inline void SortCamerasRenderOrder()
	{
		Cameras.Sort([](const FCameraSettings& It1, const FCameraSettings& It2)
		{
			if (It1.RenderOrder == It2.RenderOrder)
			{
				return It1.Resource.ViewportId.Compare(It2.Resource.ViewportId, ESearchCase::IgnoreCase) < 0;
			}
			return It1.RenderOrder < It2.RenderOrder;
		});
	}

public:
	// ICVFX Target only data
	struct FCameraSettings
	{
		void CollectRefViewports(TArray<FDisplayClusterShaderParametersICVFX_ViewportResource*>& Dst)
		{
			if (Resource.IsDefined())
			{
				Dst.Add(&Resource);
			}

			if (Chromakey.IsDefined())
			{
				Dst.Add(&Chromakey);
			}
		}

		bool IsUsed() const
		{
			return (ChromakeySource == EDisplayClusterShaderParametersICVFX_ChromakeySource::FrameColor) || Resource.IsValid();
		}

		bool IsChromakeyMarkerUsed() const
		{
			return ChromakeMarkerTextureRHI.IsValid();
		}

		void UpdateCameraContext(const FDisplayClusterShaderParametersICVFX_CameraContext& InContext)
		{
			// Support icvfx stereo - update context from camera for each eye
			CameraViewRotation = Local2WorldTransform.InverseTransformRotation(InContext.CameraViewRotation.Quaternion()).Rotator();
			CameraViewLocation = Local2WorldTransform.InverseTransformPosition(InContext.CameraViewLocation);
			CameraPrjMatrix = InContext.CameraPrjMatrix;
		}

	public:
		FDisplayClusterShaderParametersICVFX_ViewportResource Resource;
		FVector4 SoftEdge;

		FLinearColor InnerCameraBorderColor = FLinearColor::Black;
		float InnerCameraBorderThickness = 0.1f;
		float InnerCameraFrameAspectRatio = 1.0f;

		// Camera Origin
		FTransform Local2WorldTransform;

		// Camera world
		FRotator CameraViewRotation;
		FVector  CameraViewLocation;
		FMatrix  CameraPrjMatrix;

		// Chromakey settings:
		EDisplayClusterShaderParametersICVFX_ChromakeySource  ChromakeySource = EDisplayClusterShaderParametersICVFX_ChromakeySource::Disabled;
		FDisplayClusterShaderParametersICVFX_ViewportResource Chromakey;
		FLinearColor ChromakeyColor = FLinearColor::Black;

		// Chromakey markers settings:
		FLinearColor ChromakeyMarkersColor = FLinearColor::Black;
		float ChromakeyMarkersScale;
		float ChromakeyMarkersDistance;
		FVector2D ChromakeyMarkersOffset;
		FTextureRHIRef ChromakeMarkerTextureRHI;

		int32 RenderOrder = -1;
	};

	// Remove unused cameras from render
	bool CleanupCamerasForRender()
	{
		const TArray<FCameraSettings> InCameras = Cameras;
		Cameras.Empty();

		for (const FCameraSettings& CameraIt : InCameras)
		{
			if (CameraIt.IsUsed())
			{
				Cameras.Add(CameraIt);
			}
		}

		return Cameras.Num() == InCameras.Num();
	}

	TArray<FCameraSettings> Cameras;

	// Lightcard settings
	FDisplayClusterShaderParametersICVFX_ViewportResource    Lightcard;
	EDisplayClusterShaderParametersICVFX_LightcardRenderMode LightcardMode = EDisplayClusterShaderParametersICVFX_LightcardRenderMode::Under;

	/** Texture containing a UV map of the rendered UV light cards */
	FTextureRHIRef UVLightCardMap;
};
