// Copyright Epic Games, Inc. All Rights Reserved.

#include "CharacterMovementComponentAsync.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "PhysicsProxy/SingleParticlePhysicsProxy.h"
#include "Components/PrimitiveComponent.h"
#include "PBDRigidsSolver.h"
#include "Engine/World.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CharacterMovementComponentAsync)

void FCharacterMovementComponentAsyncInput::Simulate(const float DeltaSeconds, FCharacterMovementComponentAsyncOutput& Output) const
{
//	SCOPE_CYCLE_COUNTER(STAT_CharacterMovement);


	Output.DeltaTime = DeltaSeconds;

	if (CharacterInput->LocalRole > ROLE_SimulatedProxy)
	{
		// TODO Networking
		// 
		// If we are a client we might have received an update from the server.
		const bool bIsClient = (CharacterInput->LocalRole == ROLE_AutonomousProxy && bIsNetModeClient);
		/*if (bIsClient)
		{
			//if (ClientData && ClientData->bUpdatePosition)
			{
				// Call ClientUpdatePositionAfterServerUpdate();
			}
		}*/

		// TODO don't just force down ControlledCharacterMove
		if (CharacterInput->bIsLocallyControlled) //|| (!CharacterOwner->Controller && bRunPhysicsWithNoController) || (!CharacterOwner->Controller && CharacterOwner->IsPlayingRootMotion()))
		{
			ControlledCharacterMove(DeltaSeconds, Output);
		}
		/*else if (RemoteRole == ROLE_AutonomousProxy) // TODO Networking: replicate from server
		{
			// Server ticking for remote client.
			// Between net updates from the client we need to update position if based on another object,
			// otherwise the object will move on intermediate frames and we won't follow it.
			MaybeUpdateBasedMovement(DeltaTime);
			MaybeSaveBaseLocation();

			// Smooth on listen server for local view of remote clients. We may receive updates at a rate different than our own tick rate.
			if (CharacterMovementCVars::NetEnableListenServerSmoothing && !bNetworkSmoothingComplete && IsNetMode(NM_ListenServer))
			{
				SmoothClientPosition(DeltaTime);
			}
		}*/
	}
	else if (CharacterInput->LocalRole == ROLE_SimulatedProxy)
	{
		// TODO Crouching
		// Need to recreate geometry with smaller capsule from PT.
		/*if (bShrinkProxyCapsule)
		{
			AdjustProxyCapsuleSize();
		}*/
		ensure(false);
		//SimulatedTick(DeltaSeconds);
	}

	/* TODO RVOAvoidance
	if (bUseRVOAvoidance)
	{
		UpdateDefaultAvoidance();
	}*/

	// TODO EnablePhysicsInteraction (Bumping into phys objects)
	/*if (bEnablePhysicsInteraction)
	{
		SCOPE_CYCLE_COUNTER(STAT_CharPhysicsInteraction);
		ApplyDownwardForce(DeltaTime);
		ApplyRepulsionForce(DeltaTime);
	}*/
}

void FCharacterMovementComponentAsyncInput::ControlledCharacterMove(const float DeltaSeconds, FCharacterMovementComponentAsyncOutput& Output) const
{
	{
	//	SCOPE_CYCLE_COUNTER(STAT_CharUpdateAcceleration);

		// We need to check the jump state before adjusting input acceleration, to minimize latency
		// and to make sure acceleration respects our potentially new falling state.
		CharacterInput->CheckJumpInput(DeltaSeconds, *this, Output);

		// apply input to acceleration
		Output.Acceleration = ScaleInputAcceleration(ConstrainInputAcceleration(InputVector, Output), Output);
		Output.AnalogInputModifier = ComputeAnalogInputModifier(Output.Acceleration);
	}

	// TODO Networking
	//if (LocalRole == ROLE_Authority)
	{
		PerformMovement(DeltaSeconds, Output);
	}
	/*else if (CharacterOwner->GetLocalRole() == ROLE_AutonomousProxy && IsNetMode(NM_Client))
	{
		ReplicateMoveToServer(DeltaSeconds, Acceleration);
	}*/
}

void FCharacterMovementComponentAsyncInput::PerformMovement(float DeltaSeconds, FCharacterMovementComponentAsyncOutput& Output) const
{
	EMovementMode& MovementMode = Output.MovementMode;
	FVector& LastUpdateLocation = Output.LastUpdateLocation;
	const FVector UpdatedComponentLocation = UpdatedComponentInput->GetPosition();
	bool& bForceNextFloorCheck = Output.bForceNextFloorCheck;
	const FVector& Velocity = Output.Velocity;
	const FVector& LastUpdateVelocity = Output.LastUpdateVelocity;

	// TODO RootMotion
	// no movement if we can't move, or if currently doing physical simulation on UpdatedComponent
	/*if (MovementMode == MOVE_None || UpdatedComponent->Mobility != EComponentMobility::Movable) || UpdatedComponent->IsSimulatingPhysics())
	{
		if (!CharacterOwner->bClientUpdating && !CharacterOwner->bServerMoveIgnoreRootMotion)
		{
			// Consume root motion
			if (CharacterOwner->IsPlayingRootMotion() && CharacterOwner->GetMesh())
			{
				TickCharacterPose(DeltaSeconds);
				RootMotionParams.Clear();
			}
			if (CurrentRootMotion.HasActiveRootMotionSources())
			{
				CurrentRootMotion.Clear();
			}
		}
		// Clear pending physics forces
		ClearAccumulatedForces();
		return;
	}*/

	// Force floor update if we've moved outside of CharacterMovement since last update.
	bForceNextFloorCheck |= (IsMovingOnGround(Output) && UpdatedComponentLocation != LastUpdateLocation);

	// Update saved LastPreAdditiveVelocity with any external changes to character Velocity that happened since last update.
	if (RootMotion.bHasAdditiveRootMotion)
	{
		FVector Adjustment = (Velocity - LastUpdateVelocity);
		Output.LastPreAdditiveVelocity += Adjustment;

		// TODO Debug TODO Rootmotion
/*#if ROOT_MOTION_DEBUG
		if (RootMotionSourceDebug::CVarDebugRootMotionSources.GetValueOnAnyThread() == 1)
		{
			if (!Adjustment.IsNearlyZero())
			{
				FString AdjustedDebugString = FString::Printf(TEXT("PerformMovement HasAdditiveVelocity LastUpdateVelocityAdjustment LastPreAdditiveVelocity(%s) Adjustment(%s)"),
					*CurrentRootMotion.LastPreAdditiveVelocity.ToCompactString(), *Adjustment.ToCompactString());
				RootMotionSourceDebug::PrintOnScreen(*CharacterOwner, AdjustedDebugString);
			}
		}
#endif*/
	}

	// Scoped updates can improve performance of multiple MoveComponent calls.
	{
		// TODO ScopedMovementUpdate
		//FScopedMovementUpdate ScopedMovementUpdate(UpdatedComponent, bEnableScopedMovementUpdates ? EScopedUpdate::DeferredUpdates : EScopedUpdate::ImmediateUpdates);

		MaybeUpdateBasedMovement(DeltaSeconds, Output);

		// TODO Root Motion

		// Clean up invalid RootMotion Sources.
		// This includes RootMotion sources that ended naturally.
		// They might want to perform a clamp on velocity or an override, 
		// so we want this to happen before ApplyAccumulatedForces and HandlePendingLaunch as to not clobber these.
		//const bool bHasRootMotionSources = HasRootMotionSources();
		const bool bHasRootMotionSources = Output.bWasSimulatingRootMotion;
		/*if (bHasRootMotionSources)
		{
			//SCOPE_CYCLE_COUNTER(STAT_CharacterMovementRootMotionSourceCalculate);

			const FVector VelocityBeforeCleanup = Velocity;
			CurrentRootMotion.CleanUpInvalidRootMotion(DeltaSeconds, *CharacterOwner, *this);

#if ROOT_MOTION_DEBUG
			if (RootMotionSourceDebug::CVarDebugRootMotionSources.GetValueOnGameThread() == 1)
			{
				if (Velocity != VelocityBeforeCleanup)
				{
					const FVector Adjustment = Velocity - VelocityBeforeCleanup;
					FString AdjustedDebugString = FString::Printf(TEXT("PerformMovement CleanUpInvalidRootMotion Velocity(%s) VelocityBeforeCleanup(%s) Adjustment(%s)"),
						*Velocity.ToCompactString(), *VelocityBeforeCleanup.ToCompactString(), *Adjustment.ToCompactString());
					RootMotionSourceDebug::PrintOnScreen(*CharacterOwner, AdjustedDebugString);
				}
			}
#endif
		}*/

	

		Output.OldVelocity = Velocity;
		Output.OldLocation = UpdatedComponentLocation;


		ApplyAccumulatedForces(DeltaSeconds, Output);

		// Update the character state before we do our movement
		// MOVED this to CharacterMovementComponent::FillAsyncInput
		// UpdateCharacterStateBeforeMovement(DeltaSeconds, Output);

		// TODO navwalking
		/*if (MovementMode == MOVE_NavWalking && bWantsToLeaveNavWalking)
		{
			TryToLeaveNavWalking();
		}*/

		
		// TODO Launch
		// Character::LaunchCharacter() has been deferred until now.
		//HandlePendingLaunch();
		ClearAccumulatedForces(Output);

		// TODO Debug
/*#if ROOT_MOTION_DEBUG
		if (RootMotionSourceDebug::CVarDebugRootMotionSources.GetValueOnGameThread() == 1)
		{
			if (OldVelocity != Velocity)
			{
				const FVector Adjustment = Velocity - OldVelocity;
				FString AdjustedDebugString = FString::Printf(TEXT("PerformMovement ApplyAccumulatedForces+HandlePendingLaunch Velocity(%s) OldVelocity(%s) Adjustment(%s)"),
					*Velocity.ToCompactString(), *OldVelocity.ToCompactString(), *Adjustment.ToCompactString());
				RootMotionSourceDebug::PrintOnScreen(*CharacterOwner, AdjustedDebugString);
			}
		}
#endif*/

		// Update saved LastPreAdditiveVelocity with any external changes to character Velocity that happened due to ApplyAccumulatedForces/HandlePendingLaunch
		if (RootMotion.bHasAdditiveRootMotion)
		{
			const FVector Adjustment = (Velocity - Output.OldVelocity);
			Output.LastPreAdditiveVelocity += Adjustment;

			// TODO Debug
/*#if ROOT_MOTION_DEBUG
			if (RootMotionSourceDebug::CVarDebugRootMotionSources.GetValueOnGameThread() == 1)
			{
				if (!Adjustment.IsNearlyZero())
				{
					FString AdjustedDebugString = FString::Printf(TEXT("PerformMovement HasAdditiveVelocity AccumulatedForces LastPreAdditiveVelocity(%s) Adjustment(%s)"),
						*CurrentRootMotion.LastPreAdditiveVelocity.ToCompactString(), *Adjustment.ToCompactString());
					RootMotionSourceDebug::PrintOnScreen(*CharacterOwner, AdjustedDebugString);
				}
			}
#endif*/
		}

		// TODO RootMotion
		// Prepare Root Motion (generate/accumulate from root motion sources to be used later)
		/*if (bHasRootMotionSources && !CharacterOwner->bClientUpdating && !CharacterOwner->bServerMoveIgnoreRootMotion)
		{
			// Animation root motion - If using animation RootMotion, tick animations before running physics.
			if (CharacterOwner->IsPlayingRootMotion() && CharacterOwner->GetMesh())
			{
				TickCharacterPose(DeltaSeconds);

				// Make sure animation didn't trigger an event that destroyed us
				if (!HasValidData())
				{
					return;
				}

				// For local human clients, save off root motion data so it can be used by movement networking code.
				if (CharacterOwner->IsLocallyControlled() && (CharacterOwner->GetLocalRole() == ROLE_AutonomousProxy) && CharacterOwner->IsPlayingNetworkedRootMotionMontage())
				{
					CharacterOwner->ClientRootMotionParams = RootMotionParams;
				}
			}

			// Generates root motion to be used this frame from sources other than animation
			{
				SCOPE_CYCLE_COUNTER(STAT_CharacterMovementRootMotionSourceCalculate);
				CurrentRootMotion.PrepareRootMotion(DeltaSeconds, *CharacterOwner, *this, true);
			}

			// For local human clients, save off root motion data so it can be used by movement networking code.
			if (CharacterOwner->IsLocallyControlled() && (CharacterOwner->GetLocalRole() == ROLE_AutonomousProxy))
			{
				CharacterOwner->SavedRootMotion = CurrentRootMotion;
			}
		}*/

		// Apply Root Motion to Velocity
		if (RootMotion.bHasOverrideRootMotion || RootMotion.bHasAnimRootMotion)
		{
			// Animation root motion overrides Velocity and currently doesn't allow any other root motion sources
			if (RootMotion.bHasAnimRootMotion)
			{
				// Turn root motion to velocity to be used by various physics modes.
				if (RootMotion.TimeAccumulated > 0.f)
				{
					Output.AnimRootMotionVelocity = CalcAnimRootMotionVelocity(RootMotion.AnimTransform.GetTranslation(), RootMotion.TimeAccumulated, Velocity);
					Output.Velocity = ConstrainAnimRootMotionVelocity(Output.AnimRootMotionVelocity, Output.Velocity, Output);
				}

				/*UE_LOG(LogRootMotion, Log, TEXT("PerformMovement WorldSpaceRootMotion Translation: %s, Rotation: %s, Actor Facing: %s, Velocity: %s")
					, *RootMotionParams.GetRootMotionTransform().GetTranslation().ToCompactString()
					, *RootMotionParams.GetRootMotionTransform().GetRotation().Rotator().ToCompactString()
					, *CharacterOwner->GetActorForwardVector().ToCompactString()
					, *Velocity.ToCompactString()
				);*/
			}
			else
			{
				// We don't have animation root motion so we apply other sources
				if (DeltaSeconds > 0.f)
				{
					Output.Velocity = RootMotion.OverrideVelocity;
				}
			}
		}

		// TODO Debug
/*#if ROOT_MOTION_DEBUG
		if (RootMotionSourceDebug::CVarDebugRootMotionSources.GetValueOnGameThread() == 1)
		{
			FString AdjustedDebugString = FString::Printf(TEXT("PerformMovement Velocity(%s) OldVelocity(%s)"),
				*Velocity.ToCompactString(), *OldVelocity.ToCompactString());
			RootMotionSourceDebug::PrintOnScreen(*CharacterOwner, AdjustedDebugString);
		}
#endif*/

		// NaN tracking
		//devCode(ensureMsgf(!Velocity.ContainsNaN(), TEXT("UCharacterMovementComponent::PerformMovement: Velocity contains NaN (%s)\n%s"), *GetPathNameSafe(this), *Velocity.ToString()));

		// Clear jump input now, to allow movement events to trigger it for next update.
		CharacterInput->ClearJumpInput(DeltaSeconds, *this, Output);
		Output.NumJumpApexAttempts = 0;

		// change position
		StartNewPhysics(DeltaSeconds, 0, Output);

		if (!bHasValidData)
		{
			return;
		}

		// Update character state based on change from movement
		UpdateCharacterStateAfterMovement(DeltaSeconds, Output);

		if ((bAllowPhysicsRotationDuringAnimRootMotion || !RootMotion.bHasAnimRootMotion))
		{
			PhysicsRotation(DeltaSeconds, Output);
		}

		// Apply Root Motion rotation after movement is complete.
		if (RootMotion.bHasAnimRootMotion)
		{
			const FQuat OldActorRotationQuat = UpdatedComponentInput->GetRotation();
			const FQuat RootMotionRotationQuat = RootMotion.AnimTransform.GetRotation();
			if (!RootMotionRotationQuat.IsIdentity())
			{
				const FQuat NewActorRotationQuat = RootMotionRotationQuat * OldActorRotationQuat;
				MoveUpdatedComponent(FVector::ZeroVector, NewActorRotationQuat, true, Output);
			}

			// TODO Debug
/*#if !(UE_BUILD_SHIPPING)
			// debug
			if (false)
			{
				const FRotator OldActorRotation = OldActorRotationQuat.Rotator();
				const FVector ResultingLocation = UpdatedComponent->GetComponentLocation();
				const FRotator ResultingRotation = UpdatedComponent->GetComponentRotation();

				// Show current position
				DrawDebugCoordinateSystem(MyWorld, CharacterOwner->GetMesh()->GetComponentLocation() + FVector(0, 0, 1), ResultingRotation, 50.f, false);

				// Show resulting delta move.
				DrawDebugLine(MyWorld, OldLocation, ResultingLocation, FColor::Red, false, 10.f);

				// Log details.
				UE_LOG(LogRootMotion, Warning, TEXT("PerformMovement Resulting DeltaMove Translation: %s, Rotation: %s, MovementBase: %s"), //-V595
					*(ResultingLocation - OldLocation).ToCompactString(), *(ResultingRotation - OldActorRotation).GetNormalized().ToCompactString(), *GetNameSafe(CharacterOwner->GetMovementBase()));

				const FVector RMTranslation = RootMotionParams.GetRootMotionTransform().GetTranslation();
				const FRotator RMRotation = RootMotionParams.GetRootMotionTransform().GetRotation().Rotator();
				UE_LOG(LogRootMotion, Warning, TEXT("PerformMovement Resulting DeltaError Translation: %s, Rotation: %s"),
					*(ResultingLocation - OldLocation - RMTranslation).ToCompactString(), *(ResultingRotation - OldActorRotation - RMRotation).GetNormalized().ToCompactString());
			}
#endif // !(UE_BUILD_SHIPPING)*/

		}
		else if (RootMotion.bHasOverrideRootMotion)
		{
			if (UpdatedComponentInput && !RootMotion.OverrideRotation.IsIdentity())
			{
				const FQuat OldActorRotationQuat = UpdatedComponentInput->GetRotation();
				const FQuat NewActorRotationQuat = RootMotion.OverrideRotation * OldActorRotationQuat;
				MoveUpdatedComponent(FVector::ZeroVector, NewActorRotationQuat, true, Output);
			}
		}

		// consume path following requested velocity
		Output.LastUpdateRequestedVelocity = Output.bHasRequestedVelocity ? Output.RequestedVelocity : FVector::ZeroVector;
		Output.bHasRequestedVelocity = false;

		// TODO OnMOvementUpdated
		//OnMovementUpdated(DeltaSeconds, OldLocation, OldVelocity); 
	} // End scoped movement update

	// Call external post-movement events. These happen after the scoped movement completes in case the events want to use the current state of overlaps etc.
	//CallMovementUpdateDelegate(DeltaSeconds, OldLocation, OldVelocity);
	// This is called in ApplyAsyncOutput()

	//SaveBaseLocation(); TODO MovementBase call in ApplyAsyncOutput
	//UpdateComponentVelocity(); Called in CallMOvementUpdateDelegate, in ApplyAsyncOutput

	// TODO Networking
	/*const bool bHasAuthority = CharacterOwner && CharacterOwner->HasAuthority();

	// If we move we want to avoid a long delay before replication catches up to notice this change, especially if it's throttling our rate.
	if (bHasAuthority && UNetDriver::IsAdaptiveNetUpdateFrequencyEnabled() && UpdatedComponent)
	{
		UNetDriver* NetDriver = MyWorld->GetNetDriver();
		if (NetDriver && NetDriver->IsServer())
		{
			FNetworkObjectInfo* NetActor = NetDriver->FindOrAddNetworkObjectInfo(CharacterOwner);

			if (NetActor && MyWorld->GetTimeSeconds() <= NetActor->NextUpdateTime && NetDriver->IsNetworkActorUpdateFrequencyThrottled(*NetActor))
			{
				if (ShouldCancelAdaptiveReplication())
				{
					NetDriver->CancelAdaptiveReplication(*NetActor);
				}
			}
		}
	}*/

	const FVector NewLocation = UpdatedComponentInput->GetPosition();//UpdatedComponent ? UpdatedComponent->GetComponentLocation() : FVector::ZeroVector;
	const FQuat NewRotation = UpdatedComponentInput->GetRotation();//UpdatedComponent ? UpdatedComponent->GetComponentQuat() : FQuat::Identity;

	// TODO Networking
	/*if (bHasAuthority && UpdatedComponent && !IsNetMode(NM_Client))
	{
		const bool bLocationChanged = (NewLocation != LastUpdateLocation);
		const bool bRotationChanged = (NewRotation != LastUpdateRotation);
		if (bLocationChanged || bRotationChanged)
		{
			// Update ServerLastTransformUpdateTimeStamp. This is used by Linear smoothing on clients to interpolate positions with the correct delta time,
			// so the timestamp should be based on the client's move delta (ServerAccumulatedClientTimeStamp), not the server time when receiving the RPC.
			const bool bIsRemotePlayer = (CharacterOwner->GetRemoteRole() == ROLE_AutonomousProxy);
			const FNetworkPredictionData_Server_Character* ServerData = bIsRemotePlayer ? GetPredictionData_Server_Character() : nullptr;
			if (bIsRemotePlayer && ServerData && CharacterMovementCVars::NetUseClientTimestampForReplicatedTransform)
			{
				ServerLastTransformUpdateTimeStamp = float(ServerData->ServerAccumulatedClientTimeStamp);
			}
			else
			{
				ServerLastTransformUpdateTimeStamp = MyWorld->GetTimeSeconds();
			}
		}
	}*/

	Output.LastUpdateLocation = NewLocation;
	Output.LastUpdateRotation = NewRotation;
	Output.LastUpdateVelocity = Velocity;
}

void FCharacterMovementComponentAsyncInput::MaybeUpdateBasedMovement(float DeltaSeconds, FCharacterMovementComponentAsyncOutput& Output) const
{
	bool& bDeferUpdateBasedMovement = Output.bDeferUpdateBasedMovement;

	MovementBaseAsyncData.Validate(Output);
	const bool& bMovementBaseUsesRelativeLocation = MovementBaseAsyncData.bMovementBaseUsesRelativeLocationCached;
	const bool& bMovementBaseIsSimulated = MovementBaseAsyncData.bMovementBaseIsSimulatedCached;

	bDeferUpdateBasedMovement = false;

	// TODO MovementBase

	if (bMovementBaseUsesRelativeLocation) 
	{
		// Need to see if anything we're on is simulating physics or has a parent that is.		
		if (bMovementBaseIsSimulated == false)
		{
			bDeferUpdateBasedMovement = false;

			UpdateBasedMovement(DeltaSeconds, Output);

			Output.bShouldDisablePostPhysicsTick = true;
			Output.bShouldAddMovementBaseTickDependency = true;
			// If previously simulated, go back to using normal tick dependencies.
			/*if (PostPhysicsTickFunction.IsTickFunctionEnabled())
			{
				PostPhysicsTickFunction.SetTickFunctionEnable(false);
				MovementBaseUtility::AddTickDependency(PrimaryComponentTick, MovementBase);
			}*/
		}
		else
		{
			// defer movement base update until after physics
			bDeferUpdateBasedMovement = true;
			Output.bShouldEnablePostPhysicsTick = true;
			Output.bShouldRemoveMovementBaseTickDependency = true;
			// If previously not simulating, remove tick dependencies and use post physics tick function.
			/*if (!PostPhysicsTickFunction.IsTickFunctionEnabled())
			{
				PostPhysicsTickFunction.SetTickFunctionEnable(true);
				MovementBaseUtility::RemoveTickDependency(PrimaryComponentTick, MovementBase);
			}*/
		}
	}
	else
	{
		Output.bShouldEnablePostPhysicsTick = true;
		// Remove any previous physics tick dependencies. SetBase() takes care of the other dependencies.
		/*if (PostPhysicsTickFunction.IsTickFunctionEnabled())
		{
			PostPhysicsTickFunction.SetTickFunctionEnable(false);
		}*/
	}
}

void FCharacterMovementComponentAsyncInput::UpdateBasedMovement(float DeltaSeconds, FCharacterMovementComponentAsyncOutput& Output) const
{
/*	if (!HasValidData())
	{
		return;
	}*/

	MovementBaseAsyncData.Validate(Output);
	const bool& bMovementBaseUsesRelativeLocation = MovementBaseAsyncData.bMovementBaseUsesRelativeLocationCached;
	const bool& bIsMovementBaseValid = MovementBaseAsyncData.bMovementBaseIsValidCached;
	const bool& bIsMovementBaseOwnerValid = MovementBaseAsyncData.bMovementBaseOwnerIsValidCached;
	EMoveComponentFlags& MoveComponentFlags = Output.MoveComponentFlags;
	const FQuat& OldBaseQuat = MovementBaseAsyncData.OldBaseQuat;
	const FVector& OldBaseLocation = MovementBaseAsyncData.OldBaseLocation;
	

	if (bMovementBaseUsesRelativeLocation == false)
	{
		return;
	}

	if (!bIsMovementBaseValid || !bIsMovementBaseOwnerValid)
	{
		Output.NewMovementBase = nullptr; //SetBase(NULL); 
		Output.NewMovementBaseOwner = nullptr;
		return;
	}

	// Ignore collision with bases during these movements.
	TGuardValue<EMoveComponentFlags> ScopedFlagRestore(MoveComponentFlags, MoveComponentFlags | MOVECOMP_IgnoreBases);

	Output.DeltaQuat = FQuat::Identity;
	Output.DeltaPosition = FVector::ZeroVector;

	if (!MovementBaseAsyncData.bIsBaseTransformValid)
	{
		return;
	}
	FQuat NewBaseQuat = MovementBaseAsyncData.BaseQuat;
	FVector NewBaseLocation = MovementBaseAsyncData.BaseLocation;


	// Find change in rotation
	const bool bRotationChanged = !OldBaseQuat.Equals(NewBaseQuat, 1e-8f);
	if (bRotationChanged)
	{
		Output.DeltaQuat = NewBaseQuat * OldBaseQuat.Inverse();
	}

	// only if base moved
	if (bRotationChanged || (OldBaseLocation != NewBaseLocation))
	{
		// Calculate new transform matrix of base actor (ignoring scale).
		const FQuatRotationTranslationMatrix OldLocalToWorld(OldBaseQuat, OldBaseLocation);
		const FQuatRotationTranslationMatrix NewLocalToWorld(NewBaseQuat, NewBaseLocation);

		FQuat FinalQuat = UpdatedComponentInput->GetRotation();

		if (bRotationChanged && !bIgnoreBaseRotation)
		{
			// Apply change in rotation and pipe through FaceRotation to maintain axis restrictions
			const FQuat PawnOldQuat = UpdatedComponentInput->GetRotation();
			const FQuat TargetQuat = Output.DeltaQuat * FinalQuat;
			FRotator TargetRotator(TargetQuat);

			// Do we need this value after all?

			CharacterInput->FaceRotation(TargetRotator, 0.0f, *this, Output);
			FinalQuat =  Output.CharacterOutput->Rotation.Quaternion();//UpdatedComponent->GetComponentQuat(); supposed to be modified by MockFaceRotation, si this ok?


			if (PawnOldQuat.Equals(FinalQuat, 1e-6f))
			{
				// Nothing changed. This means we probably are using another rotation mechanism (bOrientToMovement etc). We should still follow the base object.
				// @todo: This assumes only Yaw is used, currently a valid assumption. This is the only reason FaceRotation() is used above really, aside from being a virtual hook.
				if (bOrientRotationToMovement || (bUseControllerDesiredRotation /*&& CharacterOwner->Controller*/))
				{
					TargetRotator.Pitch = 0.f;
					TargetRotator.Roll = 0.f;

					MoveUpdatedComponent(FVector::ZeroVector, FQuat(TargetRotator), /*bSweep=*/false, Output);
					//FinalQuat = UpdatedComponent->GetComponentQuat();
					FinalQuat = UpdatedComponentInput->GetRotation();
				}
			}

			// TODO Camera
			// Pipe through ControlRotation, to affect camera.
			/*if (CharacterOwner->Controller)
			{
				const FQuat PawnDeltaRotation = FinalQuat * PawnOldQuat.Inverse();
				FRotator FinalRotation = FinalQuat.Rotator();
				UpdateBasedRotation(FinalRotation, PawnDeltaRotation.Rotator());
				FinalQuat = UpdatedComponent->GetComponentQuat();
			}*/
		}

		// We need to offset the base of the character here, not its origin, so offset by half height
		float HalfHeight = Output.ScaledCapsuleHalfHeight;
		float Radius = Output.ScaledCapsuleRadius;

		FVector const BaseOffset(0.0f, 0.0f, HalfHeight);
		FVector const LocalBasePos = OldLocalToWorld.InverseTransformPosition(UpdatedComponentInput->GetPosition() - BaseOffset);
		FVector const NewWorldPos = ConstrainLocationToPlane(NewLocalToWorld.TransformPosition(LocalBasePos) + BaseOffset);
		Output.DeltaPosition = ConstrainDirectionToPlane(NewWorldPos - UpdatedComponentInput->GetPosition());

		// move attached actor
		if (false)//bFastAttachedMove) // TODO bFastAttachedMove
		{
			// we're trusting no other obstacle can prevent the move here
			//	UpdatedComponent->SetWorldLocationAndRotation(NewWorldPos, FinalQuat, false);

			UpdatedComponentInput->SetPosition(NewWorldPos);
			UpdatedComponentInput->SetRotation(FinalQuat);
		}
		else
		{
			// hack - transforms between local and world space introducing slight error FIXMESTEVE - discuss with engine team: just skip the transforms if no rotation?
			FVector BaseMoveDelta = NewBaseLocation - OldBaseLocation;
			if (!bRotationChanged && (BaseMoveDelta.X == 0.f) && (BaseMoveDelta.Y == 0.f))
			{
				Output.DeltaPosition.X = 0.f;
				Output.DeltaPosition.Y = 0.f;
			}

			FHitResult MoveOnBaseHit(1.f);
			const FVector OldLocation = UpdatedComponentInput->GetPosition();
			MoveUpdatedComponent(Output.DeltaPosition, FinalQuat, true, Output, &MoveOnBaseHit);
			if ((UpdatedComponentInput->GetPosition() - (OldLocation + Output.DeltaPosition)).IsNearlyZero() == false)
			{
				// TODO OnUnableToFollowBaseMove
				//OnUnableToFollowBaseMove(DeltaPosition, OldLocation, MoveOnBaseHit);
			}
		}

		MovementBaseAsyncData.Validate(Output); // ensure we haven't changed movement base
		if (MovementBaseAsyncData.bMovementBaseIsSimulatedCached /*MovementBase->IsSimulatingPhysics() && CharacterOwner->GetMesh()*/)
		{
			//CharacterOwner->GetMesh()->ApplyDeltaToAllPhysicsTransforms(DeltaPosition, DeltaQuat);

			// If we hit this multiple times, our DeltaPostion/DeltaQuat is being stomped. Do we need to call for each, or just latest?
			ensure(Output.bShouldApplyDeltaToMeshPhysicsTransforms == false);

			Output.bShouldApplyDeltaToMeshPhysicsTransforms = true;
		}
	}
}

void FCharacterMovementComponentAsyncInput::StartNewPhysics(float deltaTime, int32 Iterations, FCharacterMovementComponentAsyncOutput& Output) const
{
	if ((deltaTime < UCharacterMovementComponent::MIN_TICK_TIME) || (Iterations >= MaxSimulationIterations) || !bHasValidData)
	{
		return;
	}

	if (UpdatedComponentInput->bIsSimulatingPhysics)
	{
		//UE_LOG(LogCharacterMovement, Log, TEXT("UCharacterMovementComponent::StartNewPhysics: UpdateComponent (%s) is simulating physics - aborting."), UpdatedComponent->GetPathName());
		return;
	}

	const bool bSavedMovementInProgress = Output.bMovementInProgress;
	Output.bMovementInProgress = true;

	// TODO Other Movement Modes
	switch (Output.MovementMode)
	{
	case MOVE_None:
		break;
	case MOVE_Walking:
		PhysWalking(deltaTime, Iterations, Output);
		break;
	// TODO Nav walking
	/*case MOVE_NavWalking:
		PhysNavWalking(deltaTime, Iterations);
		break;*/
	case MOVE_Falling:
		PhysFalling(deltaTime, Iterations, Output);
		break;
		/*
	case MOVE_Flying:
		PhysFlying(deltaTime, Iterations);
		break;
	case MOVE_Swimming:
		PhysSwimming(deltaTime, Iterations);
		break;
	case MOVE_Custom:
		PhysCustom(deltaTime, Iterations);
		break;
		*/
	default:
		//UE_LOG(LogCharacterMovement, Warning, TEXT("Async Character Movement called with unsupported movement mode %d"), int32(Output.MovementMode));
		SetMovementMode(MOVE_None, Output);
		break;
	}

	Output.bMovementInProgress = bSavedMovementInProgress;
	if (bDeferUpdateMoveComponent)
	{
		ensure(false); // TODO
	//	SetUpdatedComponent(DeferredUpdatedMoveComponent);
	}
}

void FCharacterMovementComponentAsyncInput::PhysWalking(float deltaTime, int32 Iterations, FCharacterMovementComponentAsyncOutput& Output) const
{
	//SCOPE_CYCLE_COUNTER(STAT_CharPhysWalking);

	const FCharacterMovementComponentAsyncInput& Input = *this; // TODO Refactor

	if (deltaTime < UCharacterMovementComponent::MIN_TICK_TIME)
	{
		return;
	}

	FVector& Velocity = Output.Velocity;
	FVector& Acceleration = Output.Acceleration;

	// TODO Fix? We ensure when filling inputs that we have controller and owner, os this can't be hit atm.
	if (false)//!CharacterOwner || (!CharacterOwner->Controller && !bRunPhysicsWithNoController && !Output.RootMotionParams.bHasRootMotion && !Output.CurrentRootMotion.HasOverrideVelocity() && (LocalRole != ROLE_SimulatedProxy)))
	{
		Acceleration = FVector::ZeroVector;
		Velocity = FVector::ZeroVector;
		return;
	}

	if (!UpdatedComponentInput->bIsQueryCollisionEnabled)
	{
		SetMovementMode(MOVE_Walking, Output);
		return;
	}

	// TODO Debug
	//devCode(ensureMsgf(!Velocity.ContainsNaN(), TEXT("PhysWalking: Velocity contains NaN before Iteration (%s)\n%s"), *GetPathNameSafe(this), *Velocity.ToString()));

	Output.bJustTeleported = false;
	bool bCheckedFall = false;
	bool bTriedLedgeMove = false;
	float remainingTime = deltaTime;

	// Perform the move
	while ((remainingTime >= UCharacterMovementComponent::MIN_TICK_TIME) && (Iterations < MaxSimulationIterations) /*&& CharacterOwner*/ && (/*CharacterOwner->Controller ||*/ bRunPhysicsWithNoController
		|| RootMotion.bHasAnimRootMotion || RootMotion.bHasOverrideRootMotion || (true/*LocalRole == ROLE_SimulatedProxy TODO NetRole*/)))
	{
		Iterations++;
		Output.bJustTeleported = false;
		const float timeTick = GetSimulationTimeStep(remainingTime, Iterations);
		remainingTime -= timeTick;

		// Save current values
		MovementBaseAsyncData.Validate(Output); // ensure haven't cahnged movement base
		UPrimitiveComponent* const OldBase = MovementBaseAsyncData.CachedMovementBase;//GetMovementBase();
		const FVector PreviousBaseLocation = MovementBaseAsyncData.BaseLocation;//(OldBase != NULL) ? OldBase->GetComponentLocation() : FVector::ZeroVector;
		const FVector OldLocation = UpdatedComponentInput->GetPosition();// UpdatedComponent->GetComponentLocation
		const FFindFloorResult OldFloor = Output.CurrentFloor;

		RestorePreAdditiveRootMotionVelocity(Output);

		// Ensure velocity is horizontal.
		MaintainHorizontalGroundVelocity(Output);
		const FVector OldVelocity = Velocity;
		Acceleration.Z = 0.f;

		// Apply acceleration
		if (!RootMotion.bHasAnimRootMotion && !RootMotion.bHasOverrideRootMotion)
		{
			CalcVelocity(timeTick, GroundFriction, false, GetMaxBrakingDeceleration(Output), Output);
			//devCode(ensureMsgf(!Velocity.ContainsNaN(), TEXT("PhysWalking: Velocity contains NaN after CalcVelocity (%s)\n%s"), *GetPathNameSafe(this), *Velocity.ToString()));
		}

		ApplyRootMotionToVelocity(timeTick, Output);
		//devCode(ensureMsgf(!Velocity.ContainsNaN(), TEXT("PhysWalking: Velocity contains NaN after Root Motion application (%s)\n%s"), *GetPathNameSafe(this), *Velocity.ToString()));

		if (IsFalling(Output))
		{
			// Root motion could have put us into Falling.
			// No movement has taken place this movement tick so we pass on full time/past iteration count
			StartNewPhysics(remainingTime + timeTick, Iterations - 1, Output);
			return;
		}

		// Compute move parameters
		const FVector MoveVelocity = Velocity;
		const FVector Delta = timeTick * MoveVelocity;
		const bool bZeroDelta = Delta.IsNearlyZero();
		FStepDownResult StepDownResult;

		/*if (GEngine)
		{
			FString DebugMsg = FString::Printf(TEXT("Vel: %s"), *Output.Velocity.ToString());
			GEngine->AddOnScreenDebugMessage(198907, 1.f, FColor::Blue, DebugMsg);
		}*/

		if (bZeroDelta)
		{
			remainingTime = 0.f;
		}
		else
		{
			// try to move forward
			MoveAlongFloor(MoveVelocity, timeTick, &StepDownResult, Output);

			if (IsFalling(Output))
			{
				// pawn decided to jump up
				const float DesiredDist = Delta.Size();
				if (DesiredDist > UE_KINDA_SMALL_NUMBER)
				{
					const float ActualDist = (UpdatedComponentInput->GetPosition() - OldLocation).Size2D();
					remainingTime += timeTick * (1.f - FMath::Min(1.f, ActualDist / DesiredDist));
				}
				StartNewPhysics(remainingTime, Iterations, Output);
				return;
			}
			// TODO Other Movement Modes
			/*else if (IsSwimming(Output)) //just entered water
			{
				StartSwimming(OldLocation, OldVelocity, timeTick, remainingTime, Iterations);
				return;
			}*/
		}

		// Update floor.
		// StepUp might have already done it for us.
		if (StepDownResult.bComputedFloor)
		{
			Output.CurrentFloor = StepDownResult.FloorResult;
		}
		else
		{
			FindFloor(UpdatedComponentInput->GetPosition(), Output.CurrentFloor, bZeroDelta, Output);
		}

		// check for ledges here
		const bool bCheckLedges = !CanWalkOffLedges(Output);
		if (bCheckLedges && !Output.CurrentFloor.IsWalkableFloor())
		{
			// calculate possible alternate movement
			const FVector GravDir = FVector(0.f, 0.f, -1.f);
			const FVector NewDelta = bTriedLedgeMove ? FVector::ZeroVector : GetLedgeMove(OldLocation, Delta, GravDir, Output);
			if (!NewDelta.IsZero())
			{
				// first revert this move
				RevertMove(OldLocation, OldBase, PreviousBaseLocation, OldFloor, false, Output);

				// avoid repeated ledge moves if the first one fails
				bTriedLedgeMove = true;

				// Try new movement direction
				Velocity = NewDelta / timeTick;
				remainingTime += timeTick;
				continue;
			}
			else
			{
				ensure(false); // TODO MovementBase
				// see if it is OK to jump
				// @todo collision : only thing that can be problem is that oldbase has world collision on
				/*bool bMustJump = bZeroDelta || (OldBase == NULL || (!OldBase->IsQueryCollisionEnabled() && MovementBaseUtility::IsDynamicBase(OldBase)));
				if ((bMustJump || !bCheckedFall) && CheckFall(OldFloor, CurrentFloor.HitResult, Delta, OldLocation, remainingTime, timeTick, Iterations, bMustJump))
				{
					return;
				}
				bCheckedFall = true;

				// revert this move
				RevertMove(OldLocation, OldBase, PreviousBaseLocation, OldFloor, true);
				remainingTime = 0.f;*/
				break;
			}
		}
		else
		{
			// Validate the floor check
			if (Output.CurrentFloor.IsWalkableFloor())
			{
				if (ShouldCatchAir(OldFloor, Output.CurrentFloor))
				{
					HandleWalkingOffLedge(OldFloor.HitResult.ImpactNormal, OldFloor.HitResult.Normal, OldLocation, timeTick);
					if (IsMovingOnGround(Output))
					{
						// If still walking, then fall. If not, assume the user set a different mode they want to keep.
						StartFalling(Iterations, remainingTime, timeTick, Delta, OldLocation, Output);
					}
					return;
				}

				AdjustFloorHeight(Output);

				//SetBase(CurrentFloor.HitResult.Component.Get(), CurrentFloor.HitResult.BoneName);
				Output.NewMovementBase = Output.CurrentFloor.HitResult.Component.Get();
				Output.NewMovementBaseOwner = Output.CurrentFloor.HitResult.GetActor();
			}
			else if (Output.CurrentFloor.HitResult.bStartPenetrating && remainingTime <= 0.f)
			{
				// The floor check failed because it started in penetration
				// We do not want to try to move downward because the downward sweep failed, rather we'd like to try to pop out of the floor.
				FHitResult Hit(Output.CurrentFloor.HitResult);
				Hit.TraceEnd = Hit.TraceStart + FVector(0.f, 0.f, UCharacterMovementComponent::MAX_FLOOR_DIST);
				const FVector RequestedAdjustment = GetPenetrationAdjustment(Hit);
				ResolvePenetration(RequestedAdjustment, Hit, UpdatedComponentInput->GetRotation(), Output);
				Output.bForceNextFloorCheck = true;
			}

			// check if just entered water
			// TODO Other movement modes
			/*if (IsSwimming())
			{
				StartSwimming(OldLocation, Velocity, timeTick, remainingTime, Iterations);
				return;
			}*/

			// See if we need to start falling.
			if (!Output.CurrentFloor.IsWalkableFloor() && !Output.CurrentFloor.HitResult.bStartPenetrating)
			{
				// TODO MovementBase
				const bool bMustJump = Output.bJustTeleported || bZeroDelta;// || (OldBase == NULL || (!OldBase->IsQueryCollisionEnabled() && MovementBaseUtility::IsDynamicBase(OldBase)));
				if ((bMustJump || !bCheckedFall) && CheckFall(OldFloor, Output.CurrentFloor.HitResult, Delta, OldLocation, remainingTime, timeTick, Iterations, bMustJump, Output))
				{
					return;
				}
				bCheckedFall = true;
			}
		}

		// Allow overlap events and such to change physics state and velocity
		if (IsMovingOnGround(Output))
		{
			// Make velocity reflect actual move
			if (!Output.bJustTeleported && !RootMotion.bHasAnimRootMotion && !RootMotion.bHasOverrideRootMotion && timeTick >= UCharacterMovementComponent::MIN_TICK_TIME)
			{
				// TODO-RootMotionSource: Allow this to happen during partial override Velocity, but only set allowed axes?
				Velocity = (UpdatedComponentInput->GetPosition() - OldLocation) / timeTick;
				MaintainHorizontalGroundVelocity(Output);
			}
		}

		// If we didn't move at all this iteration then abort (since future iterations will also be stuck).
		if (UpdatedComponentInput->GetPosition() == OldLocation)
		{
			remainingTime = 0.f;
			break;
		}
	}

	if (IsMovingOnGround(Output))
	{
		MaintainHorizontalGroundVelocity(Output);
	}
}

void FCharacterMovementComponentAsyncInput::PhysFalling(float deltaTime, int32 Iterations, FCharacterMovementComponentAsyncOutput& Output) const
{
//	SCOPE_CYCLE_COUNTER(STAT_CharPhysFalling);

	const float MIN_TICK_TIME = UCharacterMovementComponent::MIN_TICK_TIME;

	if (deltaTime < MIN_TICK_TIME)
	{
		return;
	}

	FVector& Velocity = Output.Velocity;

	FVector FallAcceleration = GetFallingLateralAcceleration(deltaTime, Output);
	FallAcceleration.Z = 0.f;
	const bool bHasLimitedAirControl = ShouldLimitAirControl(deltaTime, FallAcceleration, Output);

	float remainingTime = deltaTime;
	while ((remainingTime >= MIN_TICK_TIME) && (Iterations < MaxSimulationIterations))
	{
		Iterations++;
		float timeTick = GetSimulationTimeStep(remainingTime, Iterations);
		remainingTime -= timeTick;

		const FVector OldLocation = UpdatedComponentInput->GetPosition();
		const FQuat PawnRotation = UpdatedComponentInput->GetRotation();
		Output.bJustTeleported = false;

		RestorePreAdditiveRootMotionVelocity(Output);

		const FVector OldVelocity = Velocity;

		// Apply input
		const float MaxDecel = GetMaxBrakingDeceleration(Output);
		if (!RootMotion.bHasAnimRootMotion && !RootMotion.bHasOverrideRootMotion)
		{
			// Compute Velocity
			{
				// Acceleration = FallAcceleration for CalcVelocity(), but we restore it after using it.
				TGuardValue<FVector> RestoreAcceleration(Output.Acceleration, FallAcceleration);
				Velocity.Z = 0.f;
				CalcVelocity(timeTick, FallingLateralFriction, false, MaxDecel, Output);
				Velocity.Z = OldVelocity.Z;
			}
		}

		// Compute current gravity
		const FVector Gravity(0.f, 0.f, GravityZ);
		float GravityTime = timeTick;

		// If jump is providing force, gravity may be affected.
		bool bEndingJumpForce = false;
		if (Output.CharacterOutput->JumpForceTimeRemaining > 0.0f)
		{
			// Consume some of the force time. Only the remaining time (if any) is affected by gravity when bApplyGravityWhileJumping=false.
			const float JumpForceTime = FMath::Min(Output.CharacterOutput->JumpForceTimeRemaining, timeTick);
			GravityTime = bApplyGravityWhileJumping ? timeTick : FMath::Max(0.0f, timeTick - JumpForceTime);

			// Update Character state
			Output.CharacterOutput->JumpForceTimeRemaining -= JumpForceTime;
			if (Output.CharacterOutput->JumpForceTimeRemaining <= 0.0f)
			{
				CharacterInput->ResetJumpState(*this, Output);
				bEndingJumpForce = true;
			}
		}

		// Apply gravity
		Velocity = NewFallVelocity(Velocity, Gravity, GravityTime, Output);

		// See if we need to sub-step to exactly reach the apex. This is important for avoiding "cutting off the top" of the trajectory as framerate varies.
		if (CharacterMovementCVars::ForceJumpPeakSubstep && OldVelocity.Z > 0.f && Velocity.Z <= 0.f && Output.NumJumpApexAttempts < MaxJumpApexAttemptsPerSimulation)
		{
			const FVector DerivedAccel = (Velocity - OldVelocity) / timeTick;
			if (!FMath::IsNearlyZero(DerivedAccel.Z))
			{
				const float TimeToApex = -OldVelocity.Z / DerivedAccel.Z;

				// The time-to-apex calculation should be precise, and we want to avoid adding a substep when we are basically already at the apex from the previous iteration's work.
				const float ApexTimeMinimum = 0.0001f;
				if (TimeToApex >= ApexTimeMinimum && TimeToApex < timeTick)
				{
					const FVector ApexVelocity = OldVelocity + DerivedAccel * TimeToApex;
					Velocity = ApexVelocity;
					Velocity.Z = 0.f; // Should be nearly zero anyway, but this makes apex notifications consistent.

					// We only want to move the amount of time it takes to reach the apex, and refund the unused time for next iteration.
					remainingTime += (timeTick - TimeToApex);
					timeTick = TimeToApex;
					Iterations--;
					Output.NumJumpApexAttempts++;
				}
			}
		}

		// TODO Rootmotion
		//UE_LOG(LogCharacterMovement, Log, TEXT("dt=(%.6f) OldLocation=(%s) OldVelocity=(%s) NewVelocity=(%s)"), timeTick, *(UpdatedComponent->GetComponentLocation()).ToString(), *OldVelocity.ToString(), *Velocity.ToString());
		//ApplyRootMotionToVelocity(timeTick);

		// TODO NotifyJumpApex
		/*if (bNotifyApex && (Velocity.Z < 0.f))
		{
			// Just passed jump apex since now going down
			bNotifyApex = false;
			NotifyJumpApex();
		}*/

		// Compute change in position (using midpoint integration method).
		FVector Adjusted = 0.5f * (OldVelocity + Velocity) * timeTick;

		// Special handling if ending the jump force where we didn't apply gravity during the jump.
		if (bEndingJumpForce && !bApplyGravityWhileJumping)
		{
			// We had a portion of the time at constant speed then a portion with acceleration due to gravity.
			// Account for that here with a more correct change in position.
			const float NonGravityTime = FMath::Max(0.f, timeTick - GravityTime);
			Adjusted = (OldVelocity * NonGravityTime) + (0.5f * (OldVelocity + Velocity) * GravityTime);
		}

		// Move
		FHitResult Hit(1.f);
		SafeMoveUpdatedComponent(Adjusted, PawnRotation, true, Hit, Output);

		if (!bHasValidData)
		{
			return;
		}

		float LastMoveTimeSlice = timeTick;
		float subTimeTickRemaining = timeTick * (1.f - Hit.Time);

		// TODO Other movement modes
		if (false)//IsSwimming()) //just entered water
		{
			/*remainingTime += subTimeTickRemaining;
			StartSwimming(OldLocation, OldVelocity, timeTick, remainingTime, Iterations);
			return;*/
		}
		else if (Hit.bBlockingHit)
		{
			if (IsValidLandingSpot(UpdatedComponentInput->GetPosition(), Hit, Output))
			{
				remainingTime += subTimeTickRemaining;
				ProcessLanded(Hit, remainingTime, Iterations, Output);
				return;
			}
			else
			{
				// Compute impact deflection based on final velocity, not integration step.
				// This allows us to compute a new velocity from the deflected vector, and ensures the full gravity effect is included in the slide result.
				Adjusted = Velocity * timeTick;

				// See if we can convert a normally invalid landing spot (based on the hit result) to a usable one.
				if (!Hit.bStartPenetrating && ShouldCheckForValidLandingSpot(timeTick, Adjusted, Hit, Output))
				{
					const FVector PawnLocation = UpdatedComponentInput->GetPosition();
					FFindFloorResult FloorResult;
					FindFloor(PawnLocation, FloorResult, false, Output);
					if (FloorResult.IsWalkableFloor() && IsValidLandingSpot(PawnLocation, FloorResult.HitResult, Output))
					{
						remainingTime += subTimeTickRemaining;
						ProcessLanded(FloorResult.HitResult, remainingTime, Iterations, Output);
						return;
					}
				}

				HandleImpact(Hit, Output, LastMoveTimeSlice, Adjusted);

				// If we've changed physics mode, abort.
				if (!bHasValidData || !IsFalling(Output))
				{
					return;
				}

				// Limit air control based on what we hit.
				// We moved to the impact point using air control, but may want to deflect from there based on a limited air control acceleration.
				FVector VelocityNoAirControl = OldVelocity;
				FVector AirControlAccel = Output.Acceleration;
				if (bHasLimitedAirControl)
				{
					// Compute VelocityNoAirControl
					{
						// Find velocity *without* acceleration.
						TGuardValue<FVector> RestoreAcceleration(Output.Acceleration, FVector::ZeroVector);
						TGuardValue<FVector> RestoreVelocity(Velocity, OldVelocity);
						Velocity.Z = 0.f;
						CalcVelocity(timeTick, FallingLateralFriction, false, MaxDecel, Output);
						VelocityNoAirControl = FVector(Velocity.X, Velocity.Y, OldVelocity.Z);
						VelocityNoAirControl = NewFallVelocity(VelocityNoAirControl, Gravity, GravityTime, Output);
					}

					const bool bCheckLandingSpot = false; // we already checked above.
					AirControlAccel = (Velocity - VelocityNoAirControl) / timeTick;
					const FVector AirControlDeltaV = LimitAirControl(LastMoveTimeSlice, AirControlAccel, Hit, bCheckLandingSpot, Output) * LastMoveTimeSlice;
					Adjusted = (VelocityNoAirControl + AirControlDeltaV) * LastMoveTimeSlice;
				}

				const FVector OldHitNormal = Hit.Normal;
				const FVector OldHitImpactNormal = Hit.ImpactNormal;
				FVector Delta = ComputeSlideVector(Adjusted, 1.f - Hit.Time, OldHitNormal, Hit, Output);

				// Compute velocity after deflection (only gravity component for RootMotion)
				if (subTimeTickRemaining > UE_KINDA_SMALL_NUMBER && !Output.bJustTeleported)
				{
					const FVector NewVelocity = (Delta / subTimeTickRemaining);
					Velocity = RootMotion.bHasAnimRootMotion || RootMotion.bHasOverrideWithIgnoreZAccumulate ? FVector(Velocity.X, Velocity.Y, NewVelocity.Z) : NewVelocity;
				}

				if (subTimeTickRemaining > UE_KINDA_SMALL_NUMBER && (Delta | Adjusted) > 0.f)
				{
					// Move in deflected direction.
					SafeMoveUpdatedComponent(Delta, PawnRotation, true, Hit, Output);

					if (Hit.bBlockingHit)
					{
						// hit second wall
						LastMoveTimeSlice = subTimeTickRemaining;
						subTimeTickRemaining = subTimeTickRemaining * (1.f - Hit.Time);

						if (IsValidLandingSpot(UpdatedComponentInput->GetPosition(), Hit, Output))
						{
							remainingTime += subTimeTickRemaining;
							ProcessLanded(Hit, remainingTime, Iterations, Output);
							return;
						}

						HandleImpact(Hit, Output, LastMoveTimeSlice, Delta);

						// If we've changed physics mode, abort.
						if (!bHasValidData || !IsFalling(Output))
						{
							return;
						}

						// Act as if there was no air control on the last move when computing new deflection.
						if (bHasLimitedAirControl && Hit.Normal.Z > CharacterMovementConstants::VERTICAL_SLOPE_NORMAL_Z)
						{
							const FVector LastMoveNoAirControl = VelocityNoAirControl * LastMoveTimeSlice;
							Delta = ComputeSlideVector(LastMoveNoAirControl, 1.f, OldHitNormal, Hit, Output);
						}

						FVector PreTwoWallDelta = Delta;
						TwoWallAdjust(Delta, Hit, OldHitNormal, Output);

						// Limit air control, but allow a slide along the second wall.
						if (bHasLimitedAirControl)
						{
							const bool bCheckLandingSpot = false; // we already checked above.
							const FVector AirControlDeltaV = LimitAirControl(subTimeTickRemaining, AirControlAccel, Hit, bCheckLandingSpot, Output) * subTimeTickRemaining;

							// Only allow if not back in to first wall
							if (FVector::DotProduct(AirControlDeltaV, OldHitNormal) > 0.f)
							{
								Delta += (AirControlDeltaV * subTimeTickRemaining);
							}
						}

						// Compute velocity after deflection (only gravity component for RootMotion)
						if (subTimeTickRemaining > UE_KINDA_SMALL_NUMBER && !Output.bJustTeleported)
						{
							const FVector NewVelocity = (Delta / subTimeTickRemaining);
							Velocity = RootMotion.bHasAnimRootMotion || RootMotion.bHasOverrideWithIgnoreZAccumulate ? FVector(Velocity.X, Velocity.Y, NewVelocity.Z) : NewVelocity;
						}

						// bDitch=true means that pawn is straddling two slopes, neither of which it can stand on
						bool bDitch = ((OldHitImpactNormal.Z > 0.f) && (Hit.ImpactNormal.Z > 0.f) && (FMath::Abs(Delta.Z) <= UE_KINDA_SMALL_NUMBER) && ((Hit.ImpactNormal | OldHitImpactNormal) < 0.f));
						SafeMoveUpdatedComponent(Delta, PawnRotation, true, Hit, Output);
						if (Hit.Time == 0.f)
						{
							// if we are stuck then try to side step
							FVector SideDelta = (OldHitNormal + Hit.ImpactNormal).GetSafeNormal2D();
							if (SideDelta.IsNearlyZero())
							{
								SideDelta = FVector(OldHitNormal.Y, -OldHitNormal.X, 0).GetSafeNormal();
							}
							SafeMoveUpdatedComponent(SideDelta, PawnRotation, true, Hit, Output);
						}

						if (bDitch || IsValidLandingSpot(UpdatedComponentInput->GetPosition(), Hit, Output) || Hit.Time == 0.f)
						{
							remainingTime = 0.f;
							ProcessLanded(Hit, remainingTime, Iterations, Output);
							return;
						}
						else if (GetPerchRadiusThreshold() > 0.f && Hit.Time == 1.f && OldHitImpactNormal.Z >= WalkableFloorZ)
						{
							// We might be in a virtual 'ditch' within our perch radius. This is rare.
							const FVector PawnLocation = UpdatedComponentInput->GetPosition();
							const float ZMovedDist = FMath::Abs(PawnLocation.Z - OldLocation.Z);
							const float MovedDist2DSq = (PawnLocation - OldLocation).SizeSquared2D();
							if (ZMovedDist <= 0.2f * timeTick && MovedDist2DSq <= 4.f * timeTick)
							{
								Velocity.X += 0.25f * GetMaxSpeed(Output) * (RandomStream.FRand() - 0.5f);
								Velocity.Y += 0.25f * GetMaxSpeed(Output) * (RandomStream.FRand() - 0.5f);
								Velocity.Z = FMath::Max<float>(JumpZVelocity * 0.25f, 1.f);
								Delta = Velocity * timeTick;
								SafeMoveUpdatedComponent(Delta, PawnRotation, true, Hit,  Output);
							}
						}
					}
				}
			}
		}

		if (Velocity.SizeSquared2D() <= UE_KINDA_SMALL_NUMBER * 10.f)
		{
			Velocity.X = 0.f;
			Velocity.Y = 0.f;
		}
	}
}

void FCharacterMovementComponentAsyncInput::PhysicsRotation(float DeltaTime, FCharacterMovementComponentAsyncOutput& Output) const
{
	if (!(bOrientRotationToMovement || bUseControllerDesiredRotation))
	{
		return;
	}

	if (!bHasValidData/* || (!CharacterOwner->Controller && !bRunPhysicsWithNoController)*/)
	{
		return;
	}

	FRotator CurrentRotation = FRotator(UpdatedComponentInput->GetRotation()); // Normalized
	CurrentRotation.DiagnosticCheckNaN(TEXT("CharacterMovementComponent::PhysicsRotation(): CurrentRotation"));

	FRotator DeltaRot = Output.GetDeltaRotation(GetRotationRate(Output), DeltaTime);
	DeltaRot.DiagnosticCheckNaN(TEXT("CharacterMovementComponent::PhysicsRotation(): GetDeltaRotation"));

	FRotator DesiredRotation = CurrentRotation;
	if (bOrientRotationToMovement)
	{
		DesiredRotation = ComputeOrientToMovementRotation(CurrentRotation, DeltaTime, DeltaRot, Output);
	}
	else if (/*CharacterOwner->Controller && */ bUseControllerDesiredRotation)
	{
		DesiredRotation = CharacterInput->ControllerDesiredRotation;
	}
	else
	{
		return;
	}

	if (ShouldRemainVertical(Output))
	{
		DesiredRotation.Pitch = 0.f;
		DesiredRotation.Yaw = FRotator::NormalizeAxis(DesiredRotation.Yaw);
		DesiredRotation.Roll = 0.f;
	}
	else
	{
		DesiredRotation.Normalize();
	}

	// Accumulate a desired new rotation.
	const float AngleTolerance = 1e-3f;

	if (!CurrentRotation.Equals(DesiredRotation, AngleTolerance))
	{
		// PITCH
		if (!FMath::IsNearlyEqual(CurrentRotation.Pitch, DesiredRotation.Pitch, AngleTolerance))
		{
			DesiredRotation.Pitch = FMath::FixedTurn(CurrentRotation.Pitch, DesiredRotation.Pitch, DeltaRot.Pitch);
		}

		// YAW
		if (!FMath::IsNearlyEqual(CurrentRotation.Yaw, DesiredRotation.Yaw, AngleTolerance))
		{
			DesiredRotation.Yaw = FMath::FixedTurn(CurrentRotation.Yaw, DesiredRotation.Yaw, DeltaRot.Yaw);
		}

		// ROLL
		if (!FMath::IsNearlyEqual(CurrentRotation.Roll, DesiredRotation.Roll, AngleTolerance))
		{
			DesiredRotation.Roll = FMath::FixedTurn(CurrentRotation.Roll, DesiredRotation.Roll, DeltaRot.Roll);
		}

		// Set the new rotation.
		DesiredRotation.DiagnosticCheckNaN(TEXT("CharacterMovementComponent::PhysicsRotation(): DesiredRotation"));
		MoveUpdatedComponent(FVector::ZeroVector, DesiredRotation.Quaternion(), /*bSweep*/ false, Output);
	}
}

void FCharacterMovementComponentAsyncInput::MoveAlongFloor(const FVector& InVelocity, float DeltaSeconds, FStepDownResult* OutStepDownResult, FCharacterMovementComponentAsyncOutput& Output) const
{
	if (!Output.CurrentFloor.IsWalkableFloor())
	{
		return;
	}

	// Move along the current floor
	const FVector Delta = FVector(InVelocity.X, InVelocity.Y, 0.f) * DeltaSeconds;
	FHitResult Hit(1.f);

	FVector RampVector = ComputeGroundMovementDelta(Delta, Output.CurrentFloor.HitResult, Output.CurrentFloor.bLineTrace, Output);
	SafeMoveUpdatedComponent(RampVector, UpdatedComponentInput->GetRotation(), true, Hit, Output);
	float LastMoveTimeSlice = DeltaSeconds;

	if (Hit.bStartPenetrating)
	{
		// Allow this hit to be used as an impact we can deflect off, otherwise we do nothing the rest of the update and appear to hitch.
		HandleImpact(Hit, Output);
		SlideAlongSurface(Delta, 1.f, Hit.Normal, Hit, true, Output);

		if (Hit.bStartPenetrating)
		{
			OnCharacterStuckInGeometry(&Hit, Output);
		}
	}
	else if (Hit.IsValidBlockingHit())
	{
		// We impacted something (most likely another ramp, but possibly a barrier).
		float PercentTimeApplied = Hit.Time;
		if ((Hit.Time > 0.f) && (Hit.Normal.Z > UE_KINDA_SMALL_NUMBER) && IsWalkable(Hit))
		{
			// Another walkable ramp.
			const float InitialPercentRemaining = 1.f - PercentTimeApplied;
			RampVector = ComputeGroundMovementDelta(Delta * InitialPercentRemaining, Hit, false, Output);
			LastMoveTimeSlice = InitialPercentRemaining * LastMoveTimeSlice;
			SafeMoveUpdatedComponent(RampVector, UpdatedComponentInput->GetRotation(), true, Hit, Output);

			const float SecondHitPercent = Hit.Time * InitialPercentRemaining;
			PercentTimeApplied = FMath::Clamp(PercentTimeApplied + SecondHitPercent, 0.f, 1.f);
		}

		if (Hit.IsValidBlockingHit())
		{
			if (CanStepUp(Hit, Output) || (Output.NewMovementBase != NULL && Output.NewMovementBaseOwner == Hit.GetActor()))
			{
				// hit a barrier, try to step up
				const FVector PreStepUpLocation = UpdatedComponentInput->GetPosition();
				const FVector GravDir(0.f, 0.f, -1.f);
				if (!StepUp(GravDir, Delta * (1.f - PercentTimeApplied), Hit, Output, OutStepDownResult))
				{
					//UE_LOG(LogCharacterMovement, Verbose, TEXT("- StepUp (ImpactNormal %s, Normal %s"), *Hit.ImpactNormal.ToString(), *Hit.Normal.ToString());
					HandleImpact(Hit, Output, LastMoveTimeSlice, RampVector);
					SlideAlongSurface(Delta, 1.f - PercentTimeApplied, Hit.Normal, Hit, true, Output);
				}
				else
				{
					//UE_LOG(LogCharacterMovement, Verbose, TEXT("+ StepUp (ImpactNormal %s, Normal %s"), *Hit.ImpactNormal.ToString(), *Hit.Normal.ToString());
					if (!bMaintainHorizontalGroundVelocity)
					{
						// Don't recalculate velocity based on this height adjustment, if considering vertical adjustments. Only consider horizontal movement.
						Output.bJustTeleported = true;
						const float StepUpTimeSlice = (1.f - PercentTimeApplied) * DeltaSeconds;
						if (!RootMotion.bHasAnimRootMotion && !RootMotion.bHasOverrideRootMotion && StepUpTimeSlice >= UE_KINDA_SMALL_NUMBER)
						{
							Output.Velocity = (UpdatedComponentInput->GetPosition() - PreStepUpLocation) / StepUpTimeSlice;
							Output.Velocity.Z = 0;
						}
					}
				}
			}
			// TODO StepUp
			/*else if (Hit.Component.IsValid() && !Hit.Component.Get()->CanCharacterStepUp(CharacterOwner))
			{
				HandleImpact(Hit, LastMoveTimeSlice, RampVector);
				SlideAlongSurface(Delta, 1.f - PercentTimeApplied, Hit.Normal, Hit, true);
			}*/
		}
	}
}

FVector FCharacterMovementComponentAsyncInput::ComputeGroundMovementDelta(const FVector& Delta, const FHitResult& RampHit, const bool bHitFromLineTrace, FCharacterMovementComponentAsyncOutput& Output) const
{
	const FVector FloorNormal = RampHit.ImpactNormal;
	const FVector ContactNormal = RampHit.Normal;

	if (FloorNormal.Z < (1.f - UE_KINDA_SMALL_NUMBER) && FloorNormal.Z > UE_KINDA_SMALL_NUMBER && ContactNormal.Z > UE_KINDA_SMALL_NUMBER && !bHitFromLineTrace && IsWalkable(RampHit))
	{
		// Compute a vector that moves parallel to the surface, by projecting the horizontal movement direction onto the ramp.
		const float FloorDotDelta = (FloorNormal | Delta);
		FVector RampMovement(Delta.X, Delta.Y, -FloorDotDelta / FloorNormal.Z);

		if (bMaintainHorizontalGroundVelocity)
		{
			return RampMovement;
		}
		else
		{
			return RampMovement.GetSafeNormal() * Delta.Size();
		}
	}

	return Delta;
}


bool FCharacterMovementComponentAsyncInput::CanCrouchInCurrentState(FCharacterMovementComponentAsyncOutput& Output) const
{
	if (!bCanEverCrouch)
	{
		return false;
	}

	return (IsFalling(Output) || IsMovingOnGround(Output)) && UpdatedComponentInput->bIsSimulatingPhysics;
}


FVector FCharacterMovementComponentAsyncInput::ConstrainInputAcceleration(FVector InputAcceleration, const FCharacterMovementComponentAsyncOutput& Output) const
{
	// walking or falling pawns ignore up/down sliding
	if (InputAcceleration.Z != 0.f && (IsMovingOnGround(Output) || IsFalling(Output)))
	{
		return FVector(InputAcceleration.X, InputAcceleration.Y, 0.f);
	}

	return InputAcceleration;
}

FVector FCharacterMovementComponentAsyncInput::ScaleInputAcceleration(FVector InputAcceleration, FCharacterMovementComponentAsyncOutput& Output) const
{
	return MaxAcceleration * InputAcceleration.GetClampedToMaxSize(1.0f);
}

float FCharacterMovementComponentAsyncInput::ComputeAnalogInputModifier(FVector Acceleration) const
{
	const float MaxAccel = MaxAcceleration;
	if (Acceleration.SizeSquared() > 0.f && MaxAccel > UE_SMALL_NUMBER)
	{
		return FMath::Clamp(Acceleration.Size() / MaxAccel, 0.f, 1.f);
	}

	return 0.f;
}

FVector FCharacterMovementComponentAsyncInput::ConstrainDirectionToPlane(FVector Direction) const
{
	if (bConstrainToPlane)
	{
		Direction = FVector::VectorPlaneProject(Direction, PlaneConstraintNormal);
	}

	return Direction;
}

FVector FCharacterMovementComponentAsyncInput::ConstrainNormalToPlane(FVector Normal) const
{
	if (bConstrainToPlane)
	{
		Normal = FVector::VectorPlaneProject(Normal, PlaneConstraintNormal).GetSafeNormal();
	}

	return Normal;
}

FVector FCharacterMovementComponentAsyncInput::ConstrainLocationToPlane(FVector Location) const
{
	if (bConstrainToPlane)
	{
		Location = FVector::PointPlaneProject(Location, PlaneConstraintOrigin, PlaneConstraintNormal);
	}

	return Location;
}


void FCharacterMovementComponentAsyncInput::MaintainHorizontalGroundVelocity(FCharacterMovementComponentAsyncOutput& Output) const
{
	if (Output.Velocity.Z != 0.f)
	{
		if (bMaintainHorizontalGroundVelocity)
		{
			// Ramp movement already maintained the velocity, so we just want to remove the vertical component.
			Output.Velocity.Z = 0.f;
		}
		else
		{
			// Rescale velocity to be horizontal but maintain magnitude of last update.
			Output.Velocity = Output.Velocity.GetSafeNormal2D() * Output.Velocity.Size();
		}
	}
}

bool FCharacterMovementComponentAsyncInput::MoveUpdatedComponent(const FVector& Delta, const FQuat& NewRotation, bool bSweep, FCharacterMovementComponentAsyncOutput& Output, FHitResult* OutHitResult, ETeleportType TeleportType) const
{
	const FVector NewDelta = ConstrainDirectionToPlane(Delta);
	return UpdatedComponentInput->MoveComponent(Delta, NewRotation, bSweep, OutHitResult, Output.MoveComponentFlags, TeleportType, *this, Output);
}

bool FCharacterMovementComponentAsyncInput::SafeMoveUpdatedComponent(const FVector& Delta, const FQuat& NewRotation, bool bSweep, FHitResult& OutHit, FCharacterMovementComponentAsyncOutput& Output, ETeleportType Teleport) const
{
	// Ensuring that we have this when filling inputs
	/*if (UpdatedComponent == NULL)
	{
		OutHit.Reset(1.f);
		return false;
	}*/

	bool bMoveResult = false;

	// Scope for move flags
	{
		// Conditionally ignore blocking overlaps (based on CVar)
		const EMoveComponentFlags IncludeBlockingOverlapsWithoutEvents = (MOVECOMP_NeverIgnoreBlockingOverlaps | MOVECOMP_DisableBlockingOverlapDispatch);
		EMoveComponentFlags& MoveComponentFlags = Output.MoveComponentFlags;
		TGuardValue<EMoveComponentFlags> ScopedFlagRestore(MoveComponentFlags, MovementComponentCVars::MoveIgnoreFirstBlockingOverlap ? MoveComponentFlags : (MoveComponentFlags | IncludeBlockingOverlapsWithoutEvents));
		bMoveResult = MoveUpdatedComponent(Delta, NewRotation, bSweep, Output, &OutHit, Teleport);
	}

	// Handle initial penetrations
	if (OutHit.bStartPenetrating/* && UpdatedComponent*/)
	{
		const FVector RequestedAdjustment = GetPenetrationAdjustment(OutHit);

		if (ResolvePenetration(RequestedAdjustment, OutHit, NewRotation, Output))
		{
			// Retry original move
			bMoveResult = MoveUpdatedComponent(Delta, NewRotation, bSweep, Output, &OutHit, Teleport);
		}
	}

	return bMoveResult;
}

void FCharacterMovementComponentAsyncInput::ApplyAccumulatedForces(float DeltaSeconds, FCharacterMovementComponentAsyncOutput& Output) const
{
	if (Output.PendingImpulseToApply.Z != 0.f || Output.PendingForceToApply.Z != 0.f)
	{
		// check to see if applied momentum is enough to overcome gravity
		if (IsMovingOnGround(Output) && (Output.PendingImpulseToApply.Z + (Output.PendingForceToApply.Z * DeltaSeconds) + (GravityZ * DeltaSeconds) > UE_SMALL_NUMBER))
		{
			SetMovementMode(MOVE_Falling, Output);
		}
	}

	Output.Velocity += Output.PendingImpulseToApply + (Output.PendingForceToApply * DeltaSeconds);

	// Don't call ClearAccumulatedForces() because it could affect launch velocity
	Output.PendingImpulseToApply = FVector::ZeroVector;
	Output.PendingForceToApply = FVector::ZeroVector;
}

void FCharacterMovementComponentAsyncInput::ClearAccumulatedForces(FCharacterMovementComponentAsyncOutput& Output) const
{
	Output.PendingImpulseToApply = FVector::ZeroVector;
	Output.PendingForceToApply = FVector::ZeroVector;
	Output.PendingLaunchVelocity = FVector::ZeroVector;
}

void FCharacterMovementComponentAsyncInput::SetMovementMode(EMovementMode NewMovementMode, FCharacterMovementComponentAsyncOutput& Output, uint8 NewCustomMode) const
{
	if (NewMovementMode != MOVE_Custom)
	{
		NewCustomMode = 0;
	}

	// If trying to use NavWalking but there is no navmesh, use walking instead.
	if (NewMovementMode == MOVE_NavWalking)
	{
		ensure(false); // TOOD
		/*if (GetNavData() == nullptr)
		{
			NewMovementMode = MOVE_Walking;
		}*/
	}

	// Do nothing if nothing is changing.
	if (Output.MovementMode == NewMovementMode)
	{
		// Allow changes in custom sub-mode.
		if ((NewMovementMode != MOVE_Custom) || (NewCustomMode == Output.CustomMovementMode))
		{
			return;
		}
	}
	
	const EMovementMode PrevMovementMode = Output.MovementMode;
	const uint8 PrevCustomMode = Output.CustomMovementMode;

	Output.MovementMode = NewMovementMode;
	Output.CustomMovementMode = NewCustomMode;

	// We allow setting movement mode before we have a component to update, in case this happens at startup.
	if (!bHasValidData)
	{
		return;
	}

	// Handle change in movement mode
	OnMovementModeChanged(PrevMovementMode, PrevCustomMode, Output);

	// @todo UE4 do we need to disable ragdoll physics here? Should this function do nothing if in ragdoll?
}

void FCharacterMovementComponentAsyncInput::OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode, FCharacterMovementComponentAsyncOutput& Output) const
{
	if (!bHasValidData)
	{
		return;
	}

	// Update collision settings if needed
	if (Output.MovementMode == MOVE_NavWalking)
	{
		ensure(false);
		// Reset cached nav location used by NavWalking
	/*	CachedNavLocation = FNavLocation();

		GroundMovementMode = MovementMode;
		// Walking uses only XY velocity
		Velocity.Z = 0.f;
		SetNavWalkingPhysics(true);*/
	}
	else if (PreviousMovementMode == MOVE_NavWalking)
	{
		ensure(false);
		/*if (MovementMode == DefaultLandMovementMode || IsWalking())
		{
			const bool bSucceeded = TryToLeaveNavWalking();
			if (!bSucceeded)
			{
				return;
			}
		}
		else
		{
			SetNavWalkingPhysics(false);
		}*/
	}

	// React to changes in the movement mode.
	if (Output.MovementMode == MOVE_Walking)
	{
		// Walking uses only XY velocity, and must be on a walkable floor, with a Base.
		Output.Velocity.Z = 0.f;
		Output.bCrouchMaintainsBaseLocation = true;
		Output.GroundMovementMode = Output.MovementMode;


		// make sure we update our new floor/base on initial entry of the walking physics
		FindFloor(UpdatedComponentInput->GetPosition(), Output.CurrentFloor, false, Output);
		AdjustFloorHeight(Output);
		SetBaseFromFloor(Output.CurrentFloor, Output);
	}
	else
	{
		Output.CurrentFloor.Clear();
		Output.bCrouchMaintainsBaseLocation = false;

		if (Output.MovementMode == MOVE_Falling)
		{
			// TODO MovementBase
			//Output.Velocity += GetImpartedMovementBaseVelocity();
			//CharacterOwner->Falling();
		}

		//SetBase(NULL);
		Output.NewMovementBase = nullptr;
		Output.NewMovementBaseOwner = nullptr;


		if (Output.MovementMode == MOVE_None)
		{
			ensure(false);
			// Kill velocity and clear queued up events
			/*StopMovementKeepPathing();
			CharacterOwner->
			tate();
			ClearAccumulatedForces();*/
		}
	}

	if (Output.MovementMode == MOVE_Falling && PreviousMovementMode != MOVE_Falling)
	{
	 // TODO PathFollowingAgent
	/*	IPathFollowingAgentInterface* PFAgent = GetPathFollowingAgent();
		if (PFAgent)
		{
			PFAgent->OnStartedFalling();
		}*/
	}

	CharacterInput->OnMovementModeChanged(PreviousMovementMode, *this, Output, PreviousCustomMode);

	//ensureMsgf(GroundMovementMode == MOVE_Walking || GroundMovementMode == MOVE_NavWalking, TEXT("Invalid GroundMovementMode %d. MovementMode: %d, PreviousMovementMode: %d"), GroundMovementMode.GetValue(), MovementMode.GetValue(), PreviousMovementMode);
}

void FCharacterMovementComponentAsyncInput::FindFloor(const FVector& CapsuleLocation, FFindFloorResult& OutFloorResult, bool bCanUseCachedLocation, FCharacterMovementComponentAsyncOutput& Output, const FHitResult* DownwardSweepResult) const
{
	//SCOPE_CYCLE_COUNTER(STAT_CharFindFloor);

	// No collision, no floor...
	if (!bHasValidData || !UpdatedComponentInput->bIsQueryCollisionEnabled)
	{
		OutFloorResult.Clear();
		return;
	}

//	UE_LOG(LogCharacterMovement, VeryVerbose, TEXT("[Role:%d] FindFloor: %s at location %s"), (int32)CharacterOwner->GetLocalRole(), *GetNameSafe(CharacterOwner), *CapsuleLocation.ToString());
	//check(CharacterOwner->GetCapsuleComponent());


	// Increase height check slightly if walking, to prevent floor height adjustment from later invalidating the floor result.
	const float MaxFloorDist = UCharacterMovementComponent::MAX_FLOOR_DIST;
	const float MinFloorDist = UCharacterMovementComponent::MIN_FLOOR_DIST;
	const float HeightCheckAdjust = (IsMovingOnGround(Output) ? MaxFloorDist + UE_KINDA_SMALL_NUMBER : -MaxFloorDist);

	float FloorSweepTraceDist = FMath::Max(MaxFloorDist, MaxStepHeight + HeightCheckAdjust);
	float FloorLineTraceDist = FloorSweepTraceDist;
	bool bNeedToValidateFloor = true;

	// Sweep floor
	if (FloorLineTraceDist > 0.f || FloorSweepTraceDist > 0.f)
	{
		//UCharacterMovementComponent* MutableThis = const_cast<UCharacterMovementComponent*>(this);

		if (bAlwaysCheckFloor || !bCanUseCachedLocation || Output.bForceNextFloorCheck || Output.bJustTeleported)
		{
			Output.bForceNextFloorCheck = false;

			ComputeFloorDist(CapsuleLocation, FloorLineTraceDist, FloorSweepTraceDist, OutFloorResult, Output.ScaledCapsuleRadius, Output, DownwardSweepResult);
		}
		else
		{
			// Force floor check if base has collision disabled or if it does not block us.
			UPrimitiveComponent* MovementBase = Output.NewMovementBase;
			//const AActor* BaseActor = MovementBase ? MovementBase->GetOwner() : NULL;

			if (MovementBase != NULL)
			{

				// For now used cached values from original movement base, it could have changed, so this is wrong if so.
				MovementBaseAsyncData.Validate(Output);
				// TODO MovementBase
				ensure(false);

				/*Output->bForceNextFloorCheck = !MovementBase->IsQueryCollisionEnabled()
					|| MovementBase->GetCollisionResponseToChannel(CollisionChannel) != ECR_Block
					|| MovementBaseUtility::IsDynamicBase(MovementBase);*/
			}

			//const bool IsActorBasePendingKill = BaseActor && !IsValid(BaseActor);

			if (false)//!bForceNextFloorCheck && !IsActorBasePendingKill && MovementBase)
			{
				//UE_LOG(LogCharacterMovement, Log, TEXT("%s SKIP check for floor"), *CharacterOwner->GetName());
				OutFloorResult = Output.CurrentFloor;
				bNeedToValidateFloor = false;
			}
			else
			{
				Output.bForceNextFloorCheck = false;
				ComputeFloorDist(CapsuleLocation, FloorLineTraceDist, FloorSweepTraceDist, OutFloorResult, Output.ScaledCapsuleRadius, Output, DownwardSweepResult);
			}
		}
	}


	// OutFloorResult.HitResult is now the result of the vertical floor check.
	// See if we should try to "perch" at this location.
	if (bNeedToValidateFloor && OutFloorResult.bBlockingHit && !OutFloorResult.bLineTrace)
	{
		const bool bCheckRadius = true;
		if (ShouldComputePerchResult(OutFloorResult.HitResult, Output, bCheckRadius))
		{
			float MaxPerchFloorDist = FMath::Max(MaxFloorDist, MaxStepHeight + HeightCheckAdjust);
			if (IsMovingOnGround(Output))
			{
				MaxPerchFloorDist += FMath::Max(0.f, PerchAdditionalHeight);
			}

			FFindFloorResult PerchFloorResult;
			if (ComputePerchResult(GetValidPerchRadius(Output), OutFloorResult.HitResult, MaxPerchFloorDist, PerchFloorResult, Output))
			{
				// Don't allow the floor distance adjustment to push us up too high, or we will move beyond the perch distance and fall next time.
				const float AvgFloorDist = (MinFloorDist + MaxFloorDist) * 0.5f;
				const float MoveUpDist = (AvgFloorDist - OutFloorResult.FloorDist);
				if (MoveUpDist + PerchFloorResult.FloorDist >= MaxPerchFloorDist)
				{
					OutFloorResult.FloorDist = AvgFloorDist;
				}

				// If the regular capsule is on an unwalkable surface but the perched one would allow us to stand, override the normal to be one that is walkable.
				if (!OutFloorResult.bWalkableFloor)
				{
					// Floor distances are used as the distance of the regular capsule to the point of collision, to make sure AdjustFloorHeight() behaves correctly.
					OutFloorResult.SetFromLineTrace(PerchFloorResult.HitResult, OutFloorResult.FloorDist, FMath::Max(OutFloorResult.FloorDist, MinFloorDist), true);
				}
			}
			else
			{
				// We had no floor (or an invalid one because it was unwalkable), and couldn't perch here, so invalidate floor (which will cause us to start falling).
				OutFloorResult.bWalkableFloor = false;
			}
		}
	}
}

void FCharacterMovementComponentAsyncInput::ComputeFloorDist(const FVector& CapsuleLocation, float LineDistance, float SweepDistance, FFindFloorResult& OutFloorResult, float SweepRadius, FCharacterMovementComponentAsyncOutput& Output, const FHitResult* DownwardSweepResult) const
{
	//UE_LOG(LogCharacterMovement, VeryVerbose, TEXT("[Role:%d] ComputeFloorDist: %s at location %s"), (int32)CharacterOwner->GetLocalRole(), *GetNameSafe(CharacterOwner), *CapsuleLocation.ToString());
	OutFloorResult.Clear();

	float PawnRadius = Output.ScaledCapsuleRadius;
	float PawnHalfHeight = Output.ScaledCapsuleHalfHeight;
	//CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleSize(PawnRadius, PawnHalfHeight);

	bool bSkipSweep = false;
	if (DownwardSweepResult != NULL && DownwardSweepResult->IsValidBlockingHit())
	{
		// Only if the supplied sweep was vertical and downward.
		if ((DownwardSweepResult->TraceStart.Z > DownwardSweepResult->TraceEnd.Z) &&
			(DownwardSweepResult->TraceStart - DownwardSweepResult->TraceEnd).SizeSquared2D() <= UE_KINDA_SMALL_NUMBER)
		{
			// Reject hits that are barely on the cusp of the radius of the capsule
			if (IsWithinEdgeTolerance(DownwardSweepResult->Location, DownwardSweepResult->ImpactPoint, PawnRadius))
			{
				// Don't try a redundant sweep, regardless of whether this sweep is usable.
				bSkipSweep = true;

				const bool bIsWalkable = IsWalkable(*DownwardSweepResult);
				const float FloorDist = (CapsuleLocation.Z - DownwardSweepResult->Location.Z);
				OutFloorResult.SetFromSweep(*DownwardSweepResult, FloorDist, bIsWalkable);

				if (bIsWalkable)
				{
					// Use the supplied downward sweep as the floor hit result.			
					return;
				}
			}
		}
	}

	// We require the sweep distance to be >= the line distance, otherwise the HitResult can't be interpreted as the sweep result.
	if (SweepDistance < LineDistance)
	{
		ensure(SweepDistance >= LineDistance);
		return;
	}

	bool bBlockingHit = false;
	//FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ComputeFloorDist), false, CharacterOwner);
	//FCollisionResponseParams ResponseParam;
	//InitCollisionParams(QueryParams, ResponseParam);

	// Sweep test
	if (!bSkipSweep && SweepDistance > 0.f && SweepRadius > 0.f)
	{
		// Use a shorter height to avoid sweeps giving weird results if we start on a surface.
		// This also allows us to adjust out of penetrations.
		const float ShrinkScale = 0.9f;
		const float ShrinkScaleOverlap = 0.1f;
		float ShrinkHeight = (PawnHalfHeight - PawnRadius) * (1.f - ShrinkScale);
		float TraceDist = SweepDistance + ShrinkHeight;
		FCollisionShape CapsuleShape = FCollisionShape::MakeCapsule(SweepRadius, PawnHalfHeight - ShrinkHeight);

		FHitResult Hit(1.f);
		bBlockingHit = FloorSweepTest(Hit, CapsuleLocation, CapsuleLocation + FVector(0.f, 0.f, -TraceDist), CollisionChannel, CapsuleShape, QueryParams, CollisionResponseParams, Output);

		if (bBlockingHit)
		{
			// Reject hits adjacent to us, we only care about hits on the bottom portion of our capsule.
			// Check 2D distance to impact point, reject if within a tolerance from radius.
			if (Hit.bStartPenetrating || !IsWithinEdgeTolerance(CapsuleLocation, Hit.ImpactPoint, CapsuleShape.Capsule.Radius))
			{
				// Use a capsule with a slightly smaller radius and shorter height to avoid the adjacent object.
				// Capsule must not be nearly zero or the trace will fall back to a line trace from the start point and have the wrong length.
				CapsuleShape.Capsule.Radius = FMath::Max(0.f, CapsuleShape.Capsule.Radius - UCharacterMovementComponent::SWEEP_EDGE_REJECT_DISTANCE - UE_KINDA_SMALL_NUMBER);
				if (!CapsuleShape.IsNearlyZero())
				{
					ShrinkHeight = (PawnHalfHeight - PawnRadius) * (1.f - ShrinkScaleOverlap);
					TraceDist = SweepDistance + ShrinkHeight;
					CapsuleShape.Capsule.HalfHeight = FMath::Max(PawnHalfHeight - ShrinkHeight, CapsuleShape.Capsule.Radius);
					Hit.Reset(1.f, false);

					bBlockingHit = FloorSweepTest(Hit, CapsuleLocation, CapsuleLocation + FVector(0.f, 0.f, -TraceDist), CollisionChannel, CapsuleShape, QueryParams, CollisionResponseParams, Output);
				}
			}

			// Reduce hit distance by ShrinkHeight because we shrank the capsule for the trace.
			// We allow negative distances here, because this allows us to pull out of penetrations.
			const float MaxPenetrationAdjust = FMath::Max(UCharacterMovementComponent::MAX_FLOOR_DIST, PawnRadius);
			const float SweepResult = FMath::Max(-MaxPenetrationAdjust, Hit.Time * TraceDist - ShrinkHeight);

			OutFloorResult.SetFromSweep(Hit, SweepResult, false);
			if (Hit.IsValidBlockingHit() && IsWalkable(Hit))
			{
				if (SweepResult <= SweepDistance)
				{
					// Hit within test distance.
					OutFloorResult.bWalkableFloor = true;
					return;
				}
			}
		}
	}

	// Since we require a longer sweep than line trace, we don't want to run the line trace if the sweep missed everything.
	// We do however want to try a line trace if the sweep was stuck in penetration.
	if (!OutFloorResult.bBlockingHit && !OutFloorResult.HitResult.bStartPenetrating)
	{
		OutFloorResult.FloorDist = SweepDistance;
		return;
	}

	// Line trace
	if (LineDistance > 0.f)
	{
		const float ShrinkHeight = PawnHalfHeight;
		const FVector LineTraceStart = CapsuleLocation;
		const float TraceDist = LineDistance + ShrinkHeight;
		const FVector Down = FVector(0.f, 0.f, -TraceDist);
		//QueryParams.TraceTag = SCENE_QUERY_STAT_NAME_ONLY(FloorLineTrace); TODO

		FHitResult Hit(1.f);
		bBlockingHit = World->LineTraceSingleByChannel(Hit, LineTraceStart, LineTraceStart + Down, CollisionChannel, QueryParams, CollisionResponseParams);

		if (bBlockingHit)
		{
			if (Hit.Time > 0.f)
			{
				// Reduce hit distance by ShrinkHeight because we started the trace higher than the base.
				// We allow negative distances here, because this allows us to pull out of penetrations.
				const float MaxPenetrationAdjust = FMath::Max(UCharacterMovementComponent::MAX_FLOOR_DIST, PawnRadius);
				const float LineResult = FMath::Max(-MaxPenetrationAdjust, Hit.Time * TraceDist - ShrinkHeight);

				OutFloorResult.bBlockingHit = true;
				if (LineResult <= LineDistance && IsWalkable(Hit))
				{
					OutFloorResult.SetFromLineTrace(Hit, OutFloorResult.FloorDist, LineResult, true);
					return;
				}
			}
		}
	}

	// No hits were acceptable.
	OutFloorResult.bWalkableFloor = false;
}

bool FCharacterMovementComponentAsyncInput::FloorSweepTest(FHitResult& OutHit, const FVector& Start, const FVector& End, ECollisionChannel TraceChannel, const FCollisionShape& CollisionShape, const FCollisionQueryParams& Params, const FCollisionResponseParams& ResponseParam, FCharacterMovementComponentAsyncOutput& Output) const
{
	bool bBlockingHit = false;

	if (!bUseFlatBaseForFloorChecks)
	{
		bBlockingHit = World->SweepSingleByChannel(OutHit, Start, End, FQuat::Identity, TraceChannel, CollisionShape, Params, ResponseParam);
	}
	else
	{
		// Test with a box that is enclosed by the capsule.
		const float CapsuleRadius = CollisionShape.GetCapsuleRadius();
		const float CapsuleHeight = CollisionShape.GetCapsuleHalfHeight();
		const FCollisionShape BoxShape = FCollisionShape::MakeBox(FVector(CapsuleRadius * 0.707f, CapsuleRadius * 0.707f, CapsuleHeight));

		// First test with the box rotated so the corners are along the major axes (ie rotated 45 degrees).
		bBlockingHit = World->SweepSingleByChannel(OutHit, Start, End, FQuat(FVector(0.f, 0.f, -1.f), UE_PI * 0.25f), TraceChannel, BoxShape, Params, ResponseParam);

		if (!bBlockingHit)
		{
			// Test again with the same box, not rotated.
			OutHit.Reset(1.f, false);
			bBlockingHit = World->SweepSingleByChannel(OutHit, Start, End, FQuat::Identity, TraceChannel, BoxShape, Params, ResponseParam);
		}
	}

	return bBlockingHit;
}

bool FCharacterMovementComponentAsyncInput::IsWithinEdgeTolerance(const FVector& CapsuleLocation, const FVector& TestImpactPoint, const float CapsuleRadius) const
{
	const float DistFromCenterSq = (TestImpactPoint - CapsuleLocation).SizeSquared2D();
	const float ReducedRadiusSq = FMath::Square(FMath::Max(UCharacterMovementComponent::SWEEP_EDGE_REJECT_DISTANCE + UE_KINDA_SMALL_NUMBER, CapsuleRadius - UCharacterMovementComponent::SWEEP_EDGE_REJECT_DISTANCE));
	return DistFromCenterSq < ReducedRadiusSq;
}

bool FCharacterMovementComponentAsyncInput::IsWalkable(const FHitResult& Hit) const
{
	// TODO: Possibly handle an override for this with UFortMovementComp_CharacterAthena_Ostrich.

	if (!Hit.IsValidBlockingHit())
	{
		// No hit, or starting in penetration
		return false;
	}

	// Never walk up vertical surfaces.
	if (Hit.ImpactNormal.Z < UE_KINDA_SMALL_NUMBER)
	{
		return false;
	}

	// todo, how to read walkable slope override off hit component?
	float TestWalkableZ = WalkableFloorZ;

	// TODO: this isn't thread safe, put in physics user data?
	
	// See if this component overrides the walkable floor z.
	/*const UPrimitiveComponent* HitComponent = Hit.Component.Get();
	if (HitComponent)
	{
		const FWalkableSlopeOverride& SlopeOverride = HitComponent->GetWalkableSlopeOverride();
		TestWalkableZ = SlopeOverride.ModifyWalkableFloorZ(TestWalkableZ);
	}*/

	// Can't walk on this surface if it is too steep.
	if (Hit.ImpactNormal.Z < TestWalkableZ)
	{
		return false;
	}

	return true;
}

void FCharacterMovementComponentAsyncInput::UpdateCharacterStateAfterMovement(float DeltaSeconds, FCharacterMovementComponentAsyncOutput& Output) const
{
	// Proxies get replicated crouch state.
	if (CharacterInput->LocalRole != ROLE_SimulatedProxy)
	{
		// Uncrouch if no longer allowed to be crouched
		if (Output.bIsCrouched && !CanCrouchInCurrentState(Output))
		{
			// TODO Crouching
			//UnCrouch(false);
		}
	}
}

float FCharacterMovementComponentAsyncInput::GetSimulationTimeStep(float RemainingTime, int32 Iterations) const
{
	static uint32 s_WarningCount = 0;
	if (RemainingTime > MaxSimulationTimeStep)
	{
		if (Iterations < MaxSimulationIterations)
		{
			// Subdivide moves to be no longer than MaxSimulationTimeStep seconds
			RemainingTime = FMath::Min(MaxSimulationTimeStep, RemainingTime * 0.5f);
		}
		else
		{
			// If this is the last iteration, just use all the remaining time. This is usually better than cutting things short, as the simulation won't move far enough otherwise.
			// Print a throttled warning.
/*#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			if ((s_WarningCount++ < 100) || (GFrameCounter & 15) == 0)
			{
				UE_LOG(LogCharacterMovement, Warning, TEXT("GetSimulationTimeStep() - Max iterations %d hit while remaining time %.6f > MaxSimulationTimeStep (%.3f) for '%s', movement '%s'"), MaxSimulationIterations, RemainingTime, MaxSimulationTimeStep, *GetNameSafe(CharacterOwner), *GetMovementName());
			}
#endif*/
		}
	}

	// no less than MIN_TICK_TIME (to avoid potential divide-by-zero during simulation).
	return FMath::Max(UCharacterMovementComponent::MIN_TICK_TIME, RemainingTime);
}

void FCharacterMovementComponentAsyncInput::CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration, FCharacterMovementComponentAsyncOutput& Output) const
{
	// Do not update velocity when using root motion or when SimulatedProxy and not simulating root motion - SimulatedProxy are repped their Velocity
	if (!bHasValidData || RootMotion.bHasAnimRootMotion || DeltaTime < UCharacterMovementComponent::MIN_TICK_TIME
		|| (/*CharacterOwner && */CharacterInput->LocalRole == ROLE_SimulatedProxy && !bWasSimulatingRootMotion))
	{
		return;
	}

	Friction = FMath::Max(0.f, Friction);
	const float MaxAccel = MaxAcceleration;
	float MaxSpeed = GetMaxSpeed(Output);

	// Check if path following requested movement
	bool bZeroRequestedAcceleration = true;
	FVector RequestedAcceleration = FVector::ZeroVector;
	float RequestedSpeed = 0.0f;

	if (ApplyRequestedMove(DeltaTime, MaxAccel, MaxSpeed, Friction, BrakingDeceleration, RequestedAcceleration, RequestedSpeed, Output))
	{
		bZeroRequestedAcceleration = false;
	}

	FVector& Acceleration = Output.Acceleration;
	FVector& Velocity = Output.Velocity;

	if (bForceMaxAccel)
	{
		// Force acceleration at full speed.
		// In consideration order for direction: Acceleration, then Velocity, then Pawn's rotation.
		if (Acceleration.SizeSquared() > UE_SMALL_NUMBER)
		{
			Acceleration = Acceleration.GetSafeNormal() * MaxAccel;
		}
		else
		{
			Acceleration = MaxAccel * (Velocity.SizeSquared() < UE_SMALL_NUMBER ? UpdatedComponentInput->GetForwardVector() : Velocity.GetSafeNormal());
		}

		Output.AnalogInputModifier = 1.f;
	}



	// Path following above didn't care about the analog modifier, but we do for everything else below, so get the fully modified value.
	// Use max of requested speed and max speed if we modified the speed in ApplyRequestedMove above.
	const float MaxInputSpeed = FMath::Max(MaxSpeed * Output.AnalogInputModifier, GetMinAnalogSpeed(Output));
	MaxSpeed = FMath::Max(RequestedSpeed, MaxInputSpeed);

	// Apply braking or deceleration
	const bool bZeroAcceleration = Acceleration.IsZero();
	const bool bVelocityOverMax = IsExceedingMaxSpeed(MaxSpeed, Output);

	// Only apply braking if there is no acceleration, or we are over our max speed and need to slow down to it.
	if ((bZeroAcceleration && bZeroRequestedAcceleration) || bVelocityOverMax)
	{
		const FVector OldVelocity = Velocity;

		const float ActualBrakingFriction = (bUseSeparateBrakingFriction ? BrakingFriction : Friction);
		ApplyVelocityBraking(DeltaTime, ActualBrakingFriction, BrakingDeceleration, Output);

		// Don't allow braking to lower us below max speed if we started above it.
		if (bVelocityOverMax && Velocity.SizeSquared() < FMath::Square(MaxSpeed) && FVector::DotProduct(Acceleration, OldVelocity) > 0.0f)
		{
			Velocity = OldVelocity.GetSafeNormal() * MaxSpeed;
		}
	}
	else if (!bZeroAcceleration)
	{
		// Friction affects our ability to change direction. This is only done for input acceleration, not path following.
		const FVector AccelDir = Acceleration.GetSafeNormal();
		const float VelSize = Velocity.Size();
		Velocity = Velocity - (Velocity - AccelDir * VelSize) * FMath::Min(DeltaTime * Friction, 1.f);
	}

	// Apply fluid friction
	if (bFluid)
	{
		Velocity = Velocity * (1.f - FMath::Min(Friction * DeltaTime, 1.f));
	}

	// Apply input acceleration
	if (!bZeroAcceleration)
	{
		const float NewMaxInputSpeed = IsExceedingMaxSpeed(MaxInputSpeed, Output) ? Velocity.Size() : MaxInputSpeed;
		Velocity += Acceleration * DeltaTime;
		Velocity = Velocity.GetClampedToMaxSize(NewMaxInputSpeed);
	}

	// Apply additional requested acceleration
	if (!bZeroRequestedAcceleration)
	{
		const float NewMaxRequestedSpeed = IsExceedingMaxSpeed(RequestedSpeed, Output) ? Velocity.Size() : RequestedSpeed;
		Velocity += RequestedAcceleration * DeltaTime;
		Velocity = Velocity.GetClampedToMaxSize(NewMaxRequestedSpeed);
	}

	// TODO RVOAvoidance
	/*if (bUseRVOAvoidance)
	{
		CalcAvoidanceVelocity(DeltaTime);
	}*/
}

bool FCharacterMovementComponentAsyncInput::ApplyRequestedMove(float DeltaTime, float MaxAccel, float MaxSpeed, float Friction, float BrakingDeceleration, FVector& OutAcceleration, float& OutRequestedSpeed, FCharacterMovementComponentAsyncOutput& Output) const
{
	if (Output.bHasRequestedVelocity)
	{
		const float RequestedSpeedSquared = Output.RequestedVelocity.SizeSquared();
		if (RequestedSpeedSquared < UE_KINDA_SMALL_NUMBER)
		{
			return false;
		}

		// Compute requested speed from path following
		float RequestedSpeed = FMath::Sqrt(RequestedSpeedSquared);
		const FVector RequestedMoveDir = Output.RequestedVelocity / RequestedSpeed;
		RequestedSpeed = (Output.bRequestedMoveWithMaxSpeed ? MaxSpeed : FMath::Min(MaxSpeed, RequestedSpeed));

		// Compute actual requested velocity
		const FVector MoveVelocity = RequestedMoveDir * RequestedSpeed;

		// Compute acceleration. Use MaxAccel to limit speed increase, 1% buffer.
		FVector NewAcceleration = FVector::ZeroVector;
		const float CurrentSpeedSq = Output.Velocity.SizeSquared();
		if (ShouldComputeAccelerationToReachRequestedVelocity(RequestedSpeed, Output))
		{
			// Turn in the same manner as with input acceleration.
			const float VelSize = FMath::Sqrt(CurrentSpeedSq);
			Output.Velocity = Output.Velocity - (Output.Velocity - RequestedMoveDir * VelSize) * FMath::Min(DeltaTime * Friction, 1.f);

			// How much do we need to accelerate to get to the new velocity?
			NewAcceleration = ((MoveVelocity - Output.Velocity) / DeltaTime);
			NewAcceleration = NewAcceleration.GetClampedToMaxSize(MaxAccel);
		}
		else
		{
			// Just set velocity directly.
			// If decelerating we do so instantly, so we don't slide through the destination if we can't brake fast enough.
			Output.Velocity = MoveVelocity;
		}

		// Copy to out params
		OutRequestedSpeed = RequestedSpeed;
		OutAcceleration = NewAcceleration;
		return true;
	}

	return false;
}

bool FCharacterMovementComponentAsyncInput::ShouldComputeAccelerationToReachRequestedVelocity(const float RequestedSpeed, FCharacterMovementComponentAsyncOutput& Output) const
{
	// Compute acceleration if accelerating toward requested speed, 1% buffer.
	return bRequestedMoveUseAcceleration && Output.Velocity.SizeSquared() < FMath::Square(RequestedSpeed * 1.01f);
}

float FCharacterMovementComponentAsyncInput::GetMinAnalogSpeed(FCharacterMovementComponentAsyncOutput& Output) const
{
	switch (Output.MovementMode)
	{
	case MOVE_Walking:
	case MOVE_NavWalking:
	case MOVE_Falling:
		return MinAnalogWalkSpeed;
	default:
		return 0.f;
	}
}

float FCharacterMovementComponentAsyncInput::GetMaxBrakingDeceleration(FCharacterMovementComponentAsyncOutput& Output) const
{
	switch (Output.MovementMode)
	{
	case MOVE_Walking:
	case MOVE_NavWalking:
		return BrakingDecelerationWalking;
	case MOVE_Falling:
		return BrakingDecelerationFalling;
	case MOVE_Swimming:
		return BrakingDecelerationSwimming;
	case MOVE_Flying:
		return BrakingDecelerationFlying;
	case MOVE_Custom:
		return 0.f;
	case MOVE_None:
	default:
		return 0.f;
	}
}

void FCharacterMovementComponentAsyncInput::ApplyVelocityBraking(float DeltaTime, float Friction, float BrakingDeceleration, FCharacterMovementComponentAsyncOutput& Output) const
{
	FVector& Velocity = Output.Velocity;

	if (Velocity.IsZero() || !bHasValidData || RootMotion.bHasAnimRootMotion || DeltaTime < UCharacterMovementComponent::MIN_TICK_TIME)
	{
		return;
	}

	const float FrictionFactor = FMath::Max(0.f, BrakingFrictionFactor);
	Friction = FMath::Max(0.f, Friction * FrictionFactor);
	BrakingDeceleration = FMath::Max(0.f, BrakingDeceleration);
	const bool bZeroFriction = (Friction == 0.f);
	const bool bZeroBraking = (BrakingDeceleration == 0.f);

	if (bZeroFriction && bZeroBraking)
	{
		return;
	}

	const FVector OldVel = Velocity;

	// subdivide braking to get reasonably consistent results at lower frame rates
	// (important for packet loss situations w/ networking)
	float RemainingTime = DeltaTime;
	const float MaxTimeStep = FMath::Clamp(BrakingSubStepTime, 1.0f / 75.0f, 1.0f / 20.0f);

	// Decelerate to brake to a stop
	const FVector RevAccel = (bZeroBraking ? FVector::ZeroVector : (-BrakingDeceleration * Velocity.GetSafeNormal()));
	while (RemainingTime >= UCharacterMovementComponent::MIN_TICK_TIME)
	{
		// Zero friction uses constant deceleration, so no need for iteration.
		const float dt = ((RemainingTime > MaxTimeStep && !bZeroFriction) ? FMath::Min(MaxTimeStep, RemainingTime * 0.5f) : RemainingTime);
		RemainingTime -= dt;

		// apply friction and braking
		Velocity = Velocity + ((-Friction) * Velocity + RevAccel) * dt;

		// Don't reverse direction
		if ((Velocity | OldVel) <= 0.f)
		{
			Velocity = FVector::ZeroVector;
			return;
		}
	}

	// Clamp to zero if nearly zero, or if below min threshold and braking.
	const float VSizeSq = Velocity.SizeSquared();
	if (VSizeSq <= UE_KINDA_SMALL_NUMBER || (!bZeroBraking && VSizeSq <= FMath::Square(UCharacterMovementComponent::BRAKE_TO_STOP_VELOCITY)))
	{
		Velocity = FVector::ZeroVector;
	}
}

FVector FCharacterMovementComponentAsyncInput::GetPenetrationAdjustment(FHitResult& HitResult) const
{
	FVector Result = MoveComponent_GetPenetrationAdjustment(HitResult);//Super::GetPenetrationAdjustment(Hit);

	//if (CharacterOwner)
	{
		const bool bIsProxy = (CharacterInput->LocalRole == ROLE_SimulatedProxy);
		float MaxDistance = bIsProxy ? MaxDepenetrationWithGeometryAsProxy : MaxDepenetrationWithGeometry;
		const AActor* HitActor = HitResult.GetActor();
		if (Cast<APawn>(HitActor)) // TODO not threadsafe
		{
			MaxDistance = bIsProxy ? MaxDepenetrationWithPawnAsProxy : MaxDepenetrationWithPawn;
		}

		Result = Result.GetClampedToMaxSize(MaxDistance);
	}

	return Result;
}

bool FCharacterMovementComponentAsyncInput::ResolvePenetration(const FVector& ProposedAdjustment, const FHitResult& Hit, const FQuat& NewRotation, FCharacterMovementComponentAsyncOutput& Output) const
{

	// SceneComponent can't be in penetration, so this function really only applies to PrimitiveComponent.
	const FVector Adjustment = ConstrainDirectionToPlane(ProposedAdjustment);
	if (!Adjustment.IsZero() && UpdatedComponentInput->UpdatedComponent)
	{
	//	QUICK_SCOPE_CYCLE_COUNTER(STAT_MovementComponent_ResolvePenetration);

		// See if we can fit at the adjusted location without overlapping anything.
		//AActor* ActorOwner = UpdatedComponent->GetOwner();
		/*if (!ActorOwner)
		{
			return false;
		}*/

		/*UE_LOG(LogMovement, Verbose, TEXT("ResolvePenetration: %s.%s at location %s inside %s.%s at location %s by %.3f (netmode: %d)"),
			*ActorOwner->GetName(),
			*UpdatedComponent->GetName(),
			*UpdatedComponent->GetComponentLocation().ToString(),
			*GetNameSafe(Hit.GetActor()),
			*GetNameSafe(Hit.GetComponent()),
			Hit.Component.IsValid() ? *Hit.GetComponent()->GetComponentLocation().ToString() : TEXT("<unknown>"),
			Hit.PenetrationDepth,
			(uint32)GetNetMode());*/

		// We really want to make sure that precision differences or differences between the overlap test and sweep tests don't put us into another overlap,
		// so make the overlap test a bit more restrictive.
		//const float OverlapInflation = MovementComponentCVars::PenetrationOverlapCheckInflation;
		// (Applying this to collision shape in inputs)


		//FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(MovementOverlapTest), false, Owner);
		bool bEncroached = World->OverlapBlockingTestByChannel(Hit.TraceStart + Adjustment, NewRotation, CollisionChannel, UpdatedComponentInput->CollisionShape, UpdatedComponentInput->MoveComponentQueryParams, UpdatedComponentInput->MoveComponentCollisionResponseParams);
		//bool bEncroached = OverlapTest(Hit.TraceStart + Adjustment, NewRotationQuat, UpdatedPrimitive->GetCollisionObjectType(), UpdatedPrimitive->GetCollisionShape(OverlapInflation), ActorOwner);
		if (!bEncroached)
		{
			// Move without sweeping.
			MoveUpdatedComponent(Adjustment, NewRotation, false, Output, nullptr, ETeleportType::TeleportPhysics);
			//UE_LOG(LogMovement, Verbose, TEXT("ResolvePenetration:   teleport by %s"), *Adjustment.ToString());
			
			
			// This line is from UCharacterMovementComponent::ResolvePenetrationImpl
			// Rest of this func is from MovementComponent impl
			Output.bJustTeleported = true;
			return true;
		}
		else
		{
			// Disable MOVECOMP_NeverIgnoreBlockingOverlaps if it is enabled, otherwise we wouldn't be able to sweep out of the object to fix the penetration.
			TGuardValue<EMoveComponentFlags> ScopedFlagRestore(Output.MoveComponentFlags, EMoveComponentFlags(Output.MoveComponentFlags & (~MOVECOMP_NeverIgnoreBlockingOverlaps)));

			// Try sweeping as far as possible...
			FHitResult SweepOutHit(1.f);
			bool bMoved = MoveUpdatedComponent(Adjustment, NewRotation, true, Output, &SweepOutHit, ETeleportType::TeleportPhysics);
			//UE_LOG(LogMovement, Verbose, TEXT("ResolvePenetration:   sweep by %s (success = %d)"), *Adjustment.ToString(), bMoved);

			// Still stuck?
			if (!bMoved && SweepOutHit.bStartPenetrating)
			{
				// Combine two MTD results to get a new direction that gets out of multiple surfaces.
				const FVector SecondMTD = GetPenetrationAdjustment(SweepOutHit);
				const FVector CombinedMTD = Adjustment + SecondMTD;
				if (SecondMTD != Adjustment && !CombinedMTD.IsZero())
				{
					bMoved = MoveUpdatedComponent(CombinedMTD, NewRotation, true, Output, nullptr, ETeleportType::TeleportPhysics);
					//UE_LOG(LogMovement, Verbose, TEXT("ResolvePenetration:   sweep by %s (MTD combo success = %d)"), *CombinedMTD.ToString(), bMoved);
				}
			}

			// Still stuck?
			if (!bMoved)
			{
				// Try moving the proposed adjustment plus the attempted move direction. This can sometimes get out of penetrations with multiple objects
				const FVector MoveDelta = ConstrainDirectionToPlane(Hit.TraceEnd - Hit.TraceStart);
				if (!MoveDelta.IsZero())
				{
					bMoved = MoveUpdatedComponent(Adjustment + MoveDelta, NewRotation, true, Output, nullptr, ETeleportType::TeleportPhysics);
					//UE_LOG(LogMovement, Verbose, TEXT("ResolvePenetration:   sweep by %s (adjusted attempt success = %d)"), *(Adjustment + MoveDelta).ToString(), bMoved);

					// Finally, try the original move without MTD adjustments, but allowing depenetration along the MTD normal.
					// This was blocked because MOVECOMP_NeverIgnoreBlockingOverlaps was true for the original move to try a better depenetration normal, but we might be running in to other geometry in the attempt.
					// This won't necessarily get us all the way out of penetration, but can in some cases and does make progress in exiting the penetration.
					if (!bMoved && FVector::DotProduct(MoveDelta, Adjustment) > 0.f)
					{
						bMoved = MoveUpdatedComponent(MoveDelta, NewRotation, true, Output, nullptr, ETeleportType::TeleportPhysics);
						//UE_LOG(LogMovement, Verbose, TEXT("ResolvePenetration:   sweep by %s (Original move, attempt success = %d)"), *(MoveDelta).ToString(), bMoved);
					}
				}
			}

			// This line is from UCharacterMovementComponent::ResolvePenetrationImpl
			// Rest of this func is from MovementComponent impl
			Output.bJustTeleported |= bMoved;

			return bMoved;
		}
	}

	return false;
}

FVector FCharacterMovementComponentAsyncInput::MoveComponent_GetPenetrationAdjustment(FHitResult& Hit) const
{
	if (!Hit.bStartPenetrating)
	{
		return FVector::ZeroVector;
	}

	FVector Result;
	const float PullBackDistance = FMath::Abs(MovementComponentCVars::PenetrationPullbackDistance);
	const float PenetrationDepth = (Hit.PenetrationDepth > 0.f ? Hit.PenetrationDepth : 0.125f);

	Result = Hit.Normal * (PenetrationDepth + PullBackDistance);

	return ConstrainDirectionToPlane(Result);
}

float FCharacterMovementComponentAsyncInput::MoveComponent_SlideAlongSurface(const FVector& Delta, float Time, const FVector& Normal, FHitResult& Hit, FCharacterMovementComponentAsyncOutput& Output, bool bHandleImpact) const
{
	if (!Hit.bBlockingHit)
	{
		return 0.f;
	}

	float PercentTimeApplied = 0.f;
	const FVector OldHitNormal = Normal;

	FVector SlideDelta = ComputeSlideVector(Delta, Time, Normal, Hit, Output);

	if ((SlideDelta | Delta) > 0.f)
	{
		const FQuat Rotation = UpdatedComponentInput->GetRotation();
		SafeMoveUpdatedComponent(SlideDelta, Rotation, true, Hit, Output);

		const float FirstHitPercent = Hit.Time;
		PercentTimeApplied = FirstHitPercent;
		if (Hit.IsValidBlockingHit())
		{
			// Notify first impact
			if (bHandleImpact)
			{
				HandleImpact(Hit, Output, FirstHitPercent * Time, SlideDelta);
			}

			// Compute new slide normal when hitting multiple surfaces.
			TwoWallAdjust(SlideDelta, Hit, OldHitNormal, Output);

			// Only proceed if the new direction is of significant length and not in reverse of original attempted move.
			if (!SlideDelta.IsNearlyZero(1e-3f) && (SlideDelta | Delta) > 0.f)
			{
				// Perform second move
				SafeMoveUpdatedComponent(SlideDelta, Rotation, true, Hit, Output);
				const float SecondHitPercent = Hit.Time * (1.f - FirstHitPercent);
				PercentTimeApplied += SecondHitPercent;

				// Notify second impact
				if (bHandleImpact && Hit.bBlockingHit)
				{
					HandleImpact(Hit, Output, SecondHitPercent * Time, SlideDelta);
				}
			}
		}

		return FMath::Clamp(PercentTimeApplied, 0.f, 1.f);
	}

	return 0.f;
}

FVector FCharacterMovementComponentAsyncInput::MoveComponent_ComputeSlideVector(const FVector& Delta, const float Time, const FVector& Normal, const FHitResult& Hit, FCharacterMovementComponentAsyncOutput& Output) const
{
	if (!bConstrainToPlane)
	{
		return FVector::VectorPlaneProject(Delta, Normal) * Time;
	}
	else
	{
		const FVector ProjectedNormal = ConstrainNormalToPlane(Normal);
		return FVector::VectorPlaneProject(Delta, ProjectedNormal) * Time;
	}
}

bool FUpdatedComponentAsyncInput::MoveComponent(const FVector& Delta, const FQuat& NewRotationQuat, bool bSweep, FHitResult* OutHit,  EMoveComponentFlags MoveFlags, ETeleportType Teleport,  const FCharacterMovementComponentAsyncInput& Input, FCharacterMovementComponentAsyncOutput& Output) const 
{
	// what does primitive component do?
	const FVector TraceStart = GetPosition();
	const FVector TraceEnd = TraceStart + Delta;
	float DeltaSizeSq = (TraceEnd - TraceStart).SizeSquared();				// Recalc here to account for precision loss of float addition
	const FQuat InitialRotationQuat = GetRotation();

	// ComponentSweepMulti does nothing if moving < KINDA_SMALL_NUMBER in distance, so it's important to not try to sweep distances smaller than that. 
	const float MinMovementDistSq = (bSweep ? FMath::Square(4.f * UE_KINDA_SMALL_NUMBER) : 0.f);
	if (DeltaSizeSq <= MinMovementDistSq)
	{
		// Skip if no vector or rotation.
		if (NewRotationQuat.Equals(InitialRotationQuat, SCENECOMPONENT_QUAT_TOLERANCE))
		{
			// copy to optional output param
			if (OutHit)
			{
				OutHit->Init(TraceStart, TraceEnd);
			}
			return true;
		}
		DeltaSizeSq = 0.f;
	}

	const bool bSkipPhysicsMove = ((MoveFlags & MOVECOMP_SkipPhysicsMove) != MOVECOMP_NoFlags);

	// WARNING: HitResult is only partially initialized in some paths. All data is valid only if bFilledHitResult is true.
	FHitResult BlockingHit(NoInit);
	BlockingHit.bBlockingHit = false;
	BlockingHit.Time = 1.f;
	bool bFilledHitResult = false;
	bool bMoved = false;
	bool bIncludesOverlapsAtEnd = false;
	bool bRotationOnly = false;
	TInlineOverlapInfoArray PendingOverlaps;
	//AActor* const Actor = Input.Owner;

	if (!bSweep)
	{
		// TODO fix not setting bMoved here

		// not sweeping, just go directly to the new transform
		//bMoved = InternalSetWorldLocationAndRotation(TraceEnd, NewRotationQuat, bSkipPhysicsMove, Teleport);
		SetPosition(TraceEnd);
		SetRotation(NewRotationQuat);
		bRotationOnly = (DeltaSizeSq == 0);
		bIncludesOverlapsAtEnd = bRotationOnly && (AreSymmetricRotations(InitialRotationQuat, NewRotationQuat, Input.UpdatedComponentInput->Scale)) && bIsQueryCollisionEnabled;
	}
	else
	{
		TArray<FHitResult> Hits;
		FVector NewLocation = TraceStart;

		// Perform movement collision checking if needed for this actor.
		if (bIsQueryCollisionEnabled && (DeltaSizeSq > 0.f))
		{
/*#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			if (!IsRegistered())
			{
				if (Actor)
				{
					ensureMsgf(IsRegistered(), TEXT("%s MovedComponent %s not initialized deleteme %d"), *Actor->GetName(), *GetName(), !IsValid(Actor));
				}
				else
				{ //-V523
					ensureMsgf(IsRegistered(), TEXT("MovedComponent %s not initialized"), *GetFullName());
				}
			}
#endif

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST) && PERF_MOVECOMPONENT_STATS
			MoveTimer.bDidLineCheck = true;
#endif */

			// now capturing params when building inputs.
			bool const bHadBlockingHit = Input.World->ComponentSweepMulti(Hits, UpdatedComponent, TraceStart, TraceEnd, InitialRotationQuat, MoveComponentQueryParams);
			

			/*using namespace Chaos;
			FSingleParticlePhysicsProxy* PhysicsActorHandle = Output.UpdatedComponent.PhysicsActorHandle;
			FPBDRigidsSolver* Solver = PhysicsActorHandle->GetSolver<Chaos::FPBDRigidsSolver>();
			FPBDRigidsEvolution* Evolution = Solver->GetEvolution();
			ensure(Evolution);
			ISpatialAccelerationCollection<FAccelerationStructureHandle, FReal, 3>* AccelerationStructure = Evolution->GetSpatialAcceleration();*/

			// how to query about world.

			if (Hits.Num() > 0)
			{
				const float DeltaSize = FMath::Sqrt(DeltaSizeSq);
				for (int32 HitIdx = 0; HitIdx < Hits.Num(); HitIdx++)
				{
					FUpdatedComponentAsyncInput::PullBackHit(Hits[HitIdx], TraceStart, TraceEnd, DeltaSize);
				}
			}

			// If we had a valid blocking hit, store it.
			// If we are looking for overlaps, store those as well.
			int32 FirstNonInitialOverlapIdx = INDEX_NONE;
			if (bHadBlockingHit || (bGatherOverlaps))
			{
				int32 BlockingHitIndex = INDEX_NONE;
				float BlockingHitNormalDotDelta = UE_BIG_NUMBER;
				for (int32 HitIdx = 0; HitIdx < Hits.Num(); HitIdx++)
				{
					const FHitResult& TestHit = Hits[HitIdx];

					if (TestHit.bBlockingHit)
					{
						// TODO make this threadsafe.
						// This ignores if query/hit actor are based on each other. Can we determine this on PT?
						if (!FUpdatedComponentAsyncInput::ShouldIgnoreHitResult(Input.World, TestHit, Delta, nullptr/*Actor*/, MoveFlags))
						{
							if (TestHit.bStartPenetrating)
							{
								// We may have multiple initial hits, and want to choose the one with the normal most opposed to our movement.
								const float NormalDotDelta = (TestHit.ImpactNormal | Delta);
								if (NormalDotDelta < BlockingHitNormalDotDelta)
								{
									BlockingHitNormalDotDelta = NormalDotDelta;
									BlockingHitIndex = HitIdx;
								}
							}
							else if (BlockingHitIndex == INDEX_NONE)
							{
								// First non-overlapping blocking hit should be used, if an overlapping hit was not.
								// This should be the only non-overlapping blocking hit, and last in the results.
								BlockingHitIndex = HitIdx;
								break;
							}
						}
					}
					else if (bGatherOverlaps)//GetGenerateOverlapEvents() || bForceGatherOverlaps)
					{
						UPrimitiveComponent* OverlapComponent = TestHit.Component.Get();

						// Overlaps are speculative, this flag will be chcked when applying outputs.
						if (OverlapComponent && (true/*OverlapComponent->GetGenerateOverlapEvents()*/ || bForceGatherOverlaps))
						{
							if (!FUpdatedComponentAsyncInput::ShouldIgnoreOverlapResult(Input.World, nullptr/*Actor*/, *UpdatedComponent, TestHit.GetActor(), *OverlapComponent /*bCheckOverlapFlags=!bForceGatherOverlaps*/))
							{
								// don't process touch events after initial blocking hits
								if (BlockingHitIndex >= 0 && TestHit.Time > Hits[BlockingHitIndex].Time)
								{
									break;
								}

								if (FirstNonInitialOverlapIdx == INDEX_NONE && TestHit.Time > 0.f)
								{
									// We are about to add the first non-initial overlap.
									FirstNonInitialOverlapIdx = PendingOverlaps.Num();
								}

								// cache touches
								Output.UpdatedComponentOutput.AddUniqueSpeculativeOverlap(FOverlapInfo(TestHit));
							}
						}
					}
				}

				// Update blocking hit, if there was a valid one.
				if (BlockingHitIndex >= 0)
				{
					BlockingHit = Hits[BlockingHitIndex];
					bFilledHitResult = true;
				}
			}

			// Update NewLocation based on the hit result
			if (!BlockingHit.bBlockingHit)
			{
				NewLocation = TraceEnd;
			}
			else
			{
				check(bFilledHitResult);
				NewLocation = TraceStart + (BlockingHit.Time * (TraceEnd - TraceStart));

				// Sanity check
				const FVector ToNewLocation = (NewLocation - TraceStart);
				if (ToNewLocation.SizeSquared() <= MinMovementDistSq)
				{
					// We don't want really small movements to put us on or inside a surface.
					NewLocation = TraceStart;
					BlockingHit.Time = 0.f;

					// Remove any pending overlaps after this point, we are not going as far as we swept.
					if (FirstNonInitialOverlapIdx != INDEX_NONE)
					{
						PendingOverlaps.SetNum(FirstNonInitialOverlapIdx, EAllowShrinking::No);
					}
				}
			}

			bIncludesOverlapsAtEnd = AreSymmetricRotations(InitialRotationQuat, NewRotationQuat, Input.UpdatedComponentInput->Scale);

/*#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			if (UCheatManager::IsDebugCapsuleSweepPawnEnabled() && BlockingHit.bBlockingHit && !IsZeroExtent())
			{
				// this is sole debug purpose to find how capsule trace information was when hit 
				// to resolve stuck or improve our movement system - To turn this on, use DebugCapsuleSweepPawn
				APawn const* const ActorPawn = (Actor ? Cast<APawn>(Actor) : NULL);
				if (ActorPawn && ActorPawn->Controller && ActorPawn->Controller->IsLocalPlayerController())
				{
					APlayerController const* const PC = CastChecked<APlayerController>(ActorPawn->Controller);
					if (PC->CheatManager)
					{
						FVector CylExtent = ActorPawn->GetSimpleCollisionCylinderExtent() * FVector(1.001f, 1.001f, 1.0f);
						FCollisionShape CapsuleShape = FCollisionShape::MakeCapsule(CylExtent);
						PC->CheatManager->AddCapsuleSweepDebugInfo(TraceStart, TraceEnd, BlockingHit.ImpactPoint, BlockingHit.Normal, BlockingHit.ImpactNormal, BlockingHit.Location, CapsuleShape.GetCapsuleHalfHeight(), CapsuleShape.GetCapsuleRadius(), true, (BlockingHit.bStartPenetrating && BlockingHit.bBlockingHit) ? true : false);
					}
				}
			}
#endif*/
		}
		else if (DeltaSizeSq > 0.f)
		{
			// apply move delta even if components has collisions disabled
			NewLocation += Delta;
			bIncludesOverlapsAtEnd = false;
		}
		else if (DeltaSizeSq == 0.f && bIsQueryCollisionEnabled)
		{
			bIncludesOverlapsAtEnd = AreSymmetricRotations(InitialRotationQuat, NewRotationQuat, Input.UpdatedComponentInput->Scale);
			bRotationOnly = true;
		}

		SetPosition(NewLocation);
		SetRotation(NewRotationQuat);
		// Update the location.  This will teleport any child components as well (not sweep).
		//bMoved = InternalSetWorldLocationAndRotation(NewLocation, NewRotationQuat, bSkipPhysicsMove, Teleport);

		// TODO compute and diff location/rotation
		bMoved = true;
	}


	// TODO actually set bMoved correctly above
	// Handle overlap notifications.
	if (bMoved)
	{
		// TODO DeferredMovementUpdates
		if (false)//IsDeferringMovementUpdates())
		{
	/*		// Defer UpdateOverlaps until the scoped move ends.
			FScopedMovementUpdate* ScopedUpdate = GetCurrentScopedMovement();
			if (bRotationOnly && bIncludesOverlapsAtEnd)
			{
				ScopedUpdate->KeepCurrentOverlapsAfterRotation(bSweep);
			}
			else
			{
				ScopedUpdate->AppendOverlapsAfterMove(PendingOverlaps, bSweep, bIncludesOverlapsAtEnd);
			}*/
		}
		else
		{
			// TODO Overlaps
			/*if (bIncludesOverlapsAtEnd)
			{
				TInlineOverlapInfoArray OverlapsAtEndLocation;
				bool bHasEndOverlaps = false;
				if (bRotationOnly)
				{
					bHasEndOverlaps = ConvertRotationOverlapsToCurrentOverlaps(OverlapsAtEndLocation, OverlappingComponents);
				}
				else
				{
					bHasEndOverlaps = ConvertSweptOverlapsToCurrentOverlaps(OverlapsAtEndLocation, PendingOverlaps, 0, GetComponentLocation(), GetComponentQuat());
				}
				TOverlapArrayView PendingOverlapsView(PendingOverlaps);
				TOverlapArrayView OverlapsAtEndView(OverlapsAtEndLocation);
				UpdateOverlaps(&PendingOverlapsView, true, bHasEndOverlaps ? &OverlapsAtEndView : nullptr);
			}
			else
			{
				TOverlapArrayView PendingOverlapsView(PendingOverlaps);
				UpdateOverlaps(&PendingOverlapsView, true, nullptr);
			}*/
		}
	}

	// Handle blocking hit notifications. Avoid if pending kill (which could happen after overlaps).
	const bool bAllowHitDispatch = !BlockingHit.bStartPenetrating || !(MoveFlags & MOVECOMP_DisableBlockingOverlapDispatch);
	if (BlockingHit.bBlockingHit && bAllowHitDispatch/* && IsValid(this)*/)
	{
		check(bFilledHitResult);
		/*if (IsDeferringMovementUpdates()) // TODO DeferredMovementUpdates
		{
			FScopedMovementUpdate* ScopedUpdate = GetCurrentScopedMovement();
			ScopedUpdate->AppendBlockingHitAfterMove(BlockingHit);
		}
		else
		{
			DispatchBlockingHit(*Actor, BlockingHit);
		}*/

		// TODO Blocking Hit Notification
	}

/*#if defined(PERF_SHOW_MOVECOMPONENT_TAKING_LONG_TIME) || LOOKING_FOR_PERF_ISSUES
	UNCLOCK_CYCLES(MoveCompTakingLongTime);
	const float MSec = FPlatformTime::ToMilliseconds(MoveCompTakingLongTime);
	if (MSec > PERF_SHOW_MOVECOMPONENT_TAKING_LONG_TIME_AMOUNT)
	{
		if (GetOwner())
		{
			UE_LOG(LogPrimitiveComponent, Log, TEXT("%10f executing MoveComponent for %s owned by %s"), MSec, *GetName(), *GetOwner()->GetFullName());
		}
		else
		{
			UE_LOG(LogPrimitiveComponent, Log, TEXT("%10f executing MoveComponent for %s"), MSec, *GetFullName());
		}
	}
#endif*/

	// copy to optional output param
	if (OutHit)
	{
		if (bFilledHitResult)
		{
			*OutHit = BlockingHit;
		}
		else
		{
			OutHit->Init(TraceStart, TraceEnd);
		}
	}

	return bMoved;
}

bool FUpdatedComponentAsyncInput::AreSymmetricRotations(const FQuat& A, const FQuat& B, const FVector& Scale3D) const
{
	if (Scale3D.X != Scale3D.Y)
	{
		return false;
	}

	const FVector AUp = A.GetAxisZ();
	const FVector BUp = B.GetAxisZ();
	return AUp.Equals(BUp);
}

// TODO Dedupe with Primitive Component
void FUpdatedComponentAsyncInput::PullBackHit(FHitResult& Hit, const FVector& Start, const FVector& End, const float Dist)
{
	const float DesiredTimeBack = FMath::Clamp(0.1f, 0.1f / Dist, 1.f / Dist) + 0.001f;
	Hit.Time = FMath::Clamp(Hit.Time - DesiredTimeBack, 0.f, 1.f);
}

bool FUpdatedComponentAsyncInput::ShouldCheckOverlapFlagToQueueOverlaps(const UPrimitiveComponent& ThisComponent)
{
	const FScopedMovementUpdate* CurrentUpdate = ThisComponent.GetCurrentScopedMovement();
	if (CurrentUpdate)
	{
		return CurrentUpdate->RequiresOverlapsEventFlag();
	}
	// By default we require the GetGenerateOverlapEvents() to queue up overlaps, since we require it to trigger events.
	return true;
}

bool FUpdatedComponentAsyncInput::ShouldIgnoreHitResult(const UWorld* InWorld, FHitResult const& TestHit, FVector const& MovementDirDenormalized, const AActor* MovingActor, EMoveComponentFlags MoveFlags)
{
	if (TestHit.bBlockingHit)
	{
		// check "ignore bases" functionality
		if ((MoveFlags & MOVECOMP_IgnoreBases) && false/*MovingActor*/)	//we let overlap components go through because their overlap is still needed and will cause beginOverlap/endOverlap events
		{
			// ignore if there's a base relationship between moving actor and hit actor
			AActor const* const HitActor = TestHit.GetActor();
			if (HitActor)
			{
				// TODO Threadsafe, must replace. TODO MovementBase
				if (MovingActor->IsBasedOnActor(HitActor) || HitActor->IsBasedOnActor(MovingActor))
				{
					return true;
				}
			}
		}

		// If we started penetrating, we may want to ignore it if we are moving out of penetration.
		// This helps prevent getting stuck in walls.
		if ((TestHit.Distance < PrimitiveComponentCVars::HitDistanceToleranceCVar || TestHit.bStartPenetrating) && !(MoveFlags & MOVECOMP_NeverIgnoreBlockingOverlaps))
		{
			const float DotTolerance = PrimitiveComponentCVars::InitialOverlapToleranceCVar;

			// Dot product of movement direction against 'exit' direction
			const FVector MovementDir = MovementDirDenormalized.GetSafeNormal();
			const float MoveDot = (TestHit.ImpactNormal | MovementDir);

			const bool bMovingOut = MoveDot > DotTolerance;

			/*#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
						{
							if (CVarShowInitialOverlaps != 0)
							{
								UE_LOG(LogTemp, Log, TEXT("Overlapping %s Dir %s Dot %f Normal %s Depth %f"), *GetNameSafe(TestHit.Component.Get()), *MovementDir.ToString(), MoveDot, *TestHit.ImpactNormal.ToString(), TestHit.PenetrationDepth);
								DrawDebugDirectionalArrow(InWorld, TestHit.TraceStart, TestHit.TraceStart + 30.f * TestHit.ImpactNormal, 5.f, bMovingOut ? FColor(64, 128, 255) : FColor(255, 64, 64), false, 4.f);
								if (TestHit.PenetrationDepth > KINDA_SMALL_NUMBER)
								{
									DrawDebugDirectionalArrow(InWorld, TestHit.TraceStart, TestHit.TraceStart + TestHit.PenetrationDepth * TestHit.Normal, 5.f, FColor(64, 255, 64), false, 4.f);
								}
							}
						}
			#endif*/

			// If we are moving out, ignore this result!
			if (bMovingOut)
			{
				return true;
			}
		}
	}

	return false;
}

bool FUpdatedComponentAsyncInput::ShouldIgnoreOverlapResult(const UWorld* World, const AActor* ThisActor, const UPrimitiveComponent& ThisComponent, const AActor* OtherActor, const UPrimitiveComponent& OtherComponent)
{
	if (&ThisComponent == &OtherComponent)
	{
		return true;
	}

	// Not supported, is handled by caller already anyway.
	/*if (bCheckOverlapFlags)
	{
		// Both components must set GetGenerateOverlapEvents()
		if (!ThisComponent.GetGenerateOverlapEvents() || !OtherComponent.GetGenerateOverlapEvents())
		{
			return true;
		}
	}*/

	// We're passing in null for ThisActor as we don't have that data, ensuring owner exists when filling inputs anyway.
	if (/*!ThisActor || */!OtherActor)
	{
		return true;
	}

	// Cannot read this on PT, check when applying speculative overlaps.
	if (!World)// || OtherActor == World->GetWorldSettings() || !OtherActor->IsActorInitialized())
	{
		return true;
	}

	return false;
}

void FUpdatedComponentAsyncInput::SetPosition(const FVector& InPosition) const
{
	if (PhysicsHandle->GetPhysicsThreadAPI() == nullptr)
	{
		return;
	}

	// This is gross, allows moving kinematics from physics thread. 
	// Eventually will have better support for this and can simplify.
	if (auto Rigid = PhysicsHandle->GetHandle_LowLevel()->CastToRigidParticle())
	{
		PhysicsHandle->GetPhysicsThreadAPI()->SetX(InPosition);
		Rigid->SetP(InPosition); // If we don't set P, UpdateParticlePosition in evolution may stomp over X with P().

		// Kinematics do not normal marshall changes back to game thread, mark dirty to ensure game thread
		// gets change in position.
		if (Rigid->ObjectState() == Chaos::EObjectStateType::Kinematic)
		{
			PhysicsHandle->GetSolver<Chaos::FPBDRigidsSolver>()->GetParticles().MarkTransientDirtyParticle(Rigid);
		}
	}
	else
	{
		ensure(false);
	}
}

FVector FUpdatedComponentAsyncInput::GetPosition() const
{
	if (PhysicsHandle && PhysicsHandle->GetPhysicsThreadAPI())
	{
		return PhysicsHandle->GetPhysicsThreadAPI()->X();
	}

	return FVector::ZeroVector;
}

void FUpdatedComponentAsyncInput::SetRotation(const FQuat& InRotation) const
{
	if (PhysicsHandle->GetPhysicsThreadAPI() == nullptr)
	{
		return;
	}

	// This is gross, allows moving kinematics from physics thread. 
	// Eventually will have better support for this and can simplify.
	if (auto Rigid = PhysicsHandle->GetHandle_LowLevel()->CastToRigidParticle())
	{
		PhysicsHandle->GetPhysicsThreadAPI()->SetR(InRotation);
		Rigid->SetQ(InRotation); // If we don't set Q, UpdateParticlePosition in evolution may stomp over R with Q().

		// Kinematics do not normal marshall changes back to game thread, mark dirty to ensure game thread
		// gets change in position.
		if (Rigid->ObjectState() == Chaos::EObjectStateType::Kinematic)
		{
			PhysicsHandle->GetSolver<Chaos::FPBDRigidsSolver>()->GetParticles().MarkTransientDirtyParticle(Rigid);
		}
	}
	else
	{
		ensure(false);
	}
}

FQuat FUpdatedComponentAsyncInput::GetRotation() const
{
	if (PhysicsHandle && PhysicsHandle->GetPhysicsThreadAPI())
	{
		return PhysicsHandle->GetPhysicsThreadAPI()->R();
	}

	return FQuat::Identity;
}


void FCharacterMovementComponentAsyncInput::HandleImpact(const FHitResult& Impact, FCharacterMovementComponentAsyncOutput& Output, float TimeSlice, const FVector& MoveDelta) const
{
	//SCOPE_CYCLE_COUNTER(STAT_CharHandleImpact);

	// TODO HandleImpact
	/*if (input.Owner)
	{
	//	CharacterOwner->MoveBlockedBy(Impact);
	}

	IPathFollowingAgentInterface* PFAgent = GetPathFollowingAgent();
	if (PFAgent)
	{
		// Also notify path following!
	//	PFAgent->OnMoveBlockedBy(Impact);
	}*/

	/*APawn* OtherPawn = Cast<APawn>(Impact.GetActor());
	if (OtherPawn)
	{
		NotifyBumpedPawn(OtherPawn);
	}*/

	// TODO EnablePhysicsInteraction
	/*if (bEnablePhysicsInteraction)
	{
		const FVector ForceAccel = Acceleration + (IsFalling() ? FVector(0.f, 0.f, GetGravityZ()) : FVector::ZeroVector);
		ApplyImpactPhysicsForces(Impact, ForceAccel, Velocity);
	}*/
}

float FCharacterMovementComponentAsyncInput::SlideAlongSurface(const FVector& Delta, float Time, const FVector& InNormal, FHitResult& Hit, bool bHandleImpact, FCharacterMovementComponentAsyncOutput& Output) const 
{
	if (!Hit.bBlockingHit)
	{
		return 0.f;
	}

	FVector Normal(InNormal);
	if (IsMovingOnGround(Output))
	{
		// We don't want to be pushed up an unwalkable surface.
		if (Normal.Z > 0.f)
		{
			if (!IsWalkable(Hit))
			{
				Normal = Normal.GetSafeNormal2D();
			}
		}
		else if (Normal.Z < -UE_KINDA_SMALL_NUMBER)
		{
			// Don't push down into the floor when the impact is on the upper portion of the capsule.
			if (Output.CurrentFloor.FloorDist < UCharacterMovementComponent::MIN_FLOOR_DIST && Output.CurrentFloor.bBlockingHit)
			{
				const FVector FloorNormal = Output.CurrentFloor.HitResult.Normal;
				const bool bFloorOpposedToMovement = (Delta | FloorNormal) < 0.f && (FloorNormal.Z < 1.f - UE_DELTA);
				if (bFloorOpposedToMovement)
				{
					Normal = FloorNormal;
				}

				Normal = Normal.GetSafeNormal2D();
			}
		}
	}

	return MoveComponent_SlideAlongSurface(Delta, Time, Normal, Hit, Output, bHandleImpact);
}

FVector FCharacterMovementComponentAsyncInput::ComputeSlideVector(const FVector& Delta, const float Time, const FVector& Normal, const FHitResult& Hit, FCharacterMovementComponentAsyncOutput& Output) const
{
	FVector Result = MoveComponent_ComputeSlideVector(Delta, Time, Normal, Hit, Output);

	// prevent boosting up slopes
	if (IsFalling(Output))
	{
		Result = HandleSlopeBoosting(Result, Delta, Time, Normal, Hit, Output);
	}

	return Result;
}

FVector FCharacterMovementComponentAsyncInput::HandleSlopeBoosting(const FVector& SlideResult, const FVector& Delta, const float Time, const FVector& Normal, const FHitResult& Hit, FCharacterMovementComponentAsyncOutput& Output) const
{
	FVector Result = SlideResult;

	if (Result.Z > 0.f)
	{
		// Don't move any higher than we originally intended.
		const float ZLimit = Delta.Z * Time;
		if (Result.Z - ZLimit > UE_KINDA_SMALL_NUMBER)
		{
			if (ZLimit > 0.f)
			{
				// Rescale the entire vector (not just the Z component) otherwise we change the direction and likely head right back into the impact.
				const float UpPercent = ZLimit / Result.Z;
				Result *= UpPercent;
			}
			else
			{
				// We were heading down but were going to deflect upwards. Just make the deflection horizontal.
				Result = FVector::ZeroVector;
			}

			// Make remaining portion of original result horizontal and parallel to impact normal.
			const FVector RemainderXY = (SlideResult - Result) * FVector(1.f, 1.f, 0.f);
			const FVector NormalXY = Normal.GetSafeNormal2D();
			const FVector Adjust = MoveComponent_ComputeSlideVector(RemainderXY, 1.f, NormalXY, Hit, Output); //Super::ComputeSlideVector(RemainderXY, 1.f, NormalXY, Hit);
			Result += Adjust;
		}
	}

	return Result;
}

void FCharacterMovementComponentAsyncInput::OnCharacterStuckInGeometry(const FHitResult* Hit, FCharacterMovementComponentAsyncOutput& Output) const 
{
	/*if (CharacterMovementCVars::StuckWarningPeriod >= 0)
	{
		const UWorld* MyWorld = World;
		const float RealTimeSeconds = MyWorld->GetRealTimeSeconds();
		if ((RealTimeSeconds - LastStuckWarningTime) >= CharacterMovementCVars::StuckWarningPeriod)
		{
			LastStuckWarningTime = RealTimeSeconds;
			if (Hit == nullptr)
			{
				UE_LOG(LogCharacterMovement, Log, TEXT("%s is stuck and failed to move! (%d other events since notify)"), *CharacterOwner->GetName(), StuckWarningCountSinceNotify);
			}
			else
			{
				UE_LOG(LogCharacterMovement, Log, TEXT("%s is stuck and failed to move! Velocity: X=%3.2f Y=%3.2f Z=%3.2f Location: X=%3.2f Y=%3.2f Z=%3.2f Normal: X=%3.2f Y=%3.2f Z=%3.2f PenetrationDepth:%.3f Actor:%s Component:%s BoneName:%s (%d other events since notify)"),
					*GetNameSafe(CharacterOwner),
					Velocity.X, Velocity.Y, Velocity.Z,
					Hit->Location.X, Hit->Location.Y, Hit->Location.Z,
					Hit->Normal.X, Hit->Normal.Y, Hit->Normal.Z,
					Hit->PenetrationDepth,
					*GetNameSafe(Hit->GetActor()),
					*GetNameSafe(Hit->GetComponent()),
					Hit->BoneName.IsValid() ? *Hit->BoneName.ToString() : TEXT("None"),
					StuckWarningCountSinceNotify
				);
			}
			ensure(false);
			StuckWarningCountSinceNotify = 0;
		}
		else
		{
			StuckWarningCountSinceNotify += 1;
		}
	}*/

	// Don't update velocity based on our (failed) change in position this update since we're stuck.
	Output.bJustTeleported = true;
}

bool FCharacterMovementComponentAsyncInput::CanStepUp(const FHitResult& Hit, FCharacterMovementComponentAsyncOutput& Output) const
{
	if (!Hit.IsValidBlockingHit() || !bHasValidData || Output.MovementMode == MOVE_Falling)
	{
		return false;
	}

	// No component for "fake" hits when we are on a known good base.
	const UPrimitiveComponent* HitComponent = Hit.Component.Get();
	if (!HitComponent)
	{
		return true;
	}

	// TODO StepUp
	/*if (!HitComponent->CanCharacterStepUp(CharacterOwner))
	{
		return false;
	}*/

	// No actor for "fake" hits when we are on a known good base.
	const AActor* HitActor = Hit.GetActor();
	if (!HitActor)
	{
		return true;
	}

	/*if (!HitActor->CanBeBaseForCharacter(CharacterOwner))
	{
		return false;
	}*/

	return true;
}

bool FCharacterMovementComponentAsyncInput::StepUp(const FVector& GravDir, const FVector& Delta, const FHitResult& InHit, FCharacterMovementComponentAsyncOutput& Output, FStepDownResult* OutStepDownResult) const
{
//	SCOPE_CYCLE_COUNTER(STAT_CharStepUp);

	if (!CanStepUp(InHit, Output) || MaxStepHeight <= 0.f)
	{
		return false;
	}

	const FVector OldLocation = UpdatedComponentInput->GetPosition();
	float PawnRadius = Output.ScaledCapsuleRadius;
	float PawnHalfHeight = Output.ScaledCapsuleHalfHeight;
	//CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleSize(PawnRadius, PawnHalfHeight);

	// Don't bother stepping up if top of capsule is hitting something.
	const float InitialImpactZ = InHit.ImpactPoint.Z;
	if (InitialImpactZ > OldLocation.Z + (PawnHalfHeight - PawnRadius))
	{
		return false;
	}

	if (GravDir.IsZero())
	{
		return false;
	}

	FFindFloorResult& CurrentFloor = Output.CurrentFloor;
//	float MaxStepHeight = MaxStepHeight;

	// Gravity should be a normalized direction
	ensure(GravDir.IsNormalized());

	float StepTravelUpHeight = MaxStepHeight;
	float StepTravelDownHeight = StepTravelUpHeight;
	const float StepSideZ = -1.f * FVector::DotProduct(InHit.ImpactNormal, GravDir);
	float PawnInitialFloorBaseZ = OldLocation.Z - PawnHalfHeight;
	float PawnFloorPointZ = PawnInitialFloorBaseZ;

	if (IsMovingOnGround(Output) && CurrentFloor.IsWalkableFloor())
	{
		// Since we float a variable amount off the floor, we need to enforce max step height off the actual point of impact with the floor.
		const float FloorDist = FMath::Max(0.f, CurrentFloor.GetDistanceToFloor());
		PawnInitialFloorBaseZ -= FloorDist;
		StepTravelUpHeight = FMath::Max(StepTravelUpHeight - FloorDist, 0.f);
		StepTravelDownHeight = (MaxStepHeight + UCharacterMovementComponent::MAX_FLOOR_DIST * 2.f);

		const bool bHitVerticalFace = !IsWithinEdgeTolerance(InHit.Location, InHit.ImpactPoint, PawnRadius);
		if (!CurrentFloor.bLineTrace && !bHitVerticalFace)
		{
			PawnFloorPointZ = CurrentFloor.HitResult.ImpactPoint.Z;
		}
		else
		{
			// Base floor point is the base of the capsule moved down by how far we are hovering over the surface we are hitting.
			PawnFloorPointZ -= CurrentFloor.FloorDist;
		}
	}

	// Don't step up if the impact is below us, accounting for distance from floor.
	if (InitialImpactZ <= PawnInitialFloorBaseZ)
	{
		return false;
	}

	// TODO ScopedMovementUpdate
	// Scope our movement updates, and do not apply them until all intermediate moves are completed.
	//FScopedMovementUpdate ScopedStepUpMovement(UpdatedComponent, EScopedUpdate::DeferredUpdates);

	// step up - treat as vertical wall
	FHitResult SweepUpHit(1.f);
	const FQuat PawnRotation = UpdatedComponentInput->GetRotation();
	MoveUpdatedComponent(-GravDir * StepTravelUpHeight, PawnRotation, true, Output, &SweepUpHit);

	if (SweepUpHit.bStartPenetrating)
	{
		// Undo movement
		ensure(false);
	//	ScopedStepUpMovement.RevertMove();
		return false;
	}

	// step fwd
	FHitResult Hit(1.f);
	MoveUpdatedComponent(Delta, PawnRotation, true, Output, &Hit);

	// Check result of forward movement
	if (Hit.bBlockingHit)
	{
		if (Hit.bStartPenetrating)
		{
			// Undo movement
			//ScopedStepUpMovement.RevertMove();
			ensure(false);
			return false;
		}

		// If we hit something above us and also something ahead of us, we should notify about the upward hit as well.
		// The forward hit will be handled later (in the bSteppedOver case below).
		// In the case of hitting something above but not forward, we are not blocked from moving so we don't need the notification.
		if (SweepUpHit.bBlockingHit && Hit.bBlockingHit)
		{
			HandleImpact(SweepUpHit, Output);
		}

		// pawn ran into a wall
		HandleImpact(Hit, Output);
		if (IsFalling(Output))
		{
			return true;
		}

		// adjust and try again
		const float ForwardHitTime = Hit.Time;
		const float ForwardSlideAmount = SlideAlongSurface(Delta, 1.f - Hit.Time, Hit.Normal, Hit, true, Output);

		if (IsFalling(Output))
		{
			ensure(false);
		//	ScopedStepUpMovement.RevertMove();
			return false;
		}

		// If both the forward hit and the deflection got us nowhere, there is no point in this step up.
		if (ForwardHitTime == 0.f && ForwardSlideAmount == 0.f)
		{
			ensure(false);
			//ScopedStepUpMovement.RevertMove();
			return false;
		}
	}

	// Step down
	MoveUpdatedComponent(GravDir * StepTravelDownHeight, UpdatedComponentInput->GetRotation(), true, Output, &Hit);

	// If step down was initially penetrating abort the step up
	if (Hit.bStartPenetrating)
	{
		ensure(false);
		//ScopedStepUpMovement.RevertMove();
		return false;
	}

	FStepDownResult StepDownResult;
	if (Hit.IsValidBlockingHit())
	{
		// See if this step sequence would have allowed us to travel higher than our max step height allows.
		const float DeltaZ = Hit.ImpactPoint.Z - PawnFloorPointZ;
		if (DeltaZ > MaxStepHeight)
		{
			//UE_LOG(LogCharacterMovement, VeryVerbose, TEXT("- Reject StepUp (too high Height %.3f) up from floor base %f to %f"), DeltaZ, PawnInitialFloorBaseZ, NewLocation.Z);
			//ScopedStepUpMovement.RevertMove();
			ensure(false);
			return false;
		}

		// Reject unwalkable surface normals here.
		if (!IsWalkable(Hit))
		{
			// Reject if normal opposes movement direction
			const bool bNormalTowardsMe = (Delta | Hit.ImpactNormal) < 0.f;
			if (bNormalTowardsMe)
			{
				//UE_LOG(LogCharacterMovement, VeryVerbose, TEXT("- Reject StepUp (unwalkable normal %s opposed to movement)"), *Hit.ImpactNormal.ToString());
				//ScopedStepUpMovement.RevertMove();
				ensure(false);
				return false;
			}

			// Also reject if we would end up being higher than our starting location by stepping down.
			// It's fine to step down onto an unwalkable normal below us, we will just slide off. Rejecting those moves would prevent us from being able to walk off the edge.
			if (Hit.Location.Z > OldLocation.Z)
			{
				//UE_LOG(LogCharacterMovement, VeryVerbose, TEXT("- Reject StepUp (unwalkable normal %s above old position)"), *Hit.ImpactNormal.ToString());
				//ScopedStepUpMovement.RevertMove();
				ensure(false);
				return false;
			}
		}

		// Reject moves where the downward sweep hit something very close to the edge of the capsule. This maintains consistency with FindFloor as well.
		if (!IsWithinEdgeTolerance(Hit.Location, Hit.ImpactPoint, PawnRadius))
		{
			//UE_LOG(LogCharacterMovement, VeryVerbose, TEXT("- Reject StepUp (outside edge tolerance)"));
			//ScopedStepUpMovement.RevertMove();
			ensure(false);
			return false;
		}

		// Don't step up onto invalid surfaces if traveling higher.
		if (DeltaZ > 0.f && !CanStepUp(Hit, Output))
		{
			//UE_LOG(LogCharacterMovement, VeryVerbose, TEXT("- Reject StepUp (up onto surface with !CanStepUp())"));
			//ScopedStepUpMovement.RevertMove();
			ensure(false);
			return false;
		}

		// See if we can validate the floor as a result of this step down. In almost all cases this should succeed, and we can avoid computing the floor outside this method.
		if (OutStepDownResult != NULL)
		{
			FindFloor(UpdatedComponentInput->GetPosition(), StepDownResult.FloorResult, false, Output, &Hit);

			// Reject unwalkable normals if we end up higher than our initial height.
			// It's fine to walk down onto an unwalkable surface, don't reject those moves.
			if (Hit.Location.Z > OldLocation.Z)
			{
				// We should reject the floor result if we are trying to step up an actual step where we are not able to perch (this is rare).
				// In those cases we should instead abort the step up and try to slide along the stair.
				if (!StepDownResult.FloorResult.bBlockingHit && StepSideZ < CharacterMovementConstants::MAX_STEP_SIDE_Z)
				{
				//	ScopedStepUpMovement.RevertMove();
					ensure(false);
					return false;
				}
			}

			StepDownResult.bComputedFloor = true;
		}
	}

	// Copy step down result.
	if (OutStepDownResult != NULL)
	{
		*OutStepDownResult = StepDownResult;
	}

	// Don't recalculate velocity based on this height adjustment, if considering vertical adjustments.
	Output.bJustTeleported |= !bMaintainHorizontalGroundVelocity;

	return true;
}

bool FCharacterMovementComponentAsyncInput::CanWalkOffLedges(FCharacterMovementComponentAsyncOutput& Output) const
{
	if (!bCanWalkOffLedgesWhenCrouching && Output.bIsCrouched)
	{
		return false;
	}

	return bCanWalkOffLedges;
}

FVector FCharacterMovementComponentAsyncInput::GetLedgeMove(const FVector& OldLocation, const FVector& Delta, const FVector& GravDir, FCharacterMovementComponentAsyncOutput& Output) const
{
	if (!bHasValidData || Delta.IsZero())
	{
		return FVector::ZeroVector;
	}

	FVector SideDir(Delta.Y, -1.f * Delta.X, 0.f);

	// try left
	if (CheckLedgeDirection(OldLocation, SideDir, GravDir, Output))
	{
		return SideDir;
	}

	// try right
	SideDir *= -1.f;
	if (CheckLedgeDirection(OldLocation, SideDir, GravDir, Output))
	{
		return SideDir;
	}

	return FVector::ZeroVector;
}

bool FCharacterMovementComponentAsyncInput::CheckLedgeDirection(const FVector& OldLocation, const FVector& SideStep, const FVector& GravDir, FCharacterMovementComponentAsyncOutput& Output) const
{
	const FVector SideDest = OldLocation + SideStep;
//	FCollisionQueryParams CapsuleParams(SCENE_QUERY_STAT(CheckLedgeDirection), false, CharacterOwner);
	//FCollisionResponseParams ResponseParam;
//	InitCollisionParams(CapsuleParams, ResponseParam);
	const FCollisionShape CapsuleShape = GetPawnCapsuleCollisionShape(EShrinkCapsuleExtent::SHRINK_None, Output);
	FHitResult Result(1.f);
	World->SweepSingleByChannel(Result, OldLocation, SideDest, FQuat::Identity, CollisionChannel, CapsuleShape, QueryParams, CollisionResponseParams);

	if (!Result.bBlockingHit || IsWalkable(Result))
	{
		if (!Result.bBlockingHit)
		{
			World->SweepSingleByChannel(Result, SideDest, SideDest + GravDir * (MaxStepHeight + LedgeCheckThreshold), FQuat::Identity, CollisionChannel, CapsuleShape, QueryParams, CollisionResponseParams);
		}
		if ((Result.Time < 1.f) && IsWalkable(Result))
		{
			return true;
		}
	}
	return false;
}

FVector FCharacterMovementComponentAsyncInput::GetPawnCapsuleExtent(const EShrinkCapsuleExtent ShrinkMode, const float CustomShrinkAmount, FCharacterMovementComponentAsyncOutput& Output) const
{
	//check(CharacterOwner);

	float Radius = Output.ScaledCapsuleRadius;
	float HalfHeight = Output.ScaledCapsuleHalfHeight;
	//CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleSize(Radius, HalfHeight);
	FVector CapsuleExtent(Radius, Radius, HalfHeight);

	float RadiusEpsilon = 0.f;
	float HeightEpsilon = 0.f;

	switch (ShrinkMode)
	{
	case EShrinkCapsuleExtent::SHRINK_None:
		return CapsuleExtent;

	case EShrinkCapsuleExtent::SHRINK_RadiusCustom:
		RadiusEpsilon = CustomShrinkAmount;
		break;

	case EShrinkCapsuleExtent::SHRINK_HeightCustom:
		HeightEpsilon = CustomShrinkAmount;
		break;

	case EShrinkCapsuleExtent::SHRINK_AllCustom:
		RadiusEpsilon = CustomShrinkAmount;
		HeightEpsilon = CustomShrinkAmount;
		break;

	default:
		ensure(false);
		//UE_LOG(LogCharacterMovement, Warning, TEXT("Unknown EShrinkCapsuleExtent in UCharacterMovementComponent::GetCapsuleExtent"));
		break;
	}

	// Don't shrink to zero extent.
	const float MinExtent = UE_KINDA_SMALL_NUMBER * 10.f;
	CapsuleExtent.X = FMath::Max(CapsuleExtent.X - RadiusEpsilon, MinExtent);
	CapsuleExtent.Y = CapsuleExtent.X;
	CapsuleExtent.Z = FMath::Max(CapsuleExtent.Z - HeightEpsilon, MinExtent);

	return CapsuleExtent;
}

FCollisionShape FCharacterMovementComponentAsyncInput::GetPawnCapsuleCollisionShape(const EShrinkCapsuleExtent ShrinkMode, FCharacterMovementComponentAsyncOutput& Output, const float CustomShrinkAmount) const
{
	FVector Extent = GetPawnCapsuleExtent(ShrinkMode, CustomShrinkAmount, Output);
	return FCollisionShape::MakeCapsule(Extent);
}

void FCharacterMovementComponentAsyncInput::TwoWallAdjust(FVector& OutDelta, const FHitResult& Hit, const FVector& OldHitNormal, FCharacterMovementComponentAsyncOutput& Output) const
{
	FVector Delta = OutDelta;
	const FVector HitNormal = Hit.Normal;

	if ((OldHitNormal | HitNormal) <= 0.f) //90 or less corner, so use cross product for direction
	{
		const FVector DesiredDir = Delta;
		FVector NewDir = (HitNormal ^ OldHitNormal);
		NewDir = NewDir.GetSafeNormal();
		Delta = (Delta | NewDir) * (1.f - Hit.Time) * NewDir;
		if ((DesiredDir | Delta) < 0.f)
		{
			Delta = -1.f * Delta;
		}
	}
	else //adjust to new wall
	{
		const FVector DesiredDir = Delta;
		Delta = ComputeSlideVector(Delta, 1.f - Hit.Time, HitNormal, Hit, Output);
		if ((Delta | DesiredDir) <= 0.f)
		{
			Delta = FVector::ZeroVector;
		}
		else if (FMath::Abs((HitNormal | OldHitNormal) - 1.f) < UE_KINDA_SMALL_NUMBER)
		{
			// we hit the same wall again even after adjusting to move along it the first time
			// nudge away from it (this can happen due to precision issues)
			Delta += HitNormal * 0.01f;
		}
	}

	OutDelta = Delta;
}

void FCharacterMovementComponentAsyncInput::RevertMove(const FVector& OldLocation, UPrimitiveComponent* OldBase, const FVector& PreviousBaseLocation, const FFindFloorResult& OldFloor, bool bFailMove, FCharacterMovementComponentAsyncOutput& Output) const
{
	//UE_LOG(LogCharacterMovement, Log, TEXT("RevertMove from %f %f %f to %f %f %f"), CharacterOwner->Location.X, CharacterOwner->Location.Y, CharacterOwner->Location.Z, OldLocation.X, OldLocation.Y, OldLocation.Z);

	ETeleportType TeleportType = GetTeleportType(Output);
	ensure(TeleportType == ETeleportType::TeleportPhysics); // We don't currently compute velocity, and only teleport. If non-teleport if this is needed, should implement.
	UpdatedComponentInput->SetPosition(OldLocation);
	//UpdatedComponent->SetWorldLocation(OldLocation, false, nullptr, TeleportType);

	//UE_LOG(LogCharacterMovement, Log, TEXT("Now at %f %f %f"), CharacterOwner->Location.X, CharacterOwner->Location.Y, CharacterOwner->Location.Z);
	Output.bJustTeleported = false;


	// We can't read off movement base, ensure that our base hasn't changed, and use cached data.
	ensure(MovementBaseAsyncData.CachedMovementBase == OldBase);
	MovementBaseAsyncData.Validate(Output);

	ensure(false);

	// TODO RevertMove

	// if our previous base couldn't have moved or changed in any physics-affecting way, restore it
	/*if (IsValid(OldBase) &&
		(!MovementBaseUtility::IsDynamicBase(OldBase) ||
			(OldBase->Mobility == EComponentMobility::Static) ||
			(OldBase->GetComponentLocation() == PreviousBaseLocation)
			)
		)
	
	{
		CurrentFloor = OldFloor;
		SetBase(OldBase, OldFloor.HitResult.BoneName);
	}
	else
	{
		SetBase(NULL);
	}

	if (bFailMove)
	{
		// end movement now
		Velocity = FVector::ZeroVector;
		Acceleration = FVector::ZeroVector;
		//UE_LOG(LogCharacterMovement, Log, TEXT("%s FAILMOVE RevertMove"), *CharacterOwner->GetName());
	}*/
}

ETeleportType FCharacterMovementComponentAsyncInput::GetTeleportType(FCharacterMovementComponentAsyncOutput& Output) const
{
	// ensuring in inputs on networek large correction
	return Output.bJustTeleported /*|| bNetworkLargeClientCorrection*/ ? ETeleportType::TeleportPhysics : ETeleportType::None;
}

void FCharacterMovementComponentAsyncInput::HandleWalkingOffLedge(const FVector& PreviousFloorImpactNormal, const FVector& PreviousFloorContactNormal, const FVector& PreviousLocation, float TimeDelta) const
{
	ensure(false); // TODO HandleWalkingOffLedge
}

bool FCharacterMovementComponentAsyncInput::ShouldCatchAir(const FFindFloorResult& OldFloor, const FFindFloorResult& NewFloor) const
{
	return false;
}

void FCharacterMovementComponentAsyncInput::StartFalling(int32 Iterations, float remainingTime, float timeTick, const FVector& Delta, const FVector& subLoc, FCharacterMovementComponentAsyncOutput& Output) const
{
	// start falling 
	const float DesiredDist = Delta.Size();
	const float ActualDist = (UpdatedComponentInput->GetPosition() - subLoc).Size2D();
	remainingTime = (DesiredDist < UE_KINDA_SMALL_NUMBER)
		? 0.f
		: remainingTime + timeTick * (1.f - FMath::Min(1.f, ActualDist / DesiredDist));

	if (IsMovingOnGround(Output))
	{
		// Probably don't need this hack with async physics.


		// This is to catch cases where the first frame of PIE is executed, and the
		// level is not yet visible. In those cases, the player will fall out of the
		// world... So, don't set MOVE_Falling straight away.
		if (!GIsEditor || (World->HasBegunPlay() && (World->GetTimeSeconds() >= 1.f)))
		{
			SetMovementMode(MOVE_Falling, Output); //default behavior if script didn't change physics
		}
		else
		{
			// Make sure that the floor check code continues processing during this delay.
			Output.bForceNextFloorCheck = true;
		}
	}
	StartNewPhysics(remainingTime, Iterations, Output);
}

void FCharacterMovementComponentAsyncInput::SetBaseFromFloor(const FFindFloorResult& FloorResult, FCharacterMovementComponentAsyncOutput& Output) const
{
	if (FloorResult.IsWalkableFloor())
	{
	//	SetBase(FloorResult.HitResult.GetComponent(), FloorResult.HitResult.BoneName);
		Output.NewMovementBase = FloorResult.HitResult.GetComponent();
		Output.NewMovementBaseOwner = FloorResult.HitResult.GetActor();
	}
	else
	{
		//SetBase(nullptr);
		Output.NewMovementBase = nullptr;
		Output.NewMovementBaseOwner = nullptr;
	}
}

void FCharacterMovementComponentAsyncInput::AdjustFloorHeight(FCharacterMovementComponentAsyncOutput& Output) const
{
	//SCOPE_CYCLE_COUNTER(STAT_CharAdjustFloorHeight);

	FFindFloorResult& CurrentFloor = Output.CurrentFloor;

	// If we have a floor check that hasn't hit anything, don't adjust height.
	if (!CurrentFloor.IsWalkableFloor())
	{
		return;
	}

	float OldFloorDist = CurrentFloor.FloorDist;
	if (CurrentFloor.bLineTrace)
	{
		if (OldFloorDist < UCharacterMovementComponent::MIN_FLOOR_DIST && CurrentFloor.LineDist >= UCharacterMovementComponent::MIN_FLOOR_DIST)
		{
			// This would cause us to scale unwalkable walls
			//UE_LOG(LogCharacterMovement, VeryVerbose, TEXT("Adjust floor height aborting due to line trace with small floor distance (line: %.2f, sweep: %.2f)"), CurrentFloor.LineDist, CurrentFloor.FloorDist);
			return;
		}
		else
		{
			// Falling back to a line trace means the sweep was unwalkable (or in penetration). Use the line distance for the vertical adjustment.
			OldFloorDist = CurrentFloor.LineDist;
		}
	}

	// Move up or down to maintain floor height.
	if (OldFloorDist < UCharacterMovementComponent::MIN_FLOOR_DIST || OldFloorDist > UCharacterMovementComponent::MAX_FLOOR_DIST)
	{
		FHitResult AdjustHit(1.f);
		const float InitialZ = UpdatedComponentInput->GetPosition().Z;
		const float AvgFloorDist = (UCharacterMovementComponent::MIN_FLOOR_DIST + UCharacterMovementComponent::MAX_FLOOR_DIST) * 0.5f;
		const float MoveDist = AvgFloorDist - OldFloorDist;
		SafeMoveUpdatedComponent(FVector(0.f, 0.f, MoveDist), UpdatedComponentInput->GetRotation(), true, AdjustHit, Output);
		//UE_LOG(LogCharacterMovement, VeryVerbose, TEXT("Adjust floor height %.3f (Hit = %d)"), MoveDist, AdjustHit.bBlockingHit);

		if (!AdjustHit.IsValidBlockingHit())
		{
			CurrentFloor.FloorDist += MoveDist;
		}
		else if (MoveDist > 0.f)
		{
			const float CurrentZ = UpdatedComponentInput->GetPosition().Z;
			CurrentFloor.FloorDist += CurrentZ - InitialZ;
		}
		else
		{
			checkSlow(MoveDist < 0.f);
			const float CurrentZ = UpdatedComponentInput->GetPosition().Z;
			CurrentFloor.FloorDist = CurrentZ - AdjustHit.Location.Z;
			if (IsWalkable(AdjustHit))
			{
				CurrentFloor.SetFromSweep(AdjustHit, CurrentFloor.FloorDist, true);
			}
		}

		// Don't recalculate velocity based on this height adjustment, if considering vertical adjustments.
		// Also avoid it if we moved out of penetration
		Output.bJustTeleported |= !bMaintainHorizontalGroundVelocity || (OldFloorDist < 0.f);

		// If something caused us to adjust our height (especially a depentration) we should ensure another check next frame or we will keep a stale result.
		if (/*CharacterOwner && */CharacterInput->LocalRole != ROLE_SimulatedProxy)
		{
			Output.bForceNextFloorCheck = true;
		}
	}
}

bool FCharacterMovementComponentAsyncInput::ShouldComputePerchResult(const FHitResult& InHit, FCharacterMovementComponentAsyncOutput& Output, bool bCheckRadius) const
{
	if (!InHit.IsValidBlockingHit())
	{
		return false;
	}

	// Don't try to perch if the edge radius is very small.
	if (GetPerchRadiusThreshold() <= UCharacterMovementComponent::SWEEP_EDGE_REJECT_DISTANCE)
	{
		return false;
	}

	if (bCheckRadius)
	{
		const float DistFromCenterSq = (InHit.ImpactPoint - InHit.Location).SizeSquared2D();
		const float StandOnEdgeRadius = GetValidPerchRadius(Output);
		if (DistFromCenterSq <= FMath::Square(StandOnEdgeRadius))
		{
			// Already within perch radius.
			return false;
		}
	}

	return true;
}

bool FCharacterMovementComponentAsyncInput::ComputePerchResult(const float TestRadius, const FHitResult& InHit, const float InMaxFloorDist, FFindFloorResult& OutPerchFloorResult, FCharacterMovementComponentAsyncOutput& Output) const
{
	if (InMaxFloorDist <= 0.f)
	{
		return false;
	}

	// Sweep further than actual requested distance, because a reduced capsule radius means we could miss some hits that the normal radius would contact.
	const float PawnRadius = Output.ScaledCapsuleRadius;
	const float PawnHalfHeight = Output.ScaledCapsuleHalfHeight;
	//CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleSize(PawnRadius, PawnHalfHeight);

	const float InHitAboveBase = FMath::Max(0.f, InHit.ImpactPoint.Z - (InHit.Location.Z - PawnHalfHeight));
	const float PerchLineDist = FMath::Max(0.f, InMaxFloorDist - InHitAboveBase);
	const float PerchSweepDist = FMath::Max(0.f, InMaxFloorDist);

	const float ActualSweepDist = PerchSweepDist + PawnRadius;
	ComputeFloorDist(InHit.Location, PerchLineDist, ActualSweepDist, OutPerchFloorResult, TestRadius, Output);

	if (!OutPerchFloorResult.IsWalkableFloor())
	{
		return false;
	}
	else if (InHitAboveBase + OutPerchFloorResult.FloorDist > InMaxFloorDist)
	{
		// Hit something past max distance
		OutPerchFloorResult.bWalkableFloor = false;
		return false;
	}

	return true;
}

float FCharacterMovementComponentAsyncInput::GetPerchRadiusThreshold() const
{
	// Don't allow negative values.
	return FMath::Max(0.f, PerchRadiusThreshold);
}

float FCharacterMovementComponentAsyncInput::GetValidPerchRadius(const FCharacterMovementComponentAsyncOutput& Output) const
{
	const float PawnRadius = Output.ScaledCapsuleRadius;
	return FMath::Clamp(PawnRadius - GetPerchRadiusThreshold(), 0.11f, PawnRadius);
}

bool FCharacterMovementComponentAsyncInput::CheckFall(const FFindFloorResult& OldFloor, const FHitResult& Hit, const FVector& Delta, const FVector& OldLocation, float remainingTime, float timeTick, int32 Iterations, bool bMustJump, FCharacterMovementComponentAsyncOutput& Output) const
{
	if (!bHasValidData)
	{
		return false;
	}

	if (bMustJump || CanWalkOffLedges(Output))
	{
		HandleWalkingOffLedge(OldFloor.HitResult.ImpactNormal, OldFloor.HitResult.Normal, OldLocation, timeTick);
		if (IsMovingOnGround(Output))
		{
			// If still walking, then fall. If not, assume the user set a different mode they want to keep.
			StartFalling(Iterations, remainingTime, timeTick, Delta, OldLocation, Output);
		}
		return true;
	}
	return false;
}

FVector FCharacterMovementComponentAsyncInput::GetFallingLateralAcceleration(float DeltaTime, FCharacterMovementComponentAsyncOutput& Output) const
{
	// No acceleration in Z
	FVector FallAcceleration = FVector(Output.Acceleration.X, Output.Acceleration.Y, 0.f);

	// bound acceleration, falling object has minimal ability to impact acceleration
	if (!RootMotion.bHasAnimRootMotion && FallAcceleration.SizeSquared2D() > 0.f)
	{
		FallAcceleration = GetAirControl(DeltaTime, AirControl, FallAcceleration, Output);
		FallAcceleration = FallAcceleration.GetClampedToMaxSize(MaxAcceleration);
	}

	return FallAcceleration;
}

float FCharacterMovementComponentAsyncInput::BoostAirControl(float DeltaTime, float TickAirControl, const FVector& FallAcceleration, FCharacterMovementComponentAsyncOutput& Output) const
{
	// Allow a burst of initial acceleration
	if (AirControlBoostMultiplier > 0.f && Output.Velocity.SizeSquared2D() < FMath::Square(AirControlBoostVelocityThreshold))
	{
		TickAirControl = FMath::Min(1.f, AirControlBoostMultiplier * TickAirControl);
	}

	return TickAirControl;
}

bool FCharacterMovementComponentAsyncInput::ShouldLimitAirControl(float DeltaTime, const FVector& FallAcceleration, FCharacterMovementComponentAsyncOutput& Output) const
{
	return (FallAcceleration.SizeSquared2D() > 0.f);
}

FVector FCharacterMovementComponentAsyncInput::LimitAirControl(float DeltaTime, const FVector& FallAcceleration, const FHitResult& HitResult, bool bCheckForValidLandingSpot, FCharacterMovementComponentAsyncOutput& Output) const
{
	FVector Result(FallAcceleration);

	if (HitResult.IsValidBlockingHit() && HitResult.Normal.Z > CharacterMovementConstants::VERTICAL_SLOPE_NORMAL_Z)
	{
		if (!bCheckForValidLandingSpot || !IsValidLandingSpot(HitResult.Location, HitResult, Output))
		{
			// If acceleration is into the wall, limit contribution.
			if (FVector::DotProduct(FallAcceleration, HitResult.Normal) < 0.f)
			{
				// Allow movement parallel to the wall, but not into it because that may push us up.
				const FVector Normal2D = HitResult.Normal.GetSafeNormal2D();
				Result = FVector::VectorPlaneProject(FallAcceleration, Normal2D);
			}
		}
	}
	else if (HitResult.bStartPenetrating)
	{
		// Allow movement out of penetration.
		return (FVector::DotProduct(Result, HitResult.Normal) > 0.f ? Result : FVector::ZeroVector);
	}

	return Result;
}

FVector FCharacterMovementComponentAsyncInput::NewFallVelocity(const FVector& InitialVelocity, const FVector& Gravity, float DeltaTime, FCharacterMovementComponentAsyncOutput& Output) const
{
	FVector Result = InitialVelocity;

	if (DeltaTime > 0.f)
	{
		// Apply gravity.
		Result += Gravity * DeltaTime;


		// Don't exceed terminal velocity.
		const float TerminalLimit = FMath::Abs(PhysicsVolumeTerminalVelocity);//FMath::Abs(GetPhysicsVolume()->TerminalVelocity);
		if (Result.SizeSquared() > FMath::Square(TerminalLimit))
		{
			const FVector GravityDir = Gravity.GetSafeNormal();
			if ((Result | GravityDir) > TerminalLimit)
			{
				Result = FVector::PointPlaneProject(Result, FVector::ZeroVector, GravityDir) + GravityDir * TerminalLimit;
			}
		}
	}

	return Result;
}

bool FCharacterMovementComponentAsyncInput::IsValidLandingSpot(const FVector& CapsuleLocation, const FHitResult& Hit, FCharacterMovementComponentAsyncOutput& Output) const
{
	if (!Hit.bBlockingHit)
	{
		return false;
	}

	// Skip some checks if penetrating. Penetration will be handled by the FindFloor call (using a smaller capsule)
	if (!Hit.bStartPenetrating)
	{
		// Reject unwalkable floor normals.
		if (!IsWalkable(Hit))
		{
			return false;
		}

		float PawnRadius = Output.ScaledCapsuleRadius;
		float PawnHalfHeight = Output.ScaledCapsuleHalfHeight;
		//CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleSize(PawnRadius, PawnHalfHeight);

		// Reject hits that are above our lower hemisphere (can happen when sliding down a vertical surface).
		const FVector::FReal LowerHemisphereZ = Hit.Location.Z - PawnHalfHeight + PawnRadius;
		if (Hit.ImpactPoint.Z >= LowerHemisphereZ)
		{
			return false;
		}

		// Reject hits that are barely on the cusp of the radius of the capsule
		if (!IsWithinEdgeTolerance(Hit.Location, Hit.ImpactPoint, PawnRadius))
		{
			return false;
		}
	}
	else
	{
		// Penetrating
		if (Hit.Normal.Z < UE_KINDA_SMALL_NUMBER)
		{
			// Normal is nearly horizontal or downward, that's a penetration adjustment next to a vertical or overhanging wall. Don't pop to the floor.
			return false;
		}
	}

	FFindFloorResult FloorResult;
	FindFloor(CapsuleLocation, FloorResult, false, Output, &Hit);

	if (!FloorResult.IsWalkableFloor())
	{
		return false;
	}

	return true;
}

void FCharacterMovementComponentAsyncInput::ProcessLanded(const FHitResult& Hit, float remainingTime, int32 Iterations, FCharacterMovementComponentAsyncOutput& Output) const
{
	//SCOPE_CYCLE_COUNTER(STAT_CharProcessLanded);

	// TODO NotifyLanded
	/*if (CharacterOwner && CharacterOwner->ShouldNotifyLanded(Hit))
	{
		CharacterOwner->Landed(Hit);
	}*/
	if (IsFalling(Output))
	{
		// TODO NavWalking
		/*if (GroundMovementMode == MOVE_NavWalking)
		{
			// verify navmesh projection and current floor
			// otherwise movement will be stuck in infinite loop:
			// navwalking -> (no navmesh) -> falling -> (standing on something) -> navwalking -> ....

			const FVector TestLocation = GetActorFeetLocation();
			FNavLocation NavLocation;

			const bool bHasNavigationData = FindNavFloor(TestLocation, NavLocation);
			if (!bHasNavigationData || NavLocation.NodeRef == INVALID_NAVNODEREF)
			{
				GroundMovementMode = MOVE_Walking;
				UE_LOG(LogNavMeshMovement, Verbose, TEXT("ProcessLanded(): %s tried to go to NavWalking but couldn't find NavMesh! Using Walking instead."), *GetNameSafe(CharacterOwner));
			}
		}*/

		SetPostLandedPhysics(Hit, Output);
	}

	// TODO PathFollowingAgent
/*	IPathFollowingAgentInterface* PFAgent = GetPathFollowingAgent();
	if (PFAgent)
	{
		PFAgent->OnLanded();
	}*/

	StartNewPhysics(remainingTime, Iterations, Output);
}

void FCharacterMovementComponentAsyncInput::SetPostLandedPhysics(const FHitResult& Hit, FCharacterMovementComponentAsyncOutput& Output) const
{
	// TODO Other movement modes
	/*if (CanEverSwim() && IsInWater())
	{
		SetMovementMode(MOVE_Swimming);
	}
	else*/
	{
		const FVector PreImpactAccel = Output.Acceleration + (IsFalling(Output) ? FVector(0.f, 0.f, GravityZ) : FVector::ZeroVector);
		const FVector PreImpactVelocity = Output.Velocity;
		if (DefaultLandMovementMode == MOVE_Walking ||
			DefaultLandMovementMode == MOVE_NavWalking ||
			DefaultLandMovementMode == MOVE_Falling)
		{
			SetMovementMode(Output.GroundMovementMode, Output);
		}
		else
		{
			SetDefaultMovementMode(Output);
		}
		// TODO bEnablePhysicsInteraction
		//ApplyImpactPhysicsForces(Hit, PreImpactAccel, PreImpactVelocity);
	}
}

void FCharacterMovementComponentAsyncInput::SetDefaultMovementMode(FCharacterMovementComponentAsyncOutput& Output) const
{
	// check for water volume
	/*if (CanEverSwim() && IsInWater())
	{
		SetMovementMode(DefaultWaterMovementMode);
	}*/
	/*else */if (/*!CharacterOwner || */Output.MovementMode != DefaultLandMovementMode)
	{
		const float SavedVelocityZ = Output.Velocity.Z;
		SetMovementMode(DefaultLandMovementMode, Output);

		// Avoid 1-frame delay if trying to walk but walking fails at this location.
		if (Output.MovementMode == MOVE_Walking && Output.NewMovementBase == NULL)
		{
			Output.Velocity.Z = SavedVelocityZ; // Prevent temporary walking state from zeroing Z velocity.
			SetMovementMode(MOVE_Falling, Output);
		}
	}
}

bool FCharacterMovementComponentAsyncInput::ShouldCheckForValidLandingSpot(float DeltaTime, const FVector& Delta, const FHitResult& Hit, FCharacterMovementComponentAsyncOutput& Output) const
{
	// See if we hit an edge of a surface on the lower portion of the capsule.
	// In this case the normal will not equal the impact normal, and a downward sweep may find a walkable surface on top of the edge.
	if (Hit.Normal.Z > UE_KINDA_SMALL_NUMBER && !Hit.Normal.Equals(Hit.ImpactNormal))
	{
		const FVector PawnLocation = UpdatedComponentInput->GetPosition();
		if (IsWithinEdgeTolerance(PawnLocation, Hit.ImpactPoint, Output.ScaledCapsuleHalfHeight))
		{
			return true;
		}
	}

	return false;
}

bool FCharacterMovementComponentAsyncInput::ShouldRemainVertical(FCharacterMovementComponentAsyncOutput& Output) const
{
	// Always remain vertical when walking or falling.
	return IsMovingOnGround(Output) || IsFalling(Output);
}

bool FCharacterMovementComponentAsyncInput::CanAttemptJump(FCharacterMovementComponentAsyncOutput& Output) const
{
	return IsJumpAllowed() &&
		!Output.bWantsToCrouch &&
		(IsMovingOnGround(Output) || IsFalling(Output));  // Falling included for double-jump and non-zero jump hold time, but validated by character.
}

bool FCharacterMovementComponentAsyncInput::DoJump(bool bReplayingMoves, FCharacterMovementComponentAsyncOutput& Output) const
{
	if (/*CharacterOwner &&*/ CharacterInput->CanJump(*this, Output))
	{
		// Don't jump if we can't move up/down.
		if (!bConstrainToPlane || FMath::Abs(PlaneConstraintNormal.Z) != 1.f)
		{
			Output.Velocity.Z = FMath::Max(Output.Velocity.Z, JumpZVelocity);
			SetMovementMode(MOVE_Falling, Output);
			return true;
		}
	}

	return false;
}

bool FCharacterMovementComponentAsyncInput::IsJumpAllowed() const
{
	return bNavAgentPropsCanJump && bMovementStateCanJump;
}

/*
*
* UMovementComponent Interface Begin
* 
*/
float FCharacterMovementComponentAsyncInput::GetMaxSpeed(FCharacterMovementComponentAsyncOutput& Output) const
{
	switch (Output.MovementMode)
	{
	case MOVE_Walking:
	case MOVE_NavWalking:
		return IsCrouching(Output) ? MaxWalkSpeedCrouched : MaxWalkSpeed;
	case MOVE_Falling:
		return MaxWalkSpeed;
	case MOVE_Swimming:
		return MaxSwimSpeed;
	case MOVE_Flying:
		return MaxFlySpeed;
	case MOVE_Custom:
		return MaxCustomMovementSpeed;
	case MOVE_None:
	default:
		return 0.f;
	}
}

bool FCharacterMovementComponentAsyncInput::IsCrouching(const FCharacterMovementComponentAsyncOutput& Output) const
{
	return Output.bIsCrouched;
}

bool FCharacterMovementComponentAsyncInput::IsFalling(const FCharacterMovementComponentAsyncOutput& Output) const
{
	return (Output.MovementMode == MOVE_Falling);// && UpdatedComponent;
}

bool FCharacterMovementComponentAsyncInput::IsFlying(const FCharacterMovementComponentAsyncOutput& Output) const
{
	return (Output.MovementMode == MOVE_Flying);// && UpdatedComponent;
}

bool FCharacterMovementComponentAsyncInput::IsMovingOnGround(const FCharacterMovementComponentAsyncOutput& Output) const
{
	return ((Output.MovementMode == MOVE_Walking) || (Output.MovementMode == MOVE_NavWalking));//&& UpdatedComponent;
}

bool FCharacterMovementComponentAsyncInput::IsExceedingMaxSpeed(float MaxSpeed, const FCharacterMovementComponentAsyncOutput& Output) const
{
	MaxSpeed = FMath::Max(0.f, MaxSpeed);
	const float MaxSpeedSquared = FMath::Square(MaxSpeed);

	// Allow 1% error tolerance, to account for numeric imprecision.
	const float OverVelocityPercent = 1.01f;
	return (Output.Velocity.SizeSquared() > MaxSpeedSquared * OverVelocityPercent);
}

FRotator FCharacterMovementComponentAsyncInput::ComputeOrientToMovementRotation(const FRotator& CurrentRotation, float DeltaTime, FRotator& DeltaRotation, FCharacterMovementComponentAsyncOutput& Output) const
{
	if (Output.Acceleration.SizeSquared() < UE_KINDA_SMALL_NUMBER)
	{
		// AI path following request can orient us in that direction (it's effectively an acceleration)
		if (Output.bHasRequestedVelocity && Output.RequestedVelocity.SizeSquared() > UE_KINDA_SMALL_NUMBER)
		{
			return Output.RequestedVelocity.GetSafeNormal().Rotation();
		}

		// Don't change rotation if there is no acceleration.
		return CurrentRotation;
	}

	// Rotate toward direction of acceleration.
	return Output.Acceleration.GetSafeNormal().Rotation();
}

void FCharacterMovementComponentAsyncInput::RestorePreAdditiveRootMotionVelocity(FCharacterMovementComponentAsyncOutput& Output) const
{
	// Restore last frame's pre-additive Velocity if we had additive applied 
	// so that we're not adding more additive velocity than intended
	if (Output.bIsAdditiveVelocityApplied)
	{
		/*#if ROOT_MOTION_DEBUG
				if (RootMotionSourceDebug::CVarDebugRootMotionSources.GetValueOnGameThread() == 1)
				{
					FString AdjustedDebugString = FString::Printf(TEXT("RestorePreAdditiveRootMotionVelocity Velocity(%s) LastPreAdditiveVelocity(%s)"),
						*Velocity.ToCompactString(), *CurrentRootMotion.LastPreAdditiveVelocity.ToCompactString());
					RootMotionSourceDebug::PrintOnScreen(*CharacterOwner, AdjustedDebugString);
				}
		#endif*/

		Output.Velocity = Output.LastPreAdditiveVelocity;
		Output.bIsAdditiveVelocityApplied = false;
	}
}

void FCharacterMovementComponentAsyncInput::ApplyRootMotionToVelocity(float deltaTime, FCharacterMovementComponentAsyncOutput& Output) const
{
	// Animation root motion is distinct from root motion sources right now and takes precedence
	if (RootMotion.bHasAnimRootMotion && deltaTime > 0.f)
	{
		Output.Velocity = ConstrainAnimRootMotionVelocity(Output.AnimRootMotionVelocity, Output.Velocity, Output);
		return;
	}

	const FVector OldVelocity = Output.Velocity;

	bool bAppliedRootMotion = false;

	// Apply override velocity
	if (RootMotion.bHasOverrideRootMotion)
	{
		Output.Velocity = RootMotion.OverrideVelocity;
		bAppliedRootMotion = true;

#if ROOT_MOTION_DEBUG
		if (RootMotionSourceDebug::CVarDebugRootMotionSources.GetValueOnGameThread() == 1)
		{
			FString AdjustedDebugString = FString::Printf(TEXT("ApplyRootMotionToVelocity HasOverrideVelocity Velocity(%s)"),
				*Output.Velocity.ToCompactString());
			//RootMotionSourceDebug::PrintOnScreen(*CharacterOwner, AdjustedDebugString);
		}
#endif
	}

	// Next apply additive root motion
	if (RootMotion.bHasAdditiveRootMotion)
	{
		Output.LastPreAdditiveVelocity = Output.Velocity; // Save off pre-additive Velocity for restoration next tick
		Output.Velocity += RootMotion.AdditiveVelocity;
		//AccumulateRootMotionVelocity(ERootMotionAccumulateMode::Additive, deltaTime, UpdatedComponentInput->UpdatedComponent, Output.Velocity, Output);
		Output.bIsAdditiveVelocityApplied = true; // Remember that we have it applied
		bAppliedRootMotion = true;
	}

	// Switch to Falling if we have vertical velocity from root motion so we can lift off the ground
	const FVector AppliedVelocityDelta = Output.Velocity - OldVelocity;
	if (bAppliedRootMotion && AppliedVelocityDelta.Z != 0.f && IsMovingOnGround(Output))
	{
		float LiftoffBound;
		if (RootMotion.bUseSensitiveLiftoff)
		{
			// Sensitive bounds - "any positive force"a
			LiftoffBound = UE_SMALL_NUMBER;
		}
		else
		{
			// Default bounds - the amount of force gravity is applying this tick
			LiftoffBound = FMath::Max(GravityZ * deltaTime, UE_SMALL_NUMBER);
		}

		if (AppliedVelocityDelta.Z > LiftoffBound)
		{
			SetMovementMode(MOVE_Falling, Output);
		}
	}
}

FVector FCharacterMovementComponentAsyncInput::ConstrainAnimRootMotionVelocity(const FVector& RootMotionVelocity, const FVector& CurrentVelocity, FCharacterMovementComponentAsyncOutput& Output) const
{
	FVector Result = RootMotionVelocity;

	// Do not override Velocity.Z if in falling physics, we want to keep the effect of gravity.
	if (IsFalling(Output))
	{
		Result.Z = CurrentVelocity.Z;
	}

	return Result;
}

FVector FCharacterMovementComponentAsyncInput::CalcAnimRootMotionVelocity(const FVector& RootMotionDeltaMove, float DeltaSeconds, const FVector& CurrentVelocity) const
{
	if (ensure(DeltaSeconds > 0.f))
	{
		FVector RootMotionVelocity = RootMotionDeltaMove / DeltaSeconds;
		return RootMotionVelocity;
	}
	else
	{
		return CurrentVelocity;
	}
}

FVector FCharacterMovementComponentAsyncInput::GetAirControl(float DeltaTime, float TickAirControl, const FVector& FallAcceleration, FCharacterMovementComponentAsyncOutput& Output) const
{
	// Boost
	if (TickAirControl != 0.f)
	{
		TickAirControl = BoostAirControl(DeltaTime, TickAirControl, FallAcceleration, Output);
	}

	return TickAirControl * FallAcceleration;
}

void FCharacterAsyncInput::FaceRotation(FRotator NewControlRotation, float DeltaTime, const FCharacterMovementComponentAsyncInput& Input, FCharacterMovementComponentAsyncOutput& Output) const
{
	// Only if we actually are going to use any component of rotation.
	if (bUseControllerRotationPitch || bUseControllerRotationYaw || bUseControllerRotationRoll)
	{
		FRotator& CurrentRotation = Output.CharacterOutput->Rotation;
		if (!bUseControllerRotationPitch)
		{
			NewControlRotation.Pitch = CurrentRotation.Pitch;
		}

		if (!bUseControllerRotationYaw)
		{
			NewControlRotation.Yaw = CurrentRotation.Yaw;
		}

		if (!bUseControllerRotationRoll)
		{
			NewControlRotation.Roll = CurrentRotation.Roll;
		}

		/*#if ENABLE_NAN_DIAGNOSTIC
				if (NewControlRotation.ContainsNaN())
				{
					logOrEnsureNanError(TEXT("APawn::FaceRotation about to apply NaN-containing rotation to actor! New:(%s), Current:(%s)"), *NewControlRotation.ToString(), *CurrentRotation.ToString());
				}
		#endif*/

		CurrentRotation = NewControlRotation;
	}
}


void FCharacterAsyncInput::CheckJumpInput(float DeltaSeconds, const FCharacterMovementComponentAsyncInput& Input, FCharacterMovementComponentAsyncOutput& Output) const
{
	Output.CharacterOutput->JumpCurrentCountPreJump = Output.CharacterOutput->JumpCurrentCount;

	if (Output.CharacterOutput->bPressedJump)
	{
		// If this is the first jump and we're already falling,
		// then increment the JumpCount to compensate.
		const bool bFirstJump = Output.CharacterOutput->JumpCurrentCount == 0;
		if (bFirstJump && Input.IsFalling(Output))
		{
			Output.CharacterOutput->JumpCurrentCount++;
		}

		// TODO bClientUpdating
		const bool bDidJump = CanJump(Input, Output) && Input.DoJump(/*bClientUpdating*/false, Output);
		if (bDidJump)
		{
			// Transition from not (actively) jumping to jumping.
			if (!Output.CharacterOutput->bWasJumping)
			{
				Output.CharacterOutput->JumpCurrentCount++;
				Output.CharacterOutput->JumpForceTimeRemaining = JumpMaxHoldTime;
				//OnJumped(); TODO Jumping
			}
		}

		Output.CharacterOutput->bWasJumping = bDidJump;
	}
}

void FCharacterAsyncInput::ClearJumpInput(float DeltaSeconds, const FCharacterMovementComponentAsyncInput& Input, FCharacterMovementComponentAsyncOutput& Output) const
{
	if (Output.CharacterOutput->bPressedJump)
	{
		Output.CharacterOutput->JumpKeyHoldTime += DeltaSeconds;

		// Don't disable bPressedJump right away if it's still held.
		// Don't modify JumpForceTimeRemaining because a frame of update may be remaining.
		if (Output.CharacterOutput->JumpKeyHoldTime >= JumpMaxHoldTime)
		{
			Output.CharacterOutput->bClearJumpInput = true;
			Output.CharacterOutput->bPressedJump = false;
		}
	}
	else
	{
		Output.CharacterOutput->JumpForceTimeRemaining = 0.0f;
		Output.CharacterOutput->bWasJumping = false;
	}
}

bool FCharacterAsyncInput::CanJump(const FCharacterMovementComponentAsyncInput& Input, FCharacterMovementComponentAsyncOutput& Output) const
{
	// Ensure the character isn't currently crouched.
	bool bCanJump = !Output.bIsCrouched;

	// Ensure that the CharacterMovement state is valid
	bCanJump &= Input.CanAttemptJump(Output);

	if (bCanJump)
	{
		// Ensure JumpHoldTime and JumpCount are valid.
		if (!Output.CharacterOutput->bWasJumping || Input.CharacterInput->JumpMaxHoldTime <= 0.0f)
		{
			if (Output.CharacterOutput->JumpCurrentCount == 0 && Input.IsFalling(Output))
			{
				bCanJump = Output.CharacterOutput->JumpCurrentCount + 1 < Input.CharacterInput->JumpMaxCount;
			}
			else
			{
				bCanJump = Output.CharacterOutput->JumpCurrentCount < Input.CharacterInput->JumpMaxCount;
			}
		}
		else
		{
			// Only consider JumpKeyHoldTime as long as:
			// A) The jump limit hasn't been met OR
			// B) The jump limit has been met AND we were already jumping
			const bool bJumpKeyHeld = (Output.CharacterOutput->bPressedJump && Output.CharacterOutput->JumpKeyHoldTime < Input.CharacterInput->JumpMaxHoldTime);
			bCanJump = bJumpKeyHeld &&
				((Output.CharacterOutput->JumpCurrentCount < Input.CharacterInput->JumpMaxCount) || (Output.CharacterOutput->bWasJumping && Output.CharacterOutput->JumpCurrentCount == Input.CharacterInput->JumpMaxCount));
		}
	}

	return bCanJump;
}

void FCharacterAsyncInput::ResetJumpState(const FCharacterMovementComponentAsyncInput& Input, FCharacterMovementComponentAsyncOutput& Output) const
{
	if (Output.CharacterOutput->bPressedJump == true)
	{
		Output.CharacterOutput->bClearJumpInput = true;
	}

	Output.CharacterOutput->bPressedJump = false;
	Output.CharacterOutput->bWasJumping = false;
	Output.CharacterOutput->JumpKeyHoldTime = 0.0f;
	Output.CharacterOutput->JumpForceTimeRemaining = 0.0f;

	if (!Input.IsFalling(Output))
	{
		Output.CharacterOutput->JumpCurrentCount = 0;
		Output.CharacterOutput->JumpCurrentCountPreJump = 0;
	}
}

void FCharacterAsyncInput::OnMovementModeChanged(EMovementMode PrevMovementMode, const FCharacterMovementComponentAsyncInput& Input, FCharacterMovementComponentAsyncOutput& Output, uint8 PreviousCustomMode)
{
	if (!Output.CharacterOutput->bPressedJump || !Input.IsFalling(Output))
	{
		ResetJumpState(Input, Output);
	}
	
	// TODO Proxy
	// Record jump force start time for proxies. Allows us to expire the jump even if not continually ticking down a timer.
	/*if (bProxyIsJumpForceApplied && Input->IsFalling(Output))
	{
		ProxyJumpForceStartedTime = GetWorld()->GetTimeSeconds();
	}*/

	// TODO Blueprint calls
	//K2_OnMovementModeChanged(PrevMovementMode, CharacterMovement->MovementMode, PrevCustomMode, CharacterMovement->CustomMovementMode);
	//MovementModeChangedDelegate.Broadcast(this, PrevMovementMode, PrevCustomMode);
}

FName FCharacterMovementComponentAsyncCallback::GetFNameForStatId() const
{
	const static FLazyName StaticName("FCharacterMovementComponentAsyncCallback");
	return StaticName;
}

void FCharacterMovementComponentAsyncCallback::OnPreSimulate_Internal()
{
	PreSimulateImpl<FCharacterMovementComponentAsyncInput, FCharacterMovementComponentAsyncOutput>(*this);
}

void FCharacterMovementComponentAsyncOutput::Copy(const FCharacterMovementComponentAsyncOutput& Value)
{
	bIsValid = Value.bIsValid;

	bWasSimulatingRootMotion = Value.bWasSimulatingRootMotion;
	MovementMode = Value.MovementMode;
	GroundMovementMode = Value.GroundMovementMode;
	CustomMovementMode = Value.CustomMovementMode;
	Acceleration = Value.Acceleration;
	AnalogInputModifier = Value.AnalogInputModifier;
	LastUpdateLocation = Value.LastUpdateLocation;
	LastUpdateRotation = Value.LastUpdateRotation;
	LastUpdateVelocity = Value.LastUpdateVelocity;
	bForceNextFloorCheck = Value.bForceNextFloorCheck;
	Velocity = Value.Velocity;
	LastPreAdditiveVelocity = Value.LastPreAdditiveVelocity;
	bIsAdditiveVelocityApplied = Value.bIsAdditiveVelocityApplied;
	bDeferUpdateBasedMovement = Value.bDeferUpdateBasedMovement;
	MoveComponentFlags = Value.MoveComponentFlags;
	PendingForceToApply = Value.PendingForceToApply;
	PendingImpulseToApply = Value.PendingImpulseToApply;
	PendingLaunchVelocity = Value.PendingLaunchVelocity;
	bCrouchMaintainsBaseLocation = Value.bCrouchMaintainsBaseLocation;
	bJustTeleported = Value.bJustTeleported;
	ScaledCapsuleRadius = Value.ScaledCapsuleRadius;
	ScaledCapsuleHalfHeight = Value.ScaledCapsuleHalfHeight;
	bIsCrouched = Value.bIsCrouched;
	bWantsToCrouch = Value.bWantsToCrouch;
	bMovementInProgress = Value.bMovementInProgress;
	CurrentFloor = Value.CurrentFloor;
	bHasRequestedVelocity = Value.bHasRequestedVelocity;
	bRequestedMoveWithMaxSpeed = Value.bRequestedMoveWithMaxSpeed;
	RequestedVelocity = Value.RequestedVelocity;
	LastUpdateRequestedVelocity = Value.LastUpdateRequestedVelocity;
	NumJumpApexAttempts = Value.NumJumpApexAttempts;
	AnimRootMotionVelocity = Value.AnimRootMotionVelocity;
	bShouldApplyDeltaToMeshPhysicsTransforms = Value.bShouldApplyDeltaToMeshPhysicsTransforms;
	DeltaPosition = Value.DeltaPosition;
	DeltaQuat = Value.DeltaQuat;
	DeltaTime = Value.DeltaTime;
	OldVelocity = Value.OldVelocity;
	OldLocation = Value.OldLocation;
	ModifiedRotationRate = Value.ModifiedRotationRate;
	bUsingModifiedRotationRate = Value.bUsingModifiedRotationRate;

	// See MaybeUpdateBasedMovement
	// TODO MovementBase, handle tick group changes properly
	bShouldDisablePostPhysicsTick = Value.bShouldDisablePostPhysicsTick;
	bShouldEnablePostPhysicsTick = Value.bShouldEnablePostPhysicsTick;
	bShouldAddMovementBaseTickDependency = Value.bShouldAddMovementBaseTickDependency;
	bShouldRemoveMovementBaseTickDependency = Value.bShouldRemoveMovementBaseTickDependency;

	NewMovementBase = Value.NewMovementBase;
	NewMovementBaseOwner = Value.NewMovementBaseOwner;

	UpdatedComponentOutput = Value.UpdatedComponentOutput;
	*CharacterOutput = *Value.CharacterOutput;
}

FRotator FCharacterMovementComponentAsyncOutput::GetDeltaRotation(const FRotator& InRotationRate, float InDeltaTime)
{
	return FRotator(GetAxisDeltaRotation(InRotationRate.Pitch, InDeltaTime), GetAxisDeltaRotation(InRotationRate.Yaw, InDeltaTime), GetAxisDeltaRotation(InRotationRate.Roll, InDeltaTime));
}

float FCharacterMovementComponentAsyncOutput::GetAxisDeltaRotation(float InAxisRotationRate, float InDeltaTime)
{
	// Values over 360 don't do anything, see FMath::FixedTurn. However we are trying to avoid giant floats from overflowing other calculations.
	return (InAxisRotationRate >= 0.f) ? FMath::Min(InAxisRotationRate * InDeltaTime, 360.f) : 360.f;
}

