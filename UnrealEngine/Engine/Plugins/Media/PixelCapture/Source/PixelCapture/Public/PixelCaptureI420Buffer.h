// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Buffer class for holding I420 frame data.
 */
class PIXELCAPTURE_API FPixelCaptureI420Buffer
{
public:
	/**
	 * Create an empty buffer of the specified size.
	 */
	FPixelCaptureI420Buffer(int InWidth, int InHeight);

	FPixelCaptureI420Buffer(const FPixelCaptureI420Buffer& Other) = default;
	~FPixelCaptureI420Buffer() = default;

	/**
	 * Get the width of the frame
	 */
	int GetWidth() const { return Width; }

	/**
	 * Get the height of the frame
	 */
	int GetHeight() const { return Height; }

	/**
	 * Get the stride of the Y plane
	 */
	int GetStrideY() const { return StrideY; }

	/**
	 * Get the stride of the U and V planes
	 */
	int GetStrideUV() const { return StrideUV; }

	/**
	 * Gets a const pointer to the beginning of entire buffer for reading.
	 */
	const uint8_t* GetData() const;

	/**
	 * Gets a const pointer to the beginning of the Y plane for reading.
	 */
	const uint8_t* GetDataY() const;

	/**
	 * Gets a const pointer to the beginning of the U plane for reading.
	 */
	const uint8_t* GetDataU() const;

	/**
	 * Gets a const pointer to the beginning of the V plane for reading.
	 */
	const uint8_t* GetDataV() const;

	/**
	 * Gets a pointer to the beginning of the entire buffer for editing.
	 */
	uint8_t* GetMutableData();

	/**
	 * Gets a pointer to the beginning of the Y plane for editing.
	 */
	uint8_t* GetMutableDataY();

	/**
	 * Gets a pointer to the beginning of the U plane for editing.
	 */
	uint8_t* GetMutableDataU();

	/**
	 * Gets a pointer to the beginning of the V plane for editing.
	 */
	uint8_t* GetMutableDataV();

	/**
	 * Gets the size of the Y plane in bytes.
	 */
	int GetDataSizeY() const;

	/**
	 * Gets the size of both the U and V planes in bytes.
	 */
	int GetDataSizeUV() const;

	/**
	 * Gets the total size of this buffer.
	 */
	int GetSize() const { return Data.Num(); }

	/**
	 * Copies the given buffer into this buffer. Supplied so copies
	 * can be explicit in code where we want to be verbose.
	 */
	void Copy(const FPixelCaptureI420Buffer& Other) { *this = Other; }

private:
	int Width = 0;
	int Height = 0;

	int StrideY = 0;
	int StrideUV = 0;

	TArray<uint8_t> Data;
};
