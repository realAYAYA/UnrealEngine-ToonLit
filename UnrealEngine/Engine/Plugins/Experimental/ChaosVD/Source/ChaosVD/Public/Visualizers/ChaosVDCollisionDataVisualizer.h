// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "ChaosVDDataVisualizerBase.h"
#include "IChaosVDParticleVisualizationDataProvider.h"

#include "ChaosVDCollisionDataVisualizer.generated.h"

struct FChaosVDParticlePairMidPhase;
struct FChaosVDParticleDataWrapper;

UENUM(meta = (Bitflags, UseEnumValuesAsMaskValuesInEditor = "true"))
enum class EChaosVDCollisionVisualizationFlags: uint32
{
	None					= 0 UMETA(Hidden),
	ContactPoints			= 1 << 0,
	NetPushOut				= 1 << 2,
	NetImpulse				= 1 << 3,
	ContactNormal			= 1 << 4,
	AccumulatedImpulse		= 1 << 5,
	DrawOnlyActiveContacts	= 1 << 6,
};
ENUM_CLASS_FLAGS(EChaosVDCollisionVisualizationFlags);

class FChaosVDCollisionDataVisualizer final : public FChaosVDDataVisualizerBase
{
public:
	explicit FChaosVDCollisionDataVisualizer(IChaosVDParticleVisualizationDataProvider& InProvider)
		: FChaosVDDataVisualizerBase(), DataProvider(InProvider)
	{
	}

	virtual void DrawVisualization(const FSceneView* View, FPrimitiveDrawInterface* PDI) override;
	
	inline static const FStringView VisualizerID = TEXT("CollisionVisualizer");

protected:
	IChaosVDParticleVisualizationDataProvider& DataProvider;
};
