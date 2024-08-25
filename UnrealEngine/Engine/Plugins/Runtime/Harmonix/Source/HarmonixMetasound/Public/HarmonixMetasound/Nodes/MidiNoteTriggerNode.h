// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetasoundNodeInterface.h"

#include "HarmonixMetasound/Common.h"

namespace HarmonixMetasound::Nodes::MidiNoteTriggerNode
{
	const HARMONIXMETASOUND_API Metasound::FNodeClassName& GetClassName();
	HARMONIXMETASOUND_API int32 GetCurrentMajorVersion();

	namespace Inputs
	{
		DECLARE_METASOUND_PARAM_ALIAS(Enable);
		DECLARE_METASOUND_PARAM_ALIAS(MidiStream);
	}

	namespace Outputs
	{
		DECLARE_METASOUND_PARAM_ALIAS(NoteOnTrigger);
		DECLARE_METASOUND_PARAM_ALIAS(NoteOffTrigger);
		DECLARE_METASOUND_PARAM_ALIAS(MidiNoteNumber);
		DECLARE_METASOUND_PARAM_ALIAS(MidiVelocity);
	}
}