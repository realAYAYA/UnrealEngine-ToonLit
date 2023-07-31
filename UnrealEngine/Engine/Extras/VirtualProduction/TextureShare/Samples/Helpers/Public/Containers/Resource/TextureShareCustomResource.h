// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Containers/TextureShareSDKContainers.h"

/**
 * Custom resource
 */
struct FTextureShareCustomResource
{
	// Request custom size or\and format
	const FIntPoint CustomSize = FIntPoint::ZeroValue;
	const DXGI_FORMAT CustomFormat = DXGI_FORMAT::DXGI_FORMAT_UNKNOWN;

public:
	FTextureShareCustomResource()
	{ }

	FTextureShareCustomResource(const FIntPoint& InCustomSize)
		: CustomSize(InCustomSize)
	{ }

	FTextureShareCustomResource(const FIntPoint& InCustomSize, const DXGI_FORMAT InCustomFormat)
		: CustomSize(InCustomSize), CustomFormat(InCustomFormat)
	{ }

	FTextureShareCustomResource(const DXGI_FORMAT InCustomFormat)
		: CustomFormat(InCustomFormat)
	{ }
};

/**
 * Copy params container
 */
struct FTextureShareTextureCopyParameters
{
	FTextureShareTextureCopyParameters() = default;

	FTextureShareTextureCopyParameters FindValidRect(const FIntPoint& InSrcSize, const FIntPoint& InDestSize) const
	{
		FTextureShareTextureCopyParameters Result;

		Result.Src  = FIntPoint(FMath::Min(Src.X,  InSrcSize.X),  FMath::Min(Src.Y,  InSrcSize.Y));
		Result.Dest = FIntPoint(FMath::Min(Dest.X, InDestSize.X), FMath::Min(Dest.Y, InDestSize.Y));

		const FIntPoint  MaxSrcRect(InSrcSize.X  - Result.Src.X,  InSrcSize.Y  - Result.Src.Y);
		const FIntPoint MaxDestRect(InDestSize.X - Result.Dest.X, InDestSize.Y - Result.Dest.Y);
		const FIntPoint MaxRect(FMath::Min(MaxSrcRect.X, MaxDestRect.X), FMath::Min(MaxSrcRect.Y, MaxDestRect.Y));

		Result.Rect = FIntPoint(FMath::Min(MaxRect.X, Rect.X), FMath::Min(MaxRect.Y, Rect.Y));

		return Result;
	}

	bool IsValid() const
	{
		return Rect.X > 0 && Rect.Y > 0;
	}

public:
	// Copy region
	FIntPoint Rect = FIntPoint::ZeroValue;
	// From source texture top-left point
	FIntPoint Src = FIntPoint::ZeroValue;
	// To Dest texture top-left point
	FIntPoint Dest = FIntPoint::ZeroValue;
};
