// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace Audio
{
	namespace EMusicalScale
	{
		// Any additions to this Enum should also be added to MetasoundMidiNoteQuantizerNode.cpp
		// in the "#pragma region Metasound Enums" area to show up as options in that Node
		enum Scale
		{
			Major,
			Dominant7th_Mixolydian,
			Minor_Dorian,
			HalfDiminished_Locrian,
			Diminished,
			MajorPentatonic,
			Lydian,
			Bebop_Major,
			HarmonicMajor,
			LydianAugmented,
			Augmented,
			SixthModeOfHarmonicMinor,
			Diminished_BeginWithHalfStep,
			Blues,
			Bebop_Dominant,
			Spanish_or_Jewish,
			LydianDominant,
			Hindu,
			WholeTone,
			Chromatic,
			DiminishedWholeTone,
			MinorPentatonic,
			Bebop_Minor,
			MelodicMinor,
			Bebop_MinorNumber2,
			HarmonicMinor,
			Diminished_BeginWithWholeStep,
			Phrygian,
			NaturalMinor_Aeolian,
			HalfDiminished_LocrianNumber2,

			Count
		};
	}

	struct SIGNALPROCESSING_API ScaleDegreeSet
	{
	public:
		// ctor
		ScaleDegreeSet(const TArray<float>& InScaleDegrees, const TArray<float>& InChordTones = {});

		TArrayView<float> GetScaleDegreeSet(bool bChordTonesOnlyIfApplicable = false);

	private:
		TArray<float> ScaleDegrees;
		TArray<float> ChordTones;
	};

	class SIGNALPROCESSING_API FMidiNoteQuantizer
	{
	public:
		static float QuantizeMidiNote(const float InNote, const float InRoot, EMusicalScale::Scale InScale, bool bChordTonesOnlyIfApplicable = false);
		static float QuantizeMidiNote(const float InNote, const float InRoot, const TArrayView<float> InScaleDegrees);

	private:
		static float QuantizeValueToScaleDegree(const float InValue, const TArrayView<float> InScaleDegrees);

	public:
		// Statically-defined scale/chord tone definitions.
		static TMap<EMusicalScale::Scale, ScaleDegreeSet> ScaleDegreeSetMap;

	}; // class FMidNoteQuantizer
} // namespace Audio