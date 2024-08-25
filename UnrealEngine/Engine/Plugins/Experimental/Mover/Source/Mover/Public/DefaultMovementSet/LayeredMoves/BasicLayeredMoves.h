// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once


#include "LayeredMove.h"
#include "BasicLayeredMoves.generated.h"

class UCurveVector;
class UCurveFloat;

UENUM(BlueprintType)
enum class ELayeredMove_ConstantVelocitySettingsFlags : uint8
{
	NoFlags						= 0,

	// Velocity direction is determined when this move starts, relative to the target actor. This flag is incompatible with other Velocity**** flags
	VelocityStartRelative		= (1 << 0),
	// Velocity direction is always relative to the target actor, even if its orientation changes. This flag is incompatible with other Velocity**** flags
	VelocityAlwaysRelative		= (1 << 1),

};

/** Linear Velocity: A method of inducing a straight-line velocity on an actor over time  */
USTRUCT(BlueprintType)
struct MOVER_API FLayeredMove_LinearVelocity : public FLayeredMoveBase
{
	GENERATED_USTRUCT_BODY()

	FLayeredMove_LinearVelocity();

	virtual ~FLayeredMove_LinearVelocity() {}

	// Units per second, could be worldspace vs relative depending on Flags
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	FVector Velocity;

	// Optional curve float for controlling the magnitude of the velocity applied to the actor over the duration of the move
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	TObjectPtr<UCurveFloat> MagnitudeOverTime;
	
	// @see ELayeredMove_ConstantVelocitySettingsFlags
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	uint8 SettingsFlags;

	// Generate a movement 
	virtual bool GenerateMove(const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, const UMoverComponent* MoverComp, UMoverBlackboard* SimBlackboard, FProposedMove& OutProposedMove) override;

	virtual FLayeredMoveBase* Clone() const override;

	virtual void NetSerialize(FArchive& Ar) override;

	virtual UScriptStruct* GetScriptStruct() const override;

	virtual FString ToSimpleString() const override;

	virtual void AddReferencedObjects(class FReferenceCollector& Collector) override;
};

template<>
struct TStructOpsTypeTraits< FLayeredMove_LinearVelocity > : public TStructOpsTypeTraitsBase2< FLayeredMove_LinearVelocity >
{
	enum
	{
		//WithNetSerializer = true,
		WithCopy = true
	};
};


/** Jump Impulse: introduces an instantaneous upwards change in velocity. This overrides the existing 'up' component of the actor's current velocity */
USTRUCT(BlueprintType)
struct MOVER_API FLayeredMove_JumpImpulse : public FLayeredMoveBase
{
	GENERATED_USTRUCT_BODY()

	FLayeredMove_JumpImpulse();

	virtual ~FLayeredMove_JumpImpulse() {}

	// Units per second, in whatever direction the target actor considers 'up'
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	float UpwardsSpeed;

	// Generate a movement 
	virtual bool GenerateMove(const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, const UMoverComponent* MoverComp, UMoverBlackboard* SimBlackboard, FProposedMove& OutProposedMove) override;

	virtual FLayeredMoveBase* Clone() const override;

	virtual void NetSerialize(FArchive& Ar) override;

	virtual UScriptStruct* GetScriptStruct() const override;

	virtual FString ToSimpleString() const override;

	virtual void AddReferencedObjects(class FReferenceCollector& Collector) override;
};

template<>
struct TStructOpsTypeTraits< FLayeredMove_JumpImpulse > : public TStructOpsTypeTraitsBase2< FLayeredMove_JumpImpulse >
{
	enum
	{
		//WithNetSerializer = true,
		WithCopy = true
	};
};

/** JumpTo: Moves this actor in a more jump-like manner - specifying height and distance of jump rather than just upwards speed
  * Note: this layered move is only intended for Mover actors using Z as it's up direction
  */
USTRUCT(BlueprintType)
struct MOVER_API FLayeredMove_JumpTo : public FLayeredMoveBase
{
	GENERATED_USTRUCT_BODY()

	FLayeredMove_JumpTo();
	virtual ~FLayeredMove_JumpTo() {}
	
	// Distance this jump impulse is supposed to reach
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	float JumpDistance;

	// Height this jump impulse is supposed to reach
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	float JumpHeight;

	// If true this jump will use the actor's rotation for jump direction instead of the Jump Rotation property.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	bool bUseActorRotation;
	
	// Direction to jump in. Only used if bUseActorRotation is false
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	FRotator JumpRotation;
	
	// Optional CurveVector used to offset the actor from the path
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	TObjectPtr<UCurveVector> PathOffsetCurve;
	
	// Optional CurveFloat to modify jump impulse over the duration of the impulse
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	TObjectPtr<UCurveFloat> TimeMappingCurve;
	
	// Generate a movement 
	virtual bool GenerateMove(const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, const UMoverComponent* MoverComp, UMoverBlackboard* SimBlackboard, FProposedMove& OutProposedMove) override;
	
	virtual FLayeredMoveBase* Clone() const override;
	
	virtual void NetSerialize(FArchive& Ar) override;
	
	virtual UScriptStruct* GetScriptStruct() const override;
	
	virtual FString ToSimpleString() const override;

	virtual void AddReferencedObjects(class FReferenceCollector& Collector) override;
protected:
	// helper function to apply movement vector offset from the PathOffsetCurve
	FVector GetPathOffset(const float MoveFraction) const;

	// Helper function to apply TimeMappingCurve to the layered move
	float EvaluateFloatCurveAtFraction(const UCurveFloat& Curve, const float Fraction) const;

	// Helper function for calculating position in jump given move fraction of total time for move
	FVector GetRelativeLocation(float MoveFraction, const FRotator& Rotator) const;
};

template<>
struct TStructOpsTypeTraits< FLayeredMove_JumpTo > : public TStructOpsTypeTraitsBase2< FLayeredMove_JumpTo >
{
	enum
	{
		//WithNetSerializer = true,
		WithCopy = true
	};
};

/** Teleport: instantly moves an actor to a new location */
USTRUCT(BlueprintType)
struct MOVER_API FLayeredMove_Teleport : public FLayeredMoveBase
{
	GENERATED_USTRUCT_BODY()

	FLayeredMove_Teleport();
	virtual ~FLayeredMove_Teleport() {}

	// Location to teleport to, in world space
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	FVector TargetLocation;

	// Generate a movement 
	virtual bool GenerateMove(const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, const UMoverComponent* MoverComp, UMoverBlackboard* SimBlackboard, FProposedMove& OutProposedMove) override;

	virtual FLayeredMoveBase* Clone() const override;

	virtual void NetSerialize(FArchive& Ar) override;

	virtual UScriptStruct* GetScriptStruct() const override;

	virtual FString ToSimpleString() const override;

	virtual void AddReferencedObjects(class FReferenceCollector& Collector) override;
};

template<>
struct TStructOpsTypeTraits< FLayeredMove_Teleport > : public TStructOpsTypeTraitsBase2< FLayeredMove_Teleport >
{
	enum
	{
		//WithNetSerializer = true,
		WithCopy = true
	};
};


/** MoveTo: Move Actor from the starting location to the target location over a duration of time.*/
USTRUCT(BlueprintType)
struct MOVER_API FLayeredMove_MoveTo : public FLayeredMoveBase
{
	GENERATED_USTRUCT_BODY()

	FLayeredMove_MoveTo();
	virtual ~FLayeredMove_MoveTo() {}

	// Location to Start the MoveTo move from
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	FVector StartLocation;
	
	// Location to move towards
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	FVector TargetLocation;

	// if true, will restrict speed to where the actor is expected to be (in regard to start, end and duration)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	bool bRestrictSpeedToExpected;
	
	// Optional CurveVector used to offset the actor from the path
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	TObjectPtr<UCurveVector> PathOffsetCurve;

	// Optional CurveFloat to apply to how fast the actor moves as they get closer to the target location
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	TObjectPtr<UCurveFloat> TimeMappingCurve;
	
	// Generate a movement 
	virtual bool GenerateMove(const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, const UMoverComponent* MoverComp, UMoverBlackboard* SimBlackboard, FProposedMove& OutProposedMove) override;

	virtual FLayeredMoveBase* Clone() const override;

	virtual void NetSerialize(FArchive& Ar) override;

	virtual UScriptStruct* GetScriptStruct() const override;

	virtual FString ToSimpleString() const override;

	virtual void AddReferencedObjects(class FReferenceCollector& Collector) override;
	
protected:
	// helper function to apply movement vector offset from the PathOffsetCurve
	FVector GetPathOffsetInWorldSpace(const float MoveFraction) const;

	// Helper function to apply TimeMappingCurve to the layered move
	float EvaluateFloatCurveAtFraction(const UCurveFloat& Curve, const float Fraction) const;
};

template<>
struct TStructOpsTypeTraits< FLayeredMove_MoveTo > : public TStructOpsTypeTraitsBase2< FLayeredMove_MoveTo >
{
	enum
	{
		//WithNetSerializer = true,
		WithCopy = true
	};
};


/** MoveToDynamic: Move Actor from the starting location to the target location over a duration of time with a moving target location
 *  You can either update TargetLocation manually to update the location this actor is moving towards or set an actor for this actor to move towards.
 */
USTRUCT(BlueprintType)
struct MOVER_API FLayeredMove_MoveToDynamic : public FLayeredMove_MoveTo
{
	GENERATED_USTRUCT_BODY()

	FLayeredMove_MoveToDynamic();
	virtual ~FLayeredMove_MoveToDynamic() {}
	
	// Optional actor to move to. Note: this overrides the TargetLocation Property
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	TObjectPtr<AActor> LocationActor;

	// Generate a movement 
	virtual bool GenerateMove(const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, const UMoverComponent* MoverComp, UMoverBlackboard* SimBlackboard, FProposedMove& OutProposedMove) override;

	virtual FLayeredMoveBase* Clone() const override;

	virtual void NetSerialize(FArchive& Ar) override;

	virtual UScriptStruct* GetScriptStruct() const override;

	virtual FString ToSimpleString() const override;

	virtual void AddReferencedObjects(class FReferenceCollector& Collector) override;
};

template<>
struct TStructOpsTypeTraits< FLayeredMove_MoveToDynamic > : public TStructOpsTypeTraitsBase2< FLayeredMove_MoveToDynamic >
{
	enum
	{
		//WithNetSerializer = true,
		WithCopy = true
	};
};

/** RadialImpulse applies a velocity pulling or pushing away from a given world location to the target actor */
USTRUCT(BlueprintType)
struct MOVER_API FLayeredMove_RadialImpulse : public FLayeredMoveBase
{
	GENERATED_USTRUCT_BODY()

	FLayeredMove_RadialImpulse();
	virtual ~FLayeredMove_RadialImpulse() {}

	// Location to pull or push actor from
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	FVector Location;

	// Optional Actor to pull or push this actor from. Note: this overrides the Location Property
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	TObjectPtr<AActor> LocationActor;

	// Radius from the location to apply radial velocity
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	float Radius;

	// Magnitude of velocity applied to actors in range
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	float Magnitude;

	// If true the velocity applied will push actor away from location, otherwise this layered move wil pull the actor towards it
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	bool bIsPush;

	// Whether to apply vertical velocity (based off mover components up direction) to the actor affected by this layered move
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	bool bNoVerticalVelocity;

	// Optional Curvefloat to dictate falloff of velocity as you get further from the velocity location
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	TObjectPtr<UCurveFloat> DistanceFalloff;

	// Optional Curvefloat to dictate magnitude of velocity applied over time
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	TObjectPtr<UCurveFloat> MagnitudeOverTime;

	// If true velocity added to the actor will be in the direction of FixedWorldDirection, otherwise it will be calculated from the position of the actor and velocity location.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	bool bUseFixedWorldDirection;

	// Direction to apply velocity from the radial impulse in. This is only taken into account if UseFixedWorldDirection is true.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	FRotator FixedWorldDirection;
	
	// Generate a movement 
	virtual bool GenerateMove(const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, const UMoverComponent* MoverComp, UMoverBlackboard* SimBlackboard, FProposedMove& OutProposedMove) override;

	virtual FLayeredMoveBase* Clone() const override;

	virtual void NetSerialize(FArchive& Ar) override;

	virtual UScriptStruct* GetScriptStruct() const override;

	virtual FString ToSimpleString() const override;

	virtual void AddReferencedObjects(class FReferenceCollector& Collector) override;
};

template<>
struct TStructOpsTypeTraits< FLayeredMove_RadialImpulse > : public TStructOpsTypeTraitsBase2< FLayeredMove_RadialImpulse >
{
	enum
	{
		//WithNetSerializer = true,
		WithCopy = true
	};
};
