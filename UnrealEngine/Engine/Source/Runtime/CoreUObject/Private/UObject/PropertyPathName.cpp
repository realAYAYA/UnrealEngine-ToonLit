// Copyright Epic Games, Inc. All Rights Reserved.

#include "UObject/PropertyPathName.h"

#include "Misc/StringBuilder.h"
#include "Templates/TypeHash.h"

namespace UE
{

inline bool FPropertyPathName::FSegment::operator==(const FSegment& Segment) const
{
	return NameWithIndex == Segment.NameWithIndex && Type == Segment.Type;
}

inline bool FPropertyPathName::FSegment::operator<(const FSegment& Segment) const
{
	return Compare(Segment) < 0;
}

inline int32 FPropertyPathName::FSegment::Compare(const FSegment& Segment) const
{
	if (int32 CompareNameWithIndex = NameWithIndex.Compare(Segment.NameWithIndex))
	{
		return CompareNameWithIndex;
	}
	return (Type == Segment.Type) ? 0 : (Type < Segment.Type ? -1 : 1);
}

bool FPropertyPathName::operator==(const FPropertyPathName& Path) const
{
	const int32 SegmentCount = Segments.Num();
	if (SegmentCount != Path.Segments.Num())
	{
		return false;
	}

	for (int32 SegmentIndex = SegmentCount - 1; SegmentIndex >= 0; --SegmentIndex)
	{
		if (!(Segments[SegmentIndex] == Path.Segments[SegmentIndex]))
		{
			return false;
		}
	}

	return true;
}

bool FPropertyPathName::operator<(const FPropertyPathName& Path) const
{
	const int32 SegmentCountA = Segments.Num();
	const int32 SegmentCountB = Path.Segments.Num();
	const int32 SegmentCountMin = FPlatformMath::Min(SegmentCountA, SegmentCountB);

	for (int32 SegmentIndex = 0; SegmentIndex < SegmentCountMin; ++SegmentIndex)
	{
		if (const int32 Compare = Segments[SegmentIndex].Compare(Path.Segments[SegmentIndex]))
		{
			return Compare < 0;
		}
	}

	return SegmentCountA < SegmentCountB;
}

void FPropertyPathName::ToString(FStringBuilderBase& Out, FStringView Separator) const
{
	bool bFirst = true;
	for (const FSegment& Segment : Segments)
	{
		if (bFirst)
		{
			bFirst = false;
		}
		else
		{
			Out.Append(Separator);
		}

		FPropertyPathNameSegment UnpackedSegment = Segment.Unpack();
		Out << UnpackedSegment.Name;

		if (const int32 Index = UnpackedSegment.Index; Index != INDEX_NONE)
		{
			Out << TEXT('[') << Index << TEXT(']');
		}

		if (!UnpackedSegment.Type.IsEmpty())
		{
			Out << TEXTVIEW(" (") << UnpackedSegment.Type << TEXT(')');
		}
	}
}

uint32 GetTypeHash(const FPropertyPathName& Path)
{
	uint32 Hash = 0;
	for (const FPropertyPathName::FSegment& Segment : Path.Segments)
	{
		Hash = HashCombineFast(Hash, GetTypeHash(Segment.NameWithIndex));
		Hash = HashCombineFast(Hash, GetTypeHash(Segment.Type));
	}
	return Hash;
}

} // UE
