// Copyright Epic Games, Inc. All Rights Reserved.

#include "WmfAudioEncoder.h"

#if PLATFORM_WINDOWS

#include "AVEncoder.h"
#include "Logging/LogMacros.h"
#include "MicrosoftCommon.h"
#include "AudioEncoder.h"
#include "IMFSampleWrapper.h"

namespace AVEncoder
{
class FWmfAudioEncoder : public FAudioEncoder
{
public:

	enum class ECodecType
	{
		AAC
	};

	FWmfAudioEncoder(ECodecType CodecType);
	~FWmfAudioEncoder() override;

	const TCHAR* GetName() const override;
	const TCHAR* GetType() const override;
	bool Initialize(const FAudioConfig& InConfig) override;
	void Shutdown() override;
	void Encode(const FAudioFrame& Frame) override;
	FAudioConfig GetConfig() const override;

private:
	bool SetInputType();
	bool SetOutputType();
	bool RetrieveStreamInfo();
	bool StartStreaming();

	FIMFSampleWrapper CreateInputSample(const uint8* SampleData, uint32 Size, FTimespan Timestamp, FTimespan Duration);
	FIMFSampleWrapper GetOutputSample();

	ECodecType CodecType;
	FString Name;
	FString Type;

	FAudioConfig Config;
	TRefCountPtr<IMFTransform> Encoder;
	TRefCountPtr<IMFMediaType> OutputType;
	MFT_INPUT_STREAM_INFO InputStreamInfo = {};
	MFT_OUTPUT_STREAM_INFO OutputStreamInfo = {};

	// Keep this as a member variable, to reuse the memory allocation
	// It's used to convert float audio data to PCM
	Audio::TSampleBuffer<int16> PCM16;
};

//////////////////////////////////////////////////////////////////////////
// FWmfAudioEncoder implementation
//////////////////////////////////////////////////////////////////////////

FWmfAudioEncoder::FWmfAudioEncoder(ECodecType CodecType)
	: CodecType(CodecType)
{
	switch (CodecType)
	{
	case ECodecType::AAC:
		Name = "aac.wmf";
		Type = "aac";
		break;
	default:
		checkNoEntry();
	}
}

FWmfAudioEncoder::~FWmfAudioEncoder()
{
}

const TCHAR* FWmfAudioEncoder::GetName() const
{
	return *Name;
}

const TCHAR* FWmfAudioEncoder::GetType() const
{
	return *Type;
}


namespace
{
	//
	// Utility function to validate if a given encoder parameter has an acceptable value
	//
	bool ValidateValue(const TCHAR* Name, uint32 Value, const TArray<uint32>& SupportedValues, FString& OutErr)
	{
		for(uint32 V : SupportedValues)
		{
			if (Value == V)
			{
				return true;
			}
		}

		OutErr = FString(Name) + TEXT(" must be (");
		for(uint32 V : SupportedValues)
		{
			OutErr += FString::Printf(
				TEXT("%s%u"),
				OutErr.IsEmpty() ? TEXT("") : TEXT(", "),
				V);
		}
		OutErr += FString::Printf(TEXT("). %u was specified."), Value);
		return false;
	}
}

bool FWmfAudioEncoder::Initialize(const FAudioConfig& InConfig)
{

	//
	// Validate config depending on the codec
	//
	if (CodecType == ECodecType::AAC)
	{
		// See  https://docs.microsoft.com/en-us/windows/desktop/medfound/aac-encoder for details
		FString ErrorStr;
		if (
			!ValidateValue(TEXT("AAC Bitrate"), InConfig.Bitrate, {12000*8, 16000*8, 20000*8, 24000*8}, ErrorStr) ||
			!ValidateValue(TEXT("AAC Samplerate"), InConfig.Samplerate, { 44100, 48000, 0 }, ErrorStr) ||
			!ValidateValue(TEXT("AAC NumChannels"), InConfig.NumChannels, {1,2,6}, ErrorStr))
		{
			UE_LOG(LogAVEncoder, Error, TEXT("%s"), *ErrorStr);
			return false;
		}
	}
	else
	{
		checkNoEntry();
		return false;
	}

	UE_LOG(LogAVEncoder, Log, TEXT("AudioEncoder config: %d channels, %d Hz, %.2f Kbps"), InConfig.NumChannels, InConfig.Samplerate, InConfig.Bitrate / 1000.0f);

	Config = InConfig;

	const GUID* CodecGuid=nullptr;
	if (CodecType == ECodecType::AAC)
	{
		CodecGuid = &CLSID_AACMFTEncoder;
	}
	else
	{
		checkNoEntry();
	}

	CHECK_HR(CoCreateInstance(*CodecGuid, nullptr, CLSCTX_INPROC_SERVER, IID_IMFTransform, reinterpret_cast<void**>(&Encoder)));

	//
	// NOTES
	// MP3: The output type must be set before the input. See https://docs.microsoft.com/en-us/windows/win32/medfound/mp3-audio-encoder
	// AAC: Output or input type can be set in either order. Input type first, or output type first.

	if (!SetInputType() || !SetOutputType() || !RetrieveStreamInfo() || !StartStreaming()) 
	{
		Encoder->Release();
		return false;
	}

	return true;
}

bool FWmfAudioEncoder::SetInputType()
{
	TRefCountPtr<IMFMediaType> MediaType;
	CHECK_HR(MFCreateMediaType(MediaType.GetInitReference()));
	CHECK_HR(MediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio));
	CHECK_HR(MediaType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM));
	CHECK_HR(MediaType->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16)); // the only value supported
	CHECK_HR(MediaType->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, Config.Samplerate));
	CHECK_HR(MediaType->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, Config.NumChannels));

	CHECK_HR(Encoder->SetInputType(0, MediaType, 0));

	return true;
}

bool FWmfAudioEncoder::SetOutputType()
{
	CHECK_HR(MFCreateMediaType(OutputType.GetInitReference()));
	CHECK_HR(OutputType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio));

	if (CodecType == ECodecType::AAC)
	{
		CHECK_HR(OutputType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_AAC));
		CHECK_HR(OutputType->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16)); // the only value supported
		CHECK_HR(OutputType->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, Config.Samplerate));
		CHECK_HR(OutputType->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, Config.NumChannels));
		CHECK_HR(OutputType->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, Config.Bitrate / 8));
	}
	else
	{
		checkNoEntry();
		return false;
	}

	CHECK_HR(Encoder->SetOutputType(0, OutputType, 0));

	return true;
}

bool FWmfAudioEncoder::RetrieveStreamInfo()
{
	CHECK_HR(Encoder->GetInputStreamInfo(0, &InputStreamInfo));
	CHECK_HR(Encoder->GetOutputStreamInfo(0, &OutputStreamInfo));

	return true;
}

bool FWmfAudioEncoder::StartStreaming()
{
	CHECK_HR(Encoder->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, 0));
	CHECK_HR(Encoder->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, 0));

	return true;
}

void FWmfAudioEncoder::Shutdown()
{
	// Nothing to do
}

void FWmfAudioEncoder::Encode(const FAudioFrame& Frame)
{
	UE_LOG(LogAVEncoder, Verbose, TEXT("Audio input: time %.3f, duration %.3f, %d samples"), Frame.Timestamp.GetTotalSeconds(), Frame.Duration.GetTotalSeconds(), Frame.Data.GetNumSamples());

	// Convert float audio data to PCM
	PCM16 = Frame.Data;

	FIMFSampleWrapper InputSample = CreateInputSample(
		reinterpret_cast<const uint8*>(PCM16.GetData()),
		PCM16.GetNumSamples()*sizeof(*PCM16.GetData()),
		Frame.Timestamp,
		Frame.Duration);

	if (!InputSample.IsValid())
	{
		return;
	}

	CHECK_HR_VOID(Encoder->ProcessInput(0, InputSample.GetSample(), 0));

	while (true)
	{
		FIMFSampleWrapper OutputSample = GetOutputSample();
		if (!OutputSample.IsValid())
		{
			break;
		}

		DWORD OutputSize;
		CHECK_HR_VOID(OutputSample.GetSample()->GetTotalLength(&OutputSize));

		UE_LOG(LogAVEncoder, Verbose, TEXT("Audio encoded: time %.3f, duration %.3f, size %d"), OutputSample.GetTime().GetTotalSeconds(), OutputSample.GetDuration().GetTotalSeconds(), OutputSize);

		FMediaPacket Packet(EPacketType::Audio);
		Packet.Timestamp = OutputSample.GetTime();
		Packet.Duration = OutputSample.GetDuration();
		OutputSample.IterateBuffers([&Packet](int BufferIdx, TArrayView<uint8> Data)
		{
			Packet.Data.Append(Data.GetData(), Data.Num());
			return false; // Terminate, because we only want 1 buffer
		});

		OnEncodedAudioFrame(Packet);
	}
}

FIMFSampleWrapper FWmfAudioEncoder::CreateInputSample(const uint8* SampleData, uint32 Size, FTimespan Timestamp, FTimespan Duration)
{
	FIMFSampleWrapper Sample = { EPacketType::Audio };

	if (!Sample.CreateSample())
	{
		return {};
	}

	int32 BufferSize = FMath::Max<int32>(InputStreamInfo.cbSize, Size);
	uint32 Alignment = InputStreamInfo.cbAlignment > 1 ? InputStreamInfo.cbAlignment - 1 : 0;
	TRefCountPtr<IMFMediaBuffer> WmfBuffer;
	CHECK_HR_DEFAULT(MFCreateAlignedMemoryBuffer(BufferSize, Alignment, WmfBuffer.GetInitReference()));

	uint8* Dst = nullptr;
	CHECK_HR_DEFAULT(WmfBuffer->Lock(&Dst, nullptr, nullptr));
	FMemory::Memcpy(Dst, SampleData, Size);
	CHECK_HR_DEFAULT(WmfBuffer->Unlock());

	CHECK_HR_DEFAULT(WmfBuffer->SetCurrentLength(Size));

	CHECK_HR_DEFAULT(Sample.GetSample()->AddBuffer(WmfBuffer));
	Sample.SetTime(Timestamp);
	Sample.SetDuration(Duration);

	return Sample;
}

FIMFSampleWrapper FWmfAudioEncoder::GetOutputSample()
{
	bool bFlag1 = OutputStreamInfo.dwFlags&MFT_OUTPUT_STREAM_PROVIDES_SAMPLES;
	bool bFlag2 = OutputStreamInfo.dwFlags&MFT_OUTPUT_STREAM_CAN_PROVIDE_SAMPLES;

	// Right now, we are always creating our samples, so make sure MFT will be ok with that
	verify((bFlag1 == false && bFlag2 == false) || (bFlag1 == false && bFlag2 == true));

	FIMFSampleWrapper Sample{ EPacketType::Audio };

	//
	// Create output sample
	//
	{
		if (!Sample.CreateSample())
		{
			return {};
		}

		TRefCountPtr<IMFMediaBuffer> Buffer = nullptr;
		uint32 Alignment = OutputStreamInfo.cbAlignment > 1 ? OutputStreamInfo.cbAlignment - 1 : 0;
		CHECK_HR_DEFAULT(MFCreateAlignedMemoryBuffer(OutputStreamInfo.cbSize, Alignment, Buffer.GetInitReference()));
		CHECK_HR_DEFAULT(Sample.GetSample()->AddBuffer(Buffer));
	}


	MFT_OUTPUT_DATA_BUFFER Output = {};
	Output.pSample = Sample.GetSample();

	DWORD Status = 0;
	HRESULT Res = Encoder->ProcessOutput(0, 1, &Output, &Status);
	TRefCountPtr<IMFCollection> Events = Output.pEvents; // unsure this is released
	if (Res == MF_E_TRANSFORM_NEED_MORE_INPUT || !Sample.IsValid())
	{
		return {};
	}
	else if (Res == MF_E_TRANSFORM_STREAM_CHANGE)
	{
		if (!SetOutputType())
		{
			return {};
		}

		return GetOutputSample();
	}
	else if (SUCCEEDED(Res))
	{
		return Sample;
	}
	else
	{
		return {};
	}

}

FAudioConfig FWmfAudioEncoder::GetConfig() const
{
	return Config;
}

//////////////////////////////////////////////////////////////////////////
// FWmfAudioEncoderFactory implementation
//////////////////////////////////////////////////////////////////////////

FWmfAudioEncoderFactory::FWmfAudioEncoderFactory()
{
}

FWmfAudioEncoderFactory::~FWmfAudioEncoderFactory()
{
}


const TCHAR* FWmfAudioEncoderFactory::GetName() const
{
	return TEXT("wmf");
}

TArray<FString> FWmfAudioEncoderFactory::GetSupportedCodecs() const
{
	TArray<FString> Codecs;
	Codecs.Add(TEXT("aac"));
	return Codecs;
}

TUniquePtr<FAudioEncoder> FWmfAudioEncoderFactory::CreateEncoder(const FString& Codec)
{
	if (Codec == "aac")
	{
		return TUniquePtr<FAudioEncoder>(new FWmfAudioEncoder(FWmfAudioEncoder::ECodecType::AAC));
	}
	else
	{
		UE_LOG(LogAVEncoder, Error, TEXT("FWmfAudioEncoderFactory doesn't support the %s codec"), *Codec);
		return nullptr;
	}
}

}

#endif //PLATFORM_WINDOWS

