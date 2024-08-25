// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "UObject/WeakObjectPtr.h"

class UCameraAsset;
class UCameraDirector;
class UCameraEvaluationContext;
class UCameraSystemEvaluator;

/** Information about a running camera evaluation context. */
struct FCameraEvaluationContextInfo
{
	/** The evaluation context. */
	UCameraEvaluationContext* EvaluationContext = nullptr;

	/** The instantiated camera director running in this context. */
	UCameraDirector* CameraDirector = nullptr;

	/** Returns whether this structure has a valid context and director. */
	bool IsValid() const { return EvaluationContext && CameraDirector; }
};

/**
 * A simple stack of evaluation contexts. The top one is the active one.
 */
struct FCameraEvaluationContextStack
{
public:

	/** Gets the active (top) context. */
	FCameraEvaluationContextInfo GetActiveContext() const;

	/** Returns whether the given context exists in the stack. */
	bool HasContext(UCameraEvaluationContext* Context) const;

	/** Push a new context on the stack and instantiate its director. */
	void PushContext(UCameraEvaluationContext* Context);

	/** Remove an existing context from the stack. */
	bool RemoveContext(UCameraEvaluationContext* Context);

	/** Pop the active (top) context. */
	void PopContext();

public:

	// Internal API
	void Initialize(UCameraSystemEvaluator* InEvaluator);
	void AddReferencedObjects(FReferenceCollector& Collector);

private:

	struct FContextEntry
	{
		TWeakObjectPtr<UCameraEvaluationContext> WeakContext;
		TObjectPtr<UCameraDirector> CameraDirector;
	};

	/** The entries in the stack. */
	TArray<FContextEntry> Entries;

	/** The owner evaluator. */
	TObjectPtr<UCameraSystemEvaluator> Evaluator;
};

