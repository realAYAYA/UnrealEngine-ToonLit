// Copyright Epic Games, Inc. All Rights Reserved.

#include "libav_Decoder_AAC.h"

/***************************************************************************************************************************************************/
#if WITH_LIBAV
extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavutil/dict.h>
}

/***************************************************************************************************************************************************/

class FLibavDecoderAAC : public ILibavDecoderAAC
{
public:
	FLibavDecoderAAC() = default;
	virtual ~FLibavDecoderAAC();

	int32 GetLastLibraryError() const override
	{ return LastError; }

	EDecoderError DecodeAccessUnit(const FInputAU& InInputAccessUnit) override;
	EDecoderError SendEndOfData() override;
	void Reset() override;
	ILibavDecoder::EOutputStatus HaveOutput(FOutputInfo& OutInfo) override;
	bool GetOutputAsS16(int16* OutInterleavedBuffer, int32 OutBufferSizeInBytes) override;

	EDecoderError Create(const TArray<uint8>& InCodecSpecificData);

private:
	void InternalReset();

	AVCodec* Codec = nullptr;
	AVCodecContext* Context = nullptr;
	AVPacket* Packet = nullptr;
	int32 PacketBufferSize = 0;
	struct AVFrame* Frame = nullptr;
	int32 LastError = 0;
	bool bHasPendingOutput = false;
	TArray<uint8> CodecSpecificData;
	FOutputInfo CurrentOutputInfo;
	bool bReinitNeeded = false;
};

FLibavDecoderAAC::~FLibavDecoderAAC()
{
	InternalReset();
}


void FLibavDecoderAAC::InternalReset()
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
	Codec = nullptr;
	bHasPendingOutput = false;
	bReinitNeeded = false;
	LastError = 0;
}

ILibavDecoder::EDecoderError FLibavDecoderAAC::Create(const TArray<uint8>& InCodecSpecificData)
{
	CodecSpecificData = InCodecSpecificData;
	InternalReset();
	Codec = avcodec_find_decoder(AV_CODEC_ID_AAC);
	if (Codec)
	{
		Context = avcodec_alloc_context3(Codec);
		if (Context)
		{
			// Set the codec specific data as extra data in the context.
			Context->extradata_size = CodecSpecificData.Num();
			Context->extradata = static_cast<uint8_t*>(av_mallocz(Context->extradata_size + AV_INPUT_BUFFER_PADDING_SIZE));
			FMemory::Memcpy(Context->extradata, CodecSpecificData.GetData(), Context->extradata_size);
			//Context->request_sample_fmt = AV_SAMPLE_FMT_S16;
			//Context->request_channel_layout = AV_CH_LAYOUT_NATIVE;
			AVDictionary* Opts = nullptr;
			LastError = avcodec_open2(Context, Codec, &Opts);
			av_dict_free(&Opts);
			return LastError == 0 ? ILibavDecoder::EDecoderError::None : ILibavDecoder::EDecoderError::Error;
		}
		return ILibavDecoder::EDecoderError::NotSupported;
	}
	return ILibavDecoder::EDecoderError::NotSupported;
}

void FLibavDecoderAAC::Reset()
{
	InternalReset();
}



ILibavDecoder::EDecoderError FLibavDecoderAAC::DecodeAccessUnit(const ILibavDecoderAAC::FInputAU& InInputAccessUnit)
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
	FMemory::Memcpy(Packet->data, InInputAccessUnit.Data, (int)InInputAccessUnit.DataSize);
	Context->reordered_opaque = InInputAccessUnit.UserValue;
	//Context->request_channel_layout = AV_CH_LAYOUT_NATIVE;
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

ILibavDecoder::EDecoderError FLibavDecoderAAC::SendEndOfData()
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


ILibavDecoder::EOutputStatus FLibavDecoderAAC::HaveOutput(ILibavDecoderAAC::FOutputInfo& OutInfo)
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
		LastError = avcodec_receive_frame(Context, Frame);
		if (LastError == 0)
		{
			CurrentOutputInfo.NumChannels = Frame->channels;
			CurrentOutputInfo.NumSamples = Frame->nb_samples;
			CurrentOutputInfo.SampleRate = Frame->sample_rate;
			CurrentOutputInfo.PTS = Frame->pts;
			CurrentOutputInfo.UserValue = Frame->reordered_opaque;
			CurrentOutputInfo.ChannelMask = Frame->channel_layout;
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

bool FLibavDecoderAAC::GetOutputAsS16(int16* OutInterleavedBuffer, int32 OutBufferSizeInBytes)
{
	if (!bHasPendingOutput)
	{
		return false;
	}

	bHasPendingOutput = false;
	int32 Format = Frame->format;
	int32 Count = Frame->linesize[0];

	// Already the desired format?
	if (Format == AV_SAMPLE_FMT_S16)
	{
		if (Frame->data[0])
		{
			FMemory::Memcpy(OutInterleavedBuffer, Frame->data[0], Count < OutBufferSizeInBytes ? Count : OutBufferSizeInBytes);
		}
		return Frame->data[0] != nullptr;
	}
	else if (Format == AV_SAMPLE_FMT_FLT)
	{
		int32 MaxSamples = OutBufferSizeInBytes / CurrentOutputInfo.NumChannels / sizeof(int16);
		MaxSamples = CurrentOutputInfo.NumSamples < MaxSamples ? CurrentOutputInfo.NumSamples : MaxSamples;
		const float* Src = (const float*)Frame->data[0];
		if (Src)
		{
			for(int32 i=0; i<MaxSamples; ++i)
			{
				for(int32 j=0; j<CurrentOutputInfo.NumChannels; ++j)
				{
					int32 s = *Src++ * 32768.0f;
					*OutInterleavedBuffer++ = (int16)(s < -32768 ? -32768 : s > 32767 ? 32767 : s);
				}
			}
		}
		return Src != nullptr;
	}
	else if (Format == AV_SAMPLE_FMT_S16P)
	{
		int32 MaxSamples = OutBufferSizeInBytes / CurrentOutputInfo.NumChannels / sizeof(int16);
		MaxSamples = CurrentOutputInfo.NumSamples < MaxSamples ? CurrentOutputInfo.NumSamples : MaxSamples;
		for(int32 i=0; i<CurrentOutputInfo.NumChannels; ++i)
		{
			const int16* Src = (const int16*)Frame->data[i];
			if (!Src)
			{
				return false;
			}
			int16* Out = OutInterleavedBuffer + i;
			for(int32 j=0;j<MaxSamples;++j)
			{
				*Out = *Src++;
				Out += CurrentOutputInfo.NumChannels;
			}
		}
		return true;
	}
	else if (Format == AV_SAMPLE_FMT_FLTP)
	{
		int32 MaxSamples = OutBufferSizeInBytes / CurrentOutputInfo.NumChannels / sizeof(int16);
		MaxSamples = CurrentOutputInfo.NumSamples < MaxSamples ? CurrentOutputInfo.NumSamples : MaxSamples;
		for(int32 i=0; i<CurrentOutputInfo.NumChannels; ++i)
		{
			const float* Src = (const float*)Frame->data[i];
			if (!Src)
			{
				return false;
			}
			int16* Out = OutInterleavedBuffer + i;
			for(int32 j=0;j<MaxSamples;++j)
			{
				int32 s = *Src++ * 32768.0f;
				*Out = (int16)(s < -32768 ? -32768 : s > 32767 ? 32767 : s);
				Out += CurrentOutputInfo.NumChannels;
			}
		}
		return true;
	}
	return false;
}


/***************************************************************************************************************************************************/

bool ILibavDecoderAAC::IsAvailable()
{
	return ILibavDecoder::IsLibAvAvailable() && avcodec_find_decoder(AV_CODEC_ID_AAC) != nullptr;
}

TSharedPtr<ILibavDecoderAAC, ESPMode::ThreadSafe> ILibavDecoderAAC::Create(const TArray<uint8>& InCodecSpecificData)
{
	TSharedPtr<FLibavDecoderAAC, ESPMode::ThreadSafe> New = MakeShared<FLibavDecoderAAC, ESPMode::ThreadSafe>();
	if (New.IsValid())
	{
		ILibavDecoder::EDecoderError Err = New->Create(InCodecSpecificData);
		if (Err != ILibavDecoder::EDecoderError::None)
		{
			New.Reset();
		}
	}
	return New;
}

#else

bool ILibavDecoderAAC::IsAvailable()
{
	// Call common method to have it print an appropriate not-available message.
	ILibavDecoder::LogLibraryNeeded();
	return false;
}

TSharedPtr<ILibavDecoderAAC, ESPMode::ThreadSafe> ILibavDecoderAAC::Create(const TArray<uint8>& InCodecSpecificData)
{
	return nullptr;
}

#endif
/***************************************************************************************************************************************************/
