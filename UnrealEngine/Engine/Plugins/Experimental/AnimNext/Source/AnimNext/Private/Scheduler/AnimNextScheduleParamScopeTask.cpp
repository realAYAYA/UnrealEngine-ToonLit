// Copyright Epic Games, Inc. All Rights Reserved.

#include "Scheduler/AnimNextScheduleParamScopeTask.h"
#include "Scheduler/ScheduleContext.h"
#include "Param/IParameterSource.h"
#include "AnimNextStats.h"
#include "Param/PropertyBagProxy.h"

DEFINE_STAT(STAT_AnimNext_Task_ScopeEntry);
DEFINE_STAT(STAT_AnimNext_Task_ScopeExit);

void FAnimNextScheduleParamScopeEntryTask::RunParamScopeEntry(const UE::AnimNext::FScheduleContext& InScheduleContext) const
{
	SCOPE_CYCLE_COUNTER(STAT_AnimNext_Task_ScopeEntry);

	using namespace UE::AnimNext;

	FParamStack& ParamStack = FParamStack::Get();

	const float DeltaTime = InScheduleContext.GetDeltaTime();
	FScheduleInstanceData& InstanceData = InScheduleContext.GetInstanceData();
	FScheduleInstanceData::FScopeCache& ScopeCache = InstanceData.ScopeCaches[ParamScopeIndex];
	check(ScopeCache.PushedLayers.Num() == 0);

	// Update & push user params (before scope)
	FScheduleInstanceData::FUserScope* FoundUserScope = InstanceData.UserScopes.Find(Scope);
	if (FoundUserScope && FoundUserScope->BeforeSource.IsValid())
	{
		FPropertyBagProxy& UserParameterSource = *FoundUserScope->BeforeSource.Get();
		UserParameterSource.Update(DeltaTime);
		ScopeCache.PushedLayers.Add(ParamStack.PushLayer(UserParameterSource.GetLayerHandle()));
	}

	// Update & push static params
	for (int32 ParameterSourceIndex = 0; ParameterSourceIndex < ScopeCache.ParameterSources.Num(); ++ParameterSourceIndex)
	{
		TUniquePtr<IParameterSource>& StaticParameterSource = ScopeCache.ParameterSources[ParameterSourceIndex];

		StaticParameterSource->Update(DeltaTime);
		ScopeCache.PushedLayers.Add(ParamStack.PushLayer(StaticParameterSource->GetLayerHandle()));
	}

	// Update & push user params (after scope)
	if (FoundUserScope && FoundUserScope->AfterSource.IsValid())
	{
		FPropertyBagProxy& UserParameterSource = *FoundUserScope->AfterSource.Get();
		UserParameterSource.Update(DeltaTime);
		ScopeCache.PushedLayers.Add(ParamStack.PushLayer(UserParameterSource.GetLayerHandle()));
	}
}

void FAnimNextScheduleParamScopeExitTask::RunParamScopeExit(const UE::AnimNext::FScheduleContext& InScheduleContext) const
{
	SCOPE_CYCLE_COUNTER(STAT_AnimNext_Task_ScopeExit);

	using namespace UE::AnimNext;

	FParamStack& ParamStack = FParamStack::Get();

	FScheduleInstanceData& InstanceData = InScheduleContext.GetInstanceData();
	FScheduleInstanceData::FScopeCache& ScopeCache = InstanceData.ScopeCaches[ParamScopeIndex];
	for (int32 LayerIndex = ScopeCache.PushedLayers.Num() - 1; LayerIndex >= 0; --LayerIndex)
	{
		ParamStack.PopLayer(ScopeCache.PushedLayers[LayerIndex]);
	}

	ScopeCache.PushedLayers.Reset();
}