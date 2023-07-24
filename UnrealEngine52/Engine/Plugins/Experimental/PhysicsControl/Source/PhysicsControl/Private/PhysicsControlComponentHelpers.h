// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/NameTypes.h"
#include "Math/Vector.h"

struct FBodyInstance;
struct FPhysicsControlData;
struct FConstraintProfileProperties;

class UMeshComponent;
class USkeletalMeshComponent;

namespace UE
{
namespace PhysicsControlComponent
{

/** 
 * Converts strength/damping ratio/extra damping into spring stiffness/damping 
 */
void ConvertStrengthToSpringParams(
	FVector& OutSpring, FVector& OutDamping, 
	const FVector& InStrength, float InDampingRatio, const FVector& InExtraDamping);

/**
 * Converts the drive settings from the constraint profile into the control data strength/damping etc. 
 * Approximations will be made if (a) the linear drive has different values for the x/y/z axes, or (b)
 * the constraint profile is set to use twist/swing instead of slerp for the angular drive.
 */
void ConvertConstraintProfileToControlData(
	FPhysicsControlData& OutControlData, const FConstraintProfileProperties& InProfileProperties);

/** 
 * Attempts to find a BodyInstance from the mesh. If it is a static mesh the single body instance
 * will be returned. If it is a skeletal mesh then if BoneName can be found, the body instance corresponding
 * to that bone will be returned. Otherwise it will return nullptr if the bone can't be found.
 */
FBodyInstance* GetBodyInstance(UMeshComponent* MeshComponent, const FName BoneName);

/**
 * Attempts to find the parent physical bone given a skeletal mesh and starting bone. This walks up
 * the hierarchy, ignoring non-physical bones, until either a physical bone is found, or it has reached
 * the root without finding a physical bone.
 */
FName GetPhysicalParentBone(USkeletalMeshComponent* SkeletalMeshComponent, FName BoneName);

/**
 * Converts strength/damping ratio/extra damping into spring stiffness/damping.
 */
template<typename TOut>
void ConvertStrengthToSpringParams(
	TOut& OutSpring, TOut& OutDamping,
	double InStrength, double InDampingRatio, double InExtraDamping)
{
	TOut AngularFrequency = FloatCastChecked<TOut>(InStrength * UE_DOUBLE_TWO_PI, UE::LWC::DefaultFloatPrecision);
	TOut Stiffness = AngularFrequency * AngularFrequency;

	OutSpring = Stiffness;
	OutDamping = FloatCastChecked<TOut>(InExtraDamping + 2 * InDampingRatio * AngularFrequency, UE::LWC::DefaultFloatPrecision);
}


/**
 * Converts spring/damping values into strength/damping ratio/extra damping. This tries to get
 * as much damping into the damping ratio term as possible, without letting it go above 1.
 */
template<typename TOut>
static void ConvertSpringToStrengthParams(
	TOut& OutStrength, TOut& OutDampingRatio, TOut& OutExtraDamping,
	const double InSpring, const double InDamping)
{
	// Simple calculation to get the strength
	TOut AngularFrequency = FloatCastChecked<TOut>(FMath::Sqrt(InSpring), UE::LWC::DefaultFloatPrecision);
	OutStrength = AngularFrequency / UE_TWO_PI;

	// For damping, try to put as much into the damping ratio as possible, up to a max DR of 1. Then
	// the rest goes into extra damping.
	OutDampingRatio = 1;
	TOut ImpliedDamping = FloatCastChecked<TOut>(2.0 * OutDampingRatio * AngularFrequency, UE::LWC::DefaultFloatPrecision);

	if (ImpliedDamping < InDamping)
	{
		OutExtraDamping = FloatCastChecked<TOut>(InDamping, UE::LWC::DefaultFloatPrecision) - ImpliedDamping;
	}
	else if (AngularFrequency > 0)
	{
		OutExtraDamping = 0;
		OutDampingRatio = FloatCastChecked<TOut>(InDamping, UE::LWC::DefaultFloatPrecision) / (2 * AngularFrequency);
	}
	else
	{
		OutDampingRatio = 1;
		OutExtraDamping = FloatCastChecked<TOut>(InDamping, UE::LWC::DefaultFloatPrecision);
	}
}

} // namespace PhysicsControlComponent
} // namespace UE
