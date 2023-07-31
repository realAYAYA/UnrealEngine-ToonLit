// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "InterchangeSourceData.h"
#include "Texture/InterchangeSlicedTexturePayloadData.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"

#include "InterchangeSlicedTexturePayloadInterface.generated.h"

UINTERFACE()
class INTERCHANGEIMPORT_API UInterchangeSlicedTexturePayloadInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * Texture payload interface. Derive from it if your payload can import texture
 */
class INTERCHANGEIMPORT_API IInterchangeSlicedTexturePayloadInterface
{
	GENERATED_BODY()
public:

	/**
	 * Once the translation is done, the import process need a way to retrieve payload data.
	 * This payload will be use by the factories to create the asset.
	 *
	 * @param SourceData - The source data containing the data to translate
	 * @param PayloadKey - The key to retrieve the a particular payload contain into the specified source data.
	 * @return a PayloadData containing the import image data. The TOptional will not be set if there is an error.
	 */
	virtual TOptional<UE::Interchange::FImportSlicedImage> GetSlicedTexturePayloadData(const UInterchangeSourceData* SourceData, const FString& PayloadKey) const = 0;
};
