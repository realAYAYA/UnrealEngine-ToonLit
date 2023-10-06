// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/Core.h"

#include <limits>

namespace Chaos
{
	namespace Private
	{
		/**
		 * Iteration counts for use by bodies and constraints. 
		 * A simulation island will use the maximum number of iterations of any body or constraint in the island.
		*/
		class FIterationSettings
		{
		private:
			using FIndexType = int8;
			FIndexType NumPositionIterations;
			FIndexType NumVelocityIterations;
			FIndexType NumProjectionIterations;

		public:
			static const int32 InvalidIterations = INDEX_NONE;
			static const int32 MaxPositionIterations = std::numeric_limits<FIndexType>::max();
			static const int32 MaxVelocityIterations = std::numeric_limits<FIndexType>::max();
			static const int32 MaxProjectionIterations = std::numeric_limits<FIndexType>::max();

			static FIterationSettings Empty()
			{
				return FIterationSettings(InvalidIterations, InvalidIterations, InvalidIterations);
			}

			FIterationSettings()
				: NumPositionIterations(InvalidIterations)
				, NumVelocityIterations(InvalidIterations)
				, NumProjectionIterations(InvalidIterations)
			{
			}

			FIterationSettings(const int32 InNumPositionIterations, const int32 InNumVelocityIterations, const int32 InNumProjectionInterations)
			{
				SetNumPositionIterations(InNumPositionIterations);
				SetNumVelocityIterations(InNumVelocityIterations);
				SetNumProjectionIterations(InNumProjectionInterations);
			}

			int32 GetNumPositionIterations() const { return NumPositionIterations; }
			int32 GetNumVelocityIterations() const { return NumVelocityIterations; }
			int32 GetNumProjectionIterations() const { return NumProjectionIterations; }

			void SetNumPositionIterations(const int32 InNum) { NumPositionIterations = FIndexType(FMath::Clamp(InNum, InvalidIterations, MaxPositionIterations)); }
			void SetNumVelocityIterations(const int32 InNum) { NumVelocityIterations = FIndexType(FMath::Clamp(InNum, InvalidIterations, MaxVelocityIterations)); }
			void SetNumProjectionIterations(const int32 InNum) { NumProjectionIterations = FIndexType(FMath::Clamp(InNum, InvalidIterations, MaxProjectionIterations)); }

			static FIterationSettings Merge(const FIterationSettings& L, const FIterationSettings& R)
			{
				FIterationSettings Result;
				Result.NumPositionIterations = FMath::Max(L.NumPositionIterations, R.NumPositionIterations);
				Result.NumVelocityIterations = FMath::Max(L.NumVelocityIterations, R.NumVelocityIterations);
				Result.NumProjectionIterations = FMath::Max(L.NumProjectionIterations, R.NumProjectionIterations);
				return Result;
			}
		};
	}
}
