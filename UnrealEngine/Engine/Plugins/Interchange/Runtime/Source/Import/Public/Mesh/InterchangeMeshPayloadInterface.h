// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Async/Future.h"
#include "InterchangeSourceData.h"
#include "Mesh/InterchangeMeshPayload.h"
#include "InterchangeMeshNode.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Interface.h"

#include "InterchangeMeshPayloadInterface.generated.h"

UINTERFACE()
class INTERCHANGEIMPORT_API UInterchangeMeshPayloadInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * Static mesh payload interface. Derive from this interface if your payload can import static mesh
 */
class INTERCHANGEIMPORT_API IInterchangeMeshPayloadInterface
{
	GENERATED_BODY()
public:

	/**
	 * Once the translation is done, the import process need a way to retrieve payload data.
	 * This payload will be use by the factories to create the asset.
	 *
	 * @param PayloadKey - The key to retrieve the a particular payload contain into the specified source data.
	 * @param MeshGlobalTransform - The transform to apply to the raw data when creating the payload data.
	 * @return a PayloadData containing the the data ask with the key.
	 */
	virtual TFuture<TOptional<UE::Interchange::FMeshPayloadData>> GetMeshPayloadData(const FInterchangeMeshPayLoadKey& PayLoadKey, const FTransform& MeshGlobalTransform) const = 0;
};


