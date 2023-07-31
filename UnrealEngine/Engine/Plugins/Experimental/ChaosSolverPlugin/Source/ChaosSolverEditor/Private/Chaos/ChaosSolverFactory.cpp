// Copyright Epic Games, Inc. All Rights Reserved.

#include "Chaos/ChaosSolverFactory.h"

#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "Engine/Selection.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ChaosSolverFactory)

#define LOCTEXT_NAMESPACE "ChaosSolver"

/////////////////////////////////////////////////////
// ChaosSolverFactory

UChaosSolverFactory::UChaosSolverFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UChaosSolver::StaticClass();
}

UChaosSolver* UChaosSolverFactory::StaticFactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	return static_cast<UChaosSolver*>(NewObject<UChaosSolver>(InParent, Class, Name, Flags | RF_Transactional | RF_Public | RF_Standalone));
}

UObject* UChaosSolverFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	UChaosSolver* NewChaosSolver = StaticFactoryCreateNew(Class, InParent, Name, Flags, Context, Warn);
	NewChaosSolver->MarkPackageDirty();
	return NewChaosSolver;
}

#undef LOCTEXT_NAMESPACE




