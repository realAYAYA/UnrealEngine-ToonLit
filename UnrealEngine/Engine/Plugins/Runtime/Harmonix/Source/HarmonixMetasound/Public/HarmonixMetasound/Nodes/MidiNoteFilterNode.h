// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetasoundNodeInterface.h"

#include "HarmonixMetasound/Common.h"

namespace HarmonixMetasound::Nodes::MidiNoteFilter
{
	HARMONIXMETASOUND_API Metasound::FNodeClassName GetClassName();
	HARMONIXMETASOUND_API int32 GetCurrentMajorVersion();

	namespace Inputs
	{
		DECLARE_METASOUND_PARAM_ALIAS(Enable);
		DECLARE_METASOUND_PARAM_ALIAS(MidiStream);
		DECLARE_METASOUND_PARAM_ALIAS(MinNoteNumber);
		DECLARE_METASOUND_PARAM_ALIAS(MaxNoteNumber);
		DECLARE_METASOUND_PARAM_ALIAS(MinVelocity);
		DECLARE_METASOUND_PARAM_ALIAS(MaxVelocity);
		DECLARE_METASOUND_PARAM_EXTERN(IncludeOtherEvents);
	}

	namespace Outputs
	{
		DECLARE_METASOUND_PARAM_ALIAS(MidiStream);
	}
}
