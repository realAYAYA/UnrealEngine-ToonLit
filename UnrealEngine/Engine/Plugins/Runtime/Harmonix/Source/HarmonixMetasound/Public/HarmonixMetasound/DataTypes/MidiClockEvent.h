// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HarmonixMidi/MidiPlayCursor.h"

namespace HarmonixMetasound
{
	struct HARMONIXMETASOUND_API FMidiClockMsg
	{
		enum class EType : uint8
		{
			Reset,
			Loop,
			SeekTo,
			SeekThru,
			AdvanceThru
		};
		
		struct FReset
		{
			int32 FromTick;
			int32 ToTick;
			bool ForceNoBroadcast;
			FReset(int32 FromTick, int32 ToTick, bool ForceNoBroadcast)
				: FromTick(FromTick), ToTick(ToTick), ForceNoBroadcast(ForceNoBroadcast) {}
		};

		struct FLoop
		{
			int32 LoopStartTick;
			int32 LoopEndTick;
			FLoop(int32 LoopStartTick, int32 LoopEndTick)
				: LoopStartTick(LoopStartTick), LoopEndTick(LoopEndTick) {}
		};

		struct FSeekTo
		{
			int32 FromTick;
			int32 ToTick;
			FSeekTo(int32 FromTick, int32 ToTick)
				: FromTick(FromTick), ToTick(ToTick) {}
		};

		struct FSeekThru
		{
			int32 FromTick;
			int32 ThruTick;
			FSeekThru(int32 FromTick, int32 ThruTick)
				: FromTick(FromTick), ThruTick(ThruTick) {}
		};

		struct FAdvanceThru
		{
			int32 FromTick;
			int32 ThruTick;
			bool IsPreRoll;
			FAdvanceThru(int32 FromTick, int32 ThruTick, bool IsPreRoll)
				: FromTick(FromTick), ThruTick(ThruTick), IsPreRoll(IsPreRoll) {}
		};

		FMidiClockMsg(const FReset& Reset) : Type(EType::Reset), Reset(Reset) {};
		FMidiClockMsg(const FLoop& Loop) : Type(EType::Loop), Loop(Loop) {};
		FMidiClockMsg(const FSeekTo& SeekTo) : Type(EType::SeekTo), SeekTo(SeekTo) {};
		FMidiClockMsg(const FSeekThru& SeekThru) : Type(EType::SeekThru), SeekThru(SeekThru) {};
		FMidiClockMsg(const FAdvanceThru AdvanceThru) : Type(EType::AdvanceThru), AdvanceThru(AdvanceThru) {};

		// type safe way to access msg data
		// asserts that is of the corresponding type
		const FReset& AsReset() const;
		const FLoop& AsLoop() const;
		const FSeekTo& AsSeekTo() const;
		const FSeekThru& AsSeekThru() const;
		const FAdvanceThru& AsAdvanceThru() const;

		// Asserts that this is a Reset, SeekTo, SeekThru, or AdvanceThru type
		int32 FromTick() const;

		// Asserts that this is a Reset, or SeekTo type
		int32 ToTick() const;

		// Asserts that this is a SeekThru or AdvanceThru type
		int32 ThruTick() const;

		const EType Type;
	private:
		union 
		{
			const FReset Reset;
			const FLoop Loop;
			const FSeekTo SeekTo;
			const FSeekThru SeekThru;
			const FAdvanceThru AdvanceThru;
		};
		
	};
	
	struct HARMONIXMETASOUND_API FMidiClockEvent
	{
		const int32 BlockFrameIndex;
		const FMidiClockMsg Msg;
		
		FMidiClockEvent(int32 InBlockFrameIndex, const FMidiClockMsg& Msg)
			: BlockFrameIndex(InBlockFrameIndex), Msg(Msg)
		{}
		
	};

};
