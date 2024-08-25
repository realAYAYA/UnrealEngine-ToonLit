// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/ChaosFleshNodesPlugin.h"


#include "Dataflow/ChaosFleshBindingsNodes.h"
#include "Dataflow/ChaosFleshCoreNodes.h"
#include "Dataflow/ChaosFleshEngineAssetNodes.h"
#include "Dataflow/ChaosFleshFiberDirectionInitializationNodes.h"
#include "Dataflow/ChaosFleshImportGEO.h"
#include "Dataflow/ChaosFleshKinematicInitializationNodes.h"
#include "Dataflow/ChaosFleshRadialTetrahedronNodes.h"
#include "Dataflow/ChaosFleshRenderInitializationNodes.h"
#include "Dataflow/ChaosFleshPositionTargetInitializationNodes.h"
#include "Dataflow/ChaosFleshSkeletalBindingsNode.h"
#include "Dataflow/ChaosFleshTetrahedralNodes.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "ChaosFleshNodes"


void IChaosFleshNodesPlugin::StartupModule()
{
	Dataflow::ChaosFleshBindingsNodes();
	Dataflow::RegisterChaosFleshEngineAssetNodes();
	Dataflow::RegisterChaosFleshCoreNodes();
	Dataflow::ChaosFleshFiberDirectionInitializationNodes();
	Dataflow::ChaosFleshRenderInitializationNodes();
	Dataflow::RegisterChaosFleshKinematicInitializationNodes();
	Dataflow::RegisterChaosFleshPositionTargetInitializationNodes();
	Dataflow::ChaosFleshTetrahedralNodes();
	Dataflow::ChaosFleshSkeletalBindingsNode();
	Dataflow::ChaosFleshRadialTetrahedronNodes();
	Dataflow::RegisterChaosFleshImportGEONodes();
}

void IChaosFleshNodesPlugin::ShutdownModule()
{
}


IMPLEMENT_MODULE(IChaosFleshNodesPlugin, ChaosFleshNodes)


#undef LOCTEXT_NAMESPACE
