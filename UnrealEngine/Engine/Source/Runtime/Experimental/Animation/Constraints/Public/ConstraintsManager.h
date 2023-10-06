﻿// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"
#include "Engine/EngineBaseTypes.h"
#include "MovieSceneSequenceID.h"
#include "ConstraintsManager.generated.h"

class IMovieScenePlayer;
class UTickableConstraint;
class ULevel;

/** 
 * FConstraintTickFunction
 * Represents the interface of constraint as a tick function. This allows both to evaluate a constraint in the
 * UE ticking system but also to handle dependencies between parents/children and constraints between themselves
 * using the tick prerequisites system.
 **/

USTRUCT()
struct FConstraintTickFunction : public FTickFunction
{
	GENERATED_USTRUCT_BODY()
public:
	CONSTRAINTS_API FConstraintTickFunction();
	CONSTRAINTS_API ~FConstraintTickFunction();

	/* Begin FTickFunction Interface */
	CONSTRAINTS_API virtual void ExecuteTick(
		float DeltaTime,
		ELevelTick TickType,
		ENamedThreads::Type CurrentThread,
		const FGraphEventRef& MyCompletionGraphEvent) override;
	
	CONSTRAINTS_API virtual FString DiagnosticMessage() override;
	/* End FTickFunction Interface */

	/** Callable function that represents the actual constraint. **/
	using ConstraintFunction = TFunction<void()>;
	
	/** Register a callable function. **/
	CONSTRAINTS_API void RegisterFunction(ConstraintFunction InConstraint);
	
	/** Register a callable function. **/
	CONSTRAINTS_API void EvaluateFunctions() const;

	/** Weak ptr to the Constraint holding this tick function. **/
	TWeakObjectPtr<UTickableConstraint> Constraint;
	
	/** The list of the constraint functions that will be called within the tick function. **/
	TArray<ConstraintFunction> ConstraintFunctions;
};

template<>
struct TStructOpsTypeTraits<FConstraintTickFunction> : public TStructOpsTypeTraitsBase2<FConstraintTickFunction>
{
	enum
	{
		WithCopy = false
	};
};

/** 
 * UTickableConstraint
 * Represents the basic interface of constraint within the constraints manager.
 **/

UCLASS(Abstract, Blueprintable, MinimalAPI)
class UTickableConstraint : public UObject
{
	GENERATED_BODY()
	
public:
	UTickableConstraint() {}
	virtual ~UTickableConstraint() {}

	/** Returns the actual function that the tick function needs to evaluate. */
	CONSTRAINTS_API virtual FConstraintTickFunction::ConstraintFunction GetFunction() const PURE_VIRTUAL(GetFunction, return {};);

	/** Evaluates the constraint in a context where it's mot done thru the ConstraintTick's tick function. */
	CONSTRAINTS_API virtual void Evaluate(bool bTickHandlesAlso = false) const;

	/** Tick function that will be registered and evaluated. */
	UPROPERTY()
	FConstraintTickFunction ConstraintTick;

	/** Sets the Active value and enable/disable the tick function. */
	CONSTRAINTS_API virtual void SetActive(const bool bIsActive);
	
	/** Get whether or not it's fully active, it's set to active and all pieces are set up,e.g. objects really exist that are being constrainted*/
	CONSTRAINTS_API virtual bool IsFullyActive() const;
	/** If true it contains objects bound to an external system, like sequencer so we don't do certain things, like remove constraints when they don't resolve*/
	CONSTRAINTS_API virtual bool HasBoundObjects() const PURE_VIRTUAL(HasBoundObjects, return false;);
	/** Resolve the bound objects so that any object it references are resovled and correctly set up*/
	CONSTRAINTS_API virtual void ResolveBoundObjects(FMovieSceneSequenceID LocalSequenceID, IMovieScenePlayer& Player, UObject* SubObject = nullptr)  PURE_VIRTUAL(ResolveBoundObjects);

	/** @todo document */
	CONSTRAINTS_API virtual uint32 GetTargetHash() const PURE_VIRTUAL(GetTargetHash, return 0;);
	/** Whether or not this object references this object. If it HasBoundObjects you should ResolveBoundObjects if you expect this to be up to date */
	CONSTRAINTS_API virtual bool ReferencesObject(TWeakObjectPtr<UObject> InObject) const PURE_VIRTUAL(ReferencesObject, return false;);
	/** Create duplicate with new Outer*/
	CONSTRAINTS_API virtual UTickableConstraint* Duplicate(UObject* NewOuter) const;

#if WITH_EDITOR
	/** Returns the constraint's label used for UI. */
	CONSTRAINTS_API virtual FString GetLabel() const;
	CONSTRAINTS_API virtual FString GetFullLabel() const;

	/** Returns the constraint's type label used for UI. */
	CONSTRAINTS_API virtual FString GetTypeLabel() const;

	// UObject interface
	CONSTRAINTS_API virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	CONSTRAINTS_API virtual void PostEditUndo() override;
	// End of UObject interface
#endif
	/** @todo documentation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, DisplayName="Active State", Category="Constraint")
	bool Active = true;
};


/** 
 * UConstraintsManager
 * This object gathers the different constraints of the level and is held by the ConstraintsActor (unique in the level)
 **/

UCLASS(BLUEPRINTABLE, MinimalAPI)
class UConstraintsManager : public UObject
{
	GENERATED_BODY()
public:

	using ConstraintPtr = TObjectPtr<UTickableConstraint>;

	/** Dynamic blueprintable delegates for knowing when a constraints are added or deleted*/
	DECLARE_DYNAMIC_MULTICAST_SPARSE_DELEGATE_TwoParams(FOnConstraintAdded, UConstraintsManager, OnConstraintAdded_BP, UConstraintsManager*, Mananger, UTickableConstraint*, Constraint);
	DECLARE_DYNAMIC_MULTICAST_SPARSE_DELEGATE_ThreeParams(FOnConstraintRemoved, UConstraintsManager, OnConstraintRemoved_BP, UConstraintsManager*, Mananger, UTickableConstraint*, Constraint, bool, bDoNotCompensate);

	
	CONSTRAINTS_API UConstraintsManager();
	CONSTRAINTS_API virtual ~UConstraintsManager();
	
	//UObjects
	CONSTRAINTS_API virtual void PostLoad() override;
	/** Get the existing Constraints Manager if existing or create a new one. */
	static CONSTRAINTS_API UConstraintsManager* Get(UWorld* InWorld);

	/** Find the existing Constraints Manager. */
	static CONSTRAINTS_API UConstraintsManager* Find(const UWorld* InWorld);

	/** @todo document */
	CONSTRAINTS_API void Init(UWorld* InWorld);
	
	/* Set tick dependencies between two constraints. */
	CONSTRAINTS_API void SetConstraintDependencies(
		FConstraintTickFunction* InFunctionToTickBefore,
		FConstraintTickFunction* InFunctionToTickAfter);

	/** @todo document */
	CONSTRAINTS_API void Clear(UWorld* World);


	/** BP Delegate fired when constraints are added*/
	UPROPERTY(BlueprintAssignable, Category = Constraints, meta = (DisplayName = "OnConstraintAdded"))
	FOnConstraintAdded OnConstraintAdded_BP;
	
	/** BP Delegate fired when constraints are removed*/
	UPROPERTY(BlueprintAssignable, Category = Constraints, meta = (DisplayName = "OnConstraintRemoved"))
	FOnConstraintRemoved OnConstraintRemoved_BP;

private:

	/** @todo document */
	FDelegateHandle OnActorDestroyedHandle;
	
	CONSTRAINTS_API void OnActorDestroyed(AActor* InActor);

	CONSTRAINTS_API void RegisterDelegates(UWorld* World);
	CONSTRAINTS_API void UnregisterDelegates(UWorld* World);
	
	/** @todo document */
	CONSTRAINTS_API void Dump() const;

	/** All of the constraints*/
	UPROPERTY()
	TArray< TObjectPtr<UTickableConstraint> > Constraints;

	friend class FConstraintsManagerController;
	friend class AConstraintsActor;
};

/** 
 * FConstraintsManagerController
 * Basic controller to add / remove / get constraints
 **/

enum class EConstraintsManagerNotifyType
{
	ConstraintAdded,					/** A new constraint has been added. */
	ConstraintRemoved,					/** A constraint has been removed. */
	ConstraintRemovedWithCompensation,	/** A constraint has been removed and needs compensation. */
	ManagerUpdated,						/** The manager has been updated/reset. */
};

DECLARE_MULTICAST_DELEGATE_TwoParams(FConstraintsManagerNotifyDelegate, EConstraintsManagerNotifyType /* type */, UObject* /* subject */);

class FConstraintsManagerController
{
public:
	/** Static to control if we should remove constraints or not, may not want to do some when
	compensating since we may be deleting spawnables while doing so*/
	static CONSTRAINTS_API bool bDoNotRemoveConstraint;
	/** Get and Set Controller acive in this world*/
	static CONSTRAINTS_API FConstraintsManagerController& Get(UWorld* InWorld);

	/** Allocates a new constraint with the constraints manager as the owner. */
	template< typename TConstraint >
	TConstraint* AllocateConstraintT(const FName& InBaseName) const;

	/** Add this constraint to the manager */
	CONSTRAINTS_API bool AddConstraint(UTickableConstraint* InConstraint) const;
	
	/** Make a copy of this constraint and add it to the manager, returns the copy mode if it was added*/
	CONSTRAINTS_API UTickableConstraint* AddConstraintFromCopy(UTickableConstraint* CopyOfConstraint) const;

	/** Get the index of the given constraint's name. */
	CONSTRAINTS_API int32 GetConstraintIndex(const FName& InConstraintName) const;
	
	/** Remove the constraint by name. */
	CONSTRAINTS_API bool RemoveConstraint(const FName& InConstraintName, bool bDoNotCompensate = false) const;

	/** Remove the constraint at the given index. */
	CONSTRAINTS_API bool RemoveConstraint(const int32 InConstraintIndex, bool bDoNotCompensate = false) const;

	/** Remove constraint by ptr*/
	CONSTRAINTS_API bool RemoveConstraint(UTickableConstraint* InConstraint, bool bDoNotCompensate = false) const;

	/** Returns the constraint based on it's name within the manager's constraints array. */
	CONSTRAINTS_API UTickableConstraint* GetConstraint(const FName& InConstraintName) const;

	/** Returns the constraint based on it's index within the manager's constraints array. */
	CONSTRAINTS_API UTickableConstraint* GetConstraint(const int32 InConstraintIndex) const;
	
	/** Get read-only access to the array of constraints. */
	CONSTRAINTS_API const TArray< TObjectPtr<UTickableConstraint> >& GetConstraintsArray() const;

	/** Returns manager's constraints array (sorted if needed). */
	CONSTRAINTS_API TArray< TObjectPtr<UTickableConstraint> > GetAllConstraints(const bool bSorted = false) const;

	/** Returns a filtered constraints array checking if the predicate for each element is true. */
	template <typename Predicate>
	TArray< TObjectPtr<UTickableConstraint> > GetConstraintsByPredicate(Predicate Pred, const bool bSorted = false) const;

	/** Get parent constraints of the specified child. If bSorted is true, then the constraints will be sorted by dependency. */
	CONSTRAINTS_API TArray< TObjectPtr<UTickableConstraint> > GetParentConstraints(
		const uint32 InTargetHash,
		const bool bSorted = false) const;

	/** Set dependencies between two constraints. */
	CONSTRAINTS_API void SetConstraintsDependencies(
		const FName& InNameToTickBefore,
		const FName& InNameToTickAfter) const;

	/** Go through each constraint in order and evaluate and tick them*/
	CONSTRAINTS_API void EvaluateAllConstraints() const;

	/** when PIEing/Simulating it's possible that the constraint isn't in the active manager but still lives*/
	CONSTRAINTS_API bool DoesExistInAnyWorld(UTickableConstraint* InConstraint);

private:
	/** Delegeate that's fired when a scene component is constrained, this is needed to make sure things like gizmo's get updated after the constraint tick happens*/
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnSceneComponentConstrained, USceneComponent* /*InSceneComponent*/);
	FOnSceneComponentConstrained SceneComponentConstrained;

	/** Delegate to trigger changes in the constraints manager. */
	FConstraintsManagerNotifyDelegate NotifyDelegate;
	
	/** Find the existing Constraints Manager in World or create a new one. */
	CONSTRAINTS_API UConstraintsManager* GetManager() const;
	
	/** Find the existing Constraints Manager in World. */
	CONSTRAINTS_API UConstraintsManager* FindManager() const;

	/** Destroy the ConstraintsManager from the World. */
	CONSTRAINTS_API void DestroyManager() const;

	/** The World that holds the ConstraintsManagerActor. */
	UWorld* World = nullptr;

public:
	/** Delegate that's fired when a scene component is constrained, this is needed to make sure things like gizmo's get updated after the constraint tick happens*/
	FOnSceneComponentConstrained& OnSceneComponentConstrained() { return SceneComponentConstrained; }

	/** Delegate to trigger changes in the constraints manager. */
	FConstraintsManagerNotifyDelegate& GetNotifyDelegate() { return NotifyDelegate; }

	/** Notify from changes in the constraints manager. */
	CONSTRAINTS_API void Notify(EConstraintsManagerNotifyType InNotifyType, UObject* InObject) const;
};
