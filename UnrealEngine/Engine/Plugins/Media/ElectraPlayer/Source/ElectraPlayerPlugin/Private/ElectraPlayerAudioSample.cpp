// Copyright Epic Games, Inc. All Rights Reserved.

#include "ElectraPlayerAudioSample.h"

void FElectraPlayerAudioSample::InitializePoolable()
{
}

void FElectraPlayerAudioSample::ShutdownPoolable()
{
	DecoderOutput.Reset();
}

