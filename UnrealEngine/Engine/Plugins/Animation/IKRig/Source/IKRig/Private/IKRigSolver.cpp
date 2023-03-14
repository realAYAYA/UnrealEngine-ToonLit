// Copyright Epic Games, Inc. All Rights Reserved.

#include "IKRigSolver.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(IKRigSolver)

#if WITH_EDITORONLY_DATA

void UIKRigSolver::PostLoad()
{
	Super::PostLoad();
	SetFlags(RF_Transactional); // patch old solvers to enable undo/redo
}

void UIKRigSolver::PostTransacted(const FTransactionObjectEvent& TransactionEvent)
{
	Super::PostTransacted(TransactionEvent);
	IKRigSolverModified.Broadcast(this);
}

#endif


