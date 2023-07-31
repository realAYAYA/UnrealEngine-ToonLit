// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "RHI.h"
#include "RHIResources.h"
#include "Engine/Texture2D.h"
#include "UObject/SoftObjectPath.h"
#include "OculusHMDTypes.generated.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS

USTRUCT(meta = (Deprecated = "5.1"))
struct FOculusSplashDesc
{
	GENERATED_USTRUCT_BODY()

	UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.")
	UPROPERTY(config, EditAnywhere, Category = Settings, meta = (
		AllowedClasses = "/Script/Engine.Texture",
		ToolTip = "Texture to display",
		DeprecatedProperty))
	FSoftObjectPath		TexturePath;

	UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.")
	UPROPERTY(config, EditAnywhere, Category = Settings, meta = (
		ToolTip = "transform of center of quad (meters).",
		DeprecatedProperty))
	FTransform			TransformInMeters;

	UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.")
	UPROPERTY(config, EditAnywhere, Category = Settings, meta = (
		ToolTip = "Dimensions in meters.",
		DeprecatedProperty))
	FVector2D			QuadSizeInMeters;

	UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.")
	UPROPERTY(config, EditAnywhere, Category = Settings, meta = (
		ToolTip = "A delta rotation that will be added each rendering frame (half rate of full vsync).",
		DeprecatedProperty))
	FQuat				DeltaRotation;

	UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.")
	UPROPERTY(config, EditAnywhere, Category = Settings, meta = (
		ToolTip = "Texture offset amount from the top left corner.",
		DeprecatedProperty))
	FVector2D			TextureOffset;

	UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.")
	UPROPERTY(config, EditAnywhere, Category = Settings, meta = (
		ToolTip = "Texture scale.",
		DeprecatedProperty))
	FVector2D			TextureScale;

	UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.")
	UPROPERTY(config, EditAnywhere, Category = Settings, meta = (
		ToolTip = "Whether the splash layer uses it's alpha channel.",
		DeprecatedProperty))
	bool				bNoAlphaChannel;

	// Runtime data
	UTexture*			LoadingTexture;
	FTextureRHIRef		LoadedTexture;
	bool				bIsDynamic;

	FOculusSplashDesc()
		: TransformInMeters(FVector(4.0f, 0.f, 0.f))
		, QuadSizeInMeters(3.f, 3.f)
		, DeltaRotation(FQuat::Identity)
		, TextureOffset(0.0f, 0.0f)
		, TextureScale(1.0f, 1.0f)
		, bNoAlphaChannel(false)
		, LoadingTexture(nullptr)
		, LoadedTexture(nullptr)
		, bIsDynamic(false)
	{
	}

	bool operator==(const FOculusSplashDesc& d) const
	{
		return TexturePath == d.TexturePath &&
			TransformInMeters.Equals(d.TransformInMeters) &&
			QuadSizeInMeters == d.QuadSizeInMeters && DeltaRotation.Equals(d.DeltaRotation) &&
			TextureOffset == d.TextureOffset && TextureScale == d.TextureScale &&
			bNoAlphaChannel == d.bNoAlphaChannel &&
			LoadingTexture == d.LoadingTexture && LoadedTexture == d.LoadedTexture && bIsDynamic == d.bIsDynamic;
	}
};
PRAGMA_ENABLE_DEPRECATION_WARNINGS
