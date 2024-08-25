// Copyright Epic Games, Inc. All Rights Reserved.


#include "MoverDataModelTypes.h"
#include "Components/PrimitiveComponent.h"
#include "MoveLibrary/BasedMovementUtils.h"
#include "MoverLog.h"



// FCharacterDefaultInputs //////////////////////////////////////////////////////////////

void FCharacterDefaultInputs::SetMoveInput(EMoveInputType InMoveInputType, const FVector& InMoveInput)
{
	MoveInputType = InMoveInputType;

	// Limit the precision that we store, so that it matches what is NetSerialized (2 decimal place of precision).
	// This ensures the authoring client, server, and any networking peers are all simulating with the same move input.
	// Note: any change to desired precision must be made here and in NetSerialize
	MoveInput.X = FMath::RoundToFloat(InMoveInput.X * 100.0) / 100.0;
	MoveInput.Y = FMath::RoundToFloat(InMoveInput.Y * 100.0) / 100.0;
	MoveInput.Z = FMath::RoundToFloat(InMoveInput.Z * 100.0) / 100.0;
}


FVector FCharacterDefaultInputs::GetMoveInput_WorldSpace() const
{
	if (bUsingMovementBase && MovementBase)
	{
		FVector MoveInputWorldSpace;
		UBasedMovementUtils::TransformBasedDirectionToWorld(MovementBase, MovementBaseBoneName, MoveInput, MoveInputWorldSpace);
		return MoveInputWorldSpace;
	}

	return MoveInput;	// already in world space
}


FVector FCharacterDefaultInputs::GetOrientationIntentDir_WorldSpace() const
{
	if (bUsingMovementBase && MovementBase)
	{
		FVector OrientIntentDirWorldSpace;
		UBasedMovementUtils::TransformBasedDirectionToWorld(MovementBase, MovementBaseBoneName, OrientationIntent, OrientIntentDirWorldSpace);
		return OrientIntentDirWorldSpace;
	}

	return OrientationIntent;	// already in world space
}


FMoverDataStructBase* FCharacterDefaultInputs::Clone() const
{
	// TODO: ensure that this memory allocation jives with deletion method
	FCharacterDefaultInputs* CopyPtr = new FCharacterDefaultInputs(*this);
	return CopyPtr;
}

bool FCharacterDefaultInputs::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	Super::NetSerialize(Ar, Map, bOutSuccess);

	Ar << MoveInputType;

	SerializePackedVector<100, 30>(MoveInput, Ar);	// Note: if you change this serialization, also change in SetMoveInput
	SerializeFixedVector<1, 16>(OrientationIntent, Ar);
	ControlRotation.SerializeCompressedShort(Ar);

	Ar << SuggestedMovementMode;

	Ar.SerializeBits(&bUsingMovementBase, 1);

	if (bUsingMovementBase)
	{
		Ar << MovementBase;
		Ar << MovementBaseBoneName;
	}
	else if (Ar.IsLoading())
	{
		// skip attempts to load movement base properties if flagged as not using a movement base
		MovementBase = nullptr;
		MovementBaseBoneName = NAME_None;
	}

	Ar.SerializeBits(&bIsJumpJustPressed, 1);
	Ar.SerializeBits(&bIsJumpPressed, 1);

	bOutSuccess = true;
	return true;
}


void FCharacterDefaultInputs::ToString(FAnsiStringBuilderBase& Out) const
{
	Super::ToString(Out);

	Out.Appendf("MoveInput: %s (Type %i)\n", TCHAR_TO_ANSI(*MoveInput.ToCompactString()), MoveInputType);
	Out.Appendf("OrientationIntent: X=%.2f Y=%.2f Z=%.2f\n", OrientationIntent.X, OrientationIntent.Y, OrientationIntent.Z);
	Out.Appendf("ControlRotation: P=%.2f Y=%.2f R=%.2f\n", ControlRotation.Pitch, ControlRotation.Yaw, ControlRotation.Roll);
	Out.Appendf("SuggestedMovementMode: %s\n", TCHAR_TO_ANSI(*SuggestedMovementMode.ToString()));

	if (MovementBase)
	{
		Out.Appendf("MovementBase: %s (bone %s)\n", TCHAR_TO_ANSI(*GetNameSafe(MovementBase->GetOwner())), TCHAR_TO_ANSI(*MovementBaseBoneName.ToString()));
	}
	else
	{
		Out.Appendf("MovementBase: none\n");
	}

	Out.Appendf("bIsJumpPressed: %i\tbIsJumpJustPressed: %i\n", bIsJumpPressed, bIsJumpJustPressed);
}



// FMoverDefaultSyncState //////////////////////////////////////////////////////////////

FMoverDataStructBase* FMoverDefaultSyncState::Clone() const
{
	// TODO: ensure that this memory allocation jives with deletion method
	FMoverDefaultSyncState* CopyPtr = new FMoverDefaultSyncState(*this);
	return CopyPtr;
}

bool FMoverDefaultSyncState::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	Super::NetSerialize(Ar, Map, bOutSuccess);

	SerializePackedVector<100, 30>(Location, Ar);
	SerializeFixedVector<2, 8>(MoveDirectionIntent, Ar);
	SerializePackedVector<10, 16>(Velocity, Ar);
	Orientation.SerializeCompressedShort(Ar);

	// Optional movement base
	bool bIsUsingMovementBase = (Ar.IsSaving() ? (MovementBase != nullptr) : false);
	Ar.SerializeBits(&bIsUsingMovementBase, 1);

	if (bIsUsingMovementBase)
	{
		Ar << MovementBase;
		Ar << MovementBaseBoneName;

		SerializePackedVector<100, 30>(MovementBasePos, Ar);
		MovementBaseQuat.NetSerialize(Ar, Map, bOutSuccess);
	}
	else if (Ar.IsLoading())
	{
		MovementBase = nullptr;
	}

	bOutSuccess = true;
	return true;
}

void FMoverDefaultSyncState::ToString(FAnsiStringBuilderBase& Out) const
{
	Super::ToString(Out);

	Out.Appendf("Loc: X=%.2f Y=%.2f Z=%.2f\n", Location.X, Location.Y, Location.Z);
	Out.Appendf("Intent: X=%.2f Y=%.2f Z=%.2f\n", MoveDirectionIntent.X, MoveDirectionIntent.Y, MoveDirectionIntent.Z);
	Out.Appendf("Vel: X=%.2f Y=%.2f Z=%.2f\n", Velocity.X, Velocity.Y, Velocity.Z);
	Out.Appendf("Orient: P=%.2f Y=%.2f R=%.2f\n", Orientation.Pitch, Orientation.Yaw, Orientation.Roll);

	if (MovementBase)
	{
		Out.Appendf("MovementBase: %s (bone %s)\n", TCHAR_TO_ANSI(*GetNameSafe(MovementBase->GetOwner())), TCHAR_TO_ANSI(*MovementBaseBoneName.ToString()));
		Out.Appendf("    BasePos: %s   BaseRot: %s\n", TCHAR_TO_ANSI(*MovementBasePos.ToCompactString()), TCHAR_TO_ANSI(*MovementBaseQuat.Rotator().ToCompactString()));
	}
	else
	{
		Out.Appendf("MovementBase: none\n");
	}

}


bool FMoverDefaultSyncState::ShouldReconcile(const FMoverDataStructBase& AuthorityState) const
{
	const FMoverDefaultSyncState* AuthoritySyncState = static_cast<const FMoverDefaultSyncState*>(&AuthorityState);
	const float DistErrorTolerance = 5.f;	// JAH TODO: define these elsewhere as CVars or data asset settings

	const bool bAreInDifferentSpaces = !((MovementBase == AuthoritySyncState->MovementBase) && (MovementBaseBoneName == AuthoritySyncState->MovementBaseBoneName));

	bool bIsNearEnough = false;

	if (!bAreInDifferentSpaces)
	{
		if (MovementBase)
		{
			bIsNearEnough = GetLocation_BaseSpace().Equals(AuthoritySyncState->GetLocation_BaseSpace(), DistErrorTolerance);
		}
		else
		{
			bIsNearEnough = GetLocation_WorldSpace().Equals(AuthoritySyncState->GetLocation_WorldSpace(), DistErrorTolerance);
		}
	}

	return bAreInDifferentSpaces || !bIsNearEnough;
}


void FMoverDefaultSyncState::Interpolate(const FMoverDataStructBase& From, const FMoverDataStructBase& To, float Pct)
{
	const FMoverDefaultSyncState* FromState = static_cast<const FMoverDefaultSyncState*>(&From);
	const FMoverDefaultSyncState* ToState = static_cast<const FMoverDefaultSyncState*>(&To);

	// TODO: investigate replacing this threshold with a flag indicating that the state (or parts thereof) isn't intended to be interpolated
	static constexpr float TeleportThreshold = 500.f * 500.f;
	if (FVector::DistSquared(FromState->Location, ToState->Location) > TeleportThreshold)
	{
		*this = *ToState;
	}
	else
	{

		// No matter what base we started from, we always interpolate into the "To" movement base's space
		MovementBase         = ToState->MovementBase;
		MovementBaseBoneName = ToState->MovementBaseBoneName;
		MovementBasePos      = ToState->MovementBasePos;
		MovementBaseQuat     = ToState->MovementBaseQuat;

		FVector FromLocation_ToSpace, FromIntent_ToSpace, FromVelocity_ToSpace;
		FRotator FromOrientation_ToSpace;

	
		// Bases match (or not using based movement at all)
		if (FromState->MovementBase == ToState->MovementBase && FromState->MovementBaseBoneName == ToState->MovementBaseBoneName)
		{
			if (FromState->MovementBase)
			{
				MovementBasePos = FMath::Lerp(FromState->MovementBasePos, ToState->MovementBasePos, Pct);
				MovementBaseQuat = FQuat::Slerp(FromState->MovementBaseQuat, ToState->MovementBaseQuat, Pct);
			}

			FromLocation_ToSpace    = FromState->Location;
			FromIntent_ToSpace      = FromState->MoveDirectionIntent;
			FromVelocity_ToSpace    = FromState->Velocity;
			FromOrientation_ToSpace = FromState->Orientation;
		}
		else if (ToState->MovementBase)	// if moving onto a different base, regardless of coming from a prior base or not
		{
			UBasedMovementUtils::TransformLocationToLocal(ToState->MovementBasePos, ToState->MovementBaseQuat, FromState->GetLocation_WorldSpace(), OUT FromLocation_ToSpace);
			UBasedMovementUtils::TransformDirectionToLocal(ToState->MovementBaseQuat, FromState->GetIntent_WorldSpace(), OUT FromIntent_ToSpace);
			UBasedMovementUtils::TransformDirectionToLocal(ToState->MovementBaseQuat, FromState->GetVelocity_WorldSpace(), OUT FromVelocity_ToSpace);
			UBasedMovementUtils::TransformRotatorToLocal(ToState->MovementBaseQuat, FromState->GetOrientation_WorldSpace(), OUT FromOrientation_ToSpace);
		}
		else if (FromState->MovementBase)	// if leaving a base
		{
			FromLocation_ToSpace	= FromState->GetLocation_WorldSpace();
			FromIntent_ToSpace		= FromState->GetIntent_WorldSpace();
			FromVelocity_ToSpace	= FromState->GetVelocity_WorldSpace();
			FromOrientation_ToSpace = FromState->GetOrientation_WorldSpace();
		}

		Location			= FMath::Lerp(FromLocation_ToSpace,		ToState->Location, Pct);
		MoveDirectionIntent = FMath::Lerp(FromIntent_ToSpace,		ToState->MoveDirectionIntent, Pct);
		Velocity			= FMath::Lerp(FromVelocity_ToSpace,		ToState->Velocity, Pct);
		Orientation			= FMath::Lerp(FromOrientation_ToSpace,	ToState->Orientation, Pct);

	}
}

void FMoverDefaultSyncState::SetTransforms_WorldSpace(FVector WorldLocation, FRotator WorldOrient, FVector WorldVelocity, UPrimitiveComponent* Base, FName BaseBone)
{
	if (SetMovementBase(Base, BaseBone))
	{
		UBasedMovementUtils::TransformLocationToLocal(  MovementBasePos,  MovementBaseQuat, WorldLocation, OUT Location);
		UBasedMovementUtils::TransformRotatorToLocal(   MovementBaseQuat, WorldOrient, OUT Orientation);
		UBasedMovementUtils::TransformDirectionToLocal( MovementBaseQuat, WorldVelocity, OUT Velocity);
	}
	else
	{
		if (Base)
		{
			UE_LOG(LogMover, Warning, TEXT("Failed to set base as %s. Falling back to world space movement"), *GetNameSafe(Base->GetOwner()));
		}

		Location = WorldLocation;
		Orientation = WorldOrient;
		Velocity = WorldVelocity;
	}
}


bool FMoverDefaultSyncState::SetMovementBase(UPrimitiveComponent* Base, FName BaseBone)
{
	MovementBase = Base;
	MovementBaseBoneName = BaseBone;

	const bool bDidCaptureBaseTransform = UpdateCurrentMovementBase();
	return !Base || bDidCaptureBaseTransform;
}


bool FMoverDefaultSyncState::UpdateCurrentMovementBase()
{
	bool bDidGetBaseTransform = false;

	if (MovementBase)
	{
		bDidGetBaseTransform = UBasedMovementUtils::GetMovementBaseTransform(MovementBase, MovementBaseBoneName, OUT MovementBasePos, OUT MovementBaseQuat);
	}

	if (!bDidGetBaseTransform)
	{
		MovementBase = nullptr;
		MovementBaseBoneName = NAME_None;
		MovementBasePos = FVector::ZeroVector;
		MovementBaseQuat = FQuat::Identity;
	}

	return bDidGetBaseTransform;
}

FVector FMoverDefaultSyncState::GetLocation_WorldSpace() const
{
	if (MovementBase)
	{
		return FTransform(MovementBaseQuat, MovementBasePos).TransformPositionNoScale(Location);
	}

	return Location; // if no base, assumed to be in world space
}

FVector FMoverDefaultSyncState::GetLocation_BaseSpace() const
{
	return Location;
}


FVector FMoverDefaultSyncState::GetIntent_WorldSpace() const
{
	if (MovementBase)
	{
		return MovementBaseQuat.RotateVector(MoveDirectionIntent);
	}

	return MoveDirectionIntent; // if no base, assumed to be in world space
}

FVector FMoverDefaultSyncState::GetIntent_BaseSpace() const
{
	return MoveDirectionIntent;
}

FVector FMoverDefaultSyncState::GetVelocity_WorldSpace() const
{
	if (MovementBase)
	{
		return MovementBaseQuat.RotateVector(Velocity);
	}

	return Velocity; // if no base, assumed to be in world space
}

FVector FMoverDefaultSyncState::GetVelocity_BaseSpace() const
{
	return Velocity;
}


FRotator FMoverDefaultSyncState::GetOrientation_WorldSpace() const
{
	if (MovementBase)
	{
		return (MovementBaseQuat * FQuat(Orientation)).Rotator();
	}

	return Orientation; // if no base, assumed to be in world space
}


FRotator FMoverDefaultSyncState::GetOrientation_BaseSpace() const
{
	return Orientation;
}


// UMoverDataModelBlueprintLibrary ///////////////////////////////////////////////////

void UMoverDataModelBlueprintLibrary::SetMoveIntent(FCharacterDefaultInputs& Inputs, const FVector& WorldDirectionIntent)
{
	Inputs.SetMoveInput(EMoveInputType::DirectionalIntent, WorldDirectionIntent);
}

FVector UMoverDataModelBlueprintLibrary::GetMoveDirectionIntentFromInputs(const FCharacterDefaultInputs& Inputs)
{
	return Inputs.GetMoveInput_WorldSpace();
}

FVector UMoverDataModelBlueprintLibrary::GetLocationFromSyncState(const FMoverDefaultSyncState& SyncState)
{
	return SyncState.GetLocation_WorldSpace();
}

FVector UMoverDataModelBlueprintLibrary::GetMoveDirectionIntentFromSyncState(const FMoverDefaultSyncState& SyncState)
{
	return SyncState.GetIntent_WorldSpace();
}

FVector UMoverDataModelBlueprintLibrary::GetVelocityFromSyncState(const FMoverDefaultSyncState& SyncState)
{
	return SyncState.GetVelocity_WorldSpace();
}

FRotator UMoverDataModelBlueprintLibrary::GetOrientationFromSyncState(const FMoverDefaultSyncState& SyncState)
{
	return SyncState.GetOrientation_WorldSpace();
}