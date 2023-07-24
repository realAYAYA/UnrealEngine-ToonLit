// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"
#include "MediaPacket.h"
#include "Templates/RefCounting.h"
#include "VideoCommon.h"

#if PLATFORM_WINDOWS

namespace AVEncoder
{

//
// Wrapper for IMFSample, to make it easier to report errors
//
class AVENCODER_API FIMFSampleWrapper
{
public:

	FIMFSampleWrapper(EPacketType InMediaType = EPacketType::Invalid, IMFSample* InSample = nullptr)
		: MediaType(InMediaType)
		, Sample(InSample)
	{
	}

	const IMFSample* GetSample() const
	{
		return Sample;
	}

	IMFSample* GetSample()
	{
		return Sample;
	}

	bool CreateSample();

	FTimespan GetTime() const;

	void SetTime(FTimespan Time);

	FTimespan GetDuration() const;

	void SetDuration(FTimespan Duration);

	bool IsVideoKeyFrame() const;

	int GetBufferCount() const;

	/**
	*
	* Calls the specified function for each buffer the sample contains
	* Signature is "int (int BufferIndex, TArrayView<uint8> Data)
	* The specified function should return true if iteration should continue, or false to finish
	*
	* The return value is what the specified function returned:
	*	true : Iterated through all the buffers
	*	false : Early termination
	*/
	template<typename T>
	bool IterateBuffers(T&& Func)
	{
		int BufferCount = GetBufferCount();
		for (int Idx = 0; Idx < BufferCount; ++Idx)
		{
			TRefCountPtr<IMFMediaBuffer> MediaBuffer = nullptr;
			verify(SUCCEEDED(Sample->GetBufferByIndex(0, MediaBuffer.GetInitReference())));

			BYTE* SrcData = nullptr;
			DWORD MaxLength = 0;
			DWORD CurrentLength = 0;
			verify(SUCCEEDED(MediaBuffer->Lock(&SrcData, &MaxLength, &CurrentLength)));
			bool res = Func(Idx, TArrayView<uint8>(SrcData, CurrentLength));
			verify(SUCCEEDED(MediaBuffer->Unlock()));
			if (!res)
			{
				return false;
			}
		}
		return true;
	}

	bool IsValid() const
	{
		return Sample.IsValid();
	}

	void Reset()
	{
		Sample = nullptr;
	}

	FIMFSampleWrapper Clone() const;

private:
	EPacketType MediaType;
	TRefCountPtr<IMFSample> Sample;
};

}

#endif // PLATFORM_WINDOWS

