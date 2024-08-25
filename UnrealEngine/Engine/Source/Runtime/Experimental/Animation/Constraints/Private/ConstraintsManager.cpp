// Copyright Epic Games, Inc. All Rights Reserved.

#include "ConstraintsManager.inl"
#include "ConstraintsActor.h"
#include "ConstraintSubsystem.h"
#include "Algo/Copy.h"
#include "Algo/StableSort.h"

#include "Engine/World.h"
#include "Engine/Level.h"
#include "Engine/Engine.h"

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

FConstraintTickFunction::FConstraintTickFunction(const FConstraintTickFunction& In) : FConstraintTickFunction()
{
	if (In.ConstraintFunctions.Num() > 0) //should only be 1
	{
		ConstraintFunctions.Add(In.ConstraintFunctions[0]);
	}
	Constraint = In.Constraint;
}

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
	if (ConstraintFunctions.IsEmpty())
	{
		ConstraintFunctions.Add(InConstraint);
	}
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

FConstraintTickFunction& UTickableConstraint::GetTickFunction(UWorld* InWorld) 
{
	FConstraintTickFunction& ConstraintTick = ConstraintTicks.FindOrAdd(InWorld->GetCurrentLevel());
	return ConstraintTick;
}

const FConstraintTickFunction& UTickableConstraint::GetTickFunction(UWorld* InWorld) const
{
	const FConstraintTickFunction& ConstraintTick = ConstraintTicks.FindOrAdd(InWorld->GetCurrentLevel());
	return ConstraintTick;
}

void UTickableConstraint::SetActive(const bool bIsActive)
{
	Active = bIsActive;
	for (TPair<TWeakObjectPtr<ULevel>, FConstraintTickFunction>& Pair : ConstraintTicks)
	{
		if (Pair.Key.IsValid())
		{
			Pair.Value.SetTickFunctionEnable(bIsActive);
		}
	}
}

bool UTickableConstraint::IsFullyActive() const
{
	return Active;
}

void UTickableConstraint::Evaluate(bool bTickHandlesAlso) const
{
	for (const TPair<TWeakObjectPtr<ULevel>, FConstraintTickFunction>& Pair : ConstraintTicks)
	{
		if (Pair.Key.IsValid())
		{
			Pair.Value.EvaluateFunctions();
		}
	}
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

void UTickableConstraint::PostDuplicate(bool bDuplicateForPIE)
{
	Super::PostDuplicate(bDuplicateForPIE);
	if (bDuplicateForPIE == false) //doing actually copy then give unique id
	{
		ConstraintID = FGuid::NewGuid();
	}
}
void UTickableConstraint::PostLoad()
{
	Super::PostLoad();
	if (ConstraintID.IsValid() == false)
	{
		ConstraintID = FGuid::NewGuid();
	}
}

void UTickableConstraint::PostInitProperties()
{
	Super::PostInitProperties();
	if (HasAnyFlags(RF_ClassDefaultObject | RF_NeedLoad) == false)
	{
		ConstraintID = FGuid::NewGuid();
	}
}

void UTickableConstraint::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = PropertyChangedEvent.GetPropertyName();
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UTickableConstraint, Active))
	{
		for (TPair<TWeakObjectPtr<ULevel>, FConstraintTickFunction>& Pair : ConstraintTicks)
		{
			if (Pair.Key.IsValid())
			{
				Pair.Value.SetTickFunctionEnable(Active);
				if (Active)
				{
					Pair.Value.EvaluateFunctions();
				}
			}	
		}
	}
}

void UTickableConstraint::PostEditUndo()
{
	Super::PostEditUndo();

	// make sure we update ticking when undone
	const bool bActiveTick = ::IsValid(this);
	for (TPair<TWeakObjectPtr<ULevel>, FConstraintTickFunction>& Pair : ConstraintTicks)
	{
		if (Pair.Key.IsValid())
		{
			Pair.Value.SetTickFunctionEnable(bActiveTick);
			Pair.Value.bCanEverTick = bActiveTick;
		}
	}
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
	for (TObjectPtr<UTickableConstraint>& ConstPtr : Constraints)
	{
		if (ConstPtr)
		{
			ConstPtr->InitConstraint(GetWorld());
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
			if (Constraints[Index] && IsValid(Constraints[Index].Get()) && Constraints[Index]->HasBoundObjects() == false && Constraints[Index]->ReferencesObject(SceneComponent))
			{
				IndicesToRemove.Add(Index);
			}
		}

		if (!IndicesToRemove.IsEmpty())
		{
			FConstraintsManagerController& Controller = FConstraintsManagerController::Get(InActor->GetWorld());
			if (FConstraintsManagerController::bDoNotRemoveConstraint == false)
			{
				for (int32 Index = IndicesToRemove.Num() - 1; Index >= 0; --Index)
				{
					Controller.RemoveConstraint(Index,/*do not compensate*/ true);
				}
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
	if (!IsValid(InWorld))
	{
		return nullptr;
	}
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
	return ConstraintsActor->ConstraintsManager;
}

UConstraintsManager* UConstraintsManager::Find(const UWorld* InWorld)
{
	if (!IsValid(InWorld))
	{
		return nullptr;
	}
	
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
void UConstraintsManager::Clear(UWorld* InWorld)
{
	UnregisterDelegates(InWorld);
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


/**
 * FConstraintsManagerController
 **/


bool FConstraintsManagerController::bDoNotRemoveConstraint = false;

FConstraintsManagerController& FConstraintsManagerController::Get(UWorld* InWorld)
{
	static FConstraintsManagerController Singleton;
	Singleton.World = InWorld;
	return Singleton;
}


UConstraintsManager* FConstraintsManagerController::GetManager() 
{
	UConstraintsManager* Manager = FindManager();
	if (Manager)
	{
		return Manager;
	}

	// create a new ConstraintsActor
	AConstraintsActor* ConstraintsActor = World->SpawnActor<AConstraintsActor>();
#if WITH_EDITOR
	ConstraintsActor->SetActorLabel(TEXT("Constraints Manager"));
#endif // WITH_EDITOR
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

void FConstraintsManagerController::StaticConstraintCreated(UWorld* InWorld, UTickableConstraint* InConstraint)
{
	if (UConstraintsManager* Manager = GetManager()) //created If needed
	{
		if (Manager->Constraints.Contains(InConstraint) == false)
		{
			Manager->Modify();
			Manager->Constraints.Add(InConstraint);
			InConstraint->Rename(nullptr, Manager, REN_ForceNoResetLoaders | REN_DontCreateRedirectors);
			Manager->OnConstraintAdded_BP.Broadcast(Manager, InConstraint);
		}
	}
}

bool FConstraintsManagerController::AddConstraint(UTickableConstraint* InConstraint) const
{
	if (!InConstraint)
	{
		return false;
	}
	UConstraintSubsystem* Subsystem = UConstraintSubsystem::Get();
	if (!Subsystem)
	{
		return false;
	}
	if (Subsystem->HasConstraint(World, InConstraint) == true)
	{
		return false;
	}
	InConstraint->InitConstraint(World);

	Subsystem->AddConstraint(World,InConstraint);

	// build dependencies
	InConstraint->AddedToWorld(World);

	// notify
	Notify(EConstraintsManagerNotifyType::ConstraintAdded, InConstraint);

	return true;
}

UTickableConstraint* FConstraintsManagerController::AddConstraintFromCopy(UTickableConstraint* CopyOfConstraint) const
{
	if (!CopyOfConstraint)
	{
		return nullptr;
	}
	UConstraintSubsystem* Subsystem = UConstraintSubsystem::Get();
	if (!Subsystem)
	{
		return nullptr;
	}
	int32 Index = GetConstraintIndex(CopyOfConstraint->ConstraintID);
	if (Index != INDEX_NONE)
	{
		const TArray<TWeakObjectPtr<UTickableConstraint>>& Constraints = Subsystem->GetConstraintsArray(World);
		return Constraints[Index].Get();
	}

	UObject* Outer = CopyOfConstraint->GetOuter();
	UTickableConstraint* OurCopy = CopyOfConstraint->Duplicate(Outer);
	OurCopy->ConstraintID = CopyOfConstraint->ConstraintID;
	OurCopy->SetActive(false); //always set false
	if (AddConstraint(OurCopy))
	{
		return OurCopy;
	}
	return nullptr;
}

int32 FConstraintsManagerController::GetConstraintIndex(const FGuid& InGuid) const
{
	const UConstraintSubsystem* Subsystem = UConstraintSubsystem::Get();
	if (!Subsystem)
	{
		return INDEX_NONE;
	}

	const TArray<TWeakObjectPtr<UTickableConstraint>> Constraints = Subsystem->GetConstraints(World);
	return Constraints.IndexOfByPredicate([InGuid](const TWeakObjectPtr<UTickableConstraint>& Constraint)
		{
			return 	(Constraint.IsValid() && Constraint->ConstraintID == InGuid);
		});
}

int32 FConstraintsManagerController::GetConstraintIndex(const FName& InConstraintName) const
{
	const UConstraintSubsystem* Subsystem = UConstraintSubsystem::Get();
	if (!Subsystem)
	{
		return INDEX_NONE;
	}

	const TArray<TWeakObjectPtr<UTickableConstraint>> Constraints = Subsystem->GetConstraints(World);
	return Constraints.IndexOfByPredicate([InConstraintName](const TWeakObjectPtr<UTickableConstraint>& Constraint)
	{
		return 	(Constraint.IsValid() && Constraint->GetFName() == InConstraintName);
	} );
}

	
bool FConstraintsManagerController::RemoveConstraint(UTickableConstraint* InConstraint, bool bDoNotCompensate) 
{
	if (FConstraintsManagerController::bDoNotRemoveConstraint == true)
	{
		return false;
	}
	UConstraintSubsystem* Subsystem = UConstraintSubsystem::Get();
	if (!Subsystem)
	{
		return false;
	}
	TArray<TWeakObjectPtr<UTickableConstraint>> Constraints = Subsystem->GetConstraints(World);

	int32 Index = Constraints.IndexOfByPredicate([InConstraint](const TWeakObjectPtr<UTickableConstraint>& Constraint)
		{
			return 	(Constraint.IsValid() && Constraint.Get() == InConstraint);
		});
	if (Index == INDEX_NONE)
	{
		return false;
	}
	return RemoveConstraint(Index, bDoNotCompensate);
}

bool FConstraintsManagerController::RemoveConstraint(const int32 InConstraintIndex,bool bDoNotCompensate) 
{
	UConstraintSubsystem* Subsystem = UConstraintSubsystem::Get();
	if (!Subsystem)
	{
		return false;
	}
	TArray<TWeakObjectPtr<UTickableConstraint>> Constraints = Subsystem->GetConstraints(World);

	const FName ConstraintName = Constraints[InConstraintIndex]->GetFName();
	UTickableConstraint* Constraint = Constraints[InConstraintIndex].Get();
	
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
	Subsystem->Modify();
	Subsystem->RemoveConstraint(World, Constraint, bDoNotCompensate);

	if (UConstraintsManager* Manager = FindManager())
	{
		if (Manager->Constraints.Contains(Constraint) == true)
		{
			Manager->Modify();
			Manager->Constraints.Remove(Constraint);
			Manager->OnConstraintRemoved_BP.Broadcast(Manager, Constraint, bDoNotCompensate);
			// destroy constraints actor if no constraints left
			if (Manager->Constraints.IsEmpty())
			{
				DestroyManager();
			}
		}
	}

	return true;
	
}

UTickableConstraint* FConstraintsManagerController::GetConstraint(const FGuid& InGuid) const
{
	const int32 Index = GetConstraintIndex(InGuid);
	if (Index == INDEX_NONE)
	{
		return nullptr;
	}

	return GetConstraint(Index);
}

UTickableConstraint* FConstraintsManagerController::GetConstraint(const int32 InConstraintIndex) const
{
	UConstraintSubsystem* Subsystem = UConstraintSubsystem::Get();
	if (!Subsystem)
	{
		return nullptr;
	}
	TArray<TWeakObjectPtr<UTickableConstraint>> Constraints = Subsystem->GetConstraints(World);

	if (!Constraints.IsValidIndex(InConstraintIndex))
	{
		return nullptr;	
	}

	return Constraints[InConstraintIndex].Get();
}

TArray< TWeakObjectPtr<UTickableConstraint> > FConstraintsManagerController::GetParentConstraints(
	const uint32 InTargetHash,
	const bool bSorted) const
{
	static const TArray< TWeakObjectPtr<UTickableConstraint> > DummyArray;
	
	if (InTargetHash == 0)
	{
		return DummyArray;
	}

	UConstraintSubsystem* Subsystem = UConstraintSubsystem::Get();
	if (!Subsystem)
	{
		return DummyArray;
	}
	TArray<TWeakObjectPtr<UTickableConstraint>> Constraints = Subsystem->GetConstraints(World);

	using ConstraintPtr = TWeakObjectPtr<UTickableConstraint>;

	TArray< TWeakObjectPtr<UTickableConstraint> > FilteredConstraints =
	Constraints.FilterByPredicate( [InTargetHash](const ConstraintPtr& Constraint)
	{
		return Constraint.IsValid() && IsValid(Constraint.Get()) && Constraint->GetTargetHash() == InTargetHash;
	} );
	
	if (bSorted)
	{
		// LHS ticks before RHS = LHS is a prerex of RHS 
		auto TicksBefore = [this](const UTickableConstraint& LHS, const UTickableConstraint& RHS)
		{
			const TArray<FTickPrerequisite>& RHSPrerex = RHS.GetTickFunction(World).GetPrerequisites();
			const FConstraintTickFunction& LHSTickFunction = LHS.GetTickFunction(World);
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
	UConstraintSubsystem* Subsystem = UConstraintSubsystem::Get();
	if (!Subsystem)
	{
		return;
	}
	const TArray<TWeakObjectPtr<UTickableConstraint>> Constraints = Subsystem->GetConstraints(World);

	const int32 IndexBefore = GetConstraintIndex(InNameToTickBefore);
	const int32 IndexAfter = GetConstraintIndex(InNameToTickAfter);
	if (IndexBefore == INDEX_NONE || IndexAfter == INDEX_NONE || IndexAfter == IndexBefore)
	{
		return;
	}

	FConstraintTickFunction& FunctionToTickBefore = Constraints[IndexBefore]->GetTickFunction(World);
	FConstraintTickFunction& FunctionToTickAfter = Constraints[IndexAfter]->GetTickFunction(World);

	Subsystem->SetConstraintDependencies(&FunctionToTickBefore, &FunctionToTickAfter);

	InvalidateEvaluationGraph();
}

void FConstraintsManagerController::SetConstraintsDependencies(const struct FGuid& InGuidToTickBefore, const struct FGuid& InGuidToTickAfter) const
{
	if (!InGuidToTickBefore.IsValid() || !InGuidToTickAfter.IsValid() || InGuidToTickBefore == InGuidToTickAfter)
	{
		return;
	}

	UConstraintSubsystem* Subsystem = UConstraintSubsystem::Get();
	if (!Subsystem)
	{
		return;
	}

	const TArray<TWeakObjectPtr<UTickableConstraint>>& Constraints = Subsystem->GetConstraintsArray(World);
	const int32 IndexBefore = Constraints.IndexOfByPredicate([InGuidToTickBefore](const TWeakObjectPtr<UTickableConstraint>& Constraint)
	{
		return Constraint.IsValid() && Constraint->ConstraintID == InGuidToTickBefore;
	});
	
	const int32 IndexAfter = Constraints.IndexOfByPredicate([InGuidToTickAfter](const TWeakObjectPtr<UTickableConstraint>& Constraint)
	{
		return Constraint.IsValid() && Constraint->ConstraintID == InGuidToTickAfter;
	});
	
	if (IndexBefore == INDEX_NONE || IndexAfter == INDEX_NONE || IndexAfter == IndexBefore)
	{
		return;
	}
	
	FConstraintTickFunction& FunctionToTickBefore = Constraints[IndexBefore]->GetTickFunction(World);
	FConstraintTickFunction& FunctionToTickAfter = Constraints[IndexAfter]->GetTickFunction(World);

	Subsystem->SetConstraintDependencies(&FunctionToTickBefore, &FunctionToTickAfter);

	InvalidateEvaluationGraph();
}

const TArray< TWeakObjectPtr<UTickableConstraint> >& FConstraintsManagerController::GetConstraintsArray() const
{
	UConstraintSubsystem* Subsystem = UConstraintSubsystem::Get();
	if (!Subsystem)
	{
		static const TArray< TWeakObjectPtr<UTickableConstraint> > Empty;
		return Empty;
	}
	const TArray<TWeakObjectPtr<UTickableConstraint>>& Constraints = Subsystem->GetConstraintsArray(World);
	return Constraints;
}

bool FConstraintsManagerController::RemoveAllConstraints(bool bDoNotCompensate) 
{
	if (UConstraintSubsystem* Subsystem = UConstraintSubsystem::Get())
	{
		Subsystem->Modify();
		TArray<TWeakObjectPtr<UTickableConstraint>> Constraints = Subsystem->GetConstraints(World);
		for (TWeakObjectPtr<UTickableConstraint>& Constraint : Constraints)
		{
			RemoveConstraint(Constraint.Get(), bDoNotCompensate);
		}
	}
	return false;
}

static void SortConstraints(UWorld* InWorld, TArray< TWeakObjectPtr<UTickableConstraint> >& InOutSortedConstraints)
{
	using ConstraintPtr = TWeakObjectPtr<UTickableConstraint>;
	// LHS ticks before RHS = LHS is a prerex of RHS 
	auto TicksBefore = [InWorld](const UTickableConstraint& LHS, const UTickableConstraint& RHS)
	{
		if (RHS.IsValid() == false || LHS.IsValid() == false)
		{
			return false;
		}
		const TArray<FTickPrerequisite>& RHSPrerex = RHS.GetTickFunction(InWorld).GetPrerequisites();
		const FConstraintTickFunction& LHSTickFunction = LHS.GetTickFunction(InWorld);
		const bool bIsLHSAPrerexOfRHS = RHSPrerex.ContainsByPredicate([&LHSTickFunction](const FTickPrerequisite& Prerex)
			{
				return Prerex.PrerequisiteTickFunction == &LHSTickFunction;
			});
		return bIsLHSAPrerexOfRHS;
	};

	Algo::Sort(InOutSortedConstraints, [TicksBefore](const ConstraintPtr& LHS, const ConstraintPtr& RHS)
	{
		return TicksBefore(*LHS, *RHS);
	});
}

static void SortConstraints(UWorld* InWorld, TArray< TObjectPtr<UTickableConstraint> >& InOutSortedConstraints)
{
	using ConstraintPtr = TObjectPtr<UTickableConstraint>;
	// LHS ticks before RHS = LHS is a prerex of RHS 
	auto TicksBefore = [InWorld](const UTickableConstraint& LHS, const UTickableConstraint& RHS)
	{
		const TArray<FTickPrerequisite>& RHSPrerex = RHS.GetTickFunction(InWorld).GetPrerequisites();
		const FConstraintTickFunction& LHSTickFunction = LHS.GetTickFunction(InWorld);
		const bool bIsLHSAPrerexOfRHS = RHSPrerex.ContainsByPredicate([&LHSTickFunction](const FTickPrerequisite& Prerex)
			{
				return Prerex.PrerequisiteTickFunction == &LHSTickFunction;
			});
		return bIsLHSAPrerexOfRHS;
	};

	Algo::Sort(InOutSortedConstraints, [TicksBefore](const ConstraintPtr& LHS, const ConstraintPtr& RHS)
	{
		return TicksBefore(*LHS, *RHS);
	});
}

TArray< TObjectPtr<UTickableConstraint> > FConstraintsManagerController::GetStaticConstraints(const bool bSorted) const
{
	UConstraintsManager* Manager = FindManager();
	if (!Manager)
	{
		static const TArray< TObjectPtr<UTickableConstraint> > Empty;
		return Empty;
	}
	TArray<TObjectPtr<UTickableConstraint>> Constraints = Manager->Constraints;

	// Remove stale constraints. Stale constraints may be caused to due unexpected unloading
	// of constributing objects, such as a level sequence.
	Manager->Constraints.RemoveAll([](const TObjectPtr<UTickableConstraint>& ExistingConstraint) -> bool
	{
		return !ExistingConstraint;
	});

	if (!bSorted)
	{
		return Constraints;
	}

	TArray< TObjectPtr<UTickableConstraint> > SortedConstraints(Constraints);
	SortConstraints(World,SortedConstraints);

	return SortedConstraints;
}

TArray< TWeakObjectPtr<UTickableConstraint> > FConstraintsManagerController::GetAllConstraints(const bool bSorted) const
{
	UConstraintSubsystem* Subsystem = UConstraintSubsystem::Get();
	if (!Subsystem)
	{
		static const TArray< TWeakObjectPtr<UTickableConstraint> > Empty;
		return Empty;
	}
	TArray<TWeakObjectPtr<UTickableConstraint>> Constraints = Subsystem->GetConstraints(World);
	
	if (!bSorted)
	{
		return Constraints;
	}

	TArray< TWeakObjectPtr<UTickableConstraint> > SortedConstraints(Constraints);
	// Remove stale constraints
	Constraints.RemoveAll([](const TWeakObjectPtr<UTickableConstraint>& ExistingConstraint) -> bool
	{
		return ExistingConstraint.IsStale();
	});
	SortConstraints(World,SortedConstraints);
	
	return SortedConstraints;
}

void FConstraintsManagerController::EvaluateAllConstraints() const
{
	using ConstraintPtr = TWeakObjectPtr<UTickableConstraint>;

	static constexpr bool bSorted = true, bTickHandles = true;
	const TArray<ConstraintPtr> Constraints = GetAllConstraints(bSorted);
	for (const ConstraintPtr& Constraint : Constraints)
	{
		if (Constraint.IsValid())
		{
			Constraint->Evaluate(bTickHandles);
		}
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
			checkSlow(Cast<UConstraintSubsystem>(InObject) != nullptr);
			break;
		default:
			checkfSlow(false, TEXT("Unchecked EConstraintsManagerNotifyType!"));
			break;
	}

	NotifyDelegate.Broadcast(InNotifyType, InObject);
}

void FConstraintsManagerController::MarkConstraintForEvaluation(UTickableConstraint* InConstraint) const
{
	UConstraintSubsystem* SubSystem = UConstraintSubsystem::Get();
	if (!SubSystem)
	{
		return;
	}
	SubSystem->GetEvaluationGraph(World).MarkForEvaluation(InConstraint);
}

void FConstraintsManagerController::InvalidateEvaluationGraph() const
{
	UConstraintSubsystem* SubSystem = UConstraintSubsystem::Get();
	if (!SubSystem)
	{
		return;
	}
	SubSystem->GetEvaluationGraph(World).InvalidateData();
}

void FConstraintsManagerController::FlushEvaluationGraph() const
{
	if (!FConstraintsEvaluationGraph::UseEvaluationGraph())
	{
		return;
	}
	UConstraintSubsystem* SubSystem = UConstraintSubsystem::Get();
	if (!SubSystem)
	{
		return;
	}

	FConstraintsEvaluationGraph& EvaluationGraph = SubSystem->GetEvaluationGraph(World);
	if (EvaluationGraph.IsPendingEvaluation())
	{
		EvaluationGraph.FlushPendingEvaluations();
	}
}

bool FConstraintsManagerController::DoesExistInAnyWorld(UTickableConstraint* InConstraint)
{
	bool bFound = false;

	if (InConstraint)
	{
		if (UConstraintSubsystem * Subsystem = UConstraintSubsystem::Get())
		{
			UWorld* CurrentWorld = World; //save current World
			for (const FWorldContext& Context : GEngine->GetWorldContexts())
			{
				World = Context.World();
				if (Subsystem->GetConstraintsArray(World).Find(InConstraint) != INDEX_NONE)
				{
					bFound = true;
					break;
				}
			}
			World = CurrentWorld; //restore current World
		}
	}
	return bFound;
}