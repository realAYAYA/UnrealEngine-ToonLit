// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/Collision/CollisionContext.h"

#include "ChaosStats.h"

namespace Chaos
{
	class FPBDCollisionConstraints;
	class FEvolutionResimCache;

	class CHAOS_API FCollisionDetector
	{
	public:
		FCollisionDetector(FPBDCollisionConstraints& InCollisionContainer)
			: Settings()
			, CollisionContainer(InCollisionContainer)
		{
		}

		virtual ~FCollisionDetector() {}

		const FCollisionDetectorSettings& GetSettings() const
		{
			return Settings;
		}

		void SetSettings(const FCollisionDetectorSettings& InSettings)
		{
			Settings = InSettings;
		}

		void SetBoundsExpansion(const FReal InBoundsExpansion)
		{
			Settings.BoundsExpansion = InBoundsExpansion;
		}

		void SetBoundsVelocityInflation(const FReal InBoundsVelocityInflation)
		{
			Settings.BoundsVelocityInflation = InBoundsVelocityInflation;
		}

		FPBDCollisionConstraints& GetCollisionContainer() { return CollisionContainer; }

		virtual void DetectCollisions(const FReal Dt, FEvolutionResimCache* ResimCache) = 0;

	protected:
		FCollisionDetectorSettings Settings;
		FPBDCollisionConstraints& CollisionContainer;
	};

}
