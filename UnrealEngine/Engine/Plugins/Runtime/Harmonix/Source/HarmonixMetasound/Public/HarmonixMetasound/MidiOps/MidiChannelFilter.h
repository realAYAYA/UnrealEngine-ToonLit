// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MidiOps.h"
#include "StuckNoteGuard.h"

#include "HarmonixMetasound/DataTypes/MidiStream.h"

namespace Harmonix::Midi::Ops
{
	/**
	 * Includes MIDI events from enabled channels only
	 */
	class HARMONIXMETASOUND_API FMidiChannelFilter
	{
	public:
		/**
		 * Pass the filtered events from the input stream to the output stream
		 * @param InStream The MIDI stream to filter
		 * @param OutStream The filtered MIDI stream
		 */
		void Process(const HarmonixMetasound::FMidiStream& InStream, HarmonixMetasound::FMidiStream& OutStream);

		/**
		 * Enable or disable a channel (or all channels)
		 * @param Channel The MIDI channel (1-16, 0 is all channels)
		 * @param Enabled Whether the channel(s) should be enabled
		 */
		void SetChannelEnabled(uint8 Channel, bool Enabled);

		/**
		 * Find out if a channel is enabled
		 * @param Channel The MIDI channel (1-16, 0 is all channels)
		 * @return true if the channel is (or all channels are) enabled
		 */
		bool GetChannelEnabled(uint8 Channel) const;

	private:
		// bitfield to store which channels are enabled
		uint16 EnabledChannels = AllChannelsOff;

		FStuckNoteGuard StuckNoteGuard;
	};
}
