// Copyright Epic Games, Inc. All Rights Reserved.

#include "Visualizers/ChaosVDCollisionDataVisualizer.h"

#include "ChaosVDEditorSettings.h"
#include "ChaosVDModule.h"
#include "ChaosVDParticleActor.h"
#include "ChaosVDScene.h"
#include "DataWrappers/ChaosVDCollisionDataWrappers.h"
#include "Visualizers/ChaosVDDebugDrawUtils.h"

void FChaosVDCollisionDataVisualizer::DrawVisualization(const FSceneView* View, FPrimitiveDrawInterface* PDI)
{
	uint32 VisualizationFlagsToUse = 0;
	if (static_cast<EChaosVDCollisionVisualizationFlags>(LocalVisualizationFlags) == EChaosVDCollisionVisualizationFlags::None)
	{
		if (const UChaosVDEditorSettings* EditorSettings = GetDefault<UChaosVDEditorSettings>())
		{
			VisualizationFlagsToUse = EditorSettings->GlobalCollisionDataVisualizationFlags;
		}
		else
		{
			UE_LOG(LogChaosVDEditor, Warning, TEXT("[%s] Failed to retrive global visualization setting. Falling back to local settings"), ANSI_TO_TCHAR(__FUNCTION__));

			VisualizationFlagsToUse = LocalVisualizationFlags;
		}	
	}
	else
	{
		VisualizationFlagsToUse = LocalVisualizationFlags;
	}

	FChaosVDVisualizationContext VisualizationContext;
	DataProvider.GetVisualizationContext(VisualizationContext);

	const FChaosVDParticleDataWrapper* ParticleDataViewer = DataProvider.GetParticleData();
	if (!ensure(ParticleDataViewer))
	{
		return;
	}

	TSharedPtr<FChaosVDScene> ScenePtr = VisualizationContext.CVDScene.Pin();
	if (!ScenePtr.IsValid())
	{
		return;
	}
 
	const FVector& OwnerActorLocation = ParticleDataViewer->ParticlePositionRotation.MX;

	for (const FChaosVDParticlePairMidPhase& MidPhase : ParticleDataViewer->ParticleMidPhases)
	{
		for (const FChaosVDConstraint& Constraint : MidPhase.Constraints)
		{
			for (const FChaosVDManifoldPoint& ManifoldPoint : Constraint.ManifoldPoints)
			{
				constexpr int32 MinShapeContactAmount = 2;
				if (Constraint.ShapeWorldTransforms.Num() < MinShapeContactAmount || ManifoldPoint.ContactPoint.ShapeContactPoints.Num() < MinShapeContactAmount)
				{
					continue;
				}

				const bool bIsProbe = Constraint.bIsProbe;
				const bool bIsActive = ManifoldPoint.bIsValid && (!ManifoldPoint.NetPushOut.IsNearlyZero() || !ManifoldPoint.NetImpulse.IsNearlyZero() || (!Constraint.bUseManifold && !Constraint.AccumulatedImpulse.IsNearlyZero()));
				if (!bIsActive && EnumHasAnyFlags(static_cast<EChaosVDCollisionVisualizationFlags>(LocalVisualizationFlags), EChaosVDCollisionVisualizationFlags::DrawOnlyActiveContacts))
				{
					continue;
				}

				const bool bPruned = ManifoldPoint.bDisabled;

				AChaosVDParticleActor* CVDParticleActor1 = ScenePtr->GetParticleActor(VisualizationContext.SolverID, Constraint.Particle1Index);
				AChaosVDParticleActor* CVDParticleActor0 = ScenePtr->GetParticleActor(VisualizationContext.SolverID, Constraint.Particle0Index);
				if (CVDParticleActor1 == nullptr || CVDParticleActor0 == nullptr)
				{
					continue;
				}

				const FTransform& WorldActorTransform1 = CVDParticleActor1->GetTransform();
				const FTransform& WorldActorTransform0 = CVDParticleActor0->GetTransform();

				constexpr int32 ContactPlaneOwner = 1;
				constexpr int32 ContactPointOwner = 1 - ContactPlaneOwner;
				const FTransform& PlaneTransform = Constraint.ImplicitTransforms[1] * WorldActorTransform1;
				const FTransform& PointTransform = Constraint.ImplicitTransforms[0] * WorldActorTransform0;

				const FVector PlaneNormal = PlaneTransform.TransformVectorNoScale(FVector(ManifoldPoint.ContactPoint.ShapeContactNormal));
				const FVector PointLocation = PointTransform.TransformPosition(FVector(ManifoldPoint.ContactPoint.ShapeContactPoints[ContactPointOwner]));
				const FVector PlaneLocation = PlaneTransform.TransformPosition(FVector(ManifoldPoint.ContactPoint.ShapeContactPoints[ContactPlaneOwner]));
				const FVector PointPlaneLocation = PointLocation - FVector::DotProduct(PointLocation - PlaneLocation, PlaneNormal) * PlaneNormal;

				// TODO: Extract these colors to a class or another place where ChaosDebugDraw and Chaos VD Visualizers can access
				// The bulk of this implementation including the following colors come from ChaosDebugDraw Implementation.
				// The implementation itself is likely to drift, but we should try to keep the colors the same between both debugging tools

				// Dynamic friction, restitution = red
				// Static friction, no restitution = green
				// Inactive = gray
				FColor DiscColor = FColor(200, 0, 0);
				FColor PlaneNormalColor = FColor(200, 0, 0);
				FColor EdgeNormalColor = FColor(200, 150, 0);
				FColor ImpulseColor = FColor(0, 0, 200);
				FColor PushOutImpusleColor = FColor(0, 200, 200);
				if (ManifoldPoint.bInsideStaticFrictionCone)
				{
					DiscColor = FColor(150, 200, 0);
				}
				if (bIsProbe)
				{
					DiscColor = FColor(50, 180, 180);
					PlaneNormalColor = FColor(50, 180, 180);
					EdgeNormalColor = FColor(50, 180, 130);
				}
				else if (!bIsActive)
				{
					DiscColor = FColor(100, 100, 100);
					PlaneNormalColor = FColor(100, 0, 0);
					EdgeNormalColor = FColor(100, 80, 0);
				}
				if (bPruned)
				{
					PlaneNormalColor = FColor(200, 0, 200);
					EdgeNormalColor = FColor(200, 0, 200);
				}
			
				const FVector WorldPointLocation = VisualizationContext.SpaceTransform.TransformPosition(PointLocation);
				const FVector WorldPlaneLocation = VisualizationContext.SpaceTransform.TransformPosition(PlaneLocation);
				const FVector WorldPointPlaneLocation = VisualizationContext.SpaceTransform.TransformPosition(PointPlaneLocation);
				const FVector WorldPlaneNormal = VisualizationContext.SpaceTransform.TransformVectorNoScale(PlaneNormal);

				if (EnumHasAnyFlags(static_cast<EChaosVDCollisionVisualizationFlags>(VisualizationFlagsToUse), EChaosVDCollisionVisualizationFlags::NetPushOut))
				{
					if (ManifoldPoint.bIsValid && !ManifoldPoint.NetPushOut.IsNearlyZero())
					{
						FChaosVDDebugDrawUtils::DrawArrowVector(PDI, WorldPointPlaneLocation, OwnerActorLocation + WorldPointPlaneLocation + VisualizationContext.SpaceTransform.TransformPosition(ManifoldPoint.NetPushOut), TEXT("Net Push out"), PushOutImpusleColor);
					}
				}

				if (EnumHasAnyFlags(static_cast<EChaosVDCollisionVisualizationFlags>(VisualizationFlagsToUse), EChaosVDCollisionVisualizationFlags::NetImpulse))
				{
					if (ManifoldPoint.bIsValid && !ManifoldPoint.NetImpulse.IsNearlyZero())
					{
						FChaosVDDebugDrawUtils::DrawArrowVector(PDI, WorldPointPlaneLocation, OwnerActorLocation + WorldPointPlaneLocation + VisualizationContext.SpaceTransform.TransformPosition(ManifoldPoint.NetImpulse), TEXT("Net Impulse"), ImpulseColor);
					}
				}

				if (EnumHasAnyFlags(static_cast<EChaosVDCollisionVisualizationFlags>(VisualizationFlagsToUse), EChaosVDCollisionVisualizationFlags::ContactPoints))
				{
					FChaosVDDebugDrawUtils::DrawPoint(PDI, WorldPlaneLocation, TEXT("Contact : WorldPlaneLocation"), DiscColor);
				}

				if (EnumHasAnyFlags(static_cast<EChaosVDCollisionVisualizationFlags>(VisualizationFlagsToUse), EChaosVDCollisionVisualizationFlags::ContactNormal))
				{
					FColor NormalColor = ((ManifoldPoint.ContactPoint.ContactType != EChaosVDContactPointType::EdgeEdge) ? PlaneNormalColor : EdgeNormalColor);
					constexpr int32 Scale = 10.0f;
					FChaosVDDebugDrawUtils::DrawArrowVector(PDI, WorldPlaneLocation, WorldPlaneLocation + WorldPlaneNormal * Scale, TEXT("Contact Normal"), NormalColor, 2.0f);
				}

				if (EnumHasAnyFlags(static_cast<EChaosVDCollisionVisualizationFlags>(VisualizationFlagsToUse), EChaosVDCollisionVisualizationFlags::ContactPoints))
				{
					if (ManifoldPoint.ContactPoint.Phi < FLT_MAX)
					{
						FColor Color = FColor(128, 128, 0);
						FChaosVDDebugDrawUtils::DrawPoint(PDI, WorldPlaneLocation - ManifoldPoint.ContactPoint.Phi * WorldPlaneNormal, TEXT("Contact : WorldPlaneLocation - Phi"), Color);
					}
					
					// Manifold point
					FChaosVDDebugDrawUtils::DrawPoint(PDI, WorldPointLocation , TEXT("Contact : Manifold Point"), DiscColor);
				}

				if (EnumHasAnyFlags(static_cast<EChaosVDCollisionVisualizationFlags>(VisualizationFlagsToUse), EChaosVDCollisionVisualizationFlags::AccumulatedImpulse))
				{
					if (!Constraint.AccumulatedImpulse.IsNearlyZero())
					{
						FChaosVDDebugDrawUtils::DrawArrowVector(PDI, WorldActorTransform0.GetLocation(), WorldActorTransform0.GetLocation() + Constraint.AccumulatedImpulse, TEXT("AccumulatedImpulse"), FColor::White, 5.0f);
					}
				}
			}
		}
	}
}
