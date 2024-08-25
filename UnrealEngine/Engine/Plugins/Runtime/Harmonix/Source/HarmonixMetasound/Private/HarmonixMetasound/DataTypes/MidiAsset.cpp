// Copyright Epic Games, Inc. All Rights Reserved.

#include "HarmonixMetasound/DataTypes/MidiAsset.h"

#include "MetasoundDataTypeRegistrationMacro.h"

REGISTER_METASOUND_DATATYPE(HarmonixMetasound::FMidiAsset, "MIDIAsset", Metasound::ELiteralType::UObjectProxy, UMidiFile);

namespace HarmonixMetasound
{
	using namespace Metasound;

	FMidiAsset::FMidiAsset(const TSharedPtr<Audio::IProxyData>& InInitData)
	{
		if (InInitData.IsValid())
		{
			if (InInitData->CheckTypeCast<FMidiFileProxy>())
			{
				// should we be getting handed a SharedPtr here?
				MidiFileProxy = MakeShared<FMidiFileProxy, ESPMode::ThreadSafe>(InInitData->GetAs<FMidiFileProxy>());
			}
		}
	}

	bool FMidiAsset::IsMidiValid() const
	{
		return MidiFileProxy.IsValid();
	}
}
