// Copyright Epic Games, Inc. All Rights Reserved.

#include "libav_Decoder_Common_Video.h"
#include "HAL/PlatformMisc.h"

/***************************************************************************************************************************************************/
#if WITH_LIBAV
extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavutil/dict.h>
#include <libavutil/pixdesc.h>
}

/***************************************************************************************************************************************************/
namespace FLibavVideoDecoderOptions
{
	static const FString ForceSW(TEXT("force_sw"));
	static const TCHAR* const EnvForceSW(TEXT("UE_LIBAV_FORCE_SW"));
	static const FString HWPriority(TEXT("hw_priority"));
	static const TCHAR* const EnvHWPref(TEXT("UE_LIBAV_HWACCEL_PREFS"));
}
/***************************************************************************************************************************************************/
class FLibavDecoderVideoCommon;
class FLibavDecoderDecodedImage;


class FLibavDecodedImageDeleter
{
public:
	FLibavDecodedImageDeleter(const TSharedPtr<FLibavDecoderVideoCommon, ESPMode::ThreadSafe>& InOwningDecoder)
		: OwningDecoder(InOwningDecoder)
	{ }
	void operator()(FLibavDecoderDecodedImage* ImageToDelete);
private:
	TWeakPtr<FLibavDecoderVideoCommon, ESPMode::ThreadSafe> OwningDecoder;
};



class FLibavDecoderDecodedImage : public ILibavDecoderDecodedImage
{
public:
	virtual ~FLibavDecoderDecodedImage()
	{
		ReleaseFrame();
	}
	const ILibavDecoderVideoCommon::FOutputInfo& GetOutputInfo() const override
	{ return OutputInfo; }

	bool Setup(const AVFrame* InFrame, const ILibavDecoderVideoCommon::FOutputInfo& InOutputInfo);

private:
	void ReleaseFrame()
	{
		if (Frame)
		{
			av_frame_free(&Frame);
			Frame = nullptr;
		}
	}
	ILibavDecoderVideoCommon::FOutputInfo OutputInfo;
	AVFrame* Frame = nullptr;
};

bool FLibavDecoderDecodedImage::Setup(const AVFrame* InFrame, const ILibavDecoderVideoCommon::FOutputInfo& InOutputInfo)
{
	ReleaseFrame();
	OutputInfo = InOutputInfo;
	Frame = av_frame_clone(InFrame);
	return true;
}


/***************************************************************************************************************************************************/

class FLibavDecoderVideoCommon : public ILibavDecoderVideoCommon, public TSharedFromThis<FLibavDecoderVideoCommon, ESPMode::ThreadSafe>
{
public:
	FLibavDecoderVideoCommon() = default;
	virtual ~FLibavDecoderVideoCommon();

	int32 GetLastLibraryError() const override
	{ return LastError; }

	EDecoderError DecodeAccessUnit(const FInputAU& InInputAccessUnit) override;
	EDecoderError SendEndOfData() override;
	void Reset() override;
	ILibavDecoder::EOutputStatus HaveOutput(FOutputInfo& OutInfo) override;
	TSharedPtr<ILibavDecoderDecodedImage, ESPMode::ThreadSafe> GetOutput() override;

	EDecoderError Initialize(int64 CodecID, ILibavDecoderVideoResourceAllocator* InVideoResourceAllocator, const TMap<FString, FVariant>& InOptions);
	bool ReturnDecodedFrame(FLibavDecoderDecodedImage* ImageToDelete);

private:
	AVPixelFormat SelectOutputFormat(const enum AVPixelFormat* InListOfPossibleFormats);
	static AVPixelFormat _SelectOutputFormat(AVCodecContext* InContext, const enum AVPixelFormat* InListOfPossibleFormats)
	{ return static_cast<FLibavDecoderVideoCommon*>(InContext->opaque)->SelectOutputFormat(InListOfPossibleFormats); }

	void InternalReset();

	ILibavDecoderVideoResourceAllocator* VideoResourceAllocator = nullptr;

	AVCodecID CodecID = AV_CODEC_ID_NONE;
	AVCodec* Codec = nullptr;
	AVCodecContext* Context = nullptr;
	AVPacket* Packet = nullptr;
	AVHWDeviceType HWDevType = AV_HWDEVICE_TYPE_NONE;
	AVPixelFormat HWPixelFormat = AV_PIX_FMT_NONE;
	AVBufferRef* HWDeviceContext = nullptr;
	int32 PacketBufferSize = 0;
	struct AVFrame* Frame = nullptr;
	int32 LastError = 0;
	bool bHasPendingOutput = false;
	bool bIsSupportedOutputFormat = false;
	FOutputInfo CurrentOutputInfo;
	bool bReinitNeeded = false;
};

FLibavDecoderVideoCommon::~FLibavDecoderVideoCommon()
{
	InternalReset();
}


void FLibavDecoderVideoCommon::InternalReset()
{
	if (Context)
	{
		if (Context->extradata)
		{
			av_free(Context->extradata);
			Context->extradata = nullptr;
		}
		avcodec_free_context(&Context);
		Context = nullptr;
	}
	if (Packet)
	{
		av_freep(&Packet->data);
		av_packet_free(&Packet);
		PacketBufferSize = 0;
	}
	if (Frame)
	{
		av_frame_free(&Frame);
	}
	if (HWDeviceContext)
	{
 		av_buffer_unref(&HWDeviceContext);
		HWDeviceContext = nullptr;
	}
	Codec = nullptr;
	HWDevType = AV_HWDEVICE_TYPE_NONE;
	HWPixelFormat = AV_PIX_FMT_NONE;
	bHasPendingOutput = false;
	bIsSupportedOutputFormat = false;
	bReinitNeeded = false;
	LastError = 0;
}

AVPixelFormat FLibavDecoderVideoCommon::SelectOutputFormat(const enum AVPixelFormat* InListOfPossibleFormats)
{
	if (HWDevType != AV_HWDEVICE_TYPE_NONE && HWPixelFormat != AV_PIX_FMT_NONE)
	{
		for(const enum AVPixelFormat* FmtLst=InListOfPossibleFormats; *FmtLst!=-1; ++FmtLst)
		{
			if (*FmtLst == HWPixelFormat)
			{
				return *FmtLst;
			}
		}
	}
	// Use the first format that is not HW accelerated. They are supposedly presented in order of quality.
	for(const enum AVPixelFormat* FmtLst=InListOfPossibleFormats; *FmtLst!=-1; ++FmtLst)
	{
		bool bIsHWFmt = false;
		switch(*FmtLst)
		{
			default:
				break;
			case AV_PIX_FMT_VAAPI:
			case AV_PIX_FMT_DXVA2_VLD:
			case AV_PIX_FMT_VDPAU:
			case AV_PIX_FMT_QSV:
			case AV_PIX_FMT_MMAL:
			case AV_PIX_FMT_D3D11VA_VLD:
			case AV_PIX_FMT_CUDA:
			case AV_PIX_FMT_VIDEOTOOLBOX:
			case AV_PIX_FMT_MEDIACODEC:
			case AV_PIX_FMT_D3D11:
			case AV_PIX_FMT_OPENCL:
				bIsHWFmt = true;
				break;
		}
		if (!bIsHWFmt)
		{
			return *FmtLst;
		}
	}
	return AV_PIX_FMT_NONE;
}

ILibavDecoder::EDecoderError FLibavDecoderVideoCommon::Initialize(int64 InCodecID, ILibavDecoderVideoResourceAllocator* InVideoResourceAllocator, const TMap<FString, FVariant>& InOptions)
{
	// Reset if this was initialized before.
	InternalReset();

	// Get options
	TArray<FString> HWPriorities;
	bool bUseHW = true;
	if (InOptions.Contains(FLibavVideoDecoderOptions::ForceSW))
	{
		bUseHW = !InOptions[FLibavVideoDecoderOptions::ForceSW].GetValue<bool>();
	}
	// If the enviroment says to use software decoding this takes precedence.
	FString EnvForceSW = FPlatformMisc::GetEnvironmentVariable(FLibavVideoDecoderOptions::EnvForceSW);
	if (EnvForceSW.Equals(TEXT("1")) || EnvForceSW.Equals(TEXT("true")))
	{
		bUseHW = false;
	}
	FString HwAccelPrefs;
	if (InOptions.Contains(FLibavVideoDecoderOptions::HWPriority))
	{
		HwAccelPrefs = InOptions[FLibavVideoDecoderOptions::HWPriority].GetValue<FString>();
	}
	// If the environment specifies preferences this overrides anything the application has set.
	// This is to allow outside overrides for an already built application.
	FString EnvHwAccelPrefs = FPlatformMisc::GetEnvironmentVariable(FLibavVideoDecoderOptions::EnvHWPref);
	if (EnvHwAccelPrefs.Len())
	{
		HwAccelPrefs = EnvHwAccelPrefs;
	}
	HwAccelPrefs.ParseIntoArray(HWPriorities, TEXT(";"), true);
	for(int32 i=0; i<HWPriorities.Num(); ++i)
	{
		HWPriorities[i].TrimStartAndEndInline();
	}

	// Select the desired hardware accelerator.
	HWDevType = AV_HWDEVICE_TYPE_NONE;
	if (bUseHW)
	{
		// See if we can get one of the desired HW accelerators.
		AVHWDeviceType DevType = AV_HWDEVICE_TYPE_NONE;
		for(auto& Pri : HWPriorities)
		{
			if ((DevType = av_hwdevice_find_type_by_name(TCHAR_TO_ANSI(*Pri))) != AV_HWDEVICE_TYPE_NONE)
			{
				HWDevType = DevType;
				UE_LOG(LogLibAV, VeryVerbose, TEXT("Preferred HW accelerator \"%s\" found"), *Pri);
				break;
			}
			else
			{
				UE_LOG(LogLibAV, VeryVerbose, TEXT("Preferred HW accelerator \"%s\" not supported"), *Pri);
			}
		}
		// None found, see if the "vdpau" is there and use it.
		if (HWDevType == AV_HWDEVICE_TYPE_NONE)
		{
			DevType = AV_HWDEVICE_TYPE_NONE;
			while((DevType = av_hwdevice_iterate_types(DevType)) != AV_HWDEVICE_TYPE_NONE)
			{
				if (DevType == AV_HWDEVICE_TYPE_VDPAU)
				{
					HWDevType = DevType;
					break;
				}
			}
		}
	}

	CodecID = (enum AVCodecID)InCodecID;
	VideoResourceAllocator = InVideoResourceAllocator;
	Codec = avcodec_find_decoder(CodecID);
	if (Codec)
	{
		// Should use hardware?
		if (HWDevType != AV_HWDEVICE_TYPE_NONE)
		{
			HWPixelFormat = AV_PIX_FMT_NONE;
			for(int32 i=0; ; ++i)
			{
				const AVCodecHWConfig* HWConfig = avcodec_get_hw_config(Codec, i);
				if (!HWConfig)
				{
					UE_LOG(LogLibAV, Warning, TEXT("Decoder %s does not support device type %s"), *FString(Codec->name), *FString(av_hwdevice_get_type_name(HWDevType)));
					break;
				}
				if ((HWConfig->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX) != 0 && HWConfig->device_type == HWDevType)
				{
					HWPixelFormat = HWConfig->pix_fmt;
					break;
				}
			}
		}

		Context = avcodec_alloc_context3(Codec);
		if (Context)
		{
			if (HWPixelFormat != AV_PIX_FMT_NONE)
			{
				LastError = av_hwdevice_ctx_create(&HWDeviceContext, HWDevType, nullptr, nullptr, 0);
				if (LastError == 0)
				{
					Context->hw_device_ctx = av_buffer_ref(HWDeviceContext);
				}
				else
				{
					// HW decoding not possible
					UE_LOG(LogLibAV, Error, TEXT("Could not create %s hardware context for decoder %s"), *FString(Codec->name), *FString(av_hwdevice_get_type_name(HWDevType)));
					return ILibavDecoder::EDecoderError::Error;
				}
			}

			Context->opaque = this;
			Context->flags = AV_CODEC_FLAG_LOW_DELAY;
			Context->get_format = &FLibavDecoderVideoCommon::_SelectOutputFormat;
			AVDictionary* Opts = nullptr;
			LastError = avcodec_open2(Context, Codec, &Opts);
			av_dict_free(&Opts);
			return LastError == 0 ? ILibavDecoder::EDecoderError::None : ILibavDecoder::EDecoderError::Error;
		}
		return ILibavDecoder::EDecoderError::NotSupported;
	}
	return ILibavDecoder::EDecoderError::NotSupported;
}

void FLibavDecoderVideoCommon::Reset()
{
	InternalReset();
}



ILibavDecoder::EDecoderError FLibavDecoderVideoCommon::DecodeAccessUnit(const ILibavDecoderVideoCommon::FInputAU& InInputAccessUnit)
{
	if (!Packet)
	{
		Packet = av_packet_alloc();
		check(Packet);
		av_init_packet(Packet);
		PacketBufferSize = 0;
	}
	if (PacketBufferSize < InInputAccessUnit.DataSize)
	{
		PacketBufferSize = InInputAccessUnit.DataSize;
		Packet->data = (uint8_t*)av_realloc(Packet->data, PacketBufferSize + AV_INPUT_BUFFER_PADDING_SIZE);
	}

	Packet->dts = InInputAccessUnit.DTS;
	Packet->pts = InInputAccessUnit.PTS;
	Packet->duration = InInputAccessUnit.Duration;
	Packet->size = (int)InInputAccessUnit.DataSize;
	Packet->flags = 0;
	if ((InInputAccessUnit.Flags & ILibavDecoderVideoCommon::FInputAU::EFlags::EVidAUFlag_DoNotOutput) != 0)
	{
		Packet->flags |= AV_PKT_FLAG_DISCARD;
	}
	FMemory::Memcpy(Packet->data, InInputAccessUnit.Data, (int)InInputAccessUnit.DataSize);
	Context->reordered_opaque = InInputAccessUnit.UserValue;

	if (bReinitNeeded)
	{
		avcodec_flush_buffers(Context);
		bReinitNeeded = false;
	}
	LastError = avcodec_send_packet(Context, Packet);
	if (LastError == 0)
	{
		return ILibavDecoder::EDecoderError::None;
	}
	else if (LastError == AVERROR(EAGAIN))
	{
		return ILibavDecoder::EDecoderError::NoBuffer;
	}
	else if (LastError == AVERROR_EOF)
	{
		return ILibavDecoder::EDecoderError::EndOfData;
	}
	else
	{
		return ILibavDecoder::EDecoderError::Error;
	}
}

ILibavDecoder::EDecoderError FLibavDecoderVideoCommon::SendEndOfData()
{
	if (bReinitNeeded)
	{
		avcodec_flush_buffers(Context);
		bReinitNeeded = false;
	}
	LastError = avcodec_send_packet(Context, nullptr);
	if (LastError == 0)
	{
		return ILibavDecoder::EDecoderError::None;
	}
	else if (LastError == AVERROR(EAGAIN))
	{
		return ILibavDecoder::EDecoderError::NoBuffer;
	}
	else if (LastError == AVERROR_EOF)
	{
		return ILibavDecoder::EDecoderError::EndOfData;
	}
	else
	{
		return ILibavDecoder::EDecoderError::Error;
	}
}


ILibavDecoder::EOutputStatus FLibavDecoderVideoCommon::HaveOutput(ILibavDecoderVideoCommon::FOutputInfo& OutInfo)
{
	if (!bHasPendingOutput)
	{
		if (bReinitNeeded)
		{
			return EOutputStatus::EndOfData;
		}
		if (!Frame)
		{
			Frame = av_frame_alloc();
		}
		bIsSupportedOutputFormat = false;
		LastError = avcodec_receive_frame(Context, Frame);
		if (LastError == 0)
		{
			CurrentOutputInfo.PTS = Frame->pts;
			CurrentOutputInfo.UserValue = Frame->reordered_opaque;
			CurrentOutputInfo.Width = Frame->width;
			CurrentOutputInfo.Height = Frame->height;
			CurrentOutputInfo.CropLeft = Frame->crop_left;
			CurrentOutputInfo.CropTop = Frame->crop_top;
			CurrentOutputInfo.CropRight = Frame->crop_right;
			CurrentOutputInfo.CropBottom = Frame->crop_bottom;
			if (Frame->sample_aspect_ratio.num == 0 && Frame->sample_aspect_ratio.den == 1)
			{
				CurrentOutputInfo.AspectNum = 1;
				CurrentOutputInfo.AspectDenom = 1;
			}
			else
			{
				CurrentOutputInfo.AspectNum = Frame->sample_aspect_ratio.num;
				CurrentOutputInfo.AspectDenom = Frame->sample_aspect_ratio.den;
			}

			CurrentOutputInfo.ISO23001_8_ColorPrimaries = (int32) Frame->color_primaries;
			CurrentOutputInfo.ISO23001_8_TransferCharacteristics = (int32) Frame->color_trc;
			CurrentOutputInfo.ISO23001_8_MatrixCoefficients = (int32) Frame->colorspace;

			int32 Plane0Width = Frame->linesize[0];
			int32 Plane0Height = Frame->height;

			// Hardware format?
			if (HWPixelFormat != AV_PIX_FMT_NONE && Frame->format == HWPixelFormat)
			{
				AVHWFramesContext* ctx = (AVHWFramesContext*)Frame->hw_frames_ctx->data;
				check(ctx);

				AVPixelFormat* TargetPixelFmts = nullptr;
				AVPixelFormat PreferredFormat = AV_PIX_FMT_NONE;
				if (av_hwframe_transfer_get_formats(Frame->hw_frames_ctx, AV_HWFRAME_TRANSFER_DIRECTION_FROM, &TargetPixelFmts, 0) == 0)
				{
					for(int32 i=0; TargetPixelFmts[i] != AV_PIX_FMT_NONE; ++i)
					{
						AVPixelFormat pf = TargetPixelFmts[i];

						if (pf == AV_PIX_FMT_YUV420P && PreferredFormat == AV_PIX_FMT_NONE)
						{
							PreferredFormat = pf;
						}
						if (pf == AV_PIX_FMT_NV12)
						{
							PreferredFormat = pf;
						}
						//UE_LOG(LogLibAV, Log, TEXT("xfer %s"), *FString(av_get_pix_fmt_name(pf)));
					}
				}
				av_free(TargetPixelFmts);
				if (PreferredFormat != AV_PIX_FMT_NONE)
				{
					AVFrame* TransferFrame = av_frame_alloc();
					TransferFrame->format = PreferredFormat;
					// Set the size of the transfer buffer to that of the hardware one.
					// This is required for av_hwframe_transfer_data()
					TransferFrame->width = ctx->width;
					TransferFrame->height = ctx->height;

					// Get buffers for the frame on the CPU side. The documentation says that av_hwframe_transfer_data()
					// will do this if there are none, but that does not work and returns an error.
					if ((LastError = av_frame_get_buffer(TransferFrame, 0)) == 0)
					{
						LastError = av_hwframe_transfer_data(TransferFrame, Frame, AV_HWFRAME_TRANSFER_DIRECTION_FROM);
					}

					Plane0Width = TransferFrame->width;
					Plane0Height = TransferFrame->height;

					// Set back to the actual image size.
					TransferFrame->width = Frame->width;
					TransferFrame->height = Frame->height;
					av_frame_unref(Frame);
					av_frame_move_ref(Frame, TransferFrame);
					av_frame_free(&TransferFrame);
				}
				else
				{
					UE_LOG(LogLibAV, Warning, TEXT("Hardware decoder output cannot be provided in one of the preferred formats, skipping."));
					LastError = AVERROR(EINVAL);
				}
			}

			if (LastError == 0 && Frame->format == AV_PIX_FMT_NV12)
			{
				bIsSupportedOutputFormat = true;
				CurrentOutputInfo.NumPlanes = 2;
				// Y plane
				CurrentOutputInfo.Planes[0].Content = FPlaneInfo::EContent::Luma;
				CurrentOutputInfo.Planes[0].Width = Frame->linesize[0];
				CurrentOutputInfo.Planes[0].Height = Plane0Height;
				CurrentOutputInfo.Planes[0].BytesPerPixel = 1;
				CurrentOutputInfo.Planes[0].ByteOffsetToFirstPixel = 0;
				CurrentOutputInfo.Planes[0].ByteOffsetBetweenPixels = 1;
				CurrentOutputInfo.Planes[0].ByteOffsetBetweenRows = Frame->linesize[0];
				CurrentOutputInfo.Planes[0].Address = Frame->data[0];

				// UV plane
				CurrentOutputInfo.Planes[1].Content = FPlaneInfo::EContent::ChromaUV;
				CurrentOutputInfo.Planes[1].Width = Frame->linesize[1];
				CurrentOutputInfo.Planes[1].Height = Plane0Height / 2;
				CurrentOutputInfo.Planes[1].BytesPerPixel = 1;
				CurrentOutputInfo.Planes[1].ByteOffsetToFirstPixel = 0;
				CurrentOutputInfo.Planes[1].ByteOffsetBetweenPixels = 2;
				CurrentOutputInfo.Planes[1].ByteOffsetBetweenRows = Frame->linesize[1] * 2;
				CurrentOutputInfo.Planes[1].Address = Frame->data[1];

			}
			else if (LastError == 0 && Frame->format == AV_PIX_FMT_YUV420P)
			{
				bIsSupportedOutputFormat = true;
				CurrentOutputInfo.NumPlanes = 3;
				// Y plane
				CurrentOutputInfo.Planes[0].Content = FPlaneInfo::EContent::Luma;
				CurrentOutputInfo.Planes[0].Width = Frame->linesize[0];
				CurrentOutputInfo.Planes[0].Height = Plane0Height;
				CurrentOutputInfo.Planes[0].BytesPerPixel = 1;
				CurrentOutputInfo.Planes[0].ByteOffsetToFirstPixel = 0;
				CurrentOutputInfo.Planes[0].ByteOffsetBetweenPixels = 1;
				CurrentOutputInfo.Planes[0].ByteOffsetBetweenRows = Frame->linesize[0];
				CurrentOutputInfo.Planes[0].Address = Frame->data[0];

				// U plane
				CurrentOutputInfo.Planes[1].Content = FPlaneInfo::EContent::ChromaU;
				CurrentOutputInfo.Planes[1].Width = Frame->linesize[1];
				CurrentOutputInfo.Planes[1].Height = Plane0Height / 2;
				CurrentOutputInfo.Planes[1].BytesPerPixel = 1;
				CurrentOutputInfo.Planes[1].ByteOffsetToFirstPixel = 0;
				CurrentOutputInfo.Planes[1].ByteOffsetBetweenPixels = 1;
				CurrentOutputInfo.Planes[1].ByteOffsetBetweenRows = Frame->linesize[1];
				CurrentOutputInfo.Planes[1].Address = Frame->data[1];

				// V plane
				CurrentOutputInfo.Planes[2].Content = FPlaneInfo::EContent::ChromaV;
				CurrentOutputInfo.Planes[2].Width = Frame->linesize[2];
				CurrentOutputInfo.Planes[2].Height = Plane0Height / 2;
				CurrentOutputInfo.Planes[2].BytesPerPixel = 1;
				CurrentOutputInfo.Planes[2].ByteOffsetToFirstPixel = 0;
				CurrentOutputInfo.Planes[2].ByteOffsetBetweenPixels = 1;
				CurrentOutputInfo.Planes[2].ByteOffsetBetweenRows = Frame->linesize[2];
				CurrentOutputInfo.Planes[2].Address = Frame->data[2];
			}

			bHasPendingOutput = true;
		}
		else if (LastError == AVERROR(EAGAIN))
		{
			return EOutputStatus::NeedInput;
		}
		else if (LastError == AVERROR_EOF)
		{
			bReinitNeeded = true;
			return EOutputStatus::EndOfData;
		}
	}
	OutInfo = CurrentOutputInfo;
	return bHasPendingOutput ? ILibavDecoder::EOutputStatus::Available : ILibavDecoder::EOutputStatus::NeedInput;
}

TSharedPtr<ILibavDecoderDecodedImage, ESPMode::ThreadSafe> FLibavDecoderVideoCommon::GetOutput()
{
	if (!bHasPendingOutput)
	{
		return nullptr;
	}

	bHasPendingOutput = false;
	FOutputInfo OutInfo = CurrentOutputInfo;
	CurrentOutputInfo = {};

	// Supported format?
	if (bIsSupportedOutputFormat)
	{
		FLibavDecoderDecodedImage* NewImage = new FLibavDecoderDecodedImage;
		if (NewImage->Setup(Frame, OutInfo))
		{
			TSharedPtr<FLibavDecoderDecodedImage, ESPMode::ThreadSafe> Pict = MakeShareable(NewImage, FLibavDecodedImageDeleter(AsShared()));
			return Pict;
		}
		delete NewImage;
	}
	return nullptr;
}

bool FLibavDecoderVideoCommon::ReturnDecodedFrame(FLibavDecoderDecodedImage* ImageToDelete)
{
	// No-op. Just delete the image.
	return true;
}

void FLibavDecodedImageDeleter::operator()(FLibavDecoderDecodedImage* ImageToDelete)
{
	TSharedPtr<FLibavDecoderVideoCommon, ESPMode::ThreadSafe> Decoder = OwningDecoder.Pin();
	bool bDelete = Decoder.IsValid() ? Decoder->ReturnDecodedFrame(ImageToDelete) : true;
	if (bDelete)
	{
		delete ImageToDelete;
	}
}


/***************************************************************************************************************************************************/

bool ILibavDecoderVideoCommon::IsAvailable(int64 CodecID)
{
	return ILibavDecoder::IsLibAvAvailable() && avcodec_find_decoder((enum AVCodecID)CodecID) != nullptr;
}

TSharedPtr<ILibavDecoderVideoCommon, ESPMode::ThreadSafe> ILibavDecoderVideoCommon::Create(int64 CodecID, ILibavDecoderVideoResourceAllocator* InVideoResourceAllocator, const TMap<FString, FVariant>& InOptions)
{
	TSharedPtr<FLibavDecoderVideoCommon, ESPMode::ThreadSafe> New = MakeShared<FLibavDecoderVideoCommon, ESPMode::ThreadSafe>();
	if (New.IsValid())
	{
		ILibavDecoder::EDecoderError Err = New->Initialize(CodecID, InVideoResourceAllocator, InOptions);
		if (Err != ILibavDecoder::EDecoderError::None)
		{
			New.Reset();
		}
	}
	return New;
}


#else

bool ILibavDecoderVideoCommon::IsAvailable(int64 CodecID)
{
	return false;
}

TSharedPtr<ILibavDecoderVideoCommon, ESPMode::ThreadSafe> ILibavDecoderVideoCommon::Create(int64 CodecID, ILibavDecoderVideoResourceAllocator* InVideoResourceAllocator, const TMap<FString, FVariant>& InOptions)
{
	return nullptr;
}

#endif
