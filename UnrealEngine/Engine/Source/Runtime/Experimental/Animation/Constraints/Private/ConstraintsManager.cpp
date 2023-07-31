// Copyright Epic Games, Inc. All Rights Reserved.

#include "ConstraintsManager.inl"
#include "ConstraintsActor.h"

#include "Algo/Copy.h"
#include "Algo/StableSort.h"

#include "Engine/World.h"
#include "Engine/Level.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ConstraintsManager)

/** 
 * FConstraintTickFunction
 **/

FConstraintTickFunction::FConstraintTickFunction()
{
	TickGroup = TG_PrePhysics;
	bCanEverTick = true;
	bStartWithTickEnabled = true;
	bHighPriority = true;
}

FConstraintTickFunction::~FConstraintTickFunction()
{}

void FConstraintTickFunction::ExecuteTick(
	float DeltaTime,
	ELevelTick TickType,
	ENamedThreads::Type CurrentThread,
	const FGraphEventRef& MyCompletionGraphEvent)
{
	EvaluateFunctions();
}

void FConstraintTickFunction::RegisterFunction(ConstraintFunction InConstraint)
{
	ConstraintFunctions.Add(InConstraint);
}

void FConstraintTickFunction::EvaluateFunctions() const
{
	for (const ConstraintFunction& Function: ConstraintFunctions)
	{
		Function();
	}	
}

FString FConstraintTickFunction::DiagnosticMessage()
{
	if(!Constraint.IsValid())
	{
		return FString::Printf(TEXT("FConstraintTickFunction::Tick[%p]"), this);	
	}

#if WITH_EDITOR
	return FString::Printf(TEXT("FConstraintTickFunction::Tick[%p] (%s)"), this, *Constraint->GetLabel());
#else
	return FString::Printf(TEXT("FConstraintTickFunction::Tick[%p] (%s)"), this, *Constraint->GetName());
#endif
}

/** 
 * UTickableConstraint
 **/

void UTickableConstraint::SetActive(const bool bIsActive)
{
	Active = bIsActive;
	ConstraintTick.SetTickFunctionEnable(bIsActive);
}

bool UTickableConstraint::IsFullyActive() const
{
	return Active;
}

void UTickableConstraint::Evaluate(bool bTickHandlesAlso) const
{
	ConstraintTick.EvaluateFunctions();
}

UTickableConstraint* UTickableConstraint::Duplicate(UObject* NewOuter) const
{
	return DuplicateObject<UTickableConstraint>(this, NewOuter, GetFName());
}

#if WITH_EDITOR

FString UTickableConstraint::GetLabel() const
{
	return UTickableConstraint::StaticClass()->GetName();
}

FString UTickableConstraint::GetFullLabel() const
{
	return GetLabel();
}

FString UTickableConstraint::GetTypeLabel() const
{
	return GetLabel();
}

void UTickableConstraint::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = PropertyChangedEvent.GetPropertyName();
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UTickableConstraint, Active))
	{
		ConstraintTick.SetTickFunctionEnable(Active);
		if (Active)
		{
			Evaluate();
		}
	}
}

void UTickableConstraint::PostEditUndo()
{
	Super::PostEditUndo();

	// make sure we update ticking when undone
	const bool bActiveTick = IsValid(this);
	ConstraintTick.SetTickFunctionEnable(bActiveTick);
	ConstraintTick.bCanEverTick = bActiveTick;
}

#endif

/** 
 * UConstraintsManager
 **/

UConstraintsManager::UConstraintsManager()
{}

UConstraintsManager::~UConstraintsManager()
{

}

void UConstraintsManager::PostLoad()
{
	Super::PostLoad();
	for (TObjectPtr<UTickableConstraint> ConstPtr : Constraints)
	{
		if (ConstPtr)
		{
			ConstPtr->ConstraintTick.Constraint = ConstPtr;
		}
	}
}

void UConstraintsManager::OnActorDestroyed(AActor* InActor)
{
	if (USceneComponent* SceneComponent = InActor->GetRootComponent())
	{

		TArray< int32 > IndicesToRemove;
		for (int32 Index = 0; Index < Constraints.Num(); ++Index)
		{
			//if the constraint has a bound object(in sequencer) we don't remove the constraint, it could be a spawnable
			if (IsValid(Constraints[Index]) && Constraints[Index]->HasBoundObjects() == false && Constraints[Index]->ReferencesObject(SceneComponent))

			{
				IndicesToRemove.Add(Index);
			}
		}

		if (!IndicesToRemove.IsEmpty())
		{
			FConstraintsManagerController& Controller = FConstraintsManagerController::Get(InActor->GetWorld());
			for (int32 Index = IndicesToRemove.Num()-1; Index >= 0; --Index)
			{
				Controller.RemoveConstraint(Index,/*do not compensate*/ true);
			}
		}
	}
}

void UConstraintsManager::RegisterDelegates(UWorld* World)
{
	if (!OnActorDestroyedHandle.IsValid())
	{
		FOnActorDestroyed::FDelegate ActorDestroyedDelegate =
				FOnActorDestroyed::FDelegate::CreateUObject(this, &UConstraintsManager::OnActorDestroyed);
		OnActorDestroyedHandle = World->AddOnActorDestroyedHandler(ActorDestroyedDelegate);
	}
}

void UConstraintsManager::UnregisterDelegates(UWorld* World)
{
	if (World)
	{
		World->RemoveOnActorSpawnedHandler(OnActorDestroyedHandle);
	}
	OnActorDestroyedHandle.Reset();
}

void UConstraintsManager::Init(UWorld* World)
{
	if (World)
	{
		UnregisterDelegates(World);

		RegisterDelegates(World);
	}
}

UConstraintsManager* UConstraintsManager::Get(UWorld* InWorld)
{
	// look for ConstraintsActor and return its manager
	if (UConstraintsManager* Manager = Find(InWorld))
	{
		return Manager;
	}

	// create a new ConstraintsActor
	AConstraintsActor* ConstraintsActor = InWorld->SpawnActor<AConstraintsActor>();
#if WITH_EDITOR
	ConstraintsActor->SetActorLabel(TEXT("Constraints Manager"));
#endif // WITH_EDITOR
	ConstraintsActor->ConstraintsManager->SetFlags(RF_Transactional);
	return ConstraintsActor->ConstraintsManager;
}

UConstraintsManager* UConstraintsManager::Find(const UWorld* InWorld)
{
	// should we work with the persistent level?
	const ULevel* Level = InWorld->GetCurrentLevel();

	// look for first ConstraintsActor
	auto FindFirstConstraintsActor = [](const ULevel* Level)
	{
		const int32 Index = Level->Actors.IndexOfByPredicate([](const AActor* Actor)
		{
			return IsValid(Actor) && Actor->IsA<AConstraintsActor>();
		} );

		return Index != INDEX_NONE ? Cast<AConstraintsActor>(Level->Actors[Index]) : nullptr;
	};

	const AConstraintsActor* ConstraintsActor = FindFirstConstraintsActor(Level);
	return ConstraintsActor ? ConstraintsActor->ConstraintsManager : nullptr;
}

// we want InFunctionToTickBefore to tick first = InFunctionToTickBefore is a prerex of InFunctionToTickAfter
void UConstraintsManager::SetConstraintDependencies(
	FConstraintTickFunction* InFunctionToTickBefore,
	FConstraintTickFunction* InFunctionToTickAfter)
{
	// look for child tick function in in parent's prerequisites. 
	const TArray<FTickPrerequisite>& ParentPrerequisites = InFunctionToTickAfter->GetPrerequisites();
	const bool bIsChildAPrerexOfParent = ParentPrerequisites.ContainsByPredicate([InFunctionToTickBefore](const FTickPrerequisite& Prerex)
	{
		return Prerex.PrerequisiteTickFunction == InFunctionToTickBefore;
	});
	
	// child tick function is already a prerex -> parent already ticks after child
	if (bIsChildAPrerexOfParent)
	{
		return;
	}

	// look for parent tick function in in child's prerequisites
	const TArray<FTickPrerequisite>& ChildPrerequisites = InFunctionToTickBefore->GetPrerequisites();
	const bool bIsParentAPrerexOfChild = ChildPrerequisites.ContainsByPredicate([InFunctionToTickAfter](const FTickPrerequisite& Prerex)
	{
		return Prerex.PrerequisiteTickFunction == InFunctionToTickAfter;
	});
	
	// parent tick function is a prerex of the child tick function (child ticks after parent)
	// so remove it before setting new dependencies.
	if (bIsParentAPrerexOfChild)
	{
		InFunctionToTickBefore->RemovePrerequisite(this, *InFunctionToTickAfter);
	}
	
	// set dependency
	InFunctionToTickAfter->AddPrerequisite(this, *InFunctionToTickBefore);
}

void UConstraintsManager::Clear(UWorld* InWorld)
{
	UnregisterDelegates(InWorld);

	if (!Constraints.IsEmpty())
	{
		static constexpr bool bDoNoCompensate = false;
		static constexpr EConstraintsManagerNotifyType RemovalNotification =
						EConstraintsManagerNotifyType::ConstraintRemovedWithCompensation;
	
		const FConstraintsManagerController& Controller = FConstraintsManagerController::Get(InWorld);
		for (UTickableConstraint* Constraint : Constraints)
		{
			if ( IsValid(Constraint) )
			{
				// notify deletion
				Controller.Notify(RemovalNotification, Constraint);
				OnConstraintRemoved_BP.Broadcast(this, Constraint, bDoNoCompensate);

				// disable constraint
				Constraint->Modify();
				Constraint->ConstraintTick.UnRegisterTickFunction();
				Constraint->SetActive(false);
			}
		}

		Modify();
		Constraints.Empty();
	}
}

void UConstraintsManager::Dump() const
{
	UE_LOG(LogTemp, Error, TEXT("nb consts = %d"), Constraints.Num());
	for (const TObjectPtr<UTickableConstraint>& Constraint: Constraints)
	{
		if (IsValid(Constraint))
		{
			UE_LOG(LogTemp, Warning, TEXT("\t%s (target hash = %u)"),
				*Constraint->GetName(), Constraint->GetTargetHash());
		}
	}
}

FConstraintsManagerController& FConstraintsManagerController::Get(UWorld* InWorld)
{
	static FConstraintsManagerController Singleton;
	Singleton.World = InWorld;
	return Singleton;
}

UConstraintsManager* FConstraintsManagerController::GetManager() const
{
	if (!World)
	{
		return nullptr;
	}
	
	// look for ConstraintsActor and return its manager
	if (UConstraintsManager* Manager = FindManager())
	{
		return Manager;
	}

	// create a new ConstraintsActor
	AConstraintsActor* ConstraintsActor = World->SpawnActor<AConstraintsActor>();
#if WITH_EDITOR
	ConstraintsActor->SetActorLabel(TEXT("Constraints Manager"));
#endif // WITH_EDITOR
	ConstraintsActor->ConstraintsManager = NewObject<UConstraintsManager>(ConstraintsActor);
	ConstraintsActor->ConstraintsManager->Init(World);
	// ULevel* Level = World->GetCurrentLevel();
	// ConstraintsActor->ConstraintsManager->Level = Level;

	return ConstraintsActor->ConstraintsManager;
}

UConstraintsManager* FConstraintsManagerController::FindManager() const
{
	if (!World)
	{
		return nullptr;
	}
	
	// should we work with the persistent level?
	const ULevel* Level = World->GetCurrentLevel();

	// look for first ConstraintsActor
	auto FindFirstConstraintsActor = [](const ULevel* Level)
	{
		const int32 Index = Level->Actors.IndexOfByPredicate([](const AActor* Actor)
		{
			return IsValid(Actor) && Actor->IsA<AConstraintsActor>();
		} );

		return Index != INDEX_NONE ? Cast<AConstraintsActor>(Level->Actors[Index]) : nullptr;
	};

	const AConstraintsActor* ConstraintsActor = FindFirstConstraintsActor(Level);
	return ConstraintsActor ? ConstraintsActor->ConstraintsManager : nullptr;
}

void FConstraintsManagerController::DestroyManager() const
{
	if (!World)
	{
		return;
	}
	
	// should we work with the persistent level?
	const ULevel* Level = World->GetCurrentLevel();

	// note there should be only one...
	TArray<AActor*> ConstraintsActorsToRemove;
	Algo::CopyIf(Level->Actors, ConstraintsActorsToRemove, [](const AActor* Actor)
	{
		return IsValid(Actor) && Actor->IsA<AConstraintsActor>();
	} );

	for (AActor* ConstraintsActor: ConstraintsActorsToRemove)
	{
		World->DestroyActor(ConstraintsActor, true);
	}
}

bool FConstraintsManagerController::AddConstraint(UTickableConstraint* InConstraint) const
{
	if (!InConstraint)
	{
		return false;
	}
	
	UConstraintsManager* Manager = GetManager(); //this will allocate if doesn't exist
	if (!Manager)
	{
		return false;
	}

	Manager->Modify();
	//it's possible this constraint was actually in another ConstraintActor::ConstraintManager so we need to move it over via Rename.
	//and clear out it ticks function since that may have been registered
	UConstraintsManager* Outer = InConstraint->GetTypedOuter<UConstraintsManager>();
	if (Outer && Outer != Manager)
	{
		InConstraint->ConstraintTick.UnRegisterTickFunction();
		InConstraint->Rename(nullptr, Manager, REN_ForceNoResetLoaders | REN_DoNotDirty | REN_DontCreateRedirectors | REN_NonTransactional);
	}
	Manager->Constraints.Emplace(InConstraint);

	InConstraint->ConstraintTick.RegisterFunction(InConstraint->GetFunction());
	InConstraint->ConstraintTick.RegisterTickFunction(World->GetCurrentLevel());
	InConstraint->ConstraintTick.Constraint = InConstraint;

	// notify
	Notify(EConstraintsManagerNotifyType::ConstraintAdded, InConstraint);
	Manager->OnConstraintAdded_BP.Broadcast(Manager, InConstraint);

	return true;
}

UTickableConstraint* FConstraintsManagerController::AddConstraintFromCopy(UTickableConstraint* CopyOfConstraint) const
{
	if (!CopyOfConstraint)
	{
		return nullptr;
	}
	UConstraintsManager* Manager = GetManager(); //this will allocate if doesn't exist
	if (!Manager)
	{
		return nullptr;
	}

	UTickableConstraint* OurCopy = CopyOfConstraint->Duplicate(Manager);
	if (AddConstraint(OurCopy))
	{
		return OurCopy;
	}
	return nullptr;
}


int32 FConstraintsManagerController::GetConstraintIndex(const FName& InConstraintName) const
{
	const UConstraintsManager* Manager = FindManager();
	if (!Manager)
	{
		return INDEX_NONE;
	}
	
	return Manager->Constraints.IndexOfByPredicate([InConstraintName](const TObjectPtr<UTickableConstraint>& Constraint)
	{
		return 	(Constraint && Constraint->GetFName() == InConstraintName);
	} );
}
	
bool  FConstraintsManagerController::RemoveConstraint(const FName& InConstraintName, bool bDoNotCompensate) const
{
	const int32 Index = GetConstraintIndex(InConstraintName);
	if (Index == INDEX_NONE)
	{
		return false;
	}
	
	return RemoveConstraint(Index, bDoNotCompensate);
}

bool FConstraintsManagerController::RemoveConstraint(const int32 InConstraintIndex,bool bDoNotCompensate) const
{
	UConstraintsManager* Manager = FindManager();
	if (!Manager)
	{
		return false;
	}
	
	if (!Manager->Constraints.IsValidIndex(InConstraintIndex))
	{
		return false;
	}

	const FName ConstraintName = Manager->Constraints[InConstraintIndex]->GetFName();
	UTickableConstraint* Constraint = Manager->Constraints[InConstraintIndex];
	
	// notify deletion
	auto GetRemoveNotifyType = [bDoNotCompensate]()
	{
		if(bDoNotCompensate)
		{
			return EConstraintsManagerNotifyType::ConstraintRemoved;	
		}
		return EConstraintsManagerNotifyType::ConstraintRemovedWithCompensation;
	};
	Notify(GetRemoveNotifyType(), Constraint);	
	Manager->OnConstraintRemoved_BP.Broadcast(Manager, Constraint, bDoNotCompensate);
	
	Manager->Constraints[InConstraintIndex]->Modify();
	Manager->Modify();

	Manager->Constraints[InConstraintIndex]->SetActive(false);
	Manager->Constraints.RemoveAt(InConstraintIndex);

	// destroy constraints actor if no constraints left
	if (Manager->Constraints.IsEmpty())
	{
		// todo will re-evaluate this based upon spawnable work. DestroyManager();
	}
	return true;
}

UTickableConstraint* FConstraintsManagerController::GetConstraint(const FName& InConstraintName) const
{
	const int32 Index = GetConstraintIndex(InConstraintName);
	if (Index == INDEX_NONE)
	{
		return nullptr;	
	}
	
	return GetConstraint(Index);
}

UTickableConstraint* FConstraintsManagerController::GetConstraint(const int32 InConstraintIndex) const
{
	UConstraintsManager* Manager = FindManager();
	if (!Manager)
	{
		return nullptr;	
	}
	
	if (!Manager->Constraints.IsValidIndex(InConstraintIndex))
	{
		return nullptr;	
	}

	return Manager->Constraints[InConstraintIndex];
}

TArray< TObjectPtr<UTickableConstraint> > FConstraintsManagerController::GetParentConstraints(
	const uint32 InTargetHash,
	const bool bSorted) const
{
		static const TArray< TObjectPtr<UTickableConstraint> > DummyArray;

	const UConstraintsManager* Manager = FindManager();
	if (!Manager)
	{
		return DummyArray;
	}
	
	if (InTargetHash == 0)
	{
		return DummyArray;
	}

	using ConstraintPtr = TObjectPtr<UTickableConstraint>;
	TArray< TObjectPtr<UTickableConstraint> > FilteredConstraints =
	Manager->Constraints.FilterByPredicate( [InTargetHash](const ConstraintPtr& Constraint)
	{
		return IsValid(Constraint) && Constraint->GetTargetHash() == InTargetHash;
	} );
	
	if (bSorted)
	{
		// LHS ticks before RHS = LHS is a prerex of RHS 
		auto TicksBefore = [](const UTickableConstraint& LHS, const UTickableConstraint& RHS)
		{
			const TArray<FTickPrerequisite>& RHSPrerex = RHS.ConstraintTick.GetPrerequisites();
			const FConstraintTickFunction& LHSTickFunction = LHS.ConstraintTick;
			const bool bIsLHSAPrerexOfRHS = RHSPrerex.ContainsByPredicate([&LHSTickFunction](const FTickPrerequisite& Prerex)
			{
				return Prerex.PrerequisiteTickFunction == &LHSTickFunction;
			});
			return bIsLHSAPrerexOfRHS;
		};
		
		Algo::StableSort(FilteredConstraints, [TicksBefore](const ConstraintPtr& LHS, const ConstraintPtr& RHS)
		{
			return TicksBefore(*LHS, *RHS);
		});
	}
	
	return FilteredConstraints;
}

void FConstraintsManagerController::SetConstraintsDependencies(
	const FName& InNameToTickBefore,
	const FName& InNameToTickAfter) const
{
	UConstraintsManager* Manager = FindManager();
	if (!Manager)
	{
		return;
	}

	const int32 IndexBefore = GetConstraintIndex(InNameToTickBefore);
	const int32 IndexAfter = GetConstraintIndex(InNameToTickAfter);
	if (IndexBefore == INDEX_NONE || IndexAfter == INDEX_NONE || IndexAfter == IndexBefore)
	{
		return;
	}

	FConstraintTickFunction& FunctionToTickBefore = Manager->Constraints[IndexBefore]->ConstraintTick;
	FConstraintTickFunction& FunctionToTickAfter = Manager->Constraints[IndexAfter]->ConstraintTick;

	Manager->SetConstraintDependencies( &FunctionToTickBefore, &FunctionToTickAfter);
}

const TArray< TObjectPtr<UTickableConstraint> >& FConstraintsManagerController::GetConstraintsArray() const
{
	UConstraintsManager* Manager = FindManager();
	if (!Manager)
	{
		static const TArray< TObjectPtr<UTickableConstraint> > Empty;
		return Empty;
	}

	return Manager->Constraints;
}

TArray< TObjectPtr<UTickableConstraint> > FConstraintsManagerController::GetAllConstraints(const bool bSorted) const
{
	UConstraintsManager* Manager = FindManager();
	if (!Manager)
	{
		static const TArray< TObjectPtr<UTickableConstraint> > Empty;
		return Empty;
	}

	if (!bSorted)
	{
		return Manager->Constraints;
	}

	using ConstraintPtr = TObjectPtr<UTickableConstraint>;
	TArray< TObjectPtr<UTickableConstraint> > SortedConstraints(Manager->Constraints);
	// LHS ticks before RHS = LHS is a prerex of RHS 
	auto TicksBefore = [](const UTickableConstraint& LHS, const UTickableConstraint& RHS)
	{
		const TArray<FTickPrerequisite>& RHSPrerex = RHS.ConstraintTick.GetPrerequisites();
		const FConstraintTickFunction& LHSTickFunction = LHS.ConstraintTick;
		const bool bIsLHSAPrerexOfRHS = RHSPrerex.ContainsByPredicate([&LHSTickFunction](const FTickPrerequisite& Prerex)
		{
			return Prerex.PrerequisiteTickFunction == &LHSTickFunction;
		});
		return bIsLHSAPrerexOfRHS;
	};
	
	Algo::StableSort(SortedConstraints, [TicksBefore](const ConstraintPtr& LHS, const ConstraintPtr& RHS)
	{
		return TicksBefore(*LHS, *RHS);
	});
	
	return SortedConstraints;
}

void FConstraintsManagerController::EvaluateAllConstraints() const
{
	TArray< TObjectPtr<UTickableConstraint>>Constraints = GetAllConstraints(true);
	for (const UTickableConstraint* InConstraint : Constraints)
	{
		InConstraint->Evaluate(true);
	}
}

void FConstraintsManagerController::Notify(EConstraintsManagerNotifyType InNotifyType, UObject* InObject) const
{
	switch (InNotifyType)
	{
		case EConstraintsManagerNotifyType::ConstraintAdded: 
		case EConstraintsManagerNotifyType::ConstraintRemoved:
		case EConstraintsManagerNotifyType::ConstraintRemovedWithCompensation:
			checkSlow(Cast<UTickableConstraint>(InObject) != nullptr);
			break;

		case EConstraintsManagerNotifyType::ManagerUpdated:
			checkSlow(Cast<UConstraintsManager>(InObject) != nullptr);
			break;
		default:
			checkfSlow(false, TEXT("Unchecked EConstraintsManagerNotifyType!"));
			break;
	}

	NotifyDelegate.Broadcast(InNotifyType, InObject);
}