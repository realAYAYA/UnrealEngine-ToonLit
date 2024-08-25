// Copyright Epic Games, Inc. All Rights Reserved.

#include "Visualizers/ChaosVDParticleDataComponentVisualizer.h"

#include "Actors/ChaosVDSolverInfoActor.h"
#include "ChaosVDEditorSettings.h"
#include "ChaosVDParticleActor.h"
#include "ChaosVDScene.h"
#include "SceneView.h"
#include "Components/ChaosVDParticleDataComponent.h"
#include "DataWrappers/ChaosVDParticleDataWrapper.h"
#include "Visualizers/ChaosVDDebugDrawUtils.h"

IMPLEMENT_HIT_PROXY(HChaosVDParticleDataProxy, HComponentVisProxy)

#define LOCTEXT_NAMESPACE "ChaosVisualDebugger"

/** Sets a Hit proxy which will be cleared out as soon this struct goes out of scope*/
struct FChaosVDScopedParticleHitProxy
{
	FChaosVDScopedParticleHitProxy(FPrimitiveDrawInterface* PDI, HHitProxy* HitProxy)
	{
		PDIPtr = PDI;
		if (PDIPtr)
		{
			PDIPtr->SetHitProxy(HitProxy);
		}
	}

	~FChaosVDScopedParticleHitProxy()
	{
		if (PDIPtr)
		{
			PDIPtr->SetHitProxy(nullptr);
		}
	}
	
	FPrimitiveDrawInterface* PDIPtr = nullptr;
};

void FChaosVDParticleDataComponentVisualizer::DrawVisualization(const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI)
{
	const UChaosVDEditorSettings* EditorSettings = GetDefault<UChaosVDEditorSettings>();
	if (!EditorSettings)
	{
		return;
	}

	if (EditorSettings->GlobalParticleDataVisualizationFlags == 0)
	{
		// Nothing to visualize
		return;
	}

	const UChaosVDParticleDataComponent* ParticleDataComponent = Cast<UChaosVDParticleDataComponent>(Component);
	if (!ParticleDataComponent)
	{
		return;
	}

	AChaosVDSolverInfoActor* SolverDataActor = Cast<AChaosVDSolverInfoActor>(Component->GetOwner());
	if (!SolverDataActor)
	{
		return;
	}

	if (!SolverDataActor->IsVisible())
	{
		return;
	}

	const TSharedPtr<FChaosVDScene> CVDScene = SolverDataActor->GetScene().Pin();
	if (!CVDScene)
	{
		return;
	}

	FChaosVDParticleDataVisualizationContext VisualizationContext;
	VisualizationContext.VisualizationFlags = EditorSettings->GlobalParticleDataVisualizationFlags;
	VisualizationContext.SpaceTransform = SolverDataActor->GetSimulationTransform();
	VisualizationContext.CVDScene = CVDScene;
	VisualizationContext.GeometryGenerator = CVDScene->GetGeometryGenerator();
	VisualizationContext.bShowDebugText = EditorSettings->bShowDebugText;
	VisualizationContext.DebugDrawSettings = &EditorSettings->ParticleDataDebugDrawSettings;

	if (!VisualizationContext.IsVisualizationFlagEnabled(EChaosVDParticleDataVisualizationFlags::EnableDraw))
	{
		return;
	}

	if (VisualizationContext.IsVisualizationFlagEnabled(EChaosVDParticleDataVisualizationFlags::DrawDataOnlyForSelectedParticle))
	{
		VisualizationContext.bIsSelectedData = true;
		SolverDataActor->VisitSelectedParticleData([this, PDI, View, &VisualizationContext, Component](const FChaosVDParticleDataWrapper& InParticleDataViewer)
		{
			DrawVisualizationForParticleData(Component, PDI, View, VisualizationContext, InParticleDataViewer);

			return true;
		});
	}
	else
	{
		SolverDataActor->VisitAllParticleData([this, PDI, View, &VisualizationContext, Component, SolverDataActor](const FChaosVDParticleDataWrapper& InParticleDataViewer)
		{
			VisualizationContext.bIsSelectedData = SolverDataActor->IsParticleSelectedByID(InParticleDataViewer.ParticleIndex);
			DrawVisualizationForParticleData(Component, PDI, View, VisualizationContext, InParticleDataViewer);

			// If we reach the debug draw limit for this frame, there is no need to continue processing particles
			return FChaosVDDebugDrawUtils::CanDebugDraw();
		});
	}
}

bool FChaosVDParticleDataComponentVisualizer::VisProxyHandleClick(FEditorViewportClient* InViewportClient, HComponentVisProxy* VisProxy, const FViewportClick& Click)
{
	bool bHandled = false;

	const HChaosVDParticleDataProxy* ParticleDataProxy = HitProxyCast<HChaosVDParticleDataProxy>(VisProxy);
	if (!ParticleDataProxy)
	{
		return bHandled;
	}
	
	const UChaosVDParticleDataComponent* ParticleDataComponent = Cast<UChaosVDParticleDataComponent>(VisProxy->Component.Get());
	if (!ParticleDataComponent)
	{
		return bHandled;
	}

	AChaosVDSolverInfoActor* SolverDataActor = Cast<AChaosVDSolverInfoActor>(ParticleDataComponent->GetOwner());
	if (!SolverDataActor)
	{
		return bHandled;
	}

	bHandled = SolverDataActor->SelectParticleByID(ParticleDataProxy->DataSelectionHandle.ParticleIndex);

	return bHandled;	
}

void FChaosVDParticleDataComponentVisualizer::DrawParticleVector(FPrimitiveDrawInterface* PDI, const FVector& StartLocation, const FVector& InVector, EChaosVDParticleDataVisualizationFlags VectorID, const FChaosVDParticleDataVisualizationContext& InVisualizationContext, float LineThickness)
{
	if (!InVisualizationContext.IsVisualizationFlagEnabled(VectorID))
	{
		return;
	}

	if (!ensure(InVisualizationContext.DebugDrawSettings))
	{
		return;
	}

	const FString DebugText = InVisualizationContext.bShowDebugText ? Chaos::VisualDebugger::Utils::GenerateDebugTextForVector(InVector, UEnum::GetDisplayValueAsText(VectorID).ToString(), Chaos::VisualDebugger::ParticleDataUnitsStrings::GetUnitByID(VectorID)) : TEXT("");
	FChaosVDDebugDrawUtils::DrawArrowVector(PDI, StartLocation, StartLocation +  InVisualizationContext.DebugDrawSettings->GetScaleFortDataID(VectorID) * InVector, FText::AsCultureInvariant(DebugText), InVisualizationContext.DebugDrawSettings->ColorSettings.GetColorForDataID(VectorID, InVisualizationContext.bIsSelectedData),  InVisualizationContext.DebugDrawSettings->DepthPriority, LineThickness);
}

void FChaosVDParticleDataComponentVisualizer::DrawVisualizationForParticleData(const UActorComponent* Component, FPrimitiveDrawInterface* PDI, const FSceneView* View, const FChaosVDParticleDataVisualizationContext& InVisualizationContext, const FChaosVDParticleDataWrapper& InParticleDataViewer)
{
	using namespace Chaos::VisualDebugger::ParticleDataUnitsStrings;

	if (!View)
	{
		return;
	}

	if (!ensure(InVisualizationContext.DebugDrawSettings))
	{
		return;
	}

	const FVector& OwnerLocation = InVisualizationContext.SpaceTransform.TransformPosition(InParticleDataViewer.ParticlePositionRotation.MX);

	// TODO: See how expensive is get the bounds. It is not something we have recorded
	constexpr float VisibleRadius = 50.0f;
	if (!View->ViewFrustum.IntersectSphere(OwnerLocation, VisibleRadius))
	{
		// If this particle location is not even visible, just ignore it.
		return;
	}

	const FQuat& OwnerRotation =  InVisualizationContext.SpaceTransform.TransformRotation(InParticleDataViewer.ParticlePositionRotation.MR);
	const FVector OwnerCoMLocation = OwnerLocation + OwnerRotation *  InVisualizationContext.SpaceTransform.TransformPosition(InParticleDataViewer.ParticleMassProps.MCenterOfMass);
	
	FChaosVDScopedParticleHitProxy ScopedHitProxy(PDI, new HChaosVDParticleDataProxy(Component, {InParticleDataViewer.ParticleIndex, InParticleDataViewer.SolverID} ));

	constexpr float DefaultLineThickness = 1.5f;
	constexpr float SelectedLineThickness = 3.5f;
	const float LineThickness = InVisualizationContext.bIsSelectedData ? SelectedLineThickness : DefaultLineThickness;
	

	if (InParticleDataViewer.ParticleVelocities.HasValidData())
	{
		DrawParticleVector(PDI, OwnerCoMLocation, InParticleDataViewer.ParticleVelocities.MV,EChaosVDParticleDataVisualizationFlags::Velocity, InVisualizationContext, LineThickness); 
		DrawParticleVector(PDI, OwnerCoMLocation, InParticleDataViewer.ParticleVelocities.MW,EChaosVDParticleDataVisualizationFlags::AngularVelocity, InVisualizationContext, LineThickness); 
	}

	if (InParticleDataViewer.ParticleDynamics.HasValidData())
	{
		DrawParticleVector(PDI, OwnerCoMLocation, InParticleDataViewer.ParticleDynamics.MAcceleration,EChaosVDParticleDataVisualizationFlags::Acceleration, InVisualizationContext, LineThickness); 
		DrawParticleVector(PDI, OwnerCoMLocation, InParticleDataViewer.ParticleDynamics.MAngularAcceleration,EChaosVDParticleDataVisualizationFlags::AngularAcceleration, InVisualizationContext, LineThickness); 
		DrawParticleVector(PDI, OwnerCoMLocation, InParticleDataViewer.ParticleDynamics.MLinearImpulseVelocity,EChaosVDParticleDataVisualizationFlags::LinearImpulse, InVisualizationContext, LineThickness); 
		DrawParticleVector(PDI, OwnerCoMLocation, InParticleDataViewer.ParticleDynamics.MAngularImpulseVelocity,EChaosVDParticleDataVisualizationFlags::AngularImpulse, InVisualizationContext, LineThickness);
	}

	if (InParticleDataViewer.ParticleMassProps.HasValidData())
	{
		if (InVisualizationContext.IsVisualizationFlagEnabled(EChaosVDParticleDataVisualizationFlags::CenterOfMass))
		{
			if (const TSharedPtr<FChaosVDGeometryBuilder> GeometryGenerator = InVisualizationContext.GeometryGenerator.Pin())
			{
				FCollisionShape Sphere;
				Sphere.SetSphere(InVisualizationContext.DebugDrawSettings->CenterOfMassRadius);
				const FPhysicsShapeAdapter SphereShapeAdapter(FQuat::Identity, Sphere);

				FChaosVDDebugDrawUtils::DrawImplicitObject(PDI, GeometryGenerator, &SphereShapeAdapter.GetGeometry(), FTransform(OwnerCoMLocation), InVisualizationContext.DebugDrawSettings->ColorSettings.GetColorForDataID(EChaosVDParticleDataVisualizationFlags::CenterOfMass, InVisualizationContext.bIsSelectedData), UEnum::GetDisplayValueAsText(EChaosVDParticleDataVisualizationFlags::CenterOfMass), InVisualizationContext.DebugDrawSettings->DepthPriority, LineThickness);
			}
		}
	}

	// TODO: This is a Proof of concept to test how debug draw connectivity data will look
	if (InParticleDataViewer.ParticleCluster.HasValidData())
	{
		if (InVisualizationContext.IsVisualizationFlagEnabled(EChaosVDParticleDataVisualizationFlags::ClusterConnectivityEdge))
		{
			for (const FChaosVDConnectivityEdge& ConnectivityEdge : InParticleDataViewer.ParticleCluster.ConnectivityEdges)
			{
				if (const TSharedPtr<FChaosVDScene> ScenePtr = InVisualizationContext.CVDScene.Pin())
				{
					if (AChaosVDParticleActor* SiblingParticle = ScenePtr->GetParticleActor(InVisualizationContext.SolverID, ConnectivityEdge.SiblingParticleID))
					{
						if (const FChaosVDParticleDataWrapper* SiblingParticleData = SiblingParticle->GetParticleData())
						{
							FColor DebugDrawColor = InVisualizationContext.DebugDrawSettings->ColorSettings.GetColorForDataID(EChaosVDParticleDataVisualizationFlags::ClusterConnectivityEdge, InVisualizationContext.bIsSelectedData);
							FVector BoxExtents(2,2,2);
							FTransform BoxTransform(OwnerRotation, OwnerLocation);
							FChaosVDDebugDrawUtils::DrawBox(PDI, BoxExtents, DebugDrawColor, BoxTransform, FText::GetEmpty(), InVisualizationContext.DebugDrawSettings->DepthPriority, LineThickness);

							FVector SiblingParticleLocation = InVisualizationContext.SpaceTransform.TransformPosition(SiblingParticleData->ParticlePositionRotation.MX);
							FChaosVDDebugDrawUtils::DrawLine(PDI, OwnerLocation, SiblingParticleLocation, DebugDrawColor, FText::FormatOrdered(LOCTEXT("StrainDebugDraw","Strain {0}"), ConnectivityEdge.Strain), InVisualizationContext.DebugDrawSettings->DepthPriority, LineThickness);
						}
					}	
				}
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
