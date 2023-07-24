// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "InterchangeSourceData.h"
#include "Mesh/InterchangeSkeletalMeshPayload.h"
#include "UObject/Interface.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"

#include "InterchangeSkeletalMeshPayloadInterface.generated.h"

UINTERFACE()
class INTERCHANGEIMPORT_API UInterchangeSkeletalMeshPayloadInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * Skeletal mesh payload interface. Derive from this interface if your payload can import skeletal mesh
 */
class INTERCHANGEIMPORT_API IInterchangeSkeletalMeshPayloadInterface
{
	GENERATED_BODY()
public:

	/**
	 * Get a skeletal mesh payload data for the specified payload key
	 *
	 * @param PayLoadKey - The key to retrieve the a particular payload contain into the specified source data.
	 * @return - The resulting PayloadData as a TFuture point by the PayloadKey. The TOptional will not be set if there is an error retrieving the payload.
	 * 
	 */
	virtual TFuture<TOptional<UE::Interchange::FSkeletalMeshLodPayloadData>> GetSkeletalMeshLodPayloadData(const FString& PayLoadKey) const = 0;

	/**
	 * Get one skeletal mesh blend shape payload data for the specified key
	 *
	 * @param PayLoadKey - The key to retrieve the a particular payload contain into the specified source data.
	 * @return - The resulting PayloadData as a TFuture point by the payload key. The TOptional will not be set if there is an error retrieving the payload.
	 * 
	 */
	virtual TFuture<TOptional<UE::Interchange::FSkeletalMeshMorphTargetPayloadData>> GetSkeletalMeshMorphTargetPayloadData(const FString& PayLoadKey) const = 0;
};


