// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/Timespan.h"
#include "Math/NumericLimits.h"

/*
	Timestamp value for media playback

	- Time:
	  Time value

	- SequenceIndex
	  Sequence that is current for this time value

	Within a single sequence time values will increase or decrease monotonically. A new sequence index is generated on each event that causes the time to no
	longer be monotonic. (e.g. seek or loop)
	A sequence index does not offer any information about the ordering of the time stamps on the timeline. Time values are comparable between all timestamps from a single playback,
	though, one needs to be careful to consider non-monotonic behavior if the sequence indices are not identical.

	Sequence indices can very much offer ordering information as far as playback progression is concerned. Higher indices are also later in playback. (even if time values may be smaller: e.g. looping)

	All comparison operators of this class will operate to indicate ordering relative to playback, not position on the timeline!

*/
class FMediaTimeStamp
{
public:
	FMediaTimeStamp() : Time(FTimespan::MinValue()), SequenceIndex(0) {}
	FMediaTimeStamp(const FTimespan & InTime) : Time(InTime), SequenceIndex(0) {}
	FMediaTimeStamp(const FTimespan & InTime, int64 InSequenceIndex) : Time(InTime), SequenceIndex(InSequenceIndex) {}

	void Invalidate() { Time = FTimespan::MinValue(); }
	bool IsValid() const { return Time != FTimespan::MinValue(); }

	bool IsRelative() const { return SequenceIndex < 0; }

	bool operator < (const FMediaTimeStamp & Other) const { return (SequenceIndex < Other.SequenceIndex) || (SequenceIndex == Other.SequenceIndex && Time < Other.Time); }
	bool operator <= (const FMediaTimeStamp & Other) const { return (SequenceIndex < Other.SequenceIndex) || (SequenceIndex == Other.SequenceIndex && Time <= Other.Time); }
	bool operator == (const FMediaTimeStamp & Other) const { return (SequenceIndex == Other.SequenceIndex && Time == Other.Time); }
	bool operator > (const FMediaTimeStamp & Other) const { return (SequenceIndex > Other.SequenceIndex) || (SequenceIndex == Other.SequenceIndex && Time > Other.Time); }
	bool operator >= (const FMediaTimeStamp & Other) const { return (SequenceIndex > Other.SequenceIndex) || (SequenceIndex == Other.SequenceIndex && Time >= Other.Time); }

	FMediaTimeStamp operator + (const FTimespan & Other) const { return FMediaTimeStamp(Time + Other, SequenceIndex); }
	FMediaTimeStamp operator - (const FTimespan & Other) const { return FMediaTimeStamp(Time - Other, SequenceIndex); }

	FMediaTimeStamp operator - (const FMediaTimeStamp& Other) const { return (Other.SequenceIndex < SequenceIndex) ? FMediaTimeStamp(FTimespan::MaxValue(), MAX_int64) : ((Other.SequenceIndex > SequenceIndex) ? FMediaTimeStamp(FTimespan::MinValue(), MAX_int64) : FMediaTimeStamp(Time - Other.Time, MAX_int64)); }

	FMediaTimeStamp& operator += (const FTimespan & Other) { Time += Other; return *this; }
	FMediaTimeStamp& operator -= (const FTimespan & Other) { Time -= Other; return *this; }

	FMediaTimeStamp& operator -= (const FMediaTimeStamp & Other) { Time -= Other.Time; SequenceIndex = MAX_int64; return *this; }

	FTimespan Time;
	int64 SequenceIndex;
};

class FMediaTimeStampSample
{
public:
	FMediaTimeStampSample() : SampledAtTime(-1.0) {}
	FMediaTimeStampSample(const FMediaTimeStamp & InTimeStamp, double InSampledAtTime) : TimeStamp(InTimeStamp), SampledAtTime(InSampledAtTime) {}

	void Invalidate() { TimeStamp.Invalidate(); }
	bool IsValid() const { return TimeStamp.IsValid(); }

	FMediaTimeStamp TimeStamp;
	double SampledAtTime;
};

/**
 * Interface for media clock time sources.
 */
class IMediaTimeSource
{
public:

	/**
	 * Get the current time code.
	 *
	 * @return Time code.
	 */
	virtual FTimespan GetTimecode() = 0;

public:

	/** Virtual destructor. */
	virtual ~IMediaTimeSource() { }
};
