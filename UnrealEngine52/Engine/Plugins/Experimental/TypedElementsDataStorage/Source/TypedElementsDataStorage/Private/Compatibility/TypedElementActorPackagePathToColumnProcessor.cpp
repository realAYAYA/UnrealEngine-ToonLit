// Copyright Epic Games, Inc. All Rights Reserved.

#include "Compatibility/TypedElementActorPackagePathToColumnProcessor.h"

#include "Elements/Columns/TypedElementPackageColumns.h"
#include "MassActorSubsystem.h"
#include "MassExecutionContext.h"
#include "TypedElementSubsystems.h"

UTypedElementActorPackagePathToColumnProcessor::UTypedElementActorPackagePathToColumnProcessor()
	: Query(*this)
{
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Editor);
	ObservedType = FMassActorFragment::StaticStruct();
	Operation = EMassObservedOperation::Add;
	bRequiresGameThreadExecution = true;
}

void UTypedElementActorPackagePathToColumnProcessor::ConfigureQueries()
{
	Query.AddRequirement(FMassActorFragment::StaticStruct(), EMassFragmentAccess::ReadOnly);
	Query.AddRequirement(FTypedElementPackagePathColumn::StaticStruct(), EMassFragmentAccess::ReadWrite);
	Query.AddRequirement(FTypedElementPackageLoadedPathColumn::StaticStruct(), EMassFragmentAccess::ReadWrite);
	
	ProcessorRequirements.AddSubsystemRequirement(UTypedElementDataStorageSubsystem::StaticClass(), EMassFragmentAccess::ReadWrite);
}

void UTypedElementActorPackagePathToColumnProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	Query.ForEachEntityChunk(EntityManager, Context, [](FMassExecutionContext& Context)
		{
			const FMassActorFragment* ActorIt = Context.GetFragmentView<FMassActorFragment>().GetData();
			FTypedElementPackagePathColumn* PathIt = Context.GetMutableFragmentView<FTypedElementPackagePathColumn>().GetData();
			FTypedElementPackageLoadedPathColumn* LoadedPathIt = Context.GetMutableFragmentView<FTypedElementPackageLoadedPathColumn>().GetData();
			
			const int32 EntityCount = Context.GetNumEntities();
			for (int32 Counter=0; Counter < EntityCount; ++Counter)
			{
				UPackage* Target = ActorIt->Get()->GetPackage();

				Target->GetPathName(nullptr, PathIt->Path);
				LoadedPathIt->LoadedPath = Target->GetLoadedPath();
				
				++ActorIt;
				++PathIt;
				++LoadedPathIt;
			}
		}
	);
}
