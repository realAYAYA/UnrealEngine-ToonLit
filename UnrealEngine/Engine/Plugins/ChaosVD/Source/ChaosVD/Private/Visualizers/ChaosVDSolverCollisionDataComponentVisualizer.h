// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "ComponentVisualizer.h"
#include "HitProxies.h"
#include "DataWrappers/ChaosVDCollisionDataWrappers.h"

struct FChaosVDParticlePairMidPhase;
struct FChaosVDVisualizationContext;
class FChaosVDScene;

struct FChaosVDCollisionDataFinder
{
	TWeakPtr<FChaosVDParticlePairMidPhase> OwningMidPhase;
	FChaosVDConstraint* OwningConstraint = nullptr;
	int32 ContactIndex = INDEX_NONE;

	void SetIsSelected(bool bNewSelected);
};

struct HChaosVDContactPointProxy : public HComponentVisProxy
{
	DECLARE_HIT_PROXY()
	
	HChaosVDContactPointProxy(const UActorComponent* Component, const FChaosVDCollisionDataFinder& InContactFinderData) : HComponentVisProxy(Component, HPP_UI), ContactFinder(InContactFinderData)
	{	
	}

	virtual EMouseCursor::Type GetMouseCursor() override
	{
		return EMouseCursor::Crosshairs;
	}

	FChaosVDCollisionDataFinder ContactFinder;
};

UENUM(meta = (Bitflags, UseEnumValuesAsMaskValuesInEditor = "true"))
enum class EChaosVDCollisionVisualizationFlags: uint32
{
	None							= 0 UMETA(Hidden),
	ContactPoints					= 1 << 0,
	ContactInfo						= 1 << 1,
	NetPushOut						= 1 << 2,
	NetImpulse						= 1 << 3,
	ContactNormal					= 1 << 4,
	AccumulatedImpulse				= 1 << 5,
	DrawInactiveContacts			= 1 << 6,
	DrawDataOnlyForSelectedParticle	= 1 << 7,
	EnableDraw						= 1 << 8,
};
ENUM_CLASS_FLAGS(EChaosVDCollisionVisualizationFlags);

class FChaosVDSolverCollisionDataComponentVisualizer : public FComponentVisualizer
{
public:
	FChaosVDSolverCollisionDataComponentVisualizer();
	~FChaosVDSolverCollisionDataComponentVisualizer();

	virtual bool ShowWhenSelected() override;
	virtual void DrawVisualization(const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI) override;

	virtual bool VisProxyHandleClick(FEditorViewportClient* InViewportClient, HComponentVisProxy* VisProxy, const FViewportClick& Click) override;

protected:
	
	void ClearCurrentSelection();

	void DrawMidPhaseData(const UActorComponent* Component, const TSharedPtr<FChaosVDParticlePairMidPhase>& MidPhase, const FChaosVDVisualizationContext& VisualizationContext, const FSceneView* View, FPrimitiveDrawInterface* PDI);

	FChaosVDCollisionDataFinder CurrentSelectedContactData;
};
