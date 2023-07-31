// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ConstraintsManager.h"

#include "Algo/StableSort.h"

template< typename TConstraint >
TConstraint* FConstraintsManagerController::AllocateConstraintT(const FName& InBaseName) const
{
	UConstraintsManager* Manager = GetManager();
	if (!Manager)
	{
		return nullptr;
	}
	// unique name (we may want to use another approach here to manage uniqueness)
	const FName Name = MakeUniqueObjectName(Manager, TConstraint::StaticClass(), InBaseName);

	TConstraint* NewConstraint = NewObject<TConstraint>(Manager, Name, RF_Transactional);
	NewConstraint->Modify();
	return NewConstraint;
}

template <typename Predicate>
TArray< TObjectPtr<UTickableConstraint> > FConstraintsManagerController::GetConstraintsByPredicate(
	Predicate Pred, const bool bSorted) const
{
	using ConstraintPtr = TObjectPtr<UTickableConstraint>;
	
	UConstraintsManager* Manager = FindManager();
	if (!Manager)
	{
		static const TArray< TObjectPtr<UTickableConstraint> > Empty;
		return Empty;
	}

	TArray< ConstraintPtr > Constraints = Manager->Constraints.FilterByPredicate(Pred);
	if (!bSorted)
	{
		return Constraints;
	}

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
	
	Algo::StableSort(Constraints, [TicksBefore](const ConstraintPtr& LHS, const ConstraintPtr& RHS)
	{
		return TicksBefore(*LHS, *RHS);
	});
	
	return Constraints;
}