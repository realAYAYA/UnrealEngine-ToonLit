// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"

namespace UE::DerivedData
{

/** Binary predicate that compares chunks by key, then value ID, then raw offset. */
struct TChunkEqual
{
	template <typename ChunkTypeA, typename ChunkTypeB>
	inline bool operator()(ChunkTypeA&& A, ChunkTypeB&& B) const
	{
		return A.Key == B.Key && A.Id == B.Id && A.RawOffset == B.RawOffset;
	}
};
/** Binary predicate that compares chunks by key, then value ID, then raw offset. */
struct TChunkLess
{
	template <typename ChunkTypeA, typename ChunkTypeB>
	inline bool operator()(ChunkTypeA&& A, ChunkTypeB&& B) const
	{
		if (A.Key < B.Key)
		{
			return true;
		}
		if (B.Key < A.Key)
		{
			return false;
		}
		if (A.Id < B.Id)
		{
			return true;
		}
		if (B.Id < A.Id)
		{
			return false;
		}
		return A.RawOffset < B.RawOffset;
	}
};

} // UE::DerivedData
