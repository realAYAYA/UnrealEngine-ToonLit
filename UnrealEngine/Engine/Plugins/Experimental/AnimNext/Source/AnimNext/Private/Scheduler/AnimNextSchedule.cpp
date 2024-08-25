// Copyright Epic Games, Inc. All Rights Reserved.

#include "Scheduler/AnimNextSchedule.h"
#include "Tasks/Task.h"
#include "Async/TaskGraphInterfaces.h"
#include "EngineLogs.h"
#include "UObject/AssetRegistryTagsContext.h"
#include "Graph/AnimNextGraph.h"
#include "Param/AnimNextParameterBlock.h"

#if WITH_EDITOR
TUniqueFunction<void(UAnimNextSchedule*)> UAnimNextSchedule::CompileFunction;
TUniqueFunction<void(const UAnimNextSchedule*, FAssetRegistryTagsContext)> UAnimNextSchedule::GetAssetRegistryTagsFunction;
#endif

void UAnimNextScheduleEntry_AnimNextGraph::GetPreloadDependencies(TArray<UObject*>& OutDeps)
{
	Super::GetPreloadDependencies(OutDeps);

	OutDeps.Add(Graph);
}

void UAnimNextScheduleEntry_ParamScope::GetPreloadDependencies(TArray<UObject*>& OutDeps)
{
	Super::GetPreloadDependencies(OutDeps);

	for(UAnimNextParameterBlock* ParameterBlock : ParameterBlocks)
	{
		OutDeps.Add(ParameterBlock);
	}

	for(UAnimNextScheduleEntry* SubEntry : SubEntries)
	{
		SubEntry->GetPreloadDependencies(OutDeps);
	}
}

void UAnimNextSchedule::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITOR
	// delay compilation until the package has been loaded
	FCoreUObjectDelegates::OnEndLoadPackage.AddUObject(this, &UAnimNextSchedule::HandlePackageDone);
#endif
}

#if WITH_EDITOR

void UAnimNextSchedule::HandlePackageDone(const FEndLoadPackageContext& Context)
{
	if (!Context.LoadedPackages.Contains(GetPackage()))
	{
		return;
	}

	CompileSchedule();
}

void UAnimNextSchedule::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	CompileSchedule();
}

void UAnimNextSchedule::PostEditUndo()
{
	Super::PostEditUndo();

	CompileSchedule();
}

void UAnimNextSchedule::CompileSchedule()
{
	check(CompileFunction);

	CompileFunction(this);
}

void UAnimNextSchedule::GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const
{
	PRAGMA_DISABLE_DEPRECATION_WARNINGS;
	Super::GetAssetRegistryTags(OutTags);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS;
}

void UAnimNextSchedule::GetAssetRegistryTags(FAssetRegistryTagsContext Context) const
{
	Super::GetAssetRegistryTags(Context);

	check(GetAssetRegistryTagsFunction);

	GetAssetRegistryTagsFunction(this, Context);
}

void UAnimNextSchedule::GetPreloadDependencies(TArray<UObject*>& OutDeps)
{
	Super::GetPreloadDependencies(OutDeps);

	for(UAnimNextScheduleEntry* Entry : Entries)
	{
		Entry->GetPreloadDependencies(OutDeps);
	}
}

#endif // #if WITH_EDITOR