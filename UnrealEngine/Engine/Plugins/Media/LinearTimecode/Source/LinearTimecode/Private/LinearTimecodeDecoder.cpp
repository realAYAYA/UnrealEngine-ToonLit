// Copyright Epic Games, Inc. All Rights Reserved.

#include "LinearTimecodeDecoder.h"
#include "HAL/UnrealMemory.h"

namespace FLinearTimecodeDecoderHelpers
{
	bool ValidateTimecode(const FDropTimecode& InTimecode)
	{
		if ((InTimecode.Timecode.Hours < 0) || (InTimecode.Timecode.Hours > 23))
		{
			return false;
		}

		if ((InTimecode.Timecode.Minutes < 0) || (InTimecode.Timecode.Minutes > 59))
		{
			return false;
		}

		if ((InTimecode.Timecode.Seconds < 0) || (InTimecode.Timecode.Seconds > 59))
		{
			return false;
		}

		if ((InTimecode.Timecode.Frames < 0) || (InTimecode.Timecode.Frames > 29))
		{
			return false;
		}

		return true;
	}

	constexpr uint16 Preamble = 0x3ffd;
	constexpr uint16 PreambleReversed = 0xbffc;
}

FLinearTimecodeDecoder::FLinearTimecodeDecoder()
{
	Reset();
}

void FLinearTimecodeDecoder::DecodeBitStream(uint16* InBitStream, EDecodePattern* InDecodePattern, int32 InForward, FDropTimecode& OutTimeCode)
{
	FMemory::Memset(&OutTimeCode, 0, sizeof(FDropTimecode));
	OutTimeCode.bRunningForward = InForward != 0;

	auto DecodeBits = [InBitStream](EDecodePattern* inDecodePatern)
	{
		return (InBitStream[inDecodePatern->Index] & inDecodePatern->Mask) ? inDecodePatern->Addition : 0;
	};

	while (InDecodePattern->Type != EDecodePattern::EDecodeType::End)
	{
		switch (InDecodePattern->Type)
		{
		case EDecodePattern::EDecodeType::Hours:
			OutTimeCode.Timecode.Hours += DecodeBits(InDecodePattern);
			break;
		case EDecodePattern::EDecodeType::Minutes:
			OutTimeCode.Timecode.Minutes += DecodeBits(InDecodePattern);
			break;
		case EDecodePattern::EDecodeType::Seconds:
			OutTimeCode.Timecode.Seconds += DecodeBits(InDecodePattern);
			break;
		case EDecodePattern::EDecodeType::Frames:
			OutTimeCode.Timecode.Frames += DecodeBits(InDecodePattern);
			break;
		case EDecodePattern::EDecodeType::DropFrame:
			OutTimeCode.Timecode.bDropFrameFormat = (InBitStream[InDecodePattern->Index] & InDecodePattern->Mask) != 0;
			break;
		case EDecodePattern::EDecodeType::ColorFrame:
			OutTimeCode.bColorFraming = (InBitStream[InDecodePattern->Index] & InDecodePattern->Mask) != 0;
			break;
		case EDecodePattern::EDecodeType::FrameRate:
			if (InForward)
			{
				if (FrameMax < OutTimeCode.Timecode.Frames)
				{
					FrameMax = OutTimeCode.Timecode.Frames;
				}
				if (FrameMax > OutTimeCode.Timecode.Frames)
				{
					FrameRate = FrameMax;
				}
				OutTimeCode.FrameRate = FrameRate;
			}
			else
			{
				if (FrameMax < OutTimeCode.Timecode.Frames && FrameRate < OutTimeCode.Timecode.Frames)
				{
					FrameRate = OutTimeCode.Timecode.Frames;
				}
				FrameMax = OutTimeCode.Timecode.Frames;
			}
			OutTimeCode.FrameRate = FrameRate;
		}
		++InDecodePattern;
	}
}

FLinearTimecodeDecoder::EDecodePattern FLinearTimecodeDecoder::ForwardPattern[] = {

	{ EDecodePattern::EDecodeType::Hours, 3, 0x8000, 1 },
	{ EDecodePattern::EDecodeType::Hours, 3, 0x4000, 2 },
	{ EDecodePattern::EDecodeType::Hours, 3, 0x2000, 4 },
	{ EDecodePattern::EDecodeType::Hours, 3, 0x1000, 8 },
	{ EDecodePattern::EDecodeType::Hours, 3, 0x0080, 10 },
	{ EDecodePattern::EDecodeType::Hours, 3, 0x0040, 20 },

	{ EDecodePattern::EDecodeType::Minutes, 2, 0x8000, 1 },
	{ EDecodePattern::EDecodeType::Minutes, 2, 0x4000, 2 },
	{ EDecodePattern::EDecodeType::Minutes, 2, 0x2000, 4 },
	{ EDecodePattern::EDecodeType::Minutes, 2, 0x1000, 8 },
	{ EDecodePattern::EDecodeType::Minutes, 2, 0x0080, 10 },
	{ EDecodePattern::EDecodeType::Minutes, 2, 0x0040, 20 },
	{ EDecodePattern::EDecodeType::Minutes, 2, 0x0020, 40 },

	{ EDecodePattern::EDecodeType::Seconds, 1, 0x8000, 1 },
	{ EDecodePattern::EDecodeType::Seconds, 1, 0x4000, 2 },
	{ EDecodePattern::EDecodeType::Seconds, 1, 0x2000, 4 },
	{ EDecodePattern::EDecodeType::Seconds, 1, 0x1000, 8 },
	{ EDecodePattern::EDecodeType::Seconds, 1, 0x0080, 10 },
	{ EDecodePattern::EDecodeType::Seconds, 1, 0x0040, 20 },
	{ EDecodePattern::EDecodeType::Seconds, 1, 0x0020, 40 },

	{ EDecodePattern::EDecodeType::Frames, 0, 0x8000, 1 },
	{ EDecodePattern::EDecodeType::Frames, 0, 0x4000, 2 },
	{ EDecodePattern::EDecodeType::Frames, 0, 0x2000, 4 },
	{ EDecodePattern::EDecodeType::Frames, 0, 0x1000, 8 },
	{ EDecodePattern::EDecodeType::Frames, 0, 0x0080, 10 },
	{ EDecodePattern::EDecodeType::Frames, 0, 0x0040, 20 },

	{ EDecodePattern::EDecodeType::FrameRate, 0, 0, 0 },

	{ EDecodePattern::EDecodeType::DropFrame, 0, 0x0020, 1 },
	{ EDecodePattern::EDecodeType::ColorFrame, 0, 0x0010, 1 },

	{ EDecodePattern::EDecodeType::End, 0, 0, 0},
};

FLinearTimecodeDecoder::EDecodePattern FLinearTimecodeDecoder::BackwardPattern[] = {

	{ EDecodePattern::EDecodeType::Hours, 1, 0x0001, 1 },
	{ EDecodePattern::EDecodeType::Hours, 1, 0x0002, 2 },
	{ EDecodePattern::EDecodeType::Hours, 1, 0x0004, 4 },
	{ EDecodePattern::EDecodeType::Hours, 1, 0x0008, 8 },
	{ EDecodePattern::EDecodeType::Hours, 1, 0x0100, 10 },
	{ EDecodePattern::EDecodeType::Hours, 1, 0x0200, 20 },

	{ EDecodePattern::EDecodeType::Minutes, 2, 0x0001, 1 },
	{ EDecodePattern::EDecodeType::Minutes, 2, 0x0002, 2 },
	{ EDecodePattern::EDecodeType::Minutes, 2, 0x0004, 4 },
	{ EDecodePattern::EDecodeType::Minutes, 2, 0x0008, 8 },
	{ EDecodePattern::EDecodeType::Minutes, 2, 0x0100, 10 },
	{ EDecodePattern::EDecodeType::Minutes, 2, 0x0200, 20 },
	{ EDecodePattern::EDecodeType::Minutes, 2, 0x0400, 40 },

	{ EDecodePattern::EDecodeType::Seconds, 3, 0x0001, 1 },
	{ EDecodePattern::EDecodeType::Seconds, 3, 0x0002, 2 },
	{ EDecodePattern::EDecodeType::Seconds, 3, 0x0004, 4 },
	{ EDecodePattern::EDecodeType::Seconds, 3, 0x0008, 8 },
	{ EDecodePattern::EDecodeType::Seconds, 3, 0x0100, 10 },
	{ EDecodePattern::EDecodeType::Seconds, 3, 0x0200, 20 },
	{ EDecodePattern::EDecodeType::Seconds, 3, 0x0400, 40 },

	{ EDecodePattern::EDecodeType::Frames, 4, 0x0001, 1 },
	{ EDecodePattern::EDecodeType::Frames, 4, 0x0002, 2 },
	{ EDecodePattern::EDecodeType::Frames, 4, 0x0004, 4 },
	{ EDecodePattern::EDecodeType::Frames, 4, 0x0008, 8 },
	{ EDecodePattern::EDecodeType::Frames, 4, 0x0100, 10 },
	{ EDecodePattern::EDecodeType::Frames, 4, 0x0200, 20 },

	{ EDecodePattern::EDecodeType::FrameRate, 0, 0, 0 },

	{ EDecodePattern::EDecodeType::DropFrame, 4, 0x0400, 1 },
	{ EDecodePattern::EDecodeType::ColorFrame, 4, 0x0800, 1 },
	
	{ EDecodePattern::EDecodeType::End, 0, 0, 0 },
};

void FLinearTimecodeDecoder::ShiftAndInsert(uint16* InBitStream, bool InBit) const
{
	InBitStream[0] = (InBitStream[0] << 1) + ((InBitStream[1] & 0x8000) ? 1 : 0);
	InBitStream[1] = (InBitStream[1] << 1) + ((InBitStream[2] & 0x8000) ? 1 : 0);
	InBitStream[2] = (InBitStream[2] << 1) + ((InBitStream[3] & 0x8000) ? 1 : 0);
	InBitStream[3] = (InBitStream[3] << 1) + ((InBitStream[4] & 0x8000) ? 1 : 0);
	InBitStream[4] = (InBitStream[4] << 1) + ((InBitStream[5] & 0x8000) ? 1 : 0);
	InBitStream[5] = (InBitStream[5] << 1) + (InBit ? 1 : 0);
}

bool FLinearTimecodeDecoder::HasCompleteFrame(uint16* InBitStream) const
{
	return ((InBitStream[0] == 0x3ffd) && (InBitStream[5] == 0x3ffd)) 
		|| ((InBitStream[0] == 0xbffc) && (InBitStream[5] == 0xbffc));
}

bool FLinearTimecodeDecoder::DecodeFrame(uint16* InBitStream, FDropTimecode& OutTimeCode)
{
	constexpr uint16 Preamble = FLinearTimecodeDecoderHelpers::Preamble;
	constexpr uint16 PreambleReversed = FLinearTimecodeDecoderHelpers::PreambleReversed;

	// forward stream
	if (InBitStream[0] == Preamble && InBitStream[5] == Preamble)
	{
		// Adding 1 to InBitStream since the decoder doesn't look at the preamble.
		DecodeBitStream(InBitStream+1, ForwardPattern, 1, OutTimeCode);
		return FLinearTimecodeDecoderHelpers::ValidateTimecode(OutTimeCode);
	}

	// backward stream
	if (InBitStream[0] == PreambleReversed && InBitStream[5] == PreambleReversed)
	{
		// Not adding +1 here since the backward pattern expects the reversed postamble to be there.
		DecodeBitStream(InBitStream, BackwardPattern, 0, OutTimeCode);
		return FLinearTimecodeDecoderHelpers::ValidateTimecode(OutTimeCode);
	}

	return false;
}

void FLinearTimecodeDecoder::Reset(void)
{
	Center = 0.0f;
	bCurrent = false;

	bFlip = false;
	Clock = 0;
	Cycles = 0;

	FrameMax = 0;
	FrameRate = 0;

	FMemory::Memset(TimecodeBits, 0, sizeof(TimecodeBits));

	MinSamplesPerEdge = 1;
	MaxSamplesPerEdge = INT_MAX;
}

int32 FLinearTimecodeDecoder::AdjustCycles(int32 InClock) const
{
	return (Cycles + InClock * 4) / 5;
}

bool FLinearTimecodeDecoder::Sample(float InSample, FDropTimecode& OutTimeCode)
{
	++Clock;

	if ((!bCurrent && InSample > Center) || (bCurrent && InSample < Center))
	{		
		bCurrent = !bCurrent;

		if ((Clock < MinSamplesPerEdge) || (Clock > MaxSamplesPerEdge))
		{
			Clock = 0;
			return false;
		}

		if (bFlip)
		{
			// second half 1-bit

			bFlip = false;
			Cycles = AdjustCycles(Clock);
			Clock = 0;
		}
		else
		{
			if (Clock < (3 * Cycles) / 4)
			{
				// first half 1-bit. 
				// Don't call AdjustCycles or we'd measure a half bit instead of a full bit.

				ShiftAndInsert(TimecodeBits, true);
				bFlip = true;
			}
			else
			{
				// full 0-bit

				ShiftAndInsert(TimecodeBits, false);
				Cycles = AdjustCycles(Clock);
				Clock = 0;
			}

			return DecodeFrame(TimecodeBits, OutTimeCode);
		}
	}

	return false;
}
