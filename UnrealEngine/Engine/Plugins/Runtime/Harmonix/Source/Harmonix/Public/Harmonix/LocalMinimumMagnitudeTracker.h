// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Math/UnrealMathUtility.h"
#include <limits>

template <int SIZE>
class FLocalMinimumMagnitudeTracker
{
public:
	FLocalMinimumMagnitudeTracker()	{ Reset(); }

   void Reset()
   {
      ring[0] = std::numeric_limits<float>::max();
      nextWrite = 1;
      minPosition = 0;
   }

   void Push(float v)
   {
      ring[nextWrite] = v;
      if (nextWrite == minPosition)
      {
         // search for new lowest...
         minPosition = (nextWrite + 1) % SIZE;
         for (int i = 1; i < SIZE; ++i)
         {
            int testIdx = (nextWrite + 1 + i) % SIZE;
            if (FMath::Abs(ring[testIdx]) <= FMath::Abs(ring[minPosition]))
               minPosition = testIdx;
         }
      }
      else if (FMath::Abs(v) <= FMath::Abs(ring[minPosition]))
         minPosition = nextWrite;
      nextWrite = (nextWrite + 1) % SIZE;
   }

   float Min() const { return ring[minPosition]; }

private:
   float ring[SIZE];
   int   nextWrite = 0;
   int   minPosition = 0;
};
