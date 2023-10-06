// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosCache/FleshComponentCacheAdapter.h"


#include "Chaos/ChaosCache.h"
#include "Chaos/PBDEvolution.h"
#include "ChaosFlesh/ChaosDeformableSolverComponent.h"
#include "ChaosFlesh/FleshComponent.h"


namespace Chaos
{
	FComponentCacheAdapter::SupportType FFleshCacheAdapter::SupportsComponentClass(UClass* InComponentClass) const
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

	UClass* FFleshCacheAdapter::GetDesiredClass() const
	{
		return UFleshComponent::StaticClass();
	}

	uint8 FFleshCacheAdapter::GetPriority() const
	{
		return EngineAdapterPriorityBegin;
	}

	void FFleshCacheAdapter::Record_PostSolve(UPrimitiveComponent* InComponent, const FTransform& InRootTransform, FPendingFrameWrite& OutFrame, Chaos::FReal InTime) const
	{
		if( FDeformableSolver* Solver = GetDeformableSolver(InComponent))
		{
			FDeformableSolver::FPhysicsThreadAccess PhysicsThreadAccess(Solver, Chaos::Softs::FPhysicsThreadAccessor());

			if (FEvolution* Evolution = PhysicsThreadAccess.GetEvolution())
			{
				const FParticles& Particles = Evolution->Particles();

				const uint32 NumParticles = Particles.Size();
				if (NumParticles > 0)
				{
					const Softs::FSolverVec3* ParticleXs = &Particles.X(0);
					const Softs::FSolverVec3* ParticleVs = &Particles.V(0);

					TArray<float> PendingVX, PendingVY, PendingVZ, PendingPX, PendingPY, PendingPZ;
					TArray<int32>& PendingID = OutFrame.PendingChannelsIndices;

					PendingID.Reserve(NumParticles);
					PendingVX.Reserve(NumParticles);
					PendingVY.Reserve(NumParticles);
					PendingVZ.Reserve(NumParticles);
					PendingPX.Reserve(NumParticles);
					PendingPY.Reserve(NumParticles);
					PendingPZ.Reserve(NumParticles);

					for (uint32 ParticleIndex = 0; ParticleIndex < NumParticles; ++ParticleIndex)
					{
						const Softs::FSolverVec3& ParticleV = ParticleVs[ParticleIndex];
						const Softs::FSolverVec3& ParticleX = ParticleXs[ParticleIndex];

						// Adding the vertices relative position to the particle write datas
						PendingID.Add(ParticleIndex);
						PendingVX.Add(ParticleV.X);
						PendingVY.Add(ParticleV.Y);
						PendingVZ.Add(ParticleV.Z);

						PendingPX.Add(ParticleX.X);
						PendingPY.Add(ParticleX.Y);
						PendingPZ.Add(ParticleX.Z);
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
	}

	void FFleshCacheAdapter::Playback_PreSolve(UPrimitiveComponent* InComponent, UChaosCache* InCache, Chaos::FReal InTime, FPlaybackTickRecord& TickRecord, TArray<TPBDRigidParticleHandle<Chaos::FReal, 3>*>& OutUpdatedRigids) const
	{
		if (FDeformableSolver* Solver = GetDeformableSolver(InComponent))
		{
			FDeformableSolver::FPhysicsThreadAccess PhysicsThreadAccess(Solver, Softs::FPhysicsThreadAccessor());

			if (FEvolution* Evolution = PhysicsThreadAccess.GetEvolution())
			{
				FCacheEvaluationContext Context(TickRecord);
				Context.bEvaluateTransform = false;
				Context.bEvaluateCurves = false;
				Context.bEvaluateEvents = false;
				Context.bEvaluateChannels = true;

				// The evaluated result are already in world space since we are passing the tickrecord.spacetransform in the context
				FCacheEvaluationResult EvaluatedResult = InCache->Evaluate(Context, nullptr);

				const TArray<float>* PendingVX = EvaluatedResult.Channels.Find(VelocityXName);
				const TArray<float>* PendingVY = EvaluatedResult.Channels.Find(VelocityYName);
				const TArray<float>* PendingVZ = EvaluatedResult.Channels.Find(VelocityZName);
				const TArray<float>* PendingPX = EvaluatedResult.Channels.Find(PositionXName);
				const TArray<float>* PendingPY = EvaluatedResult.Channels.Find(PositionYName);
				const TArray<float>* PendingPZ = EvaluatedResult.Channels.Find(PositionZName);

				const int32 NumCachedParticles = EvaluatedResult.ParticleIndices.Num();

				FParticles& Particles = Evolution->Particles();

				const int32 NumParticles = Particles.Size();
				if (NumCachedParticles > 0)
				{
					// Directly set the result of the cache into the solver particles

					Softs::FSolverVec3* ParticleXs = &Particles.X(0);
					Softs::FSolverVec3* ParticleVs = &Particles.V(0);

					for (int32 CachedIndex = 0; CachedIndex < NumCachedParticles; ++CachedIndex)
					{
						const int32 ParticleIndex = EvaluatedResult.ParticleIndices[CachedIndex];
						if (ensure(ParticleIndex < NumParticles))
						{
							Softs::FSolverVec3& ParticleV = ParticleVs[ParticleIndex];
							Softs::FSolverVec3& ParticleX = ParticleXs[ParticleIndex];
							ParticleV.X = (*PendingVX)[CachedIndex];
							ParticleV.Y = (*PendingVY)[CachedIndex];
							ParticleV.Z = (*PendingVZ)[CachedIndex];
							ParticleX.X = (*PendingPX)[CachedIndex];
							ParticleX.Y = (*PendingPY)[CachedIndex];
							ParticleX.Z = (*PendingPZ)[CachedIndex];
						}
					}
				}
			}
		}
	}

	FGuid FFleshCacheAdapter::GetGuid() const
	{
		FGuid NewGuid;
		checkSlow(FGuid::Parse(TEXT("2C054706CB7441B582377B0EDACD12EE"), NewGuid));
		return NewGuid;
	}

	bool FFleshCacheAdapter::ValidForPlayback(UPrimitiveComponent* InComponent, UChaosCache* InCache) const
	{
		// If we have a flesh mesh we can play back any cache as long as it has one or more tracks
		const UFleshComponent* FleshComp = CastChecked<UFleshComponent>(InComponent);
		return FleshComp && InCache->ChannelCurveToParticle.Num() > 0;
	}

	Chaos::Softs::FDeformableSolver* FFleshCacheAdapter::GetDeformableSolver(UPrimitiveComponent* InComponent) const
	{
		if(InComponent)
		{
			if(UFleshComponent* FleshComp = CastChecked<UFleshComponent>(InComponent) )
			{
				if(FleshComp->GetDeformableSolver())
				{
					return FleshComp->GetDeformableSolver()->Solver.Get();
				}
			}
		}
		return nullptr;
	}

	Chaos::FPhysicsSolver* FFleshCacheAdapter::GetComponentSolver(UPrimitiveComponent* InComponent) const
	{
		return nullptr;
	}


	
	Chaos::FPhysicsSolverEvents* FFleshCacheAdapter::BuildEventsSolver(UPrimitiveComponent* InComponent) const
	{
		if(UFleshComponent* FleshComp = CastChecked<UFleshComponent>(InComponent))
		{
			// Initialize the physics solver at the beginning of the play/record
			FleshComp->RecreatePhysicsState();
			return GetDeformableSolver(InComponent);
		}
		return nullptr;
	}
	
	
	void FFleshCacheAdapter::SetRestState(UPrimitiveComponent* InComponent, UChaosCache* InCache, const FTransform& InRootTransform, Chaos::FReal InTime) const
	{
		if (!InCache || InCache->GetDuration() == 0.0f)
		{
			return;
		} 

		if (UFleshComponent* FleshComp = CastChecked<UFleshComponent>(InComponent))
		{
			FPlaybackTickRecord TickRecord;
			TickRecord.SetLastTime(InTime);

			FCacheEvaluationContext Context(TickRecord);
			Context.bEvaluateTransform = false;
			Context.bEvaluateCurves = false;
			Context.bEvaluateEvents = false;
			Context.bEvaluateChannels = true;

			FCacheEvaluationResult EvaluatedResult = InCache->Evaluate(Context, nullptr);

			const TArray<float>* PendingVX = EvaluatedResult.Channels.Find(VelocityXName);
			const TArray<float>* PendingVY = EvaluatedResult.Channels.Find(VelocityYName);
			const TArray<float>* PendingVZ = EvaluatedResult.Channels.Find(VelocityZName);
			const TArray<float>* PendingPX = EvaluatedResult.Channels.Find(PositionXName);
			const TArray<float>* PendingPY = EvaluatedResult.Channels.Find(PositionYName);
			const TArray<float>* PendingPZ = EvaluatedResult.Channels.Find(PositionZName);

			const int32 NumCachedParticles = EvaluatedResult.ParticleIndices.Num();

			const bool bHasPositions = PendingPX && PendingPY && PendingPZ;
			const bool bHasVelocities = PendingVX && PendingVY && PendingVZ;


			FleshComp->ResetDynamicCollection();

			if (bHasPositions && NumCachedParticles > 0)
			{
				if (UFleshDynamicAsset* DynamicCollection = FleshComp->GetDynamicCollection())
				{
					TManagedArray<FVector3f>& DynamicVertex = DynamicCollection->GetPositions();
					//TManagedArray<FVector3f>& DynamicVertex = DynamicCollection->GetVelocities();
					const int32 NumDynamicVertex = DynamicVertex.Num();
					if (NumDynamicVertex == NumCachedParticles)
					{
						for (int32 CachedIndex = 0; CachedIndex < NumCachedParticles; ++CachedIndex)
						{
							const int32 ParticleIndex = EvaluatedResult.ParticleIndices[CachedIndex];

							if (ensure(ParticleIndex < NumDynamicVertex))
							{
								DynamicVertex[ParticleIndex].X = (*PendingPX)[CachedIndex];
								DynamicVertex[ParticleIndex].Y = (*PendingPY)[CachedIndex];
								DynamicVertex[ParticleIndex].Z = (*PendingPZ)[CachedIndex];

								auto UEVertd = [](Chaos::FVec3 V) { return FVector3d(V.X, V.Y, V.Z); };
								auto UEVertf = [](FVector3d V) { return FVector3f((float)V.X, (float)V.Y, (float)V.Z); };
								DynamicVertex[ParticleIndex] = UEVertf(FleshComp->GetComponentTransform().InverseTransformPosition(UEVertd(DynamicVertex[ParticleIndex])));
							}
						}
					}
				}
			}
		}
	}

	bool FFleshCacheAdapter::InitializeForRecord(UPrimitiveComponent* InComponent, UChaosCache* InCache)
	{
		if( FDeformableSolver* Solver = GetDeformableSolver(InComponent))
		{
			FDeformableSolver::FGameThreadAccess GameThreadAccess(Solver, Softs::FGameThreadAccessor());
			GameThreadAccess.SetEnableSolver(true);
			if (UFleshComponent* FleshComp = CastChecked<UFleshComponent>(InComponent))
			{
				FleshComp->ResetDynamicCollection();
			}
		}
		return true;
	}

	bool FFleshCacheAdapter::InitializeForPlayback(UPrimitiveComponent* InComponent, UChaosCache* InCache, float InTime)
	{
		EnsureIsInGameThreadContext();
		
		if (FDeformableSolver* Solver = GetDeformableSolver(InComponent))
		{
			FDeformableSolver::FGameThreadAccess GameThreadAccess(Solver, Softs::FGameThreadAccessor());
			GameThreadAccess.SetEnableSolver(false);
			if (UFleshComponent* FleshComp = CastChecked<UFleshComponent>(InComponent))
			{
				FleshComp->ResetDynamicCollection();
			}
		}
		return true;
	}
}    // namespace Chaos
