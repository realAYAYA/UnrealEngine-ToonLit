// Copyright Epic Games, Inc. All Rights Reserved.

#include "DSP/MidiNoteQuantizer.h"
#include "Math/NumericLimits.h"

namespace Audio
{
	// useful musical defines for scale degrees + accidentals to translate to half-steps
	// i.e. - n1 = "Natural 1" = 0.0 half-steps from the root
	//		- b3 = "Flat 3"    = 3.0f half-steps etc.
	static const float n8 = 12.0f;  // C
	static const float n7 = 11.0f;	// B
	static const float		b7 = 10.0f;
	static const float bb7 = 9.0f;
	static const float n6 = 9.0f;	// A
	static const float		b6 = 8.0f;
	static const float		s5 = 8.0f;
	static const float n5 = 7.0f;	// G
	static const float		b5 = 6.0f;
	static const float		s4 = 6.0f;
	static const float n4 = 5.0f;	// F
	static const float n3 = 4.0f;	// E
	static const float		b3 = 3.0f;
	static const float		s2 = 3.0f;
	static const float n2 = 2.0f;	// D
	static const float		b2 = 1.0f;
	static const float n1 = 0.0f;	// C


	ScaleDegreeSet::ScaleDegreeSet(const TArray<float>& InScaleDegrees, const TArray<float>& InChordTones)
		: ScaleDegrees(InScaleDegrees)
		, ChordTones(InChordTones)
	{
	}

	TArrayView<float> ScaleDegreeSet::GetScaleDegreeSet(bool bChordTonesOnlyIfApplicable)
	{
		if (bChordTonesOnlyIfApplicable && ChordTones.Num())
		{
			return ChordTones;
		}

		return ScaleDegrees;
	}

	float FMidiNoteQuantizer::QuantizeMidiNote(const float InNote, const float InRoot, EMusicalScale::Scale InScale, bool bChordTonesOnlyIfApplicable)
	{
		return QuantizeMidiNote(InNote, InRoot, ScaleDegreeSetMap[InScale].GetScaleDegreeSet(bChordTonesOnlyIfApplicable));
	}

	float FMidiNoteQuantizer::QuantizeMidiNote(const float InNote, const float InRoot, const TArrayView<float> InScaleDegrees)
	{
		// QuantizeValueToScaleDegree() works within a single octave, so compensate for the octave and root
		const float SetRange = InScaleDegrees[InScaleDegrees.Num() - 1]; // (SetRange will == 12.0 for a normal octave)
		const float NoteRootDelta = (InNote - InRoot);
		const float InNoteOctave = FMath::FloorToFloat(NoteRootDelta / SetRange);
		const float ValueToQuant = NoteRootDelta - (SetRange * InNoteOctave);

		const float QuantizedValue = QuantizeValueToScaleDegree(ValueToQuant, InScaleDegrees);

		// reconstruct the quantized midi note given the InRoot and original octave of InNote
		return (SetRange * InNoteOctave + QuantizedValue) + InRoot;
	}

	float FMidiNoteQuantizer::QuantizeValueToScaleDegree(const float InValue, const TArrayView<float> InScaleDegrees)
	{
		// the Set we are quantizing to should have at least 2 elements
		if (!ensure(InScaleDegrees.Num()))
		{
			return InValue;
		}
		else if (InScaleDegrees.Num() == 1)
		{
			// if the set only contains a single value, that is technically the quantized value
			return InScaleDegrees[0];
		}
		else if (!ensureMsgf((0.0f <= InValue) && (InValue <= InScaleDegrees[InScaleDegrees.Num() - 1])
			, TEXT("Pitch Quantization: InValue (%f) is not in the range of a single octave (Needs to satisfy: 0.0f <= InValue <= %f)")
			, InValue, InScaleDegrees[InScaleDegrees.Num() - 1]))
		{
			return InValue; // bail
		}

		bool bNegativeDelta = false;
		float PositiveDelta = TNumericLimits<float>::Max();
		float NegativeDelta = TNumericLimits<float>::Lowest();

		// In the case where InValue == last element in the note set
		// we want to initialize FirstNegativeIndex like this for our
		// post-loop logic to return the correct value:
		//	--> return Set[FirstNegativeIndex - 1];
		// (in this case "NegativeDelta" will never get updated before the loop completes)
		int32 FirstNegativeIndex = InScaleDegrees.Num();
		for (int32 i = 0; i < InScaleDegrees.Num(); ++i)
		{
			// InValue == Set[i]
			if (FMath::IsNearlyEqual(InValue, InScaleDegrees[i]))
			{
				return InScaleDegrees[i];
			}

			// We are measuring Delta as (InValue - InScaleDegrees[i]).
			// Delta should be positive until we pass by InValue in the set.
			// Once we hit a negative value, we know which two candidates we need
			// to compare for the shortest distance, and can exit the loop
			const float CurrDelta = InValue - InScaleDegrees[i];
			if (CurrDelta >= 0.0f)
			{
				PositiveDelta = CurrDelta;
				continue;
			}
			else
			{
				NegativeDelta = CurrDelta;
				FirstNegativeIndex = i;
				break;
			}
		}

		if (FMath::Abs(NegativeDelta) < FMath::Abs(PositiveDelta))
		{
			return InScaleDegrees[FirstNegativeIndex];
		}
		else
		{
			return InScaleDegrees[FirstNegativeIndex - 1];
		}
	}


	// Initialize our note & chord arrays

	/* REQUIREMENTS:
	*	The (optional) second array defines Chord Tones, if empty, the first array will be used always
	*		- this is ideally a subset of the first array (user-facing UI/UX will imply this), but does not technically need to be and is not enforced.
	* 
	*	The first array MUST have at least 1 entry or it will ensure.
	* 
	*	Both array MUST be in ascending order, or quantization behavior will be broken. This is not enforced at runtime and quantization and will not warn/ensure
	* 
	*	The Arrays must represent a full octave (inclusive): the lowest element being (0.0 and the highest being 12.0)
	*/
	TMap<EMusicalScale::Scale, ScaleDegreeSet> FMidiNoteQuantizer::ScaleDegreeSetMap{
		/* entry example:
		* 	{ EMusicalScale::Scale::[SCALE NAME],
			{{ [(required) SCALE DEGREES] }, { [(optional) CHORD TONES] }}}
		*/
		  { EMusicalScale::Scale::Major,
			{{ n1, n2, n3, n4, n5, n6, n7, n8 }, { n1, n2, n3, n5, n7, n8 }}}
		, { EMusicalScale::Scale::Dominant7th_Mixolydian,
			{{ n1, n2, n3, n4, n5, n6, b7, n8 }, { n1, n2, n3, n5, b7, n8 }}}
		, { EMusicalScale::Scale::Minor_Dorian,
			{{ n1, n2, b3, n4, n5, n6, b7, n8 }, { n1, n2, b3, n5, b7, n8 }}}
		, { EMusicalScale::Scale::HalfDiminished_Locrian,
			{{ n1, b2, b3, n4, b5, b6, b7, n8 }, { n1, b3, b5, b7, n8 }}}
		, { EMusicalScale::Scale::Diminished,
			{{ n1, n2, b3, n4, b5, b6, n6, n7, n8 }, { n1, b3, n5, bb7, n8 }}}
		, { EMusicalScale::Scale::MajorPentatonic,
			{{ n1, n2, n3, n5, n6, n8 }, { n1, n3, n5, n7, n8 }}}
		, { EMusicalScale::Scale::Lydian,
			{{ n1, n2, n3, s4, n5, n6, n7, n8 }, { n1, n3, n5, n7, n8 }}}
		, { EMusicalScale::Scale::Bebop_Major,
			{{ n1, n2, n3, s4, n5, s5, n6, n7, n8 }, { n1, n3, n5, n7, n8 }}}
		, { EMusicalScale::Scale::HarmonicMajor,
			{{ n1, n2, n3, n4, n5, b6, n7, n8 }, { n1, n3, n5, n7, n8 }}}
		, { EMusicalScale::Scale::LydianAugmented,
			{{ n1, n2, n3, s4, s5, n6, n7, n8 }, { n1, n3, s5, n7, n8 }}}
		, { EMusicalScale::Scale::Augmented,
			{{ n1, s2, n3, n4, n5, b6, n7, n8 }, { n1, n3, n5, n7, n8 }}}
		, { EMusicalScale::Scale::SixthModeOfHarmonicMinor,
			{{ n1, s2, n3, s4, n5, n6, n7, n8 }, { n1, n3, n5, n7, n8 }}}
		, { EMusicalScale::Scale::Diminished_BeginWithHalfStep,
			{{ n1, b2, s2, n3, s4, n5, n6, b7, n8 }, { n1, n3, n5, b7, n8 }}}
		, { EMusicalScale::Scale::Blues,
			{{ n1, b3, n4, s4, n5, b7, n8 }, { n1, b3, n5, b7, n8 }}}
		, { EMusicalScale::Scale::Bebop_Dominant,
			{{ n1, n2, n3, n4, n5, n6, b7, n7, n8 }, { n1, n3, n5, n7, n8 }}}
		, { EMusicalScale::Scale::Spanish_or_Jewish,
			{{ n1, b2, n3, n4, n5, b6, b7, n8 }, { n1, n3, n5, b7, n8 }}}
		, { EMusicalScale::Scale::LydianDominant,
			{{ n1, n2, n3, s4, n5, n6, b7, n8 }, { n1, n3, n5, b7, n8 }}}
		, { EMusicalScale::Scale::Hindu,
			{{ n1, n2, n3, n4, n5, b6, b7, n8 }, { n1, n3, n5, b7, n8 }}}
		, { EMusicalScale::Scale::WholeTone,
			{{ n1, n2, n3, s4, s5, b7, n8 }, { n1, n3, n5, b7, n8 }}}
		, { EMusicalScale::Scale::Chromatic,
			{{ n1, b2, n2, b3, n3, n4, b5, n5, b6, n6, b7, n7, n8 }, { /* NA */ }}}
		, { EMusicalScale::Scale::DiminishedWholeTone,
			{{ n1, b2, s2, n3, s4, n5, n6, b7, n8 }, { n1, n3, n5, b7, n8 }}}
		, { EMusicalScale::Scale::MinorPentatonic,
			{{ n1, b3, n4, n5, b7, n8 }, { n1, b3, n5, b7, n8 }}}
		, { EMusicalScale::Scale::Bebop_Minor,
			{{ n1, n2, b3, n3, n4, n5, n6, b7, n8 }, { n1, b3, n5, b7, n8 }}}
		, { EMusicalScale::Scale::MelodicMinor,
			{{ n1, n2, b3, n4, n5, n6, n7, n8 }, { n1, b3, n5, n7, n8 }}}
		, { EMusicalScale::Scale::Bebop_MinorNumber2,
			{{ n1, n2, b3, n4, n5, s5, n6, n7, n8 }, { n1, b3, n5, n7, n8 }}}
		, { EMusicalScale::Scale::HarmonicMinor,
			{{ n1, n2, b3, n4, n5, b6, n7, n8 }, { n1, b3, n5, n7, n8 }}}
		, { EMusicalScale::Scale::Diminished_BeginWithWholeStep,
			{{ n1, n2, b3, n4, s4, s5, n6, n7, n8 }, { n1, b3, s5, n7, n8 }}}
		, { EMusicalScale::Scale::Phrygian,
			{{ n1, b2, b3, n4, n5, b6, b7, n8 }, { n1, b3, n5, b7, n8 }}}
		, { EMusicalScale::Scale::NaturalMinor_Aeolian,
			{{ n1, n2, b3, n4, n5, b6, b7, n8 }, { n1, b3, n5, b7, n8 }}}
		, { EMusicalScale::Scale::HalfDiminished_LocrianNumber2,
			{{ n1, b2, b3, n4, b5, b6, b7, n8 }, { n1, b3, b5, b7, n8 }}}
	};
} // namespace Audio