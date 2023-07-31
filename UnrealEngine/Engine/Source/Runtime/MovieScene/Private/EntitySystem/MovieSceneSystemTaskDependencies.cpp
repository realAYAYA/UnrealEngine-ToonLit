// Copyright Epic Games, Inc. All Rights Reserved.

#include "EntitySystem/MovieSceneSystemTaskDependencies.h"
#include "EntitySystem/MovieSceneEntitySystemGraphs.h"
#include "Algo/Find.h"


DEFINE_STAT(MovieSceneEval_SystemDependencyCost);

namespace UE
{
namespace MovieScene
{


void FSystemTaskPrerequisites::FilterByComponent(FGraphEventArray& OutArray, std::initializer_list<FComponentTypeID> ComponentTypes) const
{
	for (const FPrerequisite& Prereq : Prereqs)
	{
		if (Algo::Find(ComponentTypes, Prereq.ComponentType) != nullptr)
		{
			OutArray.Add(Prereq.GraphEvent);
		}
	}
}

void FSystemTaskPrerequisites::AddComponentTask(FComponentTypeID ComponentType, const FGraphEventRef& InNewTask)
{
	Prereqs.Add(FPrerequisite{ InNewTask, ComponentType });
}

void FSystemTaskPrerequisites::Consume(const FSystemTaskPrerequisites& Other)
{
	Prereqs.Append(Other.Prereqs);
	AllTasks.Reset();
}



FSystemSubsequentTasks::FSystemSubsequentTasks(FMovieSceneEntitySystemGraph* InGraph, FGraphEventArray* InAllTasks)
	: Graph(InGraph)
	, AllTasks(InAllTasks)
{}

void FSystemSubsequentTasks::ResetNode(uint16 InNodeID)
{
	Subsequents = Graph->Nodes.Array[InNodeID].SubsequentTasks;
	NodeID = InNodeID;

	if (Subsequents)
	{
		Subsequents->Empty();
	}
}

void FSystemSubsequentTasks::AddMasterTask(FGraphEventRef MasterTask)
{
	SCOPE_CYCLE_COUNTER(MovieSceneEval_SystemDependencyCost)

	if (MasterTask)
	{
		if (!Subsequents)
		{
			Subsequents = MakeShared<FSystemTaskPrerequisites>();
			Graph->Nodes.Array[NodeID].SubsequentTasks = Subsequents;
		}
		Subsequents->AddMasterTask(MasterTask);

		AllTasks->Add(MasterTask);
	}
}

void FSystemSubsequentTasks::AddComponentTask(UE::MovieScene::FComponentTypeID ComponentType, FGraphEventRef ComponentTask)
{	
	SCOPE_CYCLE_COUNTER(MovieSceneEval_SystemDependencyCost)

	if (ComponentTask)
	{
		if (!Subsequents)
		{
			Subsequents = MakeShared<FSystemTaskPrerequisites>();
			Graph->Nodes.Array[NodeID].SubsequentTasks = Subsequents;
		}

		Subsequents->AddComponentTask(ComponentType, ComponentTask);
		AllTasks->Add(ComponentTask);
	}
}


} // namespace MovieScene
} // namespace UE
