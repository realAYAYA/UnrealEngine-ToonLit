// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IPixelCaptureBuffer.h"

/**
 * Buffer class for holding I420 frame data.
 */
class PIXELCAPTURE_API FPixelCaptureBufferI420 : public IPixelCaptureBuffer
{
public:
	/**
	 * Create an empty buffer of the specified size.
	 */
	FPixelCaptureBufferI420(int InWidth, int InHeight);

	FPixelCaptureBufferI420(const FPixelCaptureBufferI420& Other) = default;
	virtual	~FPixelCaptureBufferI420() = default;

	/**
	 * Get the width of the frame
	 */
	virtual int32 GetWidth() const override { return Width; }

	/**
	 * Get the height of the frame
	 */
	virtual int32 GetHeight() const override { return Height; }

	/**
	 * Get the Buffer format
	 */
	virtual int32 GetFormat() const override { return PixelCaptureBufferFormat::FORMAT_I420; };

	/**
	 * Get the stride of the Y plane
	 */
	int32 GetStrideY() const { return StrideY; }

	/**
	 * Get the stride of the U and V planes
	 */
	int32 GetStrideUV() const { return StrideUV; }

	/**
	 * Gets a const pointer to the beginning of entire buffer for reading.
	 */
	virtual const uint8_t* GetData() const override;

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
	virtual uint8_t*  GetMutableData() override;

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
	int32 GetDataSizeY() const;

	/**
	 * Gets the size of both the U and V planes in bytes.
	 */
	int32 GetDataSizeUV() const;

	/**
	 * Gets the total size of this buffer.
	 */
	virtual int64 GetSize() const override { return Data.Num(); }

	/**
	 * Copies the given buffer into this buffer. Supplied so copies
	 * can be explicit in code where we want to be verbose.
	 */
	void Copy(const FPixelCaptureBufferI420& Other) { *this = Other; }

private:
	int Width = 0;
	int Height = 0;

	int StrideY = 0;
	int StrideUV = 0;

	TArray<uint8_t> Data;
};
