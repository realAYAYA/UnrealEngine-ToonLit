// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaTransitionEnums.h"
#include "Behavior/AvaTransitionBehaviorInstance.h"
#include "Behavior/IAvaTransitionBehavior.h"
#include "Containers/Array.h"
#include "Delegates/Delegate.h"
#include "Execution/IAvaTransitionExecutor.h"
#include "Tickable.h"
#include "UObject/WeakInterfacePtr.h"

class FAvaTransitionExecutorBuilder;
class IAvaTransitionBehavior;

/** Base Implementation of an Executor dealing with multiple behaviors going out (Exit Instances) and multiple behaviors going in (Enter Instances) */
class FAvaTransitionExecutor : public IAvaTransitionExecutor, public FTickableGameObject
{
public:
	explicit FAvaTransitionExecutor(FAvaTransitionExecutorBuilder& InBuilder);

	virtual ~FAvaTransitionExecutor() override;

	bool IsRunning() const;

protected:
	void Setup();

	//~ Begin IAvaTransitionExecutor
	virtual TArray<const FAvaTransitionBehaviorInstance*> GetBehaviorInstances(const FAvaTransitionLayerComparator& InComparator) const;
	virtual void Start() override;
	virtual void Stop() override;
	//~ End IAvaTransitionExecutor

	//~ Begin FTickableGameObject
	virtual TStatId GetStatId() const override;
	virtual void Tick(float InDeltaSeconds) override;
	virtual bool IsTickable() const override;
	virtual bool IsTickableInEditor() const override { return true; }
	virtual ETickableTickType GetTickableTickType() const override { return ETickableTickType::Conditional; }
	//~ End FTickableGameObject

private:
	void ForEachInstance(TFunctionRef<void(FAvaTransitionBehaviorInstance&)> InFunc);
	void ForEachInstance(TFunctionRef<void(const FAvaTransitionBehaviorInstance&)> InFunc) const;

	void ConditionallyFinishBehaviors();

	TArray<FAvaTransitionBehaviorInstance> Instances;

	const FAvaTransitionBehaviorInstance NullInstance;

	FString ContextName;

	FSimpleDelegate OnFinished;
};
