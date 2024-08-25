// Copyright Epic Games, Inc. All Rights Reserved.

#include "PhysicsProxy/JointConstraintProxy.h"

#include "ChaosStats.h"
#include "Chaos/Collision/SpatialAccelerationBroadPhase.h"
#include "Chaos/Collision/CollisionConstraintFlags.h"
#include "Chaos/ErrorReporter.h"
#include "Chaos/ParticleHandle.h"
#include "Chaos/GeometryParticles.h"
#include "Chaos/Serializable.h"
#include "Chaos/PBDJointConstraints.h"
#include "Chaos/Framework/MultiBufferResource.h"
#include "PhysicsProxy/SingleParticlePhysicsProxy.h"
#include "PhysicsSolver.h"
#include "Chaos/PullPhysicsDataImp.h"
#include "Chaos/PhysicsObjectInternal.h"

namespace Chaos
{

FJointConstraintPhysicsProxy::FJointConstraintPhysicsProxy(FJointConstraint* InConstraint, FPBDJointConstraintHandle* InHandle, UObject* InOwner)
	: Base(EPhysicsProxyType::JointConstraintType, InOwner, MakeShared<FProxyTimestampBase>())
	, Constraint_GT(InConstraint) // This proxy assumes ownership of the Constraint, and will free it during DestroyOnPhysicsThread
	, Constraint_PT(InHandle)
	, OriginalParticleHandles_PT(nullptr, nullptr)
{
	check(Constraint_GT !=nullptr);
	Constraint_GT->SetProxy(this);
}

FGeometryParticleHandle*
FJointConstraintPhysicsProxy::GetParticleHandleFromProxy(IPhysicsProxyBase* ProxyBase)
{
	if (ProxyBase)
	{
		if (ProxyBase->GetType() == EPhysicsProxyType::SingleParticleProxy)
		{
			return ((FSingleParticlePhysicsProxy*)ProxyBase)->GetHandle_LowLevel();
		}
	}
	return nullptr;
}

/**/
void FJointConstraintPhysicsProxy::BufferPhysicsResults(FDirtyJointConstraintData& Buffer)
{
	Buffer.SetProxy(*this);
	if (Constraint_PT != nullptr && (Constraint_PT->IsValid() || Constraint_PT->IsConstraintBreaking() || Constraint_PT->IsDriveTargetChanged()))
	{
		Buffer.OutputData.bIsBreaking = Constraint_PT->IsConstraintBreaking();
		Buffer.OutputData.bIsBroken = !Constraint_PT->IsConstraintEnabled();
		Buffer.OutputData.bDriveTargetChanged = Constraint_PT->IsDriveTargetChanged();
		Buffer.OutputData.Force = Constraint_PT->GetLinearImpulse();
		Buffer.OutputData.Torque = Constraint_PT->GetAngularImpulse();

		Constraint_PT->ClearConstraintBreaking(); // it's a single frame event, so reset
		Constraint_PT->ClearDriveTargetChanged(); // it's a single frame event, so reset
	}
}

/**/
bool FJointConstraintPhysicsProxy::PullFromPhysicsState(const FDirtyJointConstraintData& Buffer, const int32 SolverSyncTimestamp)
{
	if (Constraint_GT != nullptr && Constraint_GT->IsValid())
	{
		if (Buffer.OutputData.bIsBreaking || Buffer.OutputData.bDriveTargetChanged)
		{
			Constraint_GT->GetOutputData().bIsBreaking = Buffer.OutputData.bIsBreaking;
			Constraint_GT->GetOutputData().bIsBroken = Buffer.OutputData.bIsBroken;
			Constraint_GT->GetOutputData().bDriveTargetChanged = Buffer.OutputData.bDriveTargetChanged;
		}
		Constraint_GT->GetOutputData().Force = Buffer.OutputData.Force;
		Constraint_GT->GetOutputData().Torque = Buffer.OutputData.Torque;
	}

	return true;
}

template <typename TransformType>
static void FixConnectorTransformsForRoot(const FParticlePair& OriginalHandles, const FParticlePair& RootHandles, TransformType& InOutTransformsToFix)
{
	for (int32 Index = 0; Index < 2; Index++)
	{
		if (OriginalHandles[Index] && RootHandles[Index] && RootHandles[Index] != OriginalHandles[Index])
		{
			const FTransform TransformOffset = OriginalHandles[Index]->GetTransformXR().GetRelativeTransform(RootHandles[Index]->GetTransformXR());
			InOutTransformsToFix[Index] *= TransformOffset;
		}
	}
}

void FJointConstraintPhysicsProxy::InitializeOnPhysicsThread(FPBDRigidsSolver* InSolver, FDirtyPropertiesManager& Manager, int32 DataIdx, FDirtyChaosProperties& RemoteData)
{
	auto& Handles = InSolver->GetParticles().GetParticleHandles();
	if (Handles.Size())
	{
		auto& JointConstraints = InSolver->GetJointConstraints();
		if(const FPhysicsObjectPairProperty* BodyPairs = RemoteData.FindJointPhysicsObjects(Manager, DataIdx))
		{
			FParticlePair RootHandles
			{
				BodyPairs->PhysicsBodies[0]->GetRootParticle<Chaos::EThreadContext::Internal>(),
				BodyPairs->PhysicsBodies[1]->GetRootParticle<Chaos::EThreadContext::Internal>(),
			};

			if (RootHandles[0] && RootHandles[1])
			{
				if (const FPBDJointSettings* JointSettings = RemoteData.FindJointSettings(Manager, DataIdx))
				{
					FTransformPair ConnectorTransforms{ JointSettings->ConnectorTransforms };

					// if the root particles do not match the actual particles
					// we need to adjust the frames to be in the root particle space 
					OriginalParticleHandles_PT =
					{
						BodyPairs->PhysicsBodies[0]->GetParticle<Chaos::EThreadContext::Internal>(),
						BodyPairs->PhysicsBodies[1]->GetParticle<Chaos::EThreadContext::Internal>(),
					};

					FixConnectorTransformsForRoot(OriginalParticleHandles_PT, RootHandles, ConnectorTransforms);

					Constraint_PT = JointConstraints.AddConstraint(RootHandles, ConnectorTransforms);

					RootHandles[0]->AddConstraintHandle(Constraint_PT);
					RootHandles[1]->AddConstraintHandle(Constraint_PT);

					// We added a joint to the particles, so we need to modify the inertia (See FPBDRigidsEvolutionGBF::UpdateInertiaConditioning)
					FGenericParticleHandle(RootHandles[0])->SetInertiaConditioningDirty();
					FGenericParticleHandle(RootHandles[1])->SetInertiaConditioningDirty();
				}
			}
		}
	}
}

void FJointConstraintPhysicsProxy::DestroyOnPhysicsThread(FPBDRigidsSolver* InSolver)
{
	if (Constraint_PT)
	{
		TVec2<FGeometryParticleHandle*> Particles = Constraint_PT->GetConstrainedParticles();
		
		const FPBDJointSettings Settings = Constraint_PT->GetSettings();
		const bool bValidPair = InSolver && Particles[0] && Particles[1];

		// If this constraint disables collisions - need to restore the collisions after we destroy the constraint
		if(bValidPair && !Settings.bCollisionEnabled)
		{
			InSolver->GetEvolution()->GetBroadPhase().GetIgnoreCollisionManager().RemoveIgnoreCollisions(Particles[0], Particles[1]);
		}

		// Ensure that our connected particles are aware that this constraint no longer exists
		if(Particles[0])
		{
			Particles[0]->RemoveConstraintHandle(Constraint_PT);
		}

		if(Particles[1])
		{
			Particles[1]->RemoveConstraintHandle(Constraint_PT);
		}

		// @todo(chaos): clean up constraint management
		if(Constraint_PT->IsInConstraintGraph())
		{
			InSolver->GetEvolution()->RemoveConstraintFromConstraintGraph(Constraint_PT);
		}

		FPBDRigidsSolver::FJointConstraints& JointConstraints = InSolver->GetJointConstraints();
		JointConstraints.RemoveConstraint(Constraint_PT->GetConstraintIndex());
		Constraint_PT = nullptr;
		OriginalParticleHandles_PT = { nullptr, nullptr };
	}
}

void FJointConstraintPhysicsProxy::DestroyOnGameThread()
{
	delete Constraint_GT;
	Constraint_GT = nullptr;
}


void FJointConstraintPhysicsProxy::PushStateOnGameThread(FDirtyPropertiesManager& Manager, int32 DataIdx, FDirtyChaosProperties& RemoteData)
{
	if (Constraint_GT && Constraint_GT->IsValid())
	{
		Constraint_GT->SyncRemoteData(Manager, DataIdx, RemoteData);
	}
}


void FJointConstraintPhysicsProxy::PushStateOnPhysicsThread(FPBDRigidsSolver* InSolver, const FDirtyPropertiesManager& Manager, int32 DataIdx, const FDirtyChaosProperties& RemoteData)
{
	if (Constraint_PT && Constraint_PT->IsValid())
	{
		if (const FPBDJointSettings* Data = RemoteData.FindJointSettings(Manager, DataIdx))
		{
			const FPBDJointSettings& JointSettingsBuffer = *Data;
			const FPBDJointSettings& CurrentConstraintSettings = Constraint_PT->GetSettings();

			// Handle changes to the CollisionEnabled flag
			if (CurrentConstraintSettings.bCollisionEnabled != JointSettingsBuffer.bCollisionEnabled)
			{
				const TVector<FGeometryParticleHandle*, 2>& Particles = Constraint_PT->GetConstrainedParticles();
				if (JointSettingsBuffer.bCollisionEnabled)
				{
					InSolver->GetEvolution()->GetBroadPhase().GetIgnoreCollisionManager().RemoveIgnoreCollisions(Particles[0], Particles[1]);
				}
				else
				{
					InSolver->GetEvolution()->GetBroadPhase().GetIgnoreCollisionManager().AddIgnoreCollisions(Particles[0], Particles[1]);
				}
			}

			FPBDJointSettings JointSettingsToSet{ JointSettingsBuffer };

			// PushStateOnPhysicsThread is always called right after InitializeOnPhysicsThread
			// so we need to make sure the root space correction logic is also run here to avoid resetting the transform in the wrong space
			FixConnectorTransformsForRoot(OriginalParticleHandles_PT, Constraint_PT->GetConstrainedParticles(), JointSettingsToSet.ConnectorTransforms);

			// Update the joint settings
			Constraint_PT->SetSettings(JointSettingsToSet);
		}
	}
}

}