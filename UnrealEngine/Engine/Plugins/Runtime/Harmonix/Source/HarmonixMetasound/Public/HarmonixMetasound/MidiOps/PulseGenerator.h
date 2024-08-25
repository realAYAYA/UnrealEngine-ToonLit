// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/SpscQueue.h"

#include "HarmonixDsp/Parameters/Parameter.h"

#include "HarmonixMetasound/DataTypes/MidiClock.h"
#include "HarmonixMetasound/DataTypes/MidiStream.h"
#include "HarmonixMetasound/DataTypes/MusicTimeInterval.h"

#include "HarmonixMidi/MidiVoiceId.h"

namespace Harmonix::Midi::Ops
{
	class HARMONIXMETASOUND_API FPulseGenerator
	{
	public:
		void Enable(bool bEnable);
		
		void SetClock(const TSharedPtr<const HarmonixMetasound::FMidiClock, ESPMode::NotThreadSafe>& Clock);

		Dsp::Parameters::TParameter<uint8> Channel{ 1, 16, 1 };

		Dsp::Parameters::TParameter<uint16> Track{ 1, UINT16_MAX, 1 };
		
		Dsp::Parameters::TParameter<uint8> NoteNumber{ 0, 127, 60 };
		
		Dsp::Parameters::TParameter<uint8> Velocity{ 0, 127, 127 };
		
		void SetInterval(const FMusicTimeInterval& NewInterval);

		FMusicTimeInterval GetInterval() const { return Cursor.GetInterval(); }

		void Process(HarmonixMetasound::FMidiStream& OutStream);

	private:
		FMidiVoiceGeneratorBase VoiceGenerator{};
		TOptional<HarmonixMetasound::FMidiStreamEvent> LastNoteOn;
		bool Enabled{ true };

		class FCursor final : public FMidiPlayCursor
		{
		public:
			FCursor();

			virtual ~FCursor() override;

			void SetClock(const TSharedPtr<const HarmonixMetasound::FMidiClock, ESPMode::NotThreadSafe>& NewClock);

			void SetInterval(const FMusicTimeInterval& NewInterval);

			FMusicTimeInterval GetInterval() const { return Interval; }

			struct FPulseTime
			{
				int32 AudioBlockFrame;
				int32 MidiTick;
			};

			bool Pop(FPulseTime& PulseTime);

		private:
			void Push(FPulseTime&& PulseTime);
			
			virtual void AdvanceThruTick(int32 Tick, bool IsPreRoll) override;
			
			virtual void OnTimeSig(int32 TrackIndex, int32 Tick, int32 Numerator, int32 Denominator, bool IsPreroll) override;

			TWeakPtr<const HarmonixMetasound::FMidiClock, ESPMode::NotThreadSafe> Clock;
			FMusicTimeInterval Interval{};
			TSpscQueue<FPulseTime> PulsesSinceLastProcess;
			int32 QueueSize{0};
			FTimeSignature CurrentTimeSignature{};
			FMusicTimestamp NextPulseTimestamp{ -1, -1 };
		};
		
		FCursor Cursor;
	};
}
