// Copyright Epic Games, Inc. All Rights Reserved.

#include "ScheduleInstanceData.h"
#include "Scheduler/AnimNextSchedule.h"
#include "Scheduler/ScheduleHandle.h"
#include "Scheduler/AnimNextSchedulerWorldSubsystem.h"
#include "Scheduler/AnimNextSchedulerEntry.h"
#include "Scheduler/AnimNextSchedulePort.h"
#include "AnimNextStats.h"
#include "Param/ExternalParameterRegistry.h"
#include "Param/IParameterSourceFactory.h"
#include "Param/ParameterBlockProxy.h"
#include "Param/PropertyBagProxy.h"
#include "Scheduler/AnimNextScheduleExternalParamTask.h"

DEFINE_STAT(STAT_AnimNext_CreateInstanceData);

namespace UE::AnimNext
{

FScheduleInstanceData::FScheduleInstanceData(const FScheduleContext& InScheduleContext, const UAnimNextSchedule* InSchedule, FScheduleHandle InHandle, FAnimNextSchedulerEntry* InCurrentEntry)
	: Handle(InHandle)
	, Entry(InCurrentEntry) 
{
	SCOPE_CYCLE_COUNTER(STAT_AnimNext_CreateInstanceData);

	// Preallocate data for all scopes & graphs in the schedule
	ScopeCaches.SetNum(InSchedule->NumParameterScopes);
	GraphCaches.SetNum(InSchedule->GraphTasks.Num());
	ExternalParamCaches.SetNum(InSchedule->ExternalParamTasks.Num());

	for(int32 EntryIndex = 0; EntryIndex < InSchedule->ParamScopeEntryTasks.Num(); ++EntryIndex)
	{
		const FAnimNextScheduleParamScopeEntryTask& EntryTask = InSchedule->ParamScopeEntryTasks[EntryIndex];
		FScopeCache& ScopeCache = ScopeCaches[EntryTask.ParamScopeIndex];
		ScopeCache.ParameterSources.Reserve(EntryTask.ParameterBlocks.Num());
		for(UAnimNextParameterBlock* ParameterBlock : EntryTask.ParameterBlocks)
		{
			if(ParameterBlock)
			{
				ScopeCache.ParameterSources.Emplace(MakeUnique<FParameterBlockProxy>(ParameterBlock));
			}
		}

		ScopeCache.PushedLayers.Reserve(ScopeCache.ParameterSources.Num() + 1); // +1 for any user handles added dynamically
	}

	// Setup param stack graph
	RootParamStack = InCurrentEntry->RootParamStack;
	ParamStacks.SetNum(InSchedule->NumParameterScopes);
	for (TSharedPtr<FParamStack>& ParamStack : ParamStacks)
	{
		ParamStack = MakeShared<FParamStack>();
	}

	for (const FAnimNextScheduleGraphTask& Task : InSchedule->GraphTasks)
	{
		const TSharedPtr<FParamStack> ParentStack = Task.ParamParentScopeIndex != MAX_uint32 ? ParamStacks[Task.ParamParentScopeIndex] : RootParamStack;
		ParamStacks[Task.ParamScopeIndex]->SetParent(ParentStack);
	}

	for (const FAnimNextScheduleExternalTask& ExternalTask : InSchedule->ExternalTasks)
	{
		const TSharedPtr<FParamStack> ParentStack = ExternalTask.ParamParentScopeIndex != MAX_uint32 ? ParamStacks[ExternalTask.ParamParentScopeIndex] : RootParamStack;
		ParamStacks[ExternalTask.ParamScopeIndex]->SetParent(ParentStack);
	}

	for (const FAnimNextScheduleParamScopeEntryTask& ScopeEntryTask : InSchedule->ParamScopeEntryTasks)
	{
		const TSharedPtr<FParamStack> ParentStack = ScopeEntryTask.ParamParentScopeIndex != MAX_uint32 ? ParamStacks[ScopeEntryTask.ParamParentScopeIndex] : RootParamStack;
		ParamStacks[ScopeEntryTask.ParamScopeIndex]->SetParent(ParentStack);
	}

	// Set up external parameters
	FExternalParameterContext ExternalParameterContext;
	ExternalParameterContext.Object = Entry->WeakObject.Get();

	const float DeltaTime = InScheduleContext.GetDeltaTime();
	for(int32 ExternalParamSourceIndex = 0; ExternalParamSourceIndex < ExternalParamCaches.Num(); ++ExternalParamSourceIndex)
	{
		FExternalParamCache& ExternalParamCache = ExternalParamCaches[ExternalParamSourceIndex];

		for(const FAnimNextScheduleExternalParameterSource& ParameterSource : InSchedule->ExternalParamTasks[ExternalParamSourceIndex].ParameterSources)
		{
			if(TUniquePtr<IParameterSource> NewParameterSource = FExternalParameterRegistry::CreateParameterSource(ExternalParameterContext, ParameterSource.ParameterSource, ParameterSource.Parameters))
			{
				// Initial update is required to populate the cache
				// TODO: This needs to move outside this function once we run initialization off the game thread, depending on thread-safety
				NewParameterSource->Update(DeltaTime);

				// External parameter layer is always pushed
				RootParamStack->PushLayer(NewParameterSource->GetLayerHandle());
				ExternalParamCache.ParameterSources.Add(MoveTemp(NewParameterSource));
			}
		}
	}

	// Duplicate intermediate data area
	IntermediatesData = InSchedule->IntermediatesData;

	// Make a hosting layer for the intermediates
	IntermediatesLayer = FParamStack::MakeReferenceLayer(IntermediatesData);

	// Resize remapped intermediate data layers for port tasks, they will be allocated lazily later
	PortTermLayers.SetNum(InSchedule->Ports.Num());
}

FScheduleInstanceData::~FScheduleInstanceData() = default;

void FScheduleInstanceData::AddReferencedObjects(FReferenceCollector& Collector)
{
	for (TPair<FName, FUserScope>& ParamPair : UserScopes)
	{
		if(ParamPair.Value.AfterSource.IsValid())
		{
			ParamPair.Value.AfterSource->AddReferencedObjects(Collector);
		}
		if(ParamPair.Value.BeforeSource.IsValid())
		{
			ParamPair.Value.BeforeSource->AddReferencedObjects(Collector);
		}
	}

	for (FGraphCache& GraphCache : GraphCaches)
	{
		Collector.AddPropertyReferencesWithStructARO(FAnimNextGraphInstancePtr::StaticStruct(), &GraphCache.GraphInstanceData);
	}
}

FString FScheduleInstanceData::GetReferencerName() const
{
	return TEXT("AnimNextInstanceData");
}

TSharedPtr<FParamStack> FScheduleInstanceData::GetParamStack(uint32 InIndex) const
{
	if(InIndex == MAX_uint32)
	{
		return RootParamStack;
	}
	else
	{
		return ParamStacks[InIndex];
	}
}

}