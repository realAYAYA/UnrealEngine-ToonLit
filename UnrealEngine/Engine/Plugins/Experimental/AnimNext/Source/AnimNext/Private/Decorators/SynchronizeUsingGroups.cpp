// Copyright Epic Games, Inc. All Rights Reserved.

#include "Decorators/SynchronizeUsingGroups.h"
#include "Graph/SyncGroup_GraphInstanceComponent.h"

namespace UE::AnimNext
{
	AUTO_REGISTER_ANIM_DECORATOR(FSynchronizeUsingGroupsDecorator)

	// Decorator implementation boilerplate
	#define DECORATOR_INTERFACE_ENUMERATOR(GeneratorMacro) \
		GeneratorMacro(IGroupSynchronization) \
		GeneratorMacro(ITimeline) \
		GeneratorMacro(IUpdate) \

	GENERATE_ANIM_DECORATOR_IMPLEMENTATION(FSynchronizeUsingGroupsDecorator, DECORATOR_INTERFACE_ENUMERATOR)
	#undef DECORATOR_INTERFACE_ENUMERATOR

	void FSynchronizeUsingGroupsDecorator::PreUpdate(FUpdateTraversalContext& Context, const TDecoratorBinding<IUpdate>& Binding, const FDecoratorUpdateState& DecoratorState) const
	{
		FInstanceData* InstanceData = Binding.GetInstanceData<FInstanceData>();

		TDecoratorBinding<IGroupSynchronization> GroupSyncDecorator;
		Context.GetInterface(Binding, GroupSyncDecorator);

		const FName GroupName = GroupSyncDecorator.GetGroupName(Context);
		const bool bHasGroupName = !GroupName.IsNone();

		// If we have a group name, we are active
		// Freeze the timeline, our sync group will control it
		InstanceData->bFreezeTimeline = bHasGroupName;

		// Forward the PreUpdate call, if the timeline attempts to update, we'll do nothing if we are frozen
		IUpdate::PreUpdate(Context, Binding, DecoratorState);

		if (!bHasGroupName)
		{
			// If no group name is specified, this decorator is inactive
			return;
		}

		const EAnimGroupRole::Type GroupRole = GroupSyncDecorator.GetGroupRole(Context);

		// Append this decorator to our group, we'll need to synchronize it
		FSyncGroupGraphInstanceComponent& Component = Context.GetComponent<FSyncGroupGraphInstanceComponent>();
		Component.RegisterWithGroup(GroupName, GroupRole, Binding.GetDecoratorPtr(), DecoratorState);
	}

	FName FSynchronizeUsingGroupsDecorator::GetGroupName(const FExecutionContext& Context, const TDecoratorBinding<IGroupSynchronization>& Binding) const
	{
		const FSharedData* SharedData = Binding.GetSharedData<FSharedData>();
		return SharedData->GroupName;
	}

	EAnimGroupRole::Type FSynchronizeUsingGroupsDecorator::GetGroupRole(const FExecutionContext& Context, const TDecoratorBinding<IGroupSynchronization>& Binding) const
	{
		const FSharedData* SharedData = Binding.GetSharedData<FSharedData>();
		return SharedData->GroupRole;
	}

	float FSynchronizeUsingGroupsDecorator::AdvanceBy(const FExecutionContext& Context, const TDecoratorBinding<IGroupSynchronization>& Binding, float DeltaTime) const
	{
		FInstanceData* InstanceData = Binding.GetInstanceData<FInstanceData>();

		// When the group advances the timeline, we thaw it to advance
		InstanceData->bFreezeTimeline = false;

		TDecoratorBinding<ITimeline> TimelineDecorator;
		Context.GetInterface(Binding, TimelineDecorator);

		const float ProgressRatio = TimelineDecorator.AdvanceBy(Context, DeltaTime);

		InstanceData->bFreezeTimeline = true;

		return ProgressRatio;
	}

	void FSynchronizeUsingGroupsDecorator::AdvanceToRatio(const FExecutionContext& Context, const TDecoratorBinding<IGroupSynchronization>& Binding, float ProgressRatio) const
	{
		FInstanceData* InstanceData = Binding.GetInstanceData<FInstanceData>();

		// When the group advances the timeline, we thaw it to advance
		InstanceData->bFreezeTimeline = false;

		TDecoratorBinding<ITimeline> TimelineDecorator;
		Context.GetInterface(Binding, TimelineDecorator);

		TimelineDecorator.AdvanceToRatio(Context, ProgressRatio);

		InstanceData->bFreezeTimeline = true;
	}

	float FSynchronizeUsingGroupsDecorator::AdvanceBy(const FExecutionContext& Context, const TDecoratorBinding<ITimeline>& Binding, float DeltaTime) const
	{
		const FInstanceData* InstanceData = Binding.GetInstanceData<FInstanceData>();
		if (InstanceData->bFreezeTimeline)
		{
			return 0.0f;	// If the timeline is frozen, we don't advance
		}

		return ITimeline::AdvanceBy(Context, Binding, DeltaTime);
	}

	void FSynchronizeUsingGroupsDecorator::AdvanceToRatio(const FExecutionContext& Context, const TDecoratorBinding<ITimeline>& Binding, float ProgressRatio) const
	{
		const FInstanceData* InstanceData = Binding.GetInstanceData<FInstanceData>();
		if (InstanceData->bFreezeTimeline)
		{
			return;			// If the timeline is frozen, we don't advance
		}

		ITimeline::AdvanceToRatio(Context, Binding, ProgressRatio);
	}
}
