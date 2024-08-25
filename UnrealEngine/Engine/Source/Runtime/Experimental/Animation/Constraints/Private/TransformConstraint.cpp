// Copyright Epic Games, Inc. All Rights Reserved.

#include "TransformConstraint.h"
#include "TransformableHandle.h"
#include "ConstraintsManager.inl"
#include "TransformableRegistry.h"
#include "GameFramework/Actor.h"
#include "MovieSceneSection.h"
#include "Engine/World.h"
#include "UObject/Package.h"
#include "Components/SkeletalMeshComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(TransformConstraint)

namespace
{

struct FConstraintCycleChecker
{
public:
	using ConstraintPtr = TWeakObjectPtr<UTickableConstraint>;
	using ConstraintArray = TArray<ConstraintPtr>;

	/** Checks if this handle is cycle from a tick dependencies perspective. */
	static bool IsCycling(const TWeakObjectPtr<UTransformableHandle>& InHandle)
	{
		if (!IsValid(InHandle.Get()))
		{
			return false;
		}

		TSet<const FTickFunction*> VisitedFunctions;
		const FTickFunction* TickFunction = InHandle->GetTickFunction();
		return HasPrerequisiteDependencyWith(TickFunction, TickFunction, VisitedFunctions);
	}

	/** Checks for cycling constraints and manage tick dependencies if needed to avoid cycles from a tick dependency pov. */
	static void CheckAndFixCycles(const UTickableTransformConstraint* InConstraint)
	{
		if (!IsValid(InConstraint))
		{
			return;
		}

		//todo constraints on level sequences aren't in a world
		UWorld* World = InConstraint->GetWorld();
		if (!IsValid(World))
		{
			return;
		}

		// get child's tick function
		FTickFunction* ChildTickFunction = InConstraint->GetChildHandleTickFunction();
		if (!ChildTickFunction)
		{
			return;
		}

		// filter for all constraints where the parent's tick function equals ChildTickFunction
		auto Predicate = [ChildTickFunction](const ConstraintPtr& InConstraint)
		{
			const UTickableTransformConstraint* TransformConst = Cast<UTickableTransformConstraint>(InConstraint.Get());
			if (!TransformConst)
			{
				return false;
			}

			const TObjectPtr<UTransformableHandle>& ChildHandle = TransformConst->ChildTRSHandle;
			if (!IsValid(ChildHandle) || !ChildHandle->IsValid())
			{
				return false;
			}

			const FTickFunction* ParentTickFunction = TransformConst->GetParentHandleTickFunction();
			return ParentTickFunction && ParentTickFunction == ChildTickFunction;
		};

		const FConstraintsManagerController& Controller = FConstraintsManagerController::Get(World);
		const ConstraintArray CyclingConstraints = Controller.GetConstraintsByPredicate(Predicate);
		if (CyclingConstraints.IsEmpty())
		{
			return;
		}

		// check if they can cause a cycle and manage dependencies if that's the case
		TSet<const FTickFunction*> VisitedFunctions;
		for (const ConstraintPtr& Constraint: CyclingConstraints)
		{
			if (HasPrerequisiteDependencyWith(&Constraint->GetTickFunction(World), &InConstraint->GetTickFunction(World), VisitedFunctions))
			{
				UpdateCyclingDependency(World, Cast<UTickableTransformConstraint>(Constraint));
			}
		}
	}
	
	// ensure that InPossiblePrimary is not depending on InPossibleSecondary to avoid creating cycles 
	static bool HasPrerequisiteDependencyWith(const FTickFunction* InSecondary, const FTickFunction* InPrimary, TSet<const FTickFunction*>& InOutVisitedFunctions)
	{
		if (!InSecondary || !InPrimary)
		{
			return false;
		}

		// is InSecondary a Prereq of InPrimary?
		const TArray<FTickPrerequisite>& Prerequisites = InPrimary->GetPrerequisites();
		const bool bIsSecondaryAPrereq = Prerequisites.ContainsByPredicate([InSecondary](const FTickPrerequisite& Prereq)
		{
			const FTickFunction* PrereqFunction = Prereq.Get();
			return PrereqFunction && (PrereqFunction == InSecondary);
		});

		if (bIsSecondaryAPrereq)
		{
			return true;
		}

		// check if InPrimary has already been visited to avoid endless loop
		if (InOutVisitedFunctions.Contains(InPrimary))
		{
			return false;
		}
		InOutVisitedFunctions.Add(InPrimary);
		
		// otherwise, recurse
		for (const FTickPrerequisite& Prerequisite : Prerequisites)
		{
			if (HasPrerequisiteDependencyWith(InSecondary, Prerequisite.Get(), InOutVisitedFunctions))
			{
				return true;
			}
		}

		return false;
	}

private:
	/**
	 * Manage tick dependencies if needed to avoid cycles from a tick dependency pov.
	 * Both InConstraintToUpdate and its parent handle are supposed valid at this point
	 */
	static void UpdateCyclingDependency(UWorld* InWorld, UTickableTransformConstraint* InConstraintToUpdate)
	{
		// nothing to do if this constraint doesn't tick
		if (!InConstraintToUpdate->GetTickFunction(InWorld).IsTickFunctionEnabled())
		{
			return;
		}
		
		const TObjectPtr<UTransformableHandle>& ParentHandle = InConstraintToUpdate->ParentTRSHandle;
		UObject* TargetObject = ParentHandle->GetPrerequisiteObject();

		// filter for all constraints where the child's target object equals ChildTickFunction
		auto Predicate = [TargetObject](const ConstraintPtr& InConstraint)
		{
			const UTickableTransformConstraint* TransformConst = Cast<UTickableTransformConstraint>(InConstraint.Get());
			if (!TransformConst)
			{
				return false;
			}

			const TObjectPtr<UTransformableHandle>& ChildHandle = TransformConst->ChildTRSHandle;
			if (!IsValid(ChildHandle) || !ChildHandle->IsValid())
			{
				return false;
			}
			
			const UObject* ChildPrereqObject = ChildHandle->GetPrerequisiteObject();
			return IsValid(ChildPrereqObject) && (ChildPrereqObject == TargetObject);
		};

		const FConstraintsManagerController& Controller = FConstraintsManagerController::Get(InWorld);
		const ConstraintArray ParentConstraints = Controller.GetConstraintsByPredicate(Predicate, true);

		// check if there's any active constraint in ParentConstraints
		const bool bHasActiveParentConstraint = ParentConstraints.ContainsByPredicate([](const ConstraintPtr& Constraint)
		{
			return Constraint.IsValid() && IsValid(Constraint.Get()) && Constraint->IsFullyActive(); 
		});

		// update the constraint prerequisites based on the result
		if (FTickFunction* TargetTickFunction = InConstraintToUpdate->GetParentHandleTickFunction())
		{
			if (bHasActiveParentConstraint)
			{
				InConstraintToUpdate->GetTickFunction(InWorld).RemovePrerequisite(TargetObject, *TargetTickFunction);
			}
			else
			{
				InConstraintToUpdate->GetTickFunction(InWorld).AddPrerequisite(TargetObject, *TargetTickFunction);
			}
		}
	}
};
	
}

namespace ConstraintLocals
{

static bool	bPreEvaluateChild = true;
static FAutoConsoleVariableRef CVarPreEvaluateChild(
	TEXT("Constraints.PreEvaluateChild"),
	bPreEvaluateChild,
	TEXT("Force child evaluation before constraint computation.")
	);
	
static bool	bPreTickChild = false;
static FAutoConsoleVariableRef CVarPreTickChild(
	TEXT("Constraints.PreTickChild"),
	bPreTickChild,
	TEXT("Force child ticking before constraint computation.")
	);

void PreEvaluateHandle(const TObjectPtr<UTransformableHandle>& InHandle) 
{
	if ((bPreEvaluateChild || bPreTickChild) && ::IsValid(InHandle))
	{
		InHandle->PreEvaluate(bPreTickChild);
	}
}

UObject* GetHandleTarget(const TObjectPtr<UTransformableHandle>& InHandle)
{
	return IsValid(InHandle) ? InHandle->GetTarget().Get() : nullptr; 
}

static bool	bIncludeTarget = true;
static FAutoConsoleVariableRef CVarIncludeTarget(
	TEXT("Constraints.IncludeTarget"),
	bIncludeTarget,
	TEXT("Include target when getting child's existing constraints.")
	);

static bool	bDebugDependencies = false;
static FAutoConsoleVariableRef CVarDebugDependencies(
	TEXT("Constraints.DebugDependencies"),
	bDebugDependencies,
	TEXT("Print debug info about dependencies when creating a new constraint.") );

FString GetConstraintLabel(const UTickableConstraint* InConstraint)
{
#if WITH_EDITOR
	return InConstraint->GetFullLabel();
#else
	return InConstraint->GetName();
#endif		
}

FString GetHandleLabel(const UTransformableHandle* InHandle)
{
#if WITH_EDITOR
	return InHandle->GetFullLabel();
#else
	return InHandle->GetName();
#endif		
}
	
}

/** 
 * UTickableTransformConstraint
 **/

#if WITH_EDITOR
UTickableTransformConstraint::FOnConstraintChanged UTickableTransformConstraint::OnConstraintChanged;
#endif // WITH_EDITOR

int64 UTickableTransformConstraint::GetType() const
{
	return static_cast<int64>(Type);
}
EMovieSceneTransformChannel UTickableTransformConstraint::GetChannelsToKey() const
{
	static const TMap< ETransformConstraintType, EMovieSceneTransformChannel > ConstraintToChannels({
	{ETransformConstraintType::Translation, EMovieSceneTransformChannel::Translation},
	{ETransformConstraintType::Rotation, EMovieSceneTransformChannel::Rotation},
	{ETransformConstraintType::Scale, EMovieSceneTransformChannel::Scale},
	{ETransformConstraintType::Parent, EMovieSceneTransformChannel::AllTransform},
	{ETransformConstraintType::LookAt, EMovieSceneTransformChannel::Rotation}
		});

	const ETransformConstraintType ConstType = static_cast<ETransformConstraintType>(GetType());
	if (const EMovieSceneTransformChannel* Channel = ConstraintToChannels.Find(ConstType))
	{
		return *Channel;;
	}

	return EMovieSceneTransformChannel::AllTransform;
}

UTickableConstraint* UTickableTransformConstraint::Duplicate(UObject* NewOuter) const
{
	UTickableTransformConstraint* Dup = DuplicateObject<UTickableTransformConstraint>(this, NewOuter);
	if (ChildTRSHandle)
	{
		UTransformableHandle* HandleCopy = ChildTRSHandle->Duplicate(Dup);
		Dup->ChildTRSHandle = HandleCopy;
	}
	if (ParentTRSHandle)
	{
		UTransformableHandle* HandleCopy = ParentTRSHandle->Duplicate(Dup);
		Dup->ParentTRSHandle = HandleCopy;
	}
	for (TPair<TWeakObjectPtr<ULevel>, FConstraintTickFunction>& Pair : ConstraintTicks)
	{
		if (Pair.Key.IsValid())
		{
			UWorld* World = Pair.Key->GetTypedOuter<UWorld>();
		}
	}
	return Dup;
}

#if WITH_EDITOR

FString UTickableTransformConstraint::GetLabel() const
{
	if (!ChildTRSHandle || !ChildTRSHandle->IsValid())
	{
		static const FString DummyLabel;
		return DummyLabel;
	}
	
	if (ParentTRSHandle && ParentTRSHandle->IsValid())
	{
		return FString::Printf(TEXT("%s.%s"), *ParentTRSHandle->GetLabel(), *ChildTRSHandle->GetLabel() );		
	}

	return ChildTRSHandle->GetLabel();
}

FString UTickableTransformConstraint::GetFullLabel() const
{
	if (!ChildTRSHandle || !ChildTRSHandle->IsValid())
	{
		static const FString DummyLabel;
		return DummyLabel;
	}
	
	if (ParentTRSHandle && ParentTRSHandle->IsValid())
	{
		return FString::Printf(TEXT("%s.%s"), *ParentTRSHandle->GetFullLabel(), *ChildTRSHandle->GetFullLabel() );		
	}

	return ChildTRSHandle->GetLabel();
}

FString UTickableTransformConstraint::GetTypeLabel() const
{
	static const UEnum* TypeEnum = StaticEnum<ETransformConstraintType>();
	if (TypeEnum->IsValidEnumValue(GetType()))
	{
		return TypeEnum->GetNameStringByValue(GetType());
	}

	return Super::GetTypeLabel();
}

void UTickableTransformConstraint::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		return;
	}
	
	const FName PropertyName = PropertyChangedEvent.GetPropertyName();
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UTickableTransformConstraint, bMaintainOffset))
	{
		Evaluate();
		return;
	}

	if (const FProperty* MemberProperty = PropertyChangedEvent.MemberProperty)
	{
		const FName MemberPropertyName = MemberProperty->GetFName();
		if (MemberPropertyName == GET_MEMBER_NAME_CHECKED(UTickableTranslationConstraint, OffsetTranslation) ||
			MemberPropertyName == GET_MEMBER_NAME_CHECKED(UTickableRotationConstraint, OffsetRotation) ||
			MemberPropertyName == GET_MEMBER_NAME_CHECKED(UTickableScaleConstraint, OffsetScale) ||
			MemberPropertyName == GET_MEMBER_NAME_CHECKED(UTickableParentConstraint, OffsetTransform))
		{
			OnConstraintChanged.Broadcast(this, PropertyChangedEvent);
			Evaluate();
		}
	}
}

void UTickableTransformConstraint::PostEditUndo()
{
	Super::PostEditUndo();
}

UTickableTransformConstraint::FOnConstraintChanged& UTickableTransformConstraint::GetOnConstraintChanged()
{
	return OnConstraintChanged;	
}

#endif

void UTickableTransformConstraint::UnregisterDelegates() const
{
	if (ChildTRSHandle)
	{
		ChildTRSHandle->HandleModified().RemoveAll(this);
	}
	if (ParentTRSHandle)
	{
		ParentTRSHandle->HandleModified().RemoveAll(this);
	}
}

void UTickableTransformConstraint::RegisterDelegates()
{
	UnregisterDelegates();

	if (ChildTRSHandle)
	{
		ChildTRSHandle->HandleModified().AddUObject(this, &UTickableTransformConstraint::OnHandleModified);
	}
	if (ParentTRSHandle)
	{
		ParentTRSHandle->HandleModified().AddUObject(this, &UTickableTransformConstraint::OnHandleModified);
	}	
}

void UTickableTransformConstraint::Setup()
{
	if (!ParentTRSHandle || !ChildTRSHandle || !ParentTRSHandle->IsValid() || !ChildTRSHandle->IsValid())
	{
		// handle error
		return;
	}
	
	ComputeOffset();

}

void UTickableTransformConstraint::SetupDependencies(UWorld* InWorld)
{
	//we may not be the outer for old files so move it over
	if (ParentTRSHandle)
	{
		UTickableTransformConstraint* PTRS = ParentTRSHandle->GetTypedOuter<UTickableTransformConstraint>();
		if(PTRS != this)
		{
			ParentTRSHandle->Rename(nullptr, this, REN_ForceNoResetLoaders | REN_DontCreateRedirectors);
		}
	}
	if (ChildTRSHandle)
	{
		UTickableTransformConstraint* CTRS = ChildTRSHandle->GetTypedOuter<UTickableTransformConstraint>();
		if (CTRS != this)
		{
			ChildTRSHandle->Rename(nullptr, this, REN_ForceNoResetLoaders | REN_DontCreateRedirectors);
		}
	}
	
	FTickFunction* ParentTickFunction = GetParentHandleTickFunction();
	FTickFunction* ChildTickFunction = GetChildHandleTickFunction();
	
	if (ParentTickFunction && (ChildTickFunction != ParentTickFunction))
	{
		// manage dependencies
		// force ConstraintTickFunction to tick after InParent does.
		// Note that this might not register anything if the parent can't tick (static meshes for instance)
		FConstraintTickFunction& ConstraintTick = ConstraintTicks.FindOrAdd(InWorld->GetCurrentLevel());
		ConstraintTick.AddPrerequisite(ParentTRSHandle->GetPrerequisiteObject(), *ParentTickFunction);
	}
	
	// TODO also check for cycle dependencies here
	if (ChildTickFunction)
	{
		USkeletalMeshComponent* ChildOwner = Cast<USkeletalMeshComponent>(ChildTRSHandle->GetTarget().Get());
		if (ChildOwner == nullptr)
		{
			// force InChild to tick after ConstraintTickFunction does.
			// Note that this might not register anything if the child can't tick (static meshes for instance)
			// Also don't do if it's a skel mesh will cause cycle.
			FConstraintTickFunction& ConstraintTick = ConstraintTicks.FindOrAdd(InWorld->GetCurrentLevel());
			ChildTickFunction->AddPrerequisite(this, ConstraintTick);
		}
	}
}

void UTickableTransformConstraint::EnsurePrimaryDependency(UWorld* InWorld)
{
	const FTickFunction* ParentTickFunction = GetParentHandleTickFunction();
	const FTickFunction* ChildTickFunction = GetChildHandleTickFunction();
	if (ParentTickFunction && (ChildTickFunction != ParentTickFunction))
	{
		FConstraintTickFunction& ConstraintTick = ConstraintTicks.FindOrAdd(InWorld->GetCurrentLevel());
		const TArray<FTickPrerequisite>& ParentPrerequisites = ConstraintTick.GetPrerequisites();
		if (ParentPrerequisites.IsEmpty())
		{
			// if the constraint has no prerex at this stage, this means that the parent tick function
			// is not registered or can't tick (static meshes for instance) so look for the first parent tick function if any.
			// In a context of adding several constraints, we want to make sure that the evaluation order is the right one
			FTickPrerequisite PrimaryPrerex = ParentTRSHandle->GetPrimaryPrerequisite();
			if (FTickFunction* PotentialFunction = PrimaryPrerex.Get())
			{
				UObject* Target = PrimaryPrerex.PrerequisiteObject.Get();
				ConstraintTick.AddPrerequisite(Target, *PotentialFunction);
			}
		}
	}
}

void UTickableTransformConstraint::OnActiveStateChanged() const
{
	// tick dependencies might need to be updated when dealing with cycles (than can be created between two controls for example) 
	FConstraintCycleChecker::CheckAndFixCycles(this);
}

void UTickableTransformConstraint::PostLoad()
{
	Super::PostLoad();
}

void UTickableTransformConstraint::PostDuplicate(bool bDuplicateForPIE)
{
	Super::PostDuplicate(bDuplicateForPIE);

}

uint32 UTickableTransformConstraint::GetTargetHash() const
{
	return (ChildTRSHandle && ChildTRSHandle->IsValid()) ? ChildTRSHandle->GetHash() : 0;
}

bool UTickableTransformConstraint::ReferencesObject(TWeakObjectPtr<UObject> InObject) const
{
	const TWeakObjectPtr<UObject> ChildTarget = (ChildTRSHandle && ChildTRSHandle->IsValid())? ChildTRSHandle->GetTarget() : nullptr;
	if (ChildTarget == InObject)
	{
		return true;
	}

	const TWeakObjectPtr<UObject> ParentTarget = (ParentTRSHandle && ParentTRSHandle->IsValid()) ? ParentTRSHandle->GetTarget() : nullptr;
	if (ParentTarget == InObject)
	{
		return true;	
	}
	
	return false;
}

bool UTickableTransformConstraint::HasBoundObjects() const
{
	if (ChildTRSHandle && ChildTRSHandle->HasBoundObjects())
	{
		return true;
	}
	if (ParentTRSHandle && ParentTRSHandle->HasBoundObjects())
	{
		return true;
	}
	return false;
}

void UTickableTransformConstraint::ResolveBoundObjects(FMovieSceneSequenceID LocalSequenceID, IMovieScenePlayer& Player, UObject* SubObject)
{
	// update dependencies if the constraint becomes valid once resolved 
	FConstraintDependencyScope Scope(this);
	
	if (ChildTRSHandle && ChildTRSHandle->HasBoundObjects())
	{
		ChildTRSHandle->ResolveBoundObjects(LocalSequenceID, Player, SubObject);
	}
	if (ParentTRSHandle && ParentTRSHandle->HasBoundObjects())
	{
		ParentTRSHandle->ResolveBoundObjects(LocalSequenceID, Player, SubObject);
	}
}

void UTickableTransformConstraint::Evaluate(bool bTickHandlesAlso) const
{
	if (bTickHandlesAlso)
	{
		if (ParentTRSHandle)
		{
			ParentTRSHandle->TickTarget();
		}

		Super::Evaluate();
		//todo test this more may be able to remove it
		if (ChildTRSHandle)
		{
			ChildTRSHandle->TickTarget();
		}
	}
	else
	{
		Super::Evaluate();
	}
}

void UTickableTransformConstraint::SetActive(const bool bIsActive)
{
	const bool bNeedsUpdate = (Active != bIsActive);
	Super::SetActive(bIsActive);
	
	if (bNeedsUpdate)
	{
		OnActiveStateChanged();
	}
}

void UTickableTransformConstraint::SetChildGlobalTransform(const FTransform& InGlobal) const
{
	if(ChildTRSHandle && ChildTRSHandle->IsValid())
	{
		ChildTRSHandle->SetGlobalTransform(InGlobal);
	}
}

void UTickableTransformConstraint::SetChildLocalTransform(const FTransform& InLocal) const
{
	if(ChildTRSHandle && ChildTRSHandle->IsValid())
	{
		ChildTRSHandle->SetLocalTransform(InLocal);
	}
}

FTransform UTickableTransformConstraint::GetChildGlobalTransform() const
{
	return (ChildTRSHandle && ChildTRSHandle->IsValid()) ? ChildTRSHandle->GetGlobalTransform() : FTransform::Identity;
}

FTransform UTickableTransformConstraint::GetChildLocalTransform() const
{
	return (ChildTRSHandle && ChildTRSHandle->IsValid()) ? ChildTRSHandle->GetLocalTransform() : FTransform::Identity;
}

FTransform UTickableTransformConstraint::GetParentGlobalTransform() const
{
	return (ParentTRSHandle && ParentTRSHandle->IsValid()) ? ParentTRSHandle->GetGlobalTransform() : FTransform::Identity;
}

FTransform UTickableTransformConstraint::GetParentLocalTransform() const
{
	return (ParentTRSHandle && ParentTRSHandle->IsValid()) ? ParentTRSHandle->GetLocalTransform() : FTransform::Identity;
}

void UTickableTransformConstraint::OnHandleModified(UTransformableHandle* InHandle, EHandleEvent InNotification)
{
	if (!InHandle)
	{
		return;
	}

	const UObject* Target = InHandle->GetTarget().Get();
	UWorld* World = Target != nullptr ? Target->GetWorld() : nullptr;
	if (!World)
	{
		return;
	}
	
	if(InHandle == ChildTRSHandle || InHandle == ParentTRSHandle)
	{
		if (InNotification == EHandleEvent::ComponentUpdated)
		{
			SetupDependencies(World);
			// update dependencies now the component has been updated. 
			FTransformConstraintUtils::BuildDependencies(World, this);
			return;
		}
	}

	auto MarkForEvaluation = [this, World]()
	{
		const FConstraintsManagerController& Controller = FConstraintsManagerController::Get(World);
		Controller.MarkConstraintForEvaluation(this);
	};

	auto DirectEvaluation = [this, World]()
	{
		const FConstraintTickFunction* ConstraintTick = ConstraintTicks.Find(World->GetCurrentLevel());
		if (IsFullyActive() && ConstraintTick->IsTickFunctionRegistered() && ConstraintTick->IsTickFunctionEnabled())
		{
			Evaluate(true);
		}
	};
	
	if (InHandle == ChildTRSHandle)
	{
		if (InNotification == EHandleEvent::UpperDependencyUpdated)
		{
			const UObject* ParentTarget = ConstraintLocals::GetHandleTarget(ParentTRSHandle);
			if (ParentTarget && ParentTarget != Target)
			{
				if (FConstraintsEvaluationGraph::UseEvaluationGraph())
				{
					MarkForEvaluation();
				}
				else
				{
					DirectEvaluation();
				}
				return;
			}
		}
	}

	// update the constraint if the parent's transform has been modified 
	if (InHandle == ParentTRSHandle && InNotification == EHandleEvent::GlobalTransformUpdated)
	{
		if (FConstraintsEvaluationGraph::UseEvaluationGraph())
		{
			MarkForEvaluation();
		}
		else
		{
			DirectEvaluation();
		}
		return;
	}

	if (InHandle == ParentTRSHandle && InNotification == EHandleEvent::UpperDependencyUpdated)
	{
		if (FConstraintsEvaluationGraph::UseEvaluationGraph())
		{
			MarkForEvaluation();
		}
		else
		{
			DirectEvaluation();
		}
		return;
	}

	if (InHandle == ChildTRSHandle && InNotification == EHandleEvent::GlobalTransformUpdated)
	{
		if (FConstraintsEvaluationGraph::UseEvaluationGraph())
		{
			return MarkForEvaluation();
		}
	}
}

bool UTickableTransformConstraint::IsValid(const bool bDeepCheck) const
{
	const bool bAreHandlesValid =
		::IsValid(ChildTRSHandle) && ChildTRSHandle->IsValid(bDeepCheck) &&
		::IsValid(ParentTRSHandle) && ParentTRSHandle->IsValid(bDeepCheck); 
	
	return bDeepCheck ? bAreHandlesValid && bValid : bAreHandlesValid;
}

bool UTickableTransformConstraint::IsFullyActive() const
{
	return (Active && IsValid());
}

bool UTickableTransformConstraint::NeedsCompensation() const
{
	// NOTE: this can be extended to something more complex if needed  
	return true;
}

FTickFunction* UTickableTransformConstraint::GetChildHandleTickFunction() const 
{
	return GetHandleTickFunction(ChildTRSHandle); 
}

FTickFunction* UTickableTransformConstraint::GetParentHandleTickFunction() const 
{
	return GetHandleTickFunction(ParentTRSHandle); 
}

FTickFunction* UTickableTransformConstraint::GetHandleTickFunction(const TObjectPtr<UTransformableHandle>& InHandle) const 
{
	if (!::IsValid(InHandle) || !InHandle->IsValid())
	{
		return nullptr;
	}
		
	return InHandle->GetTickFunction();
}

void UTickableTransformConstraint::InitConstraint(UWorld *InWorld)
{
	FConstraintTickFunction& ConstraintTick = ConstraintTicks.FindOrAdd(InWorld->GetCurrentLevel());

	if (ConstraintTick.ConstraintFunctions.IsEmpty())
	{
		ConstraintTick.RegisterFunction(GetFunction());
	}
	ConstraintTick.RegisterTickFunction(InWorld->GetCurrentLevel());
	ConstraintTick.Constraint = this;
	SetupDependencies(InWorld);
	RegisterDelegates();
	bValid = true;
}

void UTickableTransformConstraint::TeardownConstraint(UWorld* InWorld)
{
	FConstraintTickFunction& ConstraintTick = ConstraintTicks.FindOrAdd(InWorld->GetCurrentLevel());
	ConstraintTick.SetTickFunctionEnable(false);
	ConstraintTick.UnRegisterTickFunction();
	if (FTickFunction* ChildTickFunction = GetChildHandleTickFunction())
	{
		ChildTickFunction->RemovePrerequisite(this, ConstraintTick);
	}
	ConstraintTicks.Remove(InWorld->GetCurrentLevel());

	// unregister delegates. should handles delegates be unregistered as well?
	UnregisterDelegates();
}

void UTickableTransformConstraint::AddedToWorld(UWorld* InWorld)
{
	if (InWorld)
	{
		// update dependencies once added to the sub-system 
		FTransformConstraintUtils::BuildDependencies(InWorld, this);
	}
}

/** 
 * UTickableTranslationConstraint
 **/

UTickableTranslationConstraint::UTickableTranslationConstraint()
{
	Type = ETransformConstraintType::Translation;
}

#if WITH_EDITOR

void UTickableTranslationConstraint::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		return;
	}

	const FName PropertyName = PropertyChangedEvent.GetPropertyName();
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UTickableTransformConstraint, bDynamicOffset))
	{
		if (bDynamicOffset)
		{
			Cache.CachedInputHash = CalculateInputHash();
			
			const FTransform ChildGlobalTransform = GetChildGlobalTransform();
			const FTransform ParentWorldTransform = GetParentGlobalTransform();
			OffsetTranslation = ChildGlobalTransform.GetLocation() - ParentWorldTransform.GetLocation();
			
			Evaluate();
		}
		return;
	}
}

#endif

void UTickableTranslationConstraint::ComputeOffset()
{
	const FTransform InitParentTransform = GetParentGlobalTransform();
	const FTransform InitChildTransform = GetChildGlobalTransform();
	
	OffsetTranslation = FVector::ZeroVector;
	if (bMaintainOffset || bDynamicOffset)
	{
		OffsetTranslation = InitChildTransform.GetLocation() - InitParentTransform.GetLocation();
	}	
}

FConstraintTickFunction::ConstraintFunction UTickableTranslationConstraint::GetFunction() const
{
	return [this]()
	{
		if (!IsFullyActive())
		{
			return;
		}
		
		const float ClampedWeight = FMath::Clamp<float>(Weight, 0.f, 1.f);
		if (ClampedWeight < KINDA_SMALL_NUMBER)
		{
			return;
		}

		ConstraintLocals::PreEvaluateHandle(ChildTRSHandle);
		
		const FVector ParentTranslation = GetParentGlobalTransform().GetLocation();
		FTransform Transform = GetChildGlobalTransform();
		const FVector ChildTranslation = Transform.GetLocation();
		
		FVector NewTranslation = (!bMaintainOffset) ? ParentTranslation : ParentTranslation + OffsetTranslation;
		if (ClampedWeight < 1.0f - KINDA_SMALL_NUMBER)
		{
			NewTranslation = FMath::Lerp<FVector>(ChildTranslation, NewTranslation, ClampedWeight);
		}

		AxisFilter.FilterVector(NewTranslation, ChildTranslation);

		Transform.SetLocation(NewTranslation);
			
		SetChildGlobalTransform(Transform);
		if (ChildTRSHandle)
		{
			ChildTRSHandle->TickTarget();
		}
	};
}

void UTickableTranslationConstraint::OnHandleModified(UTransformableHandle* InHandle, EHandleEvent InEvent)
{
	Super::OnHandleModified(InHandle, InEvent);
	
	if (!IsFullyActive() || !bDynamicOffset)
	{
		return;
	}
	
	if (!InHandle || InHandle != ChildTRSHandle)
	{
		return;
	}

	const bool bUpdateFromGlobal = (InEvent == EHandleEvent::GlobalTransformUpdated);
	const bool bUpdateTransform = InEvent == EHandleEvent::LocalTransformUpdated || bUpdateFromGlobal;
	if (!bUpdateTransform)
	{
		return;
	}

	const uint32 InputHash = CalculateInputHash();
	
	// update dynamic offset
	if (InputHash != Cache.CachedInputHash)
	{
		Cache.CachedInputHash = InputHash;

		if (bUpdateFromGlobal)
		{
			const FTransform ParentWorldTransform = GetParentGlobalTransform();
			const FTransform ChildGlobalTransform = GetChildGlobalTransform();
			OffsetTranslation = ChildGlobalTransform.GetLocation() - ParentWorldTransform.GetLocation();
		}
		else
		{
			const FTransform ChildLocalTransform = GetChildLocalTransform();
			OffsetTranslation = ChildLocalTransform.GetTranslation();
		}
	}
}

uint32 UTickableTranslationConstraint::CalculateInputHash() const
{
	uint32 Hash = 0;

	// local location hash
	const FTransform ChildLocalTransform = GetChildLocalTransform();
	Hash = HashCombine(Hash, GetTypeHash(ChildLocalTransform.GetTranslation() ));

	// global location hash
	const FTransform ChildGlobalTransform = GetChildGlobalTransform();
	Hash = HashCombine(Hash, GetTypeHash(ChildGlobalTransform.GetTranslation() ));
	
	return Hash;
}

/** 
 * UTickableRotationConstraint
 **/

UTickableRotationConstraint::UTickableRotationConstraint()
{
	Type = ETransformConstraintType::Rotation;
}

#if WITH_EDITOR

void UTickableRotationConstraint::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		return;
	}
	
	const FName PropertyName = PropertyChangedEvent.GetPropertyName();
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UTickableTransformConstraint, bDynamicOffset))
	{
		if (bDynamicOffset)
		{
			Cache.CachedInputHash = CalculateInputHash();
			
			const FTransform ChildGlobalTransform = GetChildGlobalTransform();
			const FTransform ParentWorldTransform = GetParentGlobalTransform();
			OffsetRotation = ParentWorldTransform.GetRotation().Inverse() * ChildGlobalTransform.GetRotation();
			
			Evaluate();
		}
		return;
	}
}

#endif

void UTickableRotationConstraint::ComputeOffset()
{
	const FTransform InitParentTransform = GetParentGlobalTransform();
	const FTransform InitChildTransform = GetChildGlobalTransform();
	
	OffsetRotation = FQuat::Identity;
	if (bMaintainOffset || bDynamicOffset)
	{
		OffsetRotation = InitParentTransform.GetRotation().Inverse() * InitChildTransform.GetRotation();
		OffsetRotation.Normalize(); 
	}
}

FConstraintTickFunction::ConstraintFunction UTickableRotationConstraint::GetFunction() const
{
	return [this]()
	{
		if (!IsFullyActive())
		{
			return;
		}
		
		const float ClampedWeight = FMath::Clamp<float>(Weight, 0.f, 1.f);
		if (ClampedWeight < KINDA_SMALL_NUMBER)
		{
			return;
		}

		ConstraintLocals::PreEvaluateHandle(ChildTRSHandle);
		
		const FQuat ParentRotation = GetParentGlobalTransform().GetRotation();
		FTransform Transform = GetChildGlobalTransform();
		const FQuat ChildRotation = Transform.GetRotation();

		FQuat NewRotation = (!bMaintainOffset) ? ParentRotation : ParentRotation * OffsetRotation;
		if (ClampedWeight < 1.0f - KINDA_SMALL_NUMBER)
		{
			NewRotation = FQuat::Slerp(ChildRotation, NewRotation, ClampedWeight);
		}

		AxisFilter.FilterQuat(NewRotation, ChildRotation);


		Transform.SetRotation(NewRotation);
		
		SetChildGlobalTransform(Transform);

		if (ChildTRSHandle)
		{
			ChildTRSHandle->TickTarget();
		}
	};
}

void UTickableRotationConstraint::OnHandleModified(UTransformableHandle* InHandle, EHandleEvent InEvent)
{
	Super::OnHandleModified(InHandle, InEvent);
	
	if (!IsFullyActive() || !bDynamicOffset)
	{
		return;
	}
	
	if (!InHandle || InHandle != ChildTRSHandle)
	{
		return;
	}

	const bool bUpdateFromGlobal = (InEvent == EHandleEvent::GlobalTransformUpdated);
	const bool bUpdateTransform = (InEvent == EHandleEvent::LocalTransformUpdated) || bUpdateFromGlobal;
	if (!bUpdateTransform)
	{
		return;
	}

	const uint32 InputHash = CalculateInputHash();
	
	// update dynamic offset
	if (InputHash != Cache.CachedInputHash)
	{
		Cache.CachedInputHash = InputHash;

		if (bUpdateFromGlobal)
		{
			const FTransform ParentWorldTransform = GetParentGlobalTransform();
			const FTransform ChildGlobalTransform = GetChildGlobalTransform();
			OffsetRotation = ParentWorldTransform.GetRotation().Inverse() * ChildGlobalTransform.GetRotation();
		}
		else
		{
			OffsetRotation = GetChildLocalTransform().GetRotation();
		}
	}
}

uint32 UTickableRotationConstraint::CalculateInputHash() const
{
	uint32 Hash = 0;

	// local rotation hash
	const FTransform ChildLocalTransform = GetChildLocalTransform();
	Hash = HashCombine(Hash, GetTypeHash(ChildLocalTransform.GetRotation().Euler() ));

	// global rotation hash
	const FTransform ChildGlobalTransform = GetChildGlobalTransform();
	Hash = HashCombine(Hash, GetTypeHash(ChildGlobalTransform.GetRotation().Euler() ));
	
	return Hash;
}

/** 
 * UTickableScaleConstraint
 **/

UTickableScaleConstraint::UTickableScaleConstraint()
{
	Type = ETransformConstraintType::Scale;
}

#if WITH_EDITOR

void UTickableScaleConstraint::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		return;
	}
	
	const FName PropertyName = PropertyChangedEvent.GetPropertyName();
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UTickableTransformConstraint, bDynamicOffset))
	{
		if (bDynamicOffset)
		{
			Cache.CachedInputHash = CalculateInputHash();
			
			const FVector ParentScale = GetParentGlobalTransform().GetScale3D();
			OffsetScale = GetChildGlobalTransform().GetScale3D();
			OffsetScale[0] = FMath::Abs(ParentScale[0]) > KINDA_SMALL_NUMBER ? OffsetScale[0] / ParentScale[0] : 0.f;
			OffsetScale[1] = FMath::Abs(ParentScale[1]) > KINDA_SMALL_NUMBER ? OffsetScale[1] / ParentScale[1] : 0.f;
			OffsetScale[2] = FMath::Abs(ParentScale[2]) > KINDA_SMALL_NUMBER ? OffsetScale[2] / ParentScale[2] : 0.f;
			
			Evaluate();
		}
		return;
	}
}

#endif

void UTickableScaleConstraint::ComputeOffset()
{
	const FTransform InitParentTransform = GetParentGlobalTransform();
	const FTransform InitChildTransform = GetChildGlobalTransform();
	
	OffsetScale = FVector::OneVector;
	if (bMaintainOffset)
	{
		const FVector InitParentScale = InitParentTransform.GetScale3D();
		OffsetScale = InitChildTransform.GetScale3D();
		OffsetScale[0] = FMath::Abs(InitParentScale[0]) > KINDA_SMALL_NUMBER ? OffsetScale[0] / InitParentScale[0] : 0.f;
		OffsetScale[1] = FMath::Abs(InitParentScale[1]) > KINDA_SMALL_NUMBER ? OffsetScale[1] / InitParentScale[1] : 0.f;
		OffsetScale[2] = FMath::Abs(InitParentScale[2]) > KINDA_SMALL_NUMBER ? OffsetScale[2] / InitParentScale[2] : 0.f;
	}
}

FConstraintTickFunction::ConstraintFunction UTickableScaleConstraint::GetFunction() const
{
	return [this]()
	{
		if (!IsFullyActive())
		{
			return;
		}
		
		const float ClampedWeight = FMath::Clamp<float>(Weight, 0.f, 1.f);
		if (ClampedWeight < KINDA_SMALL_NUMBER)
		{
			return;
		}

		ConstraintLocals::PreEvaluateHandle(ChildTRSHandle);
		
		const FVector ParentScale = GetParentGlobalTransform().GetScale3D();
		FTransform Transform = GetChildGlobalTransform();
		const FVector ChildScale = Transform.GetScale3D();
		
		FVector NewScale = (!bMaintainOffset) ? ParentScale : ParentScale * OffsetScale;
		if (ClampedWeight < 1.0f - KINDA_SMALL_NUMBER)
		{
			NewScale = FMath::Lerp<FVector>(ChildScale, NewScale, ClampedWeight);
		}

		AxisFilter.FilterVector(NewScale, ChildScale);

		Transform.SetScale3D(NewScale);
		
		SetChildGlobalTransform(Transform);
		if (ChildTRSHandle)
		{
			ChildTRSHandle->TickTarget();
		}
	};
}

void UTickableScaleConstraint::OnHandleModified(UTransformableHandle* InHandle, EHandleEvent InEvent)
{
	Super::OnHandleModified(InHandle, InEvent);
	
	if (!IsFullyActive() || !bDynamicOffset)
	{
		return;
	}
	
	if (!InHandle || InHandle != ChildTRSHandle)
	{
		return;
	}

	const bool bUpdateFromGlobal = (InEvent == EHandleEvent::GlobalTransformUpdated);
	const bool bUpdateTransform = InEvent == EHandleEvent::LocalTransformUpdated || bUpdateFromGlobal;
	if (!bUpdateTransform)
	{
		return;
	}

	const uint32 InputHash = CalculateInputHash();
	
	// update dynamic offset
	if (InputHash != Cache.CachedInputHash)
	{
		Cache.CachedInputHash = InputHash;

		if (bUpdateFromGlobal)
		{
			const FVector ParentScale = GetParentGlobalTransform().GetScale3D();
			OffsetScale = GetChildGlobalTransform().GetScale3D();
			OffsetScale[0] = FMath::Abs(ParentScale[0]) > KINDA_SMALL_NUMBER ? OffsetScale[0] / ParentScale[0] : 0.f;
			OffsetScale[1] = FMath::Abs(ParentScale[1]) > KINDA_SMALL_NUMBER ? OffsetScale[1] / ParentScale[1] : 0.f;
			OffsetScale[2] = FMath::Abs(ParentScale[2]) > KINDA_SMALL_NUMBER ? OffsetScale[2] / ParentScale[2] : 0.f;
		}
		else
		{
			const FTransform ChildLocalTransform = GetChildLocalTransform();
			OffsetScale = ChildLocalTransform.GetScale3D();
		}
	}
}

uint32 UTickableScaleConstraint::CalculateInputHash() const
{
	uint32 Hash = 0;

	// local scale hash
	const FTransform ChildLocalTransform = GetChildLocalTransform();
	Hash = HashCombine(Hash, GetTypeHash(ChildLocalTransform.GetScale3D() ));

	// global scale hash
	const FTransform ChildGlobalTransform = GetChildGlobalTransform();
	Hash = HashCombine(Hash, GetTypeHash(ChildGlobalTransform.GetScale3D() ));
	
	return Hash;
}

/** 
 * UTickableParentConstraint
 **/

UTickableParentConstraint::UTickableParentConstraint()
{
	Type = ETransformConstraintType::Parent;
}

void UTickableParentConstraint::ComputeOffset()
{
	const FTransform InitParentTransform = GetParentGlobalTransform();
	FTransform InitChildTransform = GetChildGlobalTransform();
	
	OffsetTransform = FTransform::Identity;
	if (bMaintainOffset || bDynamicOffset)
	{
		if (!bScaling)
		{
			InitChildTransform.RemoveScaling();
		}
		OffsetTransform = InitChildTransform.GetRelativeTransform(InitParentTransform); 
	}
}

uint32 UTickableParentConstraint::CalculateInputHash() const
{
	uint32 Hash = 0;
	
	const FTransform ChildLocalTransform = GetChildLocalTransform();
	Hash = HashCombine(Hash, GetTypeHash(ChildLocalTransform.GetTranslation()));
	Hash = HashCombine(Hash, GetTypeHash(ChildLocalTransform.GetRotation().Euler() ));
	Hash = HashCombine(Hash, GetTypeHash(ChildLocalTransform.GetScale3D()));
	
	const FTransform ChildGlobalTransform = GetChildGlobalTransform();
	Hash = HashCombine(Hash, GetTypeHash(ChildGlobalTransform.GetTranslation()));
	Hash = HashCombine(Hash, GetTypeHash(ChildGlobalTransform.GetRotation().Euler() ));
	Hash = HashCombine(Hash, GetTypeHash(ChildGlobalTransform.GetScale3D()));
	
	return Hash;
}

FConstraintTickFunction::ConstraintFunction UTickableParentConstraint::GetFunction() const
{
	return [this]()
	{
		if (!IsFullyActive())
		{
			return;
		}
		
		const float ClampedWeight = FMath::Clamp<float>(Weight, 0.f, 1.f);
		if (ClampedWeight < KINDA_SMALL_NUMBER)
		{
			return;
		}

		auto LerpAndFilterTransform = [ClampedWeight](
			const FTransform& InTransform, FTransform& OutTransform, const FTransformFilter& InFilter)
		{
			const FVector Location = InTransform.GetLocation();
			const FQuat Rotation = InTransform.GetRotation();
			const FVector Scale = InTransform.GetScale3D();

			FVector NewLocation = OutTransform.GetLocation();
			FQuat NewRotation = OutTransform.GetRotation();
			FVector NewScale = OutTransform.GetScale3D();
			
			if (ClampedWeight < 1.0f - KINDA_SMALL_NUMBER)
			{
				NewLocation = FMath::Lerp<FVector>(Location, NewLocation, ClampedWeight);
				NewRotation = FQuat::Slerp(Rotation, NewRotation, ClampedWeight);
				NewScale = FMath::Lerp<FVector>(Scale, NewScale, ClampedWeight);
			}

			InFilter.TranslationFilter.FilterVector(NewLocation, Location);
			InFilter.RotationFilter.FilterQuat(NewRotation, Rotation);
			InFilter.ScaleFilter.FilterVector(NewScale, Scale);

			OutTransform.SetLocation(NewLocation);
			OutTransform.SetRotation(NewRotation);
			OutTransform.SetScale3D(NewScale);
		};

		ConstraintLocals::PreEvaluateHandle(ChildTRSHandle);
		
		const FTransform ParentTransform = GetParentGlobalTransform();
		
		FTransform TargetTransform = (!bMaintainOffset) ? ParentTransform : OffsetTransform * ParentTransform;
		//apply weight if needed
		const FTransform ChildGlobalTransform = GetChildGlobalTransform();
		LerpAndFilterTransform(ChildGlobalTransform, TargetTransform, TransformFilter);

		//remove scale?
		if (!bScaling)
		{
			TargetTransform.SetScale3D(ChildGlobalTransform.GetScale3D());
		}

		SetChildGlobalTransform(TargetTransform);
		if (ChildTRSHandle)
		{
			ChildTRSHandle->TickTarget();
		}
	};
}

void UTickableParentConstraint::OnHandleModified(UTransformableHandle* InHandle, EHandleEvent InEvent)
{
	Super::OnHandleModified(InHandle, InEvent);
	
	if (!IsFullyActive() || !bDynamicOffset)
	{
		return;
	}
	
	if (!InHandle || InHandle != ChildTRSHandle)
	{
		return;
	}

	const bool bUpdateFromGlobal = (InEvent == EHandleEvent::GlobalTransformUpdated);
	const bool bUpdateTransform = (InEvent == EHandleEvent::LocalTransformUpdated) || bUpdateFromGlobal;
	if (!bUpdateTransform)
	{
		return;
	}
	
	const uint32 InputHash = CalculateInputHash();

	// update dynamic offset
	if (InputHash != Cache.CachedInputHash)
	{
		Cache.CachedInputHash = InputHash;

		if (bUpdateFromGlobal)
		{
			const FTransform ParentWorldTransform = GetParentGlobalTransform();
			FTransform ChildGlobalTransform = GetChildGlobalTransform();
			if (!bScaling)
			{
				ChildGlobalTransform.RemoveScaling();
			}
			OffsetTransform = ChildGlobalTransform.GetRelativeTransform(ParentWorldTransform);
		}
		else
		{
			OffsetTransform = GetChildLocalTransform();
		}
	}
}

#if WITH_EDITOR

void UTickableParentConstraint::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		return;
	}
	
	auto UpdateOffset = [&]()
	{
		FTransform ChildGlobalTransform = GetChildGlobalTransform();
		if (!bScaling)
		{
			ChildGlobalTransform.RemoveScaling();
		}
		const FTransform ParentWorldTransform = GetParentGlobalTransform();
		OffsetTransform = ChildGlobalTransform.GetRelativeTransform(ParentWorldTransform);
	};
	
	const FName PropertyName = PropertyChangedEvent.GetPropertyName();
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UTickableTransformConstraint, bDynamicOffset))
	{
		if(bDynamicOffset)
		{
			Cache.CachedInputHash = CalculateInputHash();
			UpdateOffset();			
			Evaluate();
		}
		return;
	}

	if (PropertyName == GET_MEMBER_NAME_CHECKED(UTickableParentConstraint, bScaling))
	{
		// notify scale change. note that this is currently the only property change we monitor but this broadcast
		// call could be made higher in the hierarchy to monitor other changes.
		OnConstraintChanged.Broadcast(this, PropertyChangedEvent);
		
		if (bMaintainOffset || bDynamicOffset)
		{
			Cache.CachedInputHash = CalculateInputHash();
			UpdateOffset();
			Evaluate();
			return;
		}
	}
}

#endif

/** 
 * UTickableLookAtConstraint
 **/

UTickableLookAtConstraint::UTickableLookAtConstraint()
{
	bMaintainOffset = false;
	bDynamicOffset = false;
	Type = ETransformConstraintType::LookAt;
}

void UTickableLookAtConstraint::ComputeOffset()
{
	bMaintainOffset = false;
	bDynamicOffset = false;

	const FVector InitParentLocation = GetParentGlobalTransform().GetLocation();
	const FTransform InitChildTransform = GetChildGlobalTransform();
	const FVector InitLookAtDir = (InitParentLocation - InitChildTransform.GetLocation()).GetSafeNormal();

	if (!InitLookAtDir.IsNearlyZero())
	{
		Axis = InitChildTransform.InverseTransformVectorNoScale(InitLookAtDir).GetSafeNormal();
	}
}

FConstraintTickFunction::ConstraintFunction UTickableLookAtConstraint::GetFunction() const
{
	return [this]()
	{
		if (!IsFullyActive())
		{
			return;
		}

		ConstraintLocals::PreEvaluateHandle(ChildTRSHandle);
		
		const FTransform ParentTransform = GetParentGlobalTransform();
		const FTransform ChildTransform = GetChildGlobalTransform();
		
		const FVector LookAtDir = (ParentTransform.GetLocation() - ChildTransform.GetLocation()).GetSafeNormal();

		if (!LookAtDir.IsNearlyZero() && !Axis.IsNearlyZero())
		{
			const FVector AxisToOrient = ChildTransform.TransformVectorNoScale(Axis).GetSafeNormal();
		
			FQuat Rotation = FindQuatBetweenNormals(AxisToOrient, LookAtDir);
			const bool bNeedsToBeRotated = !Rotation.IsIdentity();
			if (bNeedsToBeRotated)
			{
				Rotation = Rotation * ChildTransform.GetRotation();

				FTransform Transform = ChildTransform;
				Transform.SetRotation(Rotation.GetNormalized());
				SetChildGlobalTransform(Transform);
				if (ChildTRSHandle)
				{
					ChildTRSHandle->TickTarget();
				}
			}
		}
	};
}

bool UTickableLookAtConstraint::NeedsCompensation() const
{
	return false;
}

FQuat UTickableLookAtConstraint::FindQuatBetweenNormals(const FVector& A, const FVector& B)
{
	const FQuat::FReal Dot = FVector::DotProduct(A, B);
	FQuat::FReal W = 1 + Dot;
	FQuat Result;

	if (W < SMALL_NUMBER)
	{
		// A and B point in opposite directions
		W = 2 - W;
		Result = FQuat( -A.Y * B.Z + A.Z * B.Y, -A.Z * B.X + A.X * B.Z, -A.X * B.Y + A.Y * B.X, W).GetNormalized();

		const FVector Normal = FMath::Abs(A.X) > FMath::Abs(A.Y) ? FVector::YAxisVector : FVector::XAxisVector;
		const FVector BiNormal = FVector::CrossProduct(A, Normal);
		const FVector TauNormal = FVector::CrossProduct(A, BiNormal);
		Result = Result * FQuat(TauNormal, PI);
	}
	else
	{
		//Axis = FVector::CrossProduct(A, B);
		Result = FQuat( A.Y * B.Z - A.Z * B.Y, A.Z * B.X - A.X * B.Z, A.X * B.Y - A.Y * B.X, W);
	}

	Result.Normalize();
	return Result;
}

#if WITH_EDITOR

void UTickableLookAtConstraint::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		return;
	}

	const FName PropertyName = PropertyChangedEvent.GetMemberPropertyName();
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UTickableLookAtConstraint, Axis))
	{
		Evaluate();
		return;
	}
}

#endif

/** 
 * FTransformConstraintUtils
 **/

namespace
{

UTransformableHandle* GetHandle(UObject* InObject, const FName& InSocketName)
{
	// look for customized transform handle
	const FTransformableRegistry& Registry = FTransformableRegistry::Get();
	if (const FTransformableRegistry::CreateHandleFuncT CreateFunction = Registry.GetCreateFunction(InObject->GetClass()))
	{
		return CreateFunction(InObject, InSocketName);
	}

	return nullptr;
}
	
uint32 GetConstrainableHash(const UObject* InObject)
{
	// look for customized hash function
	const FTransformableRegistry& Registry = FTransformableRegistry::Get();
	if (const FTransformableRegistry::GetHashFuncT HashFunction = Registry.GetHashFunction(InObject->GetClass()))
	{
		return HashFunction(InObject, NAME_None);
	}

	return 0;
}
	
}

UTransformableComponentHandle* FTransformConstraintUtils::CreateHandleForSceneComponent(
	USceneComponent* InSceneComponent,
	const FName& InSocketName)
{
	UTransformableComponentHandle* ComponentHandle = nullptr;
	if (InSceneComponent)
	{
		ComponentHandle = NewObject<UTransformableComponentHandle>(GetTransientPackage(), NAME_None, RF_Transactional);
		ComponentHandle->Component = InSceneComponent;
		ComponentHandle->SocketName = InSocketName;
		InSceneComponent->SetMobility(EComponentMobility::Movable);
		ComponentHandle->RegisterDelegates();
	}
	return ComponentHandle;
}

void FTransformConstraintUtils::GetParentConstraints(
	UWorld* InWorld,
	const AActor* InChild,
	TArray< TWeakObjectPtr<UTickableConstraint> >& OutConstraints)
{
	if (!InWorld || !InChild)
	{
		return;
	}

	const uint32 ChildHash = GetConstrainableHash(InChild);
	if (ChildHash == 0)
	{
		return;
	}
	
	static constexpr bool bSorted = true;
	const FConstraintsManagerController& Controller = FConstraintsManagerController::Get(InWorld);
	OutConstraints.Append(Controller.GetParentConstraints(ChildHash, bSorted));
}

UTickableTransformConstraint* FTransformConstraintUtils::CreateFromType(
	UWorld* InWorld,
	const ETransformConstraintType InType,
	const bool bUseDefault)
{
	if (!InWorld)
	{
		UE_LOG(LogTemp, Error, TEXT("FTransformConstraintUtils::CreateFromType sanity check failed."));
		return nullptr;
	}
	const UEnum* ETransformConstraintTypeEnum = StaticEnum<ETransformConstraintType>();
	if (!ETransformConstraintTypeEnum->IsValidEnumValue(static_cast<int64>(InType)))
	{
		UE_LOG(LogTemp, Error, TEXT("Constraint Type %d not recognized"), int(InType));
		return nullptr;
	}


	// unique name (we may want to use another approach here to manage uniqueness)
	const FString ConstraintTypeStr = ETransformConstraintTypeEnum->GetNameStringByValue((uint8)InType);
	const FName BaseName(*FString::Printf(TEXT("%sConstraint"), *ConstraintTypeStr));

	const FConstraintsManagerController& Controller = FConstraintsManagerController::Get(InWorld);
	UTickableTransformConstraint* Constraint = nullptr;

	switch (InType)
	{
	case ETransformConstraintType::Translation:
		Constraint = Controller.AllocateConstraintT<UTickableTranslationConstraint>(BaseName, bUseDefault);
		break;
	case ETransformConstraintType::Rotation:
		Constraint = Controller.AllocateConstraintT<UTickableRotationConstraint>(BaseName, bUseDefault);
		break;
	case ETransformConstraintType::Scale:
		Constraint = Controller.AllocateConstraintT<UTickableScaleConstraint>(BaseName, bUseDefault);
		break;
	case ETransformConstraintType::Parent:
		Constraint = Controller.AllocateConstraintT<UTickableParentConstraint>(BaseName, bUseDefault);
		break;
	case ETransformConstraintType::LookAt:
		Constraint = Controller.AllocateConstraintT<UTickableLookAtConstraint>(BaseName, bUseDefault);
		break;
	default:
		ensure(false);
		break;
	}
	return Constraint;
}

namespace
{

// we suppose that both InParentHandle and InChildHandle are safe to use
bool HasConstraintDependencyWith(UWorld* InWorld, const UTransformableHandle* InParentHandle, const UTransformableHandle* InChildHandle)
{
	using ConstraintPtr = TWeakObjectPtr<UTickableConstraint>;
	using HandlePtr = TObjectPtr<UTransformableHandle>;

	static constexpr bool bSorted = false;
	const FConstraintsManagerController& Controller = FConstraintsManagerController::Get(InWorld);
	const TArray<ConstraintPtr> Constraints = Controller.GetParentConstraints(InParentHandle->GetHash(), bSorted);

	// get parent handles
	TArray< HandlePtr > ParentHandles;
	for (const ConstraintPtr& Constraint: Constraints)
	{
		if (const UTickableTransformConstraint* TransformConstraint = Cast<UTickableTransformConstraint>(Constraint.Get()))
		{
			if (IsValid(TransformConstraint->ParentTRSHandle))
			{
				ParentHandles.Add(TransformConstraint->ParentTRSHandle);
			}
		}
	}

	// check if InChildHandle is one of them
	const uint32 ChildHash = InChildHandle->GetHash();
	const bool bIsParentADependency = ParentHandles.ContainsByPredicate([ChildHash](const HandlePtr& InHandle)
	{
		return InHandle->GetHash() == ChildHash;
	});

	if (bIsParentADependency)
	{
		return true;
	}

	// if not, recurse
	for (const HandlePtr& ParentHandle: ParentHandles)
	{
		if (HasConstraintDependencyWith(InWorld, ParentHandle, InChildHandle))
		{
			return true;
		}
	}

	return false;
}
	
bool AreHandlesConstrainable( UWorld* InWorld, UTransformableHandle* InParentHandle, UTransformableHandle* InChildHandle)
{
	static const TCHAR* ErrorPrefix = TEXT("Dependency error:");

	if (InChildHandle->GetHash() == InParentHandle->GetHash())
	{
		UE_LOG(LogTemp, Error, TEXT("%s handles are pointing at the same object."), ErrorPrefix);
		return false;
	}

	// check for direct transform dependencies (ei hierarchy)
	if (InParentHandle->HasDirectDependencyWith(*InChildHandle))
	{
#if WITH_EDITOR
		UE_LOG(LogTemp, Error, TEXT("%s: %s has a direct dependency with %s."),
			ErrorPrefix, *InParentHandle->GetLabel(), *InChildHandle->GetLabel());
#endif
		return false;
	}

	// check for indirect transform dependencies (ei constraint chain)
	if (HasConstraintDependencyWith(InWorld, InParentHandle, InChildHandle))
	{
#if WITH_EDITOR
		UE_LOG(LogTemp, Error, TEXT("%s: %s has an indirect dependency with %s."),
			ErrorPrefix, *InParentHandle->GetLabel(), *InChildHandle->GetLabel());
#endif
		return false;
	}

	return true;
}

}

UTickableTransformConstraint* FTransformConstraintUtils::CreateAndAddFromObjects(
	UWorld* InWorld,
	UObject* InParent, const FName& InParentSocketName,
	UObject* InChild, const FName& InChildSocketName,
	const ETransformConstraintType InType,
	const bool bMaintainOffset,
	const bool bUseDefault)
{
	static const TCHAR* ErrorPrefix = TEXT("FTransformConstraintUtils::CreateAndAddFromActors");
	
	// SANITY CHECK
	if (!InWorld || !InParent || !InChild)
	{
		UE_LOG(LogTemp, Error, TEXT("%s sanity check failed."), ErrorPrefix);
		return nullptr;
	}

	UTransformableHandle* ParentHandle = GetHandle(InParent, InParentSocketName);
	if (!ParentHandle)
	{
		return nullptr;
	}
	
	UTransformableHandle* ChildHandle = GetHandle(InChild, InChildSocketName);
	if (!ChildHandle)
	{
		return nullptr;
	}

	const bool bCanConstrain = AreHandlesConstrainable(InWorld, ParentHandle, ChildHandle);
	if (!bCanConstrain)
	{
		ChildHandle->MarkAsGarbage();
		ParentHandle->MarkAsGarbage();
		return nullptr;
	}
	
	UTickableTransformConstraint* Constraint = CreateFromType(InWorld, InType, bUseDefault);
	if (Constraint && (ParentHandle->IsValid() && ChildHandle->IsValid()))
	{
		if (AddConstraint(InWorld, ParentHandle, ChildHandle, Constraint, bMaintainOffset, bUseDefault) == false)
		{
			Constraint->MarkAsGarbage();
			Constraint = nullptr;
		}
	}
	return Constraint;
}

bool FTransformConstraintUtils::AddConstraint(
	UWorld* InWorld,
	UTransformableHandle* InParentHandle,
	UTransformableHandle* InChildHandle,
	UTickableTransformConstraint* InNewConstraint,
	const bool bMaintainOffset,
	const bool bUseDefault)
{
	using namespace ConstraintLocals;
	
	const bool bIsValidParent = InParentHandle && InParentHandle->IsValid();
	const bool bIsValidChild = InChildHandle && InChildHandle->IsValid();
	if (!bIsValidParent || !bIsValidChild)
	{
		UE_LOG(LogTemp, Error, TEXT("FTransformConstraintUtils::AddConst error adding constraint"));
		return false;
	}

	if (InNewConstraint == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("FTransformConstraintUtils::AddConst error creating constraint"));
		return false;
	}

	// set handles before calling AddConstraint as it will build dependencies based on them
	InNewConstraint->ParentTRSHandle = InParentHandle;
	InNewConstraint->ChildTRSHandle = InChildHandle;

	// add the new one
	const FConstraintsManagerController& Controller = FConstraintsManagerController::Get(InWorld);
	const bool bConstraintAdded = Controller.AddConstraint(InNewConstraint);

	if (!bConstraintAdded)
	{
		InNewConstraint->ParentTRSHandle = nullptr;
		InNewConstraint->ChildTRSHandle = nullptr;
		return false;
	}

	if (!bUseDefault)
	{
		InNewConstraint->bMaintainOffset = bMaintainOffset;
		InNewConstraint->Setup();
	}
	InNewConstraint->InitConstraint(InWorld);
	
	return true;
}

bool FTransformConstraintUtils::BuildDependencies(UWorld* InWorld, UTickableTransformConstraint* InConstraint)
{
	using namespace ConstraintLocals;
	
	if (!ensure(InWorld))
	{
		return false;
	}

	if (!InConstraint || !InConstraint->IsValid())
	{
		return false;
	}

	if (bDebugDependencies)
	{
		UE_LOG(LogTemp, Warning, TEXT("Building dependencies for '%s' ..."), *GetConstraintLabel(InConstraint));
	}

	const UTransformableHandle* ParentHandle = InConstraint->ParentTRSHandle.Get();
	const UTransformableHandle* ChildHandle = InConstraint->ChildTRSHandle.Get();
	
 	// get previous child constraints
	const FConstraintsManagerController& Controller = FConstraintsManagerController::Get(InWorld);
	TArray<TWeakObjectPtr<UTickableConstraint>> ChildParentConstraints = Controller.GetParentConstraints(ChildHandle->GetHash(), true);
	ChildParentConstraints.Remove(InConstraint);

	// add dependencies with the last child constraint
	if (!ChildParentConstraints.IsEmpty())
	{
		const FGuid LastChildConstraintID = ChildParentConstraints.Last()->ConstraintID;
		if (bDebugDependencies)
		{
			UE_LOG(LogTemp, Warning, TEXT("DEPENDENCY 0: tick after last constraint."));
		}
		Controller.SetConstraintsDependencies(LastChildConstraintID, InConstraint->ConstraintID);
	}

	const UObject* ParentTarget = ParentHandle->GetTarget().Get();
	const UObject* ChildTarget = ChildHandle->GetTarget().Get();
	const bool bSelf = ParentTarget && ParentTarget == ChildTarget;
	
	// internal dependencies?
	if (bSelf && bIncludeTarget)
	{
		const UObject* SelfTarget = ChildTarget;
		
		using ConstraintPtr = TWeakObjectPtr<UTickableConstraint>;
		auto Predicate = [InConstraint, SelfTarget](const ConstraintPtr& Constraint)
		{
			const UTickableTransformConstraint* TransformConstraint = Cast<UTickableTransformConstraint>(Constraint.Get());
			if (!TransformConstraint || TransformConstraint == InConstraint)
			{
				return false;
			}
			const UObject* ParentTarget = GetHandleTarget(TransformConstraint->ParentTRSHandle);
			const UObject* ChildTarget = GetHandleTarget(TransformConstraint->ChildTRSHandle);
			return ParentTarget == SelfTarget && ChildTarget == SelfTarget;
		};

		const TArray< TWeakObjectPtr<UTickableConstraint> > SelfConstraints = Controller.GetConstraintsByPredicate(Predicate);
		for (const ConstraintPtr& SelfConstraint: SelfConstraints)
		{
			const UTickableTransformConstraint* TransformConstraint = Cast<UTickableTransformConstraint>(SelfConstraint);

			// if the new handles depend on that constraint child then, TransformConstraint should tick before
			if (ParentHandle->HasDirectDependencyWith(*TransformConstraint->ChildTRSHandle))
			{
				Controller.SetConstraintsDependencies(TransformConstraint->ConstraintID, InConstraint->ConstraintID);

				if (bDebugDependencies)
				{
					UE_LOG(LogTemp, Warning, TEXT("DEPENDENCY 1: %s is parent of %s so %s must tick before %s"),
					   *GetHandleLabel(TransformConstraint->ChildTRSHandle), *GetHandleLabel(ParentHandle),
					   *GetConstraintLabel(TransformConstraint), *GetConstraintLabel(InConstraint));
				}
			}
			else if (ChildHandle->HasDirectDependencyWith(*TransformConstraint->ChildTRSHandle))
			{
				Controller.SetConstraintsDependencies(TransformConstraint->ConstraintID, InConstraint->ConstraintID);

				if (bDebugDependencies)
				{
					UE_LOG(LogTemp, Warning, TEXT("DEPENDENCY 1: %s is parent of %s so %s must tick before %s"),
					   *GetHandleLabel(TransformConstraint->ChildTRSHandle), *GetHandleLabel(ChildHandle),
					   *GetConstraintLabel(TransformConstraint), *GetConstraintLabel(InConstraint));
				}
			}

			// if the TransformConstraint handles depend on the new constraint child then, TransformConstraint should tick after
			if (TransformConstraint->ParentTRSHandle->HasDirectDependencyWith(*ChildHandle))
			{
			 	Controller.SetConstraintsDependencies(InConstraint->ConstraintID, TransformConstraint->ConstraintID);

				if (bDebugDependencies)
				{
					UE_LOG(LogTemp, Warning, TEXT("DEPENDENCY 1: %s is parent of %s so %s must tick before %s"),
					   *GetHandleLabel(ChildHandle), *GetHandleLabel(TransformConstraint->ParentTRSHandle),
					   *GetConstraintLabel(InConstraint), *GetConstraintLabel(TransformConstraint));
				}
			}
			else if (TransformConstraint->ChildTRSHandle->HasDirectDependencyWith(*ChildHandle))
			{
				Controller.SetConstraintsDependencies(InConstraint->ConstraintID, TransformConstraint->ConstraintID);

				if (bDebugDependencies)
				{
					UE_LOG(LogTemp, Warning, TEXT("DEPENDENCY 1: %s is parent of %s so %s must tick before %s"),
					   *GetHandleLabel(ChildHandle), *GetHandleLabel(TransformConstraint->ChildTRSHandle),
					   *GetConstraintLabel(InConstraint), *GetConstraintLabel(TransformConstraint) );
				}
			}
		}
	}

	// make sure we tick after the parent.
	InConstraint->EnsurePrimaryDependency(InWorld);

	// if child handle is the parent of some other constraints, ensure they will tick after that new one
	TArray<TWeakObjectPtr<UTickableConstraint>> ChildChildConstraints;
	GetChildrenConstraints(InWorld, InConstraint, ChildChildConstraints, bIncludeTarget && !bSelf);
	for (const TWeakObjectPtr<UTickableConstraint>& ChildConstraint: ChildChildConstraints)
	{
		Controller.SetConstraintsDependencies(InConstraint->ConstraintID, ChildConstraint->ConstraintID);

		if (bDebugDependencies)
		{
			const UTickableTransformConstraint* TransformConstraint = Cast<UTickableTransformConstraint>(ChildConstraint);
			UE_LOG(LogTemp, Warning, TEXT("DEPENDENCY 2: %s is parent of %s so %s must tick before %s"),
				*GetHandleLabel(ChildHandle), *GetHandleLabel(TransformConstraint->ChildTRSHandle),
				*GetConstraintLabel(InConstraint), *GetConstraintLabel(TransformConstraint));
		}
	}

	// warn for possible cycles
	if (FConstraintCycleChecker::IsCycling(InConstraint->ChildTRSHandle))
	{
		UE_LOG(LogTemp, Warning, TEXT("A cycle has been formed while creating %s."), *InConstraint->GetName());
	}

	// invalidate graph
	Controller.InvalidateEvaluationGraph();
	
	return true;
}

void FTransformConstraintUtils::UpdateTransformBasedOnConstraint(FTransform& CurrentTransform, USceneComponent* SceneComponent)
{
	TArray< TWeakObjectPtr<UTickableConstraint> > Constraints;
	AActor* ShapeActor = SceneComponent->GetTypedOuter<AActor>();

	if (ShapeActor)
	{
		FTransformConstraintUtils::GetParentConstraints(SceneComponent->GetWorld(), ShapeActor, Constraints);

		const int32 LastActiveIndex = FTransformConstraintUtils::GetLastActiveConstraintIndex(Constraints);
		if (Constraints.IsValidIndex(LastActiveIndex))
		{
			// switch to constraint space
			const FTransform WorldTransform = SceneComponent->GetSocketTransform(SceneComponent->GetAttachSocketName());
			const TOptional<FTransform> RelativeTransform =
				FTransformConstraintUtils::GetConstraintsRelativeTransform(Constraints, CurrentTransform, WorldTransform);
			if (RelativeTransform)
			{
				CurrentTransform = *RelativeTransform;
			}
		}
	}
}

FTransform FTransformConstraintUtils::ComputeRelativeTransform(
	const FTransform& InChildLocal,
	const FTransform& InChildWorld,
	const FTransform& InSpaceWorld,
	const UTickableTransformConstraint* InConstraint)
{
	if (!InConstraint)
	{
		return InChildWorld.GetRelativeTransform(InSpaceWorld);
	}
	
	const ETransformConstraintType ConstraintType = static_cast<ETransformConstraintType>(InConstraint->GetType());
	switch (ConstraintType)
	{
	case ETransformConstraintType::Translation:
		{
			FTransform RelativeTransform = InChildLocal;
			FVector RelativeTranslation = InChildWorld.GetLocation() - InSpaceWorld.GetLocation();
			if (const UTickableTranslationConstraint* TranslationConstraint = Cast<UTickableTranslationConstraint>(InConstraint))
			{
				TranslationConstraint->AxisFilter.FilterVector(RelativeTranslation, InChildLocal.GetTranslation());
			}
			RelativeTransform.SetLocation(RelativeTranslation);
			return RelativeTransform;
		}
	case ETransformConstraintType::Rotation:
		{
			FTransform RelativeTransform = InChildLocal;
			FQuat RelativeRotation = InSpaceWorld.GetRotation().Inverse() * InChildWorld.GetRotation();
			RelativeRotation.Normalize();
			if (const UTickableRotationConstraint* RotationConstraint = Cast<UTickableRotationConstraint>(InConstraint))
			{
				RotationConstraint->AxisFilter.FilterQuat(RelativeRotation, InChildLocal.GetRotation());
			}
			RelativeTransform.SetRotation(RelativeRotation);
			return RelativeTransform;
		}
	case ETransformConstraintType::Scale:
		{
			FTransform RelativeTransform = InChildLocal;
			const FVector SpaceScale = InSpaceWorld.GetScale3D();
			FVector RelativeScale = InChildWorld.GetScale3D();
			RelativeScale[0] = FMath::Abs(SpaceScale[0]) > KINDA_SMALL_NUMBER ? RelativeScale[0] / SpaceScale[0] : 0.f;
			RelativeScale[1] = FMath::Abs(SpaceScale[1]) > KINDA_SMALL_NUMBER ? RelativeScale[1] / SpaceScale[1] : 0.f;
			RelativeScale[2] = FMath::Abs(SpaceScale[2]) > KINDA_SMALL_NUMBER ? RelativeScale[2] / SpaceScale[2] : 0.f;
			if (const UTickableScaleConstraint* ScaleConstraint = Cast<UTickableScaleConstraint>(InConstraint))
			{
				ScaleConstraint->AxisFilter.FilterVector(RelativeScale, InChildLocal.GetScale3D());
			}
			RelativeTransform.SetScale3D(RelativeScale);
			return RelativeTransform;
		}
	case ETransformConstraintType::Parent:
		{
			const UTickableParentConstraint* ParentConstraint = Cast<UTickableParentConstraint>(InConstraint);
			const bool bScale = ParentConstraint ? ParentConstraint->IsScalingEnabled() : true;
			
			FTransform ChildTransform = InChildWorld;
			if (!bScale)
			{
				ChildTransform.RemoveScaling();
			}

			FTransform RelativeTransform = ChildTransform.GetRelativeTransform(InSpaceWorld);

			if (ParentConstraint && !ParentConstraint->TransformFilter.TranslationFilter.HasNoEffect())
			{
				FVector RelativeLocation = RelativeTransform.GetLocation();
				ParentConstraint->TransformFilter.TranslationFilter.FilterVector(RelativeLocation, InChildLocal.GetLocation());
				RelativeTransform.SetLocation(RelativeLocation);
			}

			if (ParentConstraint && !ParentConstraint->TransformFilter.RotationFilter.HasNoEffect())
			{
				FQuat RelativeRotation = RelativeTransform.GetRotation();
				ParentConstraint->TransformFilter.RotationFilter.FilterQuat(RelativeRotation, InChildLocal.GetRotation());
				RelativeTransform.SetRotation(RelativeRotation);
			}

			if (ParentConstraint && !ParentConstraint->TransformFilter.ScaleFilter.HasNoEffect())
			{
				FVector RelativeScale = RelativeTransform.GetScale3D();
				ParentConstraint->TransformFilter.ScaleFilter.FilterVector(RelativeScale, InChildLocal.GetScale3D());
				RelativeTransform.SetScale3D(RelativeScale);
			}
			
			if (!bScale)
			{
				RelativeTransform.SetScale3D(InChildLocal.GetScale3D());
			}
			return RelativeTransform;
		}
	case ETransformConstraintType::LookAt:
		return InChildLocal;
	default:
		break;
	}
	
	return InChildWorld.GetRelativeTransform(InSpaceWorld);
}

TOptional<FTransform> FTransformConstraintUtils::GetRelativeTransform(UWorld* InWorld, const uint32 InHandleHash)
{
	if (!InWorld || InHandleHash <= 0)
	{
		return TOptional<FTransform>();
	}

	static constexpr bool bSorted = true;
	const FConstraintsManagerController& Controller = FConstraintsManagerController::Get(InWorld);

	const TArray< TWeakObjectPtr<UTickableConstraint> > Constraints = Controller.GetParentConstraints(InHandleHash, bSorted);
	if (Constraints.IsEmpty())
	{
		return TOptional<FTransform>();
	}

	// get current active transform constraint 	
	const int32 LastActiveIndex = GetLastActiveConstraintIndex(Constraints);
	if (!Constraints.IsValidIndex(LastActiveIndex))
	{
		return TOptional<FTransform>();
	}

	// get relative transform
	const UTickableTransformConstraint* Constraint = Cast<UTickableTransformConstraint>(Constraints[LastActiveIndex]);
	const FTransform ChildLocal = Constraint->GetChildLocalTransform();
	const FTransform ChildGlobal = Constraint->GetChildGlobalTransform();
	
	return GetConstraintsRelativeTransform(Constraints, ChildLocal, ChildGlobal);
}

TOptional<FTransform> FTransformConstraintUtils::GetConstraintsRelativeTransform(
	const TArray< TWeakObjectPtr<UTickableConstraint> >& InConstraints,
	const FTransform& InChildLocal, const FTransform& InChildWorld)
{
	if (InConstraints.IsEmpty())
	{
		return TOptional<FTransform>();
	}

	// get current active transform constraint
	const int32 LastActiveIndex = GetLastActiveConstraintIndex(InConstraints);
	if (!InConstraints.IsValidIndex(LastActiveIndex))
	{
		return TOptional<FTransform>();
	}

	// get relative transform
	// if that constraint handles the entire transform then return the relative transform directly
	const UTickableTransformConstraint* Constraint = Cast<UTickableTransformConstraint>(InConstraints[LastActiveIndex]);
	if (EnumHasAllFlags(Constraint->GetChannelsToKey(), EMovieSceneTransformChannel::AllTransform))
	{
		const FTransform ParentGlobal = Constraint->GetParentGlobalTransform();
		return ComputeRelativeTransform(InChildLocal, InChildWorld, ParentGlobal, Constraint);
	}

	// otherwise, we need to look for constraints on a sub-transform basis so we compute the relative transform for each of them
	using ConstraintPtr = TWeakObjectPtr<UTickableConstraint>;
	auto GetLastSubTransformIndex = [InConstraints](const EMovieSceneTransformChannel& InChannel)
	{
		return InConstraints.FindLastByPredicate([InChannel](const ConstraintPtr& InConstraint)
		{
			if (const UTickableTransformConstraint* TransformConstraint = Cast<UTickableTransformConstraint>(InConstraint.Get()))
			{
				const bool bHasChannelFlag = EnumHasAllFlags(TransformConstraint->GetChannelsToKey(), InChannel);
				return InConstraint->IsFullyActive() && TransformConstraint->bDynamicOffset && bHasChannelFlag;
			}
			return false;
		});
	};

	// look for last constraint index for each channel 
	static constexpr EMovieSceneTransformChannel SubChannels[3] = {	EMovieSceneTransformChannel::Translation,
																	EMovieSceneTransformChannel::Rotation,
																	EMovieSceneTransformChannel::Scale };
	TArray<int32> SubTransformIndices;
	for (int32 Index = 0; Index < 3; Index++)
	{
		const int32 LastSubIndex = GetLastSubTransformIndex(SubChannels[Index]);
    	if (InConstraints.IsValidIndex(LastSubIndex))  
    	{
    		SubTransformIndices.AddUnique(LastSubIndex);
    	}
	}
	SubTransformIndices.Sort();

	// if none then return 
	if (SubTransformIndices.IsEmpty())
	{
		return TOptional<FTransform>();
	}

	// iterate thru constraints to compute the relative transform in each of them 
	FTransform ChildLocal = InChildLocal;
	for (const int32 SubConstraintIndex: SubTransformIndices)
	{
		const UTickableTransformConstraint* SubConstraint = Cast<UTickableTransformConstraint>(InConstraints[SubConstraintIndex]);
		const FTransform ParentGlobal = SubConstraint->GetParentGlobalTransform();
		ChildLocal = ComputeRelativeTransform(ChildLocal, InChildWorld, ParentGlobal, SubConstraint);
	}
	return ChildLocal;
}

int32 FTransformConstraintUtils::GetLastActiveConstraintIndex(const TArray< TWeakObjectPtr<UTickableConstraint> >& InConstraints)
{
	return InConstraints.FindLastByPredicate([](const TWeakObjectPtr<UTickableConstraint>& InConstraint)
	{
	   if (const UTickableTransformConstraint* TransformConstraint = Cast<UTickableTransformConstraint>(InConstraint.Get()))
	   {
		   return InConstraint->Active && TransformConstraint->bDynamicOffset;
	   }
	   return false;
	});
}

void FTransformConstraintUtils::GetChildrenConstraints(
	UWorld* World,
	const UTickableTransformConstraint* InConstraint,
	TArray< TWeakObjectPtr<UTickableConstraint> >& OutConstraints,
	const bool bIncludeTarget)
{
	if (!InConstraint || !InConstraint->IsValid())
	{
		// this probably has been checked before but we want to make sure the data is safe to use
		return;
	}
	
	const UTransformableHandle* Handle = InConstraint->ChildTRSHandle.Get();
	
	using ConstraintPtr = TWeakObjectPtr<UTickableConstraint>;
	
	// filter for transform constraints where the InHandle is the parent (based on its hash value)
	// and also has the same target if bIncludeTarget is true
	const uint32 ParentHash = Handle->GetHash();
	const UObject* ParentTarget = Handle->GetTarget().Get();
	auto Predicate = [InConstraint, Handle, ParentHash, bIncludeTarget, ParentTarget, World](const ConstraintPtr& Constraint)
	{
		const UTickableTransformConstraint* TransformConstraint = Cast<UTickableTransformConstraint>(Constraint.Get());
		if (!TransformConstraint || TransformConstraint == InConstraint)
		{
			return false;
		}

		if (TransformConstraint->ParentTRSHandle)
		{
			const UTransformableHandle* OtherParentHandle = TransformConstraint->ParentTRSHandle;
			if (OtherParentHandle->GetHash() == ParentHash)
			{
				return true;
			}

			if (bIncludeTarget && ParentTarget)
			{
				const UObject* Target = OtherParentHandle->GetTarget().Get();
				if (Target == ParentTarget)
				{
					// check direct dependencies to avoid evaluation order issues
					if (Handle->HasDirectDependencyWith(*OtherParentHandle))
					{
						return false;
					}

					// check constraints dependencies to avoid cycles
					if (HasConstraintDependencyWith(World, OtherParentHandle, Handle))
					{
						return false;
					}

					// check TransformConstraint's ChildHandle
					if (const UTransformableHandle* OtherChildHandle = TransformConstraint->ChildTRSHandle)
					{
						const UTransformableHandle* ParentHandle = InConstraint->ParentTRSHandle.Get();
						
						// if TransformConstraint's child is InConstraint's parent then avoid cycles
						if (OtherChildHandle->GetHash() == ParentHandle->GetHash())
						{
							return false;
						}

						// check dependencies with OtherChildHandle to avoid cycles
						const FHandleDependencyChecker Checker(World);
						if (Checker.HasDependency(*ParentHandle, *OtherChildHandle))
						{
							return false;
						}
						
						// check dependencies with OtherParentHandle to avoid cycles
						if (Checker.HasDependency(*ParentHandle, *OtherParentHandle))
						{
							return false;
						}
					}
					
					return true;
				}
			}
		}

		return false;
	};

	const FConstraintsManagerController& Controller = FConstraintsManagerController::Get(World);
	const TArray<ConstraintPtr> FilteredConstraints = Controller.GetConstraintsByPredicate(Predicate);
	OutConstraints.Append(FilteredConstraints);
}

FConstraintDependencyScope::FConstraintDependencyScope(UTickableTransformConstraint* InConstraint, UWorld* InWorld)
	: WeakConstraint(InConstraint)
	, WeakWorld(InWorld)
	, bPreviousValidity(InConstraint ? InConstraint->IsValid() : false)
{}

FConstraintDependencyScope::~FConstraintDependencyScope()
{
	if (!bPreviousValidity)
	{
		if (UTickableTransformConstraint* Constraint = WeakConstraint.IsValid() ? WeakConstraint.Get() : nullptr)
		{
			if (Constraint->IsValid())
			{
				const UObject* Target = ConstraintLocals::GetHandleTarget(Constraint->ChildTRSHandle);
				UWorld* World = WeakWorld.IsValid() ? WeakWorld.Get() : Target ? Target->GetWorld() : nullptr;
				if (::IsValid(World))
				{
					FTransformConstraintUtils::BuildDependencies(World, Constraint);
				}
			}
		}
	}
}

FHandleDependencyChecker::FHandleDependencyChecker(UWorld* InWorld)
	: WeakWorld(InWorld)
{}

bool FHandleDependencyChecker::HasDependency(const UTransformableHandle& InHandle, const UTransformableHandle& InParentToCheck) const
{
	// check direct dependency
	if (InHandle.HasDirectDependencyWith(InParentToCheck))
	{
		return true;
	}

	UWorld* World = WeakWorld.IsValid() ? WeakWorld.Get() : nullptr;
	if (::IsValid(World))
	{
		// check constraints dependency
		if (HasConstraintDependencyWith(World, &InHandle, &InParentToCheck))
		{
			return true;
		}

		// check any existing tick dependency
		{
			TSet<const FTickFunction*> VisitedFunctions;
			const FTickFunction* TickFunction = InHandle.GetTickFunction();
			const FTickFunction* ParentTickFunctionToCheck = InParentToCheck.GetTickFunction();
			if (FConstraintCycleChecker::HasPrerequisiteDependencyWith(TickFunction, ParentTickFunctionToCheck, VisitedFunctions))
			{
				return true;
			}
		}
	}
	
	return false;
}