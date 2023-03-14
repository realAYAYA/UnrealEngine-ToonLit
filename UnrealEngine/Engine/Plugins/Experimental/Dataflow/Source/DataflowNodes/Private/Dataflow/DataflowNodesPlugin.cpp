// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/DataflowNodesPlugin.h"

#include "CoreMinimal.h"
#include "Dataflow/DataflowSkeletalMeshNodes.h"
#include "Dataflow/DataflowStaticMeshNodes.h"
#include "Dataflow/DataflowNodeFactory.h"

#define LOCTEXT_NAMESPACE "DataflowNodes"


void IDataflowNodesPlugin::StartupModule()
{
	Dataflow::RegisterSkeletalMeshNodes();
	Dataflow::RegisterStaticMeshNodes();
}

void IDataflowNodesPlugin::ShutdownModule()
{
}


IMPLEMENT_MODULE(IDataflowNodesPlugin, DataflowNodes)


#undef LOCTEXT_NAMESPACE
