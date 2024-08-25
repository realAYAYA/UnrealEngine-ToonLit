// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ComponentVisualizer.h"
#include "IChaosVDParticleVisualizationDataProvider.h"
#include "DataWrappers/ChaosVDParticleDataWrapper.h"
#include "HAL/Platform.h"
#include "Templates/SharedPointer.h"

class FChaosVDGeometryBuilder;
struct FChaosParticleDataDebugDrawSettings;

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
	ClusterConnectivityEdge	= 1 << 6,
	CenterOfMass	= 1 << 7,
	DrawDataOnlyForSelectedParticle	= 1 << 8,
	EnableDraw	= 1 << 9,
};
ENUM_CLASS_FLAGS(EChaosVDParticleDataVisualizationFlags);

struct FChaosVDVisualizedParticleDataSelectionHandle
{
	int32 ParticleIndex = INDEX_NONE;
	int32 SolverID = INDEX_NONE;
};

struct FChaosParticleDataDebugDrawSettings;

struct FChaosVDParticleDataVisualizationContext : public FChaosVDVisualizationContext
{
	TWeakPtr<FChaosVDGeometryBuilder> GeometryGenerator = nullptr;
	bool bIsSelectedData = false;
	bool bShowDebugText = false;

	const FChaosParticleDataDebugDrawSettings* DebugDrawSettings = nullptr;

	bool IsVisualizationFlagEnabled(EChaosVDParticleDataVisualizationFlags Flag) const
	{
		const EChaosVDParticleDataVisualizationFlags FlagsAsParticleFlags = static_cast<EChaosVDParticleDataVisualizationFlags>(VisualizationFlags);
		return EnumHasAnyFlags(FlagsAsParticleFlags, Flag);
	}
};

namespace Chaos::VisualDebugger::ParticleDataUnitsStrings
{
	static FString Velocity = TEXT("cm/s");
	static FString AngularVelocity = TEXT("rad/s");
	static FString Acceleration = TEXT("cm/s2");
	static FString AngularAcceleration = TEXT("rad/s2");
	static FString LinearImpulse = TEXT("g.m/s");
	static FString AngularImpulse = TEXT("g.m2/s");

	static FString GetUnitByID(EChaosVDParticleDataVisualizationFlags DataID)
	{
		switch (DataID)
		{
		case EChaosVDParticleDataVisualizationFlags::Velocity:
			return Velocity;
		case EChaosVDParticleDataVisualizationFlags::AngularVelocity:
			return AngularVelocity;
		case EChaosVDParticleDataVisualizationFlags::Acceleration:
			return Acceleration;
		case EChaosVDParticleDataVisualizationFlags::AngularAcceleration:
			return AngularAcceleration;
		case EChaosVDParticleDataVisualizationFlags::LinearImpulse:
			return LinearImpulse;
		case EChaosVDParticleDataVisualizationFlags::AngularImpulse:
			return AngularImpulse;

		case EChaosVDParticleDataVisualizationFlags::None:
		case EChaosVDParticleDataVisualizationFlags::ClusterConnectivityEdge:
		case EChaosVDParticleDataVisualizationFlags::CenterOfMass:
		case EChaosVDParticleDataVisualizationFlags::DrawDataOnlyForSelectedParticle:
		default:
			return TEXT("");
		}
	}
}

/** Custom Hit Proxy for debug drawn particle data */
struct HChaosVDParticleDataProxy : public HComponentVisProxy
{
	DECLARE_HIT_PROXY()
	
	HChaosVDParticleDataProxy(const UActorComponent* Component, const FChaosVDVisualizedParticleDataSelectionHandle& InContactFinderData) : HComponentVisProxy(Component, HPP_UI), DataSelectionHandle(InContactFinderData)
	{	
	}

	virtual EMouseCursor::Type GetMouseCursor() override
	{
		return EMouseCursor::Crosshairs;
	}

	FChaosVDVisualizedParticleDataSelectionHandle DataSelectionHandle;
};

/**
 * Component visualizer in charge of generating debug draw visualizations for for particles
 */
class FChaosVDParticleDataComponentVisualizer : public FComponentVisualizer
{
public:
	virtual void DrawVisualization(const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI) override;
	virtual bool VisProxyHandleClick(FEditorViewportClient* InViewportClient, HComponentVisProxy* VisProxy, const FViewportClick& Click) override;

protected:
	void DrawParticleVector(FPrimitiveDrawInterface* PDI, const FVector& StartLocation, const FVector& InVector, EChaosVDParticleDataVisualizationFlags VectorID, const FChaosVDParticleDataVisualizationContext& InVisualizationContext, float LineThickness);
	void DrawVisualizationForParticleData(const UActorComponent* Component, FPrimitiveDrawInterface* PDI, const FSceneView* View, const FChaosVDParticleDataVisualizationContext& InVisualizationContext, const FChaosVDParticleDataWrapper& InParticleDataViewer);
};
