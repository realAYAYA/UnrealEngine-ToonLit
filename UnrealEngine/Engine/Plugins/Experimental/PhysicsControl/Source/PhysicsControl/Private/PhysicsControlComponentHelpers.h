// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/NameTypes.h"

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
 * Converts strength/damping ratio/extra damping into spring stiffness/damping.
 */
void ConvertStrengthToSpringParams(
	double& OutSpring, double& OutDamping, 
	double InStrength, double InDampingRatio, double InExtraDamping);

/** 
 * Converts strength/damping ratio/extra damping into spring stiffness/damping 
 */
void ConvertStrengthToSpringParams(
	FVector& OutSpring, FVector& OutDamping, 
	const FVector& InStrength, float InDampingRatio, const FVector& InExtraDamping);

/**
 * Converts spring/damping values into strength/damping ratio/extra damping. This tries to get
 * as much damping into the damping ratio term as possible, without letting it go above 1.
 */
void ConvertSpringToStrengthParams(
	double& OutStrength, double& OutDampingRatio, double& OutExtraDamping,
	double InSpring, double InDamping);

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

} // namespace PhysicsControlComponent
} // namespace UE
