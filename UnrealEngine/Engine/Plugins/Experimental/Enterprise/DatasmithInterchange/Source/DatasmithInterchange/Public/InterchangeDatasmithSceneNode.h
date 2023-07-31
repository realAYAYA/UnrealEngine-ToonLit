// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Nodes/InterchangeBaseNode.h"

#include "InterchangeDatasmithSceneNode.generated.h"

namespace UE::DatasmithImporter
{
	class FExternalSource;
}

class IDatasmithScene;

UCLASS(BlueprintType, Experimental)
class DATASMITHINTERCHANGE_API UInterchangeDatasmithSceneNode : public UInterchangeBaseNode
{
	GENERATED_BODY()

public:
	/**
	 * Return the node type name of the class, we use this when reporting error
	 */
	virtual FString GetTypeName() const override
	{
		const FString TypeName = TEXT("DatasmithSceneNode");
		return TypeName;
	}

	// Used by the InterchangeDatasmithPipeline while we are in the process of transferring the Datasmith Import logic over Interchange.
	// It should be removed once we no longer need the DatasmithImportContext.
	TSharedPtr<UE::DatasmithImporter::FExternalSource> ExternalSource;
	TSharedPtr<IDatasmithScene> DatasmithScene;
};