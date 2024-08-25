// Copyright Epic Games, Inc. All Rights Reserved.

#include "RewindData.h"
#include "PBDRigidsSolver.h"
#include "PhysicsProxy/SingleParticlePhysicsProxy.h"
#include "PhysicsProxy/ClusterUnionPhysicsProxy.h"
#include "HAL/IConsoleManager.h"
#include "Chaos/PBDJointConstraints.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace Chaos
{

FVec3 FGeometryParticleState::ZeroVector = FVec3(0);

void FGeometryParticleStateBase::SyncSimWritablePropsFromSim(FDirtyPropData Manager,const TPBDRigidParticleHandle<FReal,3>& Rigid)
{
	FDirtyChaosPropertyFlags Flags;
	Flags.MarkDirty(EChaosPropertyFlags::XR);
	Flags.MarkDirty(EChaosPropertyFlags::Velocities);
	Flags.MarkDirty(EChaosPropertyFlags::DynamicMisc);
	FDirtyChaosProperties Dirty;
	Dirty.SetFlags(Flags);

#if 0
	ParticlePositionRotation.SyncRemoteData(Manager,Dirty,[&Rigid](auto& Data)
	{
		Data.CopyFrom(Rigid);
	});

	Velocities.SyncRemoteData(Manager,Dirty,[&Rigid](auto& Data)
	{
		Data.SetV(Rigid.PreV());
		Data.SetW(Rigid.PreW());
	});

	KinematicTarget.SyncRemoteData(Manager, Dirty, [&Rigid](auto& Data)
	{
		Data = Rigid.KinematicTarget();
	});

	DynamicsMisc.SyncRemoteData(Manager, Dirty, [&Rigid](auto& Data)
	{
		Data.CopyFrom(Rigid);
		Data.SetObjectState(Rigid.PreObjectState());	//everything else is not writable by sim so must be the same
	});
#endif
}

void FGeometryParticleStateBase::SyncDirtyDynamics(FDirtyPropData& DestManager,const FDirtyChaosProperties& Dirty,const FConstDirtyPropData& SrcManager)
{
#if 0
	FParticleDirtyData DirtyFlags;
	DirtyFlags.SetFlags(Dirty.GetFlags());

	Dynamics.SyncRemoteData(DestManager,DirtyFlags,[&Dirty,&SrcManager](auto& Data)
	{
		Data = Dirty.GetDynamics(*SrcManager.Ptr,SrcManager.DataIdx);
	});
#endif
}

bool SimWritablePropsMayChange(const TGeometryParticleHandle<FReal,3>& Handle)
{
	const auto ObjectState = Handle.ObjectState();
	return ObjectState == EObjectStateType::Dynamic || ObjectState == EObjectStateType::Sleeping;
}

/* Deprecated UE5.4 */
bool FGeometryParticleStateBase::IsResimFrameValid(const FGeometryParticleHandle& Handle, const FFrameAndPhase FrameAndPhase) const
{
	if (FPBDRigidsSolver* RigidSolver = Handle.PhysicsProxy()->GetSolver<FPBDRigidsSolver>())
	{
		if (RigidSolver->GetEvolution() && RigidSolver->GetEvolution()->IsResimming())
		{
			const int32 ResimFrame = RigidSolver->GetEvolution()->GetIslandManager().GetParticleResimFrame(&Handle);
			if (ResimFrame != INDEX_NONE /*&& ResimFrame <= FrameAndPhase.Frame*/)
			{
				return false;
			}
		}
	}
	return true;
}

template <bool bSkipDynamics>
bool FGeometryParticleStateBase::IsInSync(const FGeometryParticleHandle& Handle, const FFrameAndPhase FrameAndPhase, const FDirtyPropertiesPool& Pool) const
{
	if (!ParticlePositionRotation.IsInSync(Handle, FrameAndPhase, Pool))
	{
		return false;
	}

	if (!NonFrequentData.IsInSync(Handle, FrameAndPhase, Pool))
	{
		return false;
	}

	//todo: deal with state change mismatch

	if (auto Kinematic = Handle.CastToKinematicParticle())
	{
		if (!Velocities.IsInSync(*Kinematic, FrameAndPhase, Pool))
		{
			return false;
		}

		if (!KinematicTarget.IsInSync(*Kinematic, FrameAndPhase, Pool))
		{
			return false;
		}
	}

	if (auto Rigid = Handle.CastToRigidParticle())
	{
		if (!bSkipDynamics)
		{
			if (!Dynamics.IsInSync(*Rigid, FrameAndPhase, Pool))
			{
				return false;
			}
		}

		if (!DynamicsMisc.IsInSync(*Rigid, FrameAndPhase, Pool))
		{
			return false;
		}

		if (!MassProps.IsInSync(*Rigid, FrameAndPhase, Pool))
		{
			return false;
		}
	}
	
	//TODO: this assumes geometry is never modified. Geometry modification has various issues in higher up Chaos code. Need stable shape id
	//For now iterate over all the shapes in latest and see if they have any mismatches
	/*if(ShapesArrayState.PerShapeData.Num())
	{
		return false;	//if any shapes changed just resim, this is not efficient but at least it's correct
	}*/

	return true;
}

template <bool bSkipDynamics>
bool FJointStateBase::IsInSync(const FPBDJointConstraintHandle& Handle, const FFrameAndPhase FrameAndPhase, const FDirtyPropertiesPool& Pool) const
{
	if (!JointSettings.IsInSync(Handle, FrameAndPhase, Pool))
	{
		return false;
	}

	return true;
}

void FRewindData::ApplyInputs(const int32 ApplyFrame, const bool bResetSolver)
{
	for (TWeakPtr<FBaseRewindHistory>& InputHistory : InputHistories)
	{
		if (InputHistory.IsValid())
		{
			InputHistory.Pin().Get()->ApplyInputs(ApplyFrame, bResetSolver);
		}
	}
}
void FRewindData::RewindStates(const int32 RewindFrame, const bool bResetSolver)
{
	for (TWeakPtr<FBaseRewindHistory>& StateHistory : StateHistories)
	{
		if (StateHistory.IsValid())
		{
			StateHistory.Pin().Get()->RewindStates(RewindFrame, bResetSolver);
		}
	}
}

void FRewindData::ApplyTargets(const int32 Frame, const bool bResetSimulation)
{
	RewindStates(Frame, bResetSimulation);

	EnsureIsInPhysicsThreadContext();

	//If property changed between Frame and CurFrame, record the latest value and rewind to old
	FFrameAndPhase RewindFrameAndPhase{ Frame, FFrameAndPhase::PostPushData };

	auto RewindHelper = [RewindFrameAndPhase, this](auto Obj, bool bResimAsFollower, auto& Property, const auto& RewindFunc)
	{
		if (!Property.IsClean(RewindFrameAndPhase) && !bResimAsFollower)
		{
			RewindFunc(Obj, *Property.Read(RewindFrameAndPhase, PropertiesPool));
		}
	};

	for (FDirtyParticleInfo& DirtyParticleInfo : DirtyParticles)
	{
		FGeometryParticleHandle* PTParticle = DirtyParticleInfo.GetObjectPtr();
		FGeometryParticleStateBase& History = DirtyParticleInfo.GetHistory();

		const bool bResimAsFollower = DirtyParticleInfo.bResimAsFollower;

		RewindHelper(PTParticle, bResimAsFollower, History.TargetPositions, [](auto Particle, const auto& Data) 
		{
			Particle->SetXR(Data); 
		});
		RewindHelper(PTParticle->CastToKinematicParticle(), bResimAsFollower, History.TargetVelocities, [](auto Particle, const auto& Data)
		{
			Particle->SetV(Data.V());
			Particle->SetW(Data.W());
		});
		RewindHelper(PTParticle->CastToRigidParticle(), bResimAsFollower, History.TargetStates, [this](auto Particle, const auto& Data) 
		{ 
			if (Particle == nullptr || Solver->GetEvolution() == nullptr)
			{
				return;
			}

			// Enable or disable the particle
			if (Particle->Disabled() != Data.Disabled())
			{
				if (Data.Disabled())
				{
					Solver->GetEvolution()->DisableParticle(Particle);
				}
				else
				{
					Solver->GetEvolution()->EnableParticle(Particle);
				}
			}

			// If we changed kinematics we need to rebuild the inertia conditioning
			const bool bDirtyInertiaConditioning = (Particle->ObjectState() != Data.ObjectState());
			if (bDirtyInertiaConditioning)
			{
				Particle->SetInertiaConditioningDirty();
			}
		
			Particle->SetDisabled(Data.Disabled());
			Solver->GetEvolution()->SetParticleObjectState(Particle, Data.ObjectState());

			if(Data.ObjectState() == EObjectStateType::Dynamic)
			{
				Particle->SetResimType(EResimType::FullResim);
			}
			else if((Data.ObjectState() == EObjectStateType::Static) || (Data.ObjectState() == EObjectStateType::Kinematic))
			{
				Particle->SetResimType(EResimType::ResimAsFollower);
			}
		});
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		if (!History.TargetPositions.IsClean(RewindFrameAndPhase) && Chaos::FPhysicsSolverBase::CanDebugNetworkPhysicsPrediction())
		{
			UE_LOG(LogChaos, Log, TEXT("Reset particle %d position to the target %s at frame %d"), PTParticle->UniqueIdx().Idx, *PTParticle->GetX().ToString(), Frame);
		}
#endif
	}
}

CHAOS_API bool bResimAllowRewindToResimulatedFrames = false;
FAutoConsoleVariableRef CVarResimAllowRewindToResimulatedFrames(TEXT("p.Resim.AllowRewindToResimulatedFrames"), bResimAllowRewindToResimulatedFrames, TEXT("Allow rewinding back to a frame that was previously part of a resimulation. If a resimulation is performed between frame 100-110, allow a new resim from 105-115 if needed, else next resim will be able to start from frame 111."));

bool FRewindData::RewindToFrame(int32 Frame)
{
	QUICK_SCOPE_CYCLE_COUNTER(RewindToFrame);

	EnsureIsInPhysicsThreadContext();
	//Can't go too far back
	const int32 EarliestFrame = GetEarliestFrame_Internal();
	if (Frame < EarliestFrame)
	{
#if DEBUG_REWIND_DATA
		UE_LOG(LogTemp, Log, TEXT("COMMON | PT | RewindToFrame | Failed due to rewind frame earlier than available history | Rewind Frame: %d | Earliest Frame: %d"), Frame, EarliestFrame);
#endif
		return false;
	}

	//If we need to save and we are right on the edge of the buffer, we can't go back to earliest frame
	if (Frame == EarliestFrame && bNeedsSave && FramesSaved == Managers.Capacity())
	{
#if DEBUG_REWIND_DATA
		UE_LOG(LogTemp, Log, TEXT("COMMON | PT | RewindToFrame | Failed due to rewinding to last available frame and bNeedsSave is set to true"));
#endif
		return false;
	}

	//If property changed between Frame and CurFrame, record the latest value and rewind to old
	FFrameAndPhase RewindFrameAndPhase{ Frame, FFrameAndPhase::PostPushData };
	FFrameAndPhase CurFrameAndPhase{ CurFrame, FFrameAndPhase::PrePushData };

	BlockResimFrame = bResimAllowRewindToResimulatedFrames ? Frame : CurFrame;

	auto RewindHelper = [RewindFrameAndPhase, CurFrameAndPhase, this](auto Obj, bool bResimAsFollower, auto& Property, const auto& RewindFunc) -> bool
	{
		if (bResimAsFollower)
		{
			//If we're rewinding a particle that doesn't need to save head (resim as follower never checks for desync so we don't care about head)
			if (auto Val = Property.Read(RewindFrameAndPhase, PropertiesPool))
			{
				RewindFunc(Obj, *Val);
			}
		}
		else
		{
			//If we're rewinding an object that needs to save head (during resim when we get back to latest frame and phase we need to check for desync)
			if (!Property.IsClean(RewindFrameAndPhase))
			{
				CopyDataFromObject(Property.WriteAccessMonotonic(CurFrameAndPhase, PropertiesPool), *Obj);
				RewindFunc(Obj, *Property.Read(RewindFrameAndPhase, PropertiesPool));

				return true;
			}
		}

		return false;
	};
			
	for(FDirtyParticleInfo& DirtyParticleInfo : DirtyParticles)
	{
		FGeometryParticleHandle* PTParticle = DirtyParticleInfo.GetObjectPtr();

		//rewind is about to start, all particles should be in sync at this point
		ensure(PTParticle->SyncState() == ESyncState::InSync);
		
		FGeometryParticleStateBase& History = DirtyParticleInfo.GetHistory(); //non-const in case we need to record what's at head for a rewind (CurFrame has already been increased to the next frame)

		History.CachePreCorrectionState(*PTParticle);

		const bool bResimAsFollower = DirtyParticleInfo.bResimAsFollower;

		bool bAnyChange = RewindHelper(PTParticle, bResimAsFollower, History.ParticlePositionRotation, [](auto Particle, const auto& Data) {Particle->SetXR(Data); });
		bAnyChange |= RewindHelper(PTParticle->CastToKinematicParticle(), bResimAsFollower, History.Velocities, [](auto Particle, const auto& Data) { Particle->SetV(Data.V()); Particle->SetW(Data.W()); });
		bAnyChange |= RewindHelper(PTParticle, bResimAsFollower, History.NonFrequentData, [this](auto Particle, const auto& Data)
			{
				Solver->GetEvolution()->InvalidateParticle(Particle); // Clear collision/constraints before updating NonFrequentData
				Particle->SetNonFrequentData(Data); 
			});
		bAnyChange |= RewindHelper(PTParticle->CastToKinematicParticle(), bResimAsFollower, History.KinematicTarget, [](auto Particle, const auto& Data) { Particle->SetKinematicTarget(Data); });
		bAnyChange |= RewindHelper(PTParticle->CastToRigidParticle(), bResimAsFollower, History.Dynamics, [](auto Particle, const auto& Data) {Particle->SetDynamics(Data); });
		bAnyChange |= RewindHelper(PTParticle->CastToRigidParticle(), bResimAsFollower, History.DynamicsMisc, [this](auto Particle, const auto& Data) {Solver->SetParticleDynamicMisc(Particle, Data); });
		bAnyChange |= RewindHelper(PTParticle->CastToRigidParticle(), bResimAsFollower, History.MassProps, [](auto Particle, const auto& Data) {Particle->SetMassProps(Data); });

		if (!bResimAsFollower)
		{
			if (bAnyChange)
			{
				//particle actually changes not just created/streamed so need to update its state

				//Data changes so send back to GT for interpolation. TODO: improve this in case data ends up being identical in resim
				Solver->GetEvolution()->GetParticles().MarkTransientDirtyParticle(DirtyParticleInfo.GetObjectPtr());

				DirtyParticleInfo.DirtyDynamics = INDEX_NONE;	//make sure to undo this as we want to record it again during resim

				//for now just mark anything that changed as enabled during resim. TODO: use bubble
				DirtyParticleInfo.GetObjectPtr()->SetEnabledDuringResim(true);
			}

			if (DirtyParticleInfo.InitializedOnStep > Frame)
			{
				//hasn't initialized yet, so disable
				//must do this after rewind because SetDynamicsMisc will re-enable
				//(the disable is a temp way to ignore objects not spawned yet, they weren't really disabled which is why it gets re-enabled)
				Solver->GetEvolution()->DisableParticle(DirtyParticleInfo.GetObjectPtr());
			}
		}
	}

#if !UE_BUILD_SHIPPING
	// For now, just ensure that the joints are InSync
	for(FDirtyJointInfo& DirtyJointInfo : DirtyJoints)
	{
		const FPBDJointConstraintHandle* Joint = DirtyJointInfo.GetObjectPtr();
		//rewind is about to start, all particles should be in sync at this point
		ensure(Joint->SyncState() == ESyncState::InSync);
	}
#endif

	ResimFrame = Frame;
	CurFrame = Frame;
	bNeedsSave = false;

	return true;
}

template <bool bSkipDynamics, typename TDirtyInfo>
void FRewindData::DesyncIfNecessary(TDirtyInfo& Info, const FFrameAndPhase FrameAndPhase)
{
	ensure(IsResim());	//shouldn't bother with desync unless we're resimming

	auto Handle = Info.GetObjectPtr();
	const auto& History = Info.GetHistory();

	if (Handle->SyncState() == ESyncState::InSync && !History.template IsInSync<bSkipDynamics>(*Handle, FrameAndPhase, PropertiesPool))
	{
		if (!SkipDesyncTest)
		{
			//first time desyncing so need to clear history from this point into the future
			DesyncObject(Info, FrameAndPhase);
		}
	}
}

template<>
void FRewindData::AccumulateErrorIfNecessary(FGeometryParticleHandle& Obj, const FFrameAndPhase FrameAndPhase)
{
	const FDirtyParticleInfo* DirtyInfo = DirtyParticles.Find(&Obj);
	if (!DirtyInfo)
	{
		return;
	}

	// Get the error offset after a correction
	const FVec3 ErrorX = DirtyInfo->GetHistory().PreCorrectionXR.X() - Obj.GetX();
	FQuat ErrorR = DirtyInfo->GetHistory().PreCorrectionXR.R() * Obj.GetR().Inverse();
	ErrorR.EnforceShortestArcWith(FQuat::Identity);
	ErrorR.Normalize();

	// Check if error is large enough to hide behind render interpolation
	if (!ErrorX.IsNearlyZero(0.1) || !ErrorR.IsIdentity(0.02))
	{
		// Find or add FDirtyParticleErrorInfo for the particle that has an error
		FDirtyParticleErrorInfo& ErrorInfo = [&]() ->FDirtyParticleErrorInfo&
		{
			if (FDirtyParticleErrorInfo* Found = DirtyParticleErrors.Find(&Obj))
			{
				return *Found;
			}

			return DirtyParticleErrors.Add(&Obj, FDirtyParticleErrorInfo(Obj));
		}();

		// Cache error for particle 
		ErrorInfo.AccumulateError(ErrorX, ErrorR);
	}
}

void FRewindData::FinishFrame()
{
	QUICK_SCOPE_CYCLE_COUNTER(RewindDataFinishFrame);

	if (IsResim())
	{
		FFrameAndPhase FutureFrame{ CurFrame + 1, FFrameAndPhase::PrePushData };

		auto FinishHelper = [this, FutureFrame](auto& DirtyObjs)
		{
			for (auto& Info : DirtyObjs)
			{
				if (Info.bResimAsFollower)
				{
					//resim as follower means always in sync and no cleanup needed
					continue;
				}

				auto& Handle = *Info.GetObjectPtr();

				if (Handle.ResimType() == EResimType::FullResim)
				{
					if (IsFinalResim())
					{
						// Cache the correction offset after a resimulation
						AccumulateErrorIfNecessary(Handle, FutureFrame);

						//Last resim so mark as in sync
						Handle.SetSyncState(ESyncState::InSync);
						Handle.SetEnabledDuringResim(false);

						//Anything saved on upcoming frame (was done during rewind) can be removed since we are now at head
						Info.ClearPhaseAndFuture(FutureFrame);
					}
					else
					{
						//solver doesn't affect dynamics, so no reason to test if they desynced from original sim
						//question: should we skip all other properties? dynamics is a commonly changed one but might be worth skipping everything solver skips
						DesyncIfNecessary</*bSkipDynamics=*/true>(Info, FutureFrame);
					}
				}
			}
		};

		FinishHelper(DirtyParticles);
		FinishHelper(DirtyJoints);
	}
	
	++CurFrame;
	LatestFrame = FMath::Max(LatestFrame, CurFrame);
}

void FRewindData::DumpHistory_Internal(const int32 FramePrintOffset, const FString& Filename)
{
	FStringOutputDevice Out;
	const int32 EarliestFrame = GetEarliestFrame_Internal();
	for(int32 Frame = EarliestFrame; Frame < CurFrame; ++Frame)
	{
		for (int32 Phase = 0; Phase < FFrameAndPhase::EParticleHistoryPhase::NumPhases; ++Phase)
		{
			for(const FDirtyParticleInfo& Info : DirtyParticles)
			{
				Out.Logf(TEXT("Frame:%d Phase:%d\n"), Frame + FramePrintOffset, Phase);
				FGeometryParticleState State = GetPastStateAtFrame(*Info.GetObjectPtr(), Frame, (FFrameAndPhase::EParticleHistoryPhase)Phase);
				Out.Logf(TEXT("%s\n"), *State.ToString());
			}

			for (const FDirtyJointInfo& Info : DirtyJoints)
			{
				Out.Logf(TEXT("Frame:%d Phase:%d\n"), Frame + FramePrintOffset, Phase);

				FJointState State = GetPastJointStateAtFrame(*Info.GetObjectPtr(), Frame, (FFrameAndPhase::EParticleHistoryPhase)Phase);
				Out.Logf(TEXT("%s\n"), *State.ToString()); 
			}
		}
	}

	FString Path = FPaths::ProfilingDir() + FString::Printf(TEXT("/RewindData/%s_%d_%d.txt"), *Filename, EarliestFrame + FramePrintOffset, CurFrame - 1 + FramePrintOffset);
	FFileHelper::SaveStringToFile(Out, *Path);
	UE_LOG(LogChaos, Warning, TEXT("Saved:%s"), *Path);
}

CHAOS_API int32 SkipDesyncTest = 0;
FAutoConsoleVariableRef CVarSkipDesyncTest(TEXT("p.SkipDesyncTest"), SkipDesyncTest, TEXT("Skips hard desync test, this means all particles will assume to be clean except spawning at different times. This is useful for a perf lower bound, not actually correct"));

void FRewindData::AdvanceFrameImp(IResimCacheBase* ResimCache)
{
	FramesSaved = FMath::Min(FramesSaved + 1, static_cast<int32>(Managers.Capacity()));

	const int32 EarliestFrame = CurFrame - FramesSaved;
	const FFrameAndPhase FrameAndPhase{ CurFrame, FFrameAndPhase::PostCallbacks };

	auto AdvanceHelper = [this, EarliestFrame, FrameAndPhase](auto& DirtyObjects, const auto& DesyncFunc, const auto& AdvanceDirtyFunc)
	{
		const int32 InitialNumDirtyObjects = DirtyObjects.Num();
		for (int32 DirtyIdx = InitialNumDirtyObjects - 1; DirtyIdx >= 0; --DirtyIdx)
		{
			auto& Info = DirtyObjects.GetDenseAt(DirtyIdx);

			ensure(IsResimAndInSync(*Info.GetObjectPtr()) || Info.GetHistory().IsClean(FrameAndPhase));  //Sim hasn't run yet so PostCallbacks (sim results) should be clean
	
			//if hasn't changed in a while stop tracking
			if (Info.LastDirtyFrame < EarliestFrame)
			{
				RemoveObject(Info.GetObjectPtr(), EAllowShrinking::No);
			}
			else
			{
				auto Handle = Info.GetObjectPtr();
				Info.bResimAsFollower = Handle->ResimType() == EResimType::ResimAsFollower;

				if (IsResim() && !Info.bResimAsFollower)
				{
					DesyncIfNecessary</*bSkipDynamics=*/false>(Info, FrameAndPhase);
				}

				if (IsResim() && Handle->SyncState() != ESyncState::InSync && !SkipDesyncTest)
				{
					Handle->SetEnabledDuringResim(true);	//for now just mark anything out of sync as resim enabled. TODO: use bubble
					DesyncFunc(Handle);
				}

				AdvanceDirtyFunc(Info, Handle);
			}
		}
		if (InitialNumDirtyObjects > 0)
		{
			DirtyObjects.Shrink();
		}
	};

	TArray<FGeometryParticleHandle*> DesyncedParticles;
	if (IsResim())
	{
		DesyncedParticles.Reserve(DirtyParticles.Num());
	}

	AdvanceHelper(DirtyParticles,
		[&DesyncedParticles](FGeometryParticleHandle* DesyncedHandle)
		{
			DesyncedParticles.Add(DesyncedHandle);
		},

		[this, FrameAndPhase](FDirtyParticleInfo& Info, FGeometryParticleHandle* Handle)
		{
			if (Info.DirtyDynamics == CurFrame && !IsResimAndInSync(*Handle))
			{
				//we only need to check the cast because right now there's no property system on PT, so any time a sim callback touches a particle we just mark it as dirty dynamics
				if (auto Rigid = Handle->CastToRigidParticle())
				{
					//sim callback is finished so record the dynamics before solve starts
					FGeometryParticleStateBase& Latest = Info.AddFrame(CurFrame);
					Latest.Dynamics.WriteAccessMonotonic(FrameAndPhase, PropertiesPool).CopyFrom(*Rigid);
				}
			}
		});

	AdvanceHelper(DirtyJoints, [](const FPBDJointConstraintHandle*) {}, [](const FDirtyJointInfo&, const FPBDJointConstraintHandle*) {});

	//TODO: if joint is desynced we should desync particles as well
	//If particle of joint is desynced, we need to make sure the joint is reconsidered too for optimization, though maybe not "desynced"

	if (IsResim() && ResimCache)
	{
		ResimCache->SetDesyncedParticles(MoveTemp(DesyncedParticles));
	}
}

#ifndef REWIND_DESYNC
#define REWIND_DESYNC 0
#endif

void FRewindData::PushGTDirtyData(const FDirtyPropertiesManager& SrcManager,const int32 SrcDataIdx,const FDirtyProxy& Dirty, const FShapeDirtyData* ShapeDirtyData)
{
	//This records changes enqueued by GT.
	bNeedsSave = true;

	IPhysicsProxyBase* Proxy = Dirty.Proxy;
	if (Proxy == nullptr)
	{
		return;
	}

	//Helper to group most of the common logic about push data recording
	//NOTE: when possible use passed in CopyFunc to do work, if lambda returns false you cannot record to history buffer
	auto CopyHelper = [this, Proxy](auto Object, const auto& CopyFunc) -> bool
	{
		//Don't bother tracking static particles. We assume they stream in and out and don't need to be rewound
		//TODO: find a way to skip statics that stream in and out - gameplay can technically spawn/destroy these so we can't just ignore statics
		/*if(PTParticle->CastToKinematicParticle() == nullptr)
		{
			return;
		}*/

		//During a resim the same exact push data comes from gt
		//If the particle is already in sync, it will stay in sync so no need to touch history
		if (IsResim() && Object->SyncState() == ESyncState::InSync)
		{
			return false;
		}

		if (IsResim() && Proxy->GetInitializedStep() == CurFrame)
		{
			//Particle is reinitialized, since it's out of sync it must be at a different time
			//So make sure it's considered during resim
			//TODO: should check if in bubble
			Object->SetEnabledDuringResim(true);
		}

		auto& Info = FindOrAddDirtyObj(*Object, Proxy->IsInitialized() ? INDEX_NONE : CurFrame);
		auto& Latest = Info.AddFrame(CurFrame);

		//At this point all phases should be clean
		ensure(Latest.IsClean(FFrameAndPhase{ CurFrame, FFrameAndPhase::PrePushData }));

		//Most objects never change but may be created/destroyed often due to streaming
		//To avoid useless writes we call this function before PushData is processed.
		//This means we will skip objects that are streamed in since they never change
		//So if Proxy has initialized it means the particle isn't just streaming in, it's actually changing
		if (Info.InitializedOnStep < CurFrame)
		{
			CopyFunc(Latest);
		}

		ensure(Latest.IsClean(FFrameAndPhase{ CurFrame, FFrameAndPhase::PostPushData }));   //PostPushData is untouched
		ensure(Latest.IsClean(FFrameAndPhase{ CurFrame, FFrameAndPhase::PostCallbacks }));	//PostCallback is untouched

		return true;
	};

	auto DirtyPropHelper = [this, &Dirty](auto& Property, const EChaosPropertyFlags PropName, const auto& Object)
	{
		if (Dirty.PropertyData.IsDirty(PropName))
		{
			auto& Data = Property.WriteAccessMonotonic(FFrameAndPhase{ CurFrame, FFrameAndPhase::PrePushData }, PropertiesPool);
			CopyDataFromObject(Data, Object);
		}
	};

	switch(Proxy->GetType())
	{
	case EPhysicsProxyType::SingleParticleProxy:
	{
		FSingleParticlePhysicsProxy* ParticleProxy = static_cast<FSingleParticlePhysicsProxy*>(Proxy);
		if (ParticleProxy == nullptr)
		{
			break;
		}

		FGeometryParticleHandle* PTParticle = ParticleProxy->GetHandle_LowLevel();
		if (PTParticle == nullptr)
		{
			break;
		}

		const bool bKeepRecording = CopyHelper(PTParticle, [PTParticle, &DirtyPropHelper](FGeometryParticleStateBase& Latest)
		{
			DirtyPropHelper(Latest.ParticlePositionRotation, EChaosPropertyFlags::XR, *PTParticle);
			DirtyPropHelper(Latest.NonFrequentData, EChaosPropertyFlags::NonFrequentData, *PTParticle);

			if (auto Kinematic = PTParticle->CastToKinematicParticle())
			{
				DirtyPropHelper(Latest.Velocities, EChaosPropertyFlags::Velocities, *Kinematic);
				DirtyPropHelper(Latest.KinematicTarget, EChaosPropertyFlags::KinematicTarget, *Kinematic);

				if (auto Rigid = Kinematic->CastToRigidParticle())
				{
					DirtyPropHelper(Latest.DynamicsMisc, EChaosPropertyFlags::DynamicMisc, *Rigid);
					DirtyPropHelper(Latest.MassProps, EChaosPropertyFlags::MassProps, *Rigid);
				}
			}
		});

		if (bKeepRecording)
		{
			//Dynamics are not available at head (sim zeroes them out), so we have to record them as PostPushData (since they're applied as part of PushData)
			if (auto NewData = Dirty.PropertyData.FindDynamics(SrcManager, SrcDataIdx))
			{
				FDirtyParticleInfo& Info = FindOrAddDirtyObj(*PTParticle, ParticleProxy->IsInitialized() ? INDEX_NONE : CurFrame);
				FGeometryParticleStateBase& Latest = Info.AddFrame(CurFrame);
				const FFrameAndPhase PostPushData{ CurFrame, FFrameAndPhase::PostPushData };
				Latest.Dynamics.WriteAccessMonotonic(PostPushData, PropertiesPool) = *NewData;
				Info.DirtyDynamics = CurFrame;	//Need to save the dirty dynamics into the next phase as well (it's possible a callback will stomp the dynamics value, so that's why it's pending)

				ensure(Latest.IsCleanExcludingDynamics(PostPushData)); //PostPushData is untouched except for dynamics
			}
		}
		break;
	}
	case EPhysicsProxyType::JointConstraintType:
	{
		FJointConstraintPhysicsProxy* JointProxy = static_cast<FJointConstraintPhysicsProxy*>(Proxy);
		if (JointProxy == nullptr)
		{
			break;
		}
		FPBDJointConstraintHandle* Joint = JointProxy->GetHandle();
		if (Joint == nullptr)
		{
			break;
		}

		CopyHelper(Joint, [Joint, &DirtyPropHelper](FJointStateBase& Latest)
		{
			DirtyPropHelper(Latest.JointSettings, EChaosPropertyFlags::JointSettings, *Joint);
		});
		break;
	}
	default:
	{
		ensure(false);	//Unsupported proxy type
	}
	}
}

void FRewindData::SpawnProxyIfNeeded(FSingleParticlePhysicsProxy& Proxy)
{
	if(Proxy.GetInitializedStep() > CurFrame)
	{
		FGeometryParticleHandle* Handle = Proxy.GetHandle_LowLevel();
		FDirtyParticleInfo& Info = FindOrAddDirtyObj(*Handle, CurFrame);

		Solver->GetEvolution()->EnableParticle(Handle);
		if(Proxy.GetInitializedStep() != CurFrame)
		{
			DesyncObject(Info, FFrameAndPhase{ Proxy.GetInitializedStep(), FFrameAndPhase::PrePushData });	//Spawned earlier so mark as desynced from that first frame
			Proxy.SetInitialized(CurFrame);
			Info.InitializedOnStep = CurFrame;
		}
	}
}

// Hand over error data per particle from RewindData to a solver collection instead which gets marshalled to GT
void FRewindData::BufferPhysicsResults(TMap<const IPhysicsProxyBase*, struct FDirtyRigidParticleReplicationErrorData>& DirtyRigidErrors)
{
	DirtyRigidErrors.Reserve(DirtyParticleErrors.Num());

	for (const FDirtyParticleErrorInfo& ErrorInfo : DirtyParticleErrors)
	{
		FDirtyRigidParticleReplicationErrorData ErrorData;
		ErrorData.ErrorX = ErrorInfo.GetErrorX();
		ErrorData.ErrorR = ErrorInfo.GetErrorR();

		const IPhysicsProxyBase* PhysicsProxy = static_cast<const IPhysicsProxyBase*>(ErrorInfo.GetObjectPtr()->PhysicsProxy());
		DirtyRigidErrors.Add(PhysicsProxy, ErrorData);

		// todo, move out of RewindData.cpp, possibly to ChaosResultManager.cpp
		// If ClusterUnion, hand over error to child GeometryCollections which do their own render interpolation and need the error offset individually
		if (PhysicsProxy->GetType() == EPhysicsProxyType::ClusterUnionProxy)
		{
			const FClusterUnionPhysicsProxy* ClusterProxy = static_cast<const FClusterUnionPhysicsProxy*>(PhysicsProxy);
			if (ClusterProxy)
			{
				for (IPhysicsProxyBase* ChildProxy : ClusterProxy->GetParticle_Internal()->PhysicsProxies())
				{
					if (ChildProxy->GetType() == EPhysicsProxyType::GeometryCollectionType)
					{
						DirtyRigidErrors.Add(ChildProxy, ErrorData);
					}
				}
			}
		}
	}

	DirtyParticleErrors.Reset();
}

void FRewindData::MarkDirtyFromPT(FGeometryParticleHandle& Handle)
{
	FDirtyParticleInfo& Info = FindOrAddDirtyObj(Handle);

	Info.DirtyDynamics = CurFrame;

	const bool bRecordingHistory = !IsResimAndInSync(Handle);

	FGeometryParticleStateBase& Latest = Info.AddFrame(CurFrame);

	//TODO: use property system
	//For now we just dirty all PT properties that we typically use
	//This means sim callback can't modify mass, geometry, etc... (only properties touched by this function)
	//Note these same properties are sent back to GT, so it's not just this function that needs updating

	const FFrameAndPhase FrameAndPhase{ CurFrame, FFrameAndPhase::PostPushData };

	if (bRecordingHistory || Latest.ParticlePositionRotation.IsClean(FrameAndPhase))
	{
		if (auto Data = Latest.ParticlePositionRotation.WriteAccessNonDecreasing(FrameAndPhase, PropertiesPool))
		{
			Data->CopyFrom(Handle);
		}
	}
	

	if (auto Kinematic = Handle.CastToKinematicParticle())
	{
		if (bRecordingHistory || Latest.Velocities.IsClean(FrameAndPhase))
		{
			if (auto Data = Latest.Velocities.WriteAccessNonDecreasing(FrameAndPhase, PropertiesPool))
			{
				Data->CopyFrom(*Kinematic);
			}
		}

		if (auto Rigid = Kinematic->CastToRigidParticle())
		{
			if (bRecordingHistory || Latest.DynamicsMisc.IsClean(FrameAndPhase))
			{
				if (auto Data = Latest.DynamicsMisc.WriteAccessNonDecreasing(FrameAndPhase, PropertiesPool))
				{
					Data->CopyFrom(*Rigid);
				}
			}
		}
	}
}

void FRewindData::MarkDirtyJointFromPT(FPBDJointConstraintHandle& Handle)
{
	FDirtyJointInfo& Info = FindOrAddDirtyObj(Handle);

	const bool bRecordingHistory = !IsResimAndInSync(Handle);

	FJointStateBase& Latest = Info.AddFrame(CurFrame);

	//TODO: use property system

	const FFrameAndPhase FrameAndPhase{ CurFrame, FFrameAndPhase::PostPushData };

	if (bRecordingHistory || Latest.JointSettings.IsClean(FrameAndPhase))
	{
		if (auto Data = Latest.JointSettings.WriteAccessNonDecreasing(FrameAndPhase, PropertiesPool))
		{
			CopyDataFromObject(*Data, Handle);
		}
	}
}

void FRewindData::ClearPhaseAndFuture(FGeometryParticleHandle& Handle, int32 Frame, FFrameAndPhase::EParticleHistoryPhase Phase)
{
	FDirtyParticleInfo& Info = FindOrAddDirtyObj(Handle);
	const FFrameAndPhase FrameAndPhase{ Frame, Phase };
	Info.ClearPhaseAndFuture(FrameAndPhase);
}

void FRewindData::ExtendHistoryWithFrame(const int32 Frame)
{
	FramesSaved = FMath::Max(CurFrame - Frame+1, FramesSaved);
}

// todo, implement into settings
enum class EResimFrameValidation : int32
{
	FullValidation = 0, // No leniency, validate all dirty particle
	IslandValidation = 1, // Validate dirty particles inside the islands that have resim trigger particles in them
	TriggerParticleValidation = 2 // Only validate the resim triggering particle(s)
};
CHAOS_API int32 ResimFrameValidation = (int32)EResimFrameValidation::IslandValidation;
FAutoConsoleVariableRef CVarResimFrameValidationLeniency(TEXT("p.Resim.ResimFrameValidation"), ResimFrameValidation, TEXT("0 = no leniency, all dirty particles need a valid target. 1 = Island leniency, all particles in resim islands need a valid target. 2 = Full leniency, only the particle triggering the resim need a valid target."));
CHAOS_API bool bResimIncompleteHistory = false;
FAutoConsoleVariableRef CVarResimIncompleteHistory(TEXT("p.Resim.IncompleteHistory"), bResimIncompleteHistory, TEXT("If a valid resim frame can't be found, use the requested resim frame and perform a resimulation with incomplete data."));

int32 FRewindData::FindValidResimFrame(const int32 RequestedFrame)
{
	int32 ValidFrame = INDEX_NONE;

	if (RequestedFrame <= BlockResimFrame)
	{
#if DEBUG_REWIND_DATA
		UE_LOG(LogTemp, Log, TEXT("COMMON | PT | FindValidResimFrame | Resim is blocked | BlockResimFrame: %d | RequestedFrame: %d"), BlockResimFrame, RequestedFrame);
#endif

		return ValidFrame;
	}

	EnsureIsInPhysicsThreadContext();

	auto TargetFinderHelper = [&](FDirtyParticleInfo* DirtyParticleInfo, const FFrameAndPhase FrameAndPhase) -> bool
	{
		bool bValid = true;
		const bool bResimAsFollower = DirtyParticleInfo->bResimAsFollower;
		FGeometryParticleStateBase& History = DirtyParticleInfo->GetHistory();
		if (const FParticleDynamicMisc* DynamicMisc = History.DynamicsMisc.Read(FrameAndPhase, PropertiesPool))
		{
			if (!DynamicMisc->Disabled() && (DynamicMisc->ObjectState() == EObjectStateType::Dynamic) && !History.TargetPositions.IsEmpty() && !History.TargetVelocities.IsEmpty() && !History.TargetStates.IsEmpty())
			{
				if (bResimAsFollower || History.TargetPositions.IsClean(FrameAndPhase) || History.TargetVelocities.IsClean(FrameAndPhase) || History.TargetStates.IsClean(FrameAndPhase))
				{
					bValid = false;
				}
			}
		}

		return bValid;
	};

	Private::FPBDIslandManager& IslandManager = Solver->GetEvolution()->GetIslandManager();

	// Cache all particles in islands that have a resim triggering particle
	TArray<const FGeometryParticleHandle*> ResimIslandParticles;
	if (ResimFrameValidation == (int32)EResimFrameValidation::IslandValidation)
	{
		TArray<const Private::FPBDIsland*> ResimIslands;
		for (FDirtyParticleInfo& DirtyParticleInfo : DirtyParticles)
		{
			FGeometryParticleHandle* Handle = DirtyParticleInfo.GetObjectPtr();
			if (IslandManager.GetParticleResimFrame(Handle) != INDEX_NONE)
			{
				ResimIslands.AddUnique(IslandManager.GetParticleIsland(Handle));
			}
		}

		ResimIslandParticles = IslandManager.FindParticlesInIslands(ResimIslands);
	}

	// First frame of the history datas
	const int32 EarliestFrame = FMath::Max(GetEarliestFrame_Internal(), BlockResimFrame);
	bool bHasTargetHistory = false;

	for (ValidFrame = RequestedFrame; ValidFrame > EarliestFrame; ValidFrame--)
	{
		const FFrameAndPhase FrameAndPhase{ ValidFrame, FFrameAndPhase::PostPushData };
		bHasTargetHistory = true;
		
#if DEBUG_REWIND_DATA
		UE_LOG(LogTemp, Log, TEXT("COMMON | PT | FindValidResimFrame | Processing resim particles | History Frame: %d | Total Particle Count: %d | ResimIslands Particle Count: %d | ResimFrameValidation: %d"), ValidFrame, DirtyParticles.Num(), ResimIslandParticles.Num(), ResimFrameValidation);
#endif

		if ((EResimFrameValidation)ResimFrameValidation == EResimFrameValidation::IslandValidation)
		{
			// Iterate over islands previously found having resim particles in them and check if the particles in the islands have targets
			for (const FGeometryParticleHandle* IslandParticle : ResimIslandParticles)
			{
				// Cache particle handles for objects in islands that need resim
				if (FDirtyParticleInfo* DirtyParticleInfo = FindDirtyObj(*IslandParticle))
				{
					if (!TargetFinderHelper(DirtyParticleInfo, FrameAndPhase))
					{
						bHasTargetHistory = false;
						break;
					}
				}
			}
		}
		else
		{
			for (FDirtyParticleInfo& DirtyParticleInfo : DirtyParticles)
			{
				// If running validation leniency, check if the particle is marked for resimulation else don't bother checking for valid target states.
				if ((EResimFrameValidation)ResimFrameValidation == EResimFrameValidation::TriggerParticleValidation)
				{
					FGeometryParticleHandle* Handle = DirtyParticleInfo.GetObjectPtr();
					if(IslandManager.GetParticleResimFrame(Handle) == INDEX_NONE)
					{
						continue;
					}
				}

				if (!TargetFinderHelper(&DirtyParticleInfo, FrameAndPhase))
				{
					bHasTargetHistory = false;
					break;
				}
			}
		}
		
		if (bHasTargetHistory)
		{
			for (TWeakPtr<FBaseRewindHistory>& InputHistory : InputHistories)
			{
				PRAGMA_DISABLE_DEPRECATION_WARNINGS // TODO: Change to HasValidData() in UE 5.6 and remove deprecation pragma
				if (InputHistory.IsValid() && !InputHistory.Pin().Get()->HasValidDatas(ValidFrame))
				PRAGMA_ENABLE_DEPRECATION_WARNINGS
				{
					bHasTargetHistory = false;
					break;
				}
			}
		}

		if (bHasTargetHistory)
		{
			for (TWeakPtr<FBaseRewindHistory>& StateHistory : StateHistories)
			{
				PRAGMA_DISABLE_DEPRECATION_WARNINGS // TODO: Change to HasValidData() in UE 5.6 and remove deprecation pragma
				if (StateHistory.IsValid() && !StateHistory.Pin().Get()->HasValidDatas(ValidFrame))
				PRAGMA_ENABLE_DEPRECATION_WARNINGS
				{
					bHasTargetHistory = false;
					break;
				}
			}
		}

		if (bHasTargetHistory)
		{
			break;
		}
	}
	
	if (!bHasTargetHistory)
	{
		ValidFrame = bResimIncompleteHistory ? RequestedFrame : INDEX_NONE;

#if DEBUG_REWIND_DATA
		UE_LOG(LogTemp, Warning, TEXT("COMMON | PT | FindValidResimFrame | No valid resim frame found | RequestedFrame: %d | ValidFrame: %d | EarliestFrame: %d | HasTargetHistory: %d | EarliestHistoryFrame: %d | CurrentFrame: %d | FramesSaved: %d | ResimFrameValidation: %d"), RequestedFrame, ValidFrame, EarliestFrame, bHasTargetHistory, GetEarliestFrame_Internal(), CurrentFrame(), FramesSaved, ResimFrameValidation);
#endif
	}

	return ValidFrame;
}

void FRewindData::PushStateAtFrame(FGeometryParticleHandle& Handle, int32 Frame, FFrameAndPhase::EParticleHistoryPhase Phase, 
	const FVector& Position, const FQuat& Quaternion, const FVector& LinVelocity, const FVector& AngVelocity, const bool bShouldSleep)
{
	const bool bRecordingHistory = !IsResimAndInSync(Handle);

	FDirtyParticleInfo& Info = FindOrAddDirtyObj(Handle);
	FGeometryParticleStateBase& Latest = Info.GetHistory();
	const FFrameAndPhase FrameAndPhase{ Frame, Phase };

	if (bRecordingHistory || Latest.TargetPositions.IsClean(FrameAndPhase))
	{
		FParticlePositionRotation& PositionRotation = Latest.TargetPositions.Insert(FrameAndPhase, PropertiesPool);
		PositionRotation.SetX(Position);
		PositionRotation.SetR(Quaternion);
	}

	if (bRecordingHistory || Latest.TargetVelocities.IsClean(FrameAndPhase))
	{
		FParticleVelocities& PreVelocities = Latest.TargetVelocities.Insert(FrameAndPhase, PropertiesPool);
		PreVelocities.SetV(LinVelocity);
		PreVelocities.SetW(AngVelocity);
	}

	if (bRecordingHistory || Latest.TargetStates.IsClean(FrameAndPhase))
	{
		FParticleDynamicMisc& PreDynamicsMisc = Latest.TargetStates.Insert(FrameAndPhase, PropertiesPool);
		PreDynamicsMisc.SetObjectState(bShouldSleep ? EObjectStateType::Sleeping : EObjectStateType::Dynamic);
		PreDynamicsMisc.SetDisabled(false);
	}
}

void FRewindData::PushPTDirtyData(TPBDRigidParticleHandle<FReal, 3>& Handle, const int32 SrcDataIdx)
{
	const bool bRecordingHistory = !IsResimAndInSync(Handle);

	FDirtyParticleInfo& Info = FindOrAddDirtyObj(Handle);
	FGeometryParticleStateBase& Latest = Info.AddFrame(CurFrame);

	const FFrameAndPhase FrameAndPhase{ CurFrame, FFrameAndPhase::PostCallbacks };

	if (bRecordingHistory || Latest.ParticlePositionRotation.IsClean(FrameAndPhase))
	{
		if (FParticlePositionRotation* PreXR = Latest.ParticlePositionRotation.WriteAccessNonDecreasing(FrameAndPhase, PropertiesPool))
		{
			PreXR->CopyFrom(Handle);
		}
	}

	if (bRecordingHistory || Latest.Velocities.IsClean(FrameAndPhase))
	{
		if (FParticleVelocities* PreVelocities = Latest.Velocities.WriteAccessNonDecreasing(FrameAndPhase, PropertiesPool))
		{
			PreVelocities->SetV(Handle.GetPreV());
			PreVelocities->SetW(Handle.GetPreW());
		}
	}
	
	if (bRecordingHistory || Latest.DynamicsMisc.IsClean(FrameAndPhase))
	{
		if (FParticleDynamicMisc* PreDynamicMisc = Latest.DynamicsMisc.WriteAccessNonDecreasing(FrameAndPhase, PropertiesPool))
		{
			PreDynamicMisc->CopyFrom(Handle);	//everything is immutable except object state
			PreDynamicMisc->SetObjectState(Handle.PreObjectState());
		}
	}
}

FGeometryParticleState FRewindData::GetPastStateAtFrame(const FGeometryParticleHandle& Handle, int32 Frame, FFrameAndPhase::EParticleHistoryPhase Phase) const
{
	return GetPastStateAtFrameImp<FGeometryParticleState>(DirtyParticles, Handle, Frame, Phase);
}

FJointState FRewindData::GetPastJointStateAtFrame(const FPBDJointConstraintHandle& Handle, int32 Frame, FFrameAndPhase::EParticleHistoryPhase Phase) const
{
	return GetPastStateAtFrameImp<FJointState>(DirtyJoints, Handle, Frame, Phase);
}

CHAOS_API int32 bInterpolateTargetGaps = 5;
FAutoConsoleVariableRef CVarResimInterpolateTargetGaps(TEXT("p.Resim.InterpolateTargetGaps"), bInterpolateTargetGaps, TEXT("How many frame gaps in replicated targets we should fill by interpolating between the previous and the new target received. Value in max number of frames to interpolate, deactivate by setting to 0."));

void FRewindData::SetTargetStateAtFrame(FGeometryParticleHandle& Handle, const int32 Frame, FFrameAndPhase::EParticleHistoryPhase Phase,
	const FVector& Position, const FQuat& Quaternion, const FVector& LinVelocity, const FVector& AngVelocity, const bool bShouldSleep)
{
	if (bInterpolateTargetGaps)
	{
		FDirtyParticleInfo& Info = FindOrAddDirtyObj(Handle);
		FGeometryParticleStateBase& Latest = Info.GetHistory();
		FFrameAndPhase FrameAndPhase;

		if (Latest.TargetPositions.GetHeadFrameAndPhase(FrameAndPhase))
		{
			const int32 FrameDiff = Frame - FrameAndPhase.Frame;
			if (FrameDiff > 1 && FrameDiff <= bInterpolateTargetGaps)
			{
				const FParticlePositionRotation* TargetXR = Latest.TargetPositions.Read(FrameAndPhase, PropertiesPool);
				const FParticleVelocities* TargetVW = Latest.TargetVelocities.Read(FrameAndPhase, PropertiesPool);
				const FParticleDynamicMisc* TargetDynamic = Latest.TargetStates.Read(FrameAndPhase, PropertiesPool);
				if (TargetXR && TargetVW && TargetDynamic)
				{
					for (int32 InterpFrame = 1; InterpFrame < FrameDiff; InterpFrame++)
					{
						const float Alpha = (1.0f / (float)FrameDiff) * (float)InterpFrame;

						PushStateAtFrame(Handle, FrameAndPhase.Frame + InterpFrame, Phase,
							FMath::Lerp(TargetXR->GetX(), Position, Alpha),
							FRotation3::Slerp(TargetXR->GetR(), Quaternion, Alpha),
							FMath::Lerp(TargetVW->GetV(), LinVelocity, Alpha),
							FMath::Lerp(TargetVW->GetW(), AngVelocity, Alpha),
							bShouldSleep && TargetDynamic->ObjectState() == EObjectStateType::Sleeping);
					}
				}
			}
		}
	}

	PushStateAtFrame(Handle, Frame, Phase,
		Position, Quaternion, LinVelocity, AngVelocity, bShouldSleep);
}

void FRewindData::BlockResim()
{
	if (LatestFrame > BlockResimFrame)
	{
		BlockResimFrame = LatestFrame;
	}
}

}