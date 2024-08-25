// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaTransitionExecutor.h"
#include "AvaTagHandleKeyFuncs.h"
#include "AvaTransitionLayer.h"
#include "AvaTransitionSubsystem.h"
#include "Behavior/IAvaTransitionBehavior.h"
#include "Engine/World.h"
#include "Execution/AvaTransitionExecutorBuilder.h"

DEFINE_LOG_CATEGORY_STATIC(LogAvaTransitionExecutor, Log, All);

FAvaTransitionExecutor::FAvaTransitionExecutor(FAvaTransitionExecutorBuilder& InBuilder)
	: Instances(MoveTemp(InBuilder.Instances))
	, NullInstance(MoveTemp(InBuilder.NullInstance))
	, ContextName(MoveTemp(InBuilder.ContextName))
	, OnFinished(MoveTemp(InBuilder.OnFinished))
{
}

FAvaTransitionExecutor::~FAvaTransitionExecutor()
{
	if (IsRunning())
	{
		// Log rather than ensuring because this can still happen when running behaviors and shutting down engine,
		// transitioning to another level, etc
		UE_LOG(LogAvaTransitionExecutor, Warning
			, TEXT("FAvaTransitionExecutor '%p' (in Context %s) has been destroyed while still running Behaviors!")
			, this
			, *ContextName);
	}
}

void FAvaTransitionExecutor::Setup()
{
	// Do a Setup pass on the Current Instances
	ForEachInstance(
		[this](FAvaTransitionBehaviorInstance& InInstance)
		{
			InInstance.SetLogContext(ContextName);
			InInstance.Setup();
		});

	using FAvaTagHandleMapType = TMap<FAvaTagHandle, EAvaTransitionType, FDefaultSetAllocator, TAvaTagHandleMapKeyFuncs<EAvaTransitionType, false>>;

	FAvaTagHandleMapType LayerToTransitionTypeMap;
	LayerToTransitionTypeMap.Reserve(Instances.Num());

	// Gather Map of Layer to the Transition Types for that Layer
	for (const FAvaTransitionBehaviorInstance& Instance : Instances)
	{
		EAvaTransitionType& LayerTransitionType = LayerToTransitionTypeMap.FindOrAdd(Instance.GetTransitionLayer());
		LayerTransitionType |= Instance.GetTransitionType();
	}

	// Ensure there's an exiting null instance for every entering Transition Instance in a layer
	for (const TPair<FAvaTagHandle, EAvaTransitionType>& Pair : LayerToTransitionTypeMap)
	{
		if (Pair.Value == EAvaTransitionType::In && !EnumHasAnyFlags(Pair.Value, EAvaTransitionType::Out))
		{
			FAvaTransitionBehaviorInstance& NullInstanceCopy = Instances.Add_GetRef(NullInstance);
			NullInstanceCopy.SetTransitionType(EAvaTransitionType::Out);
			NullInstanceCopy.SetOverrideLayer(Pair.Key);
			NullInstanceCopy.Setup();
		}
	}

	// For the Instances that are going out, if they belong in the same Transition Layer as an Instance going In
	// mark them as Needs Discard (this does not mean the scene will be discarded as there could be logic that reverts this flag)
	for (FAvaTransitionBehaviorInstance& Instance : Instances)
	{
		if (Instance.GetTransitionType() == EAvaTransitionType::In)
		{
			continue;
		}

		Instance.SetTransitionType(EAvaTransitionType::Out);

		const EAvaTransitionType LayerTransitionType = LayerToTransitionTypeMap[Instance.GetTransitionLayer()];

		if (EnumHasAnyFlags(LayerTransitionType, EAvaTransitionType::In))
		{
			if (FAvaTransitionScene* TransitionScene = Instance.GetTransitionContext().GetTransitionScene())
			{
				TransitionScene->SetFlags(EAvaTransitionSceneFlags::NeedsDiscard);
			}
		}
	}
}

void FAvaTransitionExecutor::Start()
{
	if (!ensureAlways(!IsRunning()))
	{
		UE_LOG(LogAvaTransitionExecutor, Error
			, TEXT("Trying to start an already-running FAvaTransitionExecutor '%p' (in Context %s)!")
			, this
			, *ContextName);
		return;
	}

	Setup();

	ForEachInstance(
		[](FAvaTransitionBehaviorInstance& InInstance)
		{
			InInstance.Start();
		});

	// All Behaviors might've finished on Start
	ConditionallyFinishBehaviors();
}

bool FAvaTransitionExecutor::IsRunning() const
{
	return Instances.ContainsByPredicate(
		[](const FAvaTransitionBehaviorInstance& InInstance)
		{
			return InInstance.IsRunning();
		});
}

TArray<const FAvaTransitionBehaviorInstance*> FAvaTransitionExecutor::GetBehaviorInstances(const FAvaTransitionLayerComparator& InComparator) const
{
	TArray<const FAvaTransitionBehaviorInstance*> OutInstances;
	OutInstances.Reserve(Instances.Num());

	ForEachInstance(
		[&OutInstances, &InComparator](const FAvaTransitionBehaviorInstance& InBehaviorInstance)
		{
			if (InComparator.Compare(InBehaviorInstance))
			{
				OutInstances.Add(&InBehaviorInstance);
			}
		});

	return OutInstances;
}

void FAvaTransitionExecutor::Stop()
{
	ForEachInstance(
		[](FAvaTransitionBehaviorInstance& InInstance)
		{
			InInstance.Stop();
		});

	ensureAlways(!IsRunning());
	ConditionallyFinishBehaviors();
}

TStatId FAvaTransitionExecutor::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(FAvaTransitionExecutor, STATGROUP_Tickables);
}

void FAvaTransitionExecutor::Tick(float InDeltaSeconds)
{
	ForEachInstance(
		[InDeltaSeconds](FAvaTransitionBehaviorInstance& InInstance)
		{
			InInstance.Tick(InDeltaSeconds);
		});

	ConditionallyFinishBehaviors();
}

bool FAvaTransitionExecutor::IsTickable() const
{
	return IsRunning();
}

void FAvaTransitionExecutor::ForEachInstance(TFunctionRef<void(FAvaTransitionBehaviorInstance&)> InFunc)
{
	for (FAvaTransitionBehaviorInstance& Instance : Instances)
	{
		InFunc(Instance);
	}
}

void FAvaTransitionExecutor::ForEachInstance(TFunctionRef<void(const FAvaTransitionBehaviorInstance&)> InFunc) const
{
	for (const FAvaTransitionBehaviorInstance& Instance : Instances)
	{
		InFunc(Instance);
	}
}

void FAvaTransitionExecutor::ConditionallyFinishBehaviors()
{
	if (!IsRunning())
	{
		OnFinished.ExecuteIfBound();
	}
}
