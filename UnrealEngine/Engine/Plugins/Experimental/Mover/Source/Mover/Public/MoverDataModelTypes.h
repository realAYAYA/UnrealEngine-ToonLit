// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MoverTypes.h"
#include "LayeredMove.h"
#include "MoverDataModelTypes.generated.h"





// Used to identify how to interpret a movement input vector's values
UENUM(BlueprintType)
enum class EMoveInputType : uint8
{
	Invalid,

	/** Move with intent, as a per-axis magnitude [-1,1] (E.g., "move straight forward as fast as possible" would be (1, 0, 0) and "move straight left at half speed" would be (0, -0.5, 0) regardless of frame time). Zero vector indicates intent to stop. */
	DirectionalIntent,

	/** Move with a given velocity (units per second) */
	Velocity,
};


// Data block containing all inputs that need to be authored and consumed for the default Mover character simulation
USTRUCT(BlueprintType)
struct MOVER_API FCharacterDefaultInputs : public FMoverDataStructBase
{
	GENERATED_USTRUCT_BODY()

	// Sets the directional move inputs for a simulation frame
	void SetMoveInput(EMoveInputType InMoveInputType, const FVector& InMoveInput);

	const FVector& GetMoveInput() const { return MoveInput; }
	EMoveInputType GetMoveInputType() const { return MoveInputType; }

protected:
	UPROPERTY(BlueprintReadWrite, Category = Mover)
	EMoveInputType MoveInputType;

	// Representing the directional move input for this frame. Must be interpreted according to MoveInputType. Relative to MovementBase if set, world space otherwise. Will be truncated to match network serialization precision.
	UPROPERTY(BlueprintReadWrite, Category = Mover)
	FVector MoveInput;

public:
	// Facing direction intent, as a normalized forward-facing direction. A zero vector indicates no intent to change facing direction. Relative to MovementBase if set, world space otherwise.
	UPROPERTY(BlueprintReadWrite, Category = Mover)
	FVector OrientationIntent;

	// World space orientation that the controls were based on. This is commonly a player's camera rotation.
	UPROPERTY(BlueprintReadWrite, Category = Mover)
	FRotator ControlRotation;

	// Used to force the Mover actor into a different movement mode
	UPROPERTY(BlueprintReadWrite, Category = Mover)
	FName SuggestedMovementMode;

	// Specifies whether we are using a movement base, which will affect how move inputs are interpreted
	UPROPERTY(BlueprintReadWrite, Category = Mover)
	bool bUsingMovementBase;

	// Optional: when moving on a base, input may be relative to this object
	UPROPERTY(BlueprintReadWrite, Category = Mover)
	TObjectPtr<UPrimitiveComponent> MovementBase;

	// Optional: for movement bases that are skeletal meshes, this is the bone we're based on. Only valid if MovementBase is set.
	UPROPERTY(BlueprintReadWrite, Category = Mover)
	FName MovementBaseBoneName;

	UPROPERTY(BlueprintReadWrite, Category = Mover)
	bool bIsJumpJustPressed;
	UPROPERTY(BlueprintReadWrite, Category = Mover)
	bool bIsJumpPressed;

	FVector GetMoveInput_WorldSpace() const;
	FVector GetOrientationIntentDir_WorldSpace() const;


	FCharacterDefaultInputs()
		: MoveInputType(EMoveInputType::Invalid)
		, MoveInput(ForceInitToZero)
		, OrientationIntent(ForceInitToZero)
		, ControlRotation(ForceInitToZero)
		, SuggestedMovementMode(NAME_None)
		, bUsingMovementBase(false)
		, MovementBase(nullptr)
		, MovementBaseBoneName(NAME_None)
		, bIsJumpJustPressed(false)
		, bIsJumpPressed(false)
	{
	}

	virtual ~FCharacterDefaultInputs() {}

	// @return newly allocated copy of this FCharacterDefaultInputs. Must be overridden by child classes
	virtual FMoverDataStructBase* Clone() const override;

	virtual bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess) override;

	virtual UScriptStruct* GetScriptStruct() const override { return StaticStruct(); }

	virtual void ToString(FAnsiStringBuilderBase& Out) const override;

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override { Super::AddReferencedObjects(Collector); }


};

template<>
struct TStructOpsTypeTraits< FCharacterDefaultInputs > : public TStructOpsTypeTraitsBase2< FCharacterDefaultInputs >
{
	enum
	{
		WithNetSerializer = true,
		WithCopy = true
	};
};




// Data block containing basic sync state information
USTRUCT(BlueprintType)
struct MOVER_API FMoverDefaultSyncState : public FMoverDataStructBase
{
	GENERATED_USTRUCT_BODY()
protected:
	// Position relative to MovementBase if set, world space otherwise
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Mover)
	FVector Location;

	// Forward-facing rotation relative to MovementBase if set, world space otherwise.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Mover)
	FRotator Orientation;

	// Linear velocity, units per second, relative to MovementBase if set, world space otherwise.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Mover)
	FVector Velocity;

public:
	// Movement intent direction relative to MovementBase if set, world space otherwise. Magnitude of range (0-1)
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Mover)
	FVector MoveDirectionIntent;

protected:
	// Optional: when moving on a base, input may be relative to this object
	UPROPERTY(BlueprintReadOnly, Category = Mover)
	TObjectPtr<UPrimitiveComponent> MovementBase;

	// Optional: for movement bases that are skeletal meshes, this is the bone we're based on. Only valid if MovementBase is set.
	UPROPERTY(BlueprintReadOnly, Category = Mover)
	FName MovementBaseBoneName;

	UPROPERTY(BlueprintReadOnly, Category = Mover)
	FVector MovementBasePos;

	UPROPERTY(BlueprintReadOnly, Category = Mover)
	FQuat MovementBaseQuat;

public:

	FMoverDefaultSyncState()
		: Location(ForceInitToZero)
		, Orientation(ForceInitToZero)
		, Velocity(ForceInitToZero)
		, MoveDirectionIntent(ForceInitToZero)
		, MovementBase(nullptr)
		, MovementBaseBoneName(NAME_None)
		, MovementBasePos(ForceInitToZero)
		, MovementBaseQuat(FQuat::Identity)
	{
	}

	virtual ~FMoverDefaultSyncState() {}

	// @return newly allocated copy of this FMoverDefaultSyncState. Must be overridden by child classes
	virtual FMoverDataStructBase* Clone() const override;

	virtual bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess) override;

	virtual UScriptStruct* GetScriptStruct() const override { return StaticStruct(); }

	virtual void ToString(FAnsiStringBuilderBase& Out) const override;

	virtual bool ShouldReconcile(const FMoverDataStructBase& AuthorityState) const override;

	virtual void Interpolate(const FMoverDataStructBase& From, const FMoverDataStructBase& To, float Pct) override;

	void SetTransforms_WorldSpace(FVector WorldLocation, FRotator WorldOrient, FVector WorldVelocity, UPrimitiveComponent* Base=nullptr, FName BaseBone = NAME_None);

	// Returns whether the base setting succeeded
	bool SetMovementBase(UPrimitiveComponent* Base, FName BaseBone=NAME_None);

	// Refreshes captured movement base transform based on its current state, while maintaining the same base-relative transforms
	bool UpdateCurrentMovementBase();

	// Queries
	UPrimitiveComponent* GetMovementBase() const { return MovementBase; }
	FName GetMovementBaseBoneName() const { return MovementBaseBoneName; }
	FVector GetCapturedMovementBasePos() const { return MovementBasePos; }
	FQuat GetCapturedMovementBaseQuat() const { return MovementBaseQuat; }

	FVector GetLocation_WorldSpace() const;
	FVector GetLocation_BaseSpace() const;	// If there is no movement base set, these will be the same as world space

	FVector GetIntent_WorldSpace() const;
	FVector GetIntent_BaseSpace() const;

	FVector GetVelocity_WorldSpace() const;
	FVector GetVelocity_BaseSpace() const;

	FRotator GetOrientation_WorldSpace() const;
	FRotator GetOrientation_BaseSpace() const;

};

template<>
struct TStructOpsTypeTraits< FMoverDefaultSyncState > : public TStructOpsTypeTraitsBase2< FMoverDefaultSyncState >
{
	enum
	{
		WithNetSerializer = true,
		WithCopy = true
	};
};


/**
 * Blueprint function library to make it easier to work with Mover data structs, since we can't add UFUNCTIONs to structs
 */
UCLASS()
class MOVER_API UMoverDataModelBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:	// FCharacterDefaultInputs

	/** Sets move inputs from worldspace intent, as a per-axis magnitude in the range [-1,1] Zero vector indicates intent to stop. */
	UFUNCTION(BlueprintCallable, Category = Mover)
	static void SetMoveIntent(UPARAM(Ref) FCharacterDefaultInputs& Inputs, const FVector& WorldDirectionIntent);

	/** Returns the move direction intent, if any, in world space */
	UFUNCTION(BlueprintCallable, Category = Mover)
	static FVector GetMoveDirectionIntentFromInputs(const FCharacterDefaultInputs& Inputs);


public:	// FMoverDefaultSyncState

	/** Returns the location in world space */
	UFUNCTION(BlueprintCallable, Category = Mover)
	static FVector GetLocationFromSyncState(const FMoverDefaultSyncState& SyncState);

	/** Returns the move direction intent, if any, in world space */
	UFUNCTION(BlueprintCallable, Category = Mover)
	static FVector GetMoveDirectionIntentFromSyncState(const FMoverDefaultSyncState& SyncState);

	/** Returns the velocity in world space */
	UFUNCTION(BlueprintCallable, Category = Mover)
	static FVector GetVelocityFromSyncState(const FMoverDefaultSyncState& SyncState);

	/** Returns the orientation in world space */
	UFUNCTION(BlueprintCallable, Category = Mover)
	static FRotator GetOrientationFromSyncState(const FMoverDefaultSyncState& SyncState);
};