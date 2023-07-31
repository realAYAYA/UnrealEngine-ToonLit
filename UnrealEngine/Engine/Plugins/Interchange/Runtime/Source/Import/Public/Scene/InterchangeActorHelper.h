// Copyright Epic Games, Inc. All Rights Reserved. 

#pragma once

#include "InterchangeFactoryBase.h"

class UInterchangeFactoryBaseNode;
class UInterchangeBaseNodeContainer;
class UInterchangeActorFactoryNode;
class UInterchangeMeshActorFactoryNode;
class UMeshComponent;

namespace UE::Interchange::ActorHelper
{
	/**
	 * Returns the parent actor of the actor described by the FactoryNode using the node hierarchy.
	 */
	INTERCHANGEIMPORT_API AActor* GetSpawnedParentActor(const UInterchangeBaseNodeContainer* NodeContainer, const UInterchangeActorFactoryNode* FactoryNode);

	/**
	 * Spawns and return a AAActor using the given SceneObjectsParam.
	 * The actor's parent will be determine by the node hierarchy and its type by the factory node's ObjectClass.
	 */
	INTERCHANGEIMPORT_API AActor* SpawnFactoryActor(const UInterchangeFactoryBase::FCreateSceneObjectsParams& CreateSceneObjectsParams);
	
	/**
	 * Returns the factory node of the asset instanced by ActorFactoryNode.
	 */
	INTERCHANGEIMPORT_API const UInterchangeFactoryBaseNode* FindAssetInstanceFactoryNode(const UInterchangeBaseNodeContainer* NodeContainer, const UInterchangeFactoryBaseNode* ActorFactoryNode);

	/**
	 * Applies material slot dependencies stored in ActorFactoryNode to MeshComponent.
	 */
	INTERCHANGEIMPORT_API void ApplySlotMaterialDependencies(const UInterchangeBaseNodeContainer& NodeContainer, const UInterchangeMeshActorFactoryNode& ActorFactoryNode, UMeshComponent& MeshComponent);
}
