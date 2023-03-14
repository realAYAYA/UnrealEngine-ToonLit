// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/GeometryCollectionNodesPlugin.h"

#include "CoreMinimal.h"

#include "Dataflow/DataflowNodeFactory.h"
#include "Dataflow/GeometryCollectionNodes.h"
#include "Dataflow/GeometryCollectionProcessingNodes.h"
#include "Dataflow/GeometryCollectionSkeletalMeshNodes.h"
#include "Dataflow/GeometryCollectionTetrahedralNodes.h"

#define LOCTEXT_NAMESPACE "DataflowNodes"


void IGeometryCollectionNodesPlugin::StartupModule()
{
	Dataflow::GeometryCollectionEngineAssetNodes();
	Dataflow::GeometryCollectionProcessingNodes();
	Dataflow::GeometryCollectionSkeletalMeshNodes();
	Dataflow::GeometryCollectionTetrahedralNodes();
}

void IGeometryCollectionNodesPlugin::ShutdownModule()
{
}


IMPLEMENT_MODULE(IGeometryCollectionNodesPlugin, GeometryCollectionNodes)


#undef LOCTEXT_NAMESPACE
