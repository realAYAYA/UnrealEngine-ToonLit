// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/CameraEvaluationContextStack.h"

#include "Core/CameraAsset.h"
#include "Core/CameraDirector.h"
#include "Core/CameraEvaluationContext.h"
#include "Core/CameraRuntimeInstantiator.h"
#include "Core/CameraSystemEvaluator.h"
#include "UObject/Package.h"

FCameraEvaluationContextInfo FCameraEvaluationContextStack::GetActiveContext() const
{
	for (const FContextEntry& Entry : ReverseIterate(Entries))
	{
		if (UCameraEvaluationContext* Context = Entry.WeakContext.Get())
		{
			return FCameraEvaluationContextInfo{ Context, Entry.CameraDirector };
		}
	}
	return FCameraEvaluationContextInfo();
}

bool FCameraEvaluationContextStack::HasContext(UCameraEvaluationContext* Context) const
{
	for (const FContextEntry& Entry : ReverseIterate(Entries))
	{
		if (Context == Entry.WeakContext.Get())
		{
			return true;
		}
	}
	return false;
}

void FCameraEvaluationContextStack::PushContext(UCameraEvaluationContext* Context)
{
	checkf(Evaluator, TEXT("Can't push context when no evaluator is set! Did you call Initialize?"));

	// If we're pushing an existing context, move it to the top.
	const int32 ExistingIndex = Entries.IndexOfByPredicate(
			[Context](const FContextEntry& Entry) { return Entry.WeakContext == Context; });
	if (ExistingIndex != INDEX_NONE)
	{
		if (ExistingIndex < Entries.Num() - 1)
		{
			const FContextEntry EntryCopy(Entries[ExistingIndex]);
			Entries.RemoveAt(ExistingIndex);
			Entries.Add(EntryCopy);
		}
		return;
	}

	// Instantiate the camera director.
	FCameraRuntimeInstantiationParams InstParams;
	InstParams.InstantiationOuter = Evaluator;
	if (!InstParams.InstantiationOuter)
	{
		InstParams.InstantiationOuter = GetTransientPackage();
	}

	const UCameraDirector* OriginalCameraDirector = Context->GetCameraAsset()->CameraDirector;
	UCameraDirector* NewCameraDirector = Evaluator->GetRuntimeInstantiator().InstantiateCameraDirector(
			OriginalCameraDirector, InstParams);
	
	// Add an entry in the stack.
	FContextEntry NewEntry;
	NewEntry.WeakContext = Context;
	NewEntry.CameraDirector = NewCameraDirector;
	Entries.Push(NewEntry);
}

bool FCameraEvaluationContextStack::RemoveContext(UCameraEvaluationContext* Context)
{
	const int32 NumRemoved = Entries.RemoveAll(
			[Context](FContextEntry& Entry) { return Entry.WeakContext == Context; });
	return (NumRemoved > 0);
}

void FCameraEvaluationContextStack::PopContext()
{
	Entries.Pop();
}

void FCameraEvaluationContextStack::Initialize(UCameraSystemEvaluator* InEvaluator)
{
	Evaluator = InEvaluator;
}

void FCameraEvaluationContextStack::AddReferencedObjects(FReferenceCollector& Collector)
{
	for (FContextEntry& Entry : Entries)
	{
		Collector.AddReferencedObject(Entry.CameraDirector);
	}
}

