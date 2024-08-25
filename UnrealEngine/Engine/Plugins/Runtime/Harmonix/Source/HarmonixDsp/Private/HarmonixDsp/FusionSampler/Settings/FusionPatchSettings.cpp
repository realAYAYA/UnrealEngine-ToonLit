// Copyright Epic Games, Inc. All Rights Reserved.
#include "HarmonixDsp/FusionSampler/Settings/FusionPatchSettings.h"

FFusionPatchSettings::FFusionPatchSettings()
	: VolumeDb(0.0f)
	, DownPitchBendCents(-200.0f)
	, UpPitchBendCents(200.0f)
	, FineTuneCents(0.0f)
	, StartPointOffsetMs(0.0f)
	, MaxVoices(32)
	, KeyzoneSelectMode(EKeyzoneSelectMode::Layers)
{
	Adsr[0].Target = EAdsrTarget::Volume;
	Adsr[1].Target = EAdsrTarget::FilterFreq;
	Lfo[0].Target = ELfoTarget::Pan;
	Lfo[1].Target = ELfoTarget::Pitch;
}