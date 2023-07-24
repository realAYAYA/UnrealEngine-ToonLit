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

					for (uint32 ParticleIndex = 0; ParticleIndex < NumParticles; ++ParticleIndex)
					{
						const Softs::FSolverVec3& ParticleV = ParticleVs[ParticleIndex];
						const Softs::FSolverVec3& ParticleX = ParticleXs[ParticleIndex];

						// Adding the vertices relative position to the particle write datas
						FPendingParticleWrite NewData;

						NewData.ParticleIndex = ParticleIndex;
						//	NewData.PendingTransform = FTransform(FQuat::Identity, FVector(ParticleX)).GetRelativeTransform(InRootTransform);

						NewData.PendingCurveData.Add(MakeTuple(VelocityXName, ParticleV.X));
						NewData.PendingCurveData.Add(MakeTuple(VelocityYName, ParticleV.Y));
						NewData.PendingCurveData.Add(MakeTuple(VelocityZName, ParticleV.Z));

						NewData.PendingCurveData.Add(MakeTuple(PositionXName, ParticleX.X));
						NewData.PendingCurveData.Add(MakeTuple(PositionYName, ParticleX.Y));
						NewData.PendingCurveData.Add(MakeTuple(PositionZName, ParticleX.Z));

						OutFrame.PendingParticleData.Add(MoveTemp(NewData));
					}
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
				Context.bEvaluateCurves = true;
				Context.bEvaluateEvents = false;

				// The evaluated result are already in world space since we are passing the tickrecord.spacetransform in the context
				FCacheEvaluationResult EvaluatedResult = InCache->Evaluate(Context, nullptr);
				const int32 NumCurves = EvaluatedResult.Curves.Num();

				FParticles& Particles = Evolution->Particles();

				const uint32 NumParticles = Particles.Size();
				if (NumParticles > 0)
				{
					Softs::FSolverVec3* ParticleXs = &Particles.X(0);
					Softs::FSolverVec3* ParticleVs = &Particles.V(0);

					for (uint32 ParticleIndex = 0; ParticleIndex < NumParticles; ++ParticleIndex)
					{
						Softs::FSolverVec3& ParticleV = ParticleVs[ParticleIndex];
						Softs::FSolverVec3& ParticleX = ParticleXs[ParticleIndex];

						// Directly set the result of the cache into the solver particles
						if ( ParticleIndex < (uint32)EvaluatedResult.ParticleIndices.Num())
						{
							const int32 TransformIndex = EvaluatedResult.ParticleIndices[ParticleIndex];
							if (EvaluatedResult.Curves.IsValidIndex(TransformIndex))
							{
								if (const float* VelocityX = EvaluatedResult.Curves[TransformIndex].Find(VelocityXName))
								{
									ParticleV.X = *VelocityX;
								}
								if (const float* VelocityY = EvaluatedResult.Curves[TransformIndex].Find(VelocityYName))
								{
									ParticleV.Y = *VelocityY;
								}
								if (const float* VelocityZ = EvaluatedResult.Curves[TransformIndex].Find(VelocityZName))
								{
									ParticleV.Z = *VelocityZ;
								}
								if (const float* PositionX = EvaluatedResult.Curves[TransformIndex].Find(PositionXName))
								{
									ParticleX.X = *PositionX;
								}
								if (const float* PositionY = EvaluatedResult.Curves[TransformIndex].Find(PositionYName))
								{
									ParticleX.Y = *PositionY;
								}
								if (const float* PositionZ = EvaluatedResult.Curves[TransformIndex].Find(PositionZName))
								{
									ParticleX.Z = *PositionZ;
								}
							}
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
		return FleshComp && InCache->TrackToParticle.Num() > 0;
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
			Context.bEvaluateCurves = true;
			Context.bEvaluateEvents = false;

			FCacheEvaluationResult EvaluatedResult = InCache->Evaluate(Context, nullptr);

			FleshComp->ResetDynamicCollection();
			if (UFleshDynamicAsset* DynamicCollection = FleshComp->GetDynamicCollection())
			{
				TManagedArray<FVector3f>& DynamicVertex = DynamicCollection->GetPositions();
				//TManagedArray<FVector3f>& DynamicVertex = DynamicCollection->GetVelocities();

				const int32 NumCurves = EvaluatedResult.Curves.Num();
				if (NumCurves == EvaluatedResult.ParticleIndices.Num())
				{
					if (DynamicVertex.Num() == NumCurves)
					{
						for (int32 ParticleIndex = 0; ParticleIndex < NumCurves; ++ParticleIndex)
						{
							if (ParticleIndex < EvaluatedResult.ParticleIndices.Num())
							{
								const int32 TransformIndex = EvaluatedResult.ParticleIndices[ParticleIndex];
								if (EvaluatedResult.Curves.IsValidIndex(TransformIndex))
								{
									/*
									if (const float* VelocityX = EvaluatedResult.Curves[TransformIndex].Find(VelocityXName))
									{
										SimulationContext->CachedVelocities[ParticleIndex].X = *VelocityX;
									}
									if (const float* VelocityY = EvaluatedResult.Curves[TransformIndex].Find(VelocityYName))
									{
										SimulationContext->CachedVelocities[ParticleIndex].Y = *VelocityY;
									}
									if (const float* VelocityZ = EvaluatedResult.Curves[TransformIndex].Find(VelocityZName))
									{
										SimulationContext->CachedVelocities[ParticleIndex].Z = *VelocityZ;
									}
									*/
									if (const float* PositionX = EvaluatedResult.Curves[TransformIndex].Find(PositionXName))
									{
										DynamicVertex[ParticleIndex].X = *PositionX;
									}
									if (const float* PositionY = EvaluatedResult.Curves[TransformIndex].Find(PositionYName))
									{
										DynamicVertex[ParticleIndex].Y = *PositionY;
									}
									if (const float* PositionZ = EvaluatedResult.Curves[TransformIndex].Find(PositionZName))
									{
										DynamicVertex[ParticleIndex].Z = *PositionZ;
									}
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
