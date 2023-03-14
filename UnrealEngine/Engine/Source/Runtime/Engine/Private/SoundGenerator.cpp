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
		NumSamplesToGenerate = FMath::Min(NumSamples, GetDesiredNumSamplesToRenderPerCallback());
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


