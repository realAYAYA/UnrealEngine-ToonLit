// Copyright Epic Games, Inc. All Rights Reserved.
#include "MetasoundTrigger.h"

#include "MetasoundDataReference.h"
#include "MetasoundDataTypeRegistrationMacro.h"
#include "MetasoundOperatorSettings.h"


namespace Metasound
{
	FTrigger::FTrigger(const FOperatorSettings& InSettings, bool bShouldTrigger)
	:	NumFramesPerBlock(InSettings.GetNumFramesPerBlock())
	,	SampleRate(InSettings.GetSampleRate())
	{
		if (bShouldTrigger)
		{
			TriggerFrame(0);
		}
	}

	FTrigger::FTrigger(const FOperatorSettings& InSettings, int32 InFrameToTrigger)
	:	NumFramesPerBlock(InSettings.GetNumFramesPerBlock())
	,	SampleRate(InSettings.GetSampleRate())
	{
		TriggerFrame(InFrameToTrigger);
	}

	FTrigger::FTrigger(const FOperatorSettings& InSettings)
		: FTrigger(InSettings, false)
	{}

	void FTrigger::TriggerFrame(int32 InFrameToTrigger)
	{
		// Insert trigger frame index into sorted triggered frames
		TriggeredFrames.Insert(InFrameToTrigger, Algo::LowerBound(TriggeredFrames, InFrameToTrigger));

		if (InFrameToTrigger < NumFramesPerBlock)
		{
			UpdateLastTriggerIndexInBlock();
		}

		bHasTrigger = true;
	}

	void FTrigger::AdvanceBlock()
	{
		Advance(NumFramesPerBlock);
	}

	void FTrigger::Advance(int32 InNumFrames)
	{
		if (bHasTrigger)
		{
			const int32 NumToRemove = Algo::LowerBound(TriggeredFrames, InNumFrames);

			if (NumToRemove)
			{
				TriggeredFrames.RemoveAt(0, NumToRemove, false /* bAllowShrinking */);
			}

			const int32 Num = TriggeredFrames.Num();

			if (Num > 0)
			{
				for (int32 i = 0; i < Num; i++)
				{
					TriggeredFrames[i] -= InNumFrames;
				}
			}
			else
			{
				bHasTrigger = false;
			}

			UpdateLastTriggerIndexInBlock();
		}
	}

	int32 FTrigger::Num() const
	{
		return TriggeredFrames.Num();
	}

	int32 FTrigger::NumTriggeredInBlock() const
	{
		return LastTriggerIndexInBlock;
	}

	int32 FTrigger::operator[](int32 InTriggerIndex) const
	{
		return TriggeredFrames[InTriggerIndex];
	}

	int32 FTrigger::First() const
	{
		return TriggeredFrames.IsEmpty() ? INDEX_NONE : TriggeredFrames[0];
	}

	int32 FTrigger::Last() const
	{
		return TriggeredFrames.IsEmpty() ? INDEX_NONE : TriggeredFrames.Last();
	}

	bool FTrigger::IsTriggered() const
	{
		return bHasTrigger;
	}

	bool FTrigger::IsTriggeredInBlock() const
	{
		return LastTriggerIndexInBlock > 0;
	}

	FTrigger::operator bool() const 
	{
		return IsTriggeredInBlock();
	}

	void FTrigger::Reset()
	{
		TriggeredFrames.Reset();
		LastTriggerIndexInBlock = 0;
		bHasTrigger = false;
	}

	void FTrigger::RemoveAfter(int32 InStartingFrame)
	{
		if (InStartingFrame <= 0)
		{
			Reset();
		}
		else if (TriggeredFrames.Num() > 0)
		{
			// Algo::UpperBound Returns index of first element > InStartingFrame
			int32 BeginningIndex = Algo::UpperBound(TriggeredFrames, InStartingFrame);
			int32 NumToRemove = TriggeredFrames.Num() - BeginningIndex;

			if (NumToRemove > 0)
			{
				TriggeredFrames.RemoveAt(BeginningIndex, NumToRemove);
				UpdateLastTriggerIndexInBlock();
				bHasTrigger = TriggeredFrames.Num() > 0;
			}
		}
	}

	void FTrigger::UpdateLastTriggerIndexInBlock()
	{
		LastTriggerIndexInBlock = Algo::LowerBound(TriggeredFrames, NumFramesPerBlock);
	}
}
