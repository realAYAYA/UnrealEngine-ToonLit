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
			: CollisionContainer(InCollisionContainer)
		{
		}

		virtual ~FCollisionDetector() {}

		UE_DEPRECATED(5.2, "Moved to FPBDCollisionConstraints")
		const FCollisionDetectorSettings& GetSettings() const;

		UE_DEPRECATED(5.2, "Moved to FPBDCollisionConstraints")
		void SetSettings(const FCollisionDetectorSettings& InSettings);

		UE_DEPRECATED(5.2, "Moved to FPBDCollisionConstraints and renamed to SetCullDistance")
		void SetBoundsExpansion(const FReal InBoundsExpansion);

		UE_DEPRECATED(5.2, "No longer supported")
		void SetBoundsVelocityInflation(const FReal InBoundsVelocityInflation) {}

		FPBDCollisionConstraints& GetCollisionContainer() { return CollisionContainer; }

		virtual void DetectCollisions(const FReal Dt, FEvolutionResimCache* ResimCache) = 0;

	protected:
		FPBDCollisionConstraints& CollisionContainer;
	};

}
