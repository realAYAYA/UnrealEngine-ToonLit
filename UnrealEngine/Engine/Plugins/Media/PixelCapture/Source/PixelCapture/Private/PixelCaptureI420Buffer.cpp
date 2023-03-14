// Copyright Epic Games, Inc. All Rights Reserved.

#include "PixelCaptureI420Buffer.h"

namespace
{
	int CalcI420Size(int StrideY, int StrideUV, int Height)
	{
		return StrideY * Height + ((StrideUV + StrideUV) * ((Height + 1) / 2));
	}
}

FPixelCaptureI420Buffer::FPixelCaptureI420Buffer(int InWidth, int InHeight)
	: Width(InWidth)
	, Height(InHeight)
	, StrideY(InWidth)
	, StrideUV((InWidth + 1) / 2)
{
	Data.SetNum(CalcI420Size(StrideY, StrideUV, Height));
}

const uint8_t* FPixelCaptureI420Buffer::GetData() const
{
	return Data.GetData();
}

const uint8_t* FPixelCaptureI420Buffer::GetDataY() const
{
	return GetData();
}

const uint8_t* FPixelCaptureI420Buffer::GetDataU() const
{
	return GetDataY() + GetDataSizeY();
}

const uint8_t* FPixelCaptureI420Buffer::GetDataV() const
{
	return GetDataU() + GetDataSizeUV();
}

uint8_t* FPixelCaptureI420Buffer::GetMutableData()
{
	return Data.GetData();
}

uint8_t* FPixelCaptureI420Buffer::GetMutableDataY()
{
	return GetMutableData();
}

uint8_t* FPixelCaptureI420Buffer::GetMutableDataU()
{
	return GetMutableDataY() + GetDataSizeY();
}

uint8_t* FPixelCaptureI420Buffer::GetMutableDataV()
{
	return GetMutableDataU() + GetDataSizeUV();
}

int FPixelCaptureI420Buffer::GetDataSizeY() const
{
	return StrideY * Height;
}

int FPixelCaptureI420Buffer::GetDataSizeUV() const
{
	return StrideUV * ((Height + 1) / 2);
}
