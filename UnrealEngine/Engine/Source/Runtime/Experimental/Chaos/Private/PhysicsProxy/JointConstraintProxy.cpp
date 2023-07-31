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

namespace Chaos
{

FJointConstraintPhysicsProxy::FJointConstraintPhysicsProxy(FJointConstraint* InConstraint, FPBDJointConstraintHandle* InHandle, UObject* InOwner)
	: Base(EPhysicsProxyType::JointConstraintType, InOwner, MakeShared<FProxyTimestampBase>())
	, Constraint_GT(InConstraint) // This proxy assumes ownership of the Constraint, and will free it during DestroyOnPhysicsThread
	, Constraint_PT(InHandle)
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

void FJointConstraintPhysicsProxy::InitializeOnPhysicsThread(FPBDRigidsSolver* InSolver, FDirtyPropertiesManager& Manager, int32 DataIdx, FDirtyChaosProperties& RemoteData)
{
	auto& Handles = InSolver->GetParticles().GetParticleHandles();
	if (Handles.Size())
	{
		auto& JointConstraints = InSolver->GetJointConstraints();
		if(const FProxyBasePairProperty* BasePairs = RemoteData.FindJointParticleProxies(Manager, DataIdx))
		{
			FGeometryParticleHandle* Handle0 = GetParticleHandleFromProxy(BasePairs->ParticleProxies[0]);
			FGeometryParticleHandle* Handle1 = GetParticleHandleFromProxy(BasePairs->ParticleProxies[1]);
			if (Handle0 && Handle1)
			{
				if (const FPBDJointSettings* JointSettings = RemoteData.FindJointSettings(Manager, DataIdx))
				{
					Constraint_PT = JointConstraints.AddConstraint({ Handle0,Handle1 }, JointSettings->ConnectorTransforms);

					Handle0->AddConstraintHandle(Constraint_PT);
					Handle1->AddConstraintHandle(Constraint_PT);

					// We added a joint to the particles, so we need to modify the inertia (See FPBDRigidsEvolutionGBF::UpdateInertiaConditioning)
					FGenericParticleHandle(Handle0)->SetInertiaConditioningDirty();
					FGenericParticleHandle(Handle1)->SetInertiaConditioningDirty();
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
			FPBDRigidParticleHandle* Rigid0 = Particles[0]->CastToRigidParticle();
			FPBDRigidParticleHandle* Rigid1 = Particles[1]->CastToRigidParticle();

			if(Rigid0 || Rigid1)
			{
				// At least one rigid so collisions are possible
				const FUniqueIdx Id0 = Particles[0]->UniqueIdx();
				const FUniqueIdx Id1 = Particles[1]->UniqueIdx();

				FIgnoreCollisionManager& IgnoreManager = InSolver->GetEvolution()->GetBroadPhase().GetIgnoreCollisionManager();

				// Remove the ignore flags for rigid particles. If we end up with no more ignore entries for this particle
				// then remove the broadphase flag to no longer check the ignore manager for this particles.
				if(Rigid0)
				{
					if(IgnoreManager.RemoveIgnoreCollisionsFor(Id0, Id1) == 0)
					{
						Rigid0->RemoveCollisionConstraintFlag(ECollisionConstraintFlags::CCF_BroadPhaseIgnoreCollisions);
					}
				}

				if(Rigid1)
				{
					if(IgnoreManager.RemoveIgnoreCollisionsFor(Id1, Id0) == 0)
					{
						Rigid1->RemoveCollisionConstraintFlag(ECollisionConstraintFlags::CCF_BroadPhaseIgnoreCollisions);
					}
				}
			}
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

			if (CurrentConstraintSettings.bCollisionEnabled != JointSettingsBuffer.bCollisionEnabled)
			{
				const TVector<FGeometryParticleHandle*, 2>& BasePairs = Constraint_PT->GetConstrainedParticles();
				FGeometryParticleHandle * Handle0 = BasePairs[0];
				FGeometryParticleHandle* Handle1 = BasePairs[1];

				// Three pieces of state to update on the physics thread. 
				// .. Mask on the particle array
				// .. Constraint collisions enabled array
				// .. IgnoreCollisionsManager
				if (Handle0 && Handle1)
				{
					FPBDRigidParticleHandle* RigidHandle0 = Handle0->CastToRigidParticle();
					FPBDRigidParticleHandle* RigidHandle1 = Handle1->CastToRigidParticle();

					// As long as one particle is a rigid we can add the ignore entry, one particle can be a static
					if (RigidHandle0 || RigidHandle1)
					{
						const FUniqueIdx ID0 = Handle0->UniqueIdx();
						const FUniqueIdx ID1 = Handle1->UniqueIdx();
						FIgnoreCollisionManager& IgnoreCollisionManager = InSolver->GetEvolution()->GetBroadPhase().GetIgnoreCollisionManager();

						// For rigid/dynamic particles, add the broadphase flag and the IDs to check for disabled collisions
						if(RigidHandle0)
						{
							if(JointSettingsBuffer.bCollisionEnabled)
							{
								if(IgnoreCollisionManager.RemoveIgnoreCollisionsFor(ID0, ID1) == 0)
								{
									RigidHandle0->RemoveCollisionConstraintFlag(ECollisionConstraintFlags::CCF_BroadPhaseIgnoreCollisions);
								}
							}
							else
							{
								RigidHandle0->AddCollisionConstraintFlag(ECollisionConstraintFlags::CCF_BroadPhaseIgnoreCollisions);
								IgnoreCollisionManager.AddIgnoreCollisionsFor(ID0, ID1);
							}
						}

						if(RigidHandle1)
						{
							if (JointSettingsBuffer.bCollisionEnabled)
							{
								if(IgnoreCollisionManager.RemoveIgnoreCollisionsFor(ID1, ID0) == 0)
								{
									RigidHandle1->RemoveCollisionConstraintFlag(ECollisionConstraintFlags::CCF_BroadPhaseIgnoreCollisions);
								}
							}
							else
							{
								RigidHandle1->AddCollisionConstraintFlag(ECollisionConstraintFlags::CCF_BroadPhaseIgnoreCollisions);
								IgnoreCollisionManager.AddIgnoreCollisionsFor(ID1, ID0);
							}
						}
					}
				}
			}

			Constraint_PT->SetSettings(JointSettingsBuffer);
		}
	}
}

}