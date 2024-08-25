// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Param/ParamStackLayerHandle.h"

namespace UE::AnimNext
{
	struct FParamContext;
}

namespace UE::AnimNext
{

/** An interface used to abstract AnimNext parameter sources */
class IParameterSource
{
public:
	virtual ~IParameterSource() = default;
	
	/** Update this parameter source */
	virtual void Update(float DeltaTime) = 0;

	/** Get a layer handle to allow this source to be used in the parameter stack */
	virtual const FParamStackLayerHandle& GetLayerHandle() const = 0; 

	/** Allow GC subscription - this should be called by the owner of the parameter source */
	virtual void AddReferencedObjects(FReferenceCollector& Collector) {}
};

}