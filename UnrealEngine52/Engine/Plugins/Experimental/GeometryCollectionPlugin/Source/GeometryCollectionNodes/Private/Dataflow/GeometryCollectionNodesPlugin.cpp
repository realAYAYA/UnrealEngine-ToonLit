// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/GeometryCollectionNodesPlugin.h"

#include "CoreMinimal.h"

#include "Dataflow/DataflowNodeFactory.h"
#include "Dataflow/GeometryCollectionNodes.h"
#include "Dataflow/Nodes/GeometryCollectionAssetNodes.h"
#include "Dataflow/GeometryCollectionProcessingNodes.h"
#include "Dataflow/GeometryCollectionSkeletalMeshNodes.h"
#include "Dataflow/GeometryCollectionSelectionNodes.h"
#include "Dataflow/GeometryCollectionMeshNodes.h"
#include "Dataflow/GeometryCollectionClusteringNodes.h"
#include "Dataflow/GeometryCollectionFracturingNodes.h"
#include "Dataflow/GeometryCollectionEditNodes.h"
#include "Dataflow/GeometryCollectionUtilityNodes.h"
#include "Dataflow/GeometryCollectionMaterialNodes.h"
#include "Dataflow/GeometryCollectionFieldNodes.h"
#include "Dataflow/GeometryCollectionVerticesNodes.h"

#define LOCTEXT_NAMESPACE "DataflowNodes"


void IGeometryCollectionNodesPlugin::StartupModule()
{
	Dataflow::GeometryCollectionEngineNodes();
	Dataflow::GeometryCollectionEngineAssetNodes();
	Dataflow::GeometryCollectionProcessingNodes();
	Dataflow::GeometryCollectionSkeletalMeshNodes();
	Dataflow::GeometryCollectionSelectionNodes();
	Dataflow::GeometryCollectionMeshNodes();
	Dataflow::GeometryCollectionClusteringNodes();
	Dataflow::GeometryCollectionFracturingNodes();
	Dataflow::GeometryCollectionEditNodes();
	Dataflow::GeometryCollectionUtilityNodes();
	Dataflow::GeometryCollectionMaterialNodes();
	Dataflow::GeometryCollectionFieldNodes();
	Dataflow::GeometryCollectionVerticesNodes();
}

void IGeometryCollectionNodesPlugin::ShutdownModule()
{
}


IMPLEMENT_MODULE(IGeometryCollectionNodesPlugin, GeometryCollectionNodes)


#undef LOCTEXT_NAMESPACE
