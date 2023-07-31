// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosCloth/SkeletalMeshComponentCacheAdapter.h"
#include "Chaos/ParticleHandle.h"
#include "Chaos/ChaosCache.h"
#include "Chaos/CacheManagerActor.h"
#include "Chaos/PBDEvolution.h"
#include "ChaosCloth/ChaosClothingSimulation.h"
#include "ChaosCloth/ChaosClothingSimulationSolver.h"
#include "Components/SkeletalMeshComponent.h"

namespace Chaos
{
	FComponentCacheAdapter::SupportType FSkeletalMeshCacheAdapter::SupportsComponentClass(UClass* InComponentClass) const
	{
		const UClass* Desired = GetDesiredClass();
		if(InComponentClass == Desired)
		{
			return FComponentCacheAdapter::SupportType::Direct;
		}
		else if(InComponentClass->IsChildOf(Desired))
		{
			return FComponentCacheAdapter::SupportType::Derived;
		}

		return FComponentCacheAdapter::SupportType::None;
	}

	UClass* FSkeletalMeshCacheAdapter::GetDesiredClass() const
	{
		return USkeletalMeshComponent::StaticClass();
	}

	uint8 FSkeletalMeshCacheAdapter::GetPriority() const
	{
		return EngineAdapterPriorityBegin;
	}

	void FSkeletalMeshCacheAdapter::Record_PostSolve(UPrimitiveComponent* InComponent, const FTransform& InRootTransform, FPendingFrameWrite& OutFrame, Chaos::FReal InTime) const
	{
		if( Chaos::FClothingSimulationSolver* ClothSolver = GetClothSolver(InComponent))
		{
			const uint32 NumParticles = ClothSolver->GetNumParticles();
			if(NumParticles > 0)
			{
				const Softs::FSolverVec3* ParticleXs = ClothSolver->GetParticleXs(0);
				const Softs::FSolverVec3* ParticleVs = ClothSolver->GetParticleVs(0);
				
				TArray<float> PendingVX, PendingVY, PendingVZ, PendingPX, PendingPY, PendingPZ;
				TArray<int32>& PendingID = OutFrame.PendingChannelsIndices;

				PendingID.SetNum(NumParticles);
				PendingVX.SetNum(NumParticles);
				PendingVY.SetNum(NumParticles);
				PendingVZ.SetNum(NumParticles);
				PendingPX.SetNum(NumParticles);
				PendingPY.SetNum(NumParticles);
				PendingPZ.SetNum(NumParticles);

				for(uint32 ParticleIndex = 0; ParticleIndex < NumParticles; ++ParticleIndex)
				{
					const Softs::FSolverVec3& ParticleV = ParticleVs[ParticleIndex];
					const Softs::FSolverVec3& ParticleX = ParticleXs[ParticleIndex];
					
					// Adding the vertices relative position to the particle write datas
					FPendingParticleWrite NewData;

					NewData.ParticleIndex = ParticleIndex; 
					OutFrame.PendingParticleData.Add(MoveTemp(NewData));

					PendingID[ParticleIndex] = ParticleIndex;
					PendingVX[ParticleIndex] = ParticleV.X;
					PendingVY[ParticleIndex] = ParticleV.Y;
					PendingVZ[ParticleIndex] = ParticleV.Z;

					PendingPX[ParticleIndex] = ParticleX.X;
					PendingPY[ParticleIndex] = ParticleX.Y;
					PendingPZ[ParticleIndex] = ParticleX.Z;
				}

				OutFrame.PendingChannelsData.Add(VelocityXName, PendingVX);
				OutFrame.PendingChannelsData.Add(VelocityYName, PendingVY);
				OutFrame.PendingChannelsData.Add(VelocityZName, PendingVZ);
				OutFrame.PendingChannelsData.Add(PositionXName, PendingPX);
				OutFrame.PendingChannelsData.Add(PositionYName, PendingPY);
				OutFrame.PendingChannelsData.Add(PositionZName, PendingPZ);
			}
		}
	}

	void FSkeletalMeshCacheAdapter::Playback_PreSolve(UPrimitiveComponent* InComponent, UChaosCache* InCache, Chaos::FReal InTime, FPlaybackTickRecord& TickRecord, TArray<TPBDRigidParticleHandle<Chaos::FReal, 3>*>& OutUpdatedRigids) const
	{
		if( Chaos::FClothingSimulationSolver* ClothSolver = GetClothSolver(InComponent))
		{
			FCacheEvaluationContext Context(TickRecord);
			Context.bEvaluateTransform = false;
			Context.bEvaluateCurves = true;
			Context.bEvaluateEvents = false;

			// The evaluated result are already in world space since we are passing the tickrecord.spacetransform in the context
			FCacheEvaluationResult EvaluatedResult = InCache->Evaluate(Context, nullptr);
			const int32 NumCurves = EvaluatedResult.Curves.Num();

			const uint32 NumParticles = ClothSolver->GetNumParticles();
			if(NumParticles > 0 && NumParticles == NumCurves)
			{
				Softs::FSolverVec3* ParticleXs = ClothSolver->GetParticleXs(0);
				Softs::FSolverVec3* ParticleVs = ClothSolver->GetParticleVs(0);

				TArray<float>* PendingVX = EvaluatedResult.Channels.Find(VelocityXName);
				TArray<float>* PendingVY = EvaluatedResult.Channels.Find(VelocityYName);
				TArray<float>* PendingVZ = EvaluatedResult.Channels.Find(VelocityZName);
				TArray<float>* PendingPX = EvaluatedResult.Channels.Find(PositionXName);
				TArray<float>* PendingPY = EvaluatedResult.Channels.Find(PositionYName);
				TArray<float>* PendingPZ = EvaluatedResult.Channels.Find(PositionZName);
					
				if(PendingVX && PendingVY && PendingVZ && PendingPX && PendingPY && PendingPZ)
				{
					for(uint32 ParticleIndex = 0; ParticleIndex < NumParticles; ++ParticleIndex)
					{
						Softs::FSolverVec3& ParticleV = ParticleVs[ParticleIndex];
						Softs::FSolverVec3& ParticleX = ParticleXs[ParticleIndex];
					
						// Directly set the result of the cache into the solver particles
						const int32 TransformIndex = EvaluatedResult.ParticleIndices[ParticleIndex];

						ParticleV.X = (*PendingVX)[TransformIndex];
						ParticleV.Y = (*PendingVY)[TransformIndex];
						ParticleV.Z = (*PendingVZ)[TransformIndex];
						ParticleX.X = (*PendingPX)[TransformIndex];
						ParticleX.Y = (*PendingPY)[TransformIndex];
						ParticleX.Z = (*PendingPZ)[TransformIndex];
					}
				}
			}
		}
	}

	FGuid FSkeletalMeshCacheAdapter::GetGuid() const
	{
		FGuid NewGuid;
		checkSlow(FGuid::Parse(TEXT("4328BED8C52D4E07BE0CA2433940AF6D"), NewGuid));
		return NewGuid;
	}

	bool FSkeletalMeshCacheAdapter::ValidForPlayback(UPrimitiveComponent* InComponent, UChaosCache* InCache) const
	{
		// If we have a skel mesh we can play back any cache as long as it has one or more tracks
		const USkeletalMeshComponent* MeshComp = CastChecked<USkeletalMeshComponent>(InComponent);
		return MeshComp && MeshComp->GetSkinnedAsset() && InCache->TrackToParticle.Num() > 0;
	}

	Chaos::FClothingSimulationSolver* FSkeletalMeshCacheAdapter::GetClothSolver(UPrimitiveComponent* InComponent) const
	{
		if(InComponent)
		{
			if( USkeletalMeshComponent* MeshComp = CastChecked<USkeletalMeshComponent>(InComponent) )
			{
				if(MeshComp->GetClothingSimulation())
				{
					return StaticCast<Chaos::FClothingSimulation*>(MeshComp->GetClothingSimulation())->Solver.Get();
				}
			}
		}
		return nullptr;
	}

	Chaos::FPhysicsSolverEvents* FSkeletalMeshCacheAdapter::BuildEventsSolver(UPrimitiveComponent* InComponent) const
	{
		if(USkeletalMeshComponent* MeshComp = CastChecked<USkeletalMeshComponent>(InComponent))
		{
			// Initialize the physics solver at the beginning of the play/record
			MeshComp->RecreatePhysicsState();
			MeshComp->RecreateClothingActors();

			return GetClothSolver(InComponent);
		}
		return nullptr;
	}
	
	Chaos::FPhysicsSolver* FSkeletalMeshCacheAdapter::GetComponentSolver(UPrimitiveComponent* InComponent) const
	{
		return nullptr;
	}
	
	void FSkeletalMeshCacheAdapter::SetRestState(UPrimitiveComponent* InComponent, UChaosCache* InCache, const FTransform& InRootTransform, Chaos::FReal InTime) const
	{
		if (!InCache || InCache->GetDuration() == 0.0f)
		{
			return;
		}

		if(USkeletalMeshComponent* MeshComp = CastChecked<USkeletalMeshComponent>(InComponent))
		{
			InitializeForRestState(InComponent);
		
			FPlaybackTickRecord TickRecord;
			TickRecord.SetLastTime(InTime);
		
			FCacheEvaluationContext Context(TickRecord);
			Context.bEvaluateTransform = false;
			Context.bEvaluateCurves = false;
			Context.bEvaluateEvents = false;
			Context.bEvaluateChannels = true;

			FCacheEvaluationResult EvaluatedResult = InCache->Evaluate(Context,nullptr);

			if( MeshComp->GetClothingSimulationContext())
			{
				FClothingSimulationContext* SimulationContext =
						StaticCast<FClothingSimulationContext*>(MeshComp->GetClothingSimulationContext());
				
				const int32 NumCurves = EvaluatedResult.ParticleIndices.Num();
				if(NumCurves == EvaluatedResult.ParticleIndices.Num())
				{
					SimulationContext->CachedPositions.SetNum(NumCurves);
					SimulationContext->CachedVelocities.SetNum(NumCurves);

					TArray<float>* PendingVX = EvaluatedResult.Channels.Find(VelocityXName);
					TArray<float>* PendingVY = EvaluatedResult.Channels.Find(VelocityYName);
					TArray<float>* PendingVZ = EvaluatedResult.Channels.Find(VelocityZName);
					TArray<float>* PendingPX = EvaluatedResult.Channels.Find(PositionXName);
					TArray<float>* PendingPY = EvaluatedResult.Channels.Find(PositionYName);
					TArray<float>* PendingPZ = EvaluatedResult.Channels.Find(PositionZName);
					
					if(PendingVX && PendingVY && PendingVZ && PendingPX && PendingPY && PendingPZ)
					{
						for(int32 ParticleIndex = 0; ParticleIndex < NumCurves; ++ParticleIndex)
						{
							const int32 TransformIndex = EvaluatedResult.ParticleIndices[ParticleIndex];
							
							SimulationContext->CachedVelocities[ParticleIndex].X = (*PendingVX)[TransformIndex];
							SimulationContext->CachedVelocities[ParticleIndex].Y = (*PendingVY)[TransformIndex];
							SimulationContext->CachedVelocities[ParticleIndex].Z = (*PendingVZ)[TransformIndex];
							SimulationContext->CachedPositions[ParticleIndex].X = (*PendingPX)[TransformIndex];
							SimulationContext->CachedPositions[ParticleIndex].Y = (*PendingPY)[TransformIndex];
							SimulationContext->CachedPositions[ParticleIndex].Z = (*PendingPZ)[TransformIndex];
						}
					}
				}
			}
		}
	}

	void FSkeletalMeshCacheAdapter::InitializeForRestState(UPrimitiveComponent* InComponent) const
	{
		if(USkeletalMeshComponent* MeshComp = CastChecked<USkeletalMeshComponent>(InComponent))
		{
#if WITH_EDITOR
			if(!MeshComp->GetUpdateClothInEditor() || !MeshComp->GetUpdateAnimationInEditor())
			{
				MeshComp->SetUpdateAnimationInEditor(true);
				MeshComp->SetUpdateClothInEditor(true);
			}
#endif
			// Initialize the physics solver if the cloth/animation in editor are not set
			MeshComp->RecreatePhysicsState();
			MeshComp->RecreateClothingActors();
		}

		if( Chaos::FClothingSimulationSolver* ClothSolver = GetClothSolver(InComponent))
		{
			ClothSolver->SetEnableSolver(false);
		}
	}

	bool FSkeletalMeshCacheAdapter::InitializeForRecord(UPrimitiveComponent* InComponent, UChaosCache* InCache)
	{
		if( Chaos::FClothingSimulationSolver* ClothSolver = GetClothSolver(InComponent))
		{
			ClothSolver->SetEnableSolver(true);
		}
		return true;
	}

	bool FSkeletalMeshCacheAdapter::InitializeForPlayback(UPrimitiveComponent* InComponent, UChaosCache* InCache, float InTime)
	{
		EnsureIsInGameThreadContext();
		
		if( Chaos::FClothingSimulationSolver* ClothSolver = GetClothSolver(InComponent))
		{
			ClothSolver->SetEnableSolver(false);
		}
		return true;
	}
}    // namespace Chaos
