// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IMediaTextureSample.h"

enum class EMediaIOCoreEncodePixelFormat
{
	A2B10G10R10,
	CharBGRA,
	CharUYVY,
	YUVv210,
};

class MEDIAIOCORE_API FMediaIOCoreEncodeTime
{
public:
	FMediaIOCoreEncodeTime(EMediaIOCoreEncodePixelFormat InFormat, void* InBuffer, uint32 InPitch, uint32 InWidth, uint32 InHeight);
	void Render(uint32 InHours, uint32 InMinutes, uint32 InSeconds, uint32 InFrames) const;

protected:
	using TColor = int32;
	void DrawChar(uint32 InX, uint32 InChar) const;
	void DrawTime(uint32 InX, uint32 InTime) const;
	void SetPixelScaled(uint32 InX, uint32 InY, bool InSet, uint32 InScale) const;
	void SetPixel(uint32 InX, uint32 InY, bool InSet) const;

	inline TColor& At(uint32 InX, uint32 InY) const
	{
		return *(reinterpret_cast<TColor*>(reinterpret_cast<char*>(Buffer) + (Pitch * InY)) + InX);
	}

protected:
	/** Pixel format */
	EMediaIOCoreEncodePixelFormat Format;

	/** Pointer to pixels */
	void* Buffer;

	/** Pitch of image */
	uint32 Pitch;

	/** Width of image */
	uint32 Width;

	/** Height of image */
	uint32 Height;

protected:
	TColor ColorBlack;
	TColor ColorWhite;
};