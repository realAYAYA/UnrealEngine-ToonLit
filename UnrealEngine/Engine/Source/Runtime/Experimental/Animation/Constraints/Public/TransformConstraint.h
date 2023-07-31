// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Constraint.h"
#include "ConstraintsManager.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "TransformConstraint.generated.h"

enum class EHandleEvent : uint8;
class UTransformableHandle;
class UTransformableComponentHandle;
class USceneComponent;
enum class EMovieSceneTransformChannel : uint32;

/** 
 * UTickableTransformConstraint
 **/

UCLASS(Abstract, Blueprintable)
class CONSTRAINTS_API UTickableTransformConstraint : public UTickableConstraint
{
	GENERATED_BODY()

public:

	/** Sets up the constraint so that the initial offset is set and dependencies and handles managed. */
	void Setup();
	
	/** UObjects overrides. */
	virtual void PostLoad() override;
	virtual void PostDuplicate(bool bDuplicateForPIE) override;

	/** Returns the target hash value (i.e. the child handle's hash). */
	virtual uint32 GetTargetHash() const override;
	
	/** Test whether an InObject is referenced by that constraint. (i.e. is it's parent or child). */
	virtual bool ReferencesObject(TWeakObjectPtr<UObject> InObject) const override;
	/** If true it contains objects bound to an external system, like sequencer so we don't do certain things, like remove constraints when they don't resolve*/
	virtual bool HasBoundObjects() const override;
	
	/** Resolve the bound objects so that any object it references are resovled and correctly set up*/
	virtual void ResolveBoundObjects(FMovieSceneSequenceID LocalSequenceID, IMovieScenePlayer& Player,UObject* SubObject) override;

	/** If Active and the handles and targets are valid*/
	virtual bool IsFullyActive() const override;

	/** If that constraint needs to be handled by the compensation system. */
	virtual bool NeedsCompensation() const;

	/** Create duplicate with new Outer*/
	virtual UTickableConstraint* Duplicate(UObject* NewOuter) const override;

	/** Override the evaluate so we can tick our handles*/
	virtual void Evaluate(bool bTickHandlesAlso = false) const override;

	/** Sets the Active value and enable/disable the tick function. */
	virtual void SetActive(const bool bIsActive) override;
	
	/** The transformable handle representing the parent of that constraint. */
	UPROPERTY(BlueprintReadWrite, Category = "Handle")
	TObjectPtr<UTransformableHandle> ParentTRSHandle;
	
	/** The transformable handle representing the child of that constraint. */
	UPROPERTY(BlueprintReadWrite, Category = "Handle")
	TObjectPtr<UTransformableHandle> ChildTRSHandle;

	/** Should that constraint maintain the default offset.  */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Offset", meta=(EditCondition="Type!=ETransformConstraintType::LookAt"))
	bool bMaintainOffset = true;

	/** Defines how much the constraint will be applied. */
	// UPROPERTY(EditAnywhere, BlueprintReadWrite,Category="Weight", meta = (Input, ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	//@benoit when not EditAnywhere?
	UPROPERTY(meta = (Input, ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float Weight = 1.f;

	/** Should the child be able to change it's offset dynamically. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Offset", meta=(EditCondition="(Type!=ETransformConstraintType::LookAt) && bMaintainOffset"))
	bool bDynamicOffset = false;

	/** Returns the constraint type (Position, Parent, Aim...). */
	int64 GetType() const;

	/** Get the current child's global transform. */
	FTransform GetChildGlobalTransform() const;
	
	/** Get the current child's local transform. */
	FTransform GetChildLocalTransform() const;

	/** Get the current parent's global transform. */
	FTransform GetParentGlobalTransform() const;
	
	/** Get the current parent's local transform. */
	FTransform GetParentLocalTransform() const;

	/** Returns the channels to key based on the constraint's type. */
	EMovieSceneTransformChannel GetChannelsToKey() const;

	/**
	 * Manages changes on the child/parent transformable handle. This can be used to update internal data (i.e. offset)
	 * when transform changes outside of that system and need to trigger changes within the constraint itself.   
	*/
	virtual void OnHandleModified(UTransformableHandle* InHandle, EHandleEvent InEvent);

	/** Returns the handles tick function (ensuring it lives in the same world). */
	FTickFunction* GetChildHandleTickFunction() const;
	FTickFunction* GetParentHandleTickFunction() const;

	/**
	* Sets up dependencies with the first primary prerequisite available if the parent does not tick.   
	*/
	void EnsurePrimaryDependency();
	
protected:

	/** Registers/Unregisters useful delegates for both child and parent handles. */
	void UnregisterDelegates() const;
	void RegisterDelegates();

	
	/**
	 * Computes the initial offset that is needed to keep the child's global transform unchanged when creating the
	 * constraint. This can be called whenever necessary to update the current offset.
	*/
	virtual void ComputeOffset() PURE_VIRTUAL(ComputeOffset, return;);
	
	/**
	 * Sets up dependencies between the parent, the constraint and the child using their respective tick functions.
	 * It creates a dependency graph between them so that they tick in the right order when evaluated.   
	*/
	void SetupDependencies();

	/** Set the current child's global transform. */
	void SetChildGlobalTransform(const FTransform& InGlobal) const;
	
	/** Set the current child's local transform. */
	void SetChildLocalTransform(const FTransform& InLocal) const;

	/** Defines the constraint's type (Position, Parent, Aim...). */
	UPROPERTY()
	ETransformConstraintType Type = ETransformConstraintType::Parent;

	/** Handle active state modification if needed. */
	void OnActiveStateChanged() const;

	/** Returns the handle's tick function (ensuring it lives in the same world). */
	FTickFunction* GetHandleTickFunction(const TObjectPtr<UTransformableHandle>& InHandle) const;

#if WITH_EDITOR
public:
	/** Returns the constraint's label used for UI. */
	virtual FString GetLabel() const override;
	virtual FString GetFullLabel() const override;

	/** Returns the constraint's type label used for UI. */
	virtual FString GetTypeLabel() const override;

	// UObject interface
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	// End of UObject interface
#endif
};

/** 
 * UTickableTranslationConstraint
 **/

UCLASS(Blueprintable)
class CONSTRAINTS_API UTickableTranslationConstraint : public UTickableTransformConstraint
{
	GENERATED_BODY()

public:
	UTickableTranslationConstraint();

	/** Returns the position constraint function that the tick function will evaluate. */
	virtual FConstraintTickFunction::ConstraintFunction GetFunction() const override;

	/** Updates the dynamic offset based on external child's transform changes. */
	virtual void OnHandleModified(UTransformableHandle* InHandle, EHandleEvent InEvent) override;

protected:

	/** Cache data structure to store last child local/global location. */
	struct FDynamicCache
	{
		uint32 CachedInputHash = 0;
	};
	mutable FDynamicCache Cache;
	uint32 CalculateInputHash() const;
	
	/** Computes the child's local translation offset in the parent space. */
	virtual void ComputeOffset() override;

	/** Defines the local child's translation offset in the parent space. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Offset", meta=(EditCondition="bMaintainOffset"))
	FVector OffsetTranslation = FVector::ZeroVector;

#if WITH_EDITOR
public:
	// UObject interface
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	// End of UObject interface
#endif
};

/** 
 * UTickableRotationConstraint
 **/

UCLASS(Blueprintable)
class CONSTRAINTS_API UTickableRotationConstraint : public UTickableTransformConstraint
{
	GENERATED_BODY()

public:
	UTickableRotationConstraint();

	/** Returns the rotation constraint function that the tick function will evaluate. */
	virtual FConstraintTickFunction::ConstraintFunction GetFunction() const override;

	/** Updates the dynamic offset based on external child's transform changes. */
	virtual void OnHandleModified(UTransformableHandle* InHandle, EHandleEvent InEvent) override;

protected:

	/** Cache data structure to store last child local/global rotation. */
	struct FDynamicCache
	{
		uint32 CachedInputHash = 0;
	};
	mutable FDynamicCache Cache;
	uint32 CalculateInputHash() const;
	
	/** Computes the child's local rotation offset in the parent space. */
	virtual void ComputeOffset() override;
	
	/** Defines the local child's rotation offset in the parent space. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Offset", meta=(EditCondition="bMaintainOffset"))
	FQuat OffsetRotation = FQuat::Identity;

#if WITH_EDITOR
public:
	// UObject interface
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	// End of UObject interface
#endif
};

/** 
 * UTickableScaleConstraint
 **/

UCLASS(Blueprintable)
class CONSTRAINTS_API UTickableScaleConstraint : public UTickableTransformConstraint
{
	GENERATED_BODY()

public:
	UTickableScaleConstraint();
	
	/** Returns the scale constraint function that the tick function will evaluate. */
	virtual FConstraintTickFunction::ConstraintFunction GetFunction() const override;

	/** Updates the dynamic offset based on external child's transform changes. */
	virtual void OnHandleModified(UTransformableHandle* InHandle, EHandleEvent InEvent) override;
	
protected:
	/** Computes the child's local scale offset in the parent space. */
	virtual void ComputeOffset() override;

	/** Cache data structure to store last child local/global transform. */
	struct FDynamicCache
	{
		uint32 CachedInputHash = 0;
	};
	mutable FDynamicCache Cache;
	uint32 CalculateInputHash() const;
	
	/** Defines the local child's scale offset in the parent space. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Offset", meta=(EditCondition="bMaintainOffset"))
	FVector OffsetScale = FVector::OneVector;

#if WITH_EDITOR
public:
	// UObject interface
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	// End of UObject interface
#endif
};

/** 
 * UTickableParentConstraint
 **/

UCLASS(Blueprintable)
class CONSTRAINTS_API UTickableParentConstraint : public UTickableTransformConstraint
{
	GENERATED_BODY()

public:
	UTickableParentConstraint();
	
	/** Returns the transform constraint function that the tick function will evaluate. */
	virtual FConstraintTickFunction::ConstraintFunction GetFunction() const override;

	bool IsScalingEnabled() const
	{
		return bScaling;
	}
	

	/** Updates the dynamic offset based on external child's transform changes. */
	virtual void OnHandleModified(UTransformableHandle* InHandle, EHandleEvent InEvent) override;

protected:
	/** Cache data structure to store last child local/global transform. */
	struct FDynamicCache
	{
		uint32 CachedInputHash = 0;
	};
	mutable FDynamicCache Cache;
	uint32 CalculateInputHash() const;

	/** Computes the child's local transform offset in the parent space. */
	virtual void ComputeOffset() override;

	/** Defines the local child's transform offset in the parent space. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Offset", meta=(EditCondition="bMaintainOffset"))
	FTransform OffsetTransform = FTransform::Identity;

	/** Defines whether we propagate the parent scale. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Properties")
	bool bScaling = false;

#if WITH_EDITOR
public:
	// UObject interface
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	// End of UObject interface
#endif
};

/** 
 * UTickableLookAtConstraint
 **/

UCLASS(Blueprintable)
class CONSTRAINTS_API UTickableLookAtConstraint : public UTickableTransformConstraint
{
	GENERATED_BODY()

public:
	UTickableLookAtConstraint();
	
	/** Returns the look at constraint function that the tick function will evaluate. */
	virtual FConstraintTickFunction::ConstraintFunction GetFunction() const override;

	/** If that constraint needs to be handled by the compensation system. */
	virtual bool NeedsCompensation() const override;

protected:
	/**
     * Computes the initial axis that is needed to keep the child's orientation unchanged when creating the constraint.
    */
	virtual void ComputeOffset() override;

	/** Defines the aiming axis. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Axis")
	FVector Axis = FVector::XAxisVector;

private:

	/** Computes the shortest quaternion between A and B. */
	static FQuat FindQuatBetweenNormals(const FVector& A, const FVector& B);

#if WITH_EDITOR
public:
	// UObject interface
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	// End of UObject interface
#endif
};

/** 
 * TransformConstraintUtils
 **/

struct CONSTRAINTS_API FTransformConstraintUtils
{
	/** Fills a sorted constraint array that InChild actor is the child of. */
	static void GetParentConstraints(
		UWorld* World,
		const AActor* InChild,
		TArray< TObjectPtr<UTickableConstraint> >& OutConstraints);

	/** Create a handle for the scene component.*/
	static UTransformableComponentHandle* CreateHandleForSceneComponent(
		USceneComponent* InSceneComponent,
		const FName& InSocketName,
		UObject* Outer);

	/** Creates a new transform constraint based on the InType. */
	static UTickableTransformConstraint* CreateFromType(
		UWorld* InWorld,
		const ETransformConstraintType InType);

	/** Creates respective handles and creates a new InType transform constraint. */	
	static UTickableTransformConstraint* CreateAndAddFromActors(
		UWorld* InWorld,
		AActor* InParent,
		const FName& InSocketName,
		AActor* InChild,
		const ETransformConstraintType InType,
		const bool bMaintainOffset = true);

	/** Registers a new transform constraint within the constraints manager. */	
	static bool AddConstraint(
		UWorld* InWorld,
		UTransformableHandle* InParentHandle,
		UTransformableHandle* InChildHandle,
		UTickableTransformConstraint* Constraint,
		const bool bMaintainOffset = true);

	/** Computes the relative transform between both transform based on the constraint's InType. */
	static FTransform ComputeRelativeTransform(
		const FTransform& InChildLocal,
		const FTransform& InChildWorld,
		const FTransform& InSpaceWorld,
		const UTickableTransformConstraint* InConstraint);

	/** Computes the current constraint space local transform. */
	static TOptional<FTransform> GetRelativeTransform(UWorld* InWorld, const uint32 InHandleHash);
	static TOptional<FTransform> GetConstraintsRelativeTransform(
		const TArray< TObjectPtr<UTickableConstraint> >& InConstraints,
		const FTransform& InChildLocal, const FTransform& InChildWorld);

	/** Get the last active constraint that has dynamic offset. */
	static int32 GetLastActiveConstraintIndex(const TArray< TObjectPtr<UTickableConstraint> >& InConstraints);

	/** Fills a constraint array that InParentHandle is the parent of. */
	static void GetChildrenConstraints(
		UWorld* World,
		const UTransformableHandle* InParentHandle,
		TArray< TObjectPtr<UTickableConstraint> >& OutConstraints);
};
