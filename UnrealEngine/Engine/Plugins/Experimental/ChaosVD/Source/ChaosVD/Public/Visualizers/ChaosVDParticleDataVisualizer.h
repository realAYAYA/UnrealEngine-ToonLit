// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "ChaosVDDataVisualizerBase.h"
#include "IChaosVDParticleVisualizationDataProvider.h"

#include "ChaosVDParticleDataVisualizer.generated.h"

struct FChaosVDParticleDataWrapper;

UENUM(meta = (Bitflags, UseEnumValuesAsMaskValuesInEditor = "true"))
enum class EChaosVDParticleDataVisualizationFlags : uint32
{
	None				= 0 UMETA(Hidden),
	Velocity			= 1 << 0,
	AngularVelocity		= 1 << 1,
	Acceleration		= 1 << 2,
	AngularAcceleration = 1 << 3,
	LinearImpulse		= 1 << 4,
	AngularImpulse		= 1 << 5,
};
ENUM_CLASS_FLAGS(EChaosVDParticleDataVisualizationFlags);

class FChaosVDParticleDataVisualizer final : public FChaosVDDataVisualizerBase
{
public:
	explicit FChaosVDParticleDataVisualizer(IChaosVDParticleVisualizationDataProvider& InProvider)
		: FChaosVDDataVisualizerBase(), DataProvider(InProvider)
	{
	}

	virtual void DrawVisualization(const FSceneView* View, FPrimitiveDrawInterface* PDI) override;

	inline static const FStringView VisualizerID = TEXT("ParticleDataVisualizer");

protected:
	IChaosVDParticleVisualizationDataProvider& DataProvider;
};
