// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Misc/Timespan.h"
#include "Math/IntPoint.h"
#include "Templates/SharedPointer.h"
#include "Templates/RefCounting.h"
#include "PixelFormat.h"

#include "MediaDecoderOutput.h"
#include "ParameterDictionary.h"

// -------------------------------------------------------------------------------------------------------------------------------

struct FVideoDecoderCropInfo
{
	FVideoDecoderCropInfo() : CropLeft(0), CropTop(0), CropRight(0), CropBottom(0) {}
	FVideoDecoderCropInfo(uint32 InCropLeft, uint32 InCropTop, uint32 InCropRight, uint32 InCropBottom) : CropLeft(InCropLeft), CropTop(InCropRight), CropRight(InCropRight), CropBottom(InCropBottom) {}

	bool HasCropping() const { return CropLeft != 0 || CropRight != 0 || CropTop != 0 || CropBottom != 0;  }

	uint32 CropLeft;
	uint32 CropTop;
	uint32 CropRight;
	uint32 CropBottom;
};

enum class EVideoOrientation
{
	Original = 0,
	CW90,
	CW180,
	CW270,
};

class IVideoDecoderTexture : public TSharedFromThis<IVideoDecoderTexture, ESPMode::ThreadSafe>
{
public:
	virtual ~IVideoDecoderTexture() {}
protected:
	IVideoDecoderTexture() {}
};


class FVideoDecoderOutput : public IDecoderOutput
{
public:
	virtual ~FVideoDecoderOutput() = default;

	virtual const Electra::FParamDict& GetDict() const
	{ 
		check(ParamDict.IsValid());
		return *ParamDict; 
	}

	virtual FDecoderTimeStamp GetTime() const
	{
		if (!ParamDict)
		{
			return FDecoderTimeStamp(FTimespan::Zero(), 0);
		}
		Electra::FTimeValue pts(ParamDict->GetValue("pts").GetTimeValue());
		return FDecoderTimeStamp(pts.GetAsTimespan(), pts.GetSequenceIndex());
	}

	virtual FTimespan GetDuration() const
	{
		if (!ParamDict)
		{
			return FTimespan(0);
		}
		return FTimespan(ParamDict->GetValue("duration").GetTimeValue().GetAsHNS());
	}

	virtual FIntPoint GetOutputDim() const
	{
		if (!ParamDict)
		{
			return FIntPoint::ZeroValue;
		}
		if (!(Cached.Flags & FCached::Valid_OutputDim))
		{
			Cached.OutputDim.X = (int32)ParamDict->GetValue("width").SafeGetInt64(0);
			Cached.OutputDim.Y = (int32)ParamDict->GetValue("height").SafeGetInt64(0);
			FPlatformMisc::MemoryBarrier();
			Cached.Flags |= FCached::Valid_OutputDim;
		}
		return Cached.OutputDim;
	}

	virtual double GetAspectRatio() const
	{
		if (!ParamDict)
		{
			return 1.0;
		}
		const FIntPoint Dim = GetOutputDim();
		return ((double)Dim.X / (double)Dim.Y) * ParamDict->GetValue("aspect_ratio").SafeGetDouble(1.0);
	}

	virtual FVideoDecoderCropInfo GetCropInfo() const
	{
		if (!ParamDict)
		{
			return FVideoDecoderCropInfo();
		}
		if (!(Cached.Flags & FCached::Valid_CropInfo))
		{ 
			Cached.CropInfo.CropLeft = (int32)ParamDict->GetValue("crop_left").SafeGetInt64(0);
			Cached.CropInfo.CropTop = (int32)ParamDict->GetValue("crop_top").SafeGetInt64(0);
			Cached.CropInfo.CropRight = (int32)ParamDict->GetValue("crop_right").SafeGetInt64(0);
			Cached.CropInfo.CropBottom = (int32)ParamDict->GetValue("crop_bottom").SafeGetInt64(0);
			FPlatformMisc::MemoryBarrier();
			Cached.Flags |= FCached::Valid_CropInfo;
		}
		return Cached.CropInfo;
	}

	virtual FIntPoint GetDim() const
	{
		if (!ParamDict)
		{
			return FIntPoint::ZeroValue;
		}
		const FIntPoint OutputDim = GetOutputDim();
		const FVideoDecoderCropInfo CropInfo = GetCropInfo();

		FIntPoint Dim;
		Dim.X = OutputDim.X + (CropInfo.CropLeft + CropInfo.CropRight);
		Dim.Y = OutputDim.Y + (CropInfo.CropTop + CropInfo.CropBottom);
		return Dim;
	}

	virtual EPixelFormat GetFormat() const
	{
		if (!ParamDict)
		{
			return EPixelFormat::PF_Unknown;
		}
		return (EPixelFormat)ParamDict->GetValue("pixelfmt").SafeGetInt64((int64)EPixelFormat::PF_Unknown);
	}

	virtual EVideoOrientation GetOrientation() const
	{
		if (!ParamDict)
		{
			return EVideoOrientation::Original;
		}
		if (!(Cached.Flags & FCached::Valid_Orientation))
		{
			Cached.Orientation = (EVideoOrientation)ParamDict->GetValue("orientation").SafeGetInt64((int64)EVideoOrientation::Original);
			FPlatformMisc::MemoryBarrier();
			Cached.Flags |= FCached::Valid_Orientation;
		}
		return Cached.Orientation;
	}

protected:
	FVideoDecoderOutput() = default;

	void Initialize(Electra::FParamDict* InParamDict)
	{
		Cached.Flags = 0;
		ParamDict.Reset(InParamDict);
	}

private:
	struct FCached
	{
		FCached()
		{
			OutputDim = FIntPoint::ZeroValue;
		}
		enum
		{
			Valid_CropInfo = 1 << 0,
			Valid_OutputDim = 1 << 1,
			Valid_Orientation = 1 << 2,
		};

		FVideoDecoderCropInfo CropInfo;
		FIntPoint OutputDim;
		EVideoOrientation Orientation = EVideoOrientation::Original;;
		uint32 Flags = 0;
	};
	mutable FCached Cached;
	TUniquePtr<Electra::FParamDict> ParamDict;
};

using FVideoDecoderOutputPtr = TSharedPtr<FVideoDecoderOutput, ESPMode::ThreadSafe>;
