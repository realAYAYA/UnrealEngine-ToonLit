// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Tickable.h"

#include "MetasoundDataReference.h"
#include "MetasoundEnumRegistrationMacro.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundOperatorSettings.h"
#include "MetasoundVariable.h"
#include "MetasoundSampleCounter.h"

#include "HarmonixMidi/MidiFile.h"
#include "HarmonixMidi/SongMaps.h"
#include "HarmonixMetasound/DataTypes/MusicTransport.h"
#include "HarmonixMetasound/DataTypes/MidiClockEvent.h"
#include "HarmonixMidi/MidiPlayCursorMgr.h"
#include "HarmonixMidi/MidiPlayCursor.h"
#include "Sound/QuartzQuantizationUtilities.h"

namespace Metasound
{
	DECLARE_METASOUND_ENUM(
		EMidiClockSubdivisionQuantization,
		EMidiClockSubdivisionQuantization::None,
		HARMONIXMETASOUND_API,
		FEnumMidiClockSubdivisionQuantizationType,
		FEnumMidiClockSubdivisionQuantizationTypeInfo,
		FEnumMidiClockSubdivisionQuantizationReadRef,
		FEnumMidiClockSubdivisionQuantizationWriteRef
	);
}

namespace HarmonixMetasound
{
	struct HARMONIXMETASOUND_API FMidiTimestampTransportState
	{
		int32 BlockSampleFrameIndex = 0;
		float BlockSampleFrameOffset = 0.0f;
		EMusicPlayerTransportState TransportState = EMusicPlayerTransportState::Invalid;
	};

	struct HARMONIXMETASOUND_API FMidiTimestampSpeed
	{
		int32 BlockSampleFrameIndex = 0;
		float BlockSampleFrameOffset = 0.0f;
		float Speed = 1.0f;
	};

	struct HARMONIXMETASOUND_API FMidiTimestampTempo
	{
		int32 BlockSampleFrameIndex = 0;
		float BlockSampleFrameOffset = 0.0f;
		float Tempo = 120.0f;
	};

	constexpr FMidiTimestampTempo InvalidMidiTimestampTempo{ -1, 0.0f, 0.0f };

	class HARMONIXMETASOUND_API FMidiClock : public TSharedFromThis<FMidiClock, ESPMode::NotThreadSafe>
	{
	public:
		static constexpr int32 kMidiGranularity = 128;
		
		explicit FMidiClock(const Metasound::FOperatorSettings& InSettings);
		virtual ~FMidiClock();

		FMidiClock(const FMidiClock& Other);
		FMidiClock& operator=(const FMidiClock& Other);

		void ResetAndStart(int32 FrameIndex, bool SeekToStart = true);

		void PrepareBlock();

		bool HasLowResCursors() const;
		void UpdateLowResCursors();

		void AddTransportStateChangeToBlock(const FMidiTimestampTransportState& NewTransportState);
		void AddSpeedChangeToBlock(const FMidiTimestampSpeed& NewSpeed);

		bool HasTransportStateChangeInBlock() const
		{
			return TransportChangesInBlock.IsEmpty() == false;
		}
		bool HasSpeedChangesInBlock() const
		{
			return HasSpeedChangeInBlock;
		}
		bool HasTempoChangesInBlock() const
		{
			return HasTempoChangeInBlock;
		}

		const TArray<FMidiClockEvent>& GetMidiClockEventsInBlock() const;

		EMusicPlayerTransportState GetTransportStateAtBlockSampleFrame(int32 FrameIndex) const;
		EMusicPlayerTransportState GetTransportStateAtEndOfBlock() const;
		const FMidiTimestampTransportState& GetTransportTimestampForBlockSampleFrame(int32 FrameIndex) const;
		const TArray<FMidiTimestampTransportState>& GetTransportTimestampsInBlock() const
		{
			return TransportChangesInBlock;
		}

		float GetSpeedAtBlockSampleFrame(int32 FrameIndex) const;
		float GetSpeedAtEndOfBlock() const;
		const FMidiTimestampSpeed& GetSpeedTimestampForBlockSampleFrame(int32 FrameIndex) const;
		const TArray<FMidiTimestampSpeed>& GetTempoSpeedTimestampsInBlock() const
		{
			return SpeedChangesInBlock;
		}

		float GetTempoAtBlockSampleFrame(int32 FrameIndex) const;
		float GetTempoAtEndOfBlock() const;
		int32 GetNumTempoChangesInBlock() const;
		FMidiTimestampTempo GetTempoChangeByIndex(int32 Index) const;

		int32 GetCurrentMidiTick() const;

		int32 GetCurrentBlockFrameIndex() const;

		float GetQuarterNoteIncludingCountIn() const;

		/**
		 * @brief Get the timestamp after the most recent clock update
		 * @return The current timestamp
		 */
		FMusicTimestamp GetCurrentMusicTimestamp() const;
		
		/**
		 * @brief Get the music timestamp at a given frame offset from the last processed audio block.
		 * @param Offset - The frame index from the beginning of the last processed audio block
		 * @return The music timestamp
		 */
		FMusicTimestamp GetMusicTimestampAtBlockOffset(int32 Offset) const;

		/**
		 * @brief Get the absolute time in ms for a frame within the last audio block
		 * @param Offset - The frame index from the beginning of the last processed audio block
		 * @return The absolute time in ms
		 */
		float GetMsAtBlockOffset(int32 Offset) const;

		//*****************************************************************************************
		// NOTE: These next functions are a facade in front of this class's MidiPlayCursorMgr. 
		void AttachToMidiResource(TSharedPtr<FMidiFileData> MidiDataProxy, bool ResetCursorsToStart = true, int32 PreRollBars = 0) { DrivingMidiPlayCursorMgr->AttachToMidiResource(MidiDataProxy, ResetCursorsToStart, PreRollBars); }
		void DetachFromMidiResource() { DrivingMidiPlayCursorMgr->DetachFromMidiResource(); }
		void AttachToTimeAuthority(const FMidiClock& MidiClockRef);
		void DetachFromTimeAuthority();
		void InformOfCurrentAdvanceRate(float AdvanceRate);
		
		void LockForMidiDataChanges();
		void MidiDataChangesComplete(FMidiPlayCursorMgr::EMidiChangePositionCorrectMode Mode = FMidiPlayCursorMgr::EMidiChangePositionCorrectMode::MaintainTick);
		const FSongMaps& GetSongMaps() const     { return DrivingMidiPlayCursorMgr->GetSongMaps();   }
		const FTempoMap& GetTempoMap() const     { return DrivingMidiPlayCursorMgr->GetTempoMap();   }
		const FBarMap& GetBarMap() const         { return DrivingMidiPlayCursorMgr->GetBarMap();     }
		void AdvanceHiResToMs(int32 BlockFrameIndex, float Ms, bool Broadcast);
		void SeekTo(const FMusicSeekTarget& Timestamp, int32 PreRollBars);
		void SeekTo(int32 Tick, int32 PrerollBars);

		void SetLoop(int32 StartTick, int32 EndTick) { DrivingMidiPlayCursorMgr->SetLoop(StartTick, EndTick, false, true); }
		void ClearLoop() { DrivingMidiPlayCursorMgr->ClearLoop(true); }
		bool DoesLoop() const { return DrivingMidiPlayCursorMgr->DoesLoop(false); }
		float GetLoopStartMs() const { return DrivingMidiPlayCursorMgr->GetLoopStartMs(false); }
		int32 GetLoopStartTick() const { return DrivingMidiPlayCursorMgr->GetLoopStartTick(false); }
		float GetLoopEndMs() const { return DrivingMidiPlayCursorMgr->GetLoopEndMs(false); }
		int32 GetLoopEndTick() const { return DrivingMidiPlayCursorMgr->GetLoopEndTick(false); }

		int32 GetCurrentHiResTick() const  { return DrivingMidiPlayCursorMgr->GetCurrentHiResTick();  }
		float GetCurrentHiResMs() const    { return DrivingMidiPlayCursorMgr->GetCurrentHiResMs();    }
		float GetElapsedHiResMs() const    { return DrivingMidiPlayCursorMgr->GetElapsedHiResMs();    }
		int32 GetCurrentLowResTick() const { return DrivingMidiPlayCursorMgr->GetCurrentLowResTick(); }
		float GetCurrentLowResMs() const   { return DrivingMidiPlayCursorMgr->GetCurrentLowResMs();   }
		float GetElapsedLowResMs() const   { return DrivingMidiPlayCursorMgr->GetElapsedLowResMs();   }
		// These next few are const because they need to be callable by things that hold a ReadRef to us!
		void RegisterHiResPlayCursor(FMidiPlayCursor* PlayCursor, float PreRollMs = -1.0f) const  { DrivingMidiPlayCursorMgr->RegisterHiResPlayCursor(PlayCursor, PreRollMs);  }
		void RegisterLowResPlayCursor(FMidiPlayCursor* PlayCursor, float PreRollMs = -1.0f) const { DrivingMidiPlayCursorMgr->RegisterLowResPlayCursor(PlayCursor, PreRollMs); }
		void UnregisterPlayCursor(FMidiPlayCursor* PlayCursor, bool WarnOnFail = true) const      { DrivingMidiPlayCursorMgr->UnregisterPlayCursor(PlayCursor, WarnOnFail);    }
		void UnregisterAllPlayCursors()                                                           { DrivingMidiPlayCursorMgr->UnregisterAllPlayCursors();                      }

		TSharedPtr<FMidiPlayCursorMgr> GetDrivingMidiPlayCursorMgr() const { return DrivingMidiPlayCursorMgr.ToSharedPtr(); }
		//*****************************************************************************************
		
		// handle and add transport change event
		// to be called within the Transport Post Processor with the new transport state after the normal Processor
		void HandleTransportChange(int32 BlockFrameIndex, EMusicPlayerTransportState TransportState);

		// handle single clock event
		void HandleClockEvent(const FMidiClock& DrivingClock, const FMidiClockEvent& Event, int32 PrerollBars, float Speed = 1.0f);
		
		// process and advance the clock based on the driving clock given sample frames
		// will handle the driving clock events based on the frame range
		void Process(const FMidiClock& DrivingClock, int32 StartFrame, int32 NumFrames, int32 PrerollBars, float Speed = 1.0f);

		// process and advance the clock normally based on the given sample frames
		void Process(int32 StartFrame, int32 NumFrames, int32 PrerollBars, float Speed = 1.0f);
		
		// directly perform and write an advance to this clock
		void WriteAdvance(int32 StartFrameIndex, int32 EndFrameIndex, float InSpeed = 1.0f);

		// directly seek this clock with a musical seek target or a specific tick
		void SeekTo(int32 BlockFrameIndex, const FMusicSeekTarget& InTarget, int32 InPrerollBars);
		void SeekTo(int32 BlockFrameIndex, int32 Tick, int32 InPrerollBars);
		
		/**
		 * Given an input tick, outputs a looped tick if the input tick is > the StartTick of the Loop Region
		 * If the clock is not looping, or loop region length is 0, then the output will be unchanged.
		 *
		 * The output tick will be in range [Min(Tick, LoopStartTick), LoopEndTick).
		 * Example:
		 * LoopRegion: (0, 100):
		 * 10 -> 10
		 * 100 -> 0
		 * 110 -> 10
		 * -10 -> 90
		 *
		 * LoopRegion: (40, 100):
		 * 0 -> 0
		 * 10 -> 10
		 * -10 -> -10
		 * 99  -> 99
		 * 100 -> 40
		 * 110 -> 50
		 *
		 * @param		Tick - Absolute Tick
		 * @return		Looped Tick if Tick > LoopEnd: LoopedTick = LoopStart + (Tick - LoopStart) % (LoopEnd - LoopStart) 
		 */
		int32 CalculateMappedTick(int32 Tick) const;
		
		// copy speed and tempo changes from in clock to this clock
		// with an optional Speed multiplier to adjust the out going speed on this clock
		void CopySpeedAndTempoChanges(const FMidiClock* InClock, float InSpeedMult = 1.0f);

		// Creates a new FMidiFileData with the given starting Tempo and Time Signature
		// With max song length to be played indefinitely. Useful for Midi Clock Metronomes.
		static TSharedPtr<FMidiFileData> MakeClockConductorMidiData(float TempoBPM, int32 TimeSigNum, int32 TimeSigDen);

	private:
		friend class FMidiClockEventCursor;

		class FMidiClockEventCursor : public FMidiPlayCursor
		{
		public:
			FMidiClockEventCursor(FMidiClock* MidiClock);
		
			//~ BEGIN FMidiPlayCursor Overrides
			virtual void Reset(bool ForceNoBroadcast = false) override;
			virtual void OnLoop(int32 LoopStartTick, int32 LoopEndTick) override;
			virtual void SeekToTick(int32 Tick) override;
			virtual void SeekThruTick(int32 Tick) override;
			virtual void AdvanceThruTick(int32 Tick, bool IsPreRoll) override;
			virtual void OnTempo(int32 TrackIndex, int32 Tick, int32 Tempo, bool IsPreroll = false) override;
			//~ END FMidiPlayCursor Overrides
			
			void AddEvent(const FMidiClockEvent& InEvent);
		private:
			FMidiClock* MyMidiClock = nullptr;
		};
		
		FMidiClockEventCursor MidiClockEventCursor;

	private:
		void RegisterForGameThreadUpdates();
		void UnregisterForGameThreadUpdates();
		
		int32 BlockSize;
		int32 CurrentBlockFrameIndex;
		float SampleRate;
		Metasound::FSampleCount SampleCount;
		int32 FramesUntilNextProcess = 0;
		FMidiTimestampTransportState CurrentTransportState;

		TArray<FMidiTimestampTransportState> TransportChangesInBlock;
		bool								 HasSpeedChangeInBlock;
		TArray<FMidiTimestampSpeed>          SpeedChangesInBlock;
		bool								 HasTempoChangeInBlock;
		TArray<FMidiTimestampTempo>          TempoChangesInBlock;
		
		// midi clock events are modified by a MidiClockEventCursor
		// which has to be manually registered to the MidiClock
		// (this is to avoid always needlessly generating midi clock events)
		TArray<FMidiClockEvent> MidiClockEventsInBlock;

		bool SmoothingEnabled = false;
		TSharedRef<FMidiPlayCursorMgr> DrivingMidiPlayCursorMgr;
	};

	// Declare aliases IN the namespace...
	DECLARE_METASOUND_DATA_REFERENCE_ALIAS_TYPES(FMidiClock, FMidiClockTypeInfo, FMidiClockReadRef, FMidiClockWriteRef)
}

// Declare reference types OUT of the namespace...
DECLARE_METASOUND_DATA_REFERENCE_TYPES_NO_ALIASES(HarmonixMetasound::FMidiClock, HARMONIXMETASOUND_API)
