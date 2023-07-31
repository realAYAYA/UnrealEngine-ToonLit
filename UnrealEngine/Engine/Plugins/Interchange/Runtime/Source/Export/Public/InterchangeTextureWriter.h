// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InterchangeWriterBase.h"
#include "Nodes/InterchangeBaseNodeContainer.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"

#include "InterchangeTextureWriter.generated.h"

UCLASS(BlueprintType, Experimental)
class INTERCHANGEEXPORT_API UInterchangeTextureWriter : public UInterchangeWriterBase
{
	GENERATED_BODY()
public:
	
	/**
	 * Export all nodes of type FTextureNode hold by the BaseNodeContainer
	 * @param BaseNodeContainer - Contain nodes describing what to export
	 * @return true if the writer can export the nodes, false otherwise.
	 */
	virtual bool Export(UInterchangeBaseNodeContainer* BaseNodeContainer) const override;
};


