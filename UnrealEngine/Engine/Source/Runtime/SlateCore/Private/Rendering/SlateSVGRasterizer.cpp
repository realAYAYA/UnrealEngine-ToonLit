// Copyright Epic Games, Inc. All Rights Reserved.

#include "Rendering/SlateSVGRasterizer.h"

THIRD_PARTY_INCLUDES_START
// These are required for nanosvg
#include <stdio.h>
#include <string.h>
#include <math.h>
#define NANOSVG_IMPLEMENTATION	
#include "ThirdParty/nanosvg/src/nanosvg.h"
#define NANOSVGRAST_IMPLEMENTATION
#include "ThirdParty/nanosvg/src/nanosvgrast.h"
THIRD_PARTY_INCLUDES_END

#include "Misc/FileHelper.h"
#include "Containers/StringConv.h"
#include "SlateGlobals.h"

namespace SVGConstants
{
	// The DPI is only used for unit conversion
	static const float DPI = 96.0f;
	// Bytes per pixel (RGBA)
	static const int32 BPP = 4;
}

TArray<uint8> FSlateSVGRasterizer::RasterizeSVGFromFile(const FString& Filename, FIntPoint PixelSize)
{
	TArray<uint8> PixelData;

	FString SVGString;
	if (FFileHelper::LoadFileToString(SVGString, *Filename))
	{
		NSVGimage* Image = nsvgParse(TCHAR_TO_ANSI(*SVGString), "px", SVGConstants::DPI);

		if (Image)
		{
			PixelData.AddUninitialized(PixelSize.X * PixelSize.Y * SVGConstants::BPP);

			NSVGrasterizer* Rasterizer = nsvgCreateRasterizer();

			const float SVGScaleX = (float)PixelSize.X / Image->width;
			const float SVGScaleY = (float)PixelSize.Y / Image->height;

			const int32 Stride = PixelSize.X * SVGConstants::BPP;
			nsvgRasterizeFull(Rasterizer, Image, 0, 0, SVGScaleX, SVGScaleY, PixelData.GetData(), PixelSize.X, PixelSize.Y, Stride);

			nsvgDeleteRasterizer(Rasterizer);

			nsvgDelete(Image);
		}
	}
	else
	{
		UE_LOG(LogSlate, Warning, TEXT("Unable to rasterize '%s'. File could not be found"), *Filename);
	}

	return PixelData;
}

