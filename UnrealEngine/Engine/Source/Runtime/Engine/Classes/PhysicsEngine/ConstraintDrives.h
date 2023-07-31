// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "EngineDefines.h"
#include "ConstraintDrives.generated.h"

UENUM()
namespace EAngularDriveMode
{
	enum Type
	{
		/** Spherical lerp between the current orientation/velocity and the target orientation/velocity. NOTE: This will NOT work if any angular constraints are set to Locked. */
		SLERP,
		/** Path is decomposed into twist (roll constraint) and swing (cone constraint). Doesn't follow shortest arc and may experience gimbal lock. Does work with locked angular constraints. */
		TwistAndSwing
	};
}


USTRUCT()
struct ENGINE_API FConstraintDrive
{
	GENERATED_BODY()

	/** The spring strength of the drive. Force proportional to the position error. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = Constraint, meta = (ClampMin = "0.0"))
	float Stiffness;

	/** The damping strength of the drive. Force proportional to the velocity error. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = Constraint, meta = (ClampMin = "0.0"))
	float Damping;

	/** The force limit of the drive. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = Constraint, meta = (ClampMin = "0.0"))
	float MaxForce;

	/** Enables/Disables position drive (orientation if using angular drive)*/
	UPROPERTY(EditAnywhere, Category = Constraint)
	uint8 bEnablePositionDrive : 1;

	/** Enables/Disables velocity drive (angular velocity if using angular drive) */
	UPROPERTY(EditAnywhere, Category = Constraint)
	uint8 bEnableVelocityDrive : 1;

	FConstraintDrive();

	/** Updates physx drive with properties from unreal */
	//void UpdatePhysXDrive_AssumesLocked(physx::PxD6Joint* Joint, int DriveType, bool bDriveEnabled) const;

private:
	friend struct FConstraintInstance;
	friend struct FLinearDriveConstraint;
	friend struct FAngularDriveConstraint;
	//These functions may leave the struct in an invalid state unless calling UpdatePhysX* functions.
	//They are only meant as helpers for FConstraintInstance
	void SetDriveParams(float InStiffness, float InDamping, float InForceLimit);


};

/** Linear Drive */
USTRUCT()
struct ENGINE_API FLinearDriveConstraint
{
	GENERATED_BODY()
	

	/** Target position the linear drive.*/
	UPROPERTY(EditAnywhere, Category = LinearMotor)
	FVector PositionTarget;

	/** Target velocity the linear drive. */
	UPROPERTY(EditAnywhere, Category = LinearMotor)
	FVector VelocityTarget;

	UPROPERTY(EditAnywhere, Category = LinearMotor)
	FConstraintDrive XDrive;

	UPROPERTY(EditAnywhere, Category = LinearMotor)
	FConstraintDrive YDrive;

	UPROPERTY(EditAnywhere, Category = LinearMotor)
	FConstraintDrive ZDrive;

	UPROPERTY(EditAnywhere, Category = LinearMotor)
	uint8 bEnablePositionDrive : 1;

	FLinearDriveConstraint();

	bool IsPositionDriveEnabled() const
	{
		return XDrive.bEnablePositionDrive || YDrive.bEnablePositionDrive || ZDrive.bEnablePositionDrive;
	}

	bool IsVelocityDriveEnabled() const
	{
		return XDrive.bEnableVelocityDrive || YDrive.bEnableVelocityDrive || ZDrive.bEnableVelocityDrive;
	}

private:
	friend struct FConstraintInstance;
	//These functions may leave the struct in an invalid state unless calling UpdatePhysX* functions.
	//They are only meant as helpers for FConstraintInstance
	void SetLinearPositionDrive(bool bEnableXDrive, bool bEnableYDrive, bool bEnableZDrive);
	void SetLinearVelocityDrive(bool bEnableXDrive, bool bEnableYDrive, bool bEnableZDrive);
	void SetDriveParams(float InStiffness, float InDamping, float InForceLimit);
	void SetDriveParams(const FVector& InStiffness, const FVector& InDamping, const FVector& InForceLimit);
	void GetDriveParams(float& OutStiffness, float& OutDamping, float& OutForceLimit) const;
	void GetDriveParams(FVector& OutStiffness, FVector& OutDamping, FVector& OutForceLimit) const;
};


/** Angular Drive */
USTRUCT()
struct ENGINE_API FAngularDriveConstraint
{
	GENERATED_BODY()

	/** Controls the twist (roll) constraint drive between current orientation/velocity and target orientation/velocity. This is available as long as the twist limit is set to free or limited.*/
	UPROPERTY(EditAnywhere, Category = LinearMotor, meta = (DisplayName="Twist"))
	FConstraintDrive TwistDrive;

	/** Controls the cone constraint drive between current orientation/velocity and target orientation/velocity. This is available as long as there is at least one swing limit set to free or limited. */
	UPROPERTY(EditAnywhere, Category = LinearMotor, meta = (DisplayName = "Swing"))
	FConstraintDrive SwingDrive;

	/** Controls the SLERP (spherical lerp) drive between current orientation/velocity and target orientation/velocity. NOTE: This is only available when all three angular limits are either free or limited. Locking any angular limit will turn off the drive implicitly.*/
	UPROPERTY(EditAnywhere, Category = LinearMotor, meta = (DisplayName = "SLERP"))
	FConstraintDrive SlerpDrive;
	
	/** Target orientation relative to the the body reference frame.*/
	UPROPERTY(EditAnywhere, Category = AngularMotor)
	FRotator OrientationTarget;

	/** Target angular velocity relative to the body reference frame in revolutions per second. */
	UPROPERTY(EditAnywhere, Category = AngularMotor)
	FVector AngularVelocityTarget;

	/** Whether motors use SLERP (spherical lerp) or decompose into a Swing motor (cone constraints) and Twist motor (roll constraints). NOTE: SLERP will NOT work if any of the angular constraints are locked. */
	UPROPERTY(EditAnywhere, Category = AngularMotor)
	TEnumAsByte<enum EAngularDriveMode::Type> AngularDriveMode;

	FAngularDriveConstraint();

	bool IsOrientationDriveEnabled() const
	{
		if(AngularDriveMode == EAngularDriveMode::TwistAndSwing)
		{
			return TwistDrive.bEnablePositionDrive || SwingDrive.bEnablePositionDrive;
		}
		else
		{
			return SlerpDrive.bEnablePositionDrive;
		}
	}

	bool IsVelocityDriveEnabled() const
	{
		if (AngularDriveMode == EAngularDriveMode::TwistAndSwing)
		{
			return TwistDrive.bEnableVelocityDrive || SwingDrive.bEnableVelocityDrive;
		}
		else
		{
			return SlerpDrive.bEnableVelocityDrive;
		}
		
	}

	/** Updates physx drive with properties from unreal */
	//void UpdatePhysXAngularDrive_AssumesLocked(physx::PxD6Joint* Joint) const;

private:
	friend struct FConstraintInstance;
	//These functions may leave the struct in an invalid state unless calling UpdatePhysX* functions.
	//They are only meant as helpers for FConstraintInstance
	void SetOrientationDriveTwistAndSwing(bool InEnableTwistDrive, bool InEnableSwingDrive);
	void SetOrientationDriveSLERP(bool InEnableSLERP);
	void SetAngularVelocityDriveTwistAndSwing(bool InEnableTwistDrive, bool InEnableSwingDrive);
	void SetAngularVelocityDriveSLERP(bool InEnableSLERP);
	// Sets the three drive parameters (swing, twist and slerp) to the same value 
	void SetDriveParams(float InStiffness, float InDamping, float InForceLimit);
	// Sets drive parameters in the order swing, twist, slerp
	void SetDriveParams(const FVector& InStiffness, const FVector& InDamping, const FVector& InForceLimit);
	// Gets just the swing drive parameters - assuming the single-float set function has previously been used
	void GetDriveParams(float& OutStiffness, float& OutDamping, float& OutForceLimit) const;
	// Gets drive parameters in the order swing, twist, slerp
	void GetDriveParams(FVector& OutStiffness, FVector& OutDamping, FVector& OutForceLimit) const;
	void SetAngularDriveMode(EAngularDriveMode::Type DriveMode);
};
