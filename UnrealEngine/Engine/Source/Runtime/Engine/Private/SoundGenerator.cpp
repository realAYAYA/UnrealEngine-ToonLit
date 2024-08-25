// Copyright Epic Games, Inc. All Rights Reserved.

#include "Sound/SoundGenerator.h"
#include "HAL/LowLevelMemTracker.h"

ISoundGenerator::ISoundGenerator()
{
}

ISoundGenerator::~ISoundGenerator()
{
}

int32 ISoundGenerator::GetNextBuffer(float* OutAudio, int32 NumSamples, bool bRequireNumberSamples)
{
	LLM_SCOPE(ELLMTag::AudioSynthesis);

	PumpPendingMessages();

	int32 NumSamplesToGenerate = NumSamples;
	if (!bRequireNumberSamples)
	{
		const int32 NewNumSamplesToGenerate = FMath::Min(NumSamples, GetDesiredNumSamplesToRenderPerCallback());
		if (const int32 Diff = NumSamples - NewNumSamplesToGenerate)
		{
			FMemory::Memzero(&OutAudio[NewNumSamplesToGenerate], Diff * sizeof(float));
		}
		NumSamplesToGenerate = NewNumSamplesToGenerate;
	}

	return OnGenerateAudio(OutAudio, NumSamplesToGenerate);
}

void ISoundGenerator::SynthCommand(TUniqueFunction<void()> Command)
{
	LLM_SCOPE(ELLMTag::AudioSynthesis);

	CommandQueue.Enqueue(MoveTemp(Command));
}

void ISoundGenerator::PumpPendingMessages()
{
	TUniqueFunction<void()> Command;
	while (CommandQueue.Dequeue(Command))
	{
		Command();
	}
}


