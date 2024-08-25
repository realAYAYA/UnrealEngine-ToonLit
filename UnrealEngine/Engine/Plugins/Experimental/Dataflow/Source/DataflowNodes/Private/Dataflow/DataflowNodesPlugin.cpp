// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/DataflowNodesPlugin.h"

#include "CoreMinimal.h"
#include "Dataflow/DataflowSkeletalMeshNodes.h"
#include "Dataflow/DataflowStaticMeshNodes.h"
#include "Dataflow/DataflowNodeFactory.h"
#include "Dataflow/DataflowSelectionNodes.h"
#include "Dataflow/DataflowContextOverridesNodes.h"
#include "Dataflow/DataflowCollectionAddScalarVertexPropertyNode.h"

#define LOCTEXT_NAMESPACE "DataflowNodes"


void IDataflowNodesPlugin::StartupModule()
{
	Dataflow::RegisterSkeletalMeshNodes();
	Dataflow::RegisterStaticMeshNodes();
	Dataflow::RegisterSelectionNodes();
	Dataflow::RegisterContextOverridesNodes();
	DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FDataflowCollectionAddScalarVertexPropertyNode);
}

void IDataflowNodesPlugin::ShutdownModule()
{
}


IMPLEMENT_MODULE(IDataflowNodesPlugin, DataflowNodes)


#undef LOCTEXT_NAMESPACE
