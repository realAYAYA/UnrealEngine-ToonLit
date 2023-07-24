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



class IVideoDecoderColorimetry : public TSharedFromThis<IVideoDecoderColorimetry, ESPMode::ThreadSafe>
{
public:
	struct FMPEGDefinition
	{
		uint8 ColourPrimaries = 2;
		uint8 TransferCharacteristics = 2;
		uint8 MatrixCoefficients = 2;
		uint8 VideoFullRangeFlag = 0;
		uint8 VideoFormat = 5;
	};

	virtual ~IVideoDecoderColorimetry() = default;
	virtual FMPEGDefinition const* GetMPEGDefinition() const = 0;
};



struct FVideoDecoderHDRMetadata_mastering_display_colour_volume
{
	// Index 0=red, 1=green, 2=blue
	float display_primaries_x[3];
	float display_primaries_y[3];
	float white_point_x;
	float white_point_y;
	float max_display_mastering_luminance;
	float min_display_mastering_luminance;
};

struct FVideoDecoderHDRMetadata_content_light_level_info
{
	uint16 max_content_light_level = 0;			// MaxCLL
	uint16 max_pic_average_light_level = 0;		// MaxFALL
};



class IVideoDecoderHDRInformation : public TSharedFromThis<IVideoDecoderHDRInformation, ESPMode::ThreadSafe>
{
public:
	enum class EType
	{
		Unknown,
		PQ10,				// 10 bit HDR, no metadata.
		HDR10,				// 10 bit HDR, static metadata (mastering display colour volume + content light level info)
		HLG10				// 10 bit HDR, static metadata (mastering display colour volume + content light level info) (HLG transfer characteristics)
	};

	virtual ~IVideoDecoderHDRInformation() = default;

	// Returns the type of HDR in use.
	virtual EType GetHDRType() const = 0;

	// Get mastering display colour volume if available. Returns nullptr if information is not available.
	virtual const FVideoDecoderHDRMetadata_mastering_display_colour_volume* GetMasteringDisplayColourVolume() const = 0;

	// Get content light level info if available. Returns nullptr if information is not available.
	virtual const FVideoDecoderHDRMetadata_content_light_level_info* GetContentLightLevelInfo() const = 0;
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

	virtual int32 GetBitsPerComponent() const
	{
		const int32 NumDefaultBits = 8;
		if (ParamDict)
		{
			return (int32)ParamDict->GetValue("bits_per").SafeGetInt64((int64)NumDefaultBits);
		}
		return NumDefaultBits;
	}

	virtual TSharedPtr<const IVideoDecoderHDRInformation, ESPMode::ThreadSafe> GetHDRInformation() const
	{
		if (ParamDict && ParamDict->HaveKey("hdr_info"))
		{
			return ParamDict->GetValue("hdr_info").GetSharedPointer<const IVideoDecoderHDRInformation>();
		}
		return nullptr;
	}

	virtual TSharedPtr<const IVideoDecoderColorimetry, ESPMode::ThreadSafe> GetColorimetry() const
	{
		if (ParamDict && ParamDict->HaveKey("colorimetry"))
		{
			return ParamDict->GetValue("colorimetry").GetSharedPointer<const IVideoDecoderColorimetry>();
		}
		return nullptr;
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
