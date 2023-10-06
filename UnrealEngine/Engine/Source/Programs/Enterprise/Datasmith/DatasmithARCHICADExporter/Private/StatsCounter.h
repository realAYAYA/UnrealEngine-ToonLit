// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Utils/AddonTools.h"

#include <limits>

#define HAS_STD_ATOMIC 1
#ifdef WIN32
	#include <windows.h>
	#if (_MSC_VER <= 1600)
		#undef HAS_STD_ATOMIC
		#define HAS_STD_ATOMIC 0
	#endif
#endif
#if HAS_STD_ATOMIC
	#include <atomic>
#endif

BEGIN_NAMESPACE_UE_AC

#if HAS_STD_ATOMIC

typedef volatile std::atomic_int  TAtomicInt;
typedef volatile std::atomic_uint TAtomicUInt;

#else

// Simulate used part of std::atomic
template < class Type > class TAtomic
{
  public:
	TAtomic() {}
	TAtomic(Type v)
		: m((LONG)v)
	{
	}
		 operator Type() const { return Type(m); }
	Type operator++() volatile { return Type(InterlockedIncrement(&m)); }
	Type operator--() volatile { return Type(InterlockedDecrement(&m)); }
	Type operator+=(Type v) volatile { return Type(InterlockedExchangeAdd(&m, (LONG)v)) + v; }
	bool operator==(Type v) const { return m == (LONG)v; }

	LONG volatile m;
};

typedef class TAtomic< int >		  TAtomicInt;
typedef class TAtomic< unsigned int > TAtomicUInt;

#endif

// Template to do help doing some stat
template < int First, int Last > class TStatsCounter
{
  public:
	// Constructor
	TStatsCounter() { Reset(); }

	// Reset all counter to 0
	void Reset()
	{
		Min = std::numeric_limits< int >::max();
		Max = std::numeric_limits< int >::min();
		Totals = 0;
		for (size_t i = 0; i < kSize; i++)
			Stat[i] = 0;
	}

	// Inclement counteur for specified index
	void Inc(int InNb)
	{
		Totals += InNb;
		if (InNb < Min)
			Min = InNb;
		if (InNb > Max)
			Max = InNb;
		if (InNb <= kFirst)
			++Stat[0];
		else if (InNb >= kLast)
			++Stat[kLast - kFirst];
		else
			++Stat[InNb - kFirst];
	}

	// Return formated counter state
	std::string asStrings() const
	{
		if (Totals == 0)
			return "Total=0";

		std::string s(Utf8StringFormat("Total=%u, Min=%d, Max=%d, [-∞..%d]=%d", int(Totals), int(Min), int(Max), kFirst,
									   int(Stat[0])));
		for (int i = kFirst + 1; i < kLast; i++)
			s += Utf8StringFormat(", [%d]=%d", i, int(Stat[i - kFirst]));
		return s + Utf8StringFormat(", [%d..∞]=%d", kLast, int(Stat[kLast - kFirst]));
	}

  private:
	enum
	{
		kFirst = First,
		kLast = Last,
		kSize = kLast - kFirst + 1
	};
	TAtomicInt Totals; // Total of Inc called
	TAtomicInt Min; // Maximum index value
	TAtomicInt Max; // Minimum index value
	TAtomicInt Stat[kSize]; // Counters index collected
};

END_NAMESPACE_UE_AC
