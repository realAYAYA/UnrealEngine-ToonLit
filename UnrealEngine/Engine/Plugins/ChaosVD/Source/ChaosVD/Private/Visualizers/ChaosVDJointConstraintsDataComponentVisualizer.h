// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "ComponentVisualizer.h"
#include "IChaosVDParticleVisualizationDataProvider.h"
#include "Chaos/Core.h"
#include "Components/ChaosVDSolverJointConstraintDataComponent.h"

struct FChaosVDJointsDebugDrawSettings;
class AChaosVDSolverInfoActor;
struct FChaosVDJointConstraint;

UENUM(meta = (Bitflags, UseEnumValuesAsMaskValuesInEditor = "true"))
enum class EChaosVDJointsDataVisualizationFlags : uint32
{
	None					= 0 UMETA(Hidden),
	/** Draw the PushOut vector based on the constraint's data */
	PushOut					= 1 << 0,
	/** Draw the Angular Impulse vector based on the constraint's data */
	AngularImpulse			= 1 << 1 UMETA(Hidden),
	ActorConnector			= 1 << 2,
	CenterOfMassConnector	= 1 << 3,
	Stretch					= 1 << 4,
	Axes					= 1 << 5,
	/** Draw the joint even if one of the particles or both are kinematic */
	DrawKinematic			= 1 << 6,
	/** Draw the joint even if it is disabled */
	DrawDisabled			= 1 << 7,
	/** Only debugs draw data for a selected joint constraint */
	OnlyDrawSelected		= 1 << 8,
	/** Enables Debug draw for Joint Constraint data from any solver that is visible */
	EnableDraw				= 1 << 9,
};
ENUM_CLASS_FLAGS(EChaosVDJointsDataVisualizationFlags);

/** Visualization context structure specific for Joints visualizations */
struct FChaosVDJointVisualizationDataContext : public FChaosVDVisualizationContext
{
	FChaosVDJointConstraintSelectionHandle DataSelectionHandle = FChaosVDJointConstraintSelectionHandle(nullptr);
	bool bIsServerVisualizationEnabled = false;
	
	AChaosVDSolverInfoActor* SolverInfoActor = nullptr;

	const FChaosVDJointsDebugDrawSettings* DebugDrawSettings = nullptr;

	bool bShowDebugText = false;

	bool IsVisualizationFlagEnabled(EChaosVDJointsDataVisualizationFlags Flag) const
	{
		const EChaosVDJointsDataVisualizationFlags FlagsAsParticleFlags = static_cast<EChaosVDJointsDataVisualizationFlags>(VisualizationFlags);
		return EnumHasAnyFlags(FlagsAsParticleFlags, Flag);
	}
};

/** Custom Hit Proxy for debug drawn scene queries */
struct HChaosVDJointConstraintProxy : public HComponentVisProxy
{
	DECLARE_HIT_PROXY()
	
	HChaosVDJointConstraintProxy(const UActorComponent* Component, const FChaosVDJointConstraintSelectionHandle& InContactFinderData) : HComponentVisProxy(Component, HPP_UI), DataSelectionHandle(InContactFinderData)
	{	
	}

	virtual EMouseCursor::Type GetMouseCursor() override
	{
		return EMouseCursor::Crosshairs;
	}

	FChaosVDJointConstraintSelectionHandle DataSelectionHandle;
};

/**
 * Component visualizer in charge of generating debug draw visualizations for Joint Constraints in a UChaosVDSolverJointConstraintDataComponent
 */
class FChaosVDJointConstraintsDataComponentVisualizer final : public FComponentVisualizer
{
public:
	FChaosVDJointConstraintsDataComponentVisualizer()
	{
	}

	virtual void DrawVisualization(const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI) override;
	virtual bool VisProxyHandleClick(FEditorViewportClient* InViewportClient, HComponentVisProxy* VisProxy, const FViewportClick& Click) override;

protected:

	void DebugDrawAllAxis(const FChaosVDJointConstraint& InJointConstraintData, FChaosVDJointVisualizationDataContext& VisualizationContext, FPrimitiveDrawInterface* PDI, const float LineThickness, const FVector& InPosition, const Chaos::FMatrix33& InRotationMatrix, TConstArrayView<FLinearColor>);
	void DrawJointConstraint(const UActorComponent* Component, const FChaosVDJointConstraint& InJointConstraintData, FChaosVDJointVisualizationDataContext& VisualizationContext, const FSceneView* View, FPrimitiveDrawInterface* PDI);
};
